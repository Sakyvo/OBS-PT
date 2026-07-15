# Design: AMD AMF defaults and primary-display bootstrap

## Summary

This task fixes two independent symptoms with one fresh-install contract:

1. Restore genuine first-run state in the packaged Global Config so the
   existing early/late bootstrap actually executes.
2. Narrow the legacy AMF property surface to one non-destructive `OBS-PT`
   top-level preset and make the bootstrap-written AMD quality values match the
   approved resolution rule.

AMF source ownership moves to a public `Sakyvo/obs-amd-encoder` fork because
the current parent gitlink already targets an OBS-PT-only commit that cannot be
fetched from the archived upstream repository.

## Ownership and Boundaries

### OBS-PT parent repository

- `.gitmodules`: point `plugins/enc-amf` to the public Sakyvo fork.
- `UI/data/obspt-defaults/obs-studio/global.ini`: ship incomplete first-run
  state while retaining the approved GUI geometry and DockState.
- `UI/obspt-bootstrap.cpp`: select encoder-specific quality by Base height and
  write the explicit AMF settings blob.
- `.trellis/spec/obspt-bootstrap.md`: update the fresh-bootstrap and AMF source
  ownership contracts after implementation.

### AMF fork/submodule

- `plugins/enc-amf/Include/enc-h264.h`: add the `Preset.OBSPT` locale key.
- `plugins/enc-amf/Source/enc-h264.cpp`: expose only `Preset=None (-1)` under
  the visible name `OBS-PT`.
- `plugins/enc-amf/Resources/locale/{en-US,zh-CN,zh-TW,ja-JP,ko-KR}.ini`: add
  the exact `OBS-PT` display value.

The upstream template implementation remains in source for history and
defensive compatibility, but no named template is reachable from the OBS-PT
AMF dropdown. Replay Buffer and modern OBS AMF encoders remain untouched.

## Source Publication

Create the public fork from `obsproject/obs-amd-encoder`, retain upstream
history, and create an `obs-pt` branch containing existing custom commit
`0cebebc` plus this task's AMF UI commit. Update the parent `.gitmodules` URL to:

```text
https://github.com/Sakyvo/obs-amd-encoder.git
```

The parent gitlink must identify a commit reachable from a fork branch. A final
clean-clone check uses `git ls-remote` and recursive submodule checkout. GitHub's
generated source archive retains normal gitlink behavior; buildable consumers
must clone with submodules, consistent with the rest of the OBS source tree.

Do not force-push or rewrite the archived upstream ancestry. The current task
stays uncommitted during implementation review; the submodule commit/push and
parent gitlink commit occur during the authorized finish phase.

## Fresh-Install State

The packaged Global Config currently carries runtime completion state copied
from a live install. Replace only these values:

```ini
[OBSPT]
BootstrapVersion=0
FirstRunCompleted=false
```

Keep all approved `[BasicWindow]` geometry/DockState and unrelated defaults.
On first launch:

```text
prepare_obspt_global_config
  -> completed=false => needs_repair=true
run_obspt_early_bootstrap
  -> Windows/Qt primary screen physical pixels
  -> BaseCX/BaseCY=OutputCX/OutputCY, FPS=480/1
OBSBasic::OBSInit / module load
run_first_run_bootstrap_if_needed
  -> probe amd_amf_h264
  -> set AdvOut.RecEncoder=amd_amf_h264
  -> write AMF recordEncoder.json + recording path
  -> request welcome dialog
welcome finished
  -> active Global Config marks completed=true/version=current
```

Writing the current bootstrap version during the early phase is safe because
`FirstRunCompleted=false` independently keeps the late phase eligible after a
crash/restart. No version bump or existing-install migration is required.

## Primary-Display Resolution

Keep the existing `QGuiApplication::primaryScreen()` path. Multiply logical
screen size by `devicePixelRatio()` to obtain physical pixels, write Base and
Output identically, and retain the no-cap 480 FPS policy. On multi-display
systems this follows the Windows primary display even if another display has a
higher resolution. Null/invalid screens keep the existing 1920x1080 fallback.

The reported 2K failure does not require a new monitor API: the packaged
completion marker prevented the existing function from running.

## Encoder Quality Mapping

Make the value passed to `apply_encoder_to_profile()` encoder-specific:

| Encoder | `BaseCY < 1080` | `BaseCY >= 1080` or unknown |
|---|---:|---:|
| `amd_amf_h264` | 22 | 27 |
| Existing NVENC/QSV/x264 paths | 20 | 26 |

Use a small local helper to keep the threshold and vendor mapping explicit.
Unknown/zero height follows the high-resolution fallback, matching current
behavior.

The AMF writer uses the selected value for all three QP keys and explicitly
writes the complete intended state:

```json
{
  "encoder": "amd_amf_h264",
  "AMF.H264.Preset": -1,
  "AMF.H264.Usage": 0,
  "AMF.H264.QualityPreset": 0,
  "AMF.H264.Profile": 100,
  "AMF.H264.RateControlMethod": 0,
  "AMF.H264.QP.IFrame": 27,
  "AMF.H264.QP.PFrame": 27,
  "AMF.H264.QP.BFrame": 27,
  "AMF.H264.VBVBuffer": 1,
  "AMF.H264.VBVBuffer.Size": 100000,
  "AMF.H264.KeyframeInterval": 2.0,
  "AMF.H264.BFrame.Pattern": 0
}
```

At sub-1080 height only the three QP values become 22. Other encoder templates
and their 20/26 rule are unchanged. The `AMF.H264.` prefix is part of the
plugin's serialized property ID, not a display/translation prefix; unprefixed
fields remain unknown data and do not override plugin defaults.

## AMF Property UI

The top-level `Preset` and `Quality Preset` controls are distinct. The former
currently applies large setting templates; the latter maps directly to AMF
Speed/Balanced/Quality.

Change only the top-level list population:

```cpp
obs_property_list_add_int(p, TEXT_T(AMF_H264_PRESET_OBSPT), Presets::None);
```

Do not add Reset, Recording, High Quality, Indistinguishable, Lossless, Twitch,
or YouTube items. Keep the modified callback; with `None=-1`, it leaves fields
enabled and does not rewrite CQP. Keep the separate Quality Preset list and
default it through `recordEncoder.json` to Speed (`0`).

## AMF Automatic Profile Level

The legacy `GetMinimumProfileLevel()` table ends at H.264 Level 5.2. Its old
terminator cast `-1` into `H264ProfileLevel`, whose underlying type is
`uint8_t`; the loop compared the resulting 255 with integer `-1` and therefore
never terminated. At 2560x1440 and 240/256 FPS no table row matches, so the
loop read beyond the array and passed an arbitrary value such as 249 to AMF.

Remove the sentinel and iterate over the bounded array. Compute frame samples
and samples per second with 64-bit arithmetic, treating an invalid zero
denominator or arithmetic overflow as above-range. Return Level 5.2 when no row
matches, preserving the function's intended fallback and the approved high-FPS
policy. A focused executable test covers ordinary level selection, the exact
Level 5.2 boundary, 2560x1440 at 240/256 FPS, and a zero denominator.

## Validation

- Static: exact Global Config markers, primary-screen path, quality mapping,
  `AdvOut.RecEncoder` assignment, complete prefixed AMF key set with no
  unprefixed aliases, and exactly one top-level Preset list item.
- Source: fork exists publicly; current and final gitlinks are remotely
  reachable; clean recursive checkout succeeds.
- Build: `enc-amf` and `obs` build; built/staged EXE, AMF DLL, and supported
  locale files match.
- Local fresh-run smoke: on a disposable portable root, verify monitor log,
  Base=Output=primary physical pixels, 480/1 FPS, welcome flow, and marker
  completion. Local NVIDIA hardware cannot prove AMF registration/UI.
- AMD tester: 2560x1440 first launch, AMF selected, one `OBS-PT` Preset item,
  Speed/High/CQP/QP 27, editable fields, and successful recording start/stop.
- Package: one non-overlapping `makensis` run, `7z t`, source/Desktop SHA-256
  equality, and actual Setup-window launch for both copies.

## Risks and Rollback

- Local hardware cannot validate AMF property enumeration. Do not infer AMD
  runtime success from DLL presence; retain tester acceptance.
- Resetting completion state in the packaged file intentionally affects fresh
  installs only. Installer migration/overwrite behavior is out of scope.
- A fork without the final commit reachable leaves the parent source broken;
  do not finish until the recursive-clone check passes.
- Rollback restores the previous Global Config markers, AMF list population,
  bootstrap QP mapping, `.gitmodules` URL, and parent gitlink together.
