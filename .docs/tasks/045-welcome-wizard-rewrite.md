Status: review

## What to build

Rewrite all five first-run Welcome wizard pages (`OBSWelcome.ui` page1-page5,
locale keys `OBSPT.Welcome.Page1..5`) so they reflect the current feature set
and everything new since 1.0.0, in the existing user-facing style (what the
user can do, not issue numbers / key names / code paths).

New page layout (replacing the old five-page default-config intro):

- **Page 1 — Welcome & positioning** (refine the existing page): PotPvP fork,
  OBS 27.2.4 Qt5 base, fully portable. Keep the tone; trim to fit.
- **Page 2 — Defaults**: output folder (install `recordings`), FPS 480,
  resolution = monitor, Hybrid MP4, CQP 27 (>=1080p) / 20 (<=720p), Rec. 709
  & Limited, process priority Normal. Add the new defaults shipped this
  round: audio track defaults to single-track; streaming encoder auto-follows
  the hardware probe (N-GPU -> NVENC); replay buffer preset 180 s / 4096 MB
  (off by default); Java Detection defaults to Strict.
- **Page 3 — Hybrid MP4 & FPS principles** (refine the existing page): crash
  resilience, FPS multiples, motion-blur / interpolation guidance.
- **Page 4 — Audio & Routing** (new topic): the unified Audio Routing
  Configuration panel, single/multi-track mode switch, Mix Track Behavior
  (generate / omit), application audio capture by process name, Java
  Detection strict/lenient.
- **Page 5 — Install & upgrade** (new topic): same-generation installs never
  reset or overwrite the config; hotkeys, recording paths, replay buffer
  always survive; Video Capture Device audio is isolated; orphaned routing
  entries are cleaned automatically.

All five locales (`en-US`, `zh-CN`, `zh-TW`, `ja-JP`, `ko-KR`) get the
rewritten Page1..Page5 + the existing Title/Prev/Next/End keys stay. No new
wizard pages or structural changes — same 5-page QStackedWidget, only the
`text` content (locale keys) changes.

## Acceptance criteria

- [x] Page1..Page5 locale keys rewritten in all five locales with the new
      layout above.
- [x] Each page covers its assigned topics; no 1.0.0-era default-config text
      left that contradicts the current feature set.
- [x] Copy stays user-facing: no issue numbers, config keys, or code paths.
- [x] Software-encoder warning (`OBSPT.FirstRun.SoftwareEncoderWarning`)
      still appends to Page2 when `softwareEncoder` is true.
- [ ] Manual: first-run wizard shows the new content in all five locales
      (switch UI language and relaunch).

## Blocked by

None - can start immediately
