# Investigate high-FPS render-delay regression

## Goal

Determine whether the reported 480 FPS performance loss is a source regression
and keep the current build at or below 0.5% rendering and encoding lag under a
controlled representative workload, without reducing FPS, resolution, or
recording quality.

## Background

- The reported RTX 3060 run showed 3.5% rendering lag and 6.6% encoding lag at
  1920x1080/480 FPS with `jim_nvenc`, Hybrid MP4, P1/HQ, single pass, CQP 27,
  High profile, and no B-frames, lookahead, or psycho-visual tuning.
- The same installed binaries ranged from 1.2%/2.7% with stable game capture to
  4.7%/17.6% during display-capture fallback, then 2.0%/6.5% after game capture
  returned. The high-lag session also had 960 ms of audio buffering and only
  about 1.1 GiB free physical memory.
- All candidate binaries use the known-good VS2022 v143 14.44 toolset. The
  previous VS18 compiler regression is not present.
- Controlled disposable-copy tests found no measurable difference between the
  pre-AMF and current snapshots with successful game capture and about 4 GiB
  free memory: both produced 0-0.1% rendering lag and 0.3% encoding lag.
- When Gradle Minecraft and its daemon reduced free memory below 0.9 GiB, old
  and current snapshots both exceeded 0.5% encoding lag, even without a hooked
  source. Above Normal process priority did not restore the target.
- A separate machine running the current build at 720 FPS reported 0.0%
  rendering lag and 0.2% encoding lag over approximately 280,000 encoded
  frames, confirming the release binary remains healthy beyond the 480 FPS
  baseline.

## Requirements

- Attribute the regression from controlled evidence, not from the chronology
  of the latest AMF commit.
- Preserve 1920x1080 at 480 FPS, Hybrid MP4, and the accepted NVIDIA and AMD
  encoder defaults.
- Compare retained snapshots with identical profiles, duration, output paths,
  process priority, capture state, and background-resource observations.
- Count a game-capture sample only when the log confirms shared-texture capture
  success; do not mix it with display-capture fallback or an unhooked source.
- Treat a missing encoding-lag line after a completed output as zero skipped
  frames, matching `libobs/media-io/video-io.c` logging semantics.
- Run automation from disposable portable copies, hidden and background-only,
  without stealing focus or changing `D:/OBS-PT` configuration or recordings.
- Do not change production code, encoder defaults, or process priority unless a
  controlled version-specific regression or a repeatable mitigation is proven.
- Preserve a durable benchmark procedure that detects toolchain, capture-state,
  and resource-pressure false attribution before packaging.
- If a binary or plugin DLL eventually changes, rebuild and verify a fresh NSIS
  installer according to the repository rule.

## Acceptance Criteria

- [x] Controlled evidence either identifies a version-specific causal change
      or demonstrates that the reported slowdown crosses version boundaries.
- [x] With successful game capture and adequate resource headroom, the current
      build records for at least 30 seconds with rendering and encoding lag each
      no greater than 0.5%.
- [x] The same controlled workload shows no material regression against the
      retained pre-AMF snapshot.
- [x] Samples record capture-hook validity, process priority, output completion,
      lag counters, errors, and isolated log/recording paths.
- [x] Automated OBS-PT instances remain hidden, never take foreground focus,
      and leave no process running after the test.
- [x] The conclusion does not reduce 480 FPS, resolution, recording quality, or
      the accepted NVIDIA/AMD encoder settings.
- [x] Production binaries remain unchanged unless new controlled evidence
      invalidates the current conclusion; otherwise no installer rebuild is
      required.

## Out Of Scope

- Guaranteeing 480 FPS recording while physical memory is exhausted by
  unrelated applications.
- Solving the issue by lowering frame rate, resolution, or recording quality.
- General Replay Buffer optimization or unrelated encoder feature work.
