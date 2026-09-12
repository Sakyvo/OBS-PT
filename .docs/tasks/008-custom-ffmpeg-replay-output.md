Status: done

## Parent

`.docs/prd/2026-07-20-unified-audio-routing.md`

## What to build

Extend unified topology to custom FFmpeg, Replay Buffer, and concurrently active outputs. Each output must validate independently, supported outputs share the frozen routing snapshot, and an incompatible second start must fail without changing or stopping work already in progress. It covers PRD user stories 73-74 and 78-83.

## Acceptance criteria

- [x] Custom FFmpeg no longer exposes or consumes an independent audio-mix bitmask.
- [x] A known multi-track custom container emits the complete derived topology with stable stream names and order.
- [x] A known single-track custom container automatically emits Compatibility Mix only.
- [x] Unknown or incompatible custom-container capability rejects startup with a clear error rather than guessing.
- [x] Replay Buffer consumes the same routing snapshot, stream naming, and unified bitrate rules as normal recording.
- [x] Starting Replay Buffer freezes routing; a saved replay contains the topology that existed when the buffer started.
- [x] Starting a compatible second topology-bearing output shares the existing frozen source eligibility and bitrate without reconfiguring the first.
- [x] Starting an incompatible second output rejects only that output; the already active recording or Replay Buffer continues and remains valid.
- [x] A second-output failure cannot remove tracks, change format, restart an encoder, split a file, or stop another output.
- [x] All related output state is released only when the corresponding output stops; routing remains locked while any topology-bearing output is active.
- [x] Model tests cover capability decisions and parallel-output state; real-output tests cover custom multi/single formats, Replay save, compatible coexistence, and isolated failure.

## Blocked by

- `007-standard-output-topology.md`
