# Design: AMD AMF encoder selection

## Summary

The reported UI state is caused by a missing encoder plugin, not by Settings UI
filtering. `amd_amf_h264` is already in the bootstrap priority list and AMF
profile writer, but the `plugins/enc-amf` submodule is empty, so no encoder
type is registered and the dropdown cannot show AMD AMF.

This task restores the OBS 27-era legacy `enc-amf` plugin and packages its
runtime DLL so compatible AMD machines can enumerate and select
`amd_amf_h264`.

## Architecture

### Plugin Source

- Use the existing `.gitmodules` entry:
  `plugins/enc-amf -> https://github.com/obsproject/obs-amd-encoder.git`.
- Use an OBS 27-era compatible revision/tag rather than OBS 28+ AMF HW work.
  Prior research references `1.4.3.7` as OBS 27-era schema-compatible.
- Keep this as a submodule/vendor dependency; do not hand-roll AMF encoder logic
  inside OBS-PT.

### Build Integration

`plugins/CMakeLists.txt` already gates AMF correctly:

```cmake
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/enc-amf/CMakeLists.txt")
  add_subdirectory(enc-amf)
else()
  message(STATUS "enc-amf submodule not found! ...")
endif()
```

Once `plugins/enc-amf/CMakeLists.txt` exists, the expected behavior is:

- v143 configure includes `enc-amf`;
- `cmake --build build-v143 --config RelWithDebInfo --target enc-amf` or a full
  build produces an AMF plugin DLL;
- `install_obs_plugin()` or the plugin's equivalent post-build copy places the
  DLL under `build-v143/rundir/RelWithDebInfo/obs-plugins/64bit/`;
- package staging copies the DLL into `build-v143/_pkg/OBS-PT/obs-plugins/64bit/`.

If the legacy plugin CMake is not directly compatible with the current
top-level helper functions, patch the plugin CMake narrowly rather than changing
project-wide plugin conventions.

### Runtime Selection

The UI dropdown comes from `obs_enum_encoder_types()`, so no UI special-case is
needed. Once AMF registers, Settings -> Output -> Recording should include the
AMF encoder display name.

Before this task, bootstrap probed:

```cpp
static const char *kEncoderPriority[] = {
  "jim_nvenc", "obs_qsv11", "ffmpeg_nvenc", "amd_amf_h264", "obs_x264", nullptr,
};
```

This means an Intel CPU + AMD GPU machine can still select QSV before AMF if QSV
registers. The user's screenshot shows QuickSync is available on a Radeon RX
550X machine. Product decision: AMD GPU users should get AMF, so reorder the
priority to put `amd_amf_h264` before `obs_qsv11`/`ffmpeg_nvenc`:

```cpp
"jim_nvenc", "amd_amf_h264", "obs_qsv11", "ffmpeg_nvenc", "obs_x264"
```

This preserves NVIDIA first, makes AMD discrete GPUs win over Intel iGPU QSV,
and keeps x264 as the final fallback.

### Profile Contract

`apply_encoder_to_profile()` already writes AMF-compatible OBS 27 keys:

- `Usage:0`
- `QualityPreset:0`
- `Profile:100`
- `RateControlMethod:0`
- `QP.IFrame/QP.PFrame/QP.BFrame = cqp`
- `VBVBuffer:1`
- `VBVBuffer.Size:100000`
- `KeyframeInterval:2.0`
- `BFrame.Pattern:0`

`QualityPreset=0` maps to `H264QualityPreset::Speed`, which is the maximum
performance option in the legacy AMF plugin. Do not use the plugin's top-level
`HighQuality` preset for OBS-PT defaults: that template writes QP as 26/24/22,
while the OBS-PT AMD target is 26/26/26. Keep using bare OBS 27 keys, not
`AMF.H264.*` dotted keys.

The top-level AMF `Preset` field is a UI template selector. Its `None=-1` state
renders as a blank entry and means custom settings. Setting it to a named
template can cause the plugin's `properties_modified()` path to overwrite the
explicit OBS-PT CQP values, so the safer contract is to write explicit encoder
settings and leave the top-level template unset unless product requirements
decide that a non-empty UI field is worth the template side effects.

### Packaging

After build validation:

1. Refresh `build-v143/_pkg/OBS-PT/` from `build-v143/rundir/RelWithDebInfo/`.
2. Copy `UI/data/obspt-defaults/` into the staging root.
3. Run `UI/installer/obspt-setup.nsi` via NSIS.
4. Verify the generated installer timestamp and that the staged AMF DLL exists.

This follows the runtime bugfix installer refresh contract added to
`.trellis/spec/obspt-bootstrap.md`.

## Validation

Local machine may not have AMD hardware. Local validation must still prove:

- `plugins/enc-amf` is populated at the intended compatible revision.
- CMake config/build includes `enc-amf`.
- The AMF DLL exists in `rundir`.
- The AMF DLL exists in `_pkg/OBS-PT`.
- No regressions to existing NVENC/QSV/x264 default files.
- `kEncoderPriority` is `jim_nvenc -> amd_amf_h264 -> obs_qsv11 ->
  ffmpeg_nvenc -> obs_x264`.
- New installer is generated.

AMD tester validation must prove:

- Settings -> Output -> Recording shows AMD AMF.
- Fresh install/repair writes `AdvOut.RecEncoder=amd_amf_h264` when AMF is the
  highest-priority available encoder after the final priority decision.
- `recordEncoder.json` uses AMF keys only, including `QualityPreset=0` and
  QP 26 for I/P/B frames.
- A recording starts and stops successfully.

## Risks

- The archived `obs-amd-encoder` source may need CMake or SDK compatibility
  patches for VS2022/v143.
- AMF runtime registration can fail on old AMD drivers or unsupported GPUs even
  when the DLL loads. The UI should reflect registration, not merely file
  presence.
- Reordering encoder priority affects Intel CPU + AMD GPU systems by selecting
  the AMD GPU encoder over Intel QSV. This is intentional for OBS-PT.
- Do not backport OBS 28+ `h264_texture_amf` in this task; it has different IDs
  and settings schema.
