Status: done

## Parent

`.docs/prd/2026-07-20-unified-audio-routing.md`

## What to build

Implement player-first Compatibility Mix composition around the Desktop Audio authority group. Multiple Desktop Audio devices, Application Audio Sources, non-system sources, and private internal audio must produce one complete, de-duplicated `Mix` while Editor Track content remains independently editable. It covers PRD user stories 48-55.

## Acceptance criteria

- [x] Every configured Desktop Audio device is fixed under Compatibility Mix and cannot be dragged into an Editor Track.
- [x] All Desktop Audio devices that initialized successfully at session start form one authority group and contribute to `Mix` while unmuted.
- [x] If at least one authority source is unmuted, Application Audio Sources do not contribute a duplicate copy to `Mix` but remain present in their Editor Tracks.
- [x] Explicitly muting every Desktop Audio source immediately makes session-eligible Application Audio Sources contribute to `Mix`.
- [x] Unmuting any Desktop Audio source restores authority and removes Application Audio Sources from `Mix` without changing Editor Track topology.
- [x] Desktop Audio disconnect or reconnect after session start does not change authority automatically and produces a clear warning/log entry.
- [x] If all Desktop Audio devices are unavailable at session start, eligible Application Audio Sources form the system-playback part of `Mix`.
- [x] Eligible microphones, input devices, capture cards, media sources, and other non-system sources contribute to `Mix` and their assigned Editor Track.
- [x] Private transition/plugin audio contributes to `Mix`, is absent from Editor Tracks, and is absent from the manageable routing-card list.
- [x] Only `Mix` is marked as the default playback audio stream in supported multi-track containers.
- [x] Deterministic signal analysis proves `Mix` contains the expected union exactly once while Editor Tracks contain only assigned source content.
- [x] Model tests cover authority selection and explicit mute transitions; real-output tests cover multiple source classes and de-duplication.

## Blocked by

- `004-muted-monitoring-routing.md`
