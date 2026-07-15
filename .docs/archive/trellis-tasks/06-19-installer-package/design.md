# Design — OBS-PT 0.0.1-alpha installer

## Pipeline
config-sync → version bump → complete v143 rebuild (with ATL) → assemble staging
tree → NSIS package.

## 1. Config sync (R1)
Diff live `D:/OBS-PT/obs-studio/` vs shipped
`UI/data/obspt-defaults/obs-studio/`; apply real differences only. Current scene
is already `game_capture`(Minecraft) + `monitor_capture`(Display, hidden) + 桌面音频
— safe (not the broken window-capture state). EXCLUDE machine-specifics:
`RecFilePath`/`FilePath` (bootstrap rewrites `<Install Root>/recordings` at
first-run), `CookieId`, global.ini transient state. Keep: 1080p base=output /
480fps / NV12 / 709 / Partial / jim_nvenc / CQP-by-resolution. For the latest
GUI layout request, sync only `global.ini` `[BasicWindow]` and
`[ScriptLogWindow]`; do not ship `[OBSPT]`, profiler data, backups, or runtime
version flags.

## 2. Version (R2)
Reconfigure build-v143 with `-DOBS_VERSION_OVERRIDE=0.0.1-alpha`. Safe: `CMakeLists
:100-101` splits on `-` → binary ver `0.0.1`, display string `0.0.1-alpha`
(About / log / installer filename).

## 3. Complete v143 build (R3/R4)
- Install ATL into VS2022 BuildTools: `Microsoft.VisualStudio.Component.VC.ATL`.
- Rebuild ALL_BUILD (RelWithDebInfo) → `obs-qsv11` + `frontend-tools` now compile.
- Verify rundir has `obs-qsv11.dll` + `frontend-tools.dll` alongside the rest.

## 4. Staging tree (clean package source)
`build-v143/_pkg/OBS-PT/` mirroring the portable layout:
- `bin/64bit/` ← rundir bin (v143 dlls/exe + Qt/ffmpeg deps)
- `obs-plugins/64bit/` ← rundir plugins (incl. qsv + frontend-tools)
- `data/` ← rundir data
- `obs-studio/` ← shipped `UI/data/obspt-defaults/obs-studio/` (FRESH defaults —
  NOT the user's live config; no recordings/logs/CookieId)
Portable is hardcoded (`obs-app.cpp:81`) → no marker file needed.

## 5. NSIS installer (R3) — new `UI/installer/obspt-setup.nsi`
- `Unicode true`, `RequestExecutionLevel user` (no admin), MUI2.
- `.onInit`: compute default `$INSTDIR` — enumerate fixed drives (FileFunc
  `${GetDrives} "HDD"` / `GetDriveType==DRIVE_FIXED`), first letter > C; fallback
  `C:`. Append `\OBS-PT`.
- Pages: Welcome → **Directory (pre-filled default, user-overridable)** → Install
  → Finish (run OBS-PT.exe).
- Install: `SetOverwrite on`, then recurse-pack `<staging>` → `$INSTDIR`; Start
  Menu (`$SMPROGRAMS\OBS-PT`) + Desktop shortcuts → `bin\64bit\OBS-PT.exe` with
  working directory `$INSTDIR\bin\64bit`; write uninstaller + HKCU entry
  `Software\Microsoft\Windows\CurrentVersion\Uninstall\OBS-PT`.
- Finish-page launch uses a custom `LaunchOBS` function that sets
  `$OUTDIR/$cwd` to `$INSTDIR\bin\64bit` before executing `OBS-PT.exe`.
- Uninstall: remove `$INSTDIR` + shortcuts + HKCU entry.
- `OutFile "OBS-PT-0.0.1-alpha-Installer.exe"`; icon = OBS/OBS-PT `.ico`.
- Build: `makensis UI/installer/obspt-setup.nsi`.

## 6. First-run welcome timing
`run_first_run_bootstrap_if_needed()` applies encoder/output-path defaults and
returns whether `OBSWelcome` should be shown. It must not write
`FirstRunCompleted=true` before the dialog is visible. `OBSBasic::OBSInit()`
posts welcome creation with `QTimer::singleShot(0, ...)`, raises/activates it,
and connects the dialog `finished` signal to `mark_first_run_completed()`. The
welcome final page keeps its End button enabled and calls `accept()`.

## Risks / rollback
- Drive enum MUST be `DRIVE_FIXED` only (skip removable/network) → never default to
  a USB stick.
- Per-user + drive-root path → writable without admin; if read-only the user picks
  another on the directory page.
- build-v143 + staging + installer are all new/separate; the working deployed
  install is untouched until the user runs the new installer.

## Out of scope
Code-signing, auto-update, MSI, multi-language installer UI.
