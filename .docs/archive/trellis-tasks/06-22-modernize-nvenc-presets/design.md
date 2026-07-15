# modernize NVENC presets design

## Direction

Use OBS 28.1.0 as the implementation source for the existing
`plugins/obs-ffmpeg` NVENC path. Do not migrate to the OBS 32.1.2
`plugins/obs-nvenc` architecture in this task.

OBS 32.1.2 remains the reference for user-facing labels. The shipped/default
PotPvP Profile now follows the user's current OBS-PT config:
`preset2:"p1"`, `multipass:"disabled"`, `tune:"hq"`, `profile:"high"`,
`psycho_aq:false`, and `bf:0`.

## Boundaries

In scope:

- Modernize `jim_nvenc` H.264 preset handling inside
  `plugins/obs-ffmpeg/jim-nvenc.c`.
- Update NVENC SDK declarations under
  `plugins/obs-ffmpeg/external/nvEncodeAPI.h` enough for P1-P7, tuning,
  multipass, and `nvEncGetEncodePresetConfigEx`.
- Carry the OBS 28.1.0 NVENC compatibility version pattern needed after the SDK
  header moves from NVENC API 8.1 to 12.0.
- Update `ffmpeg_nvenc` properties/defaults and option forwarding in
  `plugins/obs-ffmpeg/obs-ffmpeg-nvenc.c` so fallback settings use the same
  modern schema when no legacy-only profile is being migrated.
- Update `en-US` and `zh-CN` locale strings for P1-P7, Tuning, and Multipass.
- Update OBS-PT shipped defaults and bootstrap-generated
  `recordEncoder.json`.
- Record OBS-PT bootstrap default knowledge in `.trellis/spec/obspt-bootstrap.md`
  after implementation.

Out of scope:

- New OBS 32.1.2 encoder ids such as `obs_nvenc_h264_tex`.
- AV1/HEVC encoder registration in this fork.
- UHQ tuning from newer `plugins/obs-nvenc`; the existing OBS 28.1.0-style
  `obs-ffmpeg` path exposes only `hq`, `ll`, and `ull`.
- Product-specific quality retuning beyond adopting the high-version default
  baseline.

## Current Problem

The current `jim_nvenc` path reads only `preset`, maps legacy values such as
`hp`, `hq`, and `mq` to legacy preset GUIDs, and calls
`nvEncGetEncodePresetConfig`. Driver 610.47 reports
`NV_ENC_ERR_UNSUPPORTED_PARAM` on that path.

Changing only JSON defaults or UI labels is insufficient because the current
SDK header lacks:

- `NV_ENC_PRESET_P1_GUID` through `NV_ENC_PRESET_P7_GUID`
- `NV_ENC_TUNING_INFO`
- `NV_ENC_MULTI_PASS`
- `NvEncGetEncodePresetConfigEx`
- the extended function-list field for `nvEncGetEncodePresetConfigEx`

## Encoder Contract

`jim_nvenc` will accept the modern settings:

```json
{
  "preset2": "p5",
  "tune": "hq",
  "multipass": "qres"
}
```

Runtime mapping follows OBS 28.1.0:

- `preset2:p1..p7` maps to `NV_ENC_PRESET_P1_GUID` through
  `NV_ENC_PRESET_P7_GUID`; invalid or missing values fall back to P5.
- `tune:hq` maps to `NV_ENC_TUNING_INFO_HIGH_QUALITY`.
- `tune:ll` maps to `NV_ENC_TUNING_INFO_LOW_LATENCY`.
- `tune:ull` maps to `NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY`.
- `multipass:disabled` maps to `NV_ENC_MULTI_PASS_DISABLED`.
- `multipass:qres` maps to `NV_ENC_TWO_PASS_QUARTER_RESOLUTION`.
- `multipass:fullres` maps to `NV_ENC_TWO_PASS_FULL_RESOLUTION`.

The preset config call changes to:

```c
nv.nvEncGetEncodePresetConfigEx(enc->session, NV_ENC_CODEC_H264_GUID,
                                nv_preset, nv_tuning, &preset_config);
```

`NV_ENC_INITIALIZE_PARAMS` must also receive `params->tuningInfo = nv_tuning`,
and `config->rcParams.multiPass` must receive the selected multipass mode.

## Legacy Profile Migration

Existing profiles can contain a user `preset` value but no `preset2`. In that
case, preserve OBS 28.1.0 H.264 migration behavior:

| Legacy preset | Modern preset | Tuning | Multipass |
| --- | --- | --- | --- |
| `mq` | `p5` | `hq` | `qres` |
| `hq` | `p5` | `hq` | `disabled` |
| `default` | `p3` | `hq` | `disabled` |
| `hp` | `p1` | `hq` | `disabled` |
| `ll` | `p3` | `ll` | `disabled` |
| `llhq` | `p4` | `ll` | `disabled` |
| `llhp` | `p2` | `ll` | `disabled` |

For OBS-PT shipped and bootstrap defaults, stop writing the legacy `preset`
key for `jim_nvenc`.

## SDK/Header Strategy

Prefer importing the OBS 28.1.0 `nvEncodeAPI.h` rather than hand-patching the
current NVENC 8.1 header. The required symbols are spread across GUIDs, enums,
struct fields, function declarations, typedefs, and function-list layout.

Because the imported header is NVENC API 12.0, also carry the OBS 28.1.0
compatibility pattern:

- add `jim-nvenc-ver.h` with `NVENC_COMPAT_MAJOR_VER 11` and
  `NVENC_COMPAT_MINOR_VER 1`;
- include it from `jim-nvenc.h`;
- make the helper driver-version check compare the driver against the compat
  version rather than the raw 12.0 header version;
- use compatible struct versions for non-AV1 H.264 paths where OBS 28.1.0 does
  so (`NV_ENC_CONFIG_COMPAT_VER`, `NV_ENC_PIC_PARAMS_COMPAT_VER`,
  `NV_ENC_LOCK_BITSTREAM_COMPAT_VER`, and
  `NV_ENC_REGISTER_RESOURCE_COMPAT_VER`).

This keeps the implementation aligned with upstream and avoids a fragile custom
SDK hybrid.

## Properties and Defaults

`plugins/obs-ffmpeg/obs-ffmpeg-nvenc.c` should expose:

- `preset2` list: `p1` through `p7`
- `tune` list: `hq`, `ll`, `ull`
- `multipass` list: `disabled`, `qres`, `fullres`

Default values for both `jim_nvenc` and `ffmpeg_nvenc` should match the current
OBS-PT PotPvP Profile baseline:

```json
{
  "rate_control": "CQP",
  "cqp": 26,
  "preset2": "p1",
  "tune": "hq",
  "multipass": "disabled",
  "profile": "high",
  "bf": 0,
  "psycho_aq": false
}
```

`ffmpeg_nvenc` keeps the legacy fallback behavior when a profile has user
`preset` and no `preset2`; otherwise it forwards `preset2`, `tune`, and
`multipass` to FFmpeg NVENC options as OBS 28.1.0/32.1.2 do.

## Localization

Use `NVENC.Preset2.*` keys for P1-P7 in `obs-ffmpeg`, matching OBS 28/32
`obs-ffmpeg` convention:

- `NVENC.Preset2.p1` through `NVENC.Preset2.p7`
- `Tuning`
- `NVENC.Tuning.hq`, `NVENC.Tuning.ll`, `NVENC.Tuning.ull`
- `NVENC.Multipass`
- `NVENC.Multipass.disabled`, `NVENC.Multipass.qres`,
  `NVENC.Multipass.fullres`

The old `NVENC.Preset.*` translations may remain for compatibility, but the
new property list must not use them for the modern UI.

## Packaging and Smoke

After implementation:

- build with the VS2022 BuildTools bundled CMake;
- ensure staged defaults contain modern keys and no `preset:"hp"` for
  `jim_nvenc`;
- regenerate the NSIS installer;
- run silent install smoke and inspect installed defaults;
- on an NVIDIA machine, verify the log shows `preset`, `tuning`, and
  `multipass` and no longer reaches the legacy
  `nvEncGetEncodePresetConfig` call for `jim_nvenc`.

## Rollback

Rollback can happen at these boundaries:

- revert `nvEncodeAPI.h`, `jim-nvenc-ver.h`, `jim-nvenc.h`, and
  `jim-nvenc-helpers.c` together if SDK integration fails at compile time;
- revert `jim-nvenc.c` independently if SDK integration builds but runtime
  initialization fails;
- revert `obs-ffmpeg-nvenc.c` if bundled FFmpeg rejects modern options while
  keeping `jim_nvenc` modernized;
- revert OBS-PT default JSON/bootstrap changes if encoder changes are not ready
  to ship.
