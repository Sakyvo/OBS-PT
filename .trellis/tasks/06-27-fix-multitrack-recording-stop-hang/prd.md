# fix multi-track recording stop hang

## Goal

Fix the OBS-PT recording stop hang when advanced recording uses more than one
audio track. Stopping a multi-track recording must complete visibly in the UI,
leave a playable MP4, and allow OBS-PT to exit without an orphaned process.

## User Report

- With multiple recording audio tracks selected, clicking stop recording changes
  the button to the stopping state and then appears stuck.
- Force-closing the window only closes the UI window; the OBS-PT process remains
  alive in the taskbar.
- The recording file appears to have already stopped and the latest MP4 files in
  `D:\OBS-PT\recordings` are not damaged after the forced close.
- The user suspects this may be related to future "program independent audio
  capture" work and wants the investigation to determine whether that should be
  part of this task or a follow-up task.
- Scope decision from user: the MVP is the multi-track recording stop/exit hang.
  Program-independent audio capture is included only if implementation evidence
  proves it shares the same root cause.

## Confirmed Facts

- ACE code search was attempted first but returned HTTP 502, so local source
  search was used as the fallback.
- The UI stop flow is in `UI/window-basic-main.cpp`:
  - `OBSBasic::StopRecording()` calls `outputHandler->StopRecording(recordingStopping)`.
  - `OBSBasic::RecordStopping()` sets the button text to the stopping state and
    sets `recordingStopping = true`.
  - `OBSBasic::RecordingStop()` resets the button text/check state, logs
    `==== Recording Stop ====`, emits `OBS_FRONTEND_EVENT_RECORDING_STOPPED`,
    and then runs `AutoRemux(...)`.
- Advanced recording is configured in `UI/window-basic-main-outputs.cpp`:
  - Normal advanced file recording creates `ffmpeg_muxer` as `adv_file_output`.
  - `AdvancedOutput::SetupRecording()` reads `AdvOut.RecTracks` and attaches
    each selected AAC track to consecutive output audio encoder slots.
  - `AdvancedOutput::StopRecording(force)` calls `obs_output_stop(fileOutput)`
    or `obs_output_force_stop(fileOutput)` on repeated stop.
- libobs output stopping is in `libobs/obs-output.c`:
  - `obs_output_stop()` emits `stopping` and calls `obs_output_actual_stop(..., false, ts)`.
  - Completion depends on the output implementation eventually calling
    `obs_output_end_data_capture()` or `obs_output_signal_stop()`, which then
    emits the `stop` signal consumed by the UI.
  - Encoder shutdown runs in `end_data_capture_thread()` and stops the video
    encoder plus all active audio encoders.
- Normal advanced MP4 recording uses `plugins/obs-ffmpeg/obs-ffmpeg-mux.c`:
  - `ffmpeg_mux_stop()` only marks `stopping` and records `stop_ts`.
  - The muxer actually deactivates in `ffmpeg_mux_data()` when a later encoded
    packet has `packet->sys_dts_usec >= stop_ts`.
  - If no later encoded packet reaches the muxer after stop, `deactivate()` is
    not called, so the libobs `stop` signal may never reach the UI.
  - `deactivate()` destroys the external `obs-ffmpeg-mux.exe` pipe, logs
    `Output of file ... stopped`, and calls `obs_output_end_data_capture()`.
- The external mux helper `plugins/obs-ffmpeg/ffmpeg-mux/ffmpeg-mux.c` supports
  multiple audio tracks and writes the trailer during cleanup.
- OBS 32.1.2 contains an interleaver fix for multiple audio encoders at high
  frame rates: when more than one audio encoder is active and the video frame
  duration is shorter than the audio encoder frame duration, the interleaver
  relaxes its initial synchronization window to the audio frame duration. This
  directly matches OBS-PT's 480 fps default, where a video frame is far shorter
  than AAC's frame duration.
- Current repository does not contain the newer OBS application-audio-capture
  source implementation; only system WASAPI input/output sources are registered
  by `plugins/win-wasapi`.
- WASAPI source destruction waits on capture/reconnect threads, but the recent
  local logs show normal `WASAPI ... Terminated` during successful shutdown.
- Local log evidence:
  - Single-track recordings show the expected stop sequence:
    `Output of file ... stopped` -> `Output 'adv_file_output': stopping` ->
    `==== Recording Stop ====`.
  - `D:\OBS-PT\obs-studio\logs\2026-06-25 12-38-27.txt` contains a two-track
    recording start (`Track1` + `Track2`) at `12:39:08`, then pause/unpause, then
    shutdown at `12:42:11` with no `Output of file ... stopped` and no
    `==== Recording Stop ====`.
  - The matching multi-track MP4 is not present in the current
    `D:\OBS-PT\recordings` listing, so the current machine does not preserve the
    exact files mentioned in the user report.

## Requirements

- Multi-track advanced recording must stop reliably for at least tracks 1+2.
- The record button must leave the stopping state after recording stop completes.
- Closing OBS-PT after a failed or repeated stop attempt must not leave a
  taskbar/process orphan.
- The fix must preserve existing single-track recording behavior and playable
  MP4 output.
- The investigation must identify whether the problem is in the muxer stop
  trigger, audio encoder shutdown, AutoRemux/UI post-stop handling, WASAPI
  source shutdown, or a newer application-audio-capture gap.
- If evidence shows the root cause is not application-audio-capture, keep
  application-audio-capture completion out of this bugfix and record it as a
  follow-up planning task.

## Out of Scope

- Completing or redesigning program-independent audio capture, unless the
  multi-track stop hang is proven to originate there.
- Changing recording track selection UX or default audio-track policy.
- Reworking AutoRemux, replay buffer stop behavior, or non-advanced output modes
  unless they are directly affected by the selected fix.
- Adding automatic watchdog/timeout force-stop behavior in the UI. The current
  MVP can use the existing repeated-stop force path as the user-visible
  fallback; automatic force-stop can be planned later if manual validation shows
  it is needed.

## Acceptance Criteria

- [x] With advanced output, MP4 format, NVENC recording, and recording tracks
      1+2 selected, start -> stop returns the UI button to start-recording state.
- [x] The log for the multi-track test contains `Output of file ... stopped`,
      `Output 'adv_file_output': stopping`, and `==== Recording Stop ====`.
- [x] The produced MP4 opens successfully and contains the selected audio tracks.
- [x] Repeating stop while already stopping either forces a clean stop or remains
      harmless; it must not leave the UI permanently stuck.
- [x] Exiting after the stop scenario terminates the OBS-PT process.
- [x] Existing single-track recording still starts, stops, and saves correctly.

## Validation Results

- Built `cmake --build build-v143 --config RelWithDebInfo --target obs-ffmpeg`
  successfully after the `libobs` and `obs-ffmpeg` changes.
- Synced `build-v143/rundir/RelWithDebInfo/bin/64bit/obs.dll` and
  `build-v143/rundir/RelWithDebInfo/obs-plugins/64bit/obs-ffmpeg.dll` into
  `D:/OBS-PT` for runtime validation, keeping timestamped backups of the prior
  DLLs.
- Multi-track validation used Profile `1` with `RecTracks=3` (tracks 1+2) and a
  temporary F13 stop hotkey that was restored afterwards:
  - Log: `D:/OBS-PT/obs-studio/logs/2026-06-27 12-39-02.txt`
  - Recording: `D:/OBS-PT/recordings/2026-06-27 12-39-05.mp4`
  - Log contains Track1, Track2, `Output of file ... stopped`,
    `Output 'adv_file_output': stopping`, and `==== Recording Stop ====`.
  - `ffprobe` reports one H.264 video stream and two AAC audio streams.
- Single-track regression used a temporary `RecTracks=1` plus the same temporary
  F13 stop hotkey, then restored Profile `1`:
  - Log: `D:/OBS-PT/obs-studio/logs/2026-06-27 12-41-54.txt`
  - Recording: `D:/OBS-PT/recordings/2026-06-27 12-41-56.mp4`
  - Log contains the expected stop sequence.
  - `ffprobe` reports one H.264 video stream and one AAC audio stream.
- After tests, no `OBS-PT`/OBS process remained running and Profile `1` was
  restored to `RecTracks=3` with no stop hotkey binding.
- Refreshed installer staging from `build-v143/rundir/RelWithDebInfo/` after the
  runtime smoke pass, then regenerated
  `build-v143/_pkg/OBS-PT-1.0.0-Installer.exe` with NSIS:
  - Timestamp: `2026-06-27 12:59:48`
  - Size: `43298784` bytes
  - SHA256:
    `33E186036C848AEEC73D37D4CAB19437160C6B5D8BB727E1F7CB51B1BFDBEFF0`
  - Staged `obs.dll` and `obs-ffmpeg.dll` SHA256 values match the built
    `rundir` files.

## Open Questions

- None blocking implementation. Build validation should use the `obs-ffmpeg`
  target because it rebuilds both the `libobs` dependency and the
  `plugins/obs-ffmpeg/obs-ffmpeg-mux.c` plugin DLL; the separate
  `obs-ffmpeg-mux` target is only the external helper executable.
