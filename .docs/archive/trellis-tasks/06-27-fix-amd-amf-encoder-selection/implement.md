# Implement: AMD AMF encoder selection

## Checklist

1. Confirm planning approval. **Done.**
   - User accepted restoring and shipping legacy OBS 27 `enc-amf`.
   - User chose AMF before QSV on mixed Intel+AMD systems.
   - Run `trellis-before-dev` before editing code.

2. Populate `plugins/enc-amf`. **Done.**
   - Initialize/update the existing submodule from `.gitmodules`.
   - Pin to an OBS 27-compatible revision/tag, preferably the prior-researched
     `1.4.3.7` if available.
   - Confirm `plugins/enc-amf/CMakeLists.txt` exists.
   - Avoid importing OBS 28+ AMF HW encoder IDs/settings.

3. Reconfigure/build v143. **Done.**
   - Use the VS2022 BuildTools CMake already used by this project.
   - Reconfigure only if CMake does not pick up the newly populated submodule.
   - Build `enc-amf` if available; otherwise build the full `RelWithDebInfo`
     solution.
   - Fix compile errors narrowly inside `plugins/enc-amf` integration.

4. Confirm runtime registration path. **Locally done.**
   - Verify an AMF DLL exists in
     `build-v143/rundir/RelWithDebInfo/obs-plugins/64bit/`.
   - If the plugin does not copy itself to `rundir`, patch its CMake to use the
     local plugin install/copy helper pattern.
   - Do not edit Settings UI dropdown code unless encoder registration succeeds
     but UI still fails to list it.

5. Apply priority decision. **Done.**
   - Change `UI/obspt-bootstrap.cpp::kEncoderPriority` from
     `jim_nvenc -> obs_qsv11 -> ffmpeg_nvenc -> amd_amf_h264 -> obs_x264` to
     `jim_nvenc -> amd_amf_h264 -> obs_qsv11 -> ffmpeg_nvenc -> obs_x264`.
   - Keep NVIDIA first.
   - Keep x264 last.

6. Validate profile data. **Done after AMF safety fixes.**
   - Confirm `apply_encoder_to_profile("...", "amd_amf_h264", cqp)` writes only
     AMF keys documented in `design.md`.
   - Added explicit `QualityPreset=0` (Speed/最大性能) for the AMF branch.
   - AMF CQP values are fixed at 26/26/26 for the PotPvP Profile. Top-level
     `Preset` remains `None=-1` (blank/custom) to avoid template overwrite.
   - Also patched `plugins/enc-amf/Source/amf-h264.cpp`:
     - `SetFrameRate()`: clamp absurd frame rates to 256fps ceiling, fall back
       to 30/1 on `den==0`, use sized `std::vector<char>(32)` + `snprintf`.
     - `SetAspectRatio()`: same `snprintf` buffer fix.
   - Priority order verified: `jim_nvenc -> amd_amf_h264 -> obs_qsv11 ->
     ffmpeg_nvenc -> obs_x264`.

7. Package. **Done.**
   - Refresh `build-v143/_pkg/OBS-PT/` from `rundir/RelWithDebInfo/`.
   - Copy `UI/data/obspt-defaults/` into the staging root.
   - Run:

```powershell
& "C:\Program Files (x86)\NSIS\makensis.exe" "UI\installer\obspt-setup.nsi"
```

8. AMD tester validation. **Partially done; settings follow-up pending.**
   - Install generated installer on the RX 550X test machine.
   - Open Settings -> Output -> Recording and confirm AMD AMF is listed. **Done.**
   - Confirm fresh PotPvP Profile uses `amd_amf_h264` when final priority says it
     should.
     **Done.**
   - Confirm AMF quality preset is maximum performance and QP values are 26.
   - Start/stop a recording and inspect the log for AMF encoder creation and
     normal stop completion.
   - 2026-07-03 follow-up: AMD tester still reports stop-recording not
     responding at 480fps with encoder-overload status. User rejected lowering
     AMD FPS and prefers waiting for drain. Implemented a 30-second recording
     stop watchdog in `OBSBasic`: first stop waits normally; if no
     `RecordingStop` arrives after 30 seconds while recording is still active,
     OBS-PT calls the existing force-stop path automatically.

9. Update specs and finish. **Spec update done; tester validation pending.**
   - Update `.trellis/spec/obspt-bootstrap.md` with the final AMF packaging and
     priority contract.
   - Added the 480fps AMF stop-watchdog contract to
     `.trellis/spec/obspt-bootstrap.md`: keep PotPvP at 480fps, allow normal
     AMF drain first, then force recovery after a 30-second bounded wait.
   - Run `trellis-check`.
   - Commit task work. Archive and record journal after user validation.

## Validation Commands

```powershell
git submodule status plugins/enc-amf
Test-Path plugins/enc-amf/CMakeLists.txt
cmake --build build-v143 --config RelWithDebInfo --target enc-amf
Get-ChildItem build-v143/rundir/RelWithDebInfo/obs-plugins/64bit -Filter '*amf*'
Get-ChildItem build-v143/_pkg/OBS-PT/obs-plugins/64bit -Filter '*amf*'
```

If `enc-amf` is not a generated target name, use:

```powershell
cmake --build build-v143 --config RelWithDebInfo --parallel
```

Package:

```powershell
& "C:\Program Files (x86)\NSIS\makensis.exe" "UI\installer\obspt-setup.nsi"
```

Static checks:

```powershell
rg --line-number 'amd_amf_h264|kEncoderPriority|RateControlMethod|QP\\.IFrame' UI plugins/enc-amf .trellis/spec
```

## Implementation Notes

- Restored `plugins/enc-amf` as the existing `.gitmodules` submodule, pinned to
  OBS 27-era tag `1.4.3.7`.
- Added top-level CMake AMF SDK shim creation: when `BUILD_AMF_ENCODER` is on
  and `DepsPath/include/AMF/components/VideoEncoderVCE.h` exists, configure
  stages the AMF headers into `build-v143/_amf-sdk/amf/public/include` and sets
  `AMDAMF_SDKDir` for the legacy plugin.
- Added `plugins/enc-amf-msvc-compat.hpp` and target-level `/FI` for `enc-amf`
  on MSVC. This parses `<algorithm>` before the legacy plugin defines its
  `clamp` macro, fixing the VS2022/MSVC 14.44 compile failure without editing
  the archived submodule.
- Rebuilt `enc-amf` and `obs`, refreshed `build-v143/_pkg/OBS-PT`, and
  regenerated `build-v143/_pkg/OBS-PT-1.0.0-Installer.exe`.

## Validation Results

```powershell
git submodule status plugins/enc-amf
# +8de4ffd919b2a7958e531045e238f2a907896a33 plugins/enc-amf (1.4.3.7)

cmake --build build-v143 --config RelWithDebInfo --target enc-amf --parallel
# succeeded, produced build-v143/plugins/enc-amf/RelWithDebInfo/enc-amf.dll

cmake --build build-v143 --config RelWithDebInfo --target obs --parallel
# succeeded, rebuilt build-v143/UI/RelWithDebInfo/OBS-PT.exe

Get-ChildItem build-v143/rundir/RelWithDebInfo/obs-plugins/64bit -Filter '*amf*'
# enc-amf.dll and enc-amf.pdb present

Get-ChildItem build-v143/_pkg/OBS-PT/obs-plugins/64bit -Filter '*amf*'
# enc-amf.dll and enc-amf.pdb present

& "C:\Program Files (x86)\NSIS\makensis.exe" "UI\installer\obspt-setup.nsi"
# produced build-v143/_pkg/OBS-PT-1.0.0-Installer.exe

# 2026-07-04 AMD stop-watchdog retest installer:
# - Added a 30-second OBSBasic recording stop watchdog. First stop waits for the
#   normal output/AMF drain path; if no RecordingStop arrives and recording is
#   still active after 30 seconds, OBS-PT calls outputHandler->StopRecording(true).
# - cmake --build build-v143 --config RelWithDebInfo --target obs --parallel
#   succeeded and rebuilt build-v143/UI/RelWithDebInfo/OBS-PT.exe.
# - cmake --install build-v143 --config RelWithDebInfo --prefix
#   K:/Projects/dev/OBS-PT/build-v143/_pkg/OBS-PT succeeded.
# - NSIS regenerated build-v143/_pkg/OBS-PT-1.0.0-Installer.exe.
# - Installer timestamp: 2026-07-04 11:35:19 local time.
# - Installer size: 43,382,801 bytes.
# - SHA256:
#   B855AD11D7DF5390A693444EE9740529322D696579D003E001A3BAC023D767BC
# - 7z t passed: Type=Nsis, Files=1463, Size=213707474, Everything is Ok.

# 2026-07-02 NSIS integrity retest after tester reported "Installer integrity check has failed":
# - Rebuilt enc-amf and obs targets.
# - Recreated build-v143/_pkg/OBS-PT via cmake --install.
# - Regenerated build-v143/_pkg/OBS-PT-1.0.0-Installer.exe.
# - SHA256:
#   9C7EE4364D1667F8F7557A6AEC7080E2E7452A7B67534DFA8BE000944F43CADE
# - 7z t passed: Type=Nsis, Files=1463, Size=213706450, Everything is Ok.
# - Silent install smoke reached required-file checks and uninstall; existing
#   D:\OBS-PT HKCU uninstall metadata and shortcuts were restored afterward.
```

## Risky Files

- `plugins/enc-amf/`: archived legacy encoder plugin; may need integration fixes.
- `.gitmodules` / submodule state: must remain reproducible.
- `plugins/CMakeLists.txt`: should not need broad changes.
- `UI/obspt-bootstrap.cpp`: priority order only; AMF profile writer already
  exists.
- `build-v143/_pkg/OBS-PT`: generated staging, not committed.

## Rollback

- If AMF cannot compile within reasonable scope, revert submodule population and
  record AMD hardware encoding as unsupported for this release.
- If AMF compiles but fails to register on tester hardware, keep the plugin
  build but do not prioritize AMF until logs identify driver/runtime blockers.
- If priority reorder causes bad mixed-GPU behavior, revert only
  `kEncoderPriority` and keep the plugin packaged.
