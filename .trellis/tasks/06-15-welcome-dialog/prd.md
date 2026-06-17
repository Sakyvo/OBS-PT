# First-run Welcome dialog (OBS-PT)

## Goal

Add a multi-page **Welcome** dialog for OBS-PT that appears on first run and can
be reopened manually. It explains the fork's defaults (480fps, adaptive
resolution/encoder, CQP, etc.) so new PotPvP users understand and can adjust the
shipped config. Style follows OBS's current Qt dialogs.

## What I already know (codebase)

- OBS dialog pattern: `OBSAbout : QDialog` (`UI/window-basic-about.{cpp,hpp}`) built
  from a `.ui` (`ui_OBSAbout.h`), uses `themeID` properties for theming, and
  hyperlink `QLabel`s with `setTextInteractionFlags(TextBrowserInteraction)` +
  `setOpenExternalLinks(true)` -> opens in the default browser.
- About is shown via `OBSBasic::on_actionShowAbout_triggered()`
  (`window-basic-main.cpp:9448`): `about = new OBSAbout(this); about->show();`
  with `WA_DeleteOnClose`. Member `QPointer<OBSAbout> about`.
- First-run hook: `run_first_run_bootstrap_if_needed()` (`obspt-bootstrap.cpp:694`,
  called from `window-basic-main.cpp:1786`) currently shows
  `ShowFirstRunRecommendationsDialog()` (a simple QMessageBox) when first-run.
- First-run gating: `[OBSPT] FirstRunCompleted` in global.ini. Reuse so the
  welcome shows exactly once automatically.

## Requirements

### Behavior
- **Title**: `Welcome to OBS-PT`.
- **Multi-page** (5 pages) with bottom navigation:
  - Bottom-right **Previous** (hidden on page 1) / **Next**.
  - On the last page, **Next** becomes a disabled, black, non-clickable `End`.
  - Top-right **X** closes the dialog directly at any page.
- **Hyperlinks** open in the system default browser.
- Style follows OBS's current dialog theme (themeID / QSS).

### Entry points
- **First run**: shown automatically (once, gated by `FirstRunCompleted`).
- **Manual**: openable via About -> About; opening Welcome **closes the existing
  About dialog**.

### Page content (verbatim, page-delimited)

**Page 1 - Welcome**
> 欢迎来到OBS-PT，这是为PotPvP打造，注重于高帧录制的fork版本，基于
> [OBS 27.2.4](https://github.com/obsproject/obs-studio/releases/tag/27.2.4)，
> 也是最后一个qt5架构的版本。已做便携化，无需依赖C盘配置文件。
> 如果你是首次使用，应当了解一下它的默认配置...

**Page 2 - 默认配置**
> **或许需要更改的**
> - 文件输出目录: 安装目录下的 recordings 文件夹
> - FPS: 480
> - 分辨率: 与你的显示器相同
> - 文件格式: mp4
> - 热键: 未设定
> 与硬件相关的设置，会读取你的设备情况自动设定，但它们不一定准确
>
> **推荐保持默认的**
> - 画质参数: CQP - 26 (≥1080p), CQP - 20 (≤720p)
> - 颜色参数: Rec. 709 & Limited
> - 进程优先级: 常规
>
> 你可以在上方的**配置文件**选项卡中存储多个配置，方便切换

**Page 3 - FPS 设定原则**
> FPS的设定应该遵循实际情况而定，其原则为:
> - 是你导出FPS的整数倍 & 高于你游戏内的FPS
> - 如果你使用游戏内动态模糊，那么 240 / 360 往往已经足够
> - 如果你不使用，那么默认的 480 FPS 是一个不错的选择。当然，足够强劲的配置也可以继续提升至 600 / 660 / 720
> - 如果你不依赖 Vegas 帧混合，而是打算后期补帧得到动态模糊，那么 120 FPS 往往足矣

**Page 4 - 分辨率**
> 分辨率读取的是显示器分辨率，如果游戏内另外设置，请手动调整。务必保持基础分辨率与
> 输出分辨率相同，否则一次压缩会极大降低画质。

**Page 5 - 热键/输出**
> 未设置 热键 / 回放缓存，因为它因人而异。
> 文件输出位置可能需要手动调整。

## Acceptance Criteria

- [ ] Fresh first run shows the Welcome dialog (page 1, Previous hidden); marks
      FirstRunCompleted so it does not auto-show again.
- [ ] Previous/Next navigate; Previous hidden on page 1; on page 5 Next is a
      disabled black `End`.
- [ ] X closes at any page.
- [ ] Hyperlink (OBS 27.2.4 release) opens in the default browser.
- [ ] Opening Welcome via About closes the About dialog.
- [ ] Styling visually matches OBS's current dialogs.

## Decision (ADR-lite)

- **Localization**: bilingual locale — `OBSPT.Welcome.*` keys in **zh-CN** (user's
  Chinese) + **en-US** (English translation, also the fallback for ja/ko/others).
- **About entry**: repurpose the existing OBSAbout "About"/"关于" link — clicking it
  opens Welcome (parented to the main window) and closes the OBSAbout dialog.
  Authors/License links keep their current behavior.
- **First-run**: **replace** `ShowFirstRunRecommendationsDialog` (QMessageBox) with
  the Welcome wizard; the software-encoder warning is folded in (or kept as a
  separate small notice). FirstRunCompleted still gates the single auto-show.

## Open Questions (resolved above)

- ~~Localization~~ → bilingual locale.
- ~~About entry wiring~~ → repurpose OBSAbout "About" link.
- ~~First-run QMessageBox~~ → replace with Welcome.

## Out of Scope (tentative)

- "Don't show again" checkbox (first-run gating already handles single show).
- Changing default config values (covered by the prior 480fps task).

## Technical Notes

- Likely a new `OBSWelcome : QDialog` + `forms/OBSWelcome.ui` with a
  `QStackedWidget` for pages, mirroring OBSAbout's style/theming.
- Files in play: `UI/window-basic-main.cpp`, `UI/obspt-bootstrap.cpp`,
  `UI/window-basic-about.cpp`, `UI/CMakeLists.txt`, locale `.ini` (if localized).
