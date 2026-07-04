# fix AMD AMF encoder selection

## Goal

On AMD-only or AMD-primary Windows machines, OBS-PT should expose and select the
AMD AMF H.264 encoder for the PotPvP recording Profile instead of leaving
Advanced Output recording on `(Use Stream Encoder)` / `RecEncoder=none` or
falling back to software unexpectedly.

## User Report

- On another Windows machine with a Radeon RX 550X, after installing OBS-PT, the
  Settings -> Output -> Recording encoder list shows:
  - `(Use Stream Encoder)`
  - `QuickSync H.264`
  - `x264`
- AMD AMF is absent from the list.
- The active Advanced Output recording encoder is `(Use Stream Encoder)`.
- User reference: `https://www.obsproject.com.cn/obs/174.html`, which describes
  AMD AMF/VCE as AMD GPU hardware encoding.

## Confirmed Facts

- AMD tester feedback on 2026-06-29 confirms the packaged build can now expose
  and auto-select AMF on the target machine. The remaining issue is AMF default
  settings quality, not encoder registration.
- The tester reports the AMF preset field appears empty, the AMF quality preset
  should be the maximum-performance option, and AMF CQP should write
  `QP.IFrame=QP.PFrame=QP.BFrame=26`.
- The legacy AMF plugin exposes two separate concepts: top-level `Preset`
  templates (`Recording`, `HighQuality`, etc.) and `QualityPreset`
  (`Speed=0`, `Balanced=1`, `Quality=2`). `QualityPreset=0` is the
  maximum-performance / fastest option.
- The AMF plugin's built-in `HighQuality` template is not suitable for the
  requested OBS-PT default because it writes `QP.IFrame=26`, `QP.PFrame=24`,
  and `QP.BFrame=22`, not all 26.
- The AMF plugin hides and resets `QP.BFrame` when B-frames are disabled or
  unsupported. OBS-PT currently keeps `BFrame.Pattern=0` for performance, so the
  config file can write B-frame QP 26 even if the visible UI may hide/reset that
  field on hardware where B-frames are not active.
- `UI/window-basic-settings.cpp::LoadEncoderTypes()` fills the Advanced
  recording encoder dropdown from `obs_enum_encoder_types()`. It does not
  special-case-hide AMF; if `amd_amf_h264` is not registered, it cannot appear.
- `UI/window-basic-settings.cpp::EncoderAvailable()` uses the same encoder
  registry enumeration for Simple Output hardware encoder options.
- `UI/obspt-bootstrap.cpp::kEncoderPriority` now prefers
  `amd_amf_h264` after NVENC and before QSV, so mixed Intel+AMD machines use
  AMF before Quick Sync when both encoders register.
- `UI/obspt-bootstrap.cpp::apply_encoder_to_profile()` already has an
  `amd_amf_h264` branch that writes OBS 27-era AMF CQP keys
  (`RateControlMethod`, `QP.IFrame`, `QP.PFrame`, `QP.BFrame`, etc.).
- `.trellis/spec/obspt-bootstrap.md` now documents AMF as a packaged legacy
  plugin dependency, not a forward-looking empty-submodule placeholder.
- `plugins/CMakeLists.txt` only builds AMF when
  `plugins/enc-amf/CMakeLists.txt` exists; otherwise it logs that `enc-amf`
  is disabled.
- `plugins/enc-amf/` is populated from `obsproject/obs-amd-encoder` tag
  `1.4.3.7`, and `build-v143/rundir/RelWithDebInfo/obs-plugins/64bit/` plus
  `build-v143/_pkg/OBS-PT/obs-plugins/64bit/` contain `enc-amf.dll`.
- The repository `.gitmodules` already declares `plugins/enc-amf` pointing to
  `https://github.com/obsproject/obs-amd-encoder.git`.
- External context:
  - OBS's official hardware encoding KB lists AMD AMF as a hardware encoder and
    says AMD AMF is supported on Windows/Linux, subject to compatible GPU and
    driver.
  - `obsproject/obs-amd-encoder` is archived/read-only as of 2024-06-13, but it
    is the OBS 27-era plugin that provides native AMD AMF encoding.

## Requirements

- Restore and ship the legacy OBS 27 `enc-amf` plugin as a first-class OBS-PT
  dependency for Windows AMD H.264 hardware encoding.
- AMF support must be decided and implemented at the plugin/build/package layer,
  not by editing the encoder dropdown filter.
- If AMF is supported, `amd_amf_h264` must be built, copied to `rundir`,
  included in installer staging, and registered at runtime on compatible AMD
  machines.
- On a compatible AMD-only machine, first-run/bootstrap should write
  `AdvOut.RecEncoder=amd_amf_h264` or otherwise make Advanced recording use the
  AMF record encoder directly, not `(Use Stream Encoder)`.
- On mixed Intel+AMD machines where both QSV and AMF register, OBS-PT should
  prefer AMF over QSV for first-run/default recording selection.
- `recordEncoder.json` for AMF must contain AMF-valid keys only; no NVENC/QSV
  settings should leak into the file.
- AMF defaults must explicitly write the maximum-performance quality preset:
  `QualityPreset=0` (`Speed`).
- AMF defaults must write CQP 26 for I/P/B frame QP fields on the PotPvP Profile.
- If AMF cannot be built or is intentionally unsupported, OBS-PT must make the
  fallback behavior explicit in docs/specs so future sessions do not chase the
  UI dropdown as the root cause.
- A regenerated installer is required after the fix so the AMD tester can
  install and validate it.

## Acceptance Criteria

- [x] `plugins/enc-amf` is no longer empty, or the task explicitly records AMF
      as unsupported for this release with a tested fallback.
- [x] The v143 build includes the AMF plugin when AMF support is in scope.
- [x] `build-v143/rundir/RelWithDebInfo/obs-plugins/64bit/` contains the AMF
      plugin DLL when AMF support is in scope.
- [x] `build-v143/_pkg/OBS-PT/obs-plugins/64bit/` contains the AMF plugin DLL
      after package staging when AMF support is in scope.
- [x] On an AMD machine, the Settings -> Output encoder list shows AMD AMF.
- [x] On a fresh AMD install, the PotPvP Profile uses `amd_amf_h264` for
      Advanced Output recording rather than `(Use Stream Encoder)`.
- [x] AMF `recordEncoder.json` writes `QualityPreset=0` and
      `QP.IFrame=QP.PFrame=QP.BFrame=26`.
- [x] On a mixed Intel+AMD machine with both QSV and AMF registered, bootstrap
      chooses `amd_amf_h264` before `obs_qsv11`.
- [ ] A recording starts and stops successfully on the AMD tester machine.
- [x] A new `OBS-PT-1.0.0-Installer.exe` or next-version installer is generated
      after validation/staging.

## Local Validation Results

- `plugins/enc-amf` is pinned to `obsproject/obs-amd-encoder` tag `1.4.3.7`
  (`8de4ffd919b2a7958e531045e238f2a907896a33`).
- CMake auto-builds AMF when `DepsPath/include/AMF/components/VideoEncoderVCE.h`
  exists, by staging the legacy SDK layout under `build-v143/_amf-sdk`.
- `cmake --build build-v143 --config RelWithDebInfo --target enc-amf --parallel`
  succeeds with VS2022 BuildTools.
- `cmake --build build-v143 --config RelWithDebInfo --target obs --parallel`
  rebuilds `OBS-PT.exe` with the AMF-before-QSV priority.
- `build-v143/rundir/RelWithDebInfo/obs-plugins/64bit/enc-amf.dll` exists.
- `build-v143/_pkg/OBS-PT/obs-plugins/64bit/enc-amf.dll` exists after staging.
- `build-v143/_pkg/OBS-PT-1.0.0-Installer.exe` was regenerated and integrity
  checked on 2026-07-02 12:45:15 local time after an NSIS integrity error was
  reported against the previous installer copy. SHA256:
  `9C7EE4364D1667F8F7557A6AEC7080E2E7452A7B67534DFA8BE000944F43CADE`.
- After the 30-second stop-watchdog change, `OBS-PT.exe` was rebuilt and the
  installer was regenerated on 2026-07-04 11:35:19 local time. SHA256:
  `B855AD11D7DF5390A693444EE9740529322D696579D003E001A3BAC023D767BC`.
  `7z t` passed (`Type=Nsis`, `Files=1463`, `Everything is Ok`).

AMD hardware acceptance still requires tester validation because this machine
does not prove AMF runtime registration on a Radeon GPU.

## AMD Tester Feedback 2026-06-29

- AMF is now visible and automatically selected on the AMD tester machine.
- The AMF preset field is empty in the UI.
- Desired AMF quality preset: maximum performance (`QualityPreset=0` / Speed).
- Desired AMF QP defaults: I-frame QP 26, P-frame QP 26, B-frame QP 26.
- The tester reports a P2 popup related to P3 and a P4 AMF-exclusive bug. The
  exact text/behavior still needs one focused clarification because the images
  are not available as local files in this session.

## AMD Tester Feedback 2026-07-03

- On another AMD GPU machine, recording can start but stopping recording still
  leaves OBS-PT not responding.
- The screenshot shows OBS-PT 1.0.0 in the PotPvP Profile/Scene, recording at
  `480.00 fps`, with the record timer at about 6 seconds and the status bar
  warning in Chinese: `编码过载！请考虑降低视频设置或使用更快的编码预设。`
- Local code inspection shows OBS-PT still writes `FPSNum=480` in both the
  shipped PotPvP `basic.ini` and `apply_monitor_video_to_profile()`, while the
  legacy AMF encoder stop path calls `m_AMFEncoder->Drain()` after overloaded
  input queue behavior.
- Product decision from user: high-FPS recording is foundational for OBS-PT, so
  AMD AMF must keep the 480fps PotPvP default. Do not solve this class of bug by
  lowering AMD capture/output FPS.
- Product decision from user: stopping should preferably wait for AMF to drain
  queued frames rather than immediately discarding backlog. The remaining fix
  direction is therefore stop-path resilience with bounded waiting and a
  responsive UI, not a lower-FPS fallback and not an eager drop-on-stop policy.
- Product decision from user: the bounded wait before forced recovery is 30
  seconds. The first stop should allow AMF to drain normally; if stop completion
  has not arrived after 30 seconds, OBS-PT may enter the existing force-stop
  path to recover the UI/process.

## Out of Scope

- Porting OBS 28+ `h264_texture_amf` / modern AMD HW encoder into this OBS 27
  fork unless explicitly chosen as the scope.
- HEVC/AV1 AMF support.
- Re-ranking the overall encoder priority beyond the AMD-specific fix.
- Tuning final AMD quality/performance defaults beyond the existing CQP-by-height
  baseline, except the AMD tester-confirmed `QualityPreset=0` and QP 26 values.

## Decisions

- Restore and ship the legacy OBS 27 `enc-amf` plugin.
- AMF takes priority over QSV on mixed Intel+AMD systems. Final priority target:
  `jim_nvenc -> amd_amf_h264 -> obs_qsv11 -> ffmpeg_nvenc -> obs_x264`.
