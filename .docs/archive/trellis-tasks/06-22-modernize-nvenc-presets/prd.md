# modernize NVENC presets

## Goal

Replace OBS-PT's legacy NVIDIA NVENC preset path with the modern P1-P7 preset
model, add the matching Tuning and Multipass controls, and update default
recording settings so NVIDIA driver 610.47 no longer fails recording with
`nvEncGetEncodePresetConfig failed: 12 (NV_ENC_ERR_UNSUPPORTED_PARAM)`.

## User Value

Users on newer NVIDIA drivers/control-panel generations can record without
falling into the old legacy preset incompatibility, while the settings UI
matches modern OBS terminology and exposes the same tuning/multipass choices
shown in current OBS.

## Confirmed Facts

- Current OBS-PT still uses OBS 27-era `plugins/obs-ffmpeg/jim-nvenc.c`:
  - Reads only `preset`.
  - Maps legacy values `mq/hq/default/hp/ll/llhq/llhp` to legacy NVENC preset
    GUIDs.
  - Calls `nvEncGetEncodePresetConfig(...)`, which is where the 610.47 smoke
    failure reports `NV_ENC_ERR_UNSUPPORTED_PARAM`.
- Current OBS-PT `plugins/obs-ffmpeg/obs-ffmpeg-nvenc.c` exposes only the old
  preset list and defaults `preset:"hq"` for encoder defaults.
- Current OBS-PT shipped/bootstrap recording defaults deliberately write
  `jim_nvenc`, `rate_control:"CQP"`, `cqp:26`, `preset:"hp"`, `profile:"high"`,
  `bf:0`, `psycho_aq:false`; this is now suspected to be the direct trigger for
  driver 610.47.
- Current OBS-PT `plugins/obs-ffmpeg/external/nvEncodeAPI.h` does not define
  `NV_ENC_PRESET_P1_GUID`, `NV_ENC_TUNING_INFO`, `NV_ENC_MULTI_PASS`, or
  `NvEncGetEncodePresetConfigEx`; the task must update or minimally backport the
  SDK header/function table, not only UI/default JSON.
- OBS 28.1.0 is the first relevant upstream reference:
  - `plugins/obs-ffmpeg/jim-nvenc.c` reads `preset2`, `tune`, and `multipass`.
  - Maps `p1`..`p7` to `NV_ENC_PRESET_P*_GUID` with fallback to `p5`.
  - Maps tuning `hq/ll/ull` and multipass `disabled/qres/fullres`.
  - Calls the Ex-style modern preset path and sets `params->tuningInfo`.
  - Preserves migration mapping from old legacy `preset` values to modern
    preset/tuning/multipass when `preset2` is absent.
- OBS 28.1.0 `obs-ffmpeg-nvenc.c` defaults `preset2:"p6"`,
  `multipass:"qres"`, `tune:"hq"`, `profile:"high"`, `psycho_aq:true`, `bf:2`.
- OBS 32.1.2 latest NVENC has moved to `plugins/obs-nvenc`; its legacy
  `jim_nvenc` is a compat encoder. For compat/default behavior it uses
  `preset2:"p5"`, `multipass:"qres"`, `tune:"hq"`, `profile:"high"`,
  `psycho_aq:true`, `bf:2`.
- OBS 32.1.2 UI labels include:
  - `Preset.p1`..`Preset.p7`
  - `Tuning` with `hq/ll/ull` (plus `uhq` only in the new plugin when supported)
  - `Multipass` with `disabled/qres/fullres`
- User explicitly wants:
  - Replace old Chinese preset choices "最高质量 / 质量 / 性能 / 最大性能 /
    低延迟质量 / 低延迟 / 低延迟性能" with modern `p1`-`p7`.
  - Add "调节" and "多次编码模式".
  - Use high-version default settings for now, tune later.
  - Use OBS 28.1.0 as first-feature reference and OBS 32.1.2 as latest reference.
- 2026-06-23 user updated shipping defaults to the current PotPvP Profile:
  `preset2:"p1"`, `tune:"hq"`, `multipass:"disabled"`, `bf:0`,
  `psycho_aq:false`, and disabled the upstream auto-configuration wizard.
- Scope decision: backport the minimal OBS 28.1.0-style modern `jim_nvenc`
  behavior into the existing `plugins/obs-ffmpeg` plugin. Do not adopt the
  larger OBS 32.1.2 `plugins/obs-nvenc` architecture in this task.

## Requirements

- **R1 — Modern NVENC API path**: `jim_nvenc` must accept and use `preset2`,
  `tune`, and `multipass`, mapping them to modern NVENC SDK values and using the
  modern preset config call.
- **R2 — UI controls**: Advanced Output encoder properties must show P1-P7
  presets and add Tuning + Multipass controls for NVENC instead of the old
  legacy preset list.
- **R3 — Defaults**: OBS-PT shipped defaults and first-run/bootstrap-generated
  `recordEncoder.json` for `jim_nvenc` must use modern keys and high-version
  defaults.
- **R4 — Migration compatibility**: Existing profiles containing old `preset`
  without `preset2` should migrate/behave compatibly instead of crashing or
  silently selecting a nonsensical preset.
- **R5 — FFmpeg NVENC consistency**: If `ffmpeg_nvenc` is still a fallback
  encoder, its settings should remain compatible with the same modern
  `preset2/tune/multipass` schema where the bundled FFmpeg encoder expects it.
- **R6 — Localization**: At minimum zh-CN and en-US labels must be present for
  P1-P7, Tuning, and Multipass. Other locales may fall back to keys unless
  upstream strings can be copied mechanically.
- **R7 — Packaging**: After implementation, rebuild `OBS-PT.exe`, restage
  defaults, regenerate the installer, and run the same silent smoke checks used
  by the installer task.
- **R8 — Profile menu defaults**: Profile > New must seed a clean Profile from
  the OBS-PT PotPvP Profile defaults instead of upstream official OBS blank
  defaults, without launching the auto-configuration wizard.

## Acceptance Criteria

- [ ] Settings UI for `jim_nvenc` no longer exposes old preset labels and exposes
      `p1` through `p7`, Tuning, and Multipass.
- [ ] Default installed `recordEncoder.json` for `jim_nvenc` uses modern keys and
      contains no legacy-only `preset:"hp"` default.
- [ ] First-run/bootstrap rewrite uses the same modern `jim_nvenc` defaults as
      the shipped JSON.
- [ ] Encoding log prints modern preset/tuning/multipass values.
- [ ] A fresh install on an NVIDIA system no longer reaches the legacy
      `nvEncGetEncodePresetConfig` path for `jim_nvenc`.
- [ ] Build succeeds with VS2022 v143.
- [ ] Installer is regenerated and silent install smoke verifies modern NVENC
      defaults in the installed tree.
- [ ] Profile > New creates a Profile seeded from OBS-PT PotPvP defaults, and
      the upstream auto-configuration wizard is not launched or exposed from
      Tools.

## Likely Out of Scope

- Full migration to OBS 32.1.2's new `plugins/obs-nvenc` plugin and new encoder
  ids such as `obs_nvenc_h264_tex`.
- AV1/HEVC NVENC support.
- Final tuning of OBS-PT's product-specific quality/performance defaults beyond
  adopting high-version OBS defaults for this bug fix.
