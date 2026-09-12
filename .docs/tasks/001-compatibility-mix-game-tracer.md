Status: done

## Parent

`.docs/prd/2026-07-20-unified-audio-routing.md`

## What to build

Build the first complete Audio Routing path for one Application Audio Source: explicitly choose and persist a Scene Collection-owned assignment to Track 2 `Game`, expose that assignment through a minimal routing panel, derive Hybrid MP4 topology automatically, and emit Track 1 `Mix` plus Track 2 `Game`. This tracer establishes the shared routing-model and real-output test seams used by later slices. It covers PRD user stories 1-3, 6, 7, 9, 11, 18, 39, 76, and 94.

## Acceptance criteria

- [x] A new Application Audio Source cannot be confirmed until the user explicitly selects Track 2-6; the selected route persists through creation and undo/redo.
- [x] Track 1 is fixed as `Mix`; it cannot be renamed, reordered, or selected as an Editor Track destination.
- [x] One source cannot be assigned to more than one Editor Track through any supported command path.
- [x] The Game assignment round-trips through Scene Collection save/load and is unchanged by Profile switching.
- [x] Hybrid MP4 derives its audio encoders from Audio Routing Configuration rather than legacy per-output track masks.
- [x] A deterministic Game tone recording contains video, default audio stream `Mix`, and Editor Track `Game` in stable order.
- [x] Signal analysis confirms the expected Game tone is present in both `Mix` and `Game` without an additional audio stream.
- [x] The focused C++ model test covers the minimal assignment, persistence, Track 1 invariants, and topology derivation.
- [x] The existing Windows application-audio smoke harness covers the real OBS-PT recording and `ffprobe` assertions.

## Blocked by

None - can start immediately
