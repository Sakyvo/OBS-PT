Status: done

## Parent

`../prd/2026-07-16-application-audio-multitrack-recording.md`

## User stories

13-27, 36-38

## What to build

Deliver one complete application-audio path on supported Windows systems: the user can add `Application Audio Capture (BETA)`, choose a window and match priority, capture its process tree, route the new Source to mixer 2 by default, and record that isolated audio through the existing Hybrid MP4 output.

Adapt the OBS 28 process-loopback implementation together with the released lifecycle, performance, stop-hang, format, timestamp, silence-buffer, and logging corrections. Include the Source icon and OBS-PT's supported translations. Missing or restarted targets must remain a silent background state rather than producing modal UI.

## Acceptance criteria

- [x] Windows 10 Build 19041 or newer registers `wasapi_process_output_capture`; older builds leave the Source unregistered without affecting existing WASAPI Sources.
- [x] The Source is displayed as BETA and provides a window selector plus title, class, and executable match priorities.
- [x] The selected process and its process tree are captured through Windows process loopback.
- [x] A newly created Application Audio Source is assigned only to mixer 2 and does not automatically enable Recording Track 2.
- [x] Reusing or duplicating an existing Source preserves its stored mixer assignment.
- [x] A missing target produces no modal dialog or foreground activation, logs its initial connection failure only once, and continues waiting.
- [x] Starting the target after OBS-PT begins capture automatically; exiting and restarting it, including with a changed title under a fallback match mode, restores capture.
- [x] Inactive Sources do not continue periodic reconnect work.
- [x] Destroying the Source or stopping recording while connected, disconnected, or reconnecting completes without a hang.
- [x] One deterministic audio process can be recorded from mixer 2 into Hybrid MP4 and decoded with the expected isolated signal.
- [x] Source name, window, and priority strings are present for en-US, zh-CN, zh-TW, ja-JP, and ko-KR, and the Source has a packaged icon.
- [x] The supported VS2022 v143 RelWithDebInfo build succeeds and the Source loads from a runtime tree outside the plugin build directory.

## Blocked by

- `002-share-windows-window-matching.md`
