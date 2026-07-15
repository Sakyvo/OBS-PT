Status: open

# Investigate high-FPS render-delay regression

Source: `.docs/archive/trellis-tasks/07-13-high-fps-render-delay-regression`

Priority: `P2`

## Goal

Determine whether the reported 480 FPS performance loss is a source regression
and keep the current build at or below 0.5% rendering and encoding lag under a
controlled representative workload, without reducing FPS, resolution, or
recording quality.

## Binding Decisions

- Preserve 1920x1080 at 480 FPS, Hybrid MP4, and the accepted NVIDIA and AMD
  encoder defaults.
- Accept a game-capture sample only when its log confirms shared-texture capture
  success; do not combine it with display-capture fallback or an unhooked source.
- Treat a missing encoding-lag line after completed output as zero skipped
  frames, matching the OBS logging contract.
- Run automation from hidden disposable portable copies without taking focus or
  changing the installed OBS-PT configuration and recordings.
- Do not change production code, defaults, priority, or the installer unless a
  controlled version-specific regression or repeatable mitigation is proven.

## Acceptance Criteria

- [x] Controlled evidence either identifies a version-specific causal change or
  demonstrates that the reported slowdown crosses version boundaries.
- [x] With successful game capture and adequate resource headroom, the current
  build records for at least 30 seconds with rendering and encoding lag no
  greater than 0.5%.
- [x] The same controlled workload shows no material regression against the
  retained pre-AMF snapshot.
- [x] Samples record capture-hook validity, process priority, output completion,
  lag counters, errors, and isolated log and recording paths.
- [x] Automated OBS-PT instances remain hidden, never take foreground focus,
  and leave no process running after the test.
- [x] The conclusion does not reduce 480 FPS, resolution, recording quality, or
  the accepted NVIDIA and AMD encoder settings.
- [x] Production binaries remain unchanged because no controlled evidence
  invalidated the current conclusion; no installer rebuild is required.

## Progress So Far

- Controlled 30-second A/B runs with valid game capture and about 4.13 GiB free
  memory measured 0.0-0.1% rendering lag and 0.3% encoding lag for both the
  pre-AMF and current snapshots.
- Under 0.57-0.89 GiB free memory, both old and current snapshots exceeded the
  0.5% target. Above Normal priority did not recover the target.
- A separate machine recorded the current build at 720 FPS with 0.0% rendering
  lag and 0.2% encoding lag.
- The reported regression is therefore a local resource-pressure and capture-
  state false alarm, not an AMF-era production regression. No production source
  or binary was changed.

## Remaining Closure

- [ ] Review the archived evidence and close this migrated issue through the
  yako wrap-up flow. The original Trellis task was still marked `in_progress`
  when migrated even though all of its acceptance criteria were satisfied.

## References

- [PRD](../archive/trellis-tasks/07-13-high-fps-render-delay-regression/prd.md)
- [Design](../archive/trellis-tasks/07-13-high-fps-render-delay-regression/design.md)
- [Implementation plan](../archive/trellis-tasks/07-13-high-fps-render-delay-regression/implement.md)
- [Controlled evidence](../archive/trellis-tasks/07-13-high-fps-render-delay-regression/research/initial-evidence.md)
