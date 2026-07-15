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
   - 2026-06-21 GUI layout refresh: sync only `[BasicWindow]` and
     `[ScriptLogWindow]` from current `D:\OBS-PT\obs-studio\global.ini` into the
     shipped `global.ini`. Do not sync `[OBSPT] FirstRunCompleted`,
     `BootstrapVersion`, profiler data, scene `.bak`, runtime paths, or
     `CookieId`.
   - 2026-06-20 smoke blocker investigation: on Windows 11 + NVIDIA driver `610.47`,
     `jim_nvenc` failed at `nvEncGetEncodePresetConfig` with
     `NV_ENC_ERR_UNSUPPORTED_PARAM` when using shipped `preset:"hp"`. Stock OBS
     27.2.4 works on the same machine, and OBS simple-output NVENC writes
     `preset:"hq"`. User later explicitly chose the zh-CN "最大性能" preset for
     the default, so keep `jim_nvenc` at legacy `preset:"hp"` in both shipped
     `recordEncoder.json` and `apply_encoder_to_profile()`, but do not write
     `preset2`/`tune`/`multipass` because this encoder does not read them.
     Rebuild `OBS-PT.exe`; staging JSON-only replacement is insufficient because
     first-run/repair can rewrite the profile from bootstrap.
2. **Version (R2)**: reconfigure with bundled cmake
   `... -B build-v143 ... -DOBS_VERSION_OVERRIDE=0.0.1-alpha` (+ existing flags).
3. **Build (R3)**: `cmake --build build-v143 --config RelWithDebInfo --parallel`.
   Verify `obs-qsv11.dll` + `frontend-tools.dll` built; 0 errors.
4. **Stage (R3)**: assemble `build-v143/_pkg/OBS-PT/` = rundir
   bin+obs-plugins+data + `UI/data/obspt-defaults/obs-studio/`.
5. **NSIS script**: write `UI/installer/obspt-setup.nsi` (per design.md §5).
6. **Build installer**: `makensis UI/installer/obspt-setup.nsi` →
   `OBS-PT-0.0.1-alpha-Installer.exe`.
7. **First-run welcome fix**: late bootstrap no longer marks
   `FirstRunCompleted` before showing the dialog; the main window schedules the
   welcome on the next Qt event-loop turn, raises/activates it, and marks first
   run complete on dialog finish. The welcome final-page End button accepts the
   dialog.
8. **Smoke test (user)**: run installer (accept default path) → no admin prompt →
   installs to `<first non-C drive>/OBS-PT` → launch → About/log = `0.0.1-alpha`
   → first-run bootstraps default scene → record OK → run uninstaller.

## Validation
- `obs-qsv11.dll` + `frontend-tools.dll` present in staging.
- `OBS-PT-0.0.1-alpha-Installer.exe` produced; default `$INSTDIR` = first non-C
  fixed drive `\OBS-PT`, overridable on the directory page.
- Silent install smoke test with `/D=<repo>/build-v143/_pkg/_smoke/OBS-PT`
  installed key files, wrote HKCU uninstall metadata, and silent uninstall removed
  the install dir, registry key, desktop shortcut, and Start Menu shortcuts.
- 2026-06-21 rerun after NVENC preset decision: rebuilt `OBS-PT.exe`,
  regenerated `OBS-PT-0.0.1-alpha-Installer.exe`, silently installed to
  workspace `_smoke/OBS-PT`, verified installed `recordEncoder.json` uses
  `preset:"hp"` without `preset2`/`tune`/`multipass` and PotPvP game capture
  targets Lunar Client 1.7.10, then silently uninstalled and restored the
  pre-existing `D:\OBS-PT` HKCU uninstall entry/shortcuts.
- 2026-06-21 rerun after installer launch/overwrite fix: regenerated installer,
  pre-seeded a smoke install dir with newer `global.ini` containing
  `FirstRunCompleted=true` and newer `recordEncoder.json` using `preset:"hp"`;
  silent install overwrote both back to fresh defaults (`global.ini` has no
  `[OBSPT]`, `recordEncoder.json` uses `preset:"hp"`). Verified Start Menu and
  Desktop OBS shortcuts have working directory `<install>/bin/64bit`, then
  silently uninstalled and restored the pre-existing `D:\OBS-PT` HKCU
  uninstall entry/shortcuts.
- 2026-06-21 rerun after GUI/welcome fix: shipped `global.ini` includes the
  current `D:\OBS-PT` `[BasicWindow]` and `[ScriptLogWindow]` layout sections
  only. Fresh first-run no longer writes `[OBSPT] FirstRunCompleted=true` during
  late bootstrap; it is written only after `OBSWelcome` finishes.
- 2026-06-21 final package smoke after GUI/welcome fix: rebuilt
  `OBS-PT.exe`, recopied it plus shipped defaults into `build-v143/_pkg/OBS-PT`,
  regenerated `OBS-PT-0.0.1-alpha-Installer.exe`, then silently installed to
  workspace `_smoke/OBS-PT`. Verified installed `global.ini` has `[BasicWindow]`
  and `[ScriptLogWindow]` but no `[OBSPT]`, `recordEncoder.json` stays
  `jim_nvenc` `preset:"hp"` without `preset2`/`tune`/`multipass`, and PotPvP
  captures Lunar Client 1.7.10, not 1.8.9. Silent uninstall removed the smoke
  install dir and HKCU uninstall key; pre-existing `D:\OBS-PT` uninstall
  metadata/shortcuts were restored.
- Remaining manual user smoke: interactive default install path, launch, About/log
  display string, record OK.

## Lane note
Build/packaging infra + 1 NSIS script (+ minor config edits). Mostly ops; flag to
user if they prefer Codex to drive execution.
