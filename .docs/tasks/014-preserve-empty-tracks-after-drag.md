Status: done

Validation note: the `InternalMove` double-removal defect is fixed. A native
Qt `QDropEvent` test proves that the custom handler owns the move exactly once
while rebuilding all seven rows. The five-language background UI suite verifies
the equivalent full-application move, persistence, accessibility, docking, and
layout state without bringing OBS-PT to the foreground. Message-level Windows
input cannot enter Qt's OLE drag loop, so these two tests form the approved
non-activating drag evidence.

## Parent

`.docs/prd/2026-07-25-executable-wide-application-audio-capture.md`

## What to build

Fix Audio and Routing drag-and-drop so the custom routing mutation is the only completed move. Moving the sole source out of a track must leave the empty track row visible immediately, preserve all routing identities and ordering, and remain equivalent to context-menu movement under normal and topology-locked states.

## Acceptance criteria

- [x] Dragging the only Track 2 source to another Editor Track leaves Track 2 visible immediately without requiring mute, unmute, focus, or restart.
- [x] Track 1-6 and M remain present after every valid move, including moves from a sole-source group and moves into or out of the Muted Sources Group.
- [x] One drag produces exactly one route assignment and one optional position change; Qt does not remove a rebuilt presentation row afterward.
- [x] The moved source retains its Source Identity, name, properties, mute state where applicable, monitoring state, and collection ownership.
- [x] Track names, empty drop targets, group separators, colors, and accessible labels remain correct after a move.
- [x] Context-menu and keyboard movement produce the same persisted model state as drag-and-drop.
- [x] Invalid moves to Compatibility Mix retain the existing explanation and do not mutate presentation or model state.
- [x] Active topology-bearing outputs continue to block unsafe route moves with the existing stop/restart guidance.
- [x] Restarting OBS-PT restores the exact route and order produced by the move and still displays every empty track.
- [x] Native Qt drop plus Windows UI Automation cover the sole-source move and assert all six numbered groups plus M immediately and after restart.
- [x] Existing five-language docking, floating, layout restoration, statistics visibility, and accessibility smoke checks remain green.

## Blocked by

None - can start immediately
