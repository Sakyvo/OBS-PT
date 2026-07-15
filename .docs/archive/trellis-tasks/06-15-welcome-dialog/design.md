# Design: OBS-PT Welcome dialog

Mirrors the OBSAbout pattern (`window-basic-about.{cpp,hpp}` + `forms/OBSAbout.ui`):
a `QDialog` subclass built from a `.ui` whose root is a `QWidget`, themed via
`themeID` properties, hyperlinks via `setOpenExternalLinks(true)`.

## Components (new)

- `UI/forms/OBSWelcome.ui` — root `<widget class="QWidget" name="OBSWelcome">`,
  `<class>OBSWelcome</class>`. Layout (QVBoxLayout):
  1. Header `QLabel name="title"` (themeID `aboutName`) — shows `OBSPT.Welcome.Title`.
  2. `QStackedWidget name="pages"` with 5 `QLabel` pages (`page1`..`page5`):
     each `wordWrap=true`, `textFormat=RichText`, `openExternalLinks=true`,
     `alignment=AlignTop|AlignLeft`, `textInteractionFlags=TextBrowserInteraction`.
  3. Bottom nav `QHBoxLayout`: horizontal spacer (pushes right) +
     `QPushButton name="prevBtn"` + `QPushButton name="nextBtn"`.
- `UI/window-basic-welcome.{hpp,cpp}` — `class OBSWelcome : public QDialog`:
  - `explicit OBSWelcome(QWidget *parent = nullptr, bool softwareEncoder = false);`
  - `std::unique_ptr<Ui::OBSWelcome> ui;`
  - private: `void updateNav(); int pageCount() const;`
  - slots: `void prev(); void next();`
  - ctor: `setWindowFlags(... & ~Qt::WindowContextHelpButtonHint)`, `setupUi`,
    set window title `OBSPT.Welcome.Title`, fill each page label via `QTStr`,
    set `prevBtn`/`nextBtn` text, `setOpenExternalLinks(true)` on pages, connect
    prev/next, `pages->setCurrentIndex(0)`, `updateNav()`. If `softwareEncoder`,
    append `OBSPT.Welcome.SoftwareEncoderWarning` to page 2 (or page 5).

## Navigation state machine

State = `pages->currentIndex()` in `[0, N-1]`, N=5.
- `prev()`: `setCurrentIndex(i-1)` (guarded i>0); `updateNav()`.
- `next()`: if `i < N-1` `setCurrentIndex(i+1)`; `updateNav()`. (No-op at last.)
- `updateNav()`:
  - `prevBtn->setVisible(i > 0)` — hidden on page 1.
  - if `i == N-1`: `nextBtn->setText(QTStr("OBSPT.Welcome.End"))`,
    `nextBtn->setEnabled(false)`, black style (see below).
  - else: `nextBtn->setText(QTStr("OBSPT.Welcome.Next"))`,
    `nextBtn->setEnabled(true)`, clear the End style.
- **X** = the dialog's native close button (QWidget-root QDialog provides it); no
  extra handling — closing at any page is allowed.

### "End" black-disabled style
On entering the last page: `nextBtn->setStyleSheet("color:#000; background:#000;");`
(disabled + black, non-clickable). On leaving: `nextBtn->setStyleSheet("")`.
(Exact shade refined at implement; requirement: black, non-clickable `End`.)

## Modality

Non-modal `show()` (matches OBSAbout), `setAttribute(Qt::WA_DeleteOnClose, true)`,
centered on parent. Both first-run and manual use the same non-modal path.

## Entry points

### 1. First run (replace the QMessageBox)
- `run_first_run_bootstrap_if_needed` (`obspt-bootstrap.cpp`) signature →
  `bool run_first_run_bootstrap_if_needed(const char *active_profile_name,
  config_t *active_config, bool *out_is_software)`. Returns `true` when a
  first-run/repair pass ran and the welcome should be shown; sets
  `*out_is_software` from the probe. **Remove** `ShowFirstRunRecommendationsDialog`
  + its internal QMessageBox call (header decl too). Keep all config writes +
  `mark_first_run_completed()`.
- Caller `window-basic-main.cpp:1786`:
  ```cpp
  bool sw = false;
  if (run_first_run_bootstrap_if_needed(profile_name, basicConfig, &sw)) {
      OBSWelcome *w = new OBSWelcome(this, sw);
      w->setAttribute(Qt::WA_DeleteOnClose, true);
      w->show();
  }
  ```
  Add `#include "window-basic-welcome.hpp"`.

### 2. Manual via About → "About" link
- `window-basic-about.cpp` ctor: change the `ui->about` connection from
  `SLOT(ShowAbout())` to a new handler that opens Welcome and closes About:
  ```cpp
  connect(ui->about, &ClickableLabel::clicked, this, [this]() {
      OBSWelcome *w = new OBSWelcome(OBSBasic::Get());
      w->setAttribute(Qt::WA_DeleteOnClose, true);
      w->show();
      close();
  });
  ```
  (Keep the patreon pane as the default-on-open view via the existing ctor
  `ShowAbout()` call; only the *link click* is repurposed. Authors/License
  unchanged.) Add `#include "window-basic-welcome.hpp"`.

## CMake (UI/CMakeLists.txt)

- `obs_SOURCES` (~258, after `window-basic-about.cpp`): add `window-basic-welcome.cpp`.
- `obs_HEADERS` (~322, after `window-basic-about.hpp`): add `window-basic-welcome.hpp`.
- `obs_UI` list (~418, after `forms/OBSAbout.ui`): add `forms/OBSWelcome.ui`
  → `qt5_wrap_ui` emits `ui_OBSWelcome.h`.

## Locale keys (en-US.ini + zh-CN.ini, under the OBSPT section)

Rich-text values (HTML; bullets as `•` + `<br>`). `Title`/`End` identical in both.

| key | en-US | zh-CN |
|---|---|---|
| `OBSPT.Welcome.Title` | `Welcome to OBS-PT` | `Welcome to OBS-PT` |
| `OBSPT.Welcome.Prev` | `Previous` | `上一个` |
| `OBSPT.Welcome.Next` | `Next` | `下一个` |
| `OBSPT.Welcome.End` | `End` | `End` |
| `OBSPT.Welcome.Page1` | Welcome blurb + `<a href='https://github.com/obsproject/obs-studio/releases/tag/27.2.4'>OBS 27.2.4</a>` link, portable note | 用户原文（含同一链接） |
| `OBSPT.Welcome.Page2` | "You may want to change" list + "Recommended to keep default" list + Profile-tab note | 用户原文 |
| `OBSPT.Welcome.Page3` | FPS principles list | 用户原文 |
| `OBSPT.Welcome.Page4` | resolution note | 用户原文 |
| `OBSPT.Welcome.Page5` | hotkeys/output note | 用户原文 |
| `OBSPT.Welcome.SoftwareEncoderWarning` | reuse existing `OBSPT.FirstRun.SoftwareEncoderWarning` text (480fps) | 同 |

Full verbatim zh content is in `prd.md`; en translations authored at implement.
The page hyperlink opens in the default browser via QLabel `openExternalLinks`.

## Compatibility / rollout

- Pure addition + one first-run swap. `ShowFirstRunRecommendationsDialog` removed;
  its `OBSPT.FirstRun.*` keys become unused — leave in place (harmless) or prune.
- No config/bootstrap-version change; FirstRunCompleted still gates single auto-show.
- Rollback: revert commit; nav/entry are self-contained.

## Risks / edge cases

- Long page 2: dialog sized to fit the tallest page (fixed minimum), `wordWrap`
  on; if still tight, the page QLabel can sit in a QScrollArea (implement decides).
- Theme: use existing themeID values where possible; the `End` black style is an
  inline stylesheet so it is theme-independent (requirement is literally black).
- Software-encoder notice only appended when `softwareEncoder==true` (probe result
  threaded through the bootstrap return).

## Validation

- Build `obs` target (`--parallel`); deploy `OBS-PT.exe` to `finished/OBS-Redux/bin/64bit`.
- First-run sim (config-section already `[OBSPT]`; or clear FirstRunCompleted):
  welcome auto-shows at page 1, Previous hidden.
- Nav: forward to page 5 → `End` black/disabled; back works; X closes.
- Link opens browser. About → "About" opens Welcome + closes About.
