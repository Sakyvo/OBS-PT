# fix multi-track recording stop hang - design

## Boundary

This task fixes the advanced recording stop path for normal file recording that
uses `ffmpeg_muxer` (`adv_file_output`). The implementation boundary is
`libobs/obs-output.c` for the interleaver root cause plus
`plugins/obs-ffmpeg/obs-ffmpeg-mux.c` for forced-stop fallback, with read-only
verification of the UI/output callers in `UI/window-basic-main.cpp` and
`UI/window-basic-main-outputs.cpp`.

Program-independent audio capture is not part of this MVP unless new evidence
shows it is the same failure. The current repository does not contain the newer
OBS application-audio-capture source implementation, and the available logs show
the hang as a missing output stop completion rather than a WASAPI teardown
failure.

## Root Cause Hypothesis

The UI is waiting for the `stop` signal from libobs. For `ffmpeg_muxer`, that
signal is reached when `deactivate()` calls `obs_output_end_data_capture()`.
Current normal stop only sets `stream->stopping`, stores `stream->stop_ts`, and
turns `capturing` off. The muxer then waits for `ffmpeg_mux_data()` to receive a
later packet whose `packet->sys_dts_usec >= stop_ts`.

Multi-track advanced recording increases the chance that libobs interleaving
does not deliver such a final packet after stop. `send_interleaved()` requires a
higher opposing timestamp before releasing a buffered packet, while this branch
tracks a single `highest_audio_ts` across all audio tracks. If one audio track or
video stream stops producing the needed opposing timestamp, the muxer never
reaches the `deactivate()` call, so the UI remains in the stopping state and
process shutdown waits on an output that is still active.

Upstream 28.1.0 still has the same fragile high-FPS multi-audio startup pruning
behavior as this fork. Upstream 32.1.2 keeps the muxer stop pattern but fixes a
matching interleaver issue by using the max audio encoder frame duration as the
initial sync tolerance when multiple audio encoders are active. That is the
preferred root-cause fix for OBS-PT's default 480 fps recording mode.

## Chosen Fix

Backport the narrow libobs interleaver tolerance fix:

- Add a small helper that computes an encoder frame duration in microseconds
  from `timebase_num`, `timebase_den`, and `framesize`.
- In `prune_premature_packets()`, track the number of active audio encoders and
  the maximum audio encoder duration.
- If more than one audio encoder is active and the video frame duration is
  shorter than the maximum audio duration, use the audio duration as the pruning
  threshold.

This keeps the initial interleaver from waiting forever for a tight sync point
that is unrealistic at 480 fps with multiple AAC encoders.

Preserve the natural muxer stop behavior for the first stop request:

- `obs_output_stop()` passes a nonzero timestamp.
- `ffmpeg_mux_stop()` records `stop_ts`, marks `stopping`, clears `capturing`,
  and allows a later packet to trigger `deactivate(stream, 0)`.

Make forced stop complete the muxer immediately:

- `obs_output_force_stop()` passes `ts == 0`.
- `ffmpeg_mux_stop()` should treat `ts == 0` as a hard completion request for an
  active/stopping muxer.
- On that path, call `deactivate(stream, 0)` directly, so the external mux helper
  pipe is destroyed, the file trailer path runs in the helper cleanup, and
  `obs_output_end_data_capture()` fires without waiting for another encoded
  packet.

This matches the existing UI/output contract: `OBSBasic::StopRecording()` passes
`recordingStopping` to `AdvancedOutput::StopRecording(force)`, so clicking stop a
second time while stuck already asks libobs for a force stop. The missing part is
that `ffmpeg_muxer` does not currently make `ts == 0` complete immediately.

## Compatibility Notes

- Single-track recording should not hit the relaxed interleaver threshold and
  should keep the current log order and playable MP4 behavior.
- Forced stop may truncate to the packets already written to the mux helper. That
  is acceptable for a user-requested second stop and preferable to an orphaned
  process.
- The fix should be idempotent. Repeated forced stops after `deactivate()` must
  not double-destroy `stream->pipe` or emit duplicate completion signals.
- Replay buffer and HLS share `ffmpeg_mux_stop()`, so the forced-stop fallback
  must remain idempotent and only change behavior for `ts == 0`.

## Rollback Shape

The patch should be small and isolated. Reverting the muxer forced-stop change
should restore current behavior without requiring config migrations or user-data
changes.

## Validation Strategy

- Build the smallest relevant OBS-PT target that compiles both touched areas:
  `obs-ffmpeg`. It rebuilds `libobs` as a dependency and the plugin DLL. The
  similarly named `obs-ffmpeg-mux` target builds only the external helper
  executable and is not sufficient.
- Manual runtime validation should use the current `D:/OBS-PT` user data so the
  user's Profile, Scene Collection, and layout remain representative.
- Validate tracks 1+2, repeated stop while stopping, exit after stop, and a
  single-track regression case.
- Inspect logs for `Output of file ... stopped`, `Output 'adv_file_output':
  stopping`, and `==== Recording Stop ====`.
