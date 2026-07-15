# Initial Evidence

## Prior Regression

The archived `06-17-encoding-overload-regression` task measured the same RTX
3060, scene, 1080p/480 FPS, and NVENC settings at 4.1-4.7% encoding lag in a VS18
build versus 0.1-0.5% in official OBS. Rebuilding the complete pipeline with
VS2022 v143 reduced encoding lag to 0.8-1.2% and rendering lag to 0.4-0.8%.
The prior root cause was compiler code generation across libobs, not an NVENC
preset change.

## Current Runtime

- Process: `D:/OBS-PT/bin/64bit/OBS-PT.exe`, started 2026-07-13 12:07.
- GPU: RTX 3060, driver 581.57.
- Encoder: `jim_nvenc`, not AMF.
- Profile: 1920x1080, 480/1 FPS, NV12 709/Partial, Hybrid MP4, one audio track.
- Encoder JSON: P1, HQ tuning, multipass disabled, CQP 27, High, B-frames 0,
  lookahead off, psycho AQ off.
- Process priority observed during research: Normal.
- Scene contains hidden Display capture and visible Minecraft game capture.

PE headers report linker 14.44 for the active EXE, `obs.dll`,
`libobs-d3d11.dll`, `obs-ffmpeg.dll`, and `enc-amf.dll`. The prior VS18 14.51
miscompilation is not present in these binaries.

## Same-Binary Measurements

| Log | Capture state | Rendering lag | Encoding lag |
|---|---|---:|---:|
| 2026-07-12 16-40-11 | Game capture hooked and stable | 89/7701 (1.2%) | 205/7704 (2.7%) |
| 2026-07-13 12-07-29, first run | Game capture absent; DXGI display capture active | 572/12290 (4.7%) | 2161/12289 (17.6%) |
| 2026-07-13 12-07-29, second run | Game capture recovered | 289/14378 (2.0%) | 929/14381 (6.5%) |

All three used the same installed EXE, core/plugin DLLs, and serialized NVENC
settings. The July 13 session also accumulated 960 ms of desktop-audio buffering
before recording and started with 1117 MB free physical memory versus 2416 MB
in the lower-lag July 12 session. These observations prove runtime pressure and
capture state affect the result; they do not yet prove either is the sole cause.

## Retained Binary Snapshots

- July 5 pre-AMF/Hybrid validation: `_hybrid-smoke-fixed-default`
- July 12 AMD-era build matching the installed EXE: `_amd-smoke-20260712-113151`
- Current staged build: `_pkg/OBS-PT`

The installed `obs-outputs.dll` is byte-identical to both the July 5 Hybrid
snapshot and current staging. The active AMF DLL cannot explain NVENC behavior
without a controlled disable test because it fails to load `amfrt64.dll` on
this NVIDIA-only machine and only records a short initialization attempt.

## Binary Attribution

The three snapshots have identical SHA-256 hashes for `obs-ffmpeg.dll`
(NVENC), `win-capture.dll`, and `obs-outputs.dll` (Hybrid MP4). PE section
inspection also found identical `.text` sections in `obs.dll` and
`obs-frontend-api.dll`; their whole-file hashes differ only because of PE
metadata. The remaining code-bearing differences are `OBS-PT.exe` and
`enc-amf.dll`. On this NVIDIA-only machine the AMF plugin cannot load
`amfrt64.dll` and exits initialization, so it is not part of the active NVENC
recording pipeline.

## Controlled A/B

All runs used disposable portable copies under `build-v143/_perf-ab`, 30-second
recordings, 1920x1080 at 480 FPS, and the same copied PotPvP profile. The runner
starts OBS-PT with `--minimize-to-tray` and a hidden startup window, disables
the updater, writes to an isolated recording directory, and never mutates
`D:/OBS-PT`.

### Normal resource headroom

Run `20260713-134450-game` produced these valid game-capture samples. A sample
is valid only when its log contains `shared texture capture successful`.

| Snapshot | Iteration | Rendering lag | Encoding lag |
|---|---:|---:|---:|
| Pre-AMF | 1 | 0.1% | 0.3% |
| Current | 2 | 0.1% | 0.3% |
| Pre-AMF | 2 | 0.0% | 0.3% |

Free physical memory measured after the run was 4.13 GiB. The current binary
therefore meets the user's 0.5% target and is indistinguishable from the
pre-AMF snapshot under the same controlled conditions.

### Resource pressure

Later, a Gradle test Minecraft process and Gradle daemon started alongside
Lunar Client. Their observed working sets were approximately 2.4 GiB and
0.8 GiB, while Lunar used approximately 2.4 GiB. Free physical memory fell to
0.57-0.89 GiB. Game capture did not hook these runs, so they are not gameplay
acceptance samples; they isolate the 480 FPS render/encode scheduler with an
empty source under the same memory pressure.

| Run | Snapshot / priority | Free memory | Rendering lag | Encoding lag |
|---|---|---:|---:|---:|
| `20260713-135719-game` | AMD-era / Normal | 0.76 GiB | 0.9%, 0.6% | 3.8%, 3.8% |
| `20260713-140000-game` | Current / Normal | 0.78 GiB | 0.1% | 1.4% |
| `20260713-140112-game` | Pre-AMF / Normal | 0.89 GiB | 0.1% | 1.0% |
| `20260713-140339-game` | Current / AboveNormal | 0.57 GiB | 0.1%, 0.0% | 1.0%, 1.0% |

The old and current binaries both cross the 0.5% limit when physical memory is
nearly exhausted. Raising OBS-PT to Above Normal priority does not restore the
target and would risk degrading game responsiveness, so it is rejected as a
default mitigation.

### Independent-machine validation

The user supplied a final Statistics screenshot from another machine running
the current build at 720 FPS. Over the displayed run it recorded 300,484 render
attempts with 86 lagged frames (0.0%) and 280,218 encoded frames with 597
skipped frames (0.2%). This exceeds the product's 480 FPS baseline while staying
inside the 0.5% target, independently confirming that the release binary does
not have an intrinsic high-FPS regression.

## Measurement Corrections

- OBS logs the `number of skipped frames due to encoding lag` line only when
  the skipped count is non-zero. Its absence after a completed recording means
  0%, not a failed sample.
- Merely logging `attempting to hook process` is insufficient. Game-capture
  samples require a later shared-texture success line.
- Display-capture fallback and game-capture success must not be mixed in the
  same comparison.
- The runner now records capture-hook state, explicit process priority, errors,
  and hidden-window startup for each sample.

## Conclusion

No controlled evidence supports an AMF-source-induced NVENC regression. The
reported high-lag state is explained by two runtime variables already observed
with the same binary: capture fallback and severe memory/background-process
pressure. The latter was reproduced across pre-AMF, AMD-era, and current
snapshots, while an independent machine remained at 0.0%/0.2% at 720 FPS. No
production binary or default-setting change is justified; the durable fix is a
controlled release benchmark and strict sample-validity rules.
