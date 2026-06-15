# Research: AMD AMF (amd_amf_h264) recordEncoder.json schema (OBS 27-era)

- **Query**: Determine the exact `amd_amf_h264` (enc-amf) settings keys that appear in an OBS 27 Advanced-Output `recordEncoder.json`, the CQP rate-control keys + range, the CQP-by-resolution values (720p->20, 1080p->26), and a complete recommended JSON blob for AMD CQP recording.
- **Scope**: mixed (internal OBS source = ground truth for serialized key names; external GitHub enc-amf source = enum integer values + range confirmation)
- **Date**: 2026-06-05

## TL;DR

For OBS 27, `amd_amf_h264` is the legacy **enc-amf** plugin (obsproject/obs-amd-encoder). Constant-quality recording = **`RateControlMethod` = `0` (ConstantQP)** with **`QP.IFrame` / `QP.PFrame` / `QP.BFrame`** all set to the same QP (valid range **0-51**, lower = better). The serialized keys in `recordEncoder.json` are **flat / bare** (e.g. `"Usage"`, `"RateControlMethod"`, `"QP.IFrame"`, `"KeyframeInterval"`) — NOT the dotted `"AMF.H264.*"` form. This is confirmed by OBS 27's own simple-output AMD writer in this very fork.

**CRITICAL**: `plugins/enc-amf/` is an EMPTY submodule in this fork (0 files), so `amd_amf_h264` is never built/registered; `probe_record_encoder` will skip it. This template is forward-looking only and cannot load/select today.

## Findings

### 0. Ground truth: this fork's OBS source already writes AMF CQP keys

The authoritative key set comes from OBS itself, not the (absent) plugin. The fork's `UI/window-basic-main-outputs.cpp` `SimpleOutput::UpdateRecordingSettings_amd_cqp(int cqp)` writes these exact `obs_data` keys:

| Key (file:line) | Value written | Meaning |
|---|---|---|
| `Usage` (`window-basic-main-outputs.cpp:661`) | `0` | H264Usage::Transcoding (only mode that streams to services) |
| `Profile` (`:662`) | `100` | H264Profile::High |
| `RateControlMethod` (`:665`) | `0` | H264RateControlMethod::ConstantQP (= CQP) |
| `QP.IFrame` (`:666`) | `cqp` | I-frame QP |
| `QP.PFrame` (`:667`) | `cqp` | P-frame QP |
| `QP.BFrame` (`:668`) | `cqp` | B-frame QP |
| `VBVBuffer` (`:669`) | `1` | 1 = manual/custom buffer size |
| `VBVBuffer.Size` (`:670`) | `100000` | buffer size (kbit) |
| `KeyframeInterval` (`:673`) | `2.0` (double) | keyframe interval seconds |
| `BFrame.Pattern` (`:674`) | `0` | H264BFramePattern::None |

The streaming variant `UpdateStreamingSettings_amd` (`:637-654`) instead uses `RateControlMethod=3` (VBR_LAT), `Bitrate.Target`, `FillerData=1`. This proves the bare (un-prefixed) key form is what OBS writes/reads.

Identical code exists in OBS mainline 27.2.4 (`UI/window-basic-main-outputs.cpp:641-674`), so the schema is not fork-specific.

### 1. Exact settings keys amd_amf_h264 reads (OBS 27 / enc-amf)

The enc-amf plugin defines property keys via `TEXT_AMF_H264("x")` macros (`obs-amd-encoder` `Include/enc-h264.h`). NOTE the prefix subtlety below.

| OBS-serialized key (what's in recordEncoder.json) | enc-amf macro | Type |
|---|---|---|
| `Usage` | AMF_H264_USAGE | int (enum) |
| `QualityPreset` | AMF_H264_QUALITY_PRESET | int (enum) |
| `Profile` | AMF_H264_PROFILE | int (enum) |
| `ProfileLevel` | AMF_H264_PROFILELEVEL | int (enum, 0=Automatic) |
| `RateControlMethod` | AMF_H264_RATECONTROLMETHOD | int (enum) |
| `Bitrate.Target` | AMF_H264_BITRATE_TARGET | int (kbit) |
| `Bitrate.Peak` | AMF_H264_BITRATE_PEAK | int (kbit) |
| `QP.Minimum` / `QP.Maximum` | AMF_H264_QP_MINIMUM / _MAXIMUM | int 0-51 |
| `QP.IFrame` / `QP.PFrame` / `QP.BFrame` | AMF_H264_QP_IFRAME / _PFRAME / _BFRAME | int 0-51 |
| `FillerData` | AMF_H264_FILLERDATA | int/bool |
| `VBVBuffer` | AMF_H264_VBVBUFFER | int (0=auto via strictness, 1=manual size) |
| `VBVBuffer.Size` | AMF_H264_VBVBUFFER_SIZE | int (kbit) |
| `KeyframeInterval` | AMF_H264_KEYFRAME_INTERVAL | double (seconds) |
| `BFrame.Pattern` | AMF_H264_BFRAME_PATTERN | int 0-3 |

**Dotted-key caveat (important):** In the standalone enc-amf source, `TEXT_AMF_H264(x)` expands to `("AMF.H264." x)` (`Include/plugin.h:89-91`: `TEXT_AMF(x) = "AMF." x`, `TEXT_AMF_H264(x) = TEXT_AMF("H264." x)`). i.e. in some builds the data keys are literally `"AMF.H264.Usage"`, `"AMF.H264.QP.IFrame"`, etc. HOWEVER, OBS 27 itself (both mainline and this fork) writes/expects the **bare** keys (`"Usage"`, `"QP.IFrame"`, ...). Because OBS is the component that creates and reads `recordEncoder.json`, the **bare keys are correct for this project**. (The OBS-bundled enc-amf reads bare keys; the `AMF.H264.` prefix is the older standalone-plugin form. Do NOT mix the two in one file.)

The plugin also has a "simple"/streaming-service compatibility layer that accepts the standard OBS flat keys `rate_control` ("CQP"/"CBR"/"VBR"/"VBR_LAT"), `profile` ("high"/"main"/"baseline"/...), `preset` ("speed"/"balanced"/"quality"), `bitrate`, `keyint_sec`, then translates+unsets them (`obs-amd-encoder source/enc-h264.cpp:1360-1574`). These are only used by Simple output / service enforcement, not the Advanced record blob, but a file containing `rate_control:"CQP"` would also be honored.

### 2. Constant-quality (CQP) rate control + QP keys & range

- Rate control for constant quality = **`RateControlMethod` = `0`** (`H264RateControlMethod::ConstantQP`).
  - enum (`obs-amd-encoder Include/amf-h264.h:127`): `ConstantQP=0, ConstantBitrate=1, VariableBitrate_PeakConstrained(VBR)=2, VariableBitrate_LatencyConstrained(VBR_LAT)=3`.
- In CQP mode the encoder reads **`QP.IFrame`, `QP.PFrame`, `QP.BFrame`** (typically set all three equal). It internally forces QP min=0/max=51 (`source/enc-h264.cpp` update(): `SetQPMinimum(0); SetQPMaximum(51); SetIFrameQP(QP.IFrame); SetPFrameQP(QP.PFrame); SetBFrameQP(QP.BFrame)`).
- **Valid QP range: 0-51** (0 = best/largest, 51 = worst/smallest). Confirmed by the enc-amf wiki ("I-/P-/B-Frame QP ... Range: (best) 0 - 51 (worst)") and the source clamping. Plugin default QP = 22 for I/P/B.
- `BFrame.Pattern` (0-3) controls number of B-frames; set `0` to disable (safest, avoids VCE2 driver bug and decode issues). With pattern 0, `QP.BFrame` is ignored but harmless to include.

### 3. Mapping target CQP -> AMF QP (concrete integers)

The project rule is height-based: `< 1080 -> CQP 20`, `>= 1080 -> CQP 26` (runtime 1080p value is 26; PRD lines 18-19, 44). AMF CQP uses the same 0-51 H.264 QP scale as NVENC `cqp` / x264 CRF-ish, so the target value maps 1:1 onto all three QP keys:

| Case | Target | AMF QP.IFrame | QP.PFrame | QP.BFrame |
|---|---|---|---|---|
| <= 720p (height < 1080) | CQP 20 | 20 | 20 | 20 |
| >= 1080p (height >= 1080) | CQP 26 | 26 | 26 | 26 |

(Set I/P/B all equal — matches OBS's own `UpdateRecordingSettings_amd_cqp` which passes the single `cqp` to all three. With `BFrame.Pattern:0`, QP.BFrame is inert.)

### 4. Recommended recordEncoder.json blobs (encoder = amd_amf_h264)

Mirrors the shipped NVENC template shape (`UI/data/obsredux-defaults/.../PotPvP/recordEncoder.json`: flat object, `encoder` field + flat settings keys) and OBS 27's own AMD CQP writer. NV12 / 8-bit assumed (set at the video-output level, not in this blob).

**1080p+ case (CQP 26):**
```json
{
  "encoder": "amd_amf_h264",
  "Usage": 0,
  "Profile": 100,
  "RateControlMethod": 0,
  "QP.IFrame": 26,
  "QP.PFrame": 26,
  "QP.BFrame": 26,
  "VBVBuffer": 1,
  "VBVBuffer.Size": 100000,
  "KeyframeInterval": 2.0,
  "BFrame.Pattern": 0
}
```

**720p case (height < 1080, CQP 20):** identical but with the three QP keys = `20`:
```json
{
  "encoder": "amd_amf_h264",
  "Usage": 0,
  "Profile": 100,
  "RateControlMethod": 0,
  "QP.IFrame": 20,
  "QP.PFrame": 20,
  "QP.BFrame": 20,
  "VBVBuffer": 1,
  "VBVBuffer.Size": 100000,
  "KeyframeInterval": 2.0,
  "BFrame.Pattern": 0
}
```

Notes:
- `VBVBuffer:1` + `VBVBuffer.Size:100000` (100 Mbit) = large manual buffer, matching OBS's own CQP recording defaults; effectively unconstrained for CQP. Acceptable to omit both (encoder will auto-size) but including them matches OBS behavior.
- `KeyframeInterval` is a double (OBS writes `2.0`). For recording some guides prefer `1.0`; 2.0 is OBS's default and safe.
- Do NOT include NVENC-only keys (`preset2`, `tune`, `multipass`, `bf`, `psycho_aq`, `cqp`, `rate_control:"CQP"` is optional-but-redundant) — they are ignored by AMF and the PRD explicitly flags mismatched keys as a defect (PRD line 69, "recordEncoder.json never contains encoder-mismatched keys").

### 5. Code patterns / files (internal)

| File:line | Relevance |
|---|---|
| `UI/window-basic-main-outputs.cpp:656-678` | `UpdateRecordingSettings_amd_cqp` — authoritative AMF CQP key set |
| `UI/window-basic-main-outputs.cpp:637-654` | `UpdateStreamingSettings_amd` — AMF VBR_LAT streaming keys (RateControlMethod=3) |
| `UI/window-basic-main-outputs.cpp:375,402` | `LoadRecordingPreset_h264("amd_amf_h264")` / streaming |
| `UI/obsredux-bootstrap.cpp:363-382` | `kEncoderPriority[]` = jim_nvenc -> obs_qsv11 -> ffmpeg_nvenc -> **amd_amf_h264** -> obs_x264; `probe_record_encoder` returns first `encoder_registered()` hit |
| `UI/obsredux-bootstrap.cpp:520-535` | reads/rewrites `recordEncoder.json` `encoder` field only (current gap: leaves NVENC keys) |
| `UI/window-basic-main-profiles.cpp:830-832` | enumerates encoder types; sets `amd_supported` if `amd_amf_h264` registered |
| `UI/data/obsredux-defaults/.../PotPvP/recordEncoder.json` | shipped template (jim_nvenc, cqp 25); the AMF blob must replace ALL keys, not just `encoder` |

### External References

- enc-amf source (key macros, bare-vs-dotted): `obs-amd-encoder` `Include/enc-h264.h` (1.4.3.7 tag) — `AMF_H264_USAGE="Usage"`, `AMF_H264_QP_IFRAME="QP.IFrame"`, `AMF_H264_KEYFRAME_INTERVAL="KeyframeInterval"`, etc. Prefix macro in `Include/plugin.h:89-91`.
  - https://github.com/obsproject/obs-amd-encoder (tag 1.4.3.7 = OBS 27-era; master = 2.x rewrite which renamed `KeyframeInterval`->`Interval.Keyframe` and commented out `Usage`)
- enc-amf enum values: `Include/amf-h264.h` (1.4.3.7) — `H264RateControlMethod{ConstantQP=0,CBR=1,VBR=2,VBR_LAT=3}`, `H264Profile{Baseline=66,Main=77,High=100,...}`, `H264Usage{Transcoding=0,...}`, `H264BFramePattern{None=0,One,Two,Three}`.
- enc-amf rate_control/profile string translation: `source/enc-h264.cpp:1360-1574` (master).
- enc-amf Configuration wiki (CQP semantics, QP 0-51, B-Picture 0-3, Profile/RCM options): https://github-wiki-see.page/m/obsproject/obs-amd-encoder/wiki/Configuration
- jp9000 locale (display-name confirmation): https://github.com/jp9000/obs-studio_amf-encoder-plugin/blob/master/Resources/locale/en-US.ini
- OBS Studio AMF wiki (context: OBS 28 introduced a NEW "AMD HW" encoder distinct from OBS 27's `amd_amf_h264`): https://github.com/obsproject/obs-studio/wiki/AMF-HW-Encoder-Options-And-Information
- OBS mainline 27.2.4 cross-check (same bare keys): obs-studio `UI/window-basic-main-outputs.cpp` @ tag 27.2.4 lines 641-674.

## Caveats / Not Found

- **CRITICAL — encoder absent in this fork:** `plugins/enc-amf/` is an empty submodule (verified: `Glob plugins/enc-amf/**` = no files; `git submodule status` = uninitialized; `.gitmodules` maps it to `https://github.com/obsproject/obs-amd-encoder.git`). It is therefore not compiled and `amd_amf_h264` is never registered, so:
  - `probe_record_encoder` (`obsredux-bootstrap.cpp:371`) skips it via `encoder_registered("amd_amf_h264")==false`.
  - On an AMD-only machine the probe falls through to `obs_x264` (software). This AMF template is **forward-looking** — usable only if/when the submodule is populated and built.
- **Key-form ambiguity:** The dotted `AMF.H264.*` form exists in the standalone enc-amf source, but OBS 27 (this fork + mainline) writes/reads the **bare** form. Recommendation: use bare keys (as in section 4) to match OBS's own serializer. Could not byte-verify against the exact enc-amf binary OBS 27 bundled (no such build in this repo), but the OBS-side writer is consistent and authoritative for the file OBS itself reads back.
- **OBS 28+ note:** Newer OBS replaced `amd_amf_h264` (enc-amf) with `h264_texture_amf` ("AMD HW") which uses a different, simpler schema (`rate_control`,`cqp`,`preset`). Not relevant for OBS 27 but worth knowing if the fork is ever rebased onto a newer OBS core — the priority list and template would need updating.
- **Direct raw fetch of enc-amf source over the smart-search `fetch` provider failed** (returns HTML-only/empty for raw.githubusercontent and jsdelivr); used `curl` + GitHub trees API instead, which succeeded. Enum/key values above are quoted from the fetched source, not inferred.
- Exact recommended `VBVBuffer.Size` / `KeyframeInterval` for 480fps recording were not independently benchmarked; values mirror OBS's built-in AMD CQP recording defaults and are safe starting points.
