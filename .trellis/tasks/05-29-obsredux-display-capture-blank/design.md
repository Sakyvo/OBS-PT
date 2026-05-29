# Display Capture Blank Output Design

## Boundary

The runtime source is `plugins/win-capture/duplicator-monitor-capture.c`. On
Windows 10 with D3D11 it renders through `libobs-d3d11/d3d11-duplicator.cpp`
using DXGI Desktop Duplication, with WGC as the alternate method.

This task does not alter scene defaults or Game Capture. The existing scene
evidence points to a backend texture problem, so the diagnostic boundary is the
source backend.

## Failure Model

The current code can produce a blank source without a useful log when:

- `gs_duplicator_create()` returns `NULL`.
- `gs_duplicator_update_frame()` returns false and capture data is freed.
- update succeeds but `gs_duplicator_get_texture()` returns `NULL`.
- WGC initialization returns `NULL` or becomes inactive.

`device_duplicator_create()` currently emits failed HRESULTs at `LOG_DEBUG`,
which normal tester logs do not reliably expose. The source render path also
returns early when `duplicator` or `texture` is missing.

## Implementation Shape

- Add one-shot state flags to `struct duplicator_capture` so source-level logs
  are visible but not spammy.
- Log selected monitor, resolved DXGI index, and method when settings update.
- Log first DXGI duplicator create success/failure, first update loss, first
  texture availability, and first render-time missing texture/duplicator.
- Log WGC module availability/init failure/inactive state.
- Raise D3D11 duplicator creation failure logs from `LOG_DEBUG` to
  `LOG_WARNING`, preserving the HRESULT text.
- Add low-noise texture format diagnostics in the D3D11 copy path to verify
  whether the desktop duplication texture is created and copied.

The first pass is diagnostic by design. If logs identify a specific failing
stage, the fix can be narrowed without guessing.
