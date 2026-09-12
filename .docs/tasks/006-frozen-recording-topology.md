Status: done

## Parent

`.docs/prd/2026-07-20-unified-audio-routing.md`

## What to build

Freeze Recording Topology and source eligibility for topology-bearing sessions while preserving safe live controls. The routing panel, Scene Collection lifecycle, output orchestration, and tests must agree about which commands are blocked, which changes are deferred, and which existing streams can mute and resume. It covers PRD user stories 56-65.

## Acceptance criteria

- [x] Normal recording, custom FFmpeg recording, Replay Buffer, and VOD-enabled streaming snapshot source eligibility and non-empty tracks at startup.
- [x] A Compatibility Mix-only stream does not lock Editor Track routing by itself.
- [x] Route movement, track rename, eligibility-changing monitoring mode, unified bitrate, and Scene Collection switching are blocked while any topology-bearing output is active.
- [x] A blocked action identifies the active output and tells the user to stop and restart it; no automatic split or restart occurs.
- [x] Routing unlocks only after every topology-bearing output has stopped.
- [x] Volume, balance, sync offset, filters, and source mute remain usable during an active session.
- [x] A source eligible at startup can mute and resume its existing stream without changing track count or metadata.
- [x] A source muted or `Monitor Only` at startup remains excluded when later enabled and is visibly marked for the next session.
- [x] A newly created or duplicated audio source is allowed but marked for the next session and cannot enter the current file.
- [x] Deleting a source leaves its current stream slot intact for the remainder of the session; changing its capture target preserves Source Identity and route.
- [x] Switching or deleting unrelated visual sources remains possible without changing audio topology.
- [x] Model tests exercise every command under unlocked, recording, Replay Buffer, and VOD-lock states.
- [x] Real-output tests prove stream count remains fixed across mute/resume, source creation, source deletion, and delayed source activation.

## Blocked by

- `004-muted-monitoring-routing.md`
- `005-desktop-audio-authority.md`
