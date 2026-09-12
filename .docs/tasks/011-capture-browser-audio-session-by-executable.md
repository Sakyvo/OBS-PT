Status: done

## Parent

`.docs/prd/2026-07-25-executable-wide-application-audio-capture.md`

## What to build

Deliver the first executable-wide Application Audio Source tracer: a source configured with `进程名匹配` must derive or load a stable executable identity, discover the audio session that actually owns the sound, and record it even when the saved window title and class are stale. The path must remain collection-owned, outside every Scene Item, and produce audible data through the real Hybrid MP4 output without opening properties after startup.

## Acceptance criteria

- [x] Process-name mode stores an executable identity independently from window-oriented matching data and compares executable base names exactly, case-insensitively.
- [x] An existing process-mode source without the new identity derives it from its legacy serialized window value and persists it without losing its Source Identity or Editor Track assignment.
- [x] Application audio capture starts from a matching Windows audio session owner rather than assuming the selected window PID owns the sound.
- [x] A matching audio session can be captured when its saved title and class no longer match and when no suitable visible window currently exists.
- [x] A source with no matching session remains pending and automatically starts when the first matching session appears.
- [x] The pure audio source remains outside every Scene Item, remains owned by the Scene Collection, and reaches output through the existing audio-mix-root path.
- [x] A hidden real-runtime smoke starts OBS-PT without opening properties, records Hybrid MP4, and proves the required `Mix` and Editor Track streams contain the expected deterministic tone.
- [x] Initialization-log success alone is not accepted as evidence; decoded output must contain audible signal above the established threshold.
- [x] Existing title, class, and window-executable modes retain their single-window behavior.
- [x] Model and real-output tests pass without lowering production FPS, resolution, quality, or routing requirements.

## Evidence

- Model test: exact case-insensitive basename matching and inaccessible identity fail-closed.
- Real Hybrid MP4 smokes: `capture` and `start-later` in process mode, plus title/class/exe regressions, all at 1920x1080@480 FPS with decoded 440 Hz in `Mix` and `Game`.
- The process-mode smoke persisted `process_executable` while retaining `smoke-game`, Editor Track 2, and collection-owned placement outside Scene Items.

## Blocked by

None - can start immediately
