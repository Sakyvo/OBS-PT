# Design: Help menu and GitHub updates

## Summary

Add a Windows-only `OBSUpdateManager` that owns update discovery, notification,
download, validation, and installer launch. It will use the already-linked Qt
Network module and the existing `OBSUpdate` dialog shell, while leaving the
legacy OBS manifest/patch updater outside the execution path.

The complete flow is:

```text
startup/manual action
  -> GitHub latest stable JSON
  -> strict release + installer validation
  -> semantic comparison / skip policy
  -> Markdown update dialog
  -> streamed Desktop .part download
  -> size + SHA-256 verification
  -> atomic final filename replacement
  -> completion dialog
  -> interactive NSIS /D=<current Install Root>
  -> normal OBS-PT shutdown
```

## Components and Ownership

### Release model

Add a small, side-effect-free model/parser under `UI/win-update/` with these
logical fields:

```cpp
struct OBSUpdateAsset {
	QString name;
	QUrl downloadUrl;
	qint64 size;
	QByteArray sha256;
};

struct OBSUpdateRelease {
	QVersionNumber version;
	QString versionText;
	QString notesMarkdown;
	QUrl releaseUrl;
	OBSUpdateAsset installer;
};
```

The parser owns all external JSON validation. It accepts `x.y.z` or `vx.y.z`,
requires three non-negative numeric components and no suffix, rejects
draft/prerelease releases, and requires exactly one asset named
`OBS-PT-x.y.z-Installer.exe`. The asset must have a positive size, an HTTPS
GitHub release URL, and exactly 32 bytes parsed from `sha256:<64 hex chars>`.
Rendering and download code consume this validated model and do not re-parse
raw JSON fields.

### Update manager

Add `OBSUpdateManager : public QObject`, owned by `OBSBasic`, with one
`QNetworkAccessManager` and an explicit state:

```text
Idle -> Checking -> Idle
                 -> UpdatePrompt -> Downloading -> DownloadReady -> Idle
```

Only one check or download may exist at a time. `Check(bool manual)` disables
the Help action through a signal and ignores duplicate calls. Every terminal
path re-enables it. Destruction aborts replies, closes progress UI, closes the
file handle, and removes any active `.part` file without waiting on a worker
thread.

`OBSBasic::TimedCheckForUpdates()` schedules one queued startup check after the
main window is usable. `OBSBasic::CheckForUpdates(true)` lazily creates/reuses
the manager for manual checks. The obsolete `updateCheckThread` ownership and
shutdown waits are removed; the unrelated What's New threads remain intact.

## GitHub Request Contract

Request:

```text
GET https://api.github.com/repos/Sakyvo/OBS-PT/releases/latest
Accept: application/vnd.github+json
X-GitHub-Api-Version: 2022-11-28
User-Agent: OBS-PT/<OBS_VERSION>
```

Use asynchronous `QNetworkReply` handling, a bounded/stall timeout, and a
reasonable maximum JSON response size. HTTP status, Qt network errors, invalid
JSON, invalid versions, missing assets, and rate-limit responses all become a
single typed check failure with detailed logging and localized user text.

The latest endpoint normally excludes drafts and prereleases; the parser still
checks both flags. `QVersionNumber::compare()` compares the strict parsed
release with strict parsed `OBS_VERSION`. A manual check bypasses the stored
skip only because it is an explicit user request; startup checks honor it.

## Persistent State

Continue using `[General] EnableAutoUpdates` so the existing settings binding
does not change. Add updater-owned state to Global Config:

```ini
[OBSPTUpdater]
SettingsVersion=1
SkippedVersion=
AutoFailureNotified=false
```

`prepare_obspt_global_config()` performs `SettingsVersion < 1` migration
independently from `OBSPT_BOOTSTRAP_VERSION`: set `EnableAutoUpdates=true`, set
`SettingsVersion=1`, and save the active Global Config handle. Shipped
`global.ini` contains both the enabled value and marker; the application-level
default is also true. Remove the old bootstrap repair assignment so future
bootstrap-version changes cannot overwrite a user's later opt-out.

For automatic check failures, set and save `AutoFailureNotified=true` before
showing the first dialog. Later automatic failures only log. Any fully parsed
successful check clears and saves the marker. Manual failures always show but
do not modify it. `SkippedVersion` stores normalized text, avoiding the old
packed-integer limit.

All writes use the active `GetGlobalConfig()` handle and `config_save_safe()`;
no second handle may race the application's in-memory state.

## UI

### Help and Settings

- Remove the Windows hide/disable block for `actionCheckForUpdates`.
- Update the Website and Discord handlers and the five supported locale files.
- Keep the existing About dialog structure. Prepend a shared rich-text
  `Sakyvo Present` anchor plus a spaced horizontal divider to the initial
  acknowledgements and Authors browser content. Use exact colors `#000080` and
  `#8B0000`. Resolve its destination from `OBSApp::GetLocale()` (`zh-*` uses
  Bilibili; all other locales use GitHub). Rename the support heading for
  OBS-PT, point Repository to Issues and Get Involved to Pull Requests, and do
  not alter the License page.
- Move the existing `enableAutoUpdates` widget from General into the Advanced
  page's General group. Keep its existing object name and config hook; expose
  it on Windows and remove it on unsupported platforms.

### Release notes dialog

Keep `OBSUpdate.ui` as the single dialog. Change its buttons to Update, Skip,
Ignore in that order and use the same choices for manual and automatic checks.
Render release notes with:

```cpp
document->setMarkdown(notes,
	QTextDocument::MarkdownDialectGitHub |
	QTextDocument::MarkdownNoHTML);
```

Disable in-widget navigation. Resolve anchors against the GitHub release URL
and open only `http`/`https` links with `QDesktopServices`; reject and log all
other schemes. Render the release body as published rather than filtering
headings in the client. Release descriptions omit SHA-256/asset lists; the
asset digest remains an internal download-validation field.

### Download and completion dialogs

Use a modal `QProgressDialog` with a stable filename label, 0-100 range, and
Cancel. Keep the event-driven network reply on the UI thread so progress and
cancel remain responsive.

After success, use a custom `QMessageBox` with Automatic Install, Open File
Location, and Close. `BasicOutputHandler::Active()` already covers streaming,
recording, delayed output, Replay Buffer, and virtual camera state. Disable the
install button when active and show the localized reason; recheck immediately
on click to cover hotkeys/frontend API activity.

## Download and Integrity

Resolve the Desktop through `QStandardPaths::DesktopLocation`. Derive both
paths from the validated asset name, never from an arbitrary URL path:

```text
<Desktop>/OBS-PT-x.y.z-Installer.exe.part
<Desktop>/OBS-PT-x.y.z-Installer.exe
```

Before starting, remove a stale `.part` or fail if it cannot be removed. Stream
each `readyRead()` chunk to `QFile`, update `QCryptographicHash::Sha256`, and
track bytes written. Cancellation calls `abort()` and is distinguished from an
error so it closes silently.

Asset redirects use user-verified redirect handling. Allow only HTTPS GitHub
and GitHub release-asset hosts; reject scheme downgrades or other hosts. At
`finished`, require a successful HTTP result, no pending unread/write error,
the exact API size, and the exact digest. Close the file before promotion.

Use Windows `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING |
MOVEFILE_WRITE_THROUGH)` for same-volume atomic promotion of `.part` to the
final Desktop filename. Every earlier failure closes and removes `.part`; an
existing validated/final installer remains untouched until promotion.

## Installer Launch and Process Guard

Derive Install Root with `GetConfigPath(..., "")`. Launch the validated final
file with `ShellExecuteExW`, normal visibility, and one final parameter:

```text
/D=<current Install Root>
```

Do not pass `/S`. Only after launch succeeds, queue `OBSBasic::close()` so the
normal close path saves state and tears down OBS. If launching fails, keep the
file and do not close OBS-PT.

At the first line of the NSIS install section, before `PrepareExistingConfig`
or `File /r`, call a process probe for `OBS-PT.exe`. Use the system Windows
PowerShell path through the already bundled `nsExec` plugin and `Get-Process
-Name OBS-PT`; interpret found/not-found explicitly and fail closed on probe
errors. While found, show `MB_RETRYCANCEL|MB_ICONEXCLAMATION` and loop only on
Retry. Cancel aborts setup. Placing the gate at section entry lets the normal
installer UI open while guaranteeing no replacement begins until the old
process is gone; it also avoids a launch race while automatic installation is
closing OBS-PT.

## Localization

Add/update strings only in the five supported locale files:

- Help/About branding, Repository, and Check for Updates
- update/no-update/check-failure messages
- Update, Skip, Ignore
- progress/cancel/download and validation failures
- Automatic Install, Open File Location, Close, active-output explanation
- installer launch/shutdown failures

The NSIS installer remains English-only, matching its existing language and
the previously approved English configuration page.

## Testing Strategy

Keep strict release parsing separate from QObject/UI code and add a small
`BUILD_TESTS` model test target. Fixture coverage includes valid `x.y.z` and
`vx.y.z`, current/older/newer comparisons, draft/prerelease, malformed JSON and
versions, missing/duplicate/version-mismatched installers, invalid URLs, sizes,
and digest formats.

Runtime verification covers:

- exact Help destinations and five-language layouts;
- Advanced setting migration, opt-out persistence, and startup behavior;
- manual current-version result and offline/rate-limit error behavior;
- persisted automatic-failure suppression and success reset;
- Markdown formatting, raw HTML removal, and external-link handling;
- progress, cancel cleanup, size/hash failure cleanup, and atomic replacement;
- completion actions and all four output blockers;
- interactive `/D=` launch and normal close;
- NSIS running-process Retry/Cancel before any target files change.

The final package must follow the repository's build/install/NSIS rule and pass
`7z t` plus timestamp/SHA-256 inspection.

## Risks and Rollback

- GitHub API schema/rate-limit/network failures must degrade to a bounded error,
  never block startup.
- A bad redirect or incomplete write must never promote an executable.
- Global Config migration must be one-time; coupling it to bootstrap repair
  would repeatedly override opt-out.
- Installer process detection must fail closed, but an unavailable PowerShell
  probe could prevent installation; test it on the supported Windows image.
- If discovery is unstable, hide the automatic path while retaining the manual
  action and Desktop installer workflow; do not fall back to the OBS Project
  updater or skip digest validation.
- Rollback never restores the redundant `Sourcecode.zip`; GitHub's generated
  source archives remain authoritative.
