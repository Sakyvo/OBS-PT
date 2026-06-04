# OBSRedux Display Capture Spec

**Status**: Active
**Owner**: OBSRedux Core
**Last Updated**: 2026-05-29

## Scope

Windows Display Capture is the `monitor_capture` source registered from
`plugins/win-capture/duplicator-monitor-capture.c`. On Windows 10 with D3D11 it
uses DXGI Desktop Duplication through `libobs-d3d11/d3d11-duplicator.cpp`, with
WGC as an alternate method.

## Root Cause Note (2026-06-04)

The original "display capture blank" report was NOT a duplicator fault. The
preview/recording was black for **every** source because the render ViewProj
matrix was collapsing to a degenerate matrix — a `cl.exe` `/O2` miscompile of
the vec4 union type-punning in `libobs/graphics/matrix4.c`. The duplicator
diagnostics below confirmed DXGI was producing valid textures the whole time.

> Before chasing this capture path again, verify the global render matrix first.
> See [`obsredux-graphics-msvc.md`](./obsredux-graphics-msvc.md) for the symptom,
> diagnosis, and the MSVC `#pragma optimize` guard. The diagnostic logs in this
> spec remain useful, but a blank capture on the VS 18 2026 toolchain points at
> the math-TU miscompile, not the duplicator.

## Diagnostic Contract

Display Capture must not fail silently when preview or recording has no desktop
texture. The first failing stage should be visible in normal logs:

- selected method, monitor handle, and DXGI monitor index
- DXGI duplicator creation success/failure
- DXGI frame update reset
- first texture availability and texture format
- render skipped because duplicator or texture is missing
- WGC module/export availability and initialization failure

Logs should be one-shot or state-change based so normal rendering does not spam
per-frame messages.

## Good/Base/Bad Cases

- Good: DXGI creates a duplicator, receives a texture, logs the first texture
  dimensions/format, and renders desktop content.
- Base: WGC is unavailable but method resolves to DXGI; logs do not mention WGC
  failure unless WGC is selected.
- Bad: capture returns blank while `gs_duplicator_create`,
  `gs_duplicator_update_frame`, or `gs_duplicator_get_texture` fails without a
  visible log.

## Validation

After changing this path, build at minimum:

```powershell
cmake --build build64 --config RelWithDebInfo --target libobs-d3d11 win-capture -- /m
```

For packaged tester builds, sync:

- `build64/libobs-d3d11/RelWithDebInfo/libobs-d3d11.dll` to `<Install Root>/bin/64bit/`
- `build64/plugins/win-capture/RelWithDebInfo/win-capture.dll` to `<Install Root>/obs-plugins/64bit/`
