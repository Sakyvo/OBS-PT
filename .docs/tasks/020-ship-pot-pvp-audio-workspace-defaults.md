Status: done

## Parent

`.docs/prd/2026-07-26-audio-routing-usability-defaults.md`

## What to build

Ship the approved PotPvP Audio and Routing workspace after the creation, routing, and process-property behavior is complete. Align the unified audio bitrate setting, capture the approved current GUI and dock state, seed deterministic Minecraft and music process sources, preserve only the accepted installer categories, and qualify the complete production package. This covers PRD user stories 28 and 36-46.

## Acceptance criteria

- [x] The unified audio bitrate label aligns with Output Mode using the same settings-form label geometry without changing bitrate behavior.
- [x] Fresh and reset installations show the approved GUI and dock layout, including Audio and Routing and the Statistics dock in their approved default positions.
- [x] The packaged Scene Collection contains `minecraft-sound` targeting `javaw.exe` on Track 2 `Game` and `music` targeting `vivaldi.exe` on Track 3 `Music`.
- [x] Both seeded Application Audio Sources use process-name matching, stable project-owned Source Identities, and contain no stale window selector fields or development-machine UUIDs.
- [x] Seeded sources remain visible and routed while their target processes are absent and begin capture when matching sessions appear.
- [x] Packaged defaults contain no development-machine recording path, personal hotkey, stream credential, or Replay Buffer value.
- [x] Existing-install reset restores only stream settings, recording paths, hotkeys, and Replay Buffer settings after deploying the packaged defaults.
- [x] Scene sources, Audio Routing Configuration, GUI layout, video, audio, and encoder settings come from the packaged defaults and are not restored from the old installation.
- [x] Installer configuration-reset smoke covers fresh install, confirmed reset, preservation scope, stable source identities, process-only targets, and approved dock state.
- [x] Audio and Routing UI smoke covers the final installed layout, seeded source rows, track names, Statistics visibility, and unified bitrate alignment.
- [x] Real Application Audio smoke records distinguishable Minecraft and music test signals through the seeded route shape and verifies player-first `Mix` plus Editor Tracks.
- [x] The complete routing, executable-session, player compatibility, real-output, `zh-CN` UI, update-model, and installer regression suites pass; foreground VEGAS automation is intentionally excluded.
- [x] Production qualification retains 480 FPS, display-derived resolution, quality, and encoder defaults without lowering settings to hide defects.
- [x] RelWithDebInfo builds with the required VS2022 v143 toolchain, staging is refreshed with tests disabled, and no test helper enters the staged product.
- [x] Exactly one NSIS process produces a new local installer that passes archive integrity, SHA-256 recording, timestamp inspection, and an actual launch to the Setup main window.
- [x] No GitHub Release is created, replaced, or modified.

Qualification artifact:
`.target/_pkg/OBS-PT-1.1.0-Installer.exe`, SHA-256
`833B35A49C311112930ACB049E29C72DD01FC02701713585110AED71129685BE`.

## Blocked by

- `017-transactional-track-aware-audio-source-creation.md`
- `018-manage-existing-audio-sources-in-routing-panel.md`
- `019-live-process-capture-properties.md`
