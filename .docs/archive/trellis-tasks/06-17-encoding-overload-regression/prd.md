# Diagnose encoding-overload regression vs upstream OBS

## Goal

OBS-PT recordings show severe "encoding lag" frame-skips (long frozen-frame
runs) at 480fps, while the **same machine's official OBS 27.2.4 with an
essentially identical config does not**. Find what the fork wrongly changed
relative to upstream — not "optimize" the settings.

## Confirmed Facts (evidence)

Apples-to-apples, **same RTX 3060, same machine**:

| | OBS ver | video | NVENC settings | encoding lag @480fps |
|---|---|---|---|---|
| Official | **27.2.4** | 1080p/480/NV12/709/Partial | CQP26, preset hp, kf250, bf0… | **0.1–0.5%** |
| OBS-PT | **27.2.4** | identical | **byte-identical** | **4.1% / 4.7%** |

- `recordEncoder.json` differs only by the inert `bitrate` key + `encoder` key
  placement; both logs print the **same** applied jim-nvenc block. Config is NOT
  the cause.
- Both are **OBS 27.2.4** (verified in logs). Not a version gap.
- ffprobe: both mp4 are 480fps CFR; the 4–5% encoder-skipped frames become
  duplicate (frozen) frames in the CFR mux → the reported "long freezes".
  Bitrate 3.7–3.9 Mbps (low, consistent with low-complexity content + many
  zero-cost duplicate frames).
- **Rendering lag (1.5%) and encoding lag (4.7%) are independent counters.** The
  BitBlt window-capture the tester picked explains *rendering* lag, not the
  larger *encoding* lag (encoder input-queue backpressure, downstream of
  compositing).
- Fork's only non-UI source edits are diagnostic-only:
  - `libobs-d3d11/d3d11-duplicator.cpp` vs upstream 27.2.4 = added logging +
    `logged_texture` guard + 2× `LOG_DEBUG→LOG_WARNING`. Copy logic
    (`gs_generalize_format` + `CopyResource`) is **identical to upstream**.
  - `libobs-d3d11/d3d11-subsystem.hpp` vs upstream = **+1 line** (`logged_texture`).
  - `plugins/win-capture/duplicator-monitor-capture.c` = one-shot diagnostics.
  → D3D11/capture path has **zero functional/perf delta** from upstream.
- **Build toolchain = `Visual Studio 18 2026`** (build64/CMakeCache.txt),
  RelWithDebInfo `/O2`. Machine has **only** VS 18 2026 installed (no VS2019/2022).
- This toolchain **demonstrably miscompiles libobs** at `/O2`: per
  `.trellis/spec/obsredux-graphics-msvc.md`, VS 18 2026 `cl.exe` miscompiles
  `struct vec4/vec3` union (`float` view ↔ `__m128` view) type-punning →
  `matrix4_mul(I,I)` returned garbage → black render. Patched **only** on
  `matrix4.c` + `matrix3.c` via `#pragma optimize("",off)` (verified present).
- Upstream relies on `-fno-strict-aliasing` (GCC/Clang) for this code class; the
  **MSVC branch has no equivalent**, so every other `/O2` TU on this toolchain is
  unguarded.
- `deps2019/` (dependencies2019.zip + win64) ships in the repo → **VS2019 was the
  originally-intended toolchain**; VS 18 2026 is the deviation.

## Root Cause (high confidence)

The encoding overload is **the second symptom of the same VS 18 2026 MSVC
miscompilation defect** that caused the black-render bug. Same OBS version,
config, GPU and NVENC settings as the working official build — the only
remaining variable is the toolchain, and that toolchain is independently proven
to miscompile this codebase. The black-render fix patched just 2 graphics-math
TUs; the video/encoder feed path (libobs `media-io`/`video-io`/`obs-encoder`,
also full of union/pointer punning) is still `/O2`-compiled by the buggy
compiler, producing periodic multi-ms stalls in frame submission to NVENC
(profiler: `output_gpu_encoders` median 0.019 ms, **max 16.3 ms**) → encoder
input queue backs up → 4–5% frames skipped → frozen runs.

**Definitive confirmation = rebuild with a known-good MSVC toolset and re-measure
encoding lag on the same scene.**

## Decision (ADR-lite)

**Context**: Root cause isolated to the `Visual Studio 18 2026` MSVC miscompiling
libobs's encoder/video feed path (same defect class as the black-render bug).
Machine currently has only VS 18 2026 installed.

**Decision**: Fix at the root — **switch the build toolchain to a known-good MSVC
(VS2022 Build Tools, v143)** and rebuild OBS-PT, then confirm via a controlled
A/B re-measurement. Rejected: per-TU `#pragma optimize` whack-a-mole (fragile,
hot-path perf cost, spec-discouraged) and "audit-first" (the rebuild itself is
the cheapest confirmation).

**Consequences**: Requires installing the v143 toolset (~GB). v143 is binary-
compatible with the shipped `deps2019` (v142). The VS 18 2026 `build64/` is left
intact for A/B and rollback. If the v143 build still lags, the toolchain theory
is wrong → fall back to a full working-tree diff vs upstream 27.2.4.

## Acceptance Criteria

- [ ] OBS-PT recording at 1080p/480fps on this machine shows encoding-lag
      skip-rate comparable to official 27.2.4 (≤ ~0.5%), no long frozen runs.
- [ ] Root cause confirmed by a controlled A/B (same scene, only toolchain
      changed).
- [ ] The black-render `#pragma optimize` workaround is reconciled (kept or
      removed) consistent with the chosen toolchain.

## Out of Scope

- Encoder "optimization" / lowering fps / changing presets to mask the problem.
- Lunar Client / game-capture behavior.
- Changing the shipped PotPvP preset.

## Technical Notes

- Evidence logs: OBS-PT `…/OBS-Redux/obs-studio/logs/2026-06-15 15-10-14.txt`;
  official `%APPDATA%/obs-studio/logs/2026-06-15 11-44-51.txt`.
- Spec: `.trellis/spec/obsredux-graphics-msvc.md` (MSVC codegen).
- Prior task: archive `2026-06/05-29-obsredux-display-capture-blank`.
