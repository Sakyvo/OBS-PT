# Implement: Port Hybrid MP4 recording

## Checklist

1. Pre-development context.
   - Run `trellis-before-dev` before editing code.
   - Read `prd.md`, `design.md`, `.trellis/spec/obspt-bootstrap.md`, and the
     relevant output/plugin files.

2. Fetch upstream source evidence.
   - Use upstream OBS PR `10608` / commit `8ac8118b7f0057790e9133117d9080742c47a5b3`
     as the primary Hybrid MP4 source.
   - Retrieve the four native output files:
     - `plugins/obs-outputs/mp4-mux-internal.h`
     - `plugins/obs-outputs/mp4-mux.c`
     - `plugins/obs-outputs/mp4-mux.h`
     - `plugins/obs-outputs/mp4-output.c`
   - Retrieve `libobs/util/buffered-file-serializer.{c,h}` from the same
     upstream era or the closest compatible commit if required.
   - Keep local edits minimal and document any required compatibility changes.

3. Backport dependencies.
   - Add buffered file serializer sources/headers to `libobs/util/`.
   - Update `libobs/CMakeLists.txt`.
   - Confirm `opts-parser` is linked into `obs-outputs`.

4. Add native Hybrid MP4 output.
   - Add the MP4 mux/output files to `plugins/obs-outputs/`.
   - Update `plugins/obs-outputs/CMakeLists.txt`.
   - Update `plugins/obs-outputs/obs-outputs.c` to register `mp4_output`.
   - Add `obs-outputs` locale strings for MP4 output.
   - Build `obs-outputs` and fix compile errors narrowly.

5. Wire recording output selection.
   - Add a helper to map recording format id to file extension:
     `hybrid_mp4 -> mp4`, otherwise existing format id.
   - In standard simple/advanced recording output creation, create
     `mp4_output` when the configured normal recording format is
     `hybrid_mp4`; otherwise keep `ffmpeg_muxer`.
   - Ensure generated recording paths end in `.mp4` for Hybrid MP4.
   - Do not switch Replay Buffer to `mp4_output`.
   - Keep force-stop behavior compatible with `obs_output_force_stop`.

6. Wire Settings UI.
   - Add `hybrid_mp4` entries to simple and advanced recording format combos in
     `UI/forms/OBSBasicSettings.ui` using the existing static-item pattern.
   - Add display/tooltip strings in `UI/data/locale/en-US.ini` and
     `UI/data/locale/zh-CN.ini`.
   - Adjust warning logic so ordinary `mp4`/`mov` still show unrecoverable-file
     warnings, but `hybrid_mp4` does not show the same warning.
   - Preserve ordinary MP4 as a visible selectable fallback.

7. Wire failure behavior.
   - If `mp4_output` creation fails, surface a recording-start failure.
   - If `obs_output_start(fileOutput)` fails while the selected format is
     `hybrid_mp4`, append guidance telling the user to switch to ordinary MP4 as
     a workaround.
   - Add log context for Hybrid MP4 output failures.
   - Do not silently fallback to ordinary MP4.

8. Update defaults.
   - Change shipped PotPvP `AdvOut.RecFormat` from `mp4` to `hybrid_mp4`.
   - Confirm `AutoRemux=false` remains.
   - Search for any bootstrap/default writer that must also emit
     `hybrid_mp4` for fresh defaults.
   - Do not add existing-install migration logic.

9. Validation.
   - Build:
     ```powershell
     cmake --build build-v143 --config RelWithDebInfo --target obs-outputs --parallel
     cmake --build build-v143 --config RelWithDebInfo --target obs --parallel
     ```
   - Static checks:
     ```powershell
     git diff --check
     rg --line-number "hybrid_mp4|mp4_output|buffered_file_serializer" UI plugins libobs .trellis/spec
     ```
   - Runtime checks:
     - Launch from `build-v143/rundir/RelWithDebInfo/bin/64bit/OBS-PT.exe`.
     - Confirm Settings -> Output shows Hybrid MP4 and ordinary MP4.
     - Fresh/default PotPvP recording uses Hybrid MP4 and writes `.mp4`.
     - Short recording starts/stops and plays.
     - Multi-track recording starts/stops and plays with selected tracks.
     - Ordinary MP4 manually selected still records through the legacy path.
     - Simulated interruption leaves a recoverable fragmented MP4.
     - Hybrid MP4 startup failure path shows the ordinary-MP4 workaround text.

10. Packaging.
    - Run install staging:
      ```powershell
      cmake --install build-v143 --config RelWithDebInfo --prefix K:/Projects/dev/OBS-PT/build-v143/_pkg/OBS-PT
      ```
    - Run NSIS:
      ```powershell
      & "C:/Program Files (x86)/NSIS/makensis.exe" "UI/installer/obspt-setup.nsi"
      ```
    - Verify installer timestamp, SHA256, staged `obs-outputs.dll`, default
      `basic.ini`, and `7z t`.

11. Spec and finish.
    - Update `.trellis/spec/obspt-bootstrap.md` with the Hybrid MP4 default and
      installer validation contract if implementation succeeds.
    - Run `trellis-check`.
    - Commit task work in Phase 3.4.

## Risky Files

- `plugins/obs-outputs/mp4-mux.c`
- `plugins/obs-outputs/mp4-output.c`
- `plugins/obs-outputs/CMakeLists.txt`
- `plugins/obs-outputs/obs-outputs.c`
- `libobs/util/buffered-file-serializer.*`
- `libobs/CMakeLists.txt`
- `UI/window-basic-main-outputs.cpp`
- `UI/window-basic-settings.cpp`
- `UI/forms/OBSBasicSettings.ui`
- `UI/data/obspt-defaults/obs-studio/basic/profiles/PotPvP/basic.ini`

## Rollback Points

- If `obs-outputs` cannot compile after adding the native MP4 output, revert the
  `plugins/obs-outputs` and `libobs/util` additions first.
- If the output compiles but produces invalid files, keep ordinary MP4 visible
  and do not change shipped defaults to `hybrid_mp4`.
- If Settings cannot safely round-trip `hybrid_mp4`, hide the UI option and keep
  the task in planning/implementation until config handling is corrected.

## Notes

- This is a complex task; do not start implementation until the user approves
  the PRD/design/implementation plan.
- Inline Codex mode skips JSONL context curation, but Phase 2 must still load
  `trellis-before-dev`.
