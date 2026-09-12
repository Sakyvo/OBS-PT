Status: done

## Parent

`.docs/prd/2026-07-26-audio-routing-usability-defaults.md`

## What to build

Make collection-owned pure-audio creation a single track-aware transaction. The creation window creates only a new source, proposes the currently selected Editor Track when eligible, and retains the source and its route only after the first property editor is accepted. Existing visual-source reuse remains unchanged. This covers PRD user stories 1-19.

## Acceptance criteria

- [x] Collection-owned pure-audio creation does not display the upstream `New / Add Existing` mode selector or existing-source list.
- [x] Visual Scene Item creation retains its existing `Add Existing` workflow.
- [x] Starting creation with Track 2-6 selected proposes that Editor Track for every new pure-audio source and allows the user to change it; the track/name cells of an occupied first row count as the track selection, while its source cell does not.
- [x] Compatibility Mix, the Muted Sources Group, a source row, and no selection do not become proposed creation routes.
- [x] When no eligible selected track supplies a proposal, established source-type defaults remain intact: Application Audio requires explicit Track 2-6 selection, audio input defaults to `Microphone`, unknown types retain their deterministic fallback, and Desktop Audio remains Mix-only.
- [x] Accepting the first property editor commits one new OBS Source, one collection-owned Source Identity, and one route with no duplicate reference.
- [x] Canceling or closing the first property editor removes the provisional source, collection ownership, route, and persistence state completely.
- [x] Canceling provisional creation creates no undo record and shows no save/discard confirmation.
- [x] Canceling properties for an existing source restores its prior settings without deleting or rerouting it.
- [x] Non-activating `zh-CN` Audio and Routing UI Automation covers the hidden mode selector, selected occupied-track proposal, source-row no-proposal state, disabled required route, accepted creation, cancellation, window-close rollback, and restart persistence.
- [x] Focused creation-model tests cover Mix, Muted Sources Group, non-anchor source rows, track/source cell disambiguation, and deterministic source-type defaults; the real `creation-track` smoke covers committed output routing.
- [x] Shipped locale presentation remains complete; runtime UI Automation qualification is limited to `zh-CN`.

## Blocked by

None - can start immediately
