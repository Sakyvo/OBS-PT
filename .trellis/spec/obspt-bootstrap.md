# OBS-PT Bootstrap Module Spec

**Status**: Active  
**Owner**: OBS-PT Core  
**Last Updated**: 2026-06-27 (bootstrap v3: hardware-adaptive defaults + OBS-PT rebrand; current PotPvP default profile; new Profile seeding; installer overwrite/launch contract; runtime bugfix installer refresh contract)

## Overview

The `obspt-bootstrap` module provides portable installation support for OBS-PT, including path resolution, write permission probing, encoder detection, log retention, and first-run configuration.

**Location**: `UI/obspt-bootstrap.{h,cpp}`

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

OBS-PT owns upstream first-run state. This bootstrap spans Global Config, preset file integrity, loaded Profile config, encoder modules, and Qt dialogs, so the timing contract is split across application startup and `OBSBasic::OBSInit()`.

### 2. Signatures

```cpp
void prepare_obspt_global_config(config_t *global_config);
bool run_obspt_early_bootstrap(config_t *global_config);
bool run_first_run_bootstrap_if_needed(const char *active_profile_name,
                                       config_t *active_config,
                                       bool *out_is_software);

// Hardware-adaptive defaults (bootstrap v3)
static void apply_monitor_video_to_profile(const char *profile_name); // early, repair-gated
int apply_encoder_to_profile(const char *profile_name,
                             const char *encoder_id, int cqp);         // late, full template
```

`OBSPT_BOOTSTRAP_VERSION` is currently **3** (`UI/obspt-bootstrap.h`).

### 3. Contracts

**AppInit preparation**:
- Runs after `OBSApp::InitGlobalConfig()` and before `OBSApp::InitLocale()`.
- Must run only when shipped `obs-studio/global.ini` existed at startup.
- Repairs `[Basic] Profile/ProfileDir/SceneCollection/SceneCollectionFile` to `PotPvP` only when `[OBSPT] FirstRunCompleted` is missing/false or `BootstrapVersion` is stale.
- Writes upstream gates `General.FirstRun=true`, `General.LastVersion=LIBOBS_API_VER`, and `General.EnableAutoUpdates=false`.
- Must not write `[OBSPT] FirstRunCompleted=true`.
- Must not finalize `[OBSPT] BootstrapVersion=<current>` until early bootstrap succeeds.

**Early bootstrap**:
- `main()` must preflight `validate_obspt_preset_files()` before `upgrade_settings()`. If a required preset is missing or invalid, skip `upgrade_settings()` so upstream encoder migration cannot recreate missing JSON before the fatal OBS-PT check runs.
- Runs inside `OBSApp::AppInit()` after `InitLocale()` and before `InitTheme()`, Basic fallback defaults, `move_basic_to_profiles()`, `move_basic_to_scene_collections()`, and `MakeUserProfileDirs()`.
- This is earlier than `OBSBasic::OBSInit()` reading `SceneCollectionFile` or opening `basic.ini`, so repaired Global Config is active for the first real Profile load.
- Validates these required files: `obs-studio/global.ini`, `basic/profiles/PotPvP/basic.ini`, `recordEncoder.json`, `streamEncoder.json`, `service.json`, and `basic/scenes/PotPvP.json`.
- Creates `<Install Root>/recordings`.
- Writes `BootstrapVersion` only after the required files validate and `recordings` exists.
- **Adaptive video (v3)**: when repair is needed (`g_late_bootstrap_needed`), calls `apply_monitor_video_to_profile(PotPvP)` here — BEFORE the main window exists. Detects the primary monitor (`QGuiApplication::primaryScreen()` size × `devicePixelRatio`, physical px, **no >1080p cap**) and writes `Video` `BaseCX=OutputCX`/`BaseCY=OutputCY` = monitor resolution (base==output) plus `FPSType=2`/`FPSNum=480`/`FPSDen=1` to `PotPvP/basic.ini`. Must precede `OBSBasic::OBSInit()`→`ResetVideo()` so resolution/fps take effect the same session (uses `config_set_uint` user-values so `InitBasicConfigDefaults` does not override). Repair-gated: never runs on a normal launch, so user edits persist between version bumps. Null/headless screen → skip (ResetVideo's 1920×1080 fallback covers).

**Late bootstrap**:
- Runs inside `OBSBasic::OBSInit()` after `obs_post_load_modules()`.
- Receives the already opened `basicConfig` as `active_config`.
- Probes encoders, then writes a **COMPLETE per-encoder** `recordEncoder.json` via `apply_encoder_to_profile(profile, encoder_id, cqp)` — builds a **fresh `obs_data`** (NOT load-and-mutate), so no encoder-mismatched keys ever survive an encoder switch. CQP/CRF/QP is set by resolution: `BaseCY < 1080 → 20`, else `26` (read from `active_config`). Also writes absolute `<Install Root>/recordings` to both `[AdvOut] RecFilePath` and `[SimpleOutput] FilePath`.
- Returns `true` when a first-run/repair pass ran (caller should show the welcome) and sets `*out_is_software` from the probe. It no longer shows a dialog itself — the QMessageBox `ShowFirstRunRecommendationsDialog` was removed; `window-basic-main.cpp` now shows the multi-page `OBSWelcome` wizard (`window-basic-welcome.{hpp,cpp}` + `forms/OBSWelcome.ui`), also reopenable from the About dialog's "About" link. See `.trellis/tasks/06-15-welcome-dialog/`.
- Does not write `[OBSPT] FirstRunCompleted=true` itself. It returns `true`
  when the caller must show `OBSWelcome`; `window-basic-main.cpp` connects the
  dialog's `finished` signal to `mark_first_run_completed()`, then saves the
  active `OBSApp::globalConfig`. Do not write this marker only through a separate
  `config_open()` handle; later shutdown saves can overwrite that file with the
  in-memory Global Config. Also do not write it before the welcome dialog is
  actually shown, or an activation/display failure can permanently skip the
  first-run dialog.

**Per-encoder `recordEncoder.json` templates** (NV12 / 8-bit; `cqp` = 20 or 26 by resolution). A blind id-swap that leaves NVENC keys on QSV/x264/AMF is a defect:

| encoder_id | keys |
|---|---|
| `jim_nvenc` (default) | `rate_control:"CQP"`, `cqp`, `preset2:"p1"`, `tune:"hq"`, `multipass:"disabled"`, `profile:"high"`, `bf:0`, `psycho_aq:false`. Do not write legacy `preset` for fresh/bootstrap defaults; `jim_nvenc` now reads modern P1-P7 settings and uses `nvEncGetEncodePresetConfigEx`. Existing profiles with legacy `preset` and no `preset2` are migrated by the encoder path. |
| `ffmpeg_nvenc` | `rate_control:"CQP"`, `cqp`, `preset2:"p1"`, `tune:"hq"`, `multipass:"disabled"`, `profile:"high"`, `bf:0`, `psycho_aq:false` |
| `obs_qsv11` | `rate_control:"CQP"`, `qpi=qpp=qpb=cqp`, `target_usage:"quality"`, `profile:"high"`, `keyint_sec:2`, `bframes:3`, `latency:"normal"` |
| `amd_amf_h264` | `Usage:0`, `Profile:100`, `RateControlMethod:0`, `QP.IFrame=QP.PFrame=QP.BFrame=cqp`, `VBVBuffer:1`, `VBVBuffer.Size:100000`, `KeyframeInterval:2.0`, `BFrame.Pattern:0` (forward-looking; enc-amf submodule empty) |
| `obs_x264` | `rate_control:"CRF"`, `crf:cqp`, `preset:"veryfast"`, `profile:"high"`, `tune:""`, `keyint_sec:2`, `x264opts:""` (drop all NVENC keys; 480fps software is a correctness fallback, not performant) |

OBS treats CQP/CRF/QP as one 0–51 quality scale (`window-basic-main-outputs.cpp` `CalcCRF`), so the same `cqp` value maps 1:1 across all encoders.

### 4. Validation & Error Matrix

| Condition | Required behavior |
|---|---|
| `global.ini` missing at process start | Show OBS-PT preset failure dialog and exit; do not silently create a reusable empty Global Config |
| Required preset INI/JSON missing or invalid | Show OBS-PT preset failure dialog with the first failed path and exit before main window |
| Required encoder JSON missing before `upgrade_settings()` | Skip `upgrade_settings()`, show OBS-PT preset failure later in `AppInit`, and leave the missing file absent |
| `recordings/` cannot be created | Show OBS-PT preset failure dialog and exit before main window |
| `BootstrapVersion` stale but early validation fails | Leave `BootstrapVersion` stale so the next launch retries migration |
| `FirstRunCompleted=true` and `BootstrapVersion` current | Preserve user Profile / Scene Collection choices |

### 5. Good/Base/Bad Cases

- Good: shipped Global Config exists, stale bootstrap version is repaired to PotPvP, early validation passes, then `BootstrapVersion` is written.
- Base: `FirstRunCompleted=true` and current bootstrap version; early validation still checks the package, but no user choices are overwritten.
- Bad: missing `global.ini` is opened with `CONFIG_OPEN_ALWAYS`, saved, and then accepted on the next launch as an empty upstream-style config.
- Bad: missing `recordEncoder.json` is recreated by `upgrade_settings()` before OBS-PT integrity validation sees the damaged package.
- Bad: `[OBSPT] FirstRunCompleted=true` is written through a second config handle, then removed when shutdown saves the stale in-memory Global Config.

### 6. Tests Required

- Missing `global.ini`: launch once and assert the file is still absent after exit.
- Missing `recordEncoder.json`: launch once and assert the fatal window title appears and the missing file is not recreated.
- Stale `BootstrapVersion` with valid presets: assert PotPvP startup index, upstream gates, and current `BootstrapVersion`.
- Stale `BootstrapVersion` with a missing required preset: assert process exits and `BootstrapVersion` remains stale.
- Fresh first run: assert late bootstrap requests the first-run dialog without
  writing `FirstRunCompleted=true`; closing the dialog writes the marker and the
  marker remains after shutdown.

### 7. Wrong vs Correct

#### Wrong

```cpp
globalConfig.Open(path, CONFIG_OPEN_ALWAYS);
prepare_obspt_global_config(globalConfig); // can save a synthetic global.ini
config_set_int(globalConfig, "OBSPT", "BootstrapVersion", CURRENT);
```

#### Correct

```cpp
if (global_ini_missing_at_startup)
    exit_with_preset_failure();

prepare_obspt_global_config(globalConfig);

if (!run_obspt_early_bootstrap(globalConfig))
    return 1;
// early bootstrap writes BootstrapVersion only after validation succeeds
```

```cpp
// Before upstream upgrade_settings()
char failed[512] = {};
if (validate_obspt_preset_files(failed, sizeof(failed)))
    upgrade_settings();
// otherwise let AppInit show the localized fatal dialog
```

---

## 4. CMake Install Patterns

### Predefined Config Installation

**Source**: `UI/data/obspt-defaults/obs-studio/`  
**Destination**: `${OBS_DATA_DESTINATION}/../` (Install Root sibling)

**CMakeLists.txt Pattern**:
```cmake
install(DIRECTORY data/obspt-defaults/
        DESTINATION ${OBS_DATA_DESTINATION}/../
        FILES_MATCHING PATTERN "*")
```

**Result**: Files land at `<Install Root>/obs-studio/` in the distribution package.

### Why `/../` Works

`${OBS_DATA_DESTINATION}` typically resolves to `<Install Root>/data/obs-studio`. Appending `/../` navigates up to Install Root, allowing direct placement of `obs-studio/` directory structure.

### NSIS Installer Contract

The packaged `obs-studio/` tree is the canonical fresh default for the alpha
installer. `UI/installer/obspt-setup.nsi` must force-copy the staging tree with
`SetOverwrite on`; NSIS's default overwrite behavior can leave newer runtime
files such as `obs-studio/global.ini` and profile JSON in place during an
in-place reinstall, which preserves `[OBSPT] FirstRunCompleted=true` and skips
the welcome flow.

OBS launch entries must start in `<Install Root>/bin/64bit`:

- Finish-page launch: use `MUI_FINISHPAGE_RUN_FUNCTION`, call `SetOutPath
  "$INSTDIR\bin\64bit"`, then `Exec '"$INSTDIR\bin\64bit\OBS-PT.exe"'`.
- Start Menu/Desktop OBS shortcuts: create them while `$OUTDIR` is
  `$INSTDIR\bin\64bit`, so the shortcut working directory is the exe directory.

Runtime bugfixes that require user install-and-retest must end with a refreshed
installer, not only a hot-copy into `D:/OBS-PT`. After the relevant build and
runtime smoke pass, refresh `build-v143/_pkg/OBS-PT/` from
`build-v143/rundir/RelWithDebInfo/`, copy the shipped
`UI/data/obspt-defaults/` tree into the staging root, then run:

```powershell
& "C:\Program Files (x86)\NSIS\makensis.exe" "UI\installer\obspt-setup.nsi"
```

Verify `build-v143/_pkg/OBS-PT-<version>-Installer.exe` has a new timestamp and
that the staged files touched by the fix match the built `rundir` files.

#### Wrong
```nsi
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\64bit\OBS-PT.exe"

Section "OBS-PT" SecMain
  SetOutPath "$INSTDIR"
  File /r "${SOURCE_DIR}\*.*"
  CreateShortCut "$DESKTOP\OBS-PT.lnk" "$INSTDIR\bin\64bit\OBS-PT.exe"
SectionEnd
```

#### Correct
```nsi
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION "LaunchOBS"

Function LaunchOBS
  SetOutPath "$INSTDIR\bin\64bit"
  Exec '"$INSTDIR\bin\64bit\OBS-PT.exe"'
FunctionEnd

Section "OBS-PT" SecMain
  SetOverwrite on
  SetOutPath "$INSTDIR"
  File /r "${SOURCE_DIR}\*.*"
  SetOutPath "$INSTDIR\bin\64bit"
  CreateShortCut "$DESKTOP\OBS-PT.lnk" "$INSTDIR\bin\64bit\OBS-PT.exe"
SectionEnd
```

### Scene Collection JSON Contract

OBS-PT default scene collections must use the same shape that `OBSBasic::LoadData()` writes and reads:

- Top-level global audio devices, e.g. `DesktopAudioDevice1`, are loaded by `LoadAudioDevice()`.
- Top-level `sources[]` contains the scene source (`id: "scene"`) and child sources (`game_capture`, `monitor_capture`, etc.).
- Scene membership lives under the scene source's `settings.items[]`.
- `settings.items[]` is stored bottom-to-top; later items render later and appear higher in the UI source list.
- Do not add microphone/Aux devices to the shipped PotPvP default unless the product requirement explicitly changes.

For the PotPvP preset, `DesktopAudioDevice1` should be a `wasapi_output_capture` source with `settings.device_id = "default"`. The `PotPvP` scene should contain `Display` below `Minecraft`, which means the JSON item order is `Display` first, then `Minecraft`. The shipped default keeps `Minecraft` (game capture) visible on top and `Display` (monitor capture, a fallback) below and hidden (`visible:false`); `Display.settings.method = 0` (auto/DXGI).

#### Wrong
```json
{
  "current_scene": "PotPvP Recording",
  "sources": [
    {"id": "game_capture", "name": "Game Capture"}
  ],
  "scenes": [
    {
      "name": "PotPvP Recording",
      "sources": [{"name": "Game Capture"}]
    }
  ]
}
```

#### Correct
```json
{
  "DesktopAudioDevice1": {
    "id": "wasapi_output_capture",
    "name": "桌面音频",
    "settings": {"device_id": "default"}
  },
  "current_scene": "PotPvP",
  "sources": [
    {
      "id": "scene",
      "name": "PotPvP",
      "settings": {
        "items": [
          {"name": "Display", "visible": false},
          {"name": "Minecraft", "visible": true}
        ]
      }
    },
    {"id": "game_capture", "name": "Minecraft"},
    {"id": "monitor_capture", "name": "Display"}
  ]
}
```

### New Profile Contract

The upstream "Profile > New" flow must not create an empty official OBS-style
Profile. OBS-PT clean Profile creation copies the existing `PotPvP` Profile
template first, then rewrites only `[General] Name` for the requested Profile
name. Duplicate Profile still copies the active Profile. The upstream
auto-configuration wizard is disabled in OBS-PT (`ConfigOnNewProfile=false`,
no Tools-menu action, no first-run auto-config launch) but remains compiled for
future reuse.

#### Wrong
```cpp
config.Open(newPath.c_str(), CONFIG_OPEN_ALWAYS);
InitBasicConfigDefaults();
AutoConfig wizard(this);
wizard.exec();
```

#### Correct
```cpp
CopyProfile("PotPvP", newPath.c_str());
config.Open(newPath.c_str(), CONFIG_OPEN_ALWAYS);
config_set_string(config, "General", "Name", newName.c_str());
```

---

## 5. Common Mistakes

### Mistake: Using Video Frame Duration as the Only Multi-Track Interleave Sync Window

**Symptom**: Advanced MP4 recording with multiple AAC tracks starts, but stopping
recording never reaches `Output of file ... stopped` or `==== Recording Stop ====`;
closing the window can leave the OBS-PT process alive.

**Cause**: OBS-PT defaults to 480 fps (`FPSType=2`, `FPSNum=480`), so one video
frame is about 2.08 ms. AAC encoder frames are much longer, and multiple audio
encoders are often out of phase by more than one video frame. If
`libobs/obs-output.c::prune_premature_packets()` uses only video frame duration
as the initial sync threshold, the interleaver can fail to establish a stable
start point or stop delivery for the muxer.

**Fix**: When more than one audio encoder is active, compare the video frame
duration with the maximum active audio encoder frame duration and use the audio
duration if it is larger. Keep the change narrow to interleaver startup pruning;
do not backport unrelated multi-video or packet timing changes from newer OBS
unless the task explicitly requires them.

**Wrong**:
```c
duration_usec = video->timebase_num * 1000000LL / video->timebase_den;
return diff > duration_usec ? max_idx + 1 : 0;
```

**Correct**:
```c
duration_usec = video->timebase_num * 1000000LL / video->timebase_den;

if (audio_encoders > 1 && duration_usec < max_audio_duration_usec)
    duration_usec = max_audio_duration_usec;

return diff > duration_usec ? max_idx + 1 : 0;
```

### Mistake: Updating Shipped Encoder JSON Only

**Symptom**: Fresh installs appear fixed, but first-run/repair users still get the
old encoder settings after OBS-PT bootstraps the profile.

**Cause**: `UI/data/obspt-defaults/.../recordEncoder.json` is only the shipped
seed. `run_first_run_bootstrap_if_needed()` calls `apply_encoder_to_profile()`,
which writes a fresh per-encoder `recordEncoder.json` during first-run/repair.

**Fix**: Keep the shipped JSON and `apply_encoder_to_profile()` templates in
lockstep. For `jim_nvenc`, both must write the current OBS-PT NVENC keys
`preset2:"p1"`, `tune:"hq"`, and `multipass:"disabled"` and must not write a
fresh legacy `preset`.

#### Wrong
```json
{
  "encoder": "jim_nvenc",
  "preset": "hp",
  "bf": 0
}
```

```cpp
obs_data_set_string(root, "preset", "hq");
obs_data_set_string(root, "preset2", "p1");
obs_data_set_int(root, "bf", 0);
```

#### Correct
```json
{
  "encoder": "jim_nvenc",
  "preset2": "p1",
  "tune": "hq",
  "multipass": "disabled",
  "bf": 0,
  "psycho_aq": false
}
```

```cpp
obs_data_set_string(root, "preset2", "p1");
obs_data_set_string(root, "tune", "hq");
obs_data_set_string(root, "multipass", "disabled");
obs_data_set_int(root, "bf", 0);
obs_data_set_bool(root, "psycho_aq", false);
```

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

**Symptom**: Closing the OBS-PT first-run dialog enters the main window, but the next launch shows the dialog again.

**Cause**: `mark_first_run_completed()` writes `FirstRunCompleted=true` through a separately opened `global.ini`, then `OBSApp::globalConfig` later saves its stale in-memory state and removes the marker.

**Fix**: Set `OBS-PT.FirstRunCompleted=true` and `OBS-PT.BootstrapVersion=<current>` on the active `GetGlobalConfig()` handle, then save that handle.

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

**M4 Bootstrap**: Construct empty User Data Root → trigger M4 → verify
`recordEncoder.json::encoder` rewritten, `basic.ini::RecFilePath` rewritten to
absolute path, the welcome dialog is requested, and
`global.ini::[OBSPT] FirstRunCompleted=true` is written only after the welcome
dialog finishes.

---

## 7. Related Specs

- `CONTEXT.md` — Global Config / Profile / User Data Root terminology
- `.trellis/tasks/05-25-step1-portable-bootstrap/design.md` — Interface contracts and data flow
- `.trellis/tasks/05-25-step1-portable-bootstrap/prd.md` — Requirements and acceptance criteria
