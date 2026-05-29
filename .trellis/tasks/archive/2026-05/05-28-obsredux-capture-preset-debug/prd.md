# Debug OBSRedux PotPvP capture preset

## Goal

Make the OBSRedux PotPvP startup preset load as a usable recording setup out
of the box:

- The default scene collection must show a visible capture source.
- The audio mixer must include desktop audio by default.
- A short recording from the default preset must not encode as all-black video.

## Requirements

- Diagnose the current black-recording and missing-desktop-audio behavior using
  local config, logs, source code, and recording artifacts before changing code.
- Treat the default preset as a shipped OBSRedux startup contract, not a user
  customization flow.
- Fresh startup must enter a single usable `PotPvP` scene directly.
- The `PotPvP` scene source list must contain `Minecraft` above `Display`.
- The `Minecraft` source must be a game-capture source temporarily locked to
  the Lunar Client / Java Minecraft window shape shown in the accepted
  screenshot.
- The `Display` source must remain underneath `Minecraft` as the display-capture
  fallback.
- Fresh startup must add only `Desktop Audio` to the mixer by default.
- Fresh startup must not add or enable microphone/Aux audio by default.
- Preserve the existing portable install behavior and recording output path
  policy from the S8 bootstrap work.
- Do not overwrite user-edited scene/profile data on every launch; any repair or
  import logic must be limited to first-run/bootstrap-owned cases unless a
  damaged preset blocks startup.
- Do not add runtime black-frame detection, automatic source hiding, or automatic
  scene/source switching in this task.
- This task must update the source default preset and also synchronize-overwrite
  the current test install scene collection at
  `K:/Projects/finished/OBS-Redux/obs-studio/basic/scenes/PotPvP.json` once.

## Acceptance Criteria

- [ ] Fresh portable startup selects the `PotPvP` profile and scene collection.
- [ ] Fresh startup loads `PotPvP` as the current scene.
- [ ] `PotPvP` shows `Minecraft` above `Display` in the source list.
- [ ] `Minecraft` is the primary visible source and `Display` is the lower
      visible fallback source.
- [ ] At least one visible source can produce non-black output without manual
      source creation.
- [ ] Fresh startup shows `Desktop Audio` in the mixer without manual settings
      edits.
- [ ] Fresh startup does not show microphone/Aux in the mixer by default.
- [ ] If a required shipped preset file is missing or invalid before first-run
      bootstrap completes, startup is blocked with the localized damaged package
      message already planned in S8.
- [ ] A 2-second local recording from the preset is not detected as all-black by
      `ffmpeg` black-frame checks.
- [ ] `K:/Projects/finished/OBS-Redux/obs-studio/basic/scenes/PotPvP.json` is
      overwritten once with the corrected default preset for immediate testing.

## Confirmed Facts

- Source default `UI/data/obsredux-defaults/obs-studio/basic/scenes/PotPvP.json`
  currently contains only one `game_capture` source named `Game Capture`.
- The running finished build at
  `K:/Projects/finished/OBS-Redux/obs-studio/basic/scenes/PotPvP.json` contains
  a manual scene with `Minecraft` (`game_capture`) and `Display`
  (`monitor_capture`), both visible. Its saved JSON item order is `Display`,
  then `Minecraft`, which corresponds to the accepted UI order where
  `Minecraft` is above `Display`.
- The running finished build's scene JSON does not contain top-level
  `DesktopAudioDevice1` or other global audio device objects.
- `OBSBasic::LoadData()` only loads mixer global audio devices from top-level
  scene collection JSON keys such as `DesktopAudioDevice1`; it does not create
  desktop audio when a scene collection file exists.
- `CreateFirstRunSources()` can create default desktop and input audio, but it
  is only called from `CreateDefaultScene(firstStart)` when OBS creates a new
  default scene.
- Latest log
  `K:/Projects/finished/OBS-Redux/obs-studio/logs/2026-05-28 12-43-56.txt`
  contains no `[Loaded global audio device]` lines, confirming the mixer issue
  is caused by missing saved global audio devices.
- The latest recording
  `K:/Projects/finished/OBS-Redux/recordings/2026-05-28 12-51-44.mp4` is 2.11
  seconds, 1920x1080, 360 FPS, and `ffmpeg blackdetect` reports black video from
  0 to 2.097222 seconds.
- OBS logged 757 output frames and 791 drawn frames for that recording, so the
  encoder ran and wrote frames; the frames were already black before or at scene
  rendering.
- OBS scene rendering iterates `scene->first_item` to `next`; scene JSON item
  order is bottom-to-top because later items render later.
- `game_capture_render()` returns without drawing when no capture texture is
  active, but an active black capture texture can still cover lower scene items
  if `Minecraft` is the top source.
- `game_capture` window priority values are defined by
  `plugins/win-capture/window-helpers.h`: `0` = match title, then same window
  class; `1` = title must match; `2` = match title, then same executable.
- The accepted screenshot uses the Lunar Client window entry and the priority
  shown as "match title, otherwise find same type window", which corresponds to
  `priority: 0`.
- The local upstream OBS scene collection
  `C:/Users/ASUS/AppData/Roaming/obs-studio/basic/scenes/未命名.json` includes
  valid `DesktopAudioDevice1` and `AuxAudioDevice1` objects, but its `PotPvP`
  scene has the listed sources saved as `visible:false`; its separate `Display`
  scene contains a visible monitor-capture source.

## Current Hypotheses

- Missing mixer desktop audio has a direct config/code cause: the shipped or
  replaced scene collection must include `DesktopAudioDevice1`, or bootstrap
  must create it when installing the preset.
- The all-black recording is not explained by source item order, because the
  display-capture source is topmost in the current saved scene. Remaining likely
  causes are capture-source configuration/runtime behavior or the default preset
  not loading the intended scene at startup.

## Decisions

- The shipped startup scene should be a single immediately usable `PotPvP`
  scene, not a split `PotPvP` / `Display` scene flow.
- Source order should match the accepted UI: `Minecraft` above `Display`, with
  `Display` underneath.
- `Minecraft` should temporarily target Lunar Client only. Do not use the old
  `Minecraft|Lunar Client|Badlion Client` matching idea in the shipped preset,
  because `game_capture` does not implement that value as a wildcard title
  matcher.
- Only `DesktopAudioDevice1` should be added to the default scene collection.
  Do not import the local upstream OBS `AuxAudioDevice1` object into the
  OBSRedux preset.
- Runtime black-frame detection or automatic fallback switching is out of scope
  for this task. The default preset should keep `Minecraft` on top and `Display`
  underneath, with validation focused on the shipped startup state and manual
  short-recording check.
- The corrected scene collection should be written both to the repo default
  preset and to the current finished-build test install for immediate
  verification.

## Out of Scope

- Detecting black frames at runtime.
- Automatically hiding `Minecraft`, reordering sources, or switching scenes when
  game capture returns a black texture.
- Expanding default game capture to Badlion, vanilla launcher, or other
  Minecraft clients.
