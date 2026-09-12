Status: review

## What to build

Make the NSIS installer auto-detect the system UI language: Simplified
Chinese systems see the Chinese installer, every other language falls back to
English. No language-selection dialog — pure auto-detection.

Today `UI/installer/obspt-setup.nsi` loads only `MUI_LANGUAGE "English"`,
so the installer UI is English-only.

Change:
- Load both languages: `!insertmacro MUI_LANGUAGE "English"` (first = default
  fallback) and `!insertmacro MUI_LANGUAGE "SimpChinese"`.
- Rely on NSIS built-in installer-language selection (see NSIS "Multiple
  Languages"): on startup NSIS calls `GetUserDefaultUILanguage()` itself,
  matches it against the loaded languages, and falls back to the first one
  (English) when there is no match. No custom detection code, no
  `MUI_LANGDLL_DISPLAY`, and no popup — Simplified-Chinese systems get the
  Chinese UI automatically and all others get English.
- Localize all installer user-facing text via `LangString` (English +
  SimpChinese): the reset-configuration page header + two body labels, the
  confirm-overwrite MessageBox, the OBS-running and unknown-state MessageBoxes,
  the move-failed and merge-failed MessageBoxes, and the finish-page Run text.
  `VIAddVersionKey` block info stays English (file metadata only).

Only English and Simplified Chinese are shipped for the installer. The
in-app Welcome wizard and OBS locale stay five-language (issue 045).

## Acceptance criteria

- [x] `obspt-setup.nsi` loads `English` and `SimpChinese` MUI languages.
- [x] `.onInit` detects `0x0804` and switches to SimpChinese; all other
      system languages fall back to English.
- [x] No `MUI_LANGDLL_DISPLAY` — no language popup.
- [x] All installer-facing English strings have SimpChinese mirrors
      (welcome, directory, reset-configuration page, finalize/merge failure
      box, finish).
- [x] `test/win/obspt-installer-config-reset-smoke.ps1` static-asserts both
      languages are loaded and the detection branch exists.
- [x] Manual: run the installer on a Simplified-Chinese Windows account ->
      Chinese UI; on an English account -> English UI.

## Blocked by

None - can start immediately
