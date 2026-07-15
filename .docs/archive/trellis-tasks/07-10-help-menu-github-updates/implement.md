# Implement: Help menu and GitHub updates

## Checklist

1. Load implementation context.
   - Run `trellis-before-dev` before editing production code.
   - Read `prd.md`, `design.md`, `CONTEXT.md`,
     `.trellis/spec/obspt-bootstrap.md`, and the affected Help/settings/updater
     and installer files.
   - Confirm `git status --short` and preserve unrelated user changes.

2. Add the validated GitHub release model.
   - Add focused model source/header files under `UI/win-update/`.
   - Define the normalized release/asset contracts from `design.md`.
   - Implement strict stable-version parsing for `x.y.z` / `vx.y.z`.
   - Parse GitHub JSON once and validate draft/prerelease, exact installer
     count/name/version, positive size, HTTPS GitHub URL, and `sha256:` digest.
   - Add the production files to `UI/CMakeLists.txt`.

3. Add model tests before network/UI integration.
   - Add an explicitly buildable lightweight model test target guarded by
     `BUILD_TESTS`.
   - Cover valid and leading-`v` releases, version comparison, malformed JSON
     and versions, draft/prerelease, missing/duplicate/wrong-version assets,
     unsafe URLs, invalid sizes, and missing/malformed digests.
   - Reconfigure/build/run the target and fix parser failures before wiring the
     manager.

4. Implement `OBSUpdateManager`.
   - Add one QObject controller and one `QNetworkAccessManager` on the UI
     thread; enforce the Idle/Checking/Prompt/Downloading/Ready state machine.
   - Send the fixed GitHub endpoint and required Accept, API-version, and
     OBS-PT User-Agent headers with bounded timeout/response size.
   - Parse through the model, compare with strict `OBS_VERSION`, enforce the
     startup skip policy, and distinguish manual from automatic results.
   - Emit one terminal completion signal on every path so the Help action is
     always re-enabled.
   - Abort/clean active replies and partial files during destruction without a
     blocking thread wait.

5. Implement check persistence and error policy.
   - Store normalized `SkippedVersion` and `AutoFailureNotified` under
     `[OBSPTUpdater]` on the active Global Config handle.
   - Show every manual failure.
   - Show/persist only the first automatic failure in a consecutive failure
     period; log later failures.
   - Clear/persist the failure marker after any successful parsed check.
   - Ensure manual failures do not alter the automatic marker.

6. Integrate Help/startup actions.
   - Change Website and Discord URLs in `UI/window-basic-main.cpp`.
   - Add the locale-aware blue/red `Sakyvo Present` link above the About
     acknowledgements and Authors content with exact colors, a divider, and
     increased spacing; rename the support heading for OBS-PT and update
     Repository/Get Involved URLs while leaving License unchanged.
   - Expose and enable `actionCheckForUpdates` on Windows.
   - Replace the update thread member/waits with manager ownership.
   - Make `CheckForUpdates(true)` start a manual check and
     `TimedCheckForUpdates()` queue one enabled startup check after the main
     window is usable.
   - Keep non-Windows updater behavior disabled and leave What's New thread
     behavior unchanged.

7. Implement one-time default migration.
   - Change the application default and shipped `global.ini` to
     `EnableAutoUpdates=true`.
   - Add `[OBSPTUpdater] SettingsVersion=1` to shipped defaults.
   - In `prepare_obspt_global_config()`, independently migrate marker versions
     below 1 by enabling auto updates once and saving the active handle.
   - Remove auto-update assignment from generic bootstrap repair so future
     `OBSPT_BOOTSTRAP_VERSION` changes preserve opt-out.
   - Verify old config, fresh config, opt-out-after-migration, and later stale
     bootstrap cases.

8. Update Settings and release-notes UI.
   - Move `enableAutoUpdates` into the Advanced page's General group while
     keeping its object name/config hook.
   - Remove the Windows hide/disable code.
   - Change `OBSUpdate.ui` button order and labels to Update, Skip, Ignore.
   - Update `OBSUpdate` to render GitHub Markdown with `MarkdownNoHTML`, use a
     release base URL, and hand safe HTTP(S) links to the system browser.
   - Remove the old manual-only deletion of Skip; all newer-release dialogs use
     the same three actions.

9. Implement Desktop streaming download.
   - Resolve Desktop with `QStandardPaths` and derive final/`.part` names only
     from the validated asset.
   - Reject unsafe redirects; allow only HTTPS GitHub release hosts.
   - Stream `readyRead()` data to `QFile` and `QCryptographicHash::Sha256` while
     updating a modal `QProgressDialog`.
   - Cancel by aborting and silently deleting `.part`.
   - On completion, require successful HTTP, all writes, exact byte count, and
     exact SHA-256; localize/log each failure.
   - Atomically promote with `MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH)` only
     after validation.

10. Implement completion actions and installer launch.
    - Show Automatic Install, Open File Location, and Close after promotion.
    - Use `BasicOutputHandler::Active()` to disable installation while any
      recording/stream/delay/Replay Buffer/virtual-camera output is active and
      recheck on click.
    - Derive current Install Root through `GetConfigPath(""...)`.
    - Launch the validated installer visibly with `ShellExecuteExW` and final
      `/D=<Install Root>` only; never use `/S`.
    - Queue normal OBS-PT close only after launch succeeds. Keep the installer
      and application open on launch failure.

11. Add the NSIS process guard.
    - Add `IsOBSRunning` using `$SYSDIR` Windows PowerShell, `Get-Process`, and
      the existing `nsExec` plugin; distinguish found, not found, and probe
      failure.
    - Call it at the first line of `SecMain`, before config moves or `File /r`.
    - Loop an English Retry/Cancel prompt while found; Cancel/close aborts and
      no branch force-terminates OBS-PT.
    - Treat probe failure as blocking with a clear error rather than proceeding.

12. Localize and statically audit.
    - Update `en-US`, `zh-CN`, `zh-TW`, `ja-JP`, and `ko-KR` for Help branding,
      update choices, progress/completion, output blockers, and all required
      errors.
    - Verify no new key is missing from any supported locale and long labels
      fit the existing dialogs/settings page.
    - Search for remaining OBS official Website/Discord URLs in the affected
      Help handlers and for any code path still instantiating
      `AutoUpdateThread`.

13. Build and automated checks.

    ```powershell
    cmake -S . -B build-v143 -DBUILD_TESTS=ON
    cmake --build build-v143 --config RelWithDebInfo --target obspt-update-model-test --parallel
    # Run the produced obspt-update-model-test.exe directly.
    cmake --build build-v143 --config RelWithDebInfo --target obs --parallel
    git diff --check
    rg --line-number "api.github.com/repos/Sakyvo/OBS-PT|OBSPTUpdater|MarkdownNoHTML|MoveFileExW" UI
    ```

14. Runtime checks.
    - Verify all Help destinations and five supported languages.
    - Verify About and Authors both show the colored author link, `zh-CN` and
      `zh-TW` route it to Bilibili, other locales route it to GitHub, and the
      Repository/Get Involved/License actions follow the R1 contract.
    - Verify the Advanced checkbox defaults on, can be disabled, and remains
      disabled after restart once migration is marked.
    - Verify manual current-version result against release `1.0.0`.
    - Simulate a failed check across launches, verify one automatic dialog,
      manual failure every time, then a successful check reset.
    - Verify Markdown lists/links, raw HTML removal, link schemes, and that the
      published release body contains no visible SHA-256 appendix.
    - Exercise download progress/cancel and confirm `.part` cleanup.
    - Exercise bad size/hash fixtures and confirm the existing final installer
      is untouched and installation is never offered.
    - Verify same-name atomic replacement and the three completion actions.
    - Verify each active output blocks Automatic Install without being stopped.
    - Verify successful launch receives the current Install Root and OBS-PT
      closes normally.
    - With OBS-PT running, start setup against a disposable Install Root and
      verify Retry/Cancel occurs before any files change; close OBS-PT, Retry,
      and verify setup can continue.

15. Stage and build the required installer.

    ```powershell
    cmake --install build-v143 --config RelWithDebInfo --prefix K:/Projects/dev/OBS-PT/build-v143/_pkg/OBS-PT
    & "C:/Program Files (x86)/NSIS/makensis.exe" "UI/installer/obspt-setup.nsi"
    7z t build-v143/_pkg/OBS-PT-1.0.0-Installer.exe
    Get-Item build-v143/_pkg/OBS-PT-1.0.0-Installer.exe | Select-Object FullName,Length,LastWriteTime
    Get-FileHash build-v143/_pkg/OBS-PT-1.0.0-Installer.exe -Algorithm SHA256
    gh release view 1.0.0 --repo Sakyvo/OBS-PT --json assets --jq '.assets[].name'
    ```

    Verify the staged package contains the rebuilt `OBS-PT.exe`, updated
    locales/default Global Config, no nested installer under `SOURCE_DIR`, and
    that the GitHub release still has no redundant `Sourcecode.zip`.

16. Spec/check/finish.
    - Use `trellis-update-spec` to record the updater migration/state and NSIS
      process-guard contracts in the existing OBS-PT bootstrap spec.
    - Run `trellis-check`, resolve findings, and repeat the focused build/runtime
      checks affected by fixes.
    - Present the fresh installer for human review; only after approval run
      `trellis-finish-work` (which authorizes commit/archive under project
      rules).

## Risky Files

- `UI/win-update/` release model, manager, and dialog files
- `UI/window-basic-main.cpp`
- `UI/window-basic-main.hpp`
- `UI/forms/OBSUpdate.ui`
- `UI/forms/OBSBasicSettings.ui`
- `UI/window-basic-settings.cpp`
- `UI/obs-app.cpp`
- `UI/obspt-bootstrap.cpp`
- `UI/data/obspt-defaults/obs-studio/global.ini`
- five `UI/data/locale/*.ini` files
- `UI/installer/obspt-setup.nsi`
- `UI/CMakeLists.txt` and the focused model-test target

## Rollback Points

- If model validation fails against real GitHub payloads, stop before enabling
  the menu/startup path; never relax digest or exact-asset validation.
- If download promotion is not atomic or cleanup is unreliable, keep Update
  disabled and retain only discovery until fixed.
- If migration overwrites a post-migration opt-out, revert the migration change
  and repair the marker logic before packaging.
- If NSIS cannot reliably detect a running process, do not ship Automatic
  Install; keep the validated Desktop download while retaining the process
  guard task as blocking.
- Do not reactivate the OBS Project manifest/patch updater as a fallback.

## Planning Gate

This is a complex inline task. Do not run `task.py start` or edit production
code until the user reviews and approves `prd.md`, `design.md`, and
`implement.md`. Inline mode does not require `implement.jsonl`/`check.jsonl`.

## Validation Log (2026-07-11)

- GitHub `1.0.0` body contains Highlights only; no `SHA256` or
  `Sourcecode.zip` text. The only manual asset remains the installer.
- The real `0.9.0 -> 1.0.0` dialog rendered through the final Highlights line
  with no checksum appendix.
- Download cancel logged `curl=42` (`CURLE_ABORTED_BY_CALLBACK`), wrote zero
  bytes in the exercised run, removed `.part`, and left the main window
  responsive without a failure dialog.
- A full real download matched GitHub size/digest and reached the completion
  actions.
- Automatic Install launched the Desktop installer with exactly
  `/D=K:\Projects\dev\OBS-PT\build-v143\_runtime\OBS-PT`, no `/S`, then the
  smoke application exited normally.
- Explicit installer `/D=C:\OBS-PT` remained `C:\OBS-PT` on the directory page.
- With `OBS-PT.exe` running, Install showed Retry/Cancel before target writes;
  Retry showed the same prompt again and Cancel exited with only the original
  sentinel in the target.
- Model tests, five-locale key parity, UI XML parsing, formal `1.0.0` build,
  `git diff --check`, and `7z t` passed.
- Final installer: `43,888,150` bytes, timestamp `2026-07-11 13:52:03`, SHA-256
  `7BAFC54DC0F20BC3736CD7E878B611821BC0DCDB2CD5AE048A799D9E21A316D5`.
- The About initial and Authors views now prepend the locale-aware blue/red
  `Sakyvo Present` link; Repository/Get Involved target the OBS-PT Issues/Pulls
  pages and License is unchanged. The main `obs` target rebuilt successfully.
- Refreshed installer after the About changes: `43,903,730` bytes, timestamp
  `2026-07-11 17:01:57`, SHA-256
  `39EE832321494AF9E4FD315741A03464B9515764377D532113CF6B82C495BD06`.
  `7z t` passed with 1,467 files, and the same hash was copied to the Desktop.
- Refined the About header after visual review: exact colors `#000080` and
  `#8B0000`, a spaced horizontal divider in both acknowledgements views, and
  five-locale `Support the OBS-PT Project` copy. The `obs` target rebuilt; the
  built/staged EXE SHA-256 is
  `F86608065AEBDE599A614C717A6B332BCAA63F3005D580B5DA22C7FF2DE8F241`.
- Latest installer after visual-review fixes: `43,903,072` bytes, timestamp
  `2026-07-11 17:24:49`, SHA-256
  `66CE85CD9E3D00E681B6104309C1CEEDAF6D035A7D11BC6D9F43AFE8EE71E18B`.
  `7z t` passed with 1,467 files and the Desktop copy has the same hash.
- The preceding installer still failed NSIS self-integrity despite `7z t`
  passing. Root cause: a short-timeout `makensis` invocation was followed by a
  second invocation without first proving the original child had exited, so
  two processes could write the fixed `OutFile`. `7z t` is therefore not a
  sufficient NSIS CRC gate.
- Rebuilt once after confirming no `makensis.exe` process existed, waited for
  normal completion, and launched both build-path and Desktop copies. Both
  reached the window title `OBS-PT 1.0.0 Setup` rather than `NSIS Error`.
  Corrected Desktop installer: `43,903,072` bytes, timestamp
  `2026-07-11 17:30:32`, SHA-256
  `4CDCFA80369D5370CAA79B2BF7E80AD8619CA26B1DA3C4633ED3F02370AC7439`.
