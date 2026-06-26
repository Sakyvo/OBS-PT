# OBSRedux Graphics Math — MSVC Codegen Spec

**Status**: Active
**Owner**: OBSRedux Core
**Last Updated**: 2026-06-04

## Scope

`libobs/graphics/*.c` translation units that perform `struct vec4` / `struct vec3`
union type-punning — writing the float view (`.x/.y/.z/.w` or `.ptr[i]`) and
reading the `__m128 .m` view (or vice versa) within the same optimized function.
These compile into `obs.dll`.

## Root Cause (2026-06-04)

The **"Visual Studio 18 2026"** generator (default `cl.exe` MSVC, x64,
RelWithDebInfo = `/O2 /Ob1 /MD /Zi`) **miscompiles** vec4 union float/`__m128`
type-punning at `/O2`. Demonstrated failure: `matrix4_mul(I, I)` returns garbage
(`{xyz=0, t=[1,1,1,1]}`) instead of the identity matrix.

Effect chain: `gs_device::UpdateViewProjMatrix()` builds the view matrix via
`matrix4_mul` / `matrix4_transpose` → the miscompile collapses ViewProj to a
degenerate matrix → the vertex shader maps every sprite into a zero clip volume
→ **the entire render output is black** (preview AND recording), for every
source (color source, display capture, game capture alike).

Upstream OBS avoids this on GCC/Clang via `-fno-strict-aliasing` (root
`CMakeLists.txt`); the MSVC branch has no equivalent, and MSVC offers no direct
`-fno-strict-aliasing` analog.

> **Warning**: This is a global render-pipeline failure that masquerades as a
> per-source bug. A black display-capture / color-source / game-capture preview
> on this toolchain is most likely THIS, not the source plugin. Verify the
> ViewProj matrix (see Diagnostic) before touching any capture path.

## Convention: guard affected MSVC math TUs

Disable optimization for the whole TU, MSVC-only, wrapping every function:

```c
#ifdef _MSC_VER
#pragma optimize("", off)
#endif
/* ... all functions in the TU ... */
#ifdef _MSC_VER
#pragma optimize("", on)
#endif
```

- `off` goes after the includes, before the first function; `on` at EOF, after
  the last function.
- Both must be `#ifdef _MSC_VER`-guarded.
- These are small, non-hot TUs; the optimization loss is negligible (rendering
  is GPU-bound). Do NOT blanket-apply this to hot per-pixel/per-sample paths.

## Guarded TUs (current)

- `libobs/graphics/matrix4.c` — **REQUIRED**. Holds the demonstrated-miscompiled
  `matrix4_mul` (standard `out[i].ptr[j] = vec4_dot(...)` loop + inlined
  `matrix4_copy` that reads `.m`). This is THE fix for the black render.
- `libobs/graphics/matrix3.c` — **defensive**. The only other
  `libobs/graphics/*.c` TU using `__m128` directly; shares the punning surface
  (`matrix3_from_matrix4` mixes `.m` and `.w` writes; `matrix3_transpose` reads
  `.m`).

Pure-`.m` SSE inline helpers in `vec4.h` / `vec3.h` / `quat.h` (e.g. `vec4_add`,
`vec4_copy`) are NOT affected — the bug requires the scalar-write + `__m128`-read
mix that lives in the `.c` function bodies above, which is why guarding only
those `.c` TUs fixes the failure.

## Diagnostic (how it was isolated)

One-shot `blog(LOG_WARNING, ...)` instrumentation in `UpdateViewProjMatrix()` /
`device_draw()` dumping view / proj / ViewProj rows plus an "ABI sanity"
`matrix4_mul(I, I)` → transpose check. A garbage identity from the ABI-sanity
line (diagonal not `1,1,1,1`) while `sizeof(vec4)==16` / `sizeof(matrix4)==64`
are correct pinpoints the codegen bug (rules out ABI/layout skew). This
instrumentation is temporary — remove once diagnosed; it is not kept in tree.

## Validation

After touching any guarded TU, rebuild `libobs` and confirm a non-black preview:

```powershell
cmake --build build64 --config RelWithDebInfo --target libobs -- /m
```

Sync `build64/libobs/RelWithDebInfo/obs.dll` to `<Install Root>/bin/64bit/`,
launch OBSRedux, confirm the scene preview renders (not black). A clean rebuild
(`--clean-first`) must produce zero unbalanced-`optimize`-pragma warnings
(C4193 / C4426).

## Wrong vs Correct

### Wrong
Leave `matrix4.c` / `matrix3.c` under `/O2` on the VS 18 2026 toolchain → all
rendering is black; debugging chases the capture source plugins fruitlessly.

### Correct
Wrap the affected math TUs in `#ifdef _MSC_VER` `#pragma optimize("", off/on)`.
**Resolved (2026-06-19, task 06-17): build with VS2022 Build Tools v143** — see
Toolchain below.

## Toolchain — build with v143, not VS 18 2026 (2026-06-19)

**Requirement**: build OBS-PT with the **VS2022 Build Tools v143** toolset
(`K:/Microsoft Visual Studio/2022/BuildTools`, MSVC **14.44.35207**) via the
`Visual Studio 17 2022` generator — NOT `Visual Studio 18 2026`. v143 matches the
shipped `deps2019` / `Qt 5.15.2 msvc2019` (v142↔v143 ABI-compatible) and does not
exhibit the vec4/vec3 miscompile. (ATL is absent from the BuildTools, so
`obs-qsv11` + `frontend-tools` don't build — irrelevant to NVENC recording; add
the `VC.ATL` component if those are wanted.)

Use the VS2022 BuildTools-bundled CMake 3.x for this tree:

```powershell
& "K:\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" `
  -S . -B build-v143 -G "Visual Studio 17 2022" -A x64 `
  -D "DepsPath:PATH=K:/Projects/dev/OBS-PT/deps2019/win64" `
  -D "QTDIR:PATH=C:/Qt/5.15.2/msvc2019_64" `
  -D "OBS_VERSION_OVERRIDE:STRING=1.0.0"
```

Do not use the system `C:\Program Files\CMake\bin\cmake.exe` 4.x here; legacy
bundled dependency CMake files still declare `cmake_minimum_required(<3.5)`, and
CMake 4 rejects them. In PowerShell, pass dotted/hyphenated cache values as a
quoted typed `-D` argument (`"OBS_VERSION_OVERRIDE:STRING=1.0.0"`). An
unquoted `-DOBS_VERSION_OVERRIDE=1.0.0` can be split so CMake sees the value
as `1` and treats `.0.0` as an extra path.

**Measured impact** (same RTX 3060, same PotPvP scene, 1080p/480fps, jim_nvenc
CQP26), VS 18 2026 → v143:
- encoder-skipped frames (encoding lag): **4.1–4.7% → 0.8–1.2%** (~4×)
- rendering-lag frames: **~1.5% → ~0.4–0.8%** (~2–3×)

VS 18 2026 generated heavier/stalling code across the whole libobs pipeline, not
only the matrix TUs. Keep the `matrix4.c`/`matrix3.c` pragma guards as defensive
(harmless on v143).

**Not a toolchain bug**: the *visible* "long frame freeze" first chased as an
encoding-overload regression (task 06-17) was **window capture failing to
continuously capture Minecraft 1.7.10** (only the first frame, then static) —
present in stock OBS too, not the fork's code, not fixable by modifying OBS →
won't-fix. Game capture is the correct path; its frame drops are normal. The
toolchain lag reduction above is a separate, real win.
