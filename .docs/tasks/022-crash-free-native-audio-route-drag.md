Status: done

## Parent

`.docs/prd/2026-07-27-audio-routing-interaction-lifetime.md`

## What to build

Make native Audio and Routing drag-and-drop safe for the complete Qt drag
lifetime. Extract stable Source Identity and destination values before mutation,
perform exactly one semantic route transition, and rebuild the presentation only
after Qt has released the old items and indexes. Preserve all fixed groups and
the moved source selection immediately and after restart.

## Acceptance criteria

- [x] Native dragging repeatedly between Editor Tracks completes without a `QTreeView` crash.
- [x] Dragging the sole source out of an Editor Track leaves its empty row visible immediately.
- [x] Track 1-6 and the Muted Sources Group remain visible after every valid or rejected drop.
- [x] Drag callbacks do not retain `QTreeWidgetItem`, model-index, or equivalent presentation pointers beyond immediate value extraction.
- [x] Qt does not perform a second removal against the rebuilt presentation after the semantic move completes.
- [x] Cross-track dragging inserts at the exact indicated destination position.
- [x] Same-track dragging reorders exactly once and respects first, middle, and last positions.
- [x] Dragging an active source into the Muted Sources Group mutes it while preserving its prior Editor Track and position.
- [x] Dragging a muted non-Desktop source into an Editor Track atomically reassigns, inserts, and unmutes it.
- [x] Invalid drops on Compatibility Mix or during a topology lock preserve model and presentation state and retain the existing guidance.
- [x] The moved source retains Source Identity, source instance, properties, monitoring state, collection ownership, and selection.
- [x] Restart restores the exact route and ordering produced by each successful drag and still displays all fixed groups.
- [x] The focused Qt regression covers completion of the full `startDrag()` lifetime rather than invoking `dropEvent()` alone.
- [x] Message-level UI Automation is not reported as native OLE drag evidence.

## Blocked by

- `021-crash-free-atomic-context-menu-routing.md`
