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

## Root-Cause Narrowing (2026-05-31, diagnostic pass 1 results)

Diagnostic build deployed. Log `2026-05-31 11-01-30.txt` + recording
`2026-05-31 11-01-34.mp4` analyzed. Results:

**Confirmed working (ruled out):**
- DXGI duplicator creates successfully; `gs_duplicator_update_frame: texture
  ready 1920x1080 dxgi=B8G8R8A8_UNORM(87) gs=21 generalized=5` — texture is
  created and `CopyResource` runs.
- Render path always has a texture: NO `render skipped: ... no texture` /
  `no duplicator` warning ever fires → `duplicator_capture_render` reaches the
  `gs_draw_sprite` loop.
- D3D11 texture/SRV path is sound: GS_BGRA → `B8G8R8A8_TYPELESS` resource, both
  `shaderRes` (UNORM) and `shaderResLinear` (UNORM_SRGB) views created (texture
  create would have thrown + logged "failed to create copy texture" otherwise).
- Base effects present + valid (`opaque.effect`, `default.effect`,
  `format_conversion.effect`); no effect compile/load errors in log.
- `reset_duplicators()` runs every frame (`device_begin_frame`) → texture not
  frozen.
- Scene transform correct: `Display` item pos (-1,-1), scale 1.0,
  bounds_type=0, visible, top of z-order → covers full 1920x1080 canvas. Not an
  occlusion/zero-size problem.

**Hard evidence of black/empty output:**
- Recording 287 KB / 1994 frames ≈ 144 B/frame → H.264-compressed uniform black.
- Preview reported "completely empty / not even black" (gray) — suggests the
  scene composite is **transparent (alpha 0)**, not opaque-black: preview gray
  background shows through, while NV12 recording (no alpha) collapses to black.
  This reconciles "gray preview + black recording": the Display source is
  contributing NO opaque pixels to the scene.

**Anomaly (unexplained):** duplicator destroyed+recreated every ~1.5 s
("texture ready" logged twice for one source) with NO update-failed warning →
`free_capture_data` via a transient `obs_source_showing()==false`. Possibly
benign; flagged for follow-up.

**Conclusion:** every code path statically equals upstream 27.x yet output is
empty. Two surviving hypotheses, indistinguishable by static analysis:
1. **Global render/compositing** produces nothing for any source (effect runtime
   binding, output texture, or preview display).
2. **monitor_capture-specific** (CUSTOM_DRAW + SRGB framebuffer draw) produces
   transparent output despite valid texture (e.g. the texture content is black,
   or the srgb-framebuffer sprite draw is a no-op on this build).

**Decisive bisection experiment:** add a plain Color Source (not CUSTOM_DRAW,
uses solid.effect) full-screen on top.
- Renders red → global pipeline OK → hypothesis 2 (monitor_capture-specific).
- Still empty → hypothesis 1 (global render).

## Pass 2 results (2026-05-31, decisive bisection executed)

Tester ran the experiment. Log `2026-05-31 14-40-22.txt` + recording
`2026-05-31 14-41-15.mp4`:

- Added `色源` (`color_source_v3`) at 14:40:42 with color `0xFF0000FF`
  (opaque RED, alpha=255). No width/height in settings →
  `color_source_defaults_v3` resolves them to 1920x1080
  (color-source.c:136-138). Scene item: pos (0,0), scale (1,1), visible:true;
  Display + Minecraft both visible:false → the color source is the ONLY visible
  item and fills the full canvas.
- It draws via `OBS_EFFECT_SOLID` + `gs_draw_sprite(0,0,1920,1080)`
  (color-source.c:69-105) — the simplest possible draw path: no CUSTOM_DRAW,
  no capture texture, no SRGB framebuffer dependency (alpha=1.0).

**Objective measurement (ffmpeg single-frame extraction + 1x1 average):**
- `14-41-15.mp4` (color source) avg RGB = [2,0,2] (pure black).
- `11-01-34.mp4` (black baseline) avg RGB = [2,0,2]; both extracted PNGs are
  byte-identical size, visually pure black.
- An opaque full-screen RED quad recorded as BLACK (R=2, not 255).

**Verdict: hypothesis 1 confirmed — the global render/compositing pipeline
emits black for ANY source.** Not monitor_capture-specific. Root cause lives in
libobs core rendering, at the level where `gs_draw_sprite` draws become
effective, affecting every source equally.

`render_main_texture` (obs-video.c:127) clears to transparent-black then renders
sources; `render_output_texture` (obs-video.c:213) short-circuits to
`render_texture` when default_effect + base size match. Both statically standard
27.x. So failure is BELOW this level (effect execution / d3d11 draw / viewport /
framebuffer-srgb), or in the render_texture → output → NV12 → encoder branch.

**Pass 3 bisection — needs PREVIEW color (preview is GPU-live, not in any
file, so cannot be derived from recordings/logs):**
- Preview RED → main-texture compositing works; black enters in the
  output_texture / render_convert_texture(NV12) / encoder branch → investigate
  recording output chain.
- Preview BLACK or GRAY/empty → main-texture compositing itself produces
  nothing; `gs_draw_sprite` is effectively a no-op for all sources at the
  d3d11/effect layer → investigate graphics backend draw + framebuffer-srgb.

## Pass 3: preview confirmed GRAY/empty (2026-05-31, screenshots)

Tester screenshots (main UI + color-source Properties dialog):
- Main preview region: EMPTY — shows the OBS UI gray background, not black. The
  compositor output is transparent (alpha=0); the gray clear shows through.
- Color-source Properties dialog preview (a standalone obs_display rendering
  ONLY the single color source through its own draw callback): ALSO empty/gray.
- Source list confirms 色源 visible (swatch #FF0000); Properties confirm color
  #FFFF0000, width 1920, height 1080.

**Two INDEPENDENT render entry points both produce nothing:**
1. main_view → scene → source composite (main preview)
2. standalone obs_display + draw_callback → obs_source_video_render(single
   source) (Properties dialog preview)

Their only shared layer is `obs_source_video_render` → effect →
`gs_draw_sprite` → d3d11 draw. Draws ARE issued (pass-1 confirmed the path
reaches the gs_draw_sprite loop) but write ZERO pixels into the render target,
for every source, in every view. This is the tightest possible localization:
the bug is in that shared lowest draw layer, NOT in scene/view composition,
NOT in the NV12/encoder branch, NOT in monitor_capture.

`render_output_texture` short-circuits to `render_texture` (output==base
1920x1080) → output/scale stage is pure passthrough, not implicated. The NV12
branch is downstream of an already-empty render_texture.

Source-agnostic root-cause surface (in priority order):
- effect technique/pass binding: `solid.effect` "Solid" technique not binding a
  working pixel shader / not applying state at the d3d11 level;
- `gs_draw_sprite` sprite-geometry / vertex-buffer generation yields no
  primitives;
- `gs_set_render_target` / viewport (`gs_ortho` in set_render_size) leaves draws
  off-target or off-screen;
- OBSRedux fork-specific render edit (360fps path) skips effective drawing.

NEXT (implementation/debug, leaves brainstorm): instrument render_main_texture
to stage + read back render_texture pixels right after obs_view_render and
confirm it is transparent-black post-composite; then bisect inside the d3d11
draw / effect layer. Inspect libobs/graphics + libobs-d3d11 + effect loading for
fork edits vs upstream OBS 27.2.4.

## Scope change required

prd.md currently constrains scope to "Windows monitor_capture / Display
Capture". Pass 2-3 proved the root cause is the global libobs render pipeline
(every source, every view). The task scope must widen from "display capture
backend" to "global render pipeline emits empty frames"; the monitor_capture
diagnostics from pass 1 stay as ruled-out evidence, not the fix target.

## Pass 4: exhaustive static read — render source is 100% standard (2026-06-03)

Read the entire render submit path; ALL standard OBS 27.x, no fork edits:
- `gs_draw_sprite` / `gs_draw` / `device_draw` (graphics.c, d3d11-subsystem.cpp)
- `device_set_viewport` / `device_ortho` / `device_set_render_target` /
  `UpdateRasterState` / `UpdateBlendState` / `UpdateZStencilState` /
  `FlushOutputViews` / `UpdateViewProjMatrix`
- matrix stack `gs_matrix_get/push/pop/identity` (inits to identity)
- `render_main_texture` / `render_output_texture` / `obs_graphics_thread_loop`
- `render_texture` creation = `gs_texture_create(..., GS_RGBA, ...)`; effect
  loads standard
- `solid.effect` + `default.effect`: runtime files BYTE-IDENTICAL to dev source
- grep `OBSRedux|360` across libobs/*.c: ZERO hits (no fork markers)

**Strongest symptom-fit hypothesis: ViewProj is degenerate at runtime.**
`solid.effect` VSSolid does `pos = mul(pos, ViewProj)`. If ViewProj is zero/wrong
every vertex collapses to 0 → degenerate triangles → no pixels written, while
`gs_clear` (no vertex transform) still paints the gray background. This fits
EVERY observation: all sources, all 3 render targets black; clear works; no
device_draw error; color param irrelevant (default is WHITE not black). But the
code computing ViewProj is standard → if ViewProj is wrong it is a RUNTIME-VALUE
problem, not code logic.

**Build finding (likely root-cause class):** OBSRedux core libobs ships as
`obs.dll` (NOT libobs.dll). Runtime DLL timestamps:
- `obs.dll` (obs-video.c / obs.c / graphics.c matrix stack): 2026-05-26 15:24
- `libobs-d3d11.dll` (UpdateViewProjMatrix / gs_device): 2026-05-31 10:58
- `obs64.exe`: 2026-05-27 17:14

`obs.dll` and `libobs-d3d11.dll` are **5 days apart** yet share `gs_device`,
`matrix4`, and `obs_core_video` struct layouts through internal headers. A
mixed-version DLL set = ABI mismatch = garbage matrix/viewport fields read
cross-DLL = degenerate ViewProj = all-black, no crash. Also: the running binary
is `finished/obs.dll` built 05-26; the `dev/` source read above may have changed
since, so dev static analysis may not reflect the obs.dll actually running.

## Next steps (Pass 5)

1. **Clean FULL rebuild** of obs.dll + libobs-d3d11.dll + all modules from the
   current consistent `dev/` source; redeploy ALL to `finished/`; retest the
   color-source scene. Resolves ABI-skew AND source/binary inconsistency in one
   shot. Highest-probability, cheapest fix — do this FIRST.
2. **If still black:** one-shot diagnostic in `UpdateViewProjMatrix`
   (libobs-d3d11, already rebuilt each pass) logging curViewMatrix /
   curProjMatrix / curViewProjMatrix on first draw. Confirms or kills the
   ViewProj-degenerate hypothesis, then trace the bad input upstream.

## Pass 5: full rebuild done — build hypothesis KILLED by md5 (2026-06-03)

Ran full `cmake --build build64 --config RelWithDebInfo` (exit 0). Findings:
- `build64/libobs/RelWithDebInfo/obs.dll` was NOT recompiled (still 05-26
  15:24, 786432 B) — libobs core source unchanged since 05-26, nothing to
  rebuild.
- **md5 of running `finished/bin/64bit/obs.dll` == md5 of fresh-build obs.dll
  == `192f3b68b9fbc4bc94a536ce476c52ad`.** The deployed binary IS the
  current-source binary, byte-for-byte.

**Hypothesis A (stale/mixed build, ABI skew) is DEFINITIVELY KILLED.** Rebuild +
redeploy of obs.dll changes nothing (same bytes). The libobs render binary is
stock OBS 27.x and matches the source I read. So root cause = Hypothesis B:
runtime value corruption (most likely ViewProj degenerate) OR a fork change
outside the render files (platform-windows.c / UI / bootstrap / build macros)
that corrupts render input at runtime.

## Pass 6 plan: runtime diagnostic (the only remaining path)

Add a ONE-SHOT diagnostic in `device_draw` (d3d11-subsystem.cpp), fired on the
first draw, logging the actual GPU pipeline inputs to decisively locate the
degenerate stage in a single retest:
- `device->viewport.cx/cy` (catches viewport-zero → all triangles clipped)
- `device->curViewProjMatrix` 16 floats (catches ViewProj-degenerate)
- `device->curVertexBuffer->numVerts` (catches empty geometry)
- `curRenderTarget != null`, `curFramebufferSrgb` (catches RT/srgb issues)

Then rebuild libobs-d3d11 (already rebuilt each pass), redeploy to finished,
retester runs the color-source scene, read the new `[obsredux-diag]` log lines.
The matrix/viewport values tell us which input is bad, then trace upstream.

## Pass 7: runtime diagnostic — ViewProj DEGENERATE confirmed (2026-06-03)

Tester retested with the device_draw diagnostic deployed. Log
`2026-06-03 16-11-07.txt`, draws #1-#8 ALL identical:

- `viewport=1920x1080` ok, `numVerts=4` ok (sprite quad), `RT=yes` ok,
  `srgb=1` ok, `VS=yes PS=yes` ok — every pipeline input correct EXCEPT:
- `ViewProj r0=[0 0 0 1] r1=[0 0 0 1] r2=[0 0 0 1] r3=[0 0 0 1]` — DEGENERATE.

**ViewProj-degenerate hypothesis CONFIRMED.** All four rows `[0,0,0,1]`
collapse every transformed vertex to a single clip point -> degenerate
triangles -> ZERO pixels rasterized, while `gs_clear` (no transform) still
paints the gray background. Explains gray preview + black recording + every
source black, exactly.

The diagnostic sits at d3d11-subsystem.cpp:2170, AFTER
`device->UpdateViewProjMatrix()` (line 2155) which computes and uploads
`curViewProjMatrix` to the vertex shader (lines 836-838). So the logged value
IS the matrix the GPU used — not stale, not a red herring.

**Entire compute chain re-read, ALL standard OBS 27.x (no fork edits):**
- `UpdateViewProjMatrix` (823): `gs_matrix_get(curViewMatrix)` -> z-negate ->
  `matrix4_mul(curViewProjMatrix, curViewMatrix, curProjMatrix)` ->
  `matrix4_transpose` -> upload. Standard.
- `device_ortho` (2501): standard; inputs `gs_ortho(0,1920,0,1080,-100,100)`
  (obs-video.c:112) -> correct proj (x.x=2/1920, y.y=-2/1080, z.z=1/200,
  t=[-1,1,0.5,1]).
- `matrix4_mul` / `matrix4_transpose` (matrix4.c:60,263): standard.
- matrix stack init to identity (graphics.c:152); `gs_matrix_get` copies top.

Post-transpose `[0,0,0,1]x4` implies pre-transpose = rows xyz zero,
t=[1,1,1,1]. With standard math this requires a degenerate runtime INPUT
(curViewMatrix or curProjMatrix), NOT a code-logic bug.

**DLL split note:** finished/obs.dll = 05-26 build (matrix4_mul, gs_matrix_get,
render_main_texture); finished/libobs-d3d11.dll = 06-03 rebuild (device_ortho,
UpdateViewProjMatrix). The math fns are EXPORTED from obs.dll and called
cross-DLL from libobs-d3d11.dll with `matrix4*`/`vec4*` (vec4 has a `__m128`
union member). Leading hypotheses now: a degenerate input, OR a cross-DLL
matrix-math ABI skew.

## Pass 8 plan: log the two input matrices + cross-DLL ABI sanity

Add a one-shot diagnostic in `UpdateViewProjMatrix` (before `matrix4_mul`):
1. Log `curViewMatrix` (post z-negate) and `curProjMatrix` — the real inputs.
2. ABI sanity: build two local identity `matrix4`, call `matrix4_mul` +
   `matrix4_transpose` on them, log the result. identity*identity must equal
   identity; if not, the cross-DLL matrix math is itself broken (the smoking
   gun for ABI skew between obs.dll and libobs-d3d11.dll).

One retest then fully resolves: bad proj (-> device_ortho / curProjMatrix
offset), bad view (-> matrix stack), or broken cross-DLL math (-> ABI/build
skew).

## Pass 9: ROOT CAUSE — matrix4_identity miscompiles (runtime, not source) (2026-06-03)

Pass 8 retest, log `2026-06-03 18-03-37.txt`. The three new diag lines decode it
(draws #1-#4 identical):
- `proj#` = [x.x=.0010, y.y=-.0019, z.z=.0050, t=[-1,1,.5,1]] — CORRECT ortho.
- `view#` (post z-negate) = rows xyz all 0, t=[1,1,-1,1] => pre-negate view =
  {xyz=0, t=[1,1,1,1]} — DEGENERATE.
- `abi-sanity I*I` (identity x identity, transposed) = [0,0,0,1]x4 — must be
  identity, is NOT.

`device_ortho` builds proj by hand (vec4_zero + explicit field sets), no
matrix4_identity -> proj correct. Everything that flows through
`matrix4_identity` is degenerate, identically: {xyz=0, t=[1,1,1,1]}. The
abi-sanity result [0,0,0,1]x4 is exactly transpose({xyz=0,t=[1,1,1,1]}). So:

**ROOT CAUSE: `matrix4_identity` produces {x=0,y=0,z=0, t=[1,1,1,1]} at
runtime instead of true identity.** Its source (matrix4.h:44) is the standard,
correct OBS implementation (vec4_zero x4 then x.x=y.y=z.z=t.w=1). Correct
source + wrong runtime = a BUILD/codegen/ABI problem, NOT a logic bug — which
is why Pass 4 static reading found nothing and Pass 5 md5 (incremental, never
recompiled obs.dll) found nothing.

The degenerate view -> `matrix4_mul(view, proj)` -> transpose -> ViewProj
[0,0,0,1]x4 -> all vertices collapse -> zero pixels. Whole chain explained.

Two live sub-hypotheses:
1. **ABI skew**: deployed obs.dll (05-26, holds the matrix math) vs
   libobs-d3d11.dll (rebuilt) disagree on vec4/matrix4 layout (or obs.dll is
   stale vs current headers). Cross-DLL matrix calls then corrupt. -> a TRUE
   clean rebuild of libobs + libobs-d3d11 (Pass 5 never recompiled obs.dll)
   fixes it.
2. **Compiler codegen bug**: VS "18 2026" miscompiles the vec4 union
   type-punning in matrix4_identity (vec4_zero via __m128 member, then float
   field writes). A clean rebuild with the same toolchain would NOT fix it;
   needs a source/flag workaround.

## Pass 10 plan: clean rebuild + mechanism isolation (one round decides)

- Augment diag: log sizeof(vec4)/sizeof(matrix4); a MANUAL identity (memset +
  field writes, no matrix4_identity); a RAW inline matrix4_identity.
- CLEAN rebuild libobs (obs.dll) from scratch (--clean-first) + libobs-d3d11;
  redeploy BOTH (+pdb); report whether obs.dll md5 changed from 192f3b68….
- Retest decode:
  - abi-sanity now identity + render works -> was ABI skew -> FIXED.
  - manual-ident broken too -> compiler miscompiles struct field writes (severe).
  - manual OK, inline matrix4_identity broken -> union-punning codegen bug ->
    rewrite matrix4_identity to drop the __m128/float punning.

## Pass 10: mechanism ISOLATED — matrix4.c miscompiled by VS18 2026 /O2 (2026-06-04)

Pass 9's "matrix4_identity broken" was WRONG. Clean rebuild (obs.dll md5
192f3b...->e3bdf1..., so it DID recompile fresh) still black. Log
`2026-06-04 12-45-43.txt` isolation lines:
- `sizes vec4=16 matrix4=64` — struct layout CORRECT, no ABI/packing skew.
- `manual-ident` = true identity — raw field writes fine.
- `inline-ident` (matrix4_identity, inline in libobs-d3d11) = true identity —
  matrix4_identity is FINE.
- `abi-sanity I*I` (exported matrix4_mul + matrix4_transpose, obs.dll) =
  [0,0,0,1]x4 — BROKEN. matrix4_mul(I,I) returns {xyz=0,t=[1,1,1,1]} != I.
- `view#` degenerate, `proj#` (local device_ortho) correct.

**Pattern: everything computed locally/inline in libobs-d3d11 is correct; the
exported matrix4_mul/matrix4_transpose in obs.dll (matrix4.c) are miscompiled.**
Fed correct identity inputs, matrix4_mul still returns garbage. CODEGEN bug,
not source/ABI. This is why Pass 4 (static read) and Pass 5 (incremental, never
recompiled obs.dll) and even Pass 8-9 (clean rebuild, same toolchain) all
failed to fix it.

Build facts (build64/CMakeCache.txt): generator "Visual Studio 18 2026",
toolset default (cl.exe MSVC), x64, RelWithDebInfo = `/O2 /Ob1 /MD /Zi`
(/fp:precise, no /GL). Upstream OBS adds `-fno-strict-aliasing` for GCC/Clang
(root CMakeLists:164) but MSVC gets NO aliasing guard. matrix4_mul writes its
result via the vec4 union `.ptr[j]` (float) then inlined matrix4_copy reads
`.m` (__m128) — a union type-pun. VS18 2026's /O2 miscompiles this pun (a
preview-compiler regression; stock MSVC builds OBS fine). matrix4_identity is
too simple to trip it; matrix4_mul (loops + punning + inlined copy) does.

Why the view matrix is degenerate: scene-item render does
gs_matrix_mul(item_transform) -> matrix4_mul on the stack top. Even an identity
transform through the broken matrix4_mul yields {xyz=0,t=[1,1,1,1]} -> view
degenerate -> ViewProj collapses -> black. Explains every observation.

## Pass 11 fix: disable /O2 on matrix4.c (MSVC codegen-bug workaround)

matrix4_mul/transpose are tiny and not per-pixel hot, so disabling optimization
on matrix4.c is a safe, surgical workaround for the compiler bug:
`#ifdef _MSC_VER` / `#pragma optimize("", off)` after the includes (re-enable at
EOF). Rebuild libobs, redeploy obs.dll, retest. Decisive check: abi-sanity
returns to identity AND preview shows RED.
(If it fixes abi-sanity but view# stays degenerate -> the matrix4_copy path in
graphics.c also needs it; escalate. Broader latent risk: other SSE-punning math
TUs (vec3/quat/matrix3) likely share the bug — revisit in spec/cleanup.)
