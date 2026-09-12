Status: done

## Parent

`.docs/prd/2026-07-27-audio-routing-interaction-lifetime.md`

## What to build

Qualify the complete crash-free Audio and Routing workflow in the production
PotPvP configuration, then refresh staging and the local installer. Prove that
menu movement, native drag ownership, cell intent, Mix locking, toolbar state,
dock behavior, persistence, output topology, and high-frame recording remain
compatible without modifying a GitHub Release or disturbing foreground work.

## Acceptance criteria

- [x] All focused routing-model, source-creation, Qt table, menu-lifetime, and executable-session tests pass.
- [x] The `zh-CN` background UI Automation suite passes menu movement, source selection restoration, fixed group visibility, toolbar state, placeholder state, and dock behavior.
- [x] Runtime automation uses non-activating window operations and posted messages only, sends no real keyboard or mouse input, and terminates every process it starts.
- [x] Native OLE drag safety is supported by the focused full-lifetime Qt test and a final human verification record, not by a false message-level UIA claim.
- [x] Repeated context-menu and drag workflows produce no new OBS-PT crash report or Windows Application Error event.
- [x] Restart preserves routes, ordering, track names, Muted Sources Group order, selection-relevant state, and all seven fixed groups.
- [x] Existing standard-container, custom FFmpeg, Replay Buffer, streaming, VOD, player-compatibility, and installer-preservation regressions remain green.
- [x] Production qualification retains 480 FPS, current resolution, recording quality, encoder defaults, seeded sources, and Audio Routing Configuration.
- [x] The approved VS2022 v143 toolchain builds successfully with production test settings restored afterward.
- [x] Staging is refreshed with tests disabled and contains no test helper executable.
- [x] Build, rundir, and staged OBS-PT executable hashes match.
- [x] Exactly one NSIS process creates the current local installer, which passes timestamp, size, archive-integrity, and SHA-256 checks.
- [x] No visible OBS-PT, installer, or editor automation is launched during unattended qualification.
- [x] No GitHub Release is created, replaced, or modified.

## Qualification evidence

- `BUILD_TESTS=OFF`; build/rundir/staging EXE SHA-256:
  `A4766F01267E55C90278DDAE811F9C8FA55A4EC163517821CD89136B2131B270`.
- `zh-CN` non-activating UIA:
  `.target/_tdd/audio-routing-ui-naming-final8`; mojibake repair,
  properties rename cancellation, toolbar state, floating layout, and restart
  restoration passed. Pointer-dependent creation and drag remain covered by
  focused tests and the human record below.
- Installer: `.target/_pkg/OBS-PT-1.1.0-Installer.exe`, 44,441,459 bytes,
  SHA-256 `AB5F46A9FD9CD7B57E396A08BB465808713ED0DD3E42911E97EEE4780C2E2458`.
- `7z t` passed for 1,470 files; minimized Setup smoke reached
  `OBS-PT 1.1.0 Setup` without executing installation.
- No new crash file, matching Windows Application Error, or leftover test
  process was found after qualification.
- Human record: on 2026-07-27 the user confirmed that repeated real-mouse route
  movement no longer causes either disappearing groups or application crashes.

## Final human OLE verification

Use `.target/_pkg/OBS-PT-1.1.0-Installer.exe` and a real mouse. Do not use UI
Automation or posted window messages for this record.

1. Open Audio and Routing, then drag `minecraft-sound` between Game and Music
   at least 20 times, including first, middle, and last insertion positions.
2. Move the only source out of an Editor Track and confirm the empty track row,
   Tracks 1-6, and Muted Sources remain visible without restarting.
3. Drag an active source into Muted Sources, then drag it back to an Editor
   Track and confirm it is unmuted, selected, and audible.
4. Attempt to drag Mix/Desktop Audio to an Editor Track and confirm the move is
   rejected without hiding a row or changing its assignment.
5. Open and dismiss the source context menu repeatedly, use Move to Track, then
   open the main menu bar and confirm there is no hang or crash.
6. Restart OBS-PT and confirm the final route, order, names, and all fixed groups
   are restored.

Record: accepted by the user on 2026-07-27; neither the disappearing-group
symptom nor the crash remains on the packaged 1.1.0 build.

## Blocked by

- `021-crash-free-atomic-context-menu-routing.md`
- `022-crash-free-native-audio-route-drag.md`
- `023-enforce-mix-and-source-cell-command-boundaries.md`
- `024-finish-audio-routing-toolbar-and-dock.md`
