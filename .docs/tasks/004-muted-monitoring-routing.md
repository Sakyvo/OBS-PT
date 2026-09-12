Status: done

## Parent

`.docs/prd/2026-07-20-unified-audio-routing.md`

## What to build

Deliver Muted Sources Group and monitoring semantics end to end. Users can mute, restore, order, and inspect sources without creating silent structural tracks; `Monitor Only` remains an orthogonal local-listening state; monitoring feedback risk is visible without automatic route changes. It covers PRD user stories 27-35.

## Acceptance criteria

- [x] Muting a source moves its card into Muted Sources Group and excludes it from Compatibility Mix, Editor Tracks, streaming, and VOD.
- [x] Newly muted sources appear at the top by default; manual muted-group ordering persists per Scene Collection.
- [x] Unmuting restores the prior Editor Track and prior in-track position.
- [x] `Monitor Only` keeps the source's Editor Track assignment visible but excludes the source from every output and from effective-track calculation.
- [x] A source that is both muted and `Monitor Only` is managed by the muted group; unmuting restores its prior `Monitor Only` state.
- [x] Changing monitoring state while muted updates the state that will be restored without moving the source out of Muted Sources Group.
- [x] An Editor Track containing only muted or `Monitor Only` sources is absent from local output.
- [x] The single-source inspector preserves volume, mono, balance, sync offset, and all monitoring modes without exposing the old track matrix.
- [x] When `Monitor and Output` uses a device also captured by Desktop Audio, the inspector warns about Monitoring Feedback and links to monitoring-device settings.
- [x] The warning never auto-mutes, reassigns, or removes the source.
- [x] Model tests cover mute/monitor state combinations, ordering, restore behavior, and effective-track calculation.
- [x] Real-output tests confirm muted and `Monitor Only` tones are absent while unmuted routed tones remain correctly separated.

## Blocked by

- `002-editor-tracks-source-identity.md`
