# Research: Intel Quick Sync (obs_qsv11) H.264 recordEncoder.json schema

- **Query**: Exact obs_qsv11 settings keys + constant-quality knob, to author a CQP recording preset (CQP 20 <=720p / CQP 26 >=1080p) for Intel iGPUs.
- **Scope**: internal (in-repo plugin `plugins/obs-qsv11/`), cross-checked with UI loader.
- **Date**: 2026-06-05
- **Codec note**: This repo's QSV plugin is **H.264 only**. CMake builds a single module from `obs-qsv11.c` (`plugins/obs-qsv11/CMakeLists.txt:60-69`); it registers `obs_qsv11` (texture, GPU path) and `obs_qsv11_soft` (fallback) — both H.264 (`obs-qsv11.c:1012-1044`). There is **no HEVC QSV** in this tree (`Glob plugins/**/obs-qsv11*hevc*` = none). Encoder id to write = `"obs_qsv11"`.

## Findings

### Files Found

| File Path | Description |
|---|---|
| `plugins/obs-qsv11/obs-qsv11.c` | Encoder entry: `obs_qsv_defaults`, `obs_qsv_props`, `rate_control_modified`, `update_params`. The single source of truth for settings keys. |
| `plugins/obs-qsv11/QSV_Encoder.h` | `qsv_ratecontrols[]` (valid rate_control strings), `qsv_param_t`, profile/usage name tables. |
| `plugins/obs-qsv11/QSV_Encoder_Internal.cpp` | Maps `qsv_param_t` -> MFX (`mfx.QPI/QPB/QPP`, `mfx.ICQQuality`, GOP from keyint). |
| `UI/window-basic-main-outputs.cpp` | Advanced Output reads `recordEncoder.json` verbatim and applies it to the record encoder. |

### 1. Exact `settings` keys obs_qsv11 reads

All read in `update_params()` via `obs_data_get_*` (`obs-qsv11.c:400-417`); defaults in `obs_qsv_defaults()` (`obs-qsv11.c:145-164`); UI ranges/types in `obs_qsv_props()` (`obs-qsv11.c:333-391`).

| Key | Type | Default | Range / valid values | Notes |
|---|---|---|---|---|
| `rate_control` | string | `"CBR"` | `CBR, VBR, VCM, CQP, AVBR, ICQ, LA_ICQ, LA_CBR, LA_VBR` (`QSV_Encoder.h:72-75`) | VCM/ICQ/LA_* require Haswell+ (`haswell_or_greater` flag) |
| `target_usage` | string | `"balanced"` | `quality, balanced, speed, veryslow, slower, slow, medium, fast, faster, veryfast` (`QSV_Encoder.h:77-80`) | This is QSV's "preset" knob (maps to `MFX_TARGETUSAGE_*`, `obs-qsv11.c:428-447`) |
| `profile` | string | `"high"` | `high, main, baseline` (`QSV_Encoder.h:76`) | -> `MFX_PROFILE_AVC_*` (`obs-qsv11.c:449-454`) |
| `keyint_sec` | int | `3` | 1..20 (`obs-qsv11.c:352`) | Keyframe interval in **seconds**; GOP = keyint_sec * fps (`QSV_Encoder_Internal.cpp:264-266`) |
| `bitrate` | int (Kbps) | `2500` | 50..10000000 (`obs-qsv11.c:361`) | Ignored for CQP/ICQ/LA_ICQ (hidden + not logged, `obs-qsv11.c:277-278,524-528`) |
| `max_bitrate` | int (Kbps) | `3000` | 50..10000000 (`obs-qsv11.c:365`) | Only VBR/VCM |
| `qpi` | int | `23` | **1..51** (`obs-qsv11.c:372`) | CQP only — I-frame QP |
| `qpp` | int | `23` | **1..51** (`obs-qsv11.c:373`) | CQP only — P-frame QP |
| `qpb` | int | `23` | **1..51** (`obs-qsv11.c:374`) | CQP only — B-frame QP |
| `icq_quality` | int | `23` | **1..51** (`obs-qsv11.c:375-376`) | ICQ / LA_ICQ only — lower = better quality |
| `latency` | string | `"normal"` | `ultra-low, low, normal` (`QSV_Encoder.h:81-82`) | Sets AsyncDepth + lookahead depth (`obs-qsv11.c:486-496`) |
| `bframes` | int | `3` | 0..3 (`obs-qsv11.c:384`) | Forced to 0 on SNB/IVB (`obs-qsv11.c:423-424`) |
| `accuracy` | int | `1000` | 0..10000 (`obs-qsv11.c:369`) | AVBR only |
| `convergence` | int | `1` | 0..10 (`obs-qsv11.c:370`) | AVBR only |
| `enhancements` | bool | `false` | — | Subjective video enhancements (MBBRC+CQM); shown only on Skylake+ for CBR/VBR (`obs-qsv11.c:299-302,386-388`) |

Legacy/auto-handled keys (read but not the primary API): `cbr` (bool, deprecated override -> forces CBR, `obs-qsv11.c:458-465`); `bf` (int, overrides `bframes` if user-set, `obs-qsv11.c:419-420`); `async_depth` / `la_depth` (migrated into `latency` by `update_latency`, `obs-qsv11.c:194-236`); `mbbrc` / `CQM` (migrated into `enhancements` by `update_enhancements`, `obs-qsv11.c:238-261`).

**There is no single "qp" key.** Constant-QP uses three separate keys `qpi`/`qpp`/`qpb`. There is no "CQP-with-one-qpp"; all three exist and all three are applied (`QSV_Encoder_Internal.cpp:236-240`).

### 2. Which rate_control gives constant QUALITY (and its key + range)

Two constant-quality modes exist in this plugin:

- **`CQP`** (constant **quantizer**): reveals `qpi`/`qpp`/`qpb` (`rate_control_modified`, `obs-qsv11.c:286-292`). Maps to `MFX_RATECONTROL_CQP`; internal applies `mfx.QPI/QPB/QPP` directly (`QSV_Encoder_Internal.cpp:236-240`). Range **1..51** per QP. Lower QP = higher quality / bigger files. Works on **all** platforms (`haswell_or_greater=false`). This is the closest QSV analog to x264 CRF / NVENC CQP and is the right choice for a "CQP N" recording preset.
- **`ICQ`** (Intelligent Constant Quality): reveals `icq_quality` (`obs-qsv11.c:294-297`). Maps to `MFX_RATECONTROL_ICQ` -> `mfx.ICQQuality` (`QSV_Encoder_Internal.cpp:246-247`). Range **1..51**, lower = better. Requires **Haswell+**. `LA_ICQ` = ICQ + lookahead (also Haswell+, also uses `icq_quality`).

**Recommendation for a "CQP N" preset: use `rate_control = "CQP"` with `qpi`/`qpp`/`qpb`.** It is the literal constant-quantizer mode the task names ("CQP 20" / "CQP 26"), the integer maps 1:1 onto the QP value, and it has no platform gate (older Intel iGPUs without ICQ still work).

### 3. How "CQP 20 (<=720p)" and "CQP 26 (>=1080p)" map onto the keys

"CQP N" = set the QP value N. The plugin has 3 QP knobs. Two reasonable conventions:

- **Flat (simplest, recommended):** set all three equal to N: `qpi = qpp = qpb = N`. Gives exactly "CQP N".
- Tiered (mirrors common OBS practice, optional): keep `qpi` slightly lower (sharper keyframes), `qpp = N`, `qpb` slightly higher. Not required by the task; the flat mapping is unambiguous and matches the "CQP N" label.

Concrete integer values to write:

| Target | qpi | qpp | qpb |
|---|---|---|---|
| **<=720p -> CQP 20** | 20 | 20 | 20 |
| **>=1080p -> CQP 26** | 26 | 26 | 26 |

(Both are inside the valid 1..51 range. `icq_quality` is irrelevant in CQP mode and can be omitted; OBS keeps the default 23 internally but it is unused.)

### 4. Recommended `recordEncoder.json` objects

QSV ignores `bitrate`/`max_bitrate` in CQP, so they may be omitted; `target_usage`, `profile`, `keyint_sec`, `bframes`, `latency` should be set explicitly for a deterministic preset. `"encoder":"obs_qsv11"` selects the GPU/texture path (falls back to `obs_qsv11_soft` automatically if not an Intel primary GPU, `obs-qsv11.c:735-757`).

**>=1080p (CQP ~26):**
```json
{
  "encoder": "obs_qsv11",
  "rate_control": "CQP",
  "qpi": 26,
  "qpp": 26,
  "qpb": 26,
  "target_usage": "quality",
  "profile": "high",
  "keyint_sec": 2,
  "bframes": 3,
  "latency": "normal"
}
```

**<=720p (CQP ~20):**
```json
{
  "encoder": "obs_qsv11",
  "rate_control": "CQP",
  "qpi": 20,
  "qpp": 20,
  "qpb": 20,
  "target_usage": "quality",
  "profile": "high",
  "keyint_sec": 2,
  "bframes": 3,
  "latency": "normal"
}
```

Notes on chosen values (all are valid per ranges above, but are preset *choices*, not forced by code):
- `target_usage`: `"quality"` -> `MFX_TARGETUSAGE_BEST_QUALITY` for recording (default is `"balanced"`). Use `"balanced"` if you want OBS's stock default.
- `keyint_sec`: `2` is a common recording value (range 1..20; plugin default is 3).
- `bframes`: 3 (plugin default; auto-forced to 0 on SandyBridge/IvyBridge).
- `profile`: `"high"` (plugin default).

### Related Specs

- `.trellis/spec/plugins/` lists `enc-amf, mac-syphon, obs-browser, obs-outputs, obs-vst, win-dshow` — no dedicated `obs-qsv11` spec doc exists.

## Caveats / Not Found

- **QP key naming:** confirmed `qpi`/`qpp`/`qpb` (lowercase) as obs_data keys; UI labels are literal `"QPI"/"QPP"/"QPB"` (`obs-qsv11.c:372-374`). There is **no** single `"qp"` or `"cqp"` key.
- **ICQ vs CQP:** ICQ's key is `icq_quality` (1..51, Haswell+); CQP's keys are `qpi/qpp/qpb` (1..51, all platforms). The task asked to confirm — it is CQP with three QP keys, *not* a single `qpp`, and *not* `icq_quality`.
- **Defaults OBS fills automatically:** if a key is absent from `recordEncoder.json`, `obs_qsv_defaults` supplies it (`target_usage=balanced, bitrate=2500, max_bitrate=3000, profile=high, rate_control=CBR, qpi=qpp=qpb=23, icq_quality=23, keyint_sec=3, latency=normal, bframes=3, enhancements=false`). So a minimal CQP preset only strictly needs `encoder`, `rate_control`, and `qpi/qpp/qpb`; the rest fall back to defaults.
- **Bitrate ignored in CQP:** `bitrate`/`max_bitrate` are not used when `rate_control=CQP|ICQ|LA_ICQ` (hidden in UI, skipped in logging, `obs-qsv11.c:277-278,524-528`). Including them is harmless but meaningless.
- **HEVC:** No HEVC QSV encoder exists in this repo, so the H.264-vs-HEVC key-difference caveat is moot here. If a future HEVC QSV plugin is added it would have its own `*_defaults`/properties; do not assume these keys carry over.
- **Field width quirk:** `update_params` stores QP/bitrate into `mfxU16` fields, but the runtime path that matters for CQP (`QSV_Encoder_Internal.cpp:236-240`) copies the 1..51 QP straight into `mfx.QP*`, so 20/26 are safe.
- The exact UI default `recordEncoder.json` shipped by OBS 27 was not located as a static file in-repo (it is generated/written at runtime via the settings dialog); the JSON above is constructed from the verified key schema, not copied from a stock file.
