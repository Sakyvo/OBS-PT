# package OBS-PT 0.0.1-alpha exe installer

## Goal

Ship OBS-PT as a per-user Windows `.exe` installer (custom NSIS) that lays the
portable app tree into `<first non-C local drive>/OBS-PT` (else `C:/OBS-PT`),
bundling the v143 build + the current default config, versioned `0.0.1-alpha`.

## Confirmed Facts (explored)

- **Portable is hardcoded** (`UI/obs-app.cpp:81 const bool portable_mode = true`)
  → config lives in `<install>/obs-studio/`; installer needs NO registry/AppData
  for config. Installing to a non-C drive root avoids the Program Files admin/
  write-permission problem (matches the "便携无需C盘" design).
- **`0.0.1-alpha` is safe**: `CMakeLists.txt:100-101` splits `OBS_VERSION` on `-`,
  uses `0.0.1` for the Win binary version and keeps `0.0.1-alpha` as the display
  string. Apply via `-DOBS_VERSION_OVERRIDE=0.0.1-alpha` reconfigure + rebuild.
- Build with **v143** (see `.trellis/spec/obsredux-graphics-msvc.md`). Package
  source = `build-v143/rundir/RelWithDebInfo/` portable tree + shipped
  `UI/data/obspt-defaults/obs-studio/` config presets.
- **ATL + NSIS installed/verified**: VS2022 BuildTools has
  `Microsoft.VisualStudio.Component.VC.ATL` (`atlbase.h` present), and
  `makensis` is available at NSIS v3.12. `UI/installer/mp-installer.nsi` is OBS's
  heavy release installer (Program Files, registry, VC++ redist, admin,
  OBSInstallerUtils dep); its own header says forks should write their own →
  reference only.
- **New smoke blocker — NVENC preset compatibility**: on another Windows 11
  machine with NVIDIA driver `610.47`, recording fails with
  `nvEncGetEncodePresetConfig failed: 12 (NV_ENC_ERR_UNSUPPORTED_PARAM)`.
  User reports stock OBS 27.2.4 has no issue on that machine; newer NVIDIA
  drivers avoid the OBS-PT error but hurt game FPS. Local evidence: `jim_nvenc`
  only reads the legacy `preset` field and maps `hp` to
  `NV_ENC_PRESET_HP_GUID`; `preset2:"p1"`, `tune:"ll"`, and `multipass` are not
  read by this encoder. OBS simple-output NVENC writes `preset:"hq"`.
- **New smoke blockers — installer launch/overwrite**: when Finish-page
  "Launch OBS-PT" runs the exe directly from the installer process, OBS can fail
  video initialization even though manually launching `OBS-PT.exe` works. Also,
  NSIS must force overwrite packaged defaults on top of an existing portable
  tree; otherwise newer runtime `global.ini`/profile files can survive and skip
  the first-run welcome.

## Decision (ADR-lite)

- **Installer tech = custom minimal NSIS** (user-chosen). Rejected: adapting
  mp-installer.nsi (heavy, conflicts with portable/non-C), Inno Setup (new
  toolchain), 7-Zip SFX (can't do default-drive logic / shortcuts / uninstaller).

## Requirements

- **R1 — config sync**: update `UI/data/obspt-defaults/obs-studio/` to the current
  live config (`K:/Projects/finished/OBS-PT/obs-studio/`), selectively (exclude
  machine paths, `CookieId`, transient state) — same approach as the prior sync.
- **R2 — version**: `0.0.1-alpha`.
- **R3 — installer**: custom NSIS, per-user (no admin); folder name `OBS-PT`;
  default install dir = first non-C local fixed drive root (D→E→…), else
  `C:/OBS-PT`; lays the portable tree; Start Menu + desktop shortcut; uninstaller
  (HKCU Add/Remove entry); launch-on-finish; OBS-PT icon; keeps portable mode.
- **R4 — toolchain**: ATL + `makensis` available.

## Acceptance Criteria

- [x] `makensis` v3.12 builds `OBS-PT-0.0.1-alpha-Installer.exe`.
- [ ] Running it (defaults) installs to `<first non-C drive>/OBS-PT` with no admin
      prompt; first launch bootstraps the current default scene/profile and
      records OK; About/log show `0.0.1-alpha`.
- [x] Silent `/D=<temp>` smoke install lays key files and writes HKCU uninstall
      metadata.
- [x] Silent uninstaller removes the install dir + shortcuts + registry entry.

## Out of Scope

Code-signing, auto-update, MSI, online/streaming installer, multi-language
installer UI.

## Decisions (resolved)

- **Installer tech** = custom minimal NSIS.
- **Install location** = pre-filled, user-overridable directory page; default =
  first non-C local fixed drive `\OBS-PT` (else `C:\OBS-PT`); per-user (no admin).
- **Build completeness** = install ATL + include `obs-qsv11` + `frontend-tools`
  (preserves the hardware-adaptive design for non-NVIDIA users).
- **Config sync** = current live scene is already game_capture (safe); sync real
  diffs only, exclude machine paths / `CookieId` / transient state.
- **NVENC max-performance default** = user-selected default is the zh-CN
  "最大性能" preset, which maps to legacy `preset:"hp"` for `jim_nvenc`. Shipped
  and bootstrap templates still do not write `preset2`, `tune`, or `multipass`,
  because this encoder only reads legacy `preset`. The NVIDIA `610.47`
  `NV_ENC_ERR_UNSUPPORTED_PARAM` report remains a known compatibility risk for
  smoke testing.
- **Installer overwrite/launch** = `SetOverwrite on` before copying the staging
  tree; OBS launch paths (Finish page + OBS shortcuts) start in
  `$INSTDIR\bin\64bit`.

See `design.md` + `implement.md`.
