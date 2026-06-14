# Research: obs_x264 (software H.264) recordEncoder.json settings schema

- **Query**: x264 (obs_x264) H.264 encoder settings schema for an Advanced Output `recordEncoder.json`, to author a constant-quality (CRF) software-fallback recording preset; map our NVENC "CQP 20 (<=720p) / CQP 26 (>=1080p)" rule onto x264 CRF.
- **Scope**: internal (in-repo plugin + UI) + cross-reference
- **Date**: 2026-06-05

## Findings

### Files Found

| File Path | Description |
|---|---|
| `plugins/obs-x264/obs-x264.c` | The `obs_x264` encoder. Settings keys, defaults, rate-control mapping. Encoder id = `"obs_x264"`, codec `"h264"` (`obs-x264.c:828-843`). |
| `UI/window-basic-main-outputs.cpp` | `SimpleOutput` quality-preset code: proves NVENC `cqp` and x264 `crf` share the same numeric quality value. |
| `UI/window-basic-auto-config-test.cpp` | OBS auto-config wizard's recommended x264 high-quality recording block (`crf:20`, `rate_control:CRF`, `profile:high`, `preset:veryfast`). |
| `UI/data/obsredux-defaults/.../PotPvP/recordEncoder.json` | Current shipped NVENC blob (CQP 25; runtime uses CQP 26) — the source values to map from. |

### 1. Exact `settings` keys obs_x264 reads

From `obs_x264_defaults()` (`obs-x264.c:97-114`) and `update_params()` / `update_settings()` (`obs-x264.c:385-637`). Key | type | default | read at:

| Key | Type | Default | Notes / file:line |
|---|---|---|---|
| `rate_control` | string | `"CBR"` | `obs-x264.c:107`, parsed `398-441`. One of `CBR` / `ABR` / `VBR` / `CRF` (`obs-x264.c:181-184`). |
| `crf` | int | `23` | `obs-x264.c:103`, read `404`. Range **0–51** (props `obs-x264.c:197`). Only used when `rate_control` is **not** ABR/CBR (i.e. VBR or CRF). For CRF, `bitrate`/`buffer_size` are forced to 0 (`433-437`) and `f_rf_constant = crf` (`520`). |
| `bitrate` | int (Kbps) | `2500` | `obs-x264.c:99`, read `401`. Forced 0 in CRF mode (`435`). |
| `use_bufsize` | bool | `false` | `obs-x264.c:100`, read `408`. Irrelevant in CRF (buffer forced 0). |
| `buffer_size` | int (Kbps) | `2500` | `obs-x264.c:101`, read `402`. |
| `keyint_sec` | int (sec) | `0` | `obs-x264.c:102`, read `403` → `i_keyint_max = keyint_sec * fps` (`443-445`). `0` = x264 default GOP. |
| `preset` | string | `"veryfast"` | `obs-x264.c:109`, read `589`. Validated against `x264_preset_names`; invalid → `"veryfast"` (`validate_preset`, `336-342`). |
| `profile` | string | `""` (None) | `obs-x264.c:110`, read `590`. UI choices: `""` / `baseline` / `main` / `high` (`obs-x264.c:209-212`); applied via `x264_param_apply_profile` (`326-334`). |
| `tune` | string | `""` (None) | `obs-x264.c:111`, read `591`. Validated against `x264_tune_names`. |
| `x264opts` | string | `""` | `obs-x264.c:112`, read `593`. Space-separated `key=value` raw x264 params (`obs_parse_options` → `set_param`, `311-324`). Can override preset/profile/tune (`override_base_params`, `299-307`). |
| `repeat_headers` | bool | `false` | `obs-x264.c:113`, read `594`. Hidden prop (`227-229`); for some muxers. Leave default. |
| `bf` | int | (no default) | read `407`, applied only if user set it (`obs_data_has_user_value`, `468-469`). B-frame count. Optional. |
| `cbr` | bool | (deprecated) | `obs-x264.c:409,417` — legacy alias forcing CBR; do **not** use. |
| `vfr` | bool | (compiled out) | Behind `#ifdef ENABLE_VFR`, normally absent. Ignore. |

There is **no** `multipass`, `preset2`, `psycho_aq`, `qpi/qpp/qpb`, or `cqp` key in obs_x264 — those are NVENC/QSV-only. (Directly relevant to the PRD "encoder-mismatched keys" gap: an x264 blob must NOT carry `preset2`/`tune:ll`/`multipass`/`cqp`.)

### 2. Constant-QUALITY rate_control string + quality knob

- **`rate_control` = `"CRF"`** is the constant-quality mode (`obs-x264.c:155,184,433`). It sets `i_rc_method = X264_RC_CRF` and `f_rf_constant = crf` (`obs-x264.c:518-521`), and zeroes bitrate/bufsize (`435-436`).
- The quality knob key is **`crf`**, int, **valid range 0–51** (lower = higher quality / bigger file; `obs-x264.c:197`). obs_x264 default `crf = 23` (`obs-x264.c:103`).
- (`VBR` also exposes `crf` as a quality target alongside a bitrate cap, but `CRF` is the pure constant-quality recording mode and the correct choice here.)

### 3. Mapping our NVENC CQP rule onto x264 CRF

**NVENC CQP and x264 CRF are treated as the same numeric quality scale by OBS itself**, so a 1:1 mapping (CQP value → CRF value) is the in-repo convention — no adjustment needed:

- `SimpleOutput::UpdateRecordingSettings()` computes one quality int and feeds it unchanged to every encoder backend: `UpdateRecordingSettings_x264_crf(crf)` sets `crf` (`window-basic-main-outputs.cpp:574-580`), `_qsv11(crf)` sets `qpi/qpp/qpb` (`606-621`), `_nvenc(cqp)` sets `cqp` (`626-632`) — all from the same `crf = CalcCRF(ultra_hq ? 16 : 23)` (`680-696`). The same number is used for NVENC CQP and x264 CRF.
- Both are 0–51 H.264 QP-domain scales (lower = better), which is why OBS shares them. So:

| Resolution rule (PRD) | NVENC `cqp` | x264 `crf` (1:1) |
|---|---|---|
| `<= 720p` (height < 1080) | 20 | **20** |
| `>= 1080p` (height >= 1080) | 26 | **26** |

Concrete x264 values to use: **crf 20 for <=720p, crf 26 for >=1080p.** (Shipped NVENC blob currently `cqp 25`, runtime `cqp 26` per PRD — mirror whichever the rest of the task standardizes on; the 1080p target is 26.)

Independent corroboration that x264 CRF 20 is "high-quality recording" in this codebase: the OBS auto-config wizard's recording-quality path sets `crf:20`, `rate_control:CRF`, `profile:high`, `preset:veryfast` (`window-basic-auto-config-test.cpp:546-549`).

Caveat on exactness: NVENC CQP and x264 CRF are not bit-exact identical perceptual operating points (different encoders/rate-control internals), but OBS deliberately maps them 1:1 and that is the established convention to follow here. Do not apply an offset.

### 4. Recommended `recordEncoder.json` objects (obs_x264, CRF)

Keys limited to those obs_x264 actually reads (section 1). `keyint_sec: 2` matches the wizard's 2s GOP (`window-basic-auto-config-test.cpp:226`) and is a safe explicit value (default `0` = x264-chosen GOP is also fine). `profile: "high"` matches both the wizard and `UpdateRecordingSettings_x264_crf` (`outputs.cpp:580`). `tune` left empty (NVENC's `ll` is not an x264 tune; valid x264 tunes are film/animation/grain/stillimage/psnr/ssim/fastdecode/zerolatency).

**1080p+ (crf 26):**
```json
{
  "encoder": "obs_x264",
  "rate_control": "CRF",
  "crf": 26,
  "preset": "veryfast",
  "profile": "high",
  "tune": "",
  "keyint_sec": 2,
  "x264opts": ""
}
```

**<=720p (crf 20):**
```json
{
  "encoder": "obs_x264",
  "rate_control": "CRF",
  "crf": 20,
  "preset": "veryfast",
  "profile": "high",
  "tune": "",
  "keyint_sec": 2,
  "x264opts": ""
}
```

Notes:
- `preset`: `"veryfast"` is the obs_x264 default (`obs-x264.c:109`) and the wizard's recording preset. At 360–480 fps software encode it is still almost certainly too slow (see caveats); if the priority is "don't drop the whole pipeline" over quality, use `"ultrafast"` (the value `UpdateRecordingSettings_x264_crf` picks under `lowCPUx264`, `outputs.cpp:581-582`). Recommendation: `"veryfast"` for quality parity with the wizard, with `"ultrafast"` as the documented perf escape hatch.
- Omitting unread NVENC keys (`cqp`, `preset2`, `multipass`, `psycho_aq`, `bf`, `bitrate`) is intentional — obs_x264 ignores them, but leaving them in the file is the exact "encoder-mismatched keys" anti-pattern the PRD acceptance criteria forbid.

### Related Specs

- No `.trellis/spec/plugins/obs-x264*` spec exists; `obs-x264` is listed under plugins in the task project info but has no dedicated spec doc (searched `.trellis/spec/**`). The plugin source is the authority.
- PRD: `.trellis/tasks/06-05-obsredux-480fps-default-profile/prd.md` — CQP-by-resolution rule (height <1080 → 20, >=1080 → 26), encoder-probe fallback chain (`jim_nvenc → obs_qsv11 → ffmpeg_nvenc → amd_amf_h264 → obs_x264`), and the "swap encoder id but leave NVENC keys" gap.
- Probe code: `UI/obsredux-bootstrap.cpp:363-382` (`kEncoderPriority`, `probe_record_encoder`) — `obs_x264` is the final `is_software_fallback = true` entry.

## Caveats / Not Found

- **Software encode at 360–480 fps is generally infeasible.** x264 must compress 360–480 1080p frames/sec on the CPU in real time; even `ultrafast` on high-core-count CPUs will not sustain this for 1080p, and the encoder backpressure will force OBS to drop frames or stall. This obs_x264 preset is a **correctness fallback only** (so a non-NVIDIA/non-QSV/non-AMF machine produces a valid, non-black file and a working config), not a usably-performant 480fps recording path. Treat real 480fps capture as requiring a hardware encoder.
- `crf 26` at 1080p with `veryfast` yields a low-bitrate-ish but valid file; quality/perf both secondary to "it works" for the fallback.
- The exact perceptual equivalence of NVENC CQP vs x264 CRF is approximate; codebase convention is a 1:1 number mapping (section 3) and that is what to ship — no measured offset was found or is warranted.
- I did not run an external web search: the in-repo plugin and UI fully answer every sub-question; the 0–51 range, CRF semantics, and 1:1 CQP/CRF convention are all confirmed from source (`obs-x264.c`, `window-basic-main-outputs.cpp`).
- B-frames: obs_x264 only sets `i_bframe` if the user explicitly provides `bf` (`obs-x264.c:468-469`); otherwise the preset's default applies. No need to set `bf` for the fallback.
