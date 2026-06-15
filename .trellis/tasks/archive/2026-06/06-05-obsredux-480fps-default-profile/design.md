# Design: 480fps default profile + hardware-adaptive bootstrap

## Boundaries

Two-phase first-run bootstrap, split by the `ResetVideo()` timing constraint.

### Call-order facts (verified)

| Stage | Location | Reads/Writes | Relevance |
|---|---|---|---|
| Early: `prepare_obspt_global_config` | `obs-app.cpp:1305` (OBSApp::AppInit) | sets repair flags; forces `Basic/Profile=PotPvP` on repair | repair gate established here |
| Early: `run_obspt_early_bootstrap` | `obs-app.cpp:1314` (AppInit) | validates preset files, mkdir recordings, writes `BootstrapVersion` | **runs before the main window exists → before ResetVideo**; new resolution/fps write goes here |
| Late: `InitBasicConfig` | `window-basic-main.cpp:1729` | opens active profile `basic.ini` → `basicConfig` (`:1538`) | picks up the early-written resolution/fps from disk |
| Late: `ResetVideo()` | `window-basic-main.cpp:1734` | reads `Video/BaseCX..OutputCY` + FPS from `basicConfig`, applies to libobs (`:4291-4310`) | **resolution/fps must already be on disk by now** |
| Late: `run_first_run_bootstrap_if_needed` | `window-basic-main.cpp:1786` | probes encoder, writes `recordEncoder.json`, record paths | **after** ResetVideo → encoder/CQP write is safe here; `basicConfig` has the resolution |

So: **resolution + fps → early phase** (file write before ResetVideo); **encoder template + CQP → late phase** (probe needs loaded modules; `recordEncoder.json` is read at record time, not by ResetVideo).

### Repair gate

`prepare_obspt_global_config` sets `g_late_bootstrap_needed = needs_repair` where
`needs_repair = !FirstRunCompleted || BootstrapVersion < OBSPT_BOOTSTRAP_VERSION`
(`obspt-bootstrap.cpp:286-290`). It is cleared only in the late phase
(`:625`). The early resolution/fps write **must be gated on `g_late_bootstrap_needed`**,
otherwise every launch would clobber a user's manual resolution/fps. Bumping
`OBSPT_BOOTSTRAP_VERSION` 2→3 makes existing testers repair exactly once.

## Data flow

```
AppInit (early)
  prepare_obspt_global_config → needs_repair? force PotPvP, set g_late_bootstrap_needed
  run_obspt_early_bootstrap
    └─ if g_late_bootstrap_needed:
         apply_monitor_video_to_profile("PotPvP")           [NEW]
           QScreen primary → physical px (size × devicePixelRatio, NO >1080p cap)
           write basic.ini: Video/BaseCX=BaseCY= monitor, OutputCX=OutputCY= monitor (base==output)
                            FPSType=2, FPSNum=480, FPSDen=1
OBSBasic::OBSInit (late)
  InitBasicConfig  → basicConfig opens basic.ini (now has monitor res + 480fps)
  ResetVideo       → applies 480fps @ monitor res to libobs
  run_first_run_bootstrap_if_needed(profile, basicConfig)
    └─ enc = probe_record_encoder()
       base_cy = config_get_uint(basicConfig, "Video", "BaseCY")
       cqp = (base_cy>0 && base_cy<1080) ? 20 : 26
       apply_encoder_to_profile(profile, enc.encoder_id, cqp)   [MODIFIED: full template]
```

## Contracts

### NEW `apply_monitor_video_to_profile(const char *profile_name)` (early, C++/Qt)

- `QScreen *s = QGuiApplication::primaryScreen();` — null → return (ResetVideo's
  1920×1080 fallback at `:4312-4316` covers it).
- `qreal dpr = s->devicePixelRatio(); cx = round(s->size().width()*dpr); cy = round(...height...*dpr);`
  (mirrors OBS's own detection `window-basic-main.cpp:1264-1270`, but **omits the
  `>1920×1080 → 1920×1080` cap** at `:1278-1281`, per the explicit "monitor resolution, no cap" requirement).
- `cx<8 || cy<8` → return (don't write garbage).
- Open `obs-studio/basic/profiles/<profile>/basic.ini` (`CONFIG_OPEN_EXISTING`),
  `config_set_uint` BaseCX/BaseCY/OutputCX/OutputCY = cx/cy (base==output),
  `config_set_uint` FPSType=2/FPSNum=480/FPSDen=1, `config_save_safe`, `config_close`.
  (Same ini-edit pattern as `apply_record_path_to_profile` `:543-562`.)
- Add `#include <QScreen>` / `<QGuiApplication>`; Qt is already used in this TU.

### MODIFIED `apply_encoder_to_profile(profile_name, encoder_id, int cqp)` (late)

Signature gains `int cqp`. Header (`obspt-bootstrap.h:50`) + the single caller
(`obspt-bootstrap.cpp:612`) updated. No tests/other callers (verified by grep).

Build a **fresh** `obs_data_t` (do NOT load + mutate the old file — that is the
encoder-mismatched-keys bug) and write exactly the per-encoder key set, then
`obs_data_save_json`:

| encoder_id | keys written |
|---|---|
| `jim_nvenc` | `rate_control:"CQP"`, `cqp`, `preset:"hp"`, `preset2:"p1"`, `profile:"high"`, `tune:"ll"`, `multipass:"disabled"`, `bf:0`, `psycho_aq:false` |
| `ffmpeg_nvenc` | `rate_control:"CQP"`, `cqp`, `preset:"hq"`, `profile:"high"` (mirrors `SimpleOutput::UpdateRecordingSettings_nvenc` `:626-635`, valid for the ffmpeg wrapper) |
| `obs_qsv11` | `rate_control:"CQP"`, `qpi=qpp=qpb=cqp`, `target_usage:"quality"`, `profile:"high"`, `keyint_sec:2`, `bframes:3`, `latency:"normal"` |
| `amd_amf_h264` | `Usage:0`, `Profile:100`, `RateControlMethod:0`, `QP.IFrame=QP.PFrame=QP.BFrame=cqp`, `VBVBuffer:1`, `VBVBuffer.Size:100000`, `KeyframeInterval:2.0`, `BFrame.Pattern:0` (matches `UpdateRecordingSettings_amd_cqp` `:656-674`) |
| `obs_x264` | `rate_control:"CRF"`, `crf:cqp`, `preset:"veryfast"`, `profile:"high"`, `tune:""`, `keyint_sec:2`, `x264opts:""` (CQP→CRF 1:1; no NVENC keys) |
| default/unknown | fall back to `jim_nvenc` template (defensive) |

`encoder` field always set to `encoder_id`. CRF/CQP/QP all take the same `cqp`
int (OBS treats CQP/CRF as one quality scale — `window-basic-main-outputs.cpp` `CalcCRF`).

### CQP rule

`base_cy < 1080 → 20`, else `26`. Boundary: 1080 → 26 ("1080p 及以上"). The
720–1080 gap is unspecified by the requirement; binary `<1080 → 20` is the chosen
interpretation (anything not yet 1080p gets the lower QP).

## Static edits — sync shipped defaults to user's current runtime config

Source of truth = the user's live `finished/OBS-Redux` config, **selectively**
synced (a literal copy would break first-run, ship a black-screen scene, and bake
in machine paths). `<defaults>` = `UI/data/obspt-defaults/obs-studio`.

**SYNC (genuine setting changes):**
- `<defaults>/basic/profiles/PotPvP/basic.ini` `[Video] FPSNum` 360 → **480**.
- `<defaults>/basic/profiles/PotPvP/basic.ini` `[Hotkeys]` → **clear all 5 bindings
  to `[]`** (StartRecording/StopRecording/UnpauseRecording/StartReplayBuffer/
  StopReplayBuffer) — per user decision Q1 (was PAGEDOWN/W).
- `<defaults>/basic/profiles/PotPvP/recordEncoder.json` → canonical `jim_nvenc`
  CQP-**26** template (late bootstrap still overwrites per probed encoder).
- `<defaults>/basic/profiles/PotPvP/streamEncoder.json` `{}` →
  `{"bitrate":20000,"profile":"high"}` — per Q2.
- `<defaults>/basic/profiles/PotPvP/service.json` `{}` →
  `{"settings":{"bwtest":false,"key":"","server":"rtmp://live-push.bilivideo.com/live-bvc/","service":"Bilibili Live"},"type":"rtmp_common"}`
  (key intentionally empty — no credential shipped) — per Q2.
- `<defaults>/basic/scenes/PotPvP.json` — per Q3: item order **Minecraft (top) →
  Display (bottom)**; `Minecraft.visible=true`, `Display.visible=false`; Display
  item `pos` normalized to `0,0` (drop the -1,-1 nudge); adopt
  `Display.settings.method = 0`. Keep all other shipped scene structure.

**EXCLUDE (machine/session state — must NOT sync):**
- `RecFilePath` / `SimpleOutput.FilePath` (keep `./recordings`; bootstrap sets the
  absolute path per machine).
- `[Panels] CookieId` (UI session cookie).
- Everything new in runtime `global.ini` — esp. `[OBSPT] FirstRunCompleted=true`
  / `BootstrapVersion` (syncing breaks first-run), window geometry, version flags, BOM.
- AdvOut auto-generated fields (`TrackIndex`/`FF*`/`VodTrackIndex`) — benign OBS
  defaults; leave shipped file minimal.

**Other:**
- `UI/obspt-bootstrap.h:7`: `OBSPT_BOOTSTRAP_VERSION` 2 → **3**.

## Compatibility / rollout

- Fresh install: defaults copied, early bootstrap writes monitor res + 480, late
  writes encoder template. Works.
- Existing tester (BootstrapVersion 2): version bump → one repair pass re-asserts
  480 + monitor res + encoder template, overwriting prior video/encoder settings
  (accepted per PRD). Subsequent launches: no override.
- Rollback: revert the commit; `OBSPT_BOOTSTRAP_VERSION` returns to 2 (a
  downgraded binary simply won't repair again — no migration trap).

## Risks

- 4K/1440p @ 480fps may exceed encoder realtime throughput → no resolution cap by
  explicit requirement; documented, not mitigated.
- QSV/x264/AMF templates unverifiable on the NVIDIA-only test machine; each key is
  cited to in-repo source. AMF cannot load (enc-amf submodule empty) → probe falls
  through to x264; template is forward-looking.
- Headless/no-screen → QScreen null → early write skipped → ResetVideo 1920×1080
  fallback. Safe.

## Validation

- Build the UI/main app target (confirm exact name from `UI/CMakeLists.txt` at
  implement time) + deploy binary and `data/obspt-defaults` to `finished/OBS-Redux`.
- First-run sim: bump triggers repair on the existing install; launch; inspect log
  for `480` fps + monitor `base/output` + chosen encoder + CQP; record a clip and
  confirm non-black + correct fps/res in the file.
