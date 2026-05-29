# Debug OBSRedux display capture blank output

## Goal

Display Capture in OBSRedux must show the selected desktop in preview and
recording. The current failure is a blank/empty preview and black recording
while the same machine's upstream OBS 27.2.4 Display Capture works.

## Requirements

- Scope is limited to Windows `monitor_capture` / Display Capture.
- Lunar Client / Game Capture is explicitly out of scope for this task.
- Do not change PotPvP preset scene structure unless the display backend proves
  the scene file is the cause.
- Add temporary or low-noise diagnostics where the current capture backend fails
  silently: DXGI duplicator creation, frame update, texture availability, WGC
  initialization, and monitor index selection.
- Keep Desktop Audio present and do not regress the PotPvP first-run preset.
- Build and sync the affected runtime binaries to
  `K:/Projects/finished/OBS-Redux` for tester verification.

## Confirmed Facts

- OBSRedux log initializes `[duplicator-monitor-capture: 'Display']` with
  display 1 at 1920x1080 and DXGI/WGC setting changes, but reports no backend
  failure.
- Current installed `PotPvP.json` has `Display` visible and above `Minecraft`;
  a hidden game source is not covering the display source.
- `win-capture.dll`, `libobs-d3d11.dll`, and `libobs-winrt.dll` are present in
  the current portable package.
- Upstream OBS 27.2.4 on the same machine loads `monitor_capture` through DXGI
  and can record the display, so the hardware/OS path is generally capable.
- The OBS 27.2.4-era code returns silently when no DXGI duplicator or texture is
  available, and `device_duplicator_create` logs creation failures only at
  `LOG_DEBUG`.

## Acceptance Criteria

- [ ] OBSRedux Display Capture preview shows nonblank desktop content.
- [ ] A screenshot or short recording from OBSRedux Display Capture is not all
      black/gray.
- [ ] Logs identify the selected backend and the first failing DXGI/WGC stage if
      capture still fails on the tester machine.
- [ ] Desktop Audio source still loads in the PotPvP scene.
- [ ] Only display-capture-related code and task artifacts are changed.

## Notes

- Keep `prd.md` focused on requirements, constraints, and acceptance criteria.
- Lightweight tasks can remain PRD-only.
- For complex tasks, add `design.md` for technical design and `implement.md` for execution planning before `task.py start`.
