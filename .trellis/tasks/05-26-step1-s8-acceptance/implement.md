# Step 1 S8: Implementation Plan

**Status**: Planning
**Owner**: Sakyvo
**Created**: 2026-05-27

This plan is for the next implementation phase. Do not run `task.py start` until the planning artifacts are reviewed and approved.

## Preconditions

Before code edits:

1. Run Trellis before-dev guidance for the affected UI/bootstrap layer.
2. Re-read:
   - `CONTEXT.md`
   - `.trellis/spec/obsredux-bootstrap.md`
   - `.trellis/tasks/05-26-step1-s8-acceptance/prd.md`
   - `.trellis/tasks/05-26-step1-s8-acceptance/design.md`
3. Confirm working tree status and avoid reverting unrelated changes.

## Implementation Checklist

### 1. Preset Layout

Files:

- `UI/data/obsredux-defaults/obs-studio/global.ini`
- `UI/data/obsredux-defaults/obs-studio/recordings/README-recordings.txt`
- `UI/data/obsredux-defaults/recordings/README-recordings.txt`

Steps:

1. Remove `Language=en-US` from preset `global.ini`.
2. Add complete PotPvP startup index:
   - `Profile=PotPvP`
   - `ProfileDir=PotPvP`
   - `SceneCollection=PotPvP`
   - `SceneCollectionFile=PotPvP`
3. Add static updater gate where appropriate:
   - `EnableAutoUpdates=false`
   - `FirstRun=true`
4. Do not hardcode `LastVersion` in the preset unless the build already has a safe generated value. Prefer runtime write from `LIBOBS_API_VER`.
5. Move the recordings placeholder out of `obs-studio/recordings/` into root `recordings/`.
6. Confirm `cmake --install` copies `UI/data/obsredux-defaults/recordings/` to `dist-test/recordings/`.

### 2. Early Bootstrap API

Files:

- `UI/obsredux-bootstrap.h`
- `UI/obsredux-bootstrap.cpp`
- `UI/window-basic-main.cpp`

Add early bootstrap entry point, for example:

```cpp
void run_obsredux_early_bootstrap(void);
```

Responsibilities:

1. Determine whether repair is needed:
   - `FirstRunCompleted` missing/false
   - or `BootstrapVersion < OBSREDUX_BOOTSTRAP_VERSION`
2. Validate required preset files:
   - `obs-studio/global.ini`
   - `obs-studio/basic/profiles/PotPvP/basic.ini`
   - `obs-studio/basic/profiles/PotPvP/recordEncoder.json`
   - `obs-studio/basic/profiles/PotPvP/streamEncoder.json`
   - `obs-studio/basic/profiles/PotPvP/service.json`
   - `obs-studio/basic/scenes/PotPvP.json`
3. Create/validate root `recordings/`.
4. If repair is needed, overwrite development-period startup index:
   - `[Basic] Profile=PotPvP`
   - `[Basic] ProfileDir=PotPvP`
   - `[Basic] SceneCollection=PotPvP`
   - `[Basic] SceneCollectionFile=PotPvP`
5. Write upstream gates:
   - `[General] FirstRun=true`
   - `[General] LastVersion=LIBOBS_API_VER`
   - `[General] EnableAutoUpdates=false`
6. Write `[OBSRedux] BootstrapVersion=2` after the early repair succeeds.
7. Do not write `[OBSRedux] FirstRunCompleted=true` in this phase.

Call site:

- Must run in `OBSBasic::OBSInit()` before the code reads `SceneCollectionFile` and before `InitBasicConfig()`.

### 3. Integrity Failure Dialog

Files:

- `UI/obsredux-bootstrap.h`
- `UI/obsredux-bootstrap.cpp`
- `UI/data/locale/en-US.ini`
- `UI/data/locale/zh-CN.ini`

Steps:

1. Add a fatal OBSRedux dialog for preset integrity failure.
2. Include the first missing or invalid file path.
3. Tell the user to re-extract the OBSRedux package.
4. Provide only:
   - Open install directory
   - Exit
5. Exit before the main window continues.

Keep translations targeted to `en-US` and `zh-CN` for S8 unless existing OBSRedux keys require more.

### 4. Late Bootstrap

Files:

- `UI/obsredux-bootstrap.cpp`
- `UI/obsredux-bootstrap.h`
- `UI/window-basic-main.cpp`

Refactor current `run_first_run_bootstrap_if_needed()` into late-only behavior:

1. Skip if `FirstRunCompleted=true` and `BootstrapVersion` is current.
2. Probe encoder only after `obs_post_load_modules()`.
3. Rewrite `recordEncoder.json::encoder`.
4. Rewrite `[AdvOut] RecFilePath` and `[SimpleOutput] FilePath` to `<Install Root>/recordings`.
5. Show first-run dialog.
6. Treat button click and `X` close as accepted.
7. After dialog closes, write:
   - `[OBSRedux] FirstRunCompleted=true`
   - `[OBSRedux] BootstrapVersion=2`

If Phase A already ran only a version migration and `FirstRunCompleted=true`, do not show the first-run dialog again unless PRD says otherwise. The migration should silently repair old bad dev state.

### 5. Updater Runtime Disconnection

Files:

- `UI/obs-app.cpp`
- `UI/window-basic-main.cpp`
- `UI/window-basic-settings.cpp`
- `UI/forms/OBSBasic.ui`
- `UI/forms/OBSBasicSettings.ui` if needed

Steps:

1. Change default `EnableAutoUpdates` to false.
2. Make `OBSBasic::TimedCheckForUpdates()` return without starting upstream update checks.
3. Hide `ui->actionCheckForUpdates` after UI setup, or remove it from the Help menu while keeping the action object safe for existing code.
4. Keep `AutoUpdateThread` and updater dialog code in the tree.
5. Consider hiding/disabling the Settings checkbox for auto updates so users do not enable a dead upstream path. If hidden, make sure layout remains stable.

### 6. Auto-Configuration Wizard First-Run Suppression

Files:

- `UI/window-basic-main.cpp`
- `UI/obsredux-bootstrap.cpp`

Steps:

1. Ensure early bootstrap writes `General.FirstRun=true`.
2. Ensure early bootstrap writes `General.LastVersion=LIBOBS_API_VER`.
3. Do not remove manual menu action for Auto-Configuration Wizard.
4. Confirm `on_autoConfigure_triggered()` still works when manually invoked.

### 7. Branding

Files:

- `UI/window-basic-main.cpp`
- `UI/data/locale/en-US.ini`
- `UI/data/locale/zh-CN.ini`
- `UI/forms/OBSAbout.ui` only if needed

Steps:

1. Change title generation to:

   ```text
   OBSRedux <version> - Profile: <profile> - Scenes: <sceneCollection>
   ```

2. Remove title insertion of `Studio`.
3. Remove title insertion of `Portable Mode`.
4. Update startup log branding only if it is user-visible in screenshots/log review; keep this secondary.
5. Update About description to say OBSRedux is an OBS fork for PotPvP recording.
6. Preserve OBS Project author/license attribution.
7. Do not rename internal classes, functions, plugin IDs, `obs64.exe`, or `obs-studio/`.

### 8. Recording Path Consistency

Files:

- `UI/obsredux-bootstrap.cpp`
- `UI/data/obsredux-defaults/obs-studio/basic/profiles/PotPvP/basic.ini`
- install preset directories

Steps:

1. Keep placeholder values in `basic.ini` acceptable for before-runtime inspection.
2. At runtime, always write absolute `<Install Root>/recordings` into both:
   - `[AdvOut] RecFilePath`
   - `[SimpleOutput] FilePath`
3. Ensure root `recordings/` exists before writing these values.
4. Verify first-run dialog path matches the same root directory.

### 9. Build And Install

Commands:

```powershell
cmake --build build64 --config RelWithDebInfo
cmake --install build64 --prefix dist-test --config RelWithDebInfo
```

Expected layout:

```text
dist-test/
  bin/64bit/obs64.exe
  obs-studio/global.ini
  obs-studio/basic/profiles/PotPvP/
  obs-studio/basic/scenes/PotPvP.json
  recordings/
```

### 10. Manual B6 Validation

#### Path A: Old Bad Dev Deployment Migration

Target:

```text
D:\OBSRedux\
```

Steps:

1. Close any running OBSRedux.
2. Copy new `dist-test/` over `D:\OBSRedux\`.
3. Launch `D:\OBSRedux\bin\64bit\obs64.exe`.
4. Verify:
   - No upstream OBS update popup.
   - No Auto-Configuration Wizard.
   - Title starts with `OBSRedux`.
   - Title contains no `Studio` and no `Portable Mode`.
   - `global.ini` has PotPvP startup index and `BootstrapVersion=2`.
   - Settings shows PotPvP Advanced output, 360 fps, CQP 25, and `D:\OBSRedux\recordings`.

#### Path B: Clean Install

Target:

```text
K:\Projects\finished\OBS-Redux\
```

Steps:

1. Remove or empty the target directory after confirming it is the intended finished output path.
2. Copy `dist-test/` into that directory.
3. Launch `K:\Projects\finished\OBS-Redux\bin\64bit\obs64.exe`.
4. Verify:
   - OS-preferred language is used.
   - No upstream OBS update popup.
   - No Auto-Configuration Wizard.
   - First-run OBSRedux dialog shows `<Install Root>\recordings`, CQP 25, and 360 fps.
   - Closing with `X` marks first run complete.
   - Second launch does not show the first-run dialog.
   - Recording with PageDown creates mp4 under `K:\Projects\finished\OBS-Redux\recordings\`.

### 11. Residual Checks

Run or inspect:

```powershell
git diff -- UI/obsredux-bootstrap.cpp UI/obsredux-bootstrap.h UI/window-basic-main.cpp UI/obs-app.cpp UI/window-basic-settings.cpp UI/forms/OBSBasic.ui UI/data/locale/en-US.ini UI/data/locale/zh-CN.ini UI/data/obsredux-defaults
```

Confirm:

- No broad `OBS` string replacement.
- No `obs-studio/` rename.
- No `obs64.exe` rename.
- No unrelated formatting churn.
- No AppData writes during manual validation.

## Risk Points

- Early bootstrap must run before `SceneCollectionFile` and `InitBasicConfig()` are read.
- Late bootstrap must still run after encoder modules are loaded.
- Hiding update actions must not leave null-pointer use in `CheckForUpdates()` or `updateCheckFinished()`.
- Moving `recordings/` must not delete unrelated user recordings during validation.
- `BootstrapVersion` migration is intentionally aggressive only because this is pre-release/dev data.

## Done Criteria

Planning is ready for implementation when:

- `prd.md`, `design.md`, and `implement.md` are reviewed.
- User approves entering implementation.
- `task.py start` or equivalent Trellis transition is run.

Implementation is done when:

- Build succeeds.
- Install succeeds.
- Path A and Path B manual validation pass.
- Relevant specs are updated after implementation.
- Local git commit is created after code changes, per project instructions.
