# modernize NVENC presets implementation plan

## Checklist

1. Confirm the task is reviewed and started.
   - Wait for user approval of `prd.md`, `design.md`, and this file.
   - Run `python ./.trellis/scripts/task.py start 06-22-modernize-nvenc-presets`.
   - Load `trellis-before-dev` before editing code.

2. Import the OBS 28.1.0 NVENC SDK surface.
   - Replace `plugins/obs-ffmpeg/external/nvEncodeAPI.h` with the OBS 28.1.0
     version.
   - Add `plugins/obs-ffmpeg/jim-nvenc-ver.h` from OBS 28.1.0.
   - Add the new header to `plugins/obs-ffmpeg/CMakeLists.txt` if needed.
   - Update `plugins/obs-ffmpeg/jim-nvenc.h` to include `jim-nvenc-ver.h`.

3. Update NVENC helper compatibility.
   - Backport the OBS 28.1.0 helper version-check behavior in
     `plugins/obs-ffmpeg/jim-nvenc-helpers.c`.
   - Keep existing OBS-PT error text behavior unless the 28.1.0 error helper is
     required by the SDK integration.
   - Ensure driver support is checked against `NVENC_COMPAT_MAJOR_VER` /
     `NVENC_COMPAT_MINOR_VER`, not raw NVENC API 12.0.

4. Modernize `jim_nvenc`.
   - Add P1-P7, tuning, and multipass mapping helpers to
     `plugins/obs-ffmpeg/jim-nvenc.c`.
   - Read `preset2`, `tune`, and `multipass`.
   - Preserve H.264 legacy migration when `preset` exists and `preset2` does
     not.
   - Use `nvEncGetEncodePresetConfigEx`.
   - Set `params->tuningInfo`.
   - Set `config->rcParams.multiPass`.
   - Update log output to include modern preset, tuning, and multipass values.
   - Keep the task H.264-only; do not register HEVC/AV1.

5. Modernize `ffmpeg_nvenc` properties and defaults.
   - In `plugins/obs-ffmpeg/obs-ffmpeg-nvenc.c`, default to the current
     OBS-PT PotPvP baseline where applicable:
     `preset2:"p1"`, `multipass:"disabled"`, `tune:"hq"`,
     `psycho_aq:false`, `bf:0`, `profile:"high"`.
   - Add P1-P7, Tuning, and Multipass UI properties.
   - Keep old-profile fallback when `preset` exists and `preset2` does not.
   - Forward `preset2`, `tune`, and `multipass` to FFmpeg NVENC for modern
     profiles.

6. Update localization.
   - Update `plugins/obs-ffmpeg/data/locale/en-US.ini`.
   - Update `plugins/obs-ffmpeg/data/locale/zh-CN.ini`.
   - Use 32.1.2 `obs-ffmpeg` labels for `NVENC.Preset2.p1` through
     `NVENC.Preset2.p7` where available.
   - Include `Tuning` plus `NVENC.Tuning.*` and `NVENC.Multipass.*`.

7. Update OBS-PT defaults.
   - Update
     `UI/data/obspt-defaults/obs-studio/basic/profiles/PotPvP/recordEncoder.json`
     to modern `jim_nvenc` keys.
   - Update `UI/obspt-bootstrap.cpp::apply_encoder_to_profile()` so generated
     `jim_nvenc` profiles match the shipped JSON.
   - Update the `ffmpeg_nvenc` branch if it remains a bootstrap fallback.
   - Keep CQP value controlled by the existing bootstrap `cqp` argument.

8. Build and fix compile errors.
   - Use the VS2022 BuildTools bundled CMake, not system CMake 4.x.
   - First build `obs-ffmpeg`-affected targets through the normal full build
     command.
   - If SDK 12 struct-version errors appear, compare against OBS 28.1.0
     compatibility macros before making local deviations.

9. Validate defaults and packaging.
   - Confirm source default JSON contains `preset2:"p1"`, `tune:"hq"`,
     `multipass:"disabled"`, `bf:0`, and `psycho_aq:false`.
   - Confirm no shipped `jim_nvenc` default contains `preset:"hp"`.
   - Regenerate the installer.
   - Smoke a silent install and inspect installed default profile JSON.

10. Update specs and finish checks.
    - Update `.trellis/spec/obspt-bootstrap.md` with the modern NVENC bootstrap
      default contract.
    - Run `trellis-check`.
    - Commit only files touched for this task after validation.

## Validation Commands

Build:

```powershell
& "K:\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-v143 --config RelWithDebInfo --parallel
```

Package:

```powershell
& "C:\Program Files (x86)\NSIS\makensis.exe" "UI\installer\obspt-setup.nsi"
```

Static checks after implementation:

```powershell
rg --line-number 'preset": "hp"|preset\\":\\"hp|nvEncGetEncodePresetConfig\\(' UI plugins/obs-ffmpeg
rg --line-number 'preset2|multipass|tune' plugins/obs-ffmpeg UI/data/obspt-defaults UI/obspt-bootstrap.cpp
```

Expected installer output:

```text
build-v143/_pkg/OBS-PT-1.0.0-Installer.exe
```

## Smoke Plan

1. Install to a clean temp directory with the generated installer.
2. Inspect installed
   `obs-studio/basic/profiles/PotPvP/recordEncoder.json`.
3. Verify the installed JSON has:
   - `encoder:"jim_nvenc"`
   - `rate_control:"CQP"`
   - `preset2:"p1"`
   - `tune:"hq"`
   - `multipass:"disabled"`
   - `profile:"high"`
   - `bf:0`
   - `psycho_aq:false`
4. Launch OBS-PT once and verify bootstrap does not rewrite the encoder back to
   legacy `preset`.
5. On an NVIDIA 610.47 machine, start recording and inspect the log:
   - modern settings include preset/tuning/multipass;
   - no failure from legacy `nvEncGetEncodePresetConfig`;
   - no `NV_ENC_ERR_UNSUPPORTED_PARAM` during encoder initialization.

## Risky Files

- `plugins/obs-ffmpeg/external/nvEncodeAPI.h`: large SDK header replacement.
- `plugins/obs-ffmpeg/jim-nvenc.c`: runtime NVENC initialization path.
- `plugins/obs-ffmpeg/jim-nvenc-helpers.c`: driver compatibility gate.
- `plugins/obs-ffmpeg/obs-ffmpeg-nvenc.c`: shared property/default surface for
  `jim_nvenc` and `ffmpeg_nvenc`.
- `UI/obspt-bootstrap.cpp`: first-run/default profile generation.

## Rollback Points

- Header/helper rollback: restore `nvEncodeAPI.h`, remove
  `jim-nvenc-ver.h`, and revert `jim-nvenc.h`/helper compatibility edits.
- `jim_nvenc` rollback: revert `jim-nvenc.c` while leaving planning docs.
- FFmpeg fallback rollback: keep `jim_nvenc` modernized but revert
  `ffmpeg_nvenc` option forwarding if bundled FFmpeg rejects modern options.
- Default rollback: restore old JSON/bootstrap only if encoder modernization is
  explicitly not shipping in this installer.
