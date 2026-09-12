Status: done

## Parent

`.docs/prd/2026-08-02-audio-mode.md`

## What to build

The guard rails around Audio Mode: prevent a switch that would corrupt a
running output, and explain the one case where a command legitimately refuses
to act.

Switching the mode while recording or streaming is refused. The mode determines
the output track topology, so changing it mid-output risks an inconsistent
track structure in the file or an interrupted stream. The check happens before
any state change, so a refused switch leaves everything untouched, and the user
is told why.

In single-track mode, invoking a multi-track command reports a message
directing the user to switch modes. This is deliberately different from the
silent no-op that ordinary context unavailability produces: ADR-0003 draws the
line between the two. A command that does nothing because nothing is selected
needs no explanation, since the user will work it out. A command that does
nothing because the user themselves put the panel into single-track mode is not
discoverable, so it must point at the mode control.

## Acceptance criteria

- [x] Switching the Audio Mode while recording is refused, and the reason is
  reported.
- [x] Switching the Audio Mode while streaming is refused, and the reason is
  reported.
- [x] A refused switch changes no state at all - not the mode, not any mute
  state, not the topology.
- [x] After the output stops, switching works normally.
- [x] In single-track mode, invoking a multi-track command reports a message
  that directs the user to the mode control.
- [x] That message names the mode control clearly enough that the user can act
  on it without hunting.
- [x] Ordinary context unavailability stays silent in both modes, per ADR-0003.
- [x] The refusal and the guidance message are present in every shipped locale,
  with keys complete and placeholders in agreement.
- [x] Focused Qt coverage runs offscreen, sends no real keyboard or mouse
  input, and shows no window.
- [x] The production VS2022 v143 build succeeds and staging is refreshed with
  tests disabled.

## Blocked by

- `029-audio-mode-single-and-multi-track.md`

## Implementation

The output guard sits at the top of the mode setter, before any state is
touched, so a refused switch is a genuine no-op. It uses the same
`outputHandler->Active()` check the application already uses to decide whether
a shutdown needs confirming, which covers recording, streaming, the replay
buffer and the virtual camera together.

The mode lock is a single helper the multi-track commands call first: Add,
Remove and Move report it, and the context menu simply does not open in
single-track mode. Keeping it in one place is what stops the two ADR-0003
cases from drifting - ordinary context unavailability still returns silently
from the same handlers.

Note on the harness used while writing this: a heredoc turned the `
`
escapes in the message strings into real newlines, which split the affected
entries across lines and left unterminated quotes in all five locale files.
A structural check across every shipped locale caught it. The files are now
verified line-by-line - each non-blank, non-section line parses as
`key="value"` with balanced quotes - and all 47 AudioRouting keys agree on
key set and escape count across en-US, zh-CN, zh-TW, ja-JP and ko-KR.

## Remaining

Foreground-only, deferred to the acceptance package: seeing the two dialogs
appear with correct wording, and confirming a switch attempted during an
active recording is refused.
