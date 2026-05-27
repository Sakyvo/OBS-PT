# OBSRedux Bootstrap Module Spec

**Status**: Active  
**Owner**: OBSRedux Core  
**Last Updated**: 2026-05-27

## Overview

The `obsredux-bootstrap` module provides portable installation support for OBSRedux, including path resolution, write permission probing, encoder detection, log retention, and first-run configuration.

**Location**: `UI/obsredux-bootstrap.{h,cpp}`

---

## 1. M1 — Portable Path Resolver

### Signatures

```c
// libobs/util/platform.h
void get_portable_path(char *dst, size_t size, const char *name);
const char *get_portable_path_ptr(const char *name);
```

### Contracts

**Input**:
- `name`: Relative path from Install Root (e.g., `"obs-studio/global.ini"`, `"recordings"`, `""`)
- `dst`/`size`: Output buffer for `get_portable_path`

**Output**:
- Absolute path: `<Install Root>/<name>`
- Empty `name` or `NULL` → `<Install Root>/obs-studio`
- `get_portable_path_ptr`: Returns static thread-local buffer (max 512 bytes)

**Critical Invariant**: **Passthrough semantics** — NO auto-injection of `obs-studio/` prefix. All upstream callers already pass fully-qualified names like `"obs-studio/basic/profiles/..."`.

### Design Decision: Passthrough vs Auto-Prefix

**Context**: Initial design.md proposed auto-injecting `obs-studio/` prefix to simplify caller code.

**Discovery**: Grepping upstream revealed 20+ call sites already pass prefixed names:
```c
// Existing upstream pattern
GetConfigPath(buf, size, "obs-studio/global.ini");
GetConfigPath(buf, size, "obs-studio/basic/profiles/...");
```

**Decision**: Implement passthrough `<Install Root>/<name>` with zero callsite changes. Only special-case empty/NULL name → append `obs-studio`.

**Why**: Avoids breaking existing code and matches upstream's mental model.

### Wrong vs Correct

#### Wrong
```c
// Auto-prefix (breaks existing callers)
void get_portable_path(char *dst, size_t size, const char *name) {
    snprintf(dst, size, "%s/obs-studio/%s", install_root, name);
}
```

#### Correct
```c
// Passthrough (zero callsite change)
void get_portable_path(char *dst, size_t size, const char *name) {
    if (!name || !*name)
        snprintf(dst, size, "%s/obs-studio", install_root);
    else
        snprintf(dst, size, "%s/%s", install_root, name);
}
```

---

## 2. M3 — Encoder Capability Probe

### Signatures

```c
typedef struct {
    const char *encoder_id;
    bool is_software_fallback;
} encoder_probe_result_t;

encoder_probe_result_t probe_record_encoder(void);
```

### Contracts

**Precondition**: MUST be called AFTER `obs_load_all_modules()` completes. Encoder plugins are not registered before that point.

**Priority Order**:
1. `jim_nvenc` (NVIDIA NVENC, OBS fork)
2. `obs_qsv11` (Intel Quick Sync Video)
3. `ffmpeg_nvenc` (NVIDIA NVENC, FFmpeg wrapper)
4. `amd_amf_h264` (AMD AMF H.264)
5. `obs_x264` (software fallback)

**Output**:
- `encoder_id`: First available encoder from priority list
- `is_software_fallback`: `true` if `obs_x264`, `false` otherwise

### Design Decision: `obs_enum_encoder_types` vs `obs_get_encoder_caps`

**Problem**: Need to detect if a hardware encoder is available.

**Options Considered**:
1. `obs_get_encoder_caps(id)` — Returns capability flags
2. `obs_enum_encoder_types()` + strcmp — Enumerate all registered encoders

**Decision**: Use `obs_enum_encoder_types` + strcmp.

**Why**: `obs_get_encoder_caps` returns `0` for both "encoder not registered" and "encoder registered but caps=0", making it ambiguous. Enumeration + strcmp matches upstream's `EncoderAvailable` helper pattern and provides clear signal.

### Encoder ID Confirmation

**AMD AMF**: Confirmed as `amd_amf_h264` via grep of upstream UI code:
```bash
$ grep -r 'amd_amf' UI/
UI/window-basic-settings.cpp:  if (id == "amd_amf_h264") ...
```

**Note**: `plugins/enc-amf/` submodule is currently empty in this fork. Encoder availability depends on runtime plugin loading.

### Wrong vs Correct

#### Wrong
```c
// Ambiguous signal
if (obs_get_encoder_caps("jim_nvenc") != 0)
    return "jim_nvenc";  // Can't distinguish unregistered vs zero-caps
```

#### Correct
```c
// Clear signal via enumeration
static bool encoder_registered(const char *id) {
    const char *val = nullptr;
    for (size_t i = 0; obs_enum_encoder_types(i, &val); ++i) {
        if (val && strcmp(val, id) == 0)
            return true;
    }
    return false;
}
```

---

## 3. M4 — Split First-Run Bootstrap Timing

### 1. Scope / Trigger

OBSRedux owns upstream first-run state. This bootstrap spans Global Config, preset file integrity, loaded Profile config, encoder modules, and Qt dialogs, so the timing contract is split across application startup and `OBSBasic::OBSInit()`.

### 2. Signatures

```cpp
void prepare_obsredux_global_config(config_t *global_config);
bool run_obsredux_early_bootstrap(config_t *global_config);
void run_first_run_bootstrap_if_needed(const char *active_profile_name,
                                       config_t *active_config);
```

### 3. Contracts

**AppInit preparation**:
- Runs after `OBSApp::InitGlobalConfig()` and before `OBSApp::InitLocale()`.
- Must run only when shipped `obs-studio/global.ini` existed at startup.
- Repairs `[Basic] Profile/ProfileDir/SceneCollection/SceneCollectionFile` to `PotPvP` only when `[OBSRedux] FirstRunCompleted` is missing/false or `BootstrapVersion` is stale.
- Writes upstream gates `General.FirstRun=true`, `General.LastVersion=LIBOBS_API_VER`, and `General.EnableAutoUpdates=false`.
- Must not write `[OBSRedux] FirstRunCompleted=true`.
- Must not finalize `[OBSRedux] BootstrapVersion=<current>` until early bootstrap succeeds.

**Early bootstrap**:
- `main()` must preflight `validate_obsredux_preset_files()` before `upgrade_settings()`. If a required preset is missing or invalid, skip `upgrade_settings()` so upstream encoder migration cannot recreate missing JSON before the fatal OBSRedux check runs.
- Runs inside `OBSApp::AppInit()` after `InitLocale()` and before `InitTheme()`, Basic fallback defaults, `move_basic_to_profiles()`, `move_basic_to_scene_collections()`, and `MakeUserProfileDirs()`.
- This is earlier than `OBSBasic::OBSInit()` reading `SceneCollectionFile` or opening `basic.ini`, so repaired Global Config is active for the first real Profile load.
- Validates these required files: `obs-studio/global.ini`, `basic/profiles/PotPvP/basic.ini`, `recordEncoder.json`, `streamEncoder.json`, `service.json`, and `basic/scenes/PotPvP.json`.
- Creates `<Install Root>/recordings`.
- Writes `BootstrapVersion` only after the required files validate and `recordings` exists.

**Late bootstrap**:
- Runs inside `OBSBasic::OBSInit()` after `obs_post_load_modules()`.
- Receives the already opened `basicConfig` as `active_config`.
- Probes encoders, rewrites `recordEncoder.json::encoder`, and writes absolute `<Install Root>/recordings` to both `[AdvOut] RecFilePath` and `[SimpleOutput] FilePath`.
- Shows the first-run dialog only when `[OBSRedux] FirstRunCompleted` is missing/false.
- Writes `[OBSRedux] FirstRunCompleted=true` to the active `OBSApp::globalConfig` after the dialog closes, then saves it. Do not write this marker only through a separate `config_open()` handle; later shutdown saves can overwrite that file with the in-memory Global Config.

### 4. Validation & Error Matrix

| Condition | Required behavior |
|---|---|
| `global.ini` missing at process start | Show OBSRedux preset failure dialog and exit; do not silently create a reusable empty Global Config |
| Required preset INI/JSON missing or invalid | Show OBSRedux preset failure dialog with the first failed path and exit before main window |
| Required encoder JSON missing before `upgrade_settings()` | Skip `upgrade_settings()`, show OBSRedux preset failure later in `AppInit`, and leave the missing file absent |
| `recordings/` cannot be created | Show OBSRedux preset failure dialog and exit before main window |
| `BootstrapVersion` stale but early validation fails | Leave `BootstrapVersion` stale so the next launch retries migration |
| `FirstRunCompleted=true` and `BootstrapVersion` current | Preserve user Profile / Scene Collection choices |

### 5. Good/Base/Bad Cases

- Good: shipped Global Config exists, stale bootstrap version is repaired to PotPvP, early validation passes, then `BootstrapVersion` is written.
- Base: `FirstRunCompleted=true` and current bootstrap version; early validation still checks the package, but no user choices are overwritten.
- Bad: missing `global.ini` is opened with `CONFIG_OPEN_ALWAYS`, saved, and then accepted on the next launch as an empty upstream-style config.
- Bad: missing `recordEncoder.json` is recreated by `upgrade_settings()` before OBSRedux integrity validation sees the damaged package.
- Bad: `[OBSRedux] FirstRunCompleted=true` is written through a second config handle, then removed when shutdown saves the stale in-memory Global Config.

### 6. Tests Required

- Missing `global.ini`: launch once and assert the file is still absent after exit.
- Missing `recordEncoder.json`: launch once and assert the fatal window title appears and the missing file is not recreated.
- Stale `BootstrapVersion` with valid presets: assert PotPvP startup index, upstream gates, and current `BootstrapVersion`.
- Stale `BootstrapVersion` with a missing required preset: assert process exits and `BootstrapVersion` remains stale.
- Fresh first run: assert late bootstrap writes `FirstRunCompleted=true` only after the first-run dialog closes and that the marker remains after shutdown.

### 7. Wrong vs Correct

#### Wrong

```cpp
globalConfig.Open(path, CONFIG_OPEN_ALWAYS);
prepare_obsredux_global_config(globalConfig); // can save a synthetic global.ini
config_set_int(globalConfig, "OBSRedux", "BootstrapVersion", CURRENT);
```

#### Correct

```cpp
if (global_ini_missing_at_startup)
    exit_with_preset_failure();

prepare_obsredux_global_config(globalConfig);

if (!run_obsredux_early_bootstrap(globalConfig))
    return 1;
// early bootstrap writes BootstrapVersion only after validation succeeds
```

```cpp
// Before upstream upgrade_settings()
char failed[512] = {};
if (validate_obsredux_preset_files(failed, sizeof(failed)))
    upgrade_settings();
// otherwise let AppInit show the localized fatal dialog
```

---

## 4. CMake Install Patterns

### Predefined Config Installation

**Source**: `UI/data/obsredux-defaults/obs-studio/`  
**Destination**: `${OBS_DATA_DESTINATION}/../` (Install Root sibling)

**CMakeLists.txt Pattern**:
```cmake
install(DIRECTORY data/obsredux-defaults/
        DESTINATION ${OBS_DATA_DESTINATION}/../
        FILES_MATCHING PATTERN "*")
```

**Result**: Files land at `<Install Root>/obs-studio/` in the distribution package.

### Why `/../` Works

`${OBS_DATA_DESTINATION}` typically resolves to `<Install Root>/data/obs-studio`. Appending `/../` navigates up to Install Root, allowing direct placement of `obs-studio/` directory structure.

---

## 5. Common Mistakes

### Mistake: Using `strcat` for Path Construction

**Symptom**: Buffer overflow or missing null terminator

**Cause**: `strcat` doesn't check buffer size

**Fix**: Use `get_portable_path` or `snprintf`

**Wrong**:
```c
char path[512];
get_portable_path(path, sizeof(path), "");
strcat(path, "\\recordings");  // ❌ Unsafe
```

**Correct**:
```c
char path[512];
get_portable_path(path, sizeof(path), "recordings");  // ✅ Safe
```

### Mistake: Hardcoding Paths in Dialogs

**Symptom**: Dialog shows wrong path or breaks on non-standard install locations

**Cause**: Hardcoded string concatenation instead of runtime path resolution

**Fix**: Always use `get_portable_path` for runtime paths

**Wrong**:
```cpp
QString body = "Output: " + install_root + "\\recordings";  // ❌ Hardcoded
```

**Correct**:
```cpp
char recordings[512];
get_portable_path(recordings, sizeof(recordings), "recordings");
QString body = QTStr("...").arg(QString::fromUtf8(recordings));  // ✅ Dynamic
```

### Mistake: Bare Hotkey Names in `basic.ini`

**Symptom**: PageDown or W does not trigger recording/replay buffer even though the preset appears to name the key.

**Cause**: OBS hotkeys in Profile `basic.ini` are serialized JSON binding objects, not bare key strings.

**Fix**: Use OBS key IDs inside a `bindings` array.

**Wrong**:
```ini
[Hotkeys]
OBSBasic.StartRecording=PageDown
```

**Correct**:
```ini
[Hotkeys]
OBSBasic.StartRecording={"bindings":[{"key":"OBS_KEY_PAGEDOWN"}]}
```

### Mistake: Completing First Run Through a Separate Config Handle

**Symptom**: Closing the OBSRedux first-run dialog enters the main window, but the next launch shows the dialog again.

**Cause**: `mark_first_run_completed()` writes `FirstRunCompleted=true` through a separately opened `global.ini`, then `OBSApp::globalConfig` later saves its stale in-memory state and removes the marker.

**Fix**: Set `OBSRedux.FirstRunCompleted=true` and `OBSRedux.BootstrapVersion=<current>` on the active `GetGlobalConfig()` handle, then save that handle.

---

## 6. Testing Notes

### Unit Test Coverage (Pending S7)

**Required Tests** (per PRD test matrix):
- M1: `name=""` → `<Install Root>/obs-studio`; `name="global.ini"` → `<Install Root>/global.ini`; NULL name; oversized name
- M2: Mock fs returning EACCES → `WRITE_PROBE_SYSTEM_PROTECTED`; EROFS → `WRITE_PROBE_READ_ONLY_VOLUME`
- M3: Mock encoder registry with only `jim_nvenc` → returns `jim_nvenc`; all unavailable → returns `obs_x264` + `is_software_fallback=true`
- M5: Mock fs + clock with files aged 35/29/10 days → deletes only 35-day file

**Framework**: cmocka (C unit test framework used by upstream OBS)

### Integration Test (Pending S7)

**M4 Bootstrap**: Construct empty User Data Root → trigger M4 → verify `recordEncoder.json::encoder` rewritten, `basic.ini::RecFilePath` rewritten to absolute path, `global.ini::[OBSRedux] FirstRunCompleted=true` written.

---

## 7. Related Specs

- `CONTEXT.md` — Global Config / Profile / User Data Root terminology
- `.trellis/tasks/05-25-step1-portable-bootstrap/design.md` — Interface contracts and data flow
- `.trellis/tasks/05-25-step1-portable-bootstrap/prd.md` — Requirements and acceptance criteria
