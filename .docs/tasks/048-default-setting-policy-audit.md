Status: review

## What to build

Codify the default-setting policy — `预置` (preset, only fill absent/placeholder)
vs `强覆盖` (force-override, the only member being "以管理员身份运行 OBS-PT") —
as an audited behavior contract across the bootstrap / installer, and
document it.

This issue complements 047 (installer deployment) by auditing the bootstrap
`config_set_*` writes so that none of them overwrite an existing user value
on a normal launch, and confirming `RunAsAdministrator` is the sole
force-override member.

Audit the unconditional writes in `UI/obspt-bootstrap.cpp`:

- `EnableAutoUpdates=true` (Global `[General]`)
- `Basic.Profile` / `ProfileDir` / `SceneCollection` / `SceneCollectionFile`
- `Video.BaseCX/CY/OutputCX/CY/FPSNum/FPSDen/FPSType = 480` monitor adaptive
  (repair-gated, `g_late_bootstrap_needed`)
- `AdvOut.RecEncoder` / `AdvOut.Encoder` (probe write)
- record path writes (already preset via `OBSPTPlanRecordPaths`)

Confirm each is either (a) first-run/repair-only where no user config exists,
(b) repair-gated so it never runs on a normal launch, or (c) genuinely
absent/placeholder-only (preset). Any write that would clobber an existing
user value on a normal startup must be made preset. `RunAsAdministrator` is
the ONLY force-override member: it uses `config_set_default_bool(...,true)`
(pre-set semantics — user-set value preserved across upgrades), and is reset
to default `true` only by the force-reset install (issue 047); never forced
true on a normal launch.

## Acceptance criteria

- [x] Audit documents that no bootstrap `config_set_*` overwrites an existing
      user value on normal startup; any violation fixed to preset semantics.
- [x] `RunAsAdministrator` confirmed as the sole force-override member:
      default `true`, user value preserved across upgrades, reset only by
      force-reset install.
- [x] `.docs/obspt-bootstrap.md` gains the preset/force-override policy
      section and the audit's conclusions.
- [x] `CONTEXT.md` terms 预置设置 / 强覆盖设置 / 覆盖安装设置 present (already
      drafted) and consistent with the audit.
- [x] `test/win/obspt-installer-config-reset-smoke.ps1` (or an offline test)
      asserts the no-clobber invariant on a representative key.
- [ ] Manual: user edits Output Path + disables RunAsAdministrator, then
      upgrades → both preserved; force-reset install → both reset to default.

## Blocked by

None - can start immediately (independent audit of bootstrap writes; aligns
with but does not depend on 047's installer work)