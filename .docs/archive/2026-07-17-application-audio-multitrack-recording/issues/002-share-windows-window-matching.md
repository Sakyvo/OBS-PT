Status: done

## Parent

`../prd/2026-07-16-application-audio-multitrack-recording.md`

## User stories

14, 36, 38

## What to build

Promote the existing Windows window-enumeration, identity parsing, and matching behavior into one shared utility surface that can be consumed by both visual capture and the future Application Audio Source. Move all existing visual-capture consumers to that shared surface without changing their source properties, matching priorities, minimized-window handling, or UWP behavior.

This is the preparatory slice: it makes process audio capture possible while independently proving that the existing Game Capture and Window Capture paths remain intact.

## Acceptance criteria

- [x] The shared utility supports window title, window class, executable name, minimized-window policy, UWP resolution, serialized window identities, and title/class/executable match priorities.
- [x] Game Capture and Window Capture use the shared utility rather than a plugin-private duplicate.
- [x] Existing serialized window selections remain readable and retain their prior match semantics.
- [x] A Windows runtime smoke test verifies exact-title matching plus class and executable fallback after the refactor.
- [x] Minimized-window enumeration and the existing UWP path show no behavioral regression.
- [x] The supported VS2022 v143 RelWithDebInfo build succeeds for libobs, win-capture, and OBS-PT.
- [x] No Profile, Scene Collection, source display, or recording default changes are introduced in this slice.

## Blocked by

None - can start immediately
