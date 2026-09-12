Status: done

## Parent

`.docs/prd/2026-07-25-executable-wide-application-audio-capture.md`

## What to build

Qualify the executable-wide audio workflow, stable process UI, drag repair, and Elevated Launch policy together against OBS-PT's production runtime and package. Complete the real Vivaldi and UAC acceptance checks, preserve the 480 FPS product contract, refresh staging with tests disabled, and produce one verified local installer without publishing a Release.

## Acceptance criteria

- [x] The complete audio routing, executable-session reconciliation, elevated-launch, and update model test suites pass.
- [x] Real Hybrid MP4 smokes prove stale-title capture, browser-style child audio, multiple same-name roots, parent/child de-duplication, late start, exit, and restart with exact stream names and distinguishable tones.
- [x] Existing player compatibility and editor-track checks still prove `Mix` default playback and isolated Editor Track content under the current topology contract.
- [x] Five-language UI Automation plus the native Qt drop test pass stable Process selection, absent-target persistence, sole-source movement, all Track 1-6/M rows, accessibility, docking, floating, restart layout restoration, and default statistics visibility.
- [x] An isolated real Vivaldi profile changes its active title and restarts without opening source properties; the Hybrid MP4 contains `Mix, Music`, and both the initial and post-restart Music segments contain 440 Hz audio.
- [x] Windows acceptance confirms successful elevation preserves startup context; deterministic execution-layer refusal confirms the ordinary process continues silently on this UAC-disabled workstation.
- [x] The production 480 FPS qualification meets the established rendering-lag, encoding-lag, A/V drift, resource-headroom, stream-count, and tone-isolation thresholds without lowering resolution, quality, or encoder settings.
- [x] RelWithDebInfo builds successfully with VS2022 Build Tools v143 and the required dependency configuration.
- [x] Staging is refreshed with `BUILD_TESTS=OFF`; no model test, tone helper, UI automation helper, or other test executable is present in the staged product.
- [x] Staged executable, libobs, win-wasapi, UI resources, shipped Global Config, and source defaults match the verified build inputs.
- [x] Exactly one NSIS process builds the installer and exits normally without a residual process.
- [x] The installer has a new timestamp, passes archive integrity, has a recorded SHA-256, and reaches the Setup main window in an actual launch.
- [x] No GitHub Release is created, replaced, or modified by this issue.
- [x] Bootstrap documentation and durable troubleshooting rules describe executable-wide session discovery, process-tree de-duplication, permission failure, Qt drag ownership, and Elevated Launch behavior.

Qualification artifact:
`.target/_pkg/OBS-PT-1.1.0-Installer.exe`, SHA-256
`9C9913DA3211EAB8F5F2C5EC937A5D87AF7968C07782D8071BFA1E00DB52F269`.

## Blocked by

- `011-capture-browser-audio-session-by-executable.md`
- `012-reconcile-and-deduplicate-executable-sessions.md`
- `013-present-stable-process-targets.md`
- `014-preserve-empty-tracks-after-drag.md`
- `015-launch-elevated-by-default.md`
