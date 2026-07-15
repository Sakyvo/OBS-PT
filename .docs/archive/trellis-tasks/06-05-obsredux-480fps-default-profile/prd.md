# Update default profile to 480fps with hardware-adaptive encoder/resolution/bitrate

## Goal

Update the shipped OBS-PT default (PotPvP) profile to the current **480fps**
baseline, and extend first-run bootstrap so that **encoder (+ its full settings),
resolution, and bitrate(CQP)** are set to each user's best case at first run
(and on version-bump re-repair) instead of being hardcoded for a 1080p/NVIDIA
machine. Color format stays NV12.

## Requirements

- Shipped `basic.ini`: `FPSNum` 360 → **480** (static; satisfies "update the
  default file to 480fps").
- Bootstrap sets **Base=Output = primary-monitor native resolution**, written
  early enough to take effect the same session (before `ResetVideo()` reads
  `basic.ini`).
- Bootstrap writes the **complete, encoder-appropriate** `recordEncoder.json`
  settings blob for the probed encoder — not just the `encoder` id — so no
  encoder-mismatched keys survive.
- **CQP/quality by resolution** (constant-quality): height `< 1080 → 20`,
  height `>= 1080 → 26`, mapped per encoder:
  - `jim_nvenc`: `rate_control:CQP`, `cqp` = 20/26 (existing template).
  - `obs_qsv11`: `rate_control:CQP`, `qpi=qpp=qpb` = 20/26.
  - `obs_x264`: `rate_control:CRF`, `crf` = 20/26 (OBS maps CQP→CRF 1:1); drop
    all NVENC keys; `preset:"veryfast"`, fallback-only.
  - `amd_amf_h264`: `RateControlMethod:0` + `QP.IFrame/PFrame/BFrame` = 20/26
    (forward-looking; enc-amf absent from this fork's build).
  - `ffmpeg_nvenc`: template derived from `plugins/obs-ffmpeg` in design.
- Color format stays **NV12** (8-bit 4:2:0) for all encoders.
- Bump `OBSPT_BOOTSTRAP_VERSION` 2 → 3 so existing testers re-adapt.
- On first-run AND version-bump repair, (re)assert the canonical adaptive video
  + encoder settings (this tool's defaults are meant to be pushed to all testers).

## Acceptance Criteria

- [ ] Fresh first run on the NVIDIA/1080p test machine: 480fps, 1920x1080
      base=output, `jim_nvenc` CQP 26, non-black recording.
- [ ] Simulated non-1080p monitor: base=output match the monitor; CQP/quality
      follows the resolution rule (e.g. 1366x768 → 20, 2560x1440 → 26).
- [ ] After encoder adaptation, `recordEncoder.json` contains ONLY keys valid for
      the chosen encoder (no leftover NVENC keys on QSV/x264).
- [ ] Existing tester (bootstrap v2) re-adapts to the new canonical config after
      the version bump.
- [ ] Resolution change takes effect on the first launch (not deferred), or the
      deferral is explicitly accepted.

## Definition of Done

- Builds the `obs-studio`/UI target and deploys to `finished/OBS-Redux`.
- Manual first-run verification on the NVIDIA test machine (record a clip; check
  fps/res/encoder/CQP in log + non-black output).
- `obspt-bootstrap` spec updated with the new adaptation contract.

## Technical Approach

- **Static**: edit shipped `basic.ini` FPSNum→480.
- **Runtime (bootstrap)**:
  - Resolution: detect primary monitor (Qt `QGuiApplication::primaryScreen()` or
    Win32) and write Base/Output to `basic.ini` in the **early** bootstrap
    (AppInit, before `OBSBasic::OBSInit()`/`ResetVideo()`), so it applies the
    same session. (Encoder/CQP can stay in the late bootstrap since
    `recordEncoder.json` is read at record time and the probe needs modules.)
  - Encoder: keep `probe_record_encoder`; replace `apply_encoder_to_profile`'s
    id-only write with a **per-encoder full-template writer** keyed off the
    probed `encoder_id`, parameterized by the resolution-derived CQP/CRF/QP.
- Color format: no change (NV12 already shipped).

## Decision (ADR-lite)

**Context**: "Best case" encoder/resolution/bitrate cannot be baked into a static
file; it depends on the user's GPU vendor and monitor.

**Decisions**:
1. "码率" = **CQP constant-quality value** (not Mbps), mapped per encoder.
2. **Full per-vendor encoder templates** (NVENC/QSV/AMF/x264), not just id-swap —
   required for correctness (mismatched keys otherwise). AMF is forward-looking
   (submodule empty); x264 is a fallback (480fps software encode infeasible).
3. Color format = **NV12** for all (480fps H.264 precludes 10-bit P010).
4. CQP by height: `<1080 → 20`, `>=1080 → 26`.

**Consequences**: QSV/x264 paths are testable only indirectly (no non-NVIDIA
hardware locally); AMF template is unverifiable until enc-amf is built. The
version bump re-asserts canonical settings, overwriting any manual tester tweaks
to video/encoder — acceptable for this controlled tool.

## Out of Scope

- Streaming settings (`streamEncoder.json`, `service.json`), hotkeys, scene
  structure, Game Capture / Lunar Client tuning.
- Reordering the encoder priority list (ffmpeg_nvenc currently sits below QSV).

## Research References

- [`research/encoder-qsv.md`](research/encoder-qsv.md) — obs_qsv11 CQP uses
  `qpi/qpp/qpb` (1–51); full 1080p/720p blobs, cited to repo.
- [`research/encoder-x264.md`](research/encoder-x264.md) — obs_x264 CRF (0–51),
  CQP→CRF 1:1; must drop NVENC keys (`tune:"ll"` invalid for x264).
- [`research/encoder-amf.md`](research/encoder-amf.md) — amd_amf_h264
  `RateControlMethod:0` + `QP.IFrame/PFrame/BFrame`; enc-amf absent → cannot load.

## Technical Notes

- Files: `UI/obspt-bootstrap.cpp/.h`,
  `UI/data/obspt-defaults/obs-studio/basic/profiles/PotPvP/{basic.ini,recordEncoder.json}`.
- `OBSPT_BOOTSTRAP_VERSION` currently 2 (`UI/obspt-bootstrap.h:7`).
- Scene `PotPvP.json` needs no change: `Display` (monitor_capture) renders 1:1
  (`bounds_type:0`, `scale 1.0`), so base=monitor-res fills it exactly.
