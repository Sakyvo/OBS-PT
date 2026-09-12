Status: done

## Parent

`.docs/prd/2026-07-20-unified-audio-routing.md`

## What to build

Deliver service-aware streaming through Compatibility Mix and optional VOD Track. Bilibili and ordinary streams always receive complete Mix audio, users can silence a live stream without disconnecting, and services with VOD support can select one valid Editor Track without any fallback that leaks excluded content. It covers PRD user stories 66-72 and 84.

## Acceptance criteria

- [x] Ordinary streaming always uses Compatibility Mix and exposes no Track 1-6 stream selector.
- [x] Bilibili service configuration hides VOD controls and starts through Compatibility Mix.
- [x] Muting every eligible source keeps one silent AAC Compatibility Mix stream and does not stop, reconnect, or recreate the live output.
- [x] Unmuting resumes audio on the same live connection and does not alter local Editor Track topology.
- [x] A Mix-only stream does not lock Editor Track movement or naming when no topology-bearing output is active.
- [x] Services that support VOD expose one Editor Track selector using the Scene Collection's current names.
- [x] VOD-enabled startup is rejected when the selected Editor Track has no eligible source; the error focuses or links to the routing panel.
- [x] VOD empty-track failure never falls back to `Mix` and cannot leak excluded music into the archive.
- [x] Temporary post-start mute leaves the VOD encoder active and sends silence without stopping streaming.
- [x] VOD-enabled streaming freezes the routing snapshot and enforces the same blocked commands as recording.
- [x] A Mix-only stream and compatible local multi-track recording can run together without duplicate Mix content or conflicting source assignment.
- [x] Automated integration uses a controllable local streaming endpoint to verify connection continuity and packet audio state.
- [x] Manual Bilibili acceptance confirms service visibility, all-muted privacy behavior, and simultaneous local recording. User explicitly waived this non-critical manual check after the automated Bilibili service-branch and local RTMP evidence passed.

## Latest automated evidence

- `obspt-application-audio-smoke.ps1 -Scenario dual -Streaming -VodTrack` passed against the release staging tree, including Mix mute/unmute, VOD silence windows, and local recording concurrency.
- The same smoke with `-BilibiliLocal` rewrites only the copied Bilibili service URL to a local RTMP endpoint while retaining `rtmp_common` and `service=Bilibili Live`; it passed with one AAC Mix stream, no VOD data stream, and uninterrupted mute/unmute.

## Blocked by

- `006-frozen-recording-topology.md`
- `007-standard-output-topology.md`
