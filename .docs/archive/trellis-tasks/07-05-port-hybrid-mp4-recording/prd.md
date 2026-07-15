# Port Hybrid MP4 recording

## Goal

Port OBS Studio's newer Hybrid MP4 recording format into OBS-PT so PotPvP
recordings keep the `.mp4` workflow while becoming recoverable if recording is
interrupted before normal MP4 finalization.

The user value is direct: OBS-PT currently defaults to plain MP4, which is easy
to lose after crashes, power loss, forced process termination, encoder stalls,
or stop-path bugs. Hybrid MP4 should reduce that failure mode without forcing
users into MKV plus remux as the default workflow.

## Confirmed Facts

### Upstream OBS evidence

- OBS's official Hybrid MP4 KB says Hybrid MP4 was introduced in OBS 30.2 and
  Hybrid MOV in OBS 32.0. It describes the format as writing a fragmented file
  during recording, then finalizing it through "soft-remux" so compatible
  software treats it like a regular MP4/MOV. Source:
  `https://obsproject.com/kb/hybrid-mp4`.
- OBS's official blog explains the MP4 failure mode: normal MP4 writes the
  required `moov` metadata at finalization time; if writing is interrupted, the
  file can be extremely hard or impossible to recover. Hybrid MP4 records as
  fragmented MP4 while running, then writes a full `moov` and rewrites the early
  placeholder into one big `mdat` during soft-remux. Source:
  `https://obsproject.com/blog/obs-studio-hybrid-mp4`.
- OBS Studio 30.2 release notes list "Hybrid MP4" as a new beta output format
  and say it combines fragmented MP4 fault-tolerance with regular MP4
  compatibility/faster access. Source:
  `https://github.com/obsproject/obs-studio/releases/tag/30.2.0`.
- OBS Studio 32.0 release notes say Hybrid MOV was added, Hybrid MP4/MOV became
  the default containers for new profiles, and Hybrid MP4 file splitting bugs
  were fixed. Source:
  `https://github.com/obsproject/obs-studio/releases/tag/32.0.0`.
- Upstream PR `obsproject/obs-studio#10608` is the fast-port candidate for
  Hybrid MP4. The GitHub API reports 18 changed files, 3932 additions, and 3
  deletions. The core added files are:
  - `plugins/obs-outputs/mp4-mux-internal.h` (345 additions)
  - `plugins/obs-outputs/mp4-mux.c` (2843 additions)
  - `plugins/obs-outputs/mp4-mux.h` (43 additions)
  - `plugins/obs-outputs/mp4-output.c` (613 additions)
  It also wires `mp4_output` into `plugins/obs-outputs/obs-outputs.c`, adds
  `hybrid_mp4` format UI entries, maps `hybrid_mp4` to `.mp4`, and routes file
  output creation from `ffmpeg_muxer` to `mp4_output` when the selected format
  is `hybrid_mp4`. Source:
  `https://github.com/obsproject/obs-studio/pull/10608`.

### OBS-PT local evidence

- The shipped PotPvP Profile currently writes plain MP4:
  `UI/data/obspt-defaults/obs-studio/basic/profiles/PotPvP/basic.ini:20-26`
  has `RecType=Standard`, `RecEncoder=jim_nvenc`, `RecFormat=mp4`, and
  `AutoRemux=false`.
- Standard simple recording currently creates `ffmpeg_muxer`:
  `UI/window-basic-main-outputs.cpp:456-458`.
- Standard advanced recording currently creates `ffmpeg_muxer`:
  `UI/window-basic-main-outputs.cpp:1238-1240`.
- Advanced recording start uses `AdvOut.RecFormat` as both the generated file
  extension and the output setting `path`:
  `UI/window-basic-main-outputs.cpp:1840-1865`.
- Stop/force-stop still goes through `obs_output_stop` /
  `obs_output_force_stop`:
  `UI/window-basic-main-outputs.cpp:1971-1977`.
- Current `plugins/obs-outputs/obs-outputs.c:17-72` registers RTMP, null, FLV,
  and optionally FTL outputs only; no `mp4_output` exists in OBS-PT.
- OBS-PT's existing `plugins/obs-ffmpeg/obs-ffmpeg-mux.c:588-600` registers
  `ffmpeg_muxer`; the previous high-FPS stop-hang task added a force-stop path
  in `ffmpeg_mux_stop()` at `plugins/obs-ffmpeg/obs-ffmpeg-mux.c:416-430`.
- The OBS 30.2 PR depends on `opts-parser`; OBS-PT already has
  `deps/opts-parser/` and links it from existing plugins.
- The OBS 30.2 PR includes `<util/buffered-file-serializer.h>`; OBS-PT has
  `libobs/util/file-serializer.*` and `array-serializer.*`, but no
  `buffered-file-serializer.*`. That dependency must be backported or the new
  MP4 output must be adapted to the older serializer utilities.
- OBS-PT's recording format combo items are currently static `.ui` entries
  (`UI/forms/OBSBasicSettings.ui` includes `flv`, `mp4`, `mov`, `mkv`) rather
  than the newer OBS dynamic `LoadFormats()` format-list approach. The upstream
  UI patch is therefore not a direct drop-in.

### Related task history

- Archived task
  `.trellis/tasks/archive/2026-06/06-27-fix-multitrack-recording-stop-hang`
  fixed high-FPS multi-track MP4 stop hangs in the existing FFmpeg muxer path.
  It did not change the container safety model; it kept plain MP4 output.

## Requirements

- MVP scope is Hybrid MP4 only:
  - Add/port the native Hybrid MP4 output path.
  - Make PotPvP default recording use Hybrid MP4 while keeping `.mp4` output.
  - Keep ordinary MP4 visible/selectable in Settings as the explicit manual
    workaround if Hybrid MP4 fails on a machine.
  - Do not include Hybrid MOV in the first slice.
  - Do not switch Replay Buffer to Hybrid MP4 in the first slice. Re-evaluate
    Replay Buffer separately after normal recording with Hybrid MP4 is stable.
  - Do not include chapter-marker UI/hotkeys in the first slice unless required
    only as a compile-time stub for upstream compatibility.
- Preserve OBS-PT's direct `.mp4` user workflow for PotPvP recording.
- Reduce the risk of unrecoverable MP4 files when recording is interrupted
  before normal stop/finalization.
- OBS-PT has not been publicly released yet, so this task does not need to
  migrate existing installed user profiles. The required default change is for
  the shipped/new-install PotPvP Profile and any first-run bootstrap path that
  writes fresh defaults.
- Prefer upstream OBS's native Hybrid MP4 implementation over inventing a new
  container format or relying on FFmpeg muxer flags alone.
- Keep OBS-PT's 480fps PotPvP defaults intact.
- Keep existing encoder selection/default behavior intact unless Hybrid MP4
  requires a narrow format/output routing change.
- Build and package any new output code into the Windows installer.
- If Hybrid MP4 output creation or start fails, OBS-PT must not silently fall
  back to ordinary MP4. It must show the normal "Start Recording Failed" path
  and include user-facing guidance that they can switch the recording format
  back to ordinary MP4 as a workaround.
- If a fast-port is not feasible, document the blocker and choose the safest
  fallback explicitly rather than silently staying on plain MP4.

## Acceptance Criteria

- [ ] OBS-PT registers an `mp4_output` or equivalent native Hybrid MP4 output
      at runtime.
- [ ] PotPvP first-run/default recording can use Hybrid MP4 while producing a
      `.mp4` file extension.
- [ ] The generated installer ships new-install defaults with PotPvP recording
      set to Hybrid MP4.
- [ ] Settings -> Output can represent the selected Hybrid MP4 format without
      losing existing legacy MP4/MOV/MKV behavior; ordinary MP4 remains
      available as a manual fallback.
- [ ] Starting and stopping a normal PotPvP recording produces a playable MP4.
- [ ] If Hybrid MP4 fails to initialize/start, OBS-PT shows a recording-start
      failure dialog that tells the user they can switch to ordinary MP4; it
      does not start recording through ordinary MP4 automatically.
- [ ] A simulated interrupted recording leaves a recoverable fragmented MP4
      rather than a normal MP4 missing required final metadata.
- [ ] Multi-audio-track recording remains supported.
- [ ] 480fps recording remains configured and does not regress the previous
      stop-hang fixes.
- [ ] New build artifacts include the output plugin changes.
- [ ] A fresh installer is generated and integrity-checked before tester
      handoff.

## Likely Out of Scope

- Porting OBS 32.0 Hybrid MOV support.
- Implementing chapter-marker UI/hotkeys as part of the first slice, except for
  any minimal internal stub required to compile the upstream Hybrid MP4 output.
- Reworking OBS-PT's whole output settings UI to match modern OBS.
- Replacing the existing FFmpeg muxer for every container type.
- Switching Replay Buffer output to Hybrid MP4.
- Changing encoder defaults, bitrate/CQP defaults, or 480fps capture defaults.
