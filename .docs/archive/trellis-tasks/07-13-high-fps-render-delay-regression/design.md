# Technical Design

## Decision

Treat this task as a controlled attribution and benchmark-hardening task. Do
not modify the OBS render, NVENC, AMF, Hybrid MP4, or default-config paths unless
a version effect survives capture-state and resource controls.

Current evidence rejects a production change: pre-AMF and current snapshots
are equivalent with valid game capture and adequate memory, while both regress
under severe memory pressure. Raising process priority is not an effective
mitigation.

## Boundaries

- Test runner: task-local PowerShell under `research/run-ab.ps1`.
- Inputs: retained portable snapshots plus a copied PotPvP profile/scene.
- Outputs: per-run isolated logs, recordings, and `results.csv` under
  `build-v143/_perf-ab`.
- Protected state: `D:/OBS-PT` is read only; its logs, recordings, and config
  are excluded from copy-back or mutation.
- Production source and packaged binaries remain unchanged under the accepted
  evidence gate.

## Sample Contract

Each sample records snapshot, iteration, mode, process priority, start time,
duration, rendering lag, encoding lag, skipped/total encoded frames, capture
hook status, termination state, recording size, log path, and error.

For game mode, `GameCaptureHooked=true` requires a log line matching the
Minecraft shared-texture success event. `attempting to hook` alone is invalid.
A completed Hybrid MP4 with no encoding-lag line has zero skipped frames because
libobs emits that line only for non-zero counts.

## Execution Flow

1. Refuse to run while any OBS-PT process exists.
2. Copy each selected snapshot and installed config to an isolated run root.
3. Redirect output, disable updates and dialogs, configure the timer, source
   visibility, and explicit test process priority only in the copy.
4. Start OBS-PT hidden with `--minimize-to-tray` and wait for recording start.
5. Wait for Hybrid MP4 completion, allow the optional non-zero skip line to
   flush, parse the log, and terminate the isolated process.
6. Export every success or failure with a consistent CSV schema.
7. Accept performance comparisons only after applying the sample contract.

## Attribution Gate

- A source regression requires repeatable separation between snapshots under
  the same capture and resource state.
- A mitigation requires repeated `<=0.5%` results without weakening quality or
  harming game responsiveness.
- If neither gate passes, retain production behavior and document the runtime
  preconditions instead of introducing a speculative fix.

## Compatibility And Rollback

The runner uses Windows PowerShell APIs and existing portable OBS-PT behavior;
it does not alter release compatibility. Rollback is limited to discarding the
task-local runner and generated `_perf-ab` directories. Because no binary or
plugin changes are planned, the installer rule is not triggered.
