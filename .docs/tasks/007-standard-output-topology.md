Status: done

## Parent

`.docs/prd/2026-07-20-unified-audio-routing.md`

## What to build

Apply derived Recording Topology to every standard local output path and replace per-track output parameters with Unified Audio Bitrate. Both Simple and Advanced Output must produce the same externally visible stream set, container metadata, video-only behavior, and complete-start validation. It covers PRD user stories 75-77, 80-81, and 85-87.

## Acceptance criteria

- [x] Hybrid MP4, MP4, MOV, MKV, and TS emit `Mix` plus every non-empty Editor Track in stable numeric order.
- [x] FLV and M3U8/HLS emit only `Mix` and do not erase or rewrite Editor Track routing.
- [x] Simple Output and Advanced Output derive identical audio topology for the same Scene Collection and format.
- [x] A local session with no eligible audio source starts as video-only and contains no silent audio stream.
- [x] Only `Mix` is marked default/enabled for ordinary playback where the container supports that distinction.
- [x] Track metadata uses `Mix` and the Scene Collection's saved Editor Track names.
- [x] Output settings contain one Audio Bitrate value, default `160 Kbps`, shared by every local Mix and Editor encoder.
- [x] A user-selected unified bitrate reaches every emitted audio encoder and cannot be changed while a related output is active.
- [x] Legacy recording, FLV, and custom mixer masks cannot override Audio Routing Configuration.
- [x] Failure to initialize any required audio encoder or container stream rejects that output and reports track name, container, and underlying cause.
- [x] A rejected start does not leave a misleading playable partial artifact and never silently drops one Editor Track.
- [x] The Windows smoke matrix probes stream count, names, order, default disposition, codec, sample rate, bitrate, duration, and tone content for every standard format.
- [x] Hybrid MP4 video-only and multi-track finalization both complete cleanly after normal stop.

## Blocked by

- `005-desktop-audio-authority.md`
- `006-frozen-recording-topology.md`
