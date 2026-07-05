# Design: Port Hybrid MP4 recording

## Summary

OBS-PT currently records PotPvP directly to ordinary MP4 through the FFmpeg
muxer. That keeps the user workflow simple, but ordinary MP4 can be lost if
recording is interrupted before the final `moov` metadata is written.

The target design is to port OBS Studio's native Hybrid MP4 output from the OBS
30.2 implementation into this OBS 27-based fork. Hybrid MP4 writes a fragmented
MP4 while recording and soft-remuxes it on normal stop so the final file behaves
like a regular MP4. The first slice covers normal recording only, not Replay
Buffer, Hybrid MOV, or chapter-marker UX.

## Architecture

### Native Output

Add the upstream OBS Hybrid MP4 output to `plugins/obs-outputs`:

- `mp4-mux-internal.h`
- `mp4-mux.c`
- `mp4-mux.h`
- `mp4-output.c`

Register the output as `mp4_output` from
`plugins/obs-outputs/obs-outputs.c::obs_module_load()`.

The preferred implementation source is upstream PR
`obsproject/obs-studio#10608` because it is the original OBS 30.2 Hybrid MP4
landing change and is smaller than porting the later OBS 32.0 Hybrid MOV work.

### Dependencies

The OBS 30.2 output uses:

- `opts-parser`: already present in OBS-PT under `deps/opts-parser/`.
- `util/buffered-file-serializer`: not present in OBS-PT.

Prefer backporting `buffered-file-serializer.{c,h}` into `libobs/util/` and
adding it to `libobs/CMakeLists.txt`. This keeps `mp4-output.c` close to
upstream and avoids rewriting output I/O around older serializer helpers.

If the buffered serializer depends on newer libobs APIs that are too invasive,
the fallback design is to adapt `mp4-output.c` to OBS-PT's existing
`file-output-serializer` APIs, but that is less desirable because it creates a
local fork of the upstream output.

### Output Selection

Keep legacy containers visible:

- `mp4`: ordinary MP4 through `ffmpeg_muxer`
- `mov`: ordinary MOV through `ffmpeg_muxer`
- `mkv`
- `flv`
- new `hybrid_mp4`: Hybrid MP4 through `mp4_output`

OBS-PT's settings UI stores the recording format in `AdvOut.RecFormat` /
`SimpleOutput.RecFormat`; it does not have newer OBS's `RecFormat2` migration
layer. For this task, use the existing config keys and add the `hybrid_mp4`
value.

When the selected recording format is `hybrid_mp4`:

- create `mp4_output` instead of `ffmpeg_muxer` for normal file recording;
- generate the output file extension as `mp4`, not `hybrid_mp4`;
- keep the configured format value as `hybrid_mp4` so Settings can round-trip
  the selection;
- pass the generated path to `mp4_output` via its `path` setting.

Do not route Replay Buffer to `mp4_output` in this slice.

### Defaults

Change shipped PotPvP defaults:

- `UI/data/obspt-defaults/obs-studio/basic/profiles/PotPvP/basic.ini`
  `AdvOut.RecFormat=hybrid_mp4`
- `AutoRemux=false` remains valid because Hybrid MP4 keeps direct `.mp4` output.

Because OBS-PT has not been publicly released, there is no existing-install
migration requirement. Fresh package defaults and first-run fresh bootstrap must
write Hybrid MP4. If bootstrap writes any recording format defaults outside the
shipped `basic.ini`, keep those values in sync.

### Failure Behavior

If `mp4_output` cannot be created or `obs_output_start(fileOutput)` fails:

- do not silently create ordinary MP4 through `ffmpeg_muxer`;
- show the recording start failure dialog;
- include user-facing guidance that the user can switch the recording format
  back to ordinary MP4 in Settings as a workaround;
- log enough context to distinguish `mp4_output` creation/start failure from an
  encoder failure.

This preserves the safety contract: ordinary MP4 remains available, but only as
an explicit user choice.

### UI / Localization

OBS-PT currently has static format combo items in
`UI/forms/OBSBasicSettings.ui`, not the modern dynamic `LoadFormats()` list used
by the upstream PR. Add a `hybrid_mp4` item to the simple and advanced recording
format combos using the existing OBS-PT UI pattern.

Add locale strings at least for `en-US` and `zh-CN`:

- display label: "Hybrid MP4 (.mp4)" / equivalent Chinese wording
- failure hint: tell the user they can switch the recording format to ordinary
  MP4 if Hybrid MP4 fails

Plain MP4/MOV unrecoverable warning should not show for `hybrid_mp4`. Ordinary
MP4/MOV warnings should remain unchanged.

### Compatibility

The first slice targets the current OBS-PT encoder set and PotPvP defaults:

- H.264 video (`jim_nvenc`, `amd_amf_h264`, QSV, x264 fallback)
- AAC audio
- multi-audio-track recording
- 480fps PotPvP video defaults

Do not broaden scope to AV1/HEVC/MOV/PCM unless required by compile-time
compatibility with the ported output.

## Validation Strategy

Local validation:

- build `obs-outputs`, `obs`, and installer staging;
- verify `mp4_output` registers in logs or by output enumeration;
- verify PotPvP default config writes `hybrid_mp4` but generated files use
  `.mp4`;
- record and stop a short Hybrid MP4 file; verify it plays;
- record with multiple audio tracks and verify tracks are present;
- force-interrupt a short recording after packets are written and verify the
  incomplete file is recoverable/playable as fragmented MP4;
- select ordinary MP4 manually and verify it still uses `ffmpeg_muxer`;
- regenerate and integrity-check the installer.

External tester validation:

- fresh install uses Hybrid MP4 by default;
- PotPvP 480fps recording starts/stops successfully;
- crash/forced-close scenario leaves a recoverable file;
- if Hybrid MP4 fails, the dialog points to ordinary MP4 as a manual workaround.

## Risks

- The native MP4 muxer is large, low-level container code; small backport errors
  can create subtly invalid files.
- `buffered-file-serializer` may depend on newer libobs utility behavior.
- OBS 32.0 fixed Hybrid MP4 splitting bugs after the 30.2 landing PR; since
  Replay Buffer/file splitting is out of scope, we should avoid enabling split
  features until the normal recording path is proven.
- The UI format config differs from newer OBS (`RecFormat` vs `RecFormat2`), so
  direct cherry-pick will not apply cleanly.
- Interrupted-recording validation is inherently destructive and must be run on
  a disposable process/output path.

## Rollback

If Hybrid MP4 cannot be stabilized within this task:

- keep ordinary MP4 as the shipped default for that build;
- remove or hide the `hybrid_mp4` UI option;
- document the specific blocker in the task PRD;
- do not ship a default that appears safer but silently falls back to ordinary
  MP4.
