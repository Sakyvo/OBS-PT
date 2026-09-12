Status: done

## Parent

`.docs/prd/2026-07-20-unified-audio-routing.md`

## What to build

Expand the tracer into the complete Editor Track and Source Identity model. Deliver all five fixed roles, custom metadata names, deterministic source defaults, collection-wide source visibility, ordering, and source lifecycle behavior through the routing panel, persistence layer, Hybrid MP4 output, and both accepted test seams. It covers PRD user stories 8, 10, 12-16, 22-26, and 39-47.

## Acceptance criteria

- [x] Editor Tracks are fixed as Track 2 `Game`, Track 3 `Music`, Track 4 `Microphone`, Track 5 `Voice Chat`, and Track 6 `Aux`.
- [x] Track numbers cannot be reordered; sources can be reordered within a track and that order persists per Scene Collection.
- [x] Track 2-6 names are editable, trim outer whitespace, reject empty/control-character values, allow duplicates, and always display with their track number.
- [x] Custom track names persist with the Scene Collection, survive UI-language and Profile changes, and are written as output metadata.
- [x] Application Audio Sources expose every Editor Track role during creation, require one explicit selection, and never infer a route from the process name; adding an existing source preserves its route.
- [x] Microphone and audio-input sources default to Track 4 `Microphone`; other unknown audio types use the first empty Editor Track and then Track 6 `Aux`.
- [x] Multiple sources can share one Editor Track while every manageable Source Identity still has at most one Editor Track assignment.
- [x] The routing panel lists all manageable audio sources in the current Scene Collection, including inactive and hidden-scene sources.
- [x] A routed source remains structurally eligible when inactive, disconnected, or actually silent; a track with no assigned eligible source is omitted.
- [x] Reusing one Source Identity across Scenes preserves one route; duplicating it inherits routing into a new independent identity.
- [x] Deleting and recreating a same-named source follows new-source defaults rather than inheriting deleted routing.
- [x] Same-named sources in different Scene Collections retain independent identities, ordering, names, and assignments.
- [x] A real multi-source Hybrid MP4 contains only `Mix` and the non-empty Editor Tracks, with stable order and user metadata.
- [x] Model tests cover naming, defaults, ordering, identity lifecycle, collection boundaries, and effective-track calculation.
- [x] Real-output tests distinguish each assigned source by deterministic tone and verify exact per-track content.

## Blocked by

- `001-compatibility-mix-game-tracer.md`
