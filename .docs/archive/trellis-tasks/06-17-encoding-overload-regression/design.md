# Design — toolchain switch to fix encoding-overload regression

## Strategy

Rebuild OBS-PT with a known-good MSVC toolset; keep everything else identical so
the A/B isolates the toolchain as the only variable.

## Decisions

1. **Toolset = VS2022 Build Tools, v143** (not v142/VS2019).
   - v143 is the MSVC current OBS ships with and lacks the VS 18 2026 codegen
     defect. VS2019 is harder to obtain now; v142↔v143 are ABI-compatible.
   - Install the standalone **Build Tools for Visual Studio 2022** (no IDE):
     `winget install Microsoft.VisualStudio.2022.BuildTools` with workload
     `Microsoft.VisualStudio.Workload.VCTools` (+ Windows 10 SDK).

2. **Fresh build dir `build-v143/`** — do NOT reuse `build64/`.
   - `build64/CMakeCache.txt` hard-pins generator `Visual Studio 18 2026` and its
     `cl.exe` paths; reconfiguring in place is unreliable. A new dir also keeps
     the VS 18 2026 build available for side-by-side A/B and instant rollback.
   - Configure: `-G "Visual Studio 17 2022" -A x64` + replicate build64's cache
     vars (deps/Qt/CEF paths, `OBS_VERSION_OVERRIDE=0.0.1`), read from
     `build64/CMakeCache.txt` at execute time.

3. **Reuse `deps2019/win64`** unchanged. v142-built deps link into a v143 build
   per Microsoft's v142/v143 binary-compat guarantee. (Risk noted below.)

4. **Keep the `matrix4.c`/`matrix3.c` `#pragma optimize("",off)`.** It is
   `_MSC_VER`-guarded, on tiny non-hot TUs → harmless on v143, defensive against
   any residual MSVC codegen variance. Already documented in the MSVC spec.

## Validation (the gate)

Controlled A/B on the same machine:
- Same PotPvP scene/sources, 1080p/480fps, jim_nvenc CQP26 (unchanged config).
- Record ~2 min, then read the new OBS-PT log line
  `Video stopped, number of skipped frames due to encoding lag: N/T (X%)`.
- **Pass**: X% comparable to official 27.2.4 (≤ ~0.5%), no long frozen runs.
- **Baseline to beat**: 4.1% / 4.7% (VS 18 2026 build); official reference
  0.1–0.5%.

## Risks / Rollback

- **deps2019 ABI**: if any v142 dep fails to link/load under v143, rebuild that
  dep or pull obs-deps for VS2022. (Low risk per MS compat promise.)
- **Theory wrong**: if v143 build still lags ≫0.5%, toolchain is not the (sole)
  cause → fall back to full working-tree diff vs upstream 27.2.4 for other
  uncommitted source edits.
- **Rollback**: `build64/` and the deployed VS18 binaries are untouched until the
  v143 build passes; revert = redeploy from `build64/`.

## Out of scope

Encoder/setting "optimization", fps changes, preset scene changes.
