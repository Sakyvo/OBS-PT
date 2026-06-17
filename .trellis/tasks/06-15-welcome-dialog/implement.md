# Implement: OBS-PT Welcome dialog

See `prd.md` (verbatim page content, AC) and `design.md` (architecture, nav state
machine, entry wiring, CMake, locale key table). All work is UI-layer (CC lane).

## Step 0 — Locale strings (en-US.ini + zh-CN.ini)

- [ ] Under the OBSPT section of `UI/data/locale/en-US.ini` and
      `UI/data/locale/zh-CN.ini`, add `OBSPT.Welcome.*`:
      `Title`, `Prev`, `Next`, `End`, `Page1`..`Page5` (rich-text HTML; bullets
      `•`+`<br>`; page1 contains the OBS 27.2.4 `<a href>`), and reuse/add
      `SoftwareEncoderWarning`. zh = prd verbatim; en = translation per design.
- [ ] Verify both files still parse (no stray quotes); keys present in BOTH.

## Step 1 — Dialog class + .ui (PR1)

- [ ] `UI/forms/OBSWelcome.ui`: QWidget root `OBSWelcome`; QVBoxLayout = title
      QLabel + `QStackedWidget pages` (5 QLabel pages: wordWrap, RichText,
      openExternalLinks, TextBrowserInteraction, AlignTop) + bottom HBox
      (spacer + `prevBtn` + `nextBtn`).
- [ ] `UI/window-basic-welcome.hpp`: `class OBSWelcome : public QDialog` with
      `unique_ptr<Ui::OBSWelcome> ui`, ctor `(QWidget*=nullptr, bool softwareEncoder=false)`,
      `updateNav()`, slots `prev()`/`next()`.
- [ ] `UI/window-basic-welcome.cpp`: ctor strips ContextHelp button, `setupUi`,
      sets window title + page texts + button texts via `QTStr`, page
      `openExternalLinks(true)`, connects prev/next, index 0, `updateNav()`;
      append `SoftwareEncoderWarning` to page 2 when `softwareEncoder`.
      `updateNav()`: prevBtn visible iff index>0; last page → nextBtn text=End,
      disabled, black inline style; else Next/enabled/clear style.
- [ ] `UI/CMakeLists.txt`: add `window-basic-welcome.cpp` (obs_SOURCES ~258),
      `window-basic-welcome.hpp` (obs_HEADERS ~322), `forms/OBSWelcome.ui`
      (obs_UI list ~418, after OBSAbout.ui).

## Step 2 — Wire entry points (PR2)

- [ ] `obspt-bootstrap.h` + `.cpp`: change `run_first_run_bootstrap_if_needed` to
      return `bool` + add `bool *out_is_software` out-param; set it from the probe;
      return true when first-run/repair ran. Remove `ShowFirstRunRecommendationsDialog`
      (decl + def + its call). Keep all config writes + `mark_first_run_completed()`.
- [ ] `window-basic-main.cpp:1786`: capture return + sw flag; if true,
      `new OBSWelcome(this, sw)` + `WA_DeleteOnClose` + `show()`. Add include.
- [ ] `window-basic-about.cpp`: repurpose `ui->about` click → open `OBSWelcome`
      (parent `OBSBasic::Get()`) + `close()` the About dialog. Add include.
      Leave the default-open `ShowAbout()` patreon pane + Authors/License intact.

## Step 3 — Build / deploy / verify

- [ ] `cmake --build build64 --config RelWithDebInfo --target obs --parallel`
      (reconfigure picks up new sources/.ui; OBS_VERSION_OVERRIDE already cached).
- [ ] Deploy `build64/UI/RelWithDebInfo/OBS-PT.exe` + updated locale `en-US.ini`/
      `zh-CN.ini` → `finished/OBS-Redux/bin/64bit/` and `data/obs-studio/locale/`.
- [ ] Verify (user runtime): first-run auto-show (page1, Prev hidden); nav to
      page5 → black disabled `End`; back; X closes; OBS 27.2.4 link → browser;
      About → "About" opens Welcome + closes About; zh/en strings correct.

## Step 4 — Self-review gate

- [ ] No leftover `ShowFirstRunRecommendationsDialog` references; first-run shows
      Welcome exactly once (FirstRunCompleted gate intact).
- [ ] `OBSPT.Welcome.*` keys present in BOTH locales; page1 link correct.
- [ ] prevBtn hidden on page1; End disabled+black on page5; X works any page.
- [ ] CMake lists the 3 new entries; `ui_OBSWelcome.h` generated.

## Step 5 — Spec / commit (Phase 3)

- [ ] Update `.trellis/spec/obspt-bootstrap.md` (first-run now shows OBSWelcome;
      `run_first_run_bootstrap_if_needed` returns bool + sw out-param).
- [ ] Commit (local master).

## Rollback points

- After Step 1: dialog exists but unwired — harmless, no behavior change.
- Full revert: single commit revert (self-contained additions + one first-run swap).
