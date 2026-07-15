# Improve Help menu and GitHub updates

## Goal

Complete OBS-PT Help-menu branding and provide a Windows update flow that can
discover, download, validate, and launch stable OBS-PT installers from GitHub
Releases without blocking startup or risking an incomplete installation.

## Background and Evidence

- The Help actions and update action already exist in `UI/forms/OBSBasic.ui:519`
  and `UI/forms/OBSBasic.ui:1789`. Their handlers are in
  `UI/window-basic-main.cpp:6062` and `UI/window-basic-main.cpp:7584`.
- `OBSBasic::Load()` already calls `TimedCheckForUpdates()` at
  `UI/window-basic-main.cpp:1890`, but OBS-PT hides the action at
  `UI/window-basic-main.cpp:2091` and stubs both check methods at
  `UI/window-basic-main.cpp:3673`.
- The existing `OBSUpdate` dialog in `UI/win-update/update-window.cpp:4` is a
  reusable visual shell. The old transport in `UI/win-update/win-update.cpp`
  is not reusable as-is because it is bound to OBS Project manifests,
  OBS-owned signatures, a patch updater, and `LIBOBS_API_VER` rather than the
  running product's `OBS_VERSION`.
- The auto-update checkbox remains wired to Global Config at
  `UI/window-basic-settings.cpp:1235` and
  `UI/window-basic-settings.cpp:2999`, but it is hidden at
  `UI/window-basic-settings.cpp:373` and located on the General page at
  `UI/forms/OBSBasicSettings.ui:232`. The Advanced page begins at
  `UI/forms/OBSBasicSettings.ui:4795`.
- Auto updates currently default to false in `UI/obs-app.cpp:421`,
  `UI/obspt-bootstrap.cpp:320`, and
  `UI/data/obspt-defaults/obs-studio/global.ini:3`.
- The project UI language contract in `CONTEXT.md` is `en-US`, `zh-CN`,
  `zh-TW`, `ja-JP`, and `ko-KR`.
- GitHub's latest-release response for `Sakyvo/OBS-PT` supplies the stable
  release tag, Markdown body, installer asset URL, byte size, and a
  `sha256:` digest. The installer naming contract is
  `OBS-PT-<version>-Installer.exe`.
- The NSIS installer preserves existing User Data Root content by default and
  honors an explicit `/D=<Install Root>`, but
  `UI/installer/obspt-setup.nsi:194` currently begins installation without a
  running-process gate.

## Requirements

### R1. Help menu

- Keep **Help Portal** pointing to `https://obsproject.com/help`.
- Rename **Visit OBS Website** to **Visit OBS-PT Website** and point it to
  `https://github.com/Sakyvo/OBS-PT`.
- Point **Join Discord Server** to `https://discord.gg/yqDYUajJU9`.
- Keep **About** and expose the existing **Check for Updates** action.
- In the About dialog's initial acknowledgements view and Authors view, prepend
  one clickable `Sakyvo Present` line: render `Sakyvo` in `#000080` and
  `Present` in `#8B0000`, then add vertical spacing and a horizontal divider
  before the existing content. Open `https://space.bilibili.com/1049515077`
  when the active OBS locale is Chinese (`zh-*`), otherwise open
  `https://github.com/Sakyvo`.
- Rename the About-page heading **Support the OBS Project** to **Support the
  OBS-PT Project** in all five supported locales.
- Rename **Make a Contribution** to **Repository** (Chinese: **仓库地址**) and
  point it to `https://github.com/Sakyvo/OBS-PT/issues`.
- Keep the **Get Involved** label and point it to
  `https://github.com/Sakyvo/OBS-PT/pulls`. Keep **License** unchanged.
- Localize every changed Help label and every new updater string in `en-US`,
  `zh-CN`, `zh-TW`, `ja-JP`, and `ko-KR`.

### R2. Stable update discovery

- Use the GitHub Releases latest-stable endpoint for `Sakyvo/OBS-PT`; reject
  draft or prerelease payloads defensively.
- Parse the release tag and `OBS_VERSION` as strict stable semantic versions,
  accepting an optional leading `v`, and compare the product versions rather
  than `LIBOBS_API_VER`.
- Require exactly one installer asset named
  `OBS-PT-<normalized-version>-Installer.exe`. Missing, duplicate, malformed,
  or version-mismatched installer metadata is a check failure.
- Run requests asynchronously. A check in progress must not freeze the main
  window or start a duplicate request.
- A manual check is available from Help. When no newer stable release exists,
  it explicitly reports that OBS-PT is current.
- Schedule one automatic check after the main window becomes usable on every
  startup when automatic checks are enabled. A successful no-update result is
  silent for automatic checks.

### R3. Automatic-check setting and migration

- Show **Automatically check for updates on startup** under Settings >
  Advanced on Windows and default it to enabled for new installations.
- On the first updater-capable build, set the previously hidden
  `EnableAutoUpdates=false` value to true once and persist a separate migration
  marker. After that marker exists, preserve the user's explicit setting.
- Do not tie this one-time updater migration to rerunning unrelated first-run
  bootstrap defaults.

### R4. Check-result and failure policy

- A newer stable release opens the update dialog. Automatic checks honor a
  persistently skipped version; an explicit manual check may still show it.
- Manual-check failures always show a clear error dialog.
- Persist whether an automatic-check failure has already been reported. The
  first automatic failure in a consecutive failure period shows an error;
  later automatic failures across subsequent launches only log the error.
- Any later successful update check, manual or automatic, resets that failure
  marker. A future failure period may therefore notify once again.
- Manual failure dialogs neither consume nor reset the automatic-failure
  marker.

### R5. Update dialog

- Display the release notes in the dialog's middle region.
- Keep SHA-256 and asset-digest lists out of the published release notes; the
  digest is validation metadata and must not be shown in the dialog.
- Render the GitHub release body using Qt's GitHub Markdown dialect with
  `MarkdownNoHTML`; raw embedded HTML must be discarded.
- Open only safe external HTTP(S) links through the system browser rather than
  inside the dialog.
- Present these bottom actions in order:
  - **Update** downloads the selected installer.
  - **Skip** persists the current release version and suppresses automatic
    reminders until a newer release exists.
  - **Ignore** only dismisses the current dialog; the same release may be
    offered again on the next startup.

### R6. Download and validation

- Resolve the user's redirected Desktop through the operating system and
  download to `<installer-name>.part` in that directory.
- Show a responsive modal progress window containing the filename, percentage,
  and a Cancel action. Cancellation aborts the network reply and removes the
  partial file without showing a failure dialog.
- Stream the response to disk while calculating SHA-256; do not hold the whole
  installer in memory.
- Require a well-formed GitHub-provided `sha256:` digest before download. After
  download, require both the advertised byte size and SHA-256 to match.
- Missing/malformed digest, size mismatch, hash mismatch, network failure,
  write failure, or unsafe asset URL produces a clear error, removes the
  `.part` file, and never offers installation.
- Only after validation succeeds, atomically replace any same-named Desktop
  installer with the completed `.part` file.

### R7. Download completion and automatic install

- After a successful download, offer **Automatic Install**, **Open File
  Location**, and **Close** without forcing installation or deleting the
  downloaded installer.
- Automatic Install launches the normal interactive NSIS installer, never
  `/S`, and passes the running instance's current Install Root through the
  final `/D=` argument. OBS-PT then requests its normal shutdown.
- Disable Automatic Install when recording, streaming, Replay Buffer, or the
  virtual camera is active. Explain that all outputs must be stopped and keep
  the installer on the Desktop. Recheck immediately before launching and never
  stop an output automatically.
- Installer launch or normal OBS-PT shutdown failure leaves the validated
  Desktop installer available for manual use and reports the problem.

### R8. Installer running-process gate

- Before any file-copy/configuration step, the NSIS installer must detect all
  running `OBS-PT.exe` processes, including when launched manually.
- While OBS-PT is running, perform no installation. Show an English prompt
  asking the user to close OBS-PT normally, with **Retry** and **Cancel**.
- Retry only repeats process detection. Continue only after no OBS-PT process
  remains; Cancel/close exits setup. Never force-terminate OBS-PT.

### R9. Release assets

- Manually publish only `OBS-PT-<version>-Installer.exe`; do not create or
  upload a redundant `Sourcecode.zip`. GitHub's automatic source ZIP and
  tarball remain the source archives.
- The redundant `Sourcecode.zip` was removed from release `1.0.0` during
  planning; its remaining manual asset is `OBS-PT-1.0.0-Installer.exe`.

## Acceptance Criteria

- [ ] AC1: Help Portal, Visit OBS-PT Website, Discord, About, and Check for
      Updates appear with R1 labels and open the exact required destinations.
- [ ] AC1a: The About initial and Authors views show the blue/red
      `Sakyvo Present` link with the exact requested colors, spacing, and
      divider above their existing content and route `zh-*` to Bilibili and
      every other locale to GitHub; the heading names OBS-PT, Repository and
      Get Involved open the exact Issues/Pulls destinations, and License is
      unchanged.
- [ ] AC2: All changed/new Help and updater strings are complete in the five
      supported locales.
- [ ] AC3: Manual and startup checks are asynchronous, non-duplicating, and
      only a stable version newer than `OBS_VERSION` triggers the dialog.
- [ ] AC4: Manual no-update and failure results are explicit; automatic
      no-update is silent; automatic failures follow the persisted
      first-failure/reset cycle in R4.
- [ ] AC5: The Advanced auto-check setting defaults/migrates to enabled once,
      then preserves later user choices.
- [ ] AC6: The update dialog safely renders Markdown without a checksum
      appendix and implements Update, Skip, and Ignore with the persistence
      semantics in R5.
- [ ] AC7: Desktop download progress is visible and cancellable; success
      requires the expected size and GitHub SHA-256; every unsuccessful path
      removes `.part` and cannot reach installation.
- [ ] AC8: A validated download atomically replaces the same-named Desktop
      installer and offers Automatic Install, Open File Location, and Close.
- [ ] AC9: Automatic Install is unavailable while any named output is active,
      never force-stops outputs, launches a non-silent installer with the
      current Install Root, and requests normal OBS-PT shutdown.
- [ ] AC10: Starting setup while OBS-PT runs cannot reach installation;
      Retry continues only after process exit, Cancel exits, and no path kills
      the process.
- [x] AC11: Release `1.0.0` no longer contains the redundant `Sourcecode.zip`.
- [ ] AC12: Network/API errors, invalid JSON/version/asset metadata, unsafe
      URLs, missing/malformed digests, file errors, and installer-launch errors
      have localized user-facing outcomes and detailed logs.
- [ ] AC13: A fresh NSIS installer is built after binary verification and has
      a current timestamp under `build-v143/_pkg/`.

## Out of Scope

- Installing or notifying for GitHub prereleases or draft releases.
- Replay Buffer update support beyond treating an active Replay Buffer as an
  automatic-install blocker.
- A custom update service, delta patching, the legacy OBS updater executable,
  silent/unattended installation, or forced output/process termination.
- Code signing infrastructure beyond mandatory verification of GitHub's
  release-asset SHA-256 digest.
