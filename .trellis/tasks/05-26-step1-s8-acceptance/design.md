# Step 1 S8: Acceptance Remediation Design

**Status**: Planning
**Owner**: Sakyvo
**Created**: 2026-05-27

## Goal

Fix the B6 manual acceptance blockers found in the built portable package:

- OBSRedux must start in the OS-preferred UI language instead of forcing `en-US`.
- Upstream OBS update checks and first-run Auto-Configuration Wizard must not appear.
- PotPvP Profile / Scene Collection must be active before OBS loads `basic.ini`.
- User-visible branding must say `OBSRedux`, especially the main window title.
- Recordings must live at `<Install Root>/recordings/`.
- Existing bad dev deployments must be repaired once via `BootstrapVersion`.

This is still planning. Implementation starts only after review and `task.py start`.

## Root Cause

The current package installed the PotPvP files, but OBSRedux did not fully own the upstream first-run state machine.

The key timing bug is in `UI/window-basic-main.cpp::OBSBasic::OBSInit()`:

1. It reads `Basic.SceneCollectionFile`.
2. It calls `InitBasicConfig()` and opens the active Profile `basic.ini`.
3. Only later, after `obs_post_load_modules()`, it calls `run_first_run_bootstrap_if_needed()`.

Because the shipped `global.ini` lacked `ProfileDir=PotPvP` and `SceneCollectionFile=PotPvP`, OBS opened an upstream fallback Profile before the bootstrap touched PotPvP files. Any later rewrite of `basic/profiles/PotPvP/basic.ini` was too late for the currently opened `basicConfig`.

Therefore first-run bootstrap must be split into two phases.

## Architecture

### Phase A: Early Bootstrap

Runs before `OBSBasic::InitBasicConfig()` reads the active Profile.

Responsibilities:

- Check PotPvP preset file integrity.
- Ensure root `recordings/` exists.
- Repair or initialize `global.ini` as a complete PotPvP startup index.
- Write upstream first-run gates so Auto-Configuration Wizard and upstream version paths do not trigger.
- Disable upstream update defaults.
- Run `BootstrapVersion` migration for existing bad dev deployments.

This phase must not probe encoders because encoder plugins are not loaded yet.

Required `global.ini` shape after Phase A:

```ini
[Basic]
Profile=PotPvP
ProfileDir=PotPvP
SceneCollection=PotPvP
SceneCollectionFile=PotPvP

[General]
FirstRun=true
LastVersion=<LIBOBS_API_VER>
EnableAutoUpdates=false

[OBSRedux]
BootstrapVersion=2
```

`Language` must not be written by the preset file or bootstrap unless the user later chooses a language. This lets upstream `GetPreferredLocales()` select the Windows UI language.

### Phase B: Late Bootstrap

Runs after `obs_post_load_modules()` and before outputs are reset.

Responsibilities:

- Probe record encoder with loaded modules.
- Rewrite `recordEncoder.json::encoder` for the active PotPvP Profile.
- Rewrite `[AdvOut] RecFilePath` and `[SimpleOutput] FilePath` to `<Install Root>/recordings`.
- Show the OBSRedux first-run confirmation dialog.
- After the dialog closes, write `[OBSRedux] FirstRunCompleted=true`.

Dialog close via `X` is equivalent to the primary "start" action.

### Bootstrap Versioning

Introduce a constant such as:

```cpp
static const int OBSREDUX_BOOTSTRAP_VERSION = 2;
```

Migration rule:

- If `FirstRunCompleted` is missing or false: treat as not initialized and overwrite to PotPvP defaults.
- If `FirstRunCompleted=true` but `BootstrapVersion < OBSREDUX_BOOTSTRAP_VERSION`: run the S8 migration once.
- If both are current: do not overwrite user choices.

Because OBSRedux is not released yet, this S8 migration may overwrite old dev/test Profile and Scene selections back to PotPvP.

## Preset Integrity

The following files are required:

- `obs-studio/global.ini`
- `obs-studio/basic/profiles/PotPvP/basic.ini`
- `obs-studio/basic/profiles/PotPvP/recordEncoder.json`
- `obs-studio/basic/profiles/PotPvP/streamEncoder.json`
- `obs-studio/basic/profiles/PotPvP/service.json`
- `obs-studio/basic/scenes/PotPvP.json`

Validation expectations:

- INI files open with OBS `config_open`.
- JSON files open with OBS JSON APIs.
- Missing or parse failure is fatal.

Failure UX:

- Show a fatal OBSRedux dialog.
- Report the first missing or damaged file.
- Tell the user to re-extract the OBSRedux package.
- Offer only "open install directory" and "exit".
- Do not continue into the main window.

## Install Layout

Target layout:

```text
<Install Root>/
  bin/64bit/obs64.exe
  data/
  obs-studio/
    global.ini
    basic/profiles/PotPvP/
    basic/scenes/PotPvP.json
  recordings/
```

`obs-studio/` remains the User Data Root by project terminology. It is not renamed in S8.

`recordings/` is a root sibling of `obs-studio/`, because recordings are user output, not configuration data.

## Updater

Keep upstream updater source code, but disconnect runtime entry points:

- `TimedCheckForUpdates()` must not trigger upstream update checks.
- Default `EnableAutoUpdates=false`.
- Hide the "Check for Updates" menu action.
- Do not delete `AutoUpdateThread` or update dialog classes.

Future OBSRedux release channels can reuse these structures with an OBSRedux-owned feed.

## Branding

Scope is minimal user-visible branding, not broad internal renaming.

Must change:

- Main window title to `OBSRedux <version> - Profile: ... - Scenes: ...`.
- Remove `Studio` from the title even in Studio Mode.
- Remove `Portable Mode` from the title.
- About description must describe OBSRedux and state that it is an OBS fork.
- Obvious user-facing OBS Studio branding in dialogs owned by this task must become OBSRedux.

Must not change in S8:

- `obs64.exe`
- `obs-studio/`
- OBS class names and function names
- Plugin/API identifiers
- Author/license attribution

## Data Flow

```text
Process start
  -> AppInit loads global config and locale
  -> OBSBasic::OBSInit starts
  -> Phase A early bootstrap repairs global.ini and validates presets
  -> InitBasicConfig opens PotPvP basic.ini
  -> ResetAudio / ResetVideo
  -> obs_load_all_modules / obs_post_load_modules
  -> Phase B late bootstrap probes encoder and rewrites PotPvP files
  -> first-run dialog closes
  -> write FirstRunCompleted=true and BootstrapVersion=2
  -> main window loaded
```

## Validation Strategy

After implementation:

1. Rebuild and install to `dist-test/`.
2. Cover `D:\OBSRedux\` to validate old bad dev deployment migration.
3. Clean deploy to `K:\Projects\finished\OBS-Redux\` to validate new user first-run.
4. Confirm no AppData writes, no updater popup, no Auto-Configuration Wizard, correct title, correct language, correct PotPvP settings, correct recordings path, and no second first-run dialog.

## Follow-Up Specs

After implementation and verification, update `.trellis/spec/obsredux-bootstrap.md`. Its current M4 timing section is stale because it describes a single late bootstrap call; S8 changes the contract to a split early/late bootstrap.
