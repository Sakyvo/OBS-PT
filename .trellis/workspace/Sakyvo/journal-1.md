# Journal - Sakyvo (Part 1)

> AI development session journal
> Started: 2026-05-23

---



## Session 1: OBSRedux S8 bootstrap acceptance

**Date**: 2026-05-27
**Task**: OBSRedux S8 bootstrap acceptance
**Package**: plugins/win-dshow/libdshowcapture
**Branch**: `master`

### Summary

Fixed preset integrity timing, moved early bootstrap into AppInit, preserved first-run completion in globalConfig, and verified D:/K: deployment paths, first-run behavior, PageDown recording, and log retention.

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `e8bf887` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete


## Session 2: Fix OBSRedux PotPvP preset

**Date**: 2026-05-29
**Task**: Fix OBSRedux PotPvP preset
**Package**: plugins/win-dshow/libdshowcapture
**Branch**: `master`

### Summary

Updated the PotPvP default scene collection to OBS-loadable sources format with Desktop Audio only, Lunar Client Minecraft over Display fallback, synchronized the current test install preset, and documented the scene collection contract.

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `fa4f582` | (see git log) |
| `acccf0d` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete


## Session 3: Fix OBSRedux all-black render (MSVC /O2 vec4 union miscompile)

**Date**: 2026-06-04
**Task**: Fix OBSRedux all-black render (MSVC /O2 vec4 union miscompile)
**Package**: plugins/win-dshow/libdshowcapture
**Branch**: `master`

### Summary

Root-caused the blank preview/black recording to the VS 18 2026 cl.exe /O2 miscompiling vec4 union float/__m128 type-punning in libobs/graphics/matrix4.c (matrix4_mul returned a garbage identity), collapsing the global render ViewProj matrix to black for every source. Fix: MSVC-guarded #pragma optimize("",off/on) on matrix4.c (required) and matrix3.c (defensive, only other graphics .c using __m128). Removed temporary [obsredux-diag] instrumentation from d3d11-subsystem.cpp; kept the one-shot DXGI duplicator/monitor-capture lifecycle logs. Added spec obsredux-graphics-msvc.md and redirected the display-capture spec root cause. trellis-check passed (clean rebuild, zero warnings); user-confirmed non-black preview; deployed obs.dll + libobs-d3d11.dll.

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `cb469b2` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete


---

## Session 4: 480fps adaptive defaults + rebrand to OBS-PT

**Date**: 2026-06-14
**Task**: 06-05-obsredux-480fps-default-profile (in_progress; not archived)
**Branch**: `master`

### Summary

Made the shipped PotPvP defaults hardware-adaptive at first run and rebranded the whole product OBS-Redux -> OBS-PT (incl. exe name). Synced the static defaults to the user's current live config.

### Main Changes

- **480fps adaptive bootstrap (v3)**: new early `apply_monitor_video_to_profile` writes monitor-native Base=Output (no >1080p cap) + 480fps before ResetVideo (repair-gated); `apply_encoder_to_profile` now takes `cqp` and writes a complete per-encoder `recordEncoder.json` (fresh obs_data, no mismatched keys), CQP by height (<1080->20 else 26). `OBSPT_BOOTSTRAP_VERSION` 2->3.
- **Default sync**: 480fps, CQP 26, streamEncoder 20000/high, Bilibili service (empty key), scene Minecraft(top/shown)/Display(bottom/hidden/method 0), hotkeys cleared.
- **Rebrand OBS-Redux -> OBS-PT**: exe `OBS-PT.exe`, obs.rc.in/manifest/OBS_PRODUCT_NAME, display strings + locale values; internal `[OBSPT]`, `OBSPT.*` keys, `OBSPT_BOOTSTRAP_VERSION`, `run_obspt_*`, `obspt-bootstrap.{cpp,h}`, `obspt-defaults/`. Version 0.0.1.
- Updated `.trellis/spec/obspt-bootstrap.md` (renamed) with the v3 adaptive contract + per-encoder template table.

### Build Gotchas (resolved)

- Dir rename OBS-Redux->OBS-PT broke build64 cache (baked abs paths) -> relocated 705 build files OBS-Redux->OBS-PT.
- VS18 2026 CMake errors on `list(GET)` when OBS_VERSION is a tagless `git describe` hash -> reconfigure with `-DOBS_VERSION_OVERRIDE=0.0.1`.
- git-bash mangles `-- /m` -> use `cmake --build --parallel`.

### Git Commits

| Hash | Message |
|------|---------|
| `7d39e49` | feat: hardware-adaptive 480fps defaults + rebrand OBS-Redux->OBS-PT |

### Testing

- [OK] `obs` target builds -> `build64/UI/RelWithDebInfo/OBS-PT.exe`; libobs + frontend-api link clean.
- [OK] Deployed to `finished/OBS-Redux/bin/64bit`; user confirmed launch OK.
- [PENDING] First-run record clip at 480fps@monitor-res (user runtime).

### Status

[IN PROGRESS] Code complete, build+launch verified, committed. Awaiting user decision on archive.

### Next Steps

- Optional: refresh user's install scene (Minecraft currently hidden in their runtime config).
- User approval to archive task (per never-auto-archive rule).


## Session 4: Welcome dialog: multi-page first-run wizard + About entry

**Date**: 2026-06-17
**Task**: Welcome dialog: multi-page first-run wizard + About entry
**Package**: plugins/win-dshow/libdshowcapture
**Branch**: `master`

### Summary

Added OBSWelcome (5-page QStackedWidget wizard): first-run auto-show (replacing the QMessageBox; run_first_run_bootstrap_if_needed now returns bool+sw), reopenable via About 'About' link (closes About). Prev hidden on page1, last-page Next=disabled equal-width 'End', X closes, hyperlinks open browser. Bundled Minecraft TTF title font + bilingual OBSPT.Welcome.* locale. Built/deployed OBS-PT.exe + font; user-verified GUI.

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `ca51e3e` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete


## Session 5: Session 6: encoding-overload diagnosis -> v143 toolchain

**Date**: 2026-06-19
**Task**: Session 6: encoding-overload diagnosis -> v143 toolchain
**Package**: plugins/win-dshow/libdshowcapture
**Branch**: `master`

### Summary

480fps 'encoding overload' diagnosed: NOT a fork bug. Clean same-workload A/B (RTX3060, window-capture MC, 480fps) showed VS2022 v143 build cuts pipeline lag ~4x vs VS18 2026 (encoding 4.1-4.7%->0.8-1.2%, rendering ~1.5%->~0.6%); rebuilt+deployed OBS-PT on v143 (deps2019/Qt-msvc2019, BUILD_VST=OFF). Visible frame-freeze was window capture failing to capture Minecraft 1.7.10 (stock-OBS limitation, won't-fix); game capture is the correct path. Spec obsredux-graphics-msvc.md updated. Install dir renamed OBS-Redux->OBS-PT.

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `3fec0cd` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete
