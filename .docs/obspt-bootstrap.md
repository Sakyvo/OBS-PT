# OBS-PT Bootstrap Module Spec

**Status**: Active
**Owner**: OBS-PT Core
**Last Updated**: 2026-07-13 (bootstrap v3; hardware encoder initialization boundary; primary-display defaults; OBS-PT AMF preset and resolution-aware QP; Hybrid MP4; GitHub stable updater)

## Overview

The `obspt-bootstrap` module provides portable installation support for OBS-PT, including path resolution, write permission probing, encoder detection, log retention, and first-run configuration.

**Location**: `UI/obspt-bootstrap.{h,cpp}`

---

## 1. M1 — Portable Path Resolver

### Signatures

```c
// libobs/util/platform.h
int get_portable_path(char *dst, size_t size, const char *name);
char *get_portable_path_ptr(const char *name);
```

### Contracts

**Input**:
- `name`: Relative path from Install Root (e.g., `"obs-studio/global.ini"`, `"recordings"`, `""`)
- `dst`/`size`: Output buffer for `get_portable_path`

**Output**:
- Absolute path: `<Install Root>/<name>`
- Empty `name` or `NULL` → `<Install Root>`
- `get_portable_path`: Returns the written byte length, or `-1` if the output
  buffer is too small.
- `get_portable_path_ptr`: Returns a heap-allocated UTF-8 path; callers own it
  and must release it with `bfree` (normally through `BPtr<char>`).

**Critical Invariant**: **Passthrough semantics** — NO auto-injection of `obs-studio/` prefix. All upstream callers already pass fully-qualified names like `"obs-studio/basic/profiles/..."`.

### Design Decision: Passthrough vs Auto-Prefix

**Context**: Initial design.md proposed auto-injecting `obs-studio/` prefix to simplify caller code.

**Discovery**: Grepping upstream revealed 20+ call sites already pass prefixed names:
```c
// Existing upstream pattern
GetConfigPath(buf, size, "obs-studio/global.ini");
GetConfigPath(buf, size, "obs-studio/basic/profiles/...");
```

**Decision**: Implement passthrough `<Install Root>/<name>` with zero callsite
changes. Empty/NULL `name` resolves the Install Root itself; callers requesting
the User Data Root pass `"obs-studio"` explicitly.

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
    if (name && *name)
        snprintf(dst, size, "%s/%s", install_root, name);
    else
        snprintf(dst, size, "%s", install_root);
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
2. `amd_amf_h264` (AMD AMF H.264)
3. `obs_qsv11` (Intel Quick Sync Video)
4. `ffmpeg_nvenc` (NVIDIA NVENC, FFmpeg wrapper)
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

**AMF Packaging**: `amd_amf_h264` is provided by the legacy OBS 27-era
`plugins/enc-amf` submodule. OBS-PT carries its maintained branch in the public
`Sakyvo/obs-amd-encoder` fork, based on upstream tag `1.4.3.7`.
Encoder availability still depends on runtime plugin loading and compatible AMD
hardware/driver, but the distribution must ship `enc-amf.dll`.

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
- The packaged Global Config must contain `[OBSPT] BootstrapVersion=0` and
  `FirstRunCompleted=false`. Shipping completed/current markers bypasses the
  first-run repair and welcome flow on a genuinely fresh installation.
- Repairs `[Basic] Profile/ProfileDir/SceneCollection/SceneCollectionFile` to `PotPvP` only when `[OBSPT] FirstRunCompleted` is missing/false or `BootstrapVersion` is stale.
- Writes upstream gates `General.FirstRun=true` and
  `General.LastVersion=LIBOBS_API_VER`. Updater defaults are owned separately by
  `[OBSPTUpdater] SettingsVersion`.
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
- Probes encoders, then writes a **COMPLETE per-encoder** `recordEncoder.json` via `apply_encoder_to_profile(profile, encoder_id, cqp)` — builds a **fresh `obs_data`** (NOT load-and-mutate), so no encoder-mismatched keys ever survive an encoder switch. Quality is selected from `active_config` `BaseCY`: AMD AMF uses `<1080 → 22`, otherwise `27`; every other encoder uses `<1080 → 20`, otherwise `27`. After JSON saves, writes the probed ID to active `[AdvOut] RecEncoder` before `ResetOutputs()`, so Settings selects the same encoder and loads that JSON. Also writes absolute `<Install Root>/recordings` to both `[AdvOut] RecFilePath` and `[SimpleOutput] FilePath`.
- Returns `true` when a first-run/repair pass ran (caller should show the welcome) and sets `*out_is_software` from the probe. It no longer shows a dialog itself — the QMessageBox `ShowFirstRunRecommendationsDialog` was removed; after `OnFirstLoad()` emits `OBS_FRONTEND_EVENT_FINISHED_LOADING`, `window-basic-main.cpp` waits 500 ms and then shows the multi-page `OBSWelcome` wizard (`window-basic-welcome.{hpp,cpp}` + `forms/OBSWelcome.ui`). The wizard is also reopenable from the About dialog's "About" link. See `.docs/archive/trellis-tasks/06-15-welcome-dialog/`.
- Does not write `[OBSPT] FirstRunCompleted=true` itself. It returns `true`
  when the caller must show `OBSWelcome`; `window-basic-main.cpp` connects the
  dialog's `finished` signal to `mark_first_run_completed()`, then saves the
  active `OBSApp::globalConfig`. Do not write this marker only through a separate
  `config_open()` handle; later shutdown saves can overwrite that file with the
  in-memory Global Config. Also do not write it before the welcome dialog is
  actually shown, or an activation/display failure can permanently skip the
  first-run dialog.

**Per-encoder `recordEncoder.json` templates** (NV12 / 8-bit; `cqp` follows the encoder-specific resolution mapping above). A blind id-swap that leaves NVENC keys on QSV/x264/AMF is a defect:

| encoder_id | keys |
|---|---|
| `jim_nvenc` (default) | `rate_control:"CQP"`, `cqp`, `preset2:"p1"`, `tune:"hq"`, `multipass:"disabled"`, `profile:"high"`, `bf:0`, `psycho_aq:false`. Do not write legacy `preset` for fresh/bootstrap defaults; `jim_nvenc` now reads modern P1-P7 settings and uses `nvEncGetEncodePresetConfigEx`. Existing profiles with legacy `preset` and no `preset2` are migrated by the encoder path. |
| `ffmpeg_nvenc` | `rate_control:"CQP"`, `cqp`, `preset2:"p1"`, `tune:"hq"`, `multipass:"disabled"`, `profile:"high"`, `bf:0`, `psycho_aq:false` |
| `obs_qsv11` | `rate_control:"CQP"`, `qpi=qpp=qpb=cqp`, `target_usage:"quality"`, `profile:"high"`, `keyint_sec:2`, `bframes:3`, `latency:"normal"` |
| `amd_amf_h264` | Exact plugin IDs: `AMF.H264.Preset:-1`, `AMF.H264.Usage:0`, `AMF.H264.QualityPreset:0` (Speed/maximum performance), `AMF.H264.Profile:100` (High), `AMF.H264.RateControlMethod:0` (CQP), `AMF.H264.QP.IFrame=AMF.H264.QP.PFrame=AMF.H264.QP.BFrame=cqp` (`22` below 1080p, otherwise `27`), `AMF.H264.VBVBuffer:1`, `AMF.H264.VBVBuffer.Size:100000`, `AMF.H264.KeyframeInterval:2.0`, `AMF.H264.BFrame.Pattern:0`. Never write unprefixed aliases. The top-level Preset combo exposes exactly one localized entry named `OBS-PT`, mapped to `Presets::None (-1)`, so it does not apply or lock a legacy template; the separate Quality Preset combo retains Speed/Balanced/Quality. |
| `obs_x264` | `rate_control:"CRF"`, `crf:cqp`, `preset:"veryfast"`, `profile:"high"`, `tune:""`, `keyint_sec:2`, `x264opts:""` (drop all NVENC keys; 480fps software is a correctness fallback, not performant) |

OBS treats CQP/CRF/QP as one 0–51 quality scale (`window-basic-main-outputs.cpp` `CalcCRF`), so the same `cqp` value maps 1:1 across all encoders.

### Hardware Encoder Initialization Boundary Contract

#### 1. Scope / Trigger

Apply this contract whenever changing a hardware encoder's shipped JSON,
bootstrap template, property schema, capability table, SDK header/version, or
vendor initialization path. The boundary is:

```text
Profile JSON / bootstrap -> OBS encoder settings -> encoder plugin -> vendor API
```

An encoder being registered, its Settings panel rendering correctly, and its
configuration being serializable do not prove that the vendor runtime can
initialize it. This failed twice: legacy NVENC `preset:"hp"` reached
`nvEncGetEncodePresetConfig` and driver 610.47 returned
`NV_ENC_ERR_UNSUPPORTED_PARAM`; later AMF defaults rendered correctly, but an
unbounded Profile Level lookup passed the invalid value 249 to the driver.

#### 2. Signatures / Boundary Inputs

- Product input remains Fractional FPS `480/1` at the detected primary-display
  resolution. This is the workload the encoder path must handle or reject
  explicitly; it is not a value that vendor fixes may silently lower globally.
- Serialized settings must map to property IDs actually read by the selected
  plugin version, then to parameters accepted by its vendor API path.
- Vendor-specific normalization belongs immediately before the vendor call.
  When normalization changes a value, log both requested and effective values.

#### 3. Contracts

- Capability lookup must use bounded iteration. Never terminate an unsigned
  enum table with a negative sentinel or read beyond the last advertised row.
- Resolution/rate products and related capacity arithmetic use checked 64-bit
  calculations. Zero denominators and overflow are explicit above-range cases.
- Above-range input must select a documented highest legal fallback or return a
  clear initialization error. It must never produce a value from arbitrary
  memory or silently mutate the Profile's global FPS.
- A new property/default is not complete until the plugin's runtime mapping,
  SDK surface, compatibility version, and target driver behavior agree.
- Vendor rejection must preserve the vendor error code and parameter name in
  the log. Do not retry with unrelated defaults that hide the violated
  contract.

#### 4. Validation & Error Matrix

| Condition | Required behavior |
|---|---|
| Encoder registers and Settings renders | Continue validation; this is not runtime acceptance |
| Serialized key is not consumed by the selected plugin path | Reject the default/static check; update plugin and template together |
| Vendor preset/API is unsupported on the target driver | Use a supported compatibility path or port the complete newer API path; JSON-only changes are insufficient |
| Resolution x FPS exceeds every capability-table row | Use the documented maximum legal fallback or fail explicitly; never read past the table |
| FPS denominator is zero or capacity arithmetic overflows | Treat as invalid/above-range before the vendor call |
| Local development machine lacks the target vendor | Keep runtime acceptance pending until a representative hardware smoke passes |
| Fix would lower `FPSNum=480` globally | Reject the fix; constrain only the vendor boundary when technically required |

#### 5. Good / Base / Bad Cases

- Good: modern NVENC defaults reach `nvEncGetEncodePresetConfigEx` with a
  compatible SDK/runtime path and record successfully on the target NVIDIA
  driver.
- Good: 2560x1440 at 240/256 FPS exceeds the legacy AMF level table, bounded
  lookup returns Level 5.2, and AMF recording starts and stops successfully.
- Base: a vendor-specific frame-rate limit changes only the effective value
  passed to that vendor API and logs the conversion; OBS-PT keeps `480/1`.
- Bad: UI screenshots, JSON inspection, compilation, and packaging pass, so a
  hardware default is declared valid without a real start/stop smoke.
- Bad: an unsupported vendor value is hidden by lowering the Profile FPS or by
  substituting an unrelated preset without documenting the compatibility path.

#### 6. Tests Required

- Capability-table unit tests: ordinary rows, exact maximum boundary, first
  value above maximum, zero denominator, and overflow input.
- Static mapping check: every shipped/bootstrap key is consumed by the selected
  plugin version and reaches the intended vendor parameter.
- Representative NVIDIA/AMD runtime gate: use the shipped primary-display
  resolution and `480/1`, start recording, record for 10-30 seconds, stop, and
  assert the log contains no vendor invalid/unsupported-parameter error.
- If a vendor-local clamp exists, also test the effective boundary value and
  prove the Profile remains `480/1`.
- Package gate: confirm the installer contains the exact tested plugin DLL.

#### 7. Wrong vs Correct

##### Wrong

```cpp
level((H264ProfileLevel)-1, levelRestriction(0, 0));
for (size_t i = 0; (int32_t)table[i].first != -1; i++)
    try_vendor_value(table[i].first);
```

```text
encoder registered + Settings looks right -> default accepted
```

##### Correct

```cpp
for (const auto& entry : table) {
    if (request_fits(entry))
        return entry.first;
}
return highest_legal_fallback;
```

```text
config mapping -> bounded normalization -> vendor init -> record start/stop -> log assertion
```

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
  marker remains after shutdown. Assert the dialog is absent before
  `OBS_FRONTEND_EVENT_FINISHED_LOADING` and appears 500 ms after that event.
- Fresh packaged tree: assert markers begin as `0/false`, Base and Output match
  the Windows primary display's physical pixels, FPS remains `480/1`, and the
  bootstrap version advances only after validation succeeds.
- AMD profile: assert the top-level Preset has one `OBS-PT` entry, Quality
  Preset remains a separate three-entry control, `[AdvOut] RecEncoder` equals
  `amd_amf_h264`, and generated AMF JSON uses exact `AMF.H264.*` IDs for
  High/CQP/Speed with I/P/B QP `22` below 1080p and `27` at 1080p or above.

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

### AMD AMF Encoder Packaging Contract

OBS-PT ships AMD H.264 hardware encoding through the legacy OBS 27
`plugins/enc-amf` submodule:

- Submodule: `plugins/enc-amf`
- Public fork: `https://github.com/Sakyvo/obs-amd-encoder.git`
- Maintained branch: `obs-pt`
- Compatible upstream baseline: tag `1.4.3.7`
- Runtime encoder id: `amd_amf_h264`
- Runtime plugin DLL: `<Install Root>/obs-plugins/64bit/enc-amf.dll`
- Plugin data: `<Install Root>/data/obs-plugins/enc-amf/`

The parent `.gitmodules` URL and gitlink must both resolve through the public
fork. Before updating the parent gitlink, push the submodule commit and verify
it with `git ls-remote`; a local-only submodule commit makes recursive clones
unbuildable.

`BUILD_AMF_ENCODER` must default on for Windows builds. The legacy plugin
expects an AMF SDK root shaped as `amf/public/include`, while OBS-PT's dependency
bundle stores the headers under `DepsPath/include/AMF`. Configure should bridge
that by staging headers to `${CMAKE_BINARY_DIR}/_amf-sdk/amf/public/include` and
setting `AMDAMF_SDKDir` when `DepsPath/include/AMF/components/VideoEncoderVCE.h`
exists.

On MSVC, `enc-amf` must be compiled with a target-level forced include of
`plugins/enc-amf-msvc-compat.hpp`. The archived plugin defines a `clamp` macro
in `Include/plugin.h`; forcing a small header that includes `<algorithm>` first
prevents the macro from corrupting MSVC 14.44's `std::clamp` declarations
without carrying local edits inside the read-only upstream submodule.

#### Wrong
```cmake
# Builds only on the developer's cached CMake state and fails on clean trees.
set(AMDAMF_SDKDir "K:/Projects/dev/OBS-PT/.target/_amf-sdk")
add_subdirectory(plugins)
```

#### Correct
```cmake
if(BUILD_AMF_ENCODER AND NOT AMDAMF_SDKDir AND DepsPath)
  set(_obspt_amf_include_dir "${DepsPath}/include/AMF")
  if(EXISTS "${_obspt_amf_include_dir}/components/VideoEncoderVCE.h")
    set(_obspt_amf_sdk_dir "${CMAKE_BINARY_DIR}/_amf-sdk")
    file(MAKE_DIRECTORY "${_obspt_amf_sdk_dir}/amf/public/include")
    file(COPY "${_obspt_amf_include_dir}/"
         DESTINATION "${_obspt_amf_sdk_dir}/amf/public/include")
    set(AMDAMF_SDKDir "${_obspt_amf_sdk_dir}" CACHE PATH
        "AMD Advanced Media Framework SDK Directory" FORCE)
  endif()
endif()

add_subdirectory(plugins)

if(MSVC AND TARGET enc-amf)
  target_compile_options(enc-amf PRIVATE
    "/FI${CMAKE_SOURCE_DIR}/plugins/enc-amf-msvc-compat.hpp")
endif()
```

### Why `/../` Works

`${OBS_DATA_DESTINATION}` typically resolves to `<Install Root>/data/obs-studio`. Appending `/../` navigates up to Install Root, allowing direct placement of `obs-studio/` directory structure.

### NSIS Installer Contract

#### 1. Scope / Trigger

The packaged `obs-studio/` tree is the canonical fresh default for first
install, but in-place reinstall must preserve an existing User Data Root unless
the installer user explicitly chooses to overwrite it.

#### 2. Signatures / Files

- Installer script: `UI/installer/obspt-setup.nsi`.
- Merge helper: `UI/installer/obspt-preserve-config.ps1`, extracted to
  `$PLUGINSDIR` only during install; it is not installed into `<Install Root>`.
- User Data Root: `<Install Root>/obs-studio/`.
- Packaged defaults: `${SOURCE_DIR}/obs-studio/`.
- Installer variable: `$OverrideConfig`, default `0`.
- `InstallDir ""` is the sentinel for no explicit directory. NSIS parses a
  command-line `/D=<Install Root>` before `.onInit`, so a non-empty `$INSTDIR`
  must be preserved even when it equals `C:\OBS-PT`.
- Successful installs write HKCU `${UNINSTALL_KEY}\InstallLocation`. On a
  later manual launch with no `/D=`, `.onInit` may reuse that value only when
  `<InstallLocation>\${EXE_PATH}` still exists.

#### 3. Contracts

- The English installer must show a custom "Overwrite Configuration" page after
  the directory page and before file installation.
- The "Overwrite existing OBS-PT configuration with packaged defaults" checkbox
  is unchecked by default.
- Unchecked: setup updates program files but must leave an existing
  `<Install Root>/obs-studio/` byte-for-byte under the same path after install.
- Checked: setup must show a `MB_YESNO|MB_ICONEXCLAMATION` confirmation before
  leaving the configuration page.
- Checked + confirmed: setup replaces `<Install Root>/obs-studio/` with
  packaged defaults, then restores the preserved parts from the previous User
  Data Root:
  - stream settings: `service.json`, `streamEncoder.json`, and stream-related
    Basic Config keys;
  - recording paths: `SimpleOutput.FilePath`, `AdvOut.RecFilePath`, and
    `AdvOut.FFFilePath`;
  - hotkeys: Profile `[Hotkeys]`, `General.HotkeyFocusType`, and matching
    Scene Collection JSON `hotkeys` fields;
  - replay buffer: Profile `RecRB*` keys and Global Config automatic replay
    buffer settings.
- Fresh install with no existing `<Install Root>/obs-studio/` must install the
  packaged defaults regardless of checkbox state.
- `SecMain` must call `RequireOBSClosed` before `PrepareExistingConfig`,
  `File /r`, shortcuts, registry writes, or any other installation step.
- Process detection uses `Get-Process -Name OBS-PT`; found processes show an
  English Retry/Cancel prompt, probe failures fail closed, and no branch may
  terminate OBS-PT.
- Install Root selection order is explicit `/D=`, then the most recent valid
  HKCU `InstallLocation`, then automatic HDD selection, then `C:\OBS-PT`.
  Every successful installation overwrites the single HKCU value, so manual
  updates return to the last successfully installed root. A stale value whose
  `OBS-PT.exe` is missing must fall through to fresh-install selection.

OBS launch entries must start in `<Install Root>/bin/64bit`:

- Finish-page launch: use `MUI_FINISHPAGE_RUN_FUNCTION`, call `SetOutPath
  "$INSTDIR\bin\64bit"`, then `Exec '"$INSTDIR\bin\64bit\OBS-PT.exe"'`.
- Start Menu/Desktop OBS shortcuts: create them while `$OUTDIR` is
  `$INSTDIR\bin\64bit`, so the shortcut working directory is the exe directory.
- Start Menu must contain only a root `OBS-PT.lnk`; do not create an `OBS-PT`
  Start Menu folder or an uninstall shortcut. Install and uninstall paths should
  still remove legacy `$SMPROGRAMS\OBS-PT\*.lnk` entries from older installers.

#### 4. Validation & Error Matrix

| Condition | Required behavior |
|---|---|
| Existing `obs-studio/`, checkbox unchecked | Temporarily move old `obs-studio/`, copy files, remove newly copied packaged `obs-studio/`, restore old directory |
| Existing `obs-studio/`, checkbox checked + confirmed | Temporarily move old `obs-studio/`, copy files, merge preserved settings from backup, remove backup only after merge success |
| User clicks No in overwrite confirmation | Stay on the configuration page and do not install yet |
| Existing config cannot be moved or restored | Show a stop dialog and abort without deleting the backup |
| Merge helper fails | Keep the previous config backup and show its path in a warning |
| Explicit `/D=C:\OBS-PT` | Keep `C:\OBS-PT`; do not run automatic drive selection |
| Valid HKCU `InstallLocation`, no `/D=` | Prefill the Directory page with that existing OBS-PT root |
| Stale HKCU `InstallLocation`, no `/D=` | Ignore it and run fresh-install drive selection |
| `OBS-PT.exe` is running | Show Retry/Cancel before any target file changes |
| Process probe fails | Block installation and offer Retry/Cancel |

#### 5. Good / Base / Bad Cases

- Good: reinstall with the checkbox unchecked keeps an existing
  `obs-studio/global.ini` and Profile files unchanged while program binaries are
  refreshed.
- Base: fresh install seeds the packaged PotPvP defaults, including the current
  `global.ini` DockState and Profile defaults.
- Bad: recursive `File /r` overwrites the active User Data Root on a normal
  reinstall without explicit confirmation.

#### 6. Tests Required

- Static check: verify `Page custom ConfigPageCreate ConfigPageLeave`,
  `$OverrideConfig` default `0`, and the PowerShell helper extraction exist in
  `UI/installer/obspt-setup.nsi`.
- Merge helper check: run `obspt-preserve-config.ps1` against temporary old/new
  `obs-studio/` trees and assert stream files, recording paths, `[Hotkeys]`, and
  replay buffer keys survive.
- Installer refresh: assert no `makensis.exe` process exists, run exactly one
  NSIS build with enough command timeout for full compression, and wait for it
  to exit successfully. Then require both `7z t` and an actual launch to the
  `OBS-PT <version> Setup` window; `7z t` alone does not validate the NSIS
  self-check CRC.
- Launch with an explicit `/D=` equal to the normal default and assert the
  directory page preserves it.
- Set HKCU `InstallLocation` to a disposable root containing `${EXE_PATH}` and
  launch without `/D=`; assert the Directory page selects it. Remove the EXE
  and assert the same registry value is ignored. Restore the original registry
  value after the test.
- With `OBS-PT.exe` running, advance to Install, click Retry, and assert the
  target tree is unchanged and the prompt returns; Cancel must exit setup.
- Smoke install if behavior changed: install once over a disposable existing
  User Data Root with checkbox unchecked and assert active config files remain
  unchanged; repeat with checkbox checked and assert packaged defaults are
  present while preserved keys survive.

#### 7. Wrong vs Correct

#### Wrong
```nsi
SetOverwrite on
SetOutPath "$INSTDIR"
File /r "${SOURCE_DIR}\*.*"
```

#### Correct
```nsi
Call PrepareExistingConfig

SetOverwrite on
SetOutPath "$INSTDIR"
File /r "${SOURCE_DIR}\*.*"

Call FinalizeExistingConfig
```

#### Wrong
```nsi
InstallDir "C:\OBS-PT"

Function .onInit
  ${If} $INSTDIR == "C:\OBS-PT"
    StrCpy $INSTDIR ""
  ${EndIf}
FunctionEnd
```

#### Correct
```nsi
InstallDir ""

Function .onInit
  ${If} $INSTDIR != ""
    Return ; Preserve NSIS-parsed /D=, including C:\OBS-PT.
  ${EndIf}

  ReadRegStr $0 HKCU "${UNINSTALL_KEY}" "InstallLocation"
  ${If} $0 != ""
    IfFileExists "$0\${EXE_PATH}" 0 select_new_install_dir
    StrCpy $INSTDIR "$0"
    Return
  ${EndIf}

select_new_install_dir:
  ${GetDrives} "HDD" "PickDefaultDrive"
FunctionEnd

Section "OBS-PT" SecMain
  Call RequireOBSClosed
  Call PrepareExistingConfig
SectionEnd
```

Runtime bugfixes that require user install-and-retest must end with a refreshed
installer, not only a hot-copy into `D:/OBS-PT`. After the relevant build and
runtime smoke pass, refresh `.target/_pkg/OBS-PT/` from
`.target/rundir/RelWithDebInfo/`, copy the shipped
`UI/data/obspt-defaults/` tree into the staging root, then run:

```powershell
if (Get-Process makensis -ErrorAction SilentlyContinue) {
    throw "makensis is already running"
}
& "C:\Program Files (x86)\NSIS\makensis.exe" "UI\installer\obspt-setup.nsi"
if ($LASTEXITCODE -ne 0) {
    throw "makensis failed"
}
```

The shell/tool timeout for this command must exceed the observed full-package
compression time. If the wrapper times out, do not rerun immediately: first
wait for or explicitly stop the original `makensis.exe` and verify no process
still owns the fixed `OutFile`. Two packagers must never write the same output.

Verify `.target/_pkg/OBS-PT-<version>-Installer.exe` has a new timestamp and
that the staged files touched by the fix match the built `rundir` files.
Timestamp alone is not a sufficient installer validation. Before handing an
installer to a tester, also verify:

- `.target/_pkg/OBS-PT/` exists and contains the expected portable roots
  (`bin/`, `data/`, `obs-plugins/`, `obs-studio/`).
- No `OBS-PT-*-Installer.exe` exists anywhere under the NSIS `SOURCE_DIR`;
  `OutFile` must stay outside the recursively packed tree.
- The installer archive passes an integrity test, e.g.
  `7z t .target/_pkg/OBS-PT-<version>-Installer.exe`.
- Launch the generated build-path installer and assert its main window title is
  `OBS-PT <version> Setup`, not `NSIS Error`, before copying it anywhere.
- After copying to Desktop/release staging, require exact source/destination
  size and SHA-256 equality and repeat the actual launch check on that copy.
- If doing a silent install smoke test, snapshot and restore the existing HKCU
  uninstall key plus Desktop/Start Menu shortcuts first; the NSIS script writes
  per-user installer metadata even when `/D=` points to a workspace smoke path.

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
  CreateShortCut "$SMPROGRAMS\OBS-PT.lnk" "$INSTDIR\bin\64bit\OBS-PT.exe"
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

### Hybrid MP4 Recording Default Contract

#### 1. Scope / Trigger

Hybrid MP4 is the default for normal recording only. Replay Buffer must stay on
the existing `replay_buffer` output until normal Hybrid MP4 recording is stable
and a separate task explicitly evaluates Replay Buffer.

#### 2. Signatures / Keys

- Profile keys: `SimpleOutput.RecFormat` and `AdvOut.RecFormat`.
- Hybrid format id: `hybrid_mp4`.
- Generated file extension: `hybrid_mp4 -> mp4`.
- Normal recording output id: `hybrid_mp4 -> mp4_output`; other standard
  formats keep `ffmpeg_muxer`.
- Shipped default: `UI/data/obspt-defaults/.../PotPvP/basic.ini`
  `[AdvOut] RecFormat=hybrid_mp4` and `RecTracks=1` (record audio track 1
  only by default).
- Fresh fallback defaults in `OBSBasic::InitBasicConfigDefaults()` must set both
  simple and advanced `RecFormat` defaults to `hybrid_mp4`.
- Native output registration: `plugins/obs-outputs/obs-outputs.c` must register
  `mp4_output_info`.

#### 3. Contracts

- Hybrid MP4 writes directly to a `.mp4` path and must not be auto-remuxed to
  another temporary container.
- Ordinary `mp4` remains visible in Settings and continues to use
  `ffmpeg_muxer` as the manual workaround.
- If `mp4_output` cannot be created or `obs_output_start()` fails, OBS-PT must
  show the recording-start failure dialog and append
  `Output.StartFailedHybridMP4Fallback`; it must not silently fall back to
  ordinary MP4.
- The native MP4 muxer must accept OBS-PT's actual AAC encoder codec string.
  `ffmpeg_aac` exposes `.codec = "AAC"` in this branch, so the muxer's AAC
  match must be case-insensitive.
- The native MP4 muxer writes the final `moov` box through
  `array_output_serializer` so nested MP4 box sizes can be backfilled. That
  serializer must implement `seek` and maintain a write cursor; append-only
  behavior corrupts box sizes and produces MP4 files with no discoverable
  streams after normal stop.
- Keep PotPvP's high-FPS contract: `FPSType=2`, `FPSNum=480`, `FPSDen=1`.

#### 4. Validation & Error Matrix

| Condition | Required behavior |
|---|---|
| New install / fresh default Profile | `RecFormat=hybrid_mp4`, `AutoRemux=false`, `FPSNum=480`, `RecTracks=1` |
| Normal recording with `hybrid_mp4` | Creates `mp4_output`, generated path ends in `.mp4` |
| Normal recording with ordinary `mp4` | Creates `ffmpeg_muxer`, ordinary MP4 warning behavior remains |
| Replay Buffer enabled while `RecFormat=hybrid_mp4` | Still creates `replay_buffer`; do not create `mp4_output` |
| `mp4_output` creation/start failure | Show start-failed dialog with ordinary-MP4 workaround text |
| Installer refresh | Staged `obs-outputs.dll` hash matches build output; installer passes `7z t` |

#### 5. Good / Base / Bad Cases

- Good: PotPvP default normal recording selects `hybrid_mp4`, records to
  `.mp4`, and uses native fragmented MP4 while active.
- Base: User manually selects ordinary `mp4`; recording uses the legacy FFmpeg
  muxer and remains a deliberate workaround.
- Bad: `hybrid_mp4` writes `.hybrid_mp4`, auto-remuxes to itself, switches Replay
  Buffer to `mp4_output`, or catches a start failure by silently using ordinary
  MP4.

#### 6. Tests Required

- Build `obs-outputs` and `obs`.
- Static check: search `hybrid_mp4`, `mp4_output`, `replay_buffer`, and
  `buffered_file_serializer` to verify routing and packaging.
- Verify staged `PotPvP/basic.ini` contains `RecFormat=hybrid_mp4`,
  `AutoRemux=false`, and `FPSNum=480`.
- Verify staged `obs-outputs.dll` hash equals the built DLL hash.
- Generate a fresh installer and run `7z t`.
- Runtime smoke before tester handoff: start/stop a short Hybrid MP4 normal
  recording, test selected audio tracks, and force-interrupt a disposable
  recording to confirm fragmented MP4 recovery.

#### 7. Wrong vs Correct

#### Wrong
```cpp
// Treats the format id as the file extension and silently falls back.
fileOutput = obs_output_create("ffmpeg_muxer", "file_output", nullptr, nullptr);
strPath = GetRecordingFilename(path, "hybrid_mp4", noSpace, overwrite, fmt);
```

```c
// OBS-PT's ffmpeg_aac codec string is "AAC" in this branch.
if (strcmp(codec, "aac") == 0)
    return CODEC_AAC;
```

#### Correct
```cpp
fileOutput = obs_output_create(
    IsHybridMP4Format(format) ? "mp4_output" : "ffmpeg_muxer",
    "file_output", nullptr, nullptr);
strPath = GetRecordingFilename(path, RecordingFormatExtension(format),
                               noSpace, overwrite, fmt,
                               !IsHybridMP4Format(format));
```

```c
if (astrcmpi(codec, "aac") == 0)
    return CODEC_AAC;
```

#### Wrong
```c
// Append-only array serializer: serializer_seek() fails, so MP4 box-size
// backfills are appended instead of overwritten.
static size_t array_output_write(void *param, const void *data, size_t size)
{
    struct array_output_data *output = param;
    da_push_back_array(output->bytes, (uint8_t *)data, size);
    return size;
}
```

#### Correct
```c
// Seekable array serializer: Hybrid MP4 can rewrite placeholder box sizes
// while building the final moov buffer.
s->write = array_output_write;
s->seek = array_output_seek;
s->get_pos = array_output_get_pos;
```

---

## 5. Windows GitHub Stable Updater Contract

### 1. Scope / Trigger

This contract applies to the Windows Help-menu/startup updater, GitHub Release
parsing, Desktop installer download, automatic installer launch, and updater
state stored in Global Config.

### 2. Signatures

```cpp
bool ParseOBSStableVersion(const QString &text, QVersionNumber &version,
                           QString *normalized = nullptr);
OBSUpdateParseResult ParseOBSUpdateRelease(const QByteArray &json);

class OBSUpdateManager : public QObject {
public:
    void Check(bool manual);
    bool IsBusy() const;
};
```

Persistent keys:

```ini
[General]
EnableAutoUpdates=true

[OBSPTUpdater]
SettingsVersion=1
SkippedVersion=
AutoFailureNotified=false
```

### 3. Contracts

- Request only `https://api.github.com/repos/Sakyvo/OBS-PT/releases/latest`
  through packaged libcurl/Schannel. Checks are asynchronous and one manager
  owns at most one check/download.
- Accept only stable `x.y.z` or `vx.y.z` tags, a matching GitHub release URL,
  and exactly one `OBS-PT-x.y.z-Installer.exe` asset with positive integer
  size, exact GitHub browser URL, and `sha256:<64 hex>` digest.
- Render the published Release body with GitHub Markdown plus
  `MarkdownNoHTML`. Release descriptions omit SHA-256/asset lists; the digest
  is validation-only metadata and is not displayed.
- A successful parsed check clears `AutoFailureNotified`. Manual failures
  always show; automatic failures show only once until a later success.
  Manual checks bypass `SkippedVersion`; automatic checks suppress only an
  exact skipped version.
- Download to the redirected Windows Desktop as `<asset>.part`, stream through
  SHA-256, enforce the declared size, validate every redirect/effective host,
  then atomically replace `<asset>` with `MoveFileExW`.
- `QProgressDialog::setValue()` may process events while called from libcurl's
  progress callback. Cancel handlers set `downloadCanceled` only; the progress
  callback returns non-zero and cleanup occurs after libcurl reports
  `CURLE_ABORTED_BY_CALLBACK`.
- Automatic Install is disabled while `BasicOutputHandler::Active()` is true,
  rechecks immediately before launch, starts the interactive installer without
  `/S`, passes final `/D=<Install Root>`, then requests normal OBS-PT shutdown.
- Publish only the installer asset manually. GitHub-generated source archives
  replace any redundant `Sourcecode.zip`.

### 4. Validation & Error Matrix

| Condition | Required behavior |
|---|---|
| Draft/prerelease, invalid JSON/tag/URL/asset/digest | Localized check failure; never offer download |
| Manual current/older release | Explicit "current" dialog |
| Automatic current/older release | Silent success and reset failure marker |
| Automatic failure after marker is set | Log only; do not show another dialog |
| Unsafe redirect, write/size/hash/promote failure | Delete `.part`; keep any existing final installer; never offer install |
| User cancels download | Abort through progress callback, delete `.part`, no failure dialog |
| Any output becomes active | Keep installer on Desktop and refuse Automatic Install |
| Installer launch fails | Keep installer and OBS-PT open; show localized failure |

### 5. Good / Base / Bad Cases

- Good: a newer stable Release downloads to Desktop, validates against GitHub
  size/digest, and launches the normal installer with the current Install Root.
- Base: no update is silent on startup but explicit from a manual check; Skip
  suppresses only automatic reminders for that version.
- Bad: parse a digest from release-note text, display checksum appendices,
  promote a partial file, destroy a curl handle from inside its callback, or
  launch a silent installer.

### 6. Tests Required

- Model test: plain/leading-`v` versions; older/current/newer comparison; invalid
  JSON/fields/tag; draft/prerelease; missing/duplicate/mismatched assets; bad
  size/URL/digest; redirect allowlist.
- Runtime: manual current result; automatic failure suppression/reset;
  Skip/Ignore semantics; Markdown/no-HTML rendering; Release body without a
  checksum appendix.
- Runtime download: success size/hash, cancel logs
  `CURLE_ABORTED_BY_CALLBACK`, `.part` cleanup, and completion actions.
- Runtime launch: process command line contains `/D=<current Install Root>` and
  no `/S`; active outputs prevent launch.
- Package: clean staging, built/staged EXE hash equality, `7z t`, current
  installer timestamp/SHA-256, and only one manual GitHub asset.

### 7. Wrong vs Correct

#### Wrong
```cpp
void OBSUpdateManager::CancelDownload()
{
    downloadCanceled = true;
    curl->Abort(); // May destroy the easy handle during setValue() re-entry.
    CleanupDownload(true);
}
```

#### Correct
```cpp
void OBSUpdateManager::CancelDownload()
{
    downloadCanceled = true;
}

static int DownloadProgress(void *opaque, curl_off_t, curl_off_t,
                            curl_off_t, curl_off_t)
{
    auto *state = static_cast<OBSUpdateCurlState *>(opaque);
    return state->owner->downloadCanceled ? 1 : 0;
}
```

---

## 6. Common Mistakes

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

### Mistake: Writing AMF Labels Instead of Serialized Property IDs

**Symptom**: Fresh AMD installs detect `amd_amf_h264`, but Settings initially
falls back to "Use stream encoder". After AMD is selected manually, the AMF
panel shows Balanced, Main, CBR, and 3500 Kbps instead of OBS-PT defaults.

**Cause**: Two independent cross-layer contracts were missed:

1. `recordEncoder.json` used unprefixed names such as `Profile` and
   `RateControlMethod`. The legacy plugin registers properties under exact IDs
   such as `AMF.H264.Profile` and `AMF.H264.RateControlMethod`; `obs_data_apply`
   preserves unknown JSON fields silently, so populated JSON can still have no
   effect.
2. Bootstrap rewrote the encoder JSON but left `[AdvOut] RecEncoder` at the
   packaged `jim_nvenc`. On AMD, Settings could not find that encoder and kept
   index 0. A later manual encoder change deliberately creates default settings
   instead of loading JSON for a different encoder ID.

**Fix**: Write only exact `AMF.H264.*` IDs from
`plugins/enc-amf/Include/enc-h264.h`. After the JSON save succeeds, set active
`[AdvOut] RecEncoder` to the probed encoder before `ResetOutputs()`.

**Prevention**: Static checks must compare serialized AMF keys with the
plugin's property ID prefix, reject unprefixed aliases, and assert the active
Profile selects the same encoder as `recordEncoder.json`. AMD tester acceptance
must inspect the first automatic selection, not a panel opened after manually
changing encoders.

#### Wrong
```cpp
obs_data_set_int(root, "Profile", 100);
obs_data_set_int(root, "RateControlMethod", 0);
```

#### Correct
```cpp
obs_data_set_int(root, "AMF.H264.Profile", 100);
obs_data_set_int(root, "AMF.H264.RateControlMethod", 0);
config_set_string(active_config, "AdvOut", "RecEncoder", encoder_id);
```

### Mistake: Treating Installer Timestamp as Integrity Validation

**Symptom**: A tester launches the latest-looking
`OBS-PT-<version>-Installer.exe` and gets NSIS "Installer integrity check has
failed" before the directory page.

**Cause**: The packaging pass verified only timestamp and/or `7z t`. Neither
proves that the staging tree was correct, the final NSIS self-check CRC is
valid, or copying/uploading preserved the file. A particularly dangerous path
is launching `makensis` through a command timeout shorter than compression and
immediately retrying: the first child process can keep writing the fixed
`OutFile` while the second process writes it too. In the observed failure,
source and Desktop hashes matched and `7z t` passed, but launching the file
still produced `Installer integrity check has failed`.

**Fix**: Verify no `makensis.exe` exists, recreate staging, run one packager to
normal exit with a long timeout, then test the final exe with `7z t` **and**
launch it to the normal Setup window. Copy only that verified file, compare
size/SHA-256, and launch the copy too. If a smoke install is needed, use a
workspace `/D=` path but snapshot/restore the existing HKCU uninstall key and
shortcuts because the installer writes per-user metadata.

**Prevention**: Never overlap `makensis` calls and never treat `7z t` as the
only integrity gate. Every installer refresh records timestamp, SHA-256,
`7z t`, actual Setup-window launch for build and distributed copies, and the
key staged files that justify the user retest.

#### Wrong
```powershell
# Wrapper times out while makensis may still be alive, then a second writer is
# started against the same OutFile.
& makensis.exe UI/installer/obspt-setup.nsi  # 5-second tool timeout
& makensis.exe UI/installer/obspt-setup.nsi
7z t .target/_pkg/OBS-PT-1.0.0-Installer.exe
```

#### Correct
```powershell
if (Get-Process makensis -ErrorAction SilentlyContinue) {
    throw "makensis is already running"
}
& makensis.exe UI/installer/obspt-setup.nsi  # wait to normal exit
7z t .target/_pkg/OBS-PT-1.0.0-Installer.exe
# Launch once and assert MainWindowTitle == "OBS-PT 1.0.0 Setup".
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

### Mistake: AMF Frame-Rate Errors Must Not Use Empty Buffers

**Symptom**: AMF recording fails to start or hangs on stop when the capture pipeline reports an extreme frame rate.

**Cause**: `plugins/enc-amf/Source/amf-h264.cpp` forwarded raw frame-rate values directly into AMF and formatted failure messages with `std::vector<char> msgBuf; sprintf(msgBuf.data(), ...)`, which writes into unallocated memory when the AMF call rejects the input.

**Fix**: Keep the AMF encoder change narrow: fall back to `30/1` when the denominator is zero, clamp absurd frame rates to a sane AMF-only ceiling, and use a sized buffer with `snprintf` for diagnostic formatting.

#### Wrong
```cpp
std::vector<char> msgBuf;
sprintf(msgBuf.data(), "%d/%d", num, den);
```

#### Correct
```cpp
std::vector<char> msgBuf(32);
snprintf(msgBuf.data(), msgBuf.size(), "%d/%d", num, den);
```

### Mistake: Using a Negative Sentinel for an Unsigned AMF Enum

**Symptom**: AMF recording fails during encoder creation with an invalid H.264
Profile Level such as `249`, even though the UI leaves Profile Level on
Automatic.

**Cause**: `H264ProfileLevel` has `uint8_t` as its underlying type. Casting
`-1` to that enum produces 255, so a loop that compares the cast value with
integer `-1` never terminates. A resolution/frame-rate pair above the Level 5.2
table limit then reads beyond the array and passes arbitrary memory to AMF.

**Fix**: Store table limits as 64-bit values, iterate over the bounded array,
and return Level 5.2 when no row matches. Samples-per-second calculation must
treat a zero denominator or overflow as above-range. Do not lower the PotPvP
FPS default to make an existing row match.

#### Wrong
```cpp
level((H264ProfileLevel)-1, levelRestriction(0, 0));
for (uint32_t index = 0; (int32_t)curLevel.first != -1; index++)
    curLevel = profileLevelLimit[index];
```

#### Correct
```cpp
for (const level& curLevel : profileLevelLimit) {
    if (samples <= curLevel.second.first &&
        samples_sec <= curLevel.second.second)
        return curLevel.first;
}
return H264ProfileLevel::L52;
```

**Prevention**: `enc-amf-profile-level-test` must cover ordinary levels, the
Level 5.2 boundary, 2560x1440 at 240/256 FPS, a zero denominator, and overflow
input. The AMD tester must confirm recording starts without a Profile Level
error; local non-AMD builds cannot validate the runtime property call.

### Mistake: Fixing AMF Stop Hangs by Lowering PotPvP FPS

**Symptom**: On AMD AMF, recording starts at the PotPvP 480fps default, then
stopping recording can leave OBS-PT not responding while the status bar reports
encoder overload.

**Cause**: The legacy AMF stop path may spend a long time draining encoder
backlog after overload. Lowering `FPSNum=480` hides the symptom but violates the
OBS-PT product contract: high-FPS recording is foundational for PotPvP defaults.

**Fix**: Keep the 480fps default. Let the first stop request drain normally, but
bound the wait. `OBSBasic::RecordStopping()` must start a 30-second single-shot
watchdog. If `RecordingStop` has not arrived, `recordingStopping` is still true,
and `outputHandler->RecordingActive()` is still true, the watchdog calls
`outputHandler->StopRecording(true)` to enter the existing force-stop path.
`RecordingStart()` and `RecordingStop()` must clear `recordingStopping` and stop
the watchdog.

#### Wrong
```cpp
// Do not special-case AMD by lowering the PotPvP profile frame rate.
config_set_uint(config, "Video", "FPSNum", 60);
```

#### Correct
```cpp
recordingStopping = true;
recordingStopTimer->start(30000);

// On timeout, only force recovery if the original stop is still pending.
if (recordingStopping && outputHandler && outputHandler->RecordingActive())
    outputHandler->StopRecording(true);
```

**Prevention**: AMD stop-hang fixes must preserve `FPSNum=480`, prefer normal
encoder drain first, and document any force-stop threshold in the task PRD and
installer validation notes.

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

## 7. Testing Notes

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

## 8. Related Specs

- `CONTEXT.md` — Global Config / Profile / User Data Root terminology
- `.docs/archive/trellis-tasks/05-25-step1-portable-bootstrap/design.md` — Interface contracts and data flow
- `.docs/archive/trellis-tasks/05-25-step1-portable-bootstrap/prd.md` — Requirements and acceptance criteria
