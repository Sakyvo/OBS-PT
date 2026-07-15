# fix multi-track recording stop hang - implement

## Preconditions

- Do not implement until the user approves these artifacts and
  `python ./.trellis/scripts/task.py start 06-27-fix-multitrack-recording-stop-hang`
  has been run.
- Before editing, load `trellis-before-dev` and read the task artifacts again.
- ACE search returned HTTP 502 during planning; if it remains unavailable during
  implementation, document the fallback to local `rg` in the session notes.

## Implementation Checklist

1. Re-read the exact target code before editing:
   - `plugins/obs-ffmpeg/obs-ffmpeg-mux.c`
   - `libobs/obs-output.c`
   - `UI/window-basic-main-outputs.cpp`
   - `UI/window-basic-main.cpp`
2. Patch `libobs/obs-output.c` so `prune_premature_packets()` handles multiple
   audio encoders at high frame rates:
   - Add a local `get_encoder_duration()` helper.
   - Track active audio encoders and the max audio frame duration.
   - When multiple audio encoders are active and video frame duration is shorter
     than the audio frame duration, use the audio frame duration as the pruning
     threshold.
3. Patch `ffmpeg_mux_stop(void *data, uint64_t ts)` so `ts == 0` forces
   completion for an active/stopping muxer:
   - If the muxer is not active and not capturing, return without emitting extra
     stop signals.
   - If `ts == 0` and the muxer is active, set `stopping = true`,
     `capturing = false`, and call `deactivate(stream, 0)` immediately.
   - Keep the nonzero timestamp path equivalent to the current natural stop
     behavior.
4. Check idempotence:
   - `deactivate()` already guards pipe destruction with `active(stream)`.
   - Confirm `obs_output_end_data_capture()` is only called when `stopping` is
     still true.
   - Confirm repeated force-stop after completion is harmless.
5. Do not alter program-independent audio capture in this task unless new code
   evidence proves it is the same root cause.
6. Do not change recording defaults, audio-track UI, AutoRemux, or replay buffer
   behavior unless required by compilation or a direct regression.

## Validation Commands

Use the existing VS2022 BuildTools build tree:

```powershell
cmake --build build-v143 --config RelWithDebInfo --target obs-ffmpeg
```

If that target is unavailable or does not update `rundir`, use the full solution
target:

```powershell
cmake --build build-v143 --config RelWithDebInfo
```

After runtime validation, refresh package staging and regenerate the installer:

```powershell
& "C:\Program Files (x86)\NSIS\makensis.exe" "UI\installer\obspt-setup.nsi"
```

Manual runtime checks:

1. Launch the OBS-PT build with the current `D:/OBS-PT` configuration.
2. Advanced output, MP4, NVENC, tracks 1+2 selected: start recording, then stop.
3. If the first stop does not complete quickly, click stop again to exercise the
   force path.
4. Confirm the button returns to start-recording state and OBS-PT exits without
   leaving a process.
5. Confirm the latest MP4 opens and contains the selected audio tracks.
6. Repeat with a single selected track.
7. Review the latest log for:
   - `Output of file ... stopped`
   - `Output 'adv_file_output': stopping`
   - `==== Recording Stop ====`

## Risk Points

- Forcing muxer deactivate before every stream has delivered a post-stop packet
  can drop trailing buffered packets. Limit this behavior to `ts == 0` forced
  stop, not normal first stop.
- Destroying the helper pipe is what lets the mux helper write trailer data
  during cleanup. Avoid replacing that with only `obs_output_signal_stop()`, or
  MP4 finalization may regress.
- Avoid broad libobs interleaver changes in this MVP. They have a larger blast
  radius across streaming, replay buffer, and other encoded outputs.
- Keep the libobs interleaver change to the upstream-proven duration threshold.
  Do not backport unrelated multi-video or packet timing refactors from 32.1.2.

## Follow-up Checks Before Finish

- If validation shows the first normal stop still hangs until a second force
  stop, decide whether a UI watchdog should be planned as a separate task.
- If logs implicate source teardown rather than muxer completion, reopen the PRD
  and create a separate program-independent audio capture task instead of
  expanding this one silently.
- After implementation and validation, run `trellis-check`, then decide whether
  the forced-stop muxer contract should be captured in `.trellis/spec/`.
