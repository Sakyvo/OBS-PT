# Implement: AMD AMF defaults and primary-display bootstrap

## Checklist

1. Load implementation context.
   - Run `trellis-before-dev` before production edits.
   - Read `prd.md`, `design.md`, `CONTEXT.md`,
     `.trellis/spec/obspt-bootstrap.md`, the bootstrap files, shipped Global
     Config, AMF property/default code, five AMF locale files, `.gitmodules`,
     and relevant CMake/installer files.
   - Preserve the existing updater/About worktree changes.

2. Establish the public AMF fork without task commits.
   - Create public `Sakyvo/obs-amd-encoder` from the archived upstream using
     `gh repo fork ... --clone=false`.
   - Add the fork as a submodule remote and create local branch `obs-pt` from
     existing custom commit `0cebebc`.
   - Push the already-committed `0cebebc` ancestry so the current parent gitlink
     becomes fetchable; do not commit this task's new code before human review.
   - Change parent `.gitmodules` URL to the Sakyvo fork and run `git submodule
     sync -- plugins/enc-amf`.

3. Add a focused static contract check before implementation.
   - Add a lightweight PowerShell or CMake-script check only if it fits existing
     test patterns; otherwise record reproducible `rg`/config assertions in the
     validation log rather than introducing a one-off framework.
   - Assert the packaged markers are incomplete, the AMF preset list exposes
     exactly one item named `OBS-PT`, and the AMF branch includes all approved
     keys and 22/27 mapping.

4. Restore fresh-install Global Config state.
   - Change only `[OBSPT] BootstrapVersion` to `0` and
     `FirstRunCompleted` to `false` in the shipped default.
   - Verify GUI geometry, DockState, stats visibility, updater defaults, profile,
     scene collection, paths, hotkeys, and replay settings are unchanged.

5. Implement encoder-specific quality selection.
   - Add a local helper in `UI/obspt-bootstrap.cpp`:
     `amd_amf_h264` maps `<1080 -> 22`, otherwise `27`; all other encoders keep
     `<1080 -> 20`, otherwise `26`.
   - Pass that selected value to `apply_encoder_to_profile()`.
   - In the AMF branch, remove hardcoded 26, use the selected value for I/P/B,
     and write exact `AMF.H264.*` plugin property IDs, including
     `AMF.H264.Preset=-1`, while preserving High/CQP/Speed and the existing
     VBV/B-frame performance settings.
   - When encoder JSON saves successfully, write the probed encoder ID to the
     active Profile's `[AdvOut] RecEncoder` before `ResetOutputs()`.
   - Do not change 480 FPS or other encoder templates.

6. Narrow the AMF top-level Preset list.
   - Add `AMF_H264_PRESET_OBSPT` mapped to `Preset.OBSPT`.
   - Replace all top-level template list additions with one
     `Presets::None (-1)` entry named `OBS-PT`.
   - Add `AMF.H264.Preset.OBSPT="OBS-PT"` to en-US, zh-CN, zh-TW, ja-JP,
     and ko-KR plugin locales.
   - Leave the separate Quality Preset control and its Speed/Balanced/Quality
     entries intact.

6a. Bound automatic AMF H.264 Profile Level selection.
   - Remove the invalid `-1` sentinel from the `uint8_t` Profile Level table
     and use bounded iteration.
   - Use overflow-aware 64-bit sample calculations and return Level 5.2 when a
     resolution/rate pair exceeds every table row.
   - Add and run a focused executable test for common levels, the Level 5.2
     boundary, 2560x1440 at 240/256 FPS, and a zero denominator.

7. Run static checks.
   - `git diff --check` in the parent and submodule.
   - Verify only intended Global Config marker lines changed from the packaged
     layout baseline.
   - Count one top-level Preset item, confirm no named template item is added,
     and confirm five locale values are exact `OBS-PT`.
   - Verify AMF JSON uses only exact `AMF.H264.*` property IDs, contains no
     unprefixed AMF aliases, and covers both sides of the 1080 threshold;
     verify other encoder quality values remain 20/26.

8. Build and stage.

   ```powershell
   cmake --build build-v143 --config RelWithDebInfo --target enc-amf --parallel
   cmake --build build-v143 --config RelWithDebInfo --target obs --parallel
   cmake --install build-v143 --config RelWithDebInfo --prefix K:/Projects/dev/OBS-PT/build-v143/_pkg/OBS-PT
   ```

   - Compare built/rundir/staged `enc-amf.dll` hashes.
   - Compare built/staged `OBS-PT.exe` hashes.
   - Confirm staged Global Config and five AMF locale files contain the new
     values.

9. Run a disposable fresh-install smoke.
   - Copy the staged portable tree to an ignored workspace smoke root; never
     mutate staging or `D:/OBS-PT` for this test.
   - Launch and inspect the log for `applied monitor video`, 480/1 FPS, and the
     physical Windows primary-display size.
   - Verify smoke `basic.ini` Base=Output, welcome behavior, and completion
     marker lifecycle. Preserve the no-screen fallback.
   - Record that AMF UI/runtime still requires the AMD tester.

10. Build and verify the installer.
    - Assert no `makensis.exe` process exists, invoke it once with a timeout
      longer than full compression, and wait for normal exit.
    - Run `7z t`, record timestamp/size/SHA-256, and actually launch the
      build-path installer to `OBS-PT 1.0.0 Setup`.
    - Copy to Desktop, compare exact size/SHA-256, and launch the Desktop copy
      to the same Setup window. Never overlap packagers writing the fixed
      `OutFile`.

11. AMD tester acceptance.
    - Clean-install on the reported AMD/2560x1440 machine.
    - Verify first-launch Base=Output=2560x1440 at 480 FPS and welcome appears.
    - Verify AMF is selected without user action; top-level Preset has only `OBS-PT`; Quality Preset
      is Speed; Profile High; rate control CQP; I/P/B QP 27; fields remain
      editable.
    - Run a short record/start/stop smoke and retain the 30-second AMF stop
      watchdog behavior.
    - If available, verify a sub-1080 AMD Profile writes I/P/B QP 22.

12. Source publication and finish gate.
    - After human review, commit the AMF submodule changes on `obs-pt`, push to
      `Sakyvo/obs-amd-encoder`, and verify the commit via `git ls-remote`.
    - Update the parent gitlink, run a clean recursive-clone/fetch check, and
      update `.trellis/spec/obspt-bootstrap.md` with final contracts.
    - Run `trellis-check`, repeat affected static/build/package checks, and
      present results.
    - Only after approval run `trellis-finish-work`; it authorizes the parent
      commit/archive under project rules.

## Runtime Acceptance Result

- 2026-07-13: The AMD tester confirmed the rebuilt installer resolves the
  recording-start failure after the default 2560x1440 AMF settings had already
  been accepted. The invalid `ProfileLevel=249` path no longer reproduces.
- The successful runtime result closes the AMD start/stop acceptance gate. The
  remaining finish work is source publication, final full-scope checking, and
  the Trellis-authorized commit/archive flow.

## Release Publication Addendum

- 2026-07-13: Rebuilt after the final source check and replaced the public
  `1.0.0` installer asset with the 43,915,579-byte build. GitHub reports digest
  `sha256:209330b8eb113787f0473e8771a695d93a9af63b0e42b181dfcfad56224532af`.
- Re-downloaded the asset from both GitHub Releases and the documented
  `ghfast.top` mirror; both hashes match the local installer and the GitHub copy
  passes `7z t`.
- Both the build-path installer and Desktop copy reached
  `OBS-PT 1.0.0 Setup`; the test windows were closed without installing.
- Updated the public Release body to a Chinese-primary changelog with an
  expandable English sublog and no displayed checksum appendix.
- Replaced local `README.rst` with Chinese-default `README.md` plus
  `README.en.md`; pushed the final source as `cc1a675` and moved the `1.0.0`
  release tag to that commit so GitHub-generated source archives match.

## Risky Files

- `.gitmodules` and the `plugins/enc-amf` gitlink
- `plugins/enc-amf/Include/enc-h264.h`
- `plugins/enc-amf/Source/enc-h264.cpp`
- five `plugins/enc-amf/Resources/locale/*.ini` files
- `UI/obspt-bootstrap.cpp`
- `UI/data/obspt-defaults/obs-studio/global.ini`
- staging and `build-v143/_pkg/OBS-PT-1.0.0-Installer.exe`

## Rollback Points

- If the fork cannot host/fetch the exact custom commit, stop before updating
  the parent gitlink; do not create another local-only dependency.
- If first-run smoke does not execute both bootstrap phases, restore markers
  and fix lifecycle state before packaging.
- If AMF property narrowing changes Quality Preset or other controls, revert
  the plugin UI patch and keep the explicit bootstrap settings until corrected.
- If installer launch reports `NSIS Error`, discard it and rerun one isolated
  packager; `7z t` alone cannot clear this gate.

## Planning Gate

This is a complex inline task. Do not run `task.py start`, create the GitHub
fork, or edit production code until the user reviews and approves `prd.md`,
`design.md`, and `implement.md`.
