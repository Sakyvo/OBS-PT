# Implementation Plan

1. Start the Trellis task after artifacts are written.
2. Add low-noise Display Capture diagnostics in `duplicator-monitor-capture.c`.
3. Promote D3D11 duplicator creation failures and texture-copy diagnostics in
   `d3d11-duplicator.cpp`.
4. Build `libobs-d3d11` and `win-capture`.
5. Sync the updated runtime DLL/PDB files to
   `K:/Projects/finished/OBS-Redux`.
6. Ask the tester to launch OBSRedux, select Display Capture, and provide the
   new log if the preview is still blank.
7. Run Trellis check/finish steps and commit only files changed for this task.

## Validation

- `cmake --build build64 --config RelWithDebInfo --target libobs-d3d11 win-capture`
- Confirm updated DLL timestamps in `K:/Projects/finished/OBS-Redux`.
- Inspect new log lines for `duplicator-monitor-capture` and
  `device_duplicator_create`.
