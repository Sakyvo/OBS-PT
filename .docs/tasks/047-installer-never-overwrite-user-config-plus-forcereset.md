Status: review

## What to build

Make the installer stop overwriting the user's User Data Root on upgrade,
and add an explicit "覆盖安装设置" (force-reset) option.

Background / root cause: the packaged staging tree ships preset templates
under `obs-studio\` (`global.ini`, `PotPvP\basic.ini`, `recordEncoder.json`,
`streamEncoder.json`, `service.json`, `scenes\PotPvP.json`), populated by
`install(DIRECTORY data/obspt-defaults/ ...)` in `UI/CMakeLists.txt`. The
installer's `SetOverwrite on` + `File /r "${SOURCE_DIR}\*.*"` unconditionally
copies that whole tree over an existing `$INSTDIR\obs-studio\`, so even a
same-generation reinstall (where `NeedsConfigReset` returns 0 and
`PrepareExistingConfig` returns early) clobbers the user's real config —
hotkeys, recording paths, encoder, audio-mode state — with the preset
templates. This is why "热键和视频输出位置安装后仍被吞" despite issue 039.

Change:

1. **Upgrade never touches User Data Root.** When `$INSTDIR\obs-studio\`
   already exists, the installer copies ONLY program files (`bin\`,
   `data\`, `obs-plugins\`) and does NOT deploy the staged `obs-studio\`
   templates at all. `obs-studio\` templates are deployed only on a fresh
   install (when `$INSTDIR\obs-studio\` does not exist yet). Deploy obs-studio
   from staging separately from the indiscriminate `File /r`, gated on
   "fresh install".

2. **Add a "覆盖安装设置 (恢复出厂默认)" option** shown only when an
   existing User Data Root is detected (i.e. upgrade / repair, not fresh
   install). Put the checkbox on a custom confirmation page after the
   directory page (same style as the existing `ConfigPageCreate` reset page).
   Default unchecked. When checked, confirm with a dialog:
   "此操作将把 OBS-PT 配置重置为出厂默认,当前配置将备份至 obs-studio.__obspt_backup。是否继续?"
   On confirmed force-reset: rename the existing `obs-studio` to
   `obs-studio.__obspt_backup` (reuse the backup-name reservation logic),
   then deploy the fresh templates; after install surface the backup
   location on the finish page. This is distinct from the legacy-generation
   reset+merge flow (issue 039) — force-reset intentionally discards all
   preserved categories.

3. Reconcile with `CONFIG_GENERATION` / `NeedsConfigReset`: the legacy
   reset+merge flow stays for below-floor generations; the explicit
   force-reset checkbox is an independent user override for current-generation
   upgrades.

## Acceptance criteria

- [x] Upgrade/reinstall over an existing `obs-studio\` does NOT deploy the
      staged templates (`global.ini`, PotPvP preset files, scenes) — program
      files still update.
- [x] Fresh install (no existing `obs-studio\`) deploys the templates.
- [x] "覆盖安装设置" checkbox appears only when an existing User Data Root
      is detected (not on fresh install); default unchecked.
- [ ] Checking it shows the force-reset confirmation dialog; confirming
      backs up existing config to `obs-studio.__obspt_backup` then deploys
      fresh templates; finish page notes the backup location.
- [x] `test/win/obspt-installer-config-reset-smoke.ps1` extended:
      upgrade path does not deploy obs-studio templates; force-reset writes
      the backup and renames, fresh install deploys.
- [x] `.docs/obspt-bootstrap.md` NSIS installer contract updated for
      upgrade-vs-fresh obs-studio deployment and the force-reset option.
- [ ] Handbook (manual): reinstall over a config whose hotkey/output path
      differ from default keeps them; fresh install gets defaults.

## Blocked by

None - can start immediately