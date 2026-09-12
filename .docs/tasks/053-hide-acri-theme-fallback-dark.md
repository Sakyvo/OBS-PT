Status: review

## What to build

Hide the legacy `Acri` theme from Settings › 通用 › 主题 and migrate users
who already selected it back to Dark.

Two parts:

1. **Combo filter** — `LoadGeneralSettings()` fills the theme combo by
   iterating `data/themes/*.qss`. Skip `Acri` so it never appears in the
   dropdown. Keep `Acri.qss` on disk (hidden only, minimal rollback).

2. **Startup fallback** — in `obs-app.cpp` `InitTheme()`, after reading the
   persisted theme name and before applying it, if the name is `Acri`, force
   it to `Dark` and persist `General.CurrentTheme2 = Dark` so upgraded users
   silently land on Dark and never see a broken/absent theme.

## Acceptance criteria

- [ ] The theme dropdown does not list Acri (combo enumeration skips it).
- [ ] `Acri.qss` remains shipped under `data/themes/`.
- [ ] Startup with persisted theme `Acri` applies Dark and rewrites the
      persisted theme to `Dark`.
- [ ] Users on other themes (Dark/System/Yami/Rex/...) are unaffected.
- [ ] Static/smoke check asserts the combo-build loop skips Acri and the
      startup fallback maps Acri to Dark.
- [ ] Manual: preselect Acri in an old config, launch — UI comes up in Dark;
      open Settings › 通用 › 主题 — Acri absent from the list.

## Blocked by

None - can start immediately
