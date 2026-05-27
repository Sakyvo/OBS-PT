# PRD: Step 1 S8 — Acceptance Testing & Build Setup

**Status**: In Progress (B1–B5 ✅ / B6 manual exec)
**Owner**: Sakyvo
**Created**: 2026-05-26
**Last Updated**: 2026-05-27

---

## Problem Statement

OBSRedux Step 1 (M1–M7) 代码已实施并通过 trellis-check，但仍是"代码层面正确"。还需要回答两个用户视角的问题：

1. **能不能在 2026 年的开发机上把它编出来？** OBS Studio 27.x 是 2022 年的代码（cmake_minimum_required 老语法、jansson 内嵌 deps、子模块依赖等），开发机却是 VS2026 Community + CMake 4.3.2 + 还没安装 Qt5/obs-deps。toolchain 跨度 3 年以上。
2. **编出来后，便携部署 + 首启自举行为是否真的符合 PRD？** M1-M7 是"在 OBS 启动链中注入便携路径解析 + 首启写入"的浸入式改造，无法仅靠单元测试验证；必须实际启动、检查 AppData 是否被绕过、首启弹窗是否出现、录像是否落到便携目录。

如果上述任一未验证，Step 1 不能视为"用户可交付"。

## Solution

S8 拆成 6 个有序构建任务 (B1-B6) 一次性完成"从空盘到可运行便携发行包 + AC 验收"全链路：

- **B1-B2**: 识别 + 安装编译依赖（OBS deps2019 预编译包 + Qt 5.15.2 MSVC 2019 64-bit + VS2026 Community 已装）
- **B3**: CMake configure 通过，配出全部 plugin 依赖
- **B4**: 编译 obs64.exe + libobs + 所有 plugin DLL
- **B5**: `cmake --install` 打包到 `dist-test/`，确认 PotPvP 预置文件（`global.ini` + 4 个 Profile 文件 + 1 个 Scene）与根目录 `recordings/` 占位全部归位
- **B6**: 先覆盖 `D:\OBSRedux\` 验证旧坏目录迁移，再干净部署到 `K:\Projects\finished\OBS-Redux\` 验证全新安装首启；手工执行 AC1/2/3/5/6/8/9（单机可验证子集）

把"过程中发现的 6 个 toolchain 兼容性修复"固化进本 PRD（见 Implementation Decisions），使任意第三方/未来 AI 可重放此构建配方。

## B6 Manual Acceptance Findings（2026-05-27）

手工启动 `D:\OBSRedux\bin\64bit\obs64.exe` 后，截图暴露出 4 个验收阻断点：

1. **界面语言不符合当前系统环境**：Windows 系统语言为中文，但 OBSRedux 首启弹窗、更新弹窗、自动配置向导与设置页均显示英文。
   - 代码证据：预置 `UI/data/obsredux-defaults/obs-studio/global.ini` 显式写 `Language=en-US`；`UI/obs-app.cpp` 的 `DEFAULT_LANG` 也是 `en-US`。
2. **上游 OBS 自动更新弹窗仍会出现**：启动后显示 "OBS Studio 32.1.2" 更新提示，说明 `EnableAutoUpdates` 与 Win32 `AutoUpdateThread` 仍走上游逻辑。
   - 代码证据：`UI/obs-app.cpp::InitGlobalConfigDefaults()` 默认 `EnableAutoUpdates=true`；`UI/window-basic-main.cpp::TimedCheckForUpdates()` 在 `LastVersion < LIBOBS_API_VER` 时强制重置 `LastUpdateCheck=0` 并触发 `CheckForUpdates(false)`。
3. **上游 Auto-Configuration Wizard 仍会出现**：截图显示自动配置向导，说明上游首次启动判定仍未被 OBSRedux 预置 Global Config 抑制。
   - 代码证据：`UI/window-basic-main.cpp` 中 `!General.FirstRun && !General.LastVersion && !Active()` 会排队调用 `on_autoConfigure_triggered()`；预置 `global.ini` 没有写 `General.FirstRun=true` 或 `General.LastVersion=<current>`。
4. **PotPvP Profile / Scene Collection 未被完整激活**：设置页仍显示 Simple output、默认视频路径 `C:\Users\ASUS\Videos`，未体现预置 `basic.ini` 的 Advanced + 360fps + `<Install Root>/recordings`。
   - 代码证据：预置 `global.ini` 只写 `SceneCollection=PotPvP` 与 `Profile=PotPvP`，缺少上游实际读取的 `SceneCollectionFile=PotPvP` 与 `ProfileDir=PotPvP`；首启 bootstrap 当前只改写已存在 Profile 文件，不负责修复 Global Config 的激活键。
   - 时序证据：`UI/window-basic-main.cpp::OBSBasic::OBSInit()` 先构造 `SceneCollectionFile` 路径并调用 `InitBasicConfig()` 打开 Profile，再在 `obs_post_load_modules()` 后调用 `run_first_run_bootstrap_if_needed()`。因此当前首启 bootstrap 改写 PotPvP Profile 的时机太晚，不能影响已打开的 `basicConfig`。

初步判断：问题不是单一“函数名/文件夹未从 obs 改为 obs-redux”。`obs-studio/` 作为 User Data Root 物理目录在 `CONTEXT.md` 中明确保留，不能仅凭目录名判定为 bug。当前更像是 **OBSRedux 自举层没有完全接管上游首次启动状态机**：语言默认、更新默认、Auto Config Wizard 门槛、Global Config 激活键仍沿用 OBS Studio 首启语义。

### Resolved Planning Decisions

1. **语言预置策略**：删除预置 `global.ini` 中的 `Language=en-US`，不要强制写 `zh-CN`。让上游已有的 OS preferred locale 逻辑按系统语言自动选择界面语言；中文系统应显示中文，非中文系统不应被强行切到中文。
2. **更新策略**：注释/禁用上游 OBS 自动更新入口，避免 OBSRedux 用户看到 OBS Studio 官方版本更新弹窗。后续若需要更新能力，应单独设计 OBSRedux 自己的发布渠道与更新源，不复用上游 OBS feed。
3. **Auto-Configuration Wizard 策略**：跳过上游首启自动配置向导，不允许其在 OBSRedux 首启时自动弹出或覆盖 PotPvP 预置 Profile。保留用户从菜单手动打开的能力，作为高级用户自测硬件/网络的工具。
4. **User Data Root 目录名策略**：不要把物理目录 `obs-studio/` 改成 `obs-redux/`。该目录名在 `CONTEXT.md` 中已定义为兼容上游 Profile / Scene / plugin config 文件结构的 User Data Root，不是 UI 品牌名；Step 1 去上游化聚焦 UI 品牌、更新入口与首启状态机。
5. **Global Config 预置策略**：保留 `global.ini`，但把它补成完整的 PotPvP 启动索引，而不是把它改成 Profile 配置文件。`global.ini` 应负责激活指针与全局首启状态：`Profile=PotPvP`、`ProfileDir=PotPvP`、`SceneCollection=PotPvP`、`SceneCollectionFile=PotPvP`；FPS、CQP、录制路径等 PotPvP 参数仍放在 `basic/profiles/PotPvP/basic.ini` 与 encoder JSON 中。
6. **Global Config 自愈边界**：启动代码可以在 OBSRedux 首次启动阶段补齐/覆盖 PotPvP 启动索引与上游首启状态，但不能每次启动都覆盖用户选择。`[OBSRedux] FirstRunCompleted=true` 后，用户手动切换的 Profile / Scene Collection 必须保留。
7. **预置文件完整性策略**：正常发行包不应出现 PotPvP 预置文件损坏或缺失。一旦启动期发现 `basic.ini`、`recordEncoder.json`、`streamEncoder.json`、`service.json`、`PotPvP.json` 或 `global.ini` 缺失/解析失败，OBSRedux 必须阻断启动并提示发行包损坏或预置文件缺失，不允许静默降级成上游空白配置。
8. **预置文件完整性失败弹窗**：完整性检查失败时显示致命错误对话框，列出第一个缺失或损坏的关键文件，提示用户重新解压 OBSRedux 发行包。弹窗只提供“打开安装目录”和“退出”两个操作；不继续进入主界面。该交互已确认。
9. **用户可见品牌策略**：本轮纳入最小用户可见品牌修复，主窗口标题必须从 `OBS ...` 改为 `OBSRedux ...`，避免用户误认为仍在运行上游 OBS Studio。可见弹窗/关于页中明显面向用户的 `OBS Studio` 品牌文案同步收敛到 `OBSRedux`；内部类名、函数名、插件 API、`obs64.exe` 文件名与 `obs-studio/` 物理目录名不在本轮重命名范围。
10. **更新菜单入口策略**：隐藏上游 “Check for Updates / 检查更新” 菜单入口，而不是保留禁用态或点击后提示暂不可用。未来接入 OBSRedux 自己的发布渠道时，可在同一菜单位置恢复。
11. **未完成首启的覆盖策略**：如果 `global.ini` 已存在但缺少 `[OBSRedux] FirstRunCompleted=true`，即使其中已有 Profile / Scene Collection 值，也按 OBSRedux 未完成初始化处理，允许覆盖成 PotPvP 初始启动索引。正常用户不会手动编辑该文件；用户自定义只在 `FirstRunCompleted=true` 之后受保护。
12. **录像目录策略**：录像输出目录统一为 `<Install Root>/recordings/`，不放在 `<Install Root>/obs-studio/recordings/`。`recordings/` 是用户产出内容，不是 User Data Root 内的配置数据；CMake install 预置目录、首启弹窗、Profile `RecFilePath` 与 S8 验收口径必须统一到 Install Root 根目录。
13. **上游首启状态门闩**：OBSRedux 首次启动阶段必须同步写入 `General.FirstRun=true` 与 `General.LastVersion=<当前版本>`，让上游状态机不再触发 Auto-Configuration Wizard、What's New 或上游版本更新误判。OBSRedux 自己的真实首启完成状态仍只以 `[OBSRedux] FirstRunCompleted=true` 为准。
14. **首启确认弹窗关闭行为**：OBSRedux 三项首启确认弹窗的右上角 `X` 等同于“开始使用”。该弹窗只是默认值告知，不是阻断式确认；用户关闭后仍写入 `[OBSRedux] FirstRunCompleted=true`，后续启动不再重复弹出。
15. **首启完成标记写入时机**：`[OBSRedux] FirstRunCompleted=true` 必须在首启确认弹窗关闭后写入。预置文件完整性检查、Global Config 自愈、Profile 路径/编码器改写必须先完成；如果程序在弹窗显示前崩溃，下次启动应重新执行首启自举。
16. **首启自举拆分策略**：首启自举确认拆成早期 Global Config / 预置完整性阶段与后期编码器探测阶段。Global Config 自愈、PotPvP 启动索引补齐、预置文件完整性检查必须发生在 `OBSBasic::InitBasicConfig()` 之前；硬件编码器探测仍必须发生在 `obs_post_load_modules()` 之后。
17. **标题 Portable Mode 策略**：主窗口标题中去掉 `Portable Mode`。OBSRedux 的 Portable Mode 是永久强制行为，不是用户可切换状态，不应在标题中沿用上游便携模式提示。
18. **标题 Studio 策略**：主窗口标题中去掉 `Studio`。即使处于 Studio Mode，标题也统一显示 `OBSRedux <version> - Profile: ... - Scenes: ...`，不使用 `OBSRedux Studio` 或任何接近 `OBS Studio` 的品牌形态。
19. **可执行文件名策略**：本轮暂不把 `obs64.exe` 改成 `obsredux64.exe`。exe 文件名重命名涉及 CMake output、install 布局、快捷方式、验收命令与潜在插件假设，放入后续发布打包待办；S8 只修用户可见窗口标题与弹窗/关于页品牌。
20. **About 页策略**：About 页产品描述改为 OBSRedux，并明确说明 OBSRedux 是 OBS 的 fork 项目；保留上游 OBS Project / Authors / License 入口与署名，不抹除原项目贡献与 GPL 许可信息。
21. **品牌文案替换范围**：只替换明显指代产品品牌的用户可见文案，不做全量 `OBS` 字符串扫替换。窗口标题、About 简介、更新弹窗、致命错误/首启弹窗标题等品牌入口必须收敛到 `OBSRedux`；功能语境中的 OBS 技术术语、插件/API 名称、作者/许可证文本暂不替换，避免破坏上下文或翻译键。
22. **首启弹窗默认值文案策略**：首版首启确认弹窗里的 CQP 25 与 360 fps 可以作为固定 OBSRedux PotPvP 默认值展示，不需要运行时动态解析配置文件；但验收必须确认弹窗文案与 `basic/profiles/PotPvP/basic.ini`、`recordEncoder.json` 中的实际预置值一致。
23. **Updater 保留策略**：保留上游 updater 相关源码与类型，但本轮断开运行路径：`TimedCheckForUpdates()` 不触发上游检查，菜单入口隐藏，默认配置关闭 `EnableAutoUpdates`。不删除 `AutoUpdateThread`、更新对话框类或相关 CMake/翻译资源，方便未来接入 OBSRedux 自己的发布渠道。
24. **重新发行包验收策略**：修复完成后必须重新 build/install 生成 `dist-test/`，再覆盖部署到 `D:\OBSRedux\` 做完整 B6 手工验收。源码检查不足以证明通过；必须真实启动确认中文系统语言自动生效、无上游更新弹窗、无自动配置向导、标题为 `OBSRedux ...` 且不含 `Studio`/`Portable Mode`、Settings 中激活 PotPvP Advanced/360fps/CQP/`<Install Root>/recordings`，以及第二次启动不再弹首启确认。
25. **Bootstrap 迁移版本策略**：新增 `[OBSRedux] BootstrapVersion=<current>` 作为自举迁移门闩。即使旧坏目录已经写入 `FirstRunCompleted=true`，只要 `BootstrapVersion` 低于当前值，也允许执行一次迁移修复，用于补齐本次已知缺陷（PotPvP 启动索引、上游首启门闩、禁用上游更新、录像路径等）。迁移完成后写入当前版本；之后不再每次启动覆盖用户配置。
26. **开发期迁移覆盖策略**：当前 OBSRedux 尚未发布，只有测试员/开发验收数据；本次 `BootstrapVersion` 迁移允许覆盖旧坏目录里的 Profile / Scene 选择，强制恢复到 PotPvP 初始状态。发布后的后续迁移再按用户数据保护标准收紧，不默认覆盖用户选择。
27. **B6 双路径验收策略**：修复后必须同时验收覆盖旧坏目录迁移与全新干净安装。先覆盖当前 `D:\OBSRedux\` 验证 `BootstrapVersion` 能修复旧状态；再用干净目录验证发行包开箱首启路径正确。
28. **输出目录策略**：仅本次 S8 迁移验收覆盖 `D:\OBSRedux\`。后续正式输出/完成品目录统一使用 `K:\Projects\finished\OBS-Redux\`，避免继续把开发期测试目录当作发布输出位置。
29. **本次干净安装验收目录**：本次 B6 干净安装验收使用 `K:\Projects\finished\OBS-Redux\`，同时验证该完成品路径下的 Portable Path Resolver、`recordings/` 输出路径与写权限逻辑。`D:\OBSRedux\` 仅用于旧坏目录覆盖迁移测试。

### Follow-up TODOs

- 发布打包阶段评估是否将启动入口从 `bin/64bit/obs64.exe` 改为 `obsredux64.exe`，或提供外层 OBSRedux 启动器，同时同步安装布局、快捷方式与验收命令。
- 后续发布/交付流程把完成品输出到 `K:\Projects\finished\OBS-Redux\`。

## User Stories

1. As a **OBSRedux 维护者**, I want to know确切的依赖版本+下载 URL+安装路径, so that 重装系统时可以 30 分钟内重建编译环境
2. As a **OBSRedux 维护者**, I want CMake configure 命令在文档中一次性给全, so that 不用反复试错 `-DQTDIR` / `-DDepsPath` / `-DCMAKE_POLICY_VERSION_MINIMUM` / `-DBUILD_BROWSER=OFF` 等参数组合
3. As a **OBSRedux 维护者**, I want 知道 VS2026 链接 deps2019 是否会撞 ABI, so that 不需要为了"安全"而强装一个 VS2019 Build Tools
4. As a **OBSRedux 维护者**, I want 看到编译过程中所有 `obsredux-bootstrap.cpp` 报错的根因+修复, so that 未来扩展 M1-M7 时不会重复踩 include / 签名 / 头文件声明问题
5. As a **OBSRedux 用户**, I want 双击 `obs64.exe` 后**不**被问"要不要从 AppData 迁移配置", so that 我的 AppData 永远干净，跟 OBS 同盘运行 (AC1)
6. As a **OBSRedux 用户**, I want 首次启动时看到一个明确说明"配置文件全部在 D:\OBSRedux\"的欢迎弹窗, so that 我知道录像、Profile、Scene 都不在 C 盘 (AC5)
7. As a **OBSRedux 用户**, I want 这个欢迎弹窗在我使用软件编码（无硬件 NVENC/QSV）时额外警告"性能会差", so that 我可以提前更换硬件 (AC5 sub-criterion)
8. As a **OBSRedux 用户**, I want 第二次启动时**不**再弹欢迎窗, so that 不被打扰 (AC6)
9. As a **OBSRedux 用户**, I want 我的 AppData\Roaming\obs-studio\ 在 OBSRedux 启动后 mtime 不变, so that 真便携而不是"伪便携偷偷写 AppData" (AC3)
10. As a **OBSRedux 用户**, I want 按 PageDown 录一段就能在 `D:\OBSRedux\recordings\` 看到 mp4 文件, so that 录像默认输出到便携目录 (AC8)
11. As a **OBSRedux 用户**, I want OBSRedux 启动时自动清掉 `D:\OBSRedux\logs\` 里超过 30 天的旧日志, so that 长期使用不会无限增长 (AC9)
12. As a **OBSRedux 用户**, I want `D:\OBSRedux\obs-studio\` 下出现完整目录结构 (`global.ini`/`basic/profiles/PotPvP/`/`basic/scenes/PotPvP.json`/`recordings/`), so that 我能用文件管理器直接备份所有配置 (AC2)
13. As a **OBSRedux QA**, I want PRD 明确说明 AC4/AC7/AC10 在 S8 不验证 + 各自的现实约束, so that 不会被 reviewer 误认为"漏测"
14. As a **未来 trellis-implement agent**, I want 看到本次构建中 sub-agent 幻觉问题（S5 dispatch 报告完成但实际 0 行写入）已被记录到 `.trellis/spec/guides/trellis-workflow-gotchas.md`, so that 不会重复同样的失败模式

## Implementation Decisions

### Build Recipe（B1-B5 固化）

**1. Toolchain 版本锁定**

| 组件 | 选定版本 | 决策依据 |
|------|---------|---------|
| Visual Studio | 2026 Community (v17.50.x MSVC) | 用户已装；实测对 deps2019 (VS2019 编译的 C 库) ABI 兼容；FFmpeg/x264/curl 都是 C 接口，MSVC v14.x 运行时向后兼容 |
| Qt | 5.15.2 MSVC 2019 64-bit | OBS 27.x 强依赖 Qt5；用 Qt 在线安装器 + 勾选 **Archive** 才能找到 5.15.2（开源版无离线包） |
| obs-deps | dependencies2019.zip | OBS 官方预编译包，包含 FFmpeg/x264/curl/mbedTLS/Vulkan SDK 等 |
| CMake | 4.3.2 | 用户已装；新到砍掉 `cmake_minimum_required<3.5`，需补 policy 兼容参数 |

**关键路径约定**：
- Qt: `C:\Qt\5.15.2\msvc2019_64`
- deps2019 解压目标: `K:\Projects\dev\OBS-Redux\deps2019\win64`（项目内但不在 build 树）
- 构建目录: `K:\Projects\dev\OBS-Redux\build64`
- 发行包: `K:\Projects\dev\OBS-Redux\dist-test`
- 本次迁移验收目标: `D:\OBSRedux`
- 本次干净安装/后续完成品目标: `K:\Projects\finished\OBS-Redux`

**2. CMake configure 参数（最终通过版）**

```
cmake -S . -B build64 -G "Visual Studio 18 2026" -A x64 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DQTDIR=C:/Qt/5.15.2/msvc2019_64 \
  -DDepsPath=K:/Projects/dev/OBS-Redux/deps2019/win64 \
  -DBUILD_BROWSER=OFF \
  -DBUILD_VST=OFF
```

四个非默认 flag 的理由：
- `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` — 兜底 `deps/obs-scripting/CMakeLists.txt` 等老 cmake_minimum_required
- `-DBUILD_BROWSER=OFF` — `plugins/obs-browser` 子模块未拉取（PotPvP 录制不需要浏览器源）
- `-DBUILD_VST=OFF` — 减少不必要 audio plugin 编译
- 其余 plugin 缺子模块（`libdshowcapture` / `enc-amf` / VLC）会自动跳过，不报错

**3. obsredux-bootstrap.cpp 编译期修复（6 处）**

| # | 错误 | 根因 | 修复 |
|---|------|------|------|
| 1 | `config_open / config_set_bool / config_save / config_close` 未定义 | 缺 include | 加 `#include <util/config-file.h>` |
| 2 | `QTStr` 未定义 | 缺 obs-app 头 | 加 `#include "obs-app.hpp"` |
| 3 | `jansson.h` 找不到 | UI 层不应直接依赖 libobs 内嵌的 jansson | 改用 OBS 公共 API `obs_data_create_from_json_file` / `obs_data_set_string` / `obs_data_save_json` |
| 4 | `config_open` 接受 2 参数（实际签名 `int config_open(config_t**, file, type)`） | 误用 jansson 风格 | 3 处 call site 改成 `config_t *cfg = nullptr; if (config_open(&cfg, path, ...) != CONFIG_SUCCESS) ...` |
| 5 | `run_first_run_bootstrap_if_needed` 在 window-basic-main.cpp 找不到符号 | 头文件漏声明 | obsredux-bootstrap.h 的 C++ 段加 `void run_first_run_bootstrap_if_needed(const char *active_profile_name);` |
| 6 | （已在 trellis-check 修过）M7 对话框硬编码 `\\recordings` | 静态字符串拼接 | 改用 `get_portable_path(buf, sizeof(buf), "recordings")` 然后 `QString::fromUtf8(buf)` |

**4. Install 路径布局**

`cmake --install build64 --prefix dist-test --config RelWithDebInfo` 产出：
- `dist-test/bin/64bit/obs64.exe` (3 MB)
- `dist-test/bin/64bit/*.dll` (Qt5 + FFmpeg + mbedTLS + ...)
- `dist-test/data/{obs-plugins,libobs}/` 插件资源
- `dist-test/obs-studio/` ← UI/CMakeLists.txt 加的 `install(DIRECTORY data/obsredux-defaults/ DESTINATION ${OBS_DATA_DESTINATION}/../ ...)` 直接落根
- `dist-test/recordings/` ← 用户录像输出根目录，占位确保首次启动无需手工建目录
- 总大小 ≈ 182 MB

**5. 便携部署模式**

约束：发行包根目录的兄弟节点 = `bin/`、`obs-studio/`、`recordings/`。OBSRedux 用 `GetModuleFileNameW` + 上溯 3 级目录定位 Install Root，因此把 `dist-test/` 整体 copy 到目标 Install Root 后即可双击 `bin/64bit/obs64.exe` 启动。本次 S8 同时验证 `D:\OBSRedux\` 覆盖迁移与 `K:\Projects\finished\OBS-Redux\` 干净安装。

### AC Verification Harness（B6 矩阵）

**单机可验证项**：

| AC | 操作 | 预期 | 验证手段 |
|---|---|---|---|
| AC1 | 双击 `bin/64bit/obs64.exe` | 不弹 AppData 迁移；不弹上游更新；不弹 Auto-Configuration Wizard；进入 OBSRedux 首启确认或主界面 | 视觉 + 用户报告 |
| AC2 | 启动后查 Install Root | `obs-studio/global.ini` + `basic/profiles/PotPvP/*` + `basic/scenes/PotPvP.json` 存在；根目录 `recordings/` 存在 | `Get-ChildItem` |
| AC3 | 启动前后对比 `%APPDATA%\obs-studio\global.ini` mtime | mtime 不变；不创建/修改 AppData OBS 配置 | PowerShell `Get-Item` 前后 diff |
| AC5 | 首次启动或删掉 `[OBSRedux] FirstRunCompleted` 后启动 | 弹 OBSRedux 三项确认；输出位置为 `<Install Root>\recordings`；CQP 25 / 360 fps 与预置一致；`X` 等同开始使用 | 视觉 + 配置文件核验 |
| AC6 | 立即第二次启动 | 不再弹三项确认；标题为 `OBSRedux <version> - Profile: PotPvP - Scenes: PotPvP`，不含 `Studio`/`Portable Mode` | 视觉 |
| AC8 | 按 PageDown 录 5 秒停 | 根目录 `recordings/` 出现 mp4 | `Get-ChildItem <Install Root>\recordings\*.mp4` |
| AC9 | 手工 `SetFileTime` 给 logs/ 内两个文件分别打 35d / 29d 前时间戳 → 重启 | 35d 文件被删，29d 保留 | `Test-Path` 验证 |
| AC11 | 覆盖旧坏 `D:\OBSRedux\` 启动 | `BootstrapVersion` 迁移修复 PotPvP 启动索引、禁用上游更新、录像路径；允许本次开发期覆盖旧选择 | 视觉 + `global.ini` / Profile 核验 |
| AC12 | 干净部署到 `K:\Projects\finished\OBS-Redux\` 启动 | 全新安装路径下首启行为正确，`recordings/` 指向该 Install Root | 视觉 + 文件核验 |

**跳过项**：

| AC | 跳过原因 | 后续验证途径 |
|---|---|---|
| AC4 | Program Files 写入拦截 → 需 admin + UAC 确认 | 留给独立 release-time 验收 |
| AC7 | 三种硬件编码器探测 → 需 N/A/Intel 三卡机器 | 留给多机器矩阵测试 |
| AC10 | ctest 全 PASS → S7 单元测试未实现 | 已在 Step 1 完成轮档案，留给 Step 2 |

## Testing Decisions

### 什么是好测试（本任务语境）

- **外部行为可见**：所有 AC 都从"用户视角"判断——文件出现/未出现、弹窗出现/未出现、mtime 变/不变。不验证 M1-M7 内部代码路径。
- **端到端 = 真启动**：M4 首启弹窗、M3 编码器探测都涉及 OBS 完整启动链（obs_load_all_modules → obs_post_load_modules → run_first_run_bootstrap_if_needed），mock 不出来——必须真启动。
- **可重放**：所有 AC 的 setup / 操作 / 断言都用 PowerShell 或文件管理器一键可重做。

### 测试模块

- **Build Recipe** ← 验证由 B3/B4/B5 三步通过即视为 PASS（已完成）
- **Portable Distribution Layout** ← 验证由 B5 install 后 `dist-test/obs-studio/` 与根目录 `dist-test/recordings/` 输出匹配（需重跑）
- **AC Harness** ← 验证由 B6 覆盖迁移 + 干净安装双路径手工矩阵执行（进行中）

### Prior Art

- `.trellis/spec/obsredux-bootstrap.md` — 已记录 M1 passthrough 语义、M3 enum vs caps 决策、M4 时序约束
- `.trellis/spec/guides/trellis-workflow-gotchas.md` — 已记录 S5 sub-agent 幻觉
- 上游 OBS Studio 27.x 本身没有便携模式的端到端验收测试，本任务相当于自建

### 暂不写自动化测试的模块

- **M1 路径解析** — 测试要 mock `GetModuleFileNameW`，cmocka 改造成本高于回报；目前依赖 AC2 间接验证
- **M3 编码器探测** — 同上，要 mock `obs_enum_encoder_types`；AC5 弹窗内容能间接看到 fallback 路径
- 整体决定推迟到 Step 2/S7 阶段引入 cmocka 框架后统一补

## Out of Scope

- ❌ **AC4**（Program Files / Windows 目录拦截）— 需 admin 权限 + UAC 交互，单机批量验证不现实，留给独立 release-time 验收
- ❌ **AC7**（NVENC / QSV / AMF 三种硬件编码器探测）— 需三种 GPU 的物理机器，单台开发机只能见到一卡
- ❌ **AC10**（`ctest` 全 PASS）— S7 单元测试本身未实现，Step 1 验收时已书面同意推迟
- ❌ **APPDATA 迁移功能** — Step 1 的产品决策是"不迁移，AppData 不动"；不在 OBSRedux 范围
- ❌ **Linux/macOS 构建** — Step 1 PRD 限定 Windows-only
- ❌ **VS2019 Build Tools 兜底路径** — 实测 VS2026 工作正常，省去这个备选
- ❌ **obs-browser / VST / dshowcapture / enc-amf 插件** — PotPvP 录制不需要，子模块未拉取即默认 OFF

## Further Notes

### VS2026 + deps2019 ABI 兼容性实证

PRD 起草时担心 "VS2026 与 VS2019 编译的 deps2019 可能 C++ ABI 冲突"，因此把"降级到 VS2019 Build Tools"列为风险缓解。实测结果：

- B4 编译全过、零 link error
- obs64.exe + 全部 plugin DLL 正常产出
- 原因：deps2019 内核心库 (FFmpeg / x264 / curl / mbedTLS) 均是 C 接口，MSVC 运行时（msvcr140 系列）从 VS2015 起对 C ABI 保证向后兼容；少量 C++ 库（如 Vulkan SDK 头文件）也是 header-only

**结论**：VS2026 单一 toolchain 即可，不再需要 VS2019 Build Tools 兜底。本节固化此发现，避免后续被 PRD 旧文字误导。

### 历史 build 失败记录

| 时刻 | 错误 | 解决 |
|------|------|------|
| 14:34 第 1 次 cmake | "Compatibility with CMake < 3.5 has been removed" (deps/obs-scripting) | 加 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` |
| 14:50 第 2 次 cmake | "obs-browser submodule not found" | 加 `-DBUILD_BROWSER=OFF -DBUILD_VST=OFF` |
| 15:20 第 1 次 build | jansson.h 找不到 + config_open 签名错 + QTStr 找不到 + 函数未声明 | 6 处 include/签名/头文件修复（见 Implementation Decisions） |
| 15:48 第 2 次 build | `obs` target 单独建，install 时缺 glad/libobs/各插件 | 改 `cmake --build build64 --config RelWithDebInfo`（不指定 target = 建 ALL_BUILD） |
| 15:55 install | ✅ 通过 |

### 后续动作（B6 完成后）

- 把本次发现的 6 处 obsredux-bootstrap.cpp toolchain 修复点同步写入 `.trellis/spec/obsredux-bootstrap.md` 的 "Common Mistakes" 一节（trellis-update-spec）
- B6 全部 PASS 后归档本任务（trellis:finish-work）
- Step 1 整体收官报告

---

## References

- [OBS deps download](https://obsproject.com/downloads/dependencies2019.zip)
- [Qt Online Installer](https://download.qt.io/official_releases/online_installers/)
- Step 1 PRD: `.trellis/tasks/archive/2026-05/05-25-step1-portable-bootstrap/prd.md`
- Module spec: `.trellis/spec/obsredux-bootstrap.md`
- Workflow gotchas: `.trellis/spec/guides/trellis-workflow-gotchas.md`
