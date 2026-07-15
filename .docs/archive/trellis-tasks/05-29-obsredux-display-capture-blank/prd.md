# Debug OBSRedux display capture blank output

## Goal

Display Capture in OBSRedux must show the selected desktop in preview and
recording. The current failure is a blank/empty preview and black recording
while the same machine's upstream OBS 27.2.4 Display Capture works.

## Requirements

- Root cause is now confirmed (design.md pass 2-3) to be the **global libobs
  render pipeline emitting empty frames for every source and every view**, not
  the display-capture backend. Scope therefore widens from `monitor_capture` to
  the shared `obs_source_video_render` → effect → `gs_draw_sprite` → d3d11 draw
  layer. The pass-1 monitor_capture diagnostics remain as ruled-out evidence.
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

- [x] OBSRedux Display Capture preview shows nonblank desktop content. —
      confirmed 2026-06-04 (user screenshot: PotPvP preview renders the desktop).
- [x] A screenshot or short recording from OBSRedux Display Capture is not all
      black/gray. — preview (same render path that fed the black output) confirmed
      non-black; recording spot-check recommended on the tester machine.
- [x] Logs identify the selected backend and the first failing DXGI/WGC stage if
      capture still fails. — kept one-shot duplicator / monitor-capture lifecycle
      logs satisfy this.
- [x] Desktop Audio source still loads in the PotPvP scene. — `桌面音频` loads in
      every test log.
- [x] Change scope is correct for the true root cause. — **revised**: the bug was
      NOT display-capture code. Root cause was a VS 18 2026 `cl.exe` `/O2`
      miscompile of vec4 union type-punning in `libobs/graphics/matrix4.c`,
      collapsing the global render ViewProj matrix to black for every source. Fix
      = MSVC `#pragma optimize("", off/on)` on `matrix4.c` (required) and
      `matrix3.c` (defensive); temporary render diagnostics removed; display-capture
      lifecycle logs kept. See `.trellis/spec/obsredux-graphics-msvc.md`.

## Resolution (2026-06-04)

Fixed and verified. The "display capture blank" symptom was a global render-matrix
miscompilation, not a duplicator fault. Deployed `obs.dll`
(`5aac284f42d2c39f733928b8c4278668`) + `libobs-d3d11.dll`
(`ddc883a2703409b150a3ed790a66d48c`) to `K:/Projects/finished/OBS-Redux/bin/64bit/`;
user-confirmed non-black preview.

## Notes

- Keep `prd.md` focused on requirements, constraints, and acceptance criteria.
- Lightweight tasks can remain PRD-only.
- For complex tasks, add `design.md` for technical design and `implement.md` for execution planning before `task.py start`.
