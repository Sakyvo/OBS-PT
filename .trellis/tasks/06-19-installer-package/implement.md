# Implement — OBS-PT 0.0.1-alpha installer

## Prereqs (verified)
1. **ATL** is installed in VS2022 BuildTools:
   `K:\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\atlmfc\include\atlbase.h`.
2. **NSIS** is installed: `C:/Program Files (x86)/NSIS/makensis.exe`, v3.12.

## Steps
1. **Config sync (R1)**: diff live vs `UI/data/obspt-defaults/obs-studio/`; apply
   real diffs only (exclude `RecFilePath`/`FilePath`/`CookieId`/global state).
   Confirm shipped scene = game_capture. Done: Minecraft window default kept on
   Lunar Client 1.7.10; CQP encoder defaults cleaned.
2. **Version (R2)**: reconfigure with bundled cmake
   `... -B build-v143 ... -DOBS_VERSION_OVERRIDE=0.0.1-alpha` (+ existing flags).
3. **Build (R3)**: `cmake --build build-v143 --config RelWithDebInfo --parallel`.
   Verify `obs-qsv11.dll` + `frontend-tools.dll` built; 0 errors.
4. **Stage (R3)**: assemble `build-v143/_pkg/OBS-PT/` = rundir
   bin+obs-plugins+data + `UI/data/obspt-defaults/obs-studio/`.
5. **NSIS script**: write `UI/installer/obspt-setup.nsi` (per design.md §5).
6. **Build installer**: `makensis UI/installer/obspt-setup.nsi` →
   `OBS-PT-0.0.1-alpha-Installer.exe`.
7. **Smoke test (user)**: run installer (accept default path) → no admin prompt →
   installs to `<first non-C drive>/OBS-PT` → launch → About/log = `0.0.1-alpha`
   → first-run bootstraps default scene → record OK → run uninstaller.

## Validation
- `obs-qsv11.dll` + `frontend-tools.dll` present in staging.
- `OBS-PT-0.0.1-alpha-Installer.exe` produced; default `$INSTDIR` = first non-C
  fixed drive `\OBS-PT`, overridable on the directory page.
- Silent install smoke test with `/D=<repo>/build-v143/_pkg/_smoke/OBS-PT`
  installed key files, wrote HKCU uninstall metadata, and silent uninstall removed
  the install dir, registry key, desktop shortcut, and Start Menu shortcuts.
- Remaining manual user smoke: interactive default install path, launch, About/log
  display string, record OK.

## Lane note
Build/packaging infra + 1 NSIS script (+ minor config edits). Mostly ops; flag to
user if they prefer Codex to drive execution.
