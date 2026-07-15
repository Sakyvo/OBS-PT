# Step 1: Portable Bootstrap & PotPvP Defaults

> **决策来源**：2026-05-25 一轮 14 问 grill-with-docs 会话。术语统一参见 [`CONTEXT.md`](../../../CONTEXT.md)；架构性决策见 [`docs/adr/0001-forced-portable-mode.md`](../../../docs/adr/0001-forced-portable-mode.md) 和 [`docs/adr/0002-positioning-obs-for-potpvp.md`](../../../docs/adr/0002-positioning-obs-for-potpvp.md)。

---

## Goal

把上游 OBS 改造成可解压即用的 PotPvP 录制定制版：

1. **永久强制便携化**——所有用户数据写在 `<Install Root>/obs-studio/`，零兼容 `%APPDATA%` fallback；
2. **开箱即录**——发行包解压后即拥有预置 PotPvP Profile / Scene Collection / Global Config，首次启动经过硬件编码器探测 + 三项确认弹窗后立刻可录；
3. **自维护**——日志和崩溃转储自动保留 30 天滚动清理。

---

## Problem Statement

上游 OBS Studio 在 PotPvP cinematic 工作流上有三类摩擦：

1. **配置散落系统盘**：默认 `%APPDATA%/obs-studio/` 不在用户的录像盘上，备份和迁移困难；上游"便携模式"是可选标记文件，容易在解压/CI 中漏放，无法作为硬保证。
2. **开箱不可用**：新装 OBS 后用户必须手动配 360fps Fractional FPS、NVENC + tune=ll + bf=0 等一整套针对 360fps × Frame Blending × Bilibili Distribution Ceiling 优化过的参数。新手不知道这些参数；老手也要重复劳动。
3. **磁盘自维护缺失**：长期使用累积大量 `logs/` 与 `crashes/`，OBS 上游无自动清理。

PotPvP 玩家期望解压一个目录、双击 exe、确认三项默认值，**30 秒内开始录制**，且**所有数据都在程序同盘**。

---

## Solution

把上游 OBS 的"可选便携"硬化成"恒定便携"，发行包内预置一份完整的 PotPvP Profile / Scene Collection / Global Config 与 `recordings/` 占位目录，启动期跑写权限自检 → 首次启动跑硬件编码器探测 + 首次启动确认弹窗 + 日志清理扫描。所有决策由 CONTEXT.md 中定义的术语驱动（**Install Root**、**User Data Root**、**Fractional FPS**、**Distribution Ceiling** 等）。

---

## User Stories

### 安装与启动

1. 作为 PotPvP 玩家，我想把 OBSRedux 解压到 D 盘的任意目录，双击就能启动，无需任何配置文件操作，**这样我不用记住"还要放个 portable_mode.txt"**。
2. 作为 PotPvP 玩家，我想看到所有用户数据都在 `<Install Root>/obs-studio/` 下而不是 `%APPDATA%`，**这样我可以整文件夹搬迁/备份/上传到云盘**。
3. 作为不小心装到 `C:\Program Files\` 的用户，我想看到清晰的错误弹窗告诉我"换个目录"，**这样我不会以为是软件坏了**。
4. 作为只读分区或被杀毒软件拦截的用户，我想从同一弹窗的列举提示里看到可能的原因，**这样我能自己排查**。

### 默认 Profile / Scene Collection

5. 作为首次启动的 PotPvP 玩家，我想看到一个名为 `PotPvP` 的 Profile 已经存在并被激活，**这样我不用从空白工程开始**。
6. 作为首次启动的 PotPvP 玩家，我想看到 Canvas 已是 360/1 Fractional FPS、CQP 25、NVENC H.264 + `tune=ll` + `bf=0` 等针对 Frame Blending 优化的预设，**这样我不用查教程**。
7. 作为持 AMD 卡的玩家，我想看到默认编码器自动切到 AMF H.264 而不是报"NVENC 不可用"，**这样我和 N 卡用户体验一致**。
8. 作为 Intel iGPU 用户，我想看到 QSV H.264 自动选中作为兜底，**这样集显跑 MC 1.7.10 也能录**。
9. 作为没有任何硬件编码器的用户，我想看到 x264 软编码兜底 + 警示弹窗 "性能可能受限"，**这样我知道下一步要升级硬件**。
10. 作为 PotPvP 玩家，我想看到 Game Capture 来源已预配为窗口标题匹配（`Minecraft` / `Lunar Client` / `Badlion Client`）模式，**这样我打开客户端就被抓到，不用手动新建源**。

### 首次启动引导

11. 作为首次启动的玩家，我想看到一个简洁的确认弹窗列出：输出位置 `<Install Root>/recordings/`、画质档位 CQP 25、录制帧率 360 fps，**这样我知道默认值是什么，需要时可以去对应设置改**。
12. 作为不喜欢被打扰的用户，我想这个弹窗只出现一次，**这样我不会每次启动都点确认**。
13. 作为切换了 Profile 的用户，我想这个弹窗不会因为 Profile 数据变了再次弹出，**这样我可以自由实验默认值**。

### 录制输出

14. 作为录 360fps × CQP 25 的玩家，我想看到录像直接写到 `<Install Root>/recordings/`（与程序同盘），**这样我不用先建文件夹也不会写满 C 盘**。
15. 作为整盘搬迁 OBSRedux 的玩家，我想录像路径自动跟随新位置，**这样我不需要在新机器上重新配 RecFilePath**。

### 自维护

16. 作为长期使用 OBSRedux 的玩家，我想日志和崩溃转储自动只保留 30 天，**这样不会无限累积占盘**。
17. 作为遇到崩溃的玩家，我想最近的崩溃 dump 一定还在，**这样可以发给开发者复现**。

---

## Implementation Decisions

### M1 — Portable Path Resolver

替换上游所有"`<config>/obs-studio/...`" 拼接为"`<Install Root>/obs-studio/...`"。

- 不再做 `portable_mode ? CONFIG_PATH : os_get_config_path()` 分支判断；统一返回相对 exe 的 `<Install Root>/obs-studio/<name>`。
- 砍掉 `BASE_PATH "/config"` 这一层中间目录（沿用 CONTEXT.md 决策：保留 `obs-studio/`，去掉 `config/`）。
- 接口形态：`int get_portable_path(char *dst, size_t size, const char *name)`，签名向后兼容上游 `os_get_config_path()` 调用点，最大化复用上游 20+ 处调用而无需逐处改写。
- 所有上游对 `os_get_config_path` / `os_get_config_path_ptr` / `GetConfigPath` 的调用走同一新路径。
- 上游 `OBS_UNIX_STRUCTURE` 分支删除（OBSRedux 仅 Windows 目标）。

### M2 — Install Root Write-Probe

启动入口最早期（在任何配置文件读写之前）做一次写权限自检。

- 接口形态：枚举 `FailureReason { Ok, SystemProtected, ReadOnlyVolume, AntiVirusBlocked, Unknown }`。
- 实现：尝试在 `<Install Root>/.write_probe_<uuid>` 创建+删除一个临时文件；按 errno / GetLastError 分流原因。
- 失败时调用 M6 弹窗后 `exit(1)`，不进入主流程。

### M3 — Encoder Capability Probe

枚举可用编码器，按固定优先级挑选第一个。

- 优先级硬编码：`["jim_nvenc", "obs_qsv11", "ffmpeg_nvenc", "amd_amf_h264", "obs_x264"]`（取决于上游具体 ID，实现时校对）。
- 仅对 H.264 编码器探测；HEVC 完全跳过（ADR-0002 决策）。
- 接口形态：返回选中的 encoder ID 字符串；找不到则返回 `obs_x264` 并设置标志位让 M7 显示软编码警示。

### M4 — First-Run Bootstrap

判断"是否首次启动"的依据：Global Config 中是否存在 `[OBSRedux] FirstRunCompleted=true` 键。

- 首次启动序列：
  1. 调 M3 取选中的 encoder ID
  2. 读发行包预置的 `<User Data Root>/basic/profiles/PotPvP/recordEncoder.json`，把 encoder 字段改写为 M3 选中的 ID（若与默认的 `jim_nvenc` 不同）
  3. 读 `<User Data Root>/basic/profiles/PotPvP/basic.ini`，把 `[AdvOut] RecFilePath` 与 `[SimpleOutput] FilePath` 改写为运行时计算的绝对路径 `<Install Root>/recordings/`
  4. 调 M7 显示三项确认弹窗
  5. 写入 `[OBSRedux] FirstRunCompleted=true`
- 探测结果一次性固化（CONTEXT.md 决策："首次启动一次性探测后固化，不再重测"）。

### M5 — Log Retention Sweeper

启动期单次扫描，清理超龄文件。

- 扫描 `<User Data Root>/logs/*.txt` 与 `<User Data Root>/crashes/*.log`（按上游实际文件名格式校对）。
- 删除条件：`mtime < now - 30 days`。
- 接口形态：`void sweep_retention(const char *dir, int retention_days, const char *glob)`，可注入 mock clock 与 mock fs 单测。
- 触发时机：在 M1 解析路径之后、UI 起来之前，与 M2 同阶段。
- 仅 Win32 实现（参考 `libobs/util/platform-windows.c` 的 `os_glob` / `os_unlink`）。

### M6 — Write-Permission Failure Dialog

A2 文案 Qt 弹窗。

- 标题：`OBSRedux 无法启动`
- 正文（多行，列举式）：
  > OBSRedux 需要在安装目录读写用户配置与录像。当前目录无写入权限，可能是因为：
  > - 安装在 `C:\Program Files\` 或其他系统保护位置
  > - 所在分区为只读
  > - 杀毒软件拦截
  >
  > 建议解决方案：将 OBSRedux 文件夹移动到桌面、`D:\` 或 `E:\` 等普通目录后重试。
- 按钮：`打开当前安装位置`（调 `ShellExecuteW` 打 explorer）、`退出`（默认）。
- 五语言文案（en/zh-CN/zh-TW/ja/ko）走上游 i18n 框架。

### M7 — First-Run Recommendations Dialog

精简三项确认弹窗。

- 标题：`OBSRedux 已为 PotPvP 录制预置默认值`
- 正文：
  > 请先确认部分选项是否符合你的设备：
  > 1. 输出位置：`<Install Root>/recordings/`
  > 2. 画质档位：CQP 25
  > 3. 录制帧率：360 fps
- 按钮：`开始使用`（默认）。
- 无"不再提示"勾选框——靠 Global Config 的 `FirstRunCompleted` 标志位天然只弹一次。

### CMake / 发行包改动

不算运行时模块但属于 Step 1 范围：

- 修改 `cmake/Modules/ObsHelpers.cmake` 或 install 流程，把以下文件作为 install 资源复制到发行包：
  - `<Install Root>/obs-studio/basic/profiles/PotPvP/basic.ini`（移植 obscfg/basic.ini，FPS=360 + CQP 25 + 占位 `RecFilePath`）
  - `<Install Root>/obs-studio/basic/profiles/PotPvP/recordEncoder.json`（移植 obscfg/recordEncoder.json，cqp=25）
  - `<Install Root>/obs-studio/basic/profiles/PotPvP/streamEncoder.json`（移植 obscfg/streamEncoder.json，空对象）
  - `<Install Root>/obs-studio/basic/scenes/PotPvP.json`（新建：单 Scene + 一个 Game Capture 来源 + Desktop Audio + Mic）
  - `<Install Root>/obs-studio/global.ini`（新建：language、SceneCollection=PotPvP、Profile=PotPvP）
  - `<Install Root>/obs-studio/recordings/README-recordings.txt`（占位文件，确保发行包带这个目录）

### 影响范围（涉及上游文件）

- `UI/obs-app.cpp`：`portable_mode` 全局变量硬编码；删除 CLI 解析与 marker 检测；插入 M2 调用
- `UI/obs-app.cpp` 中 `GetConfigPath` / `GetConfigPathPtr`：M1 替换
- `UI/window-basic-main.cpp` 中 `GetProfilePath`：路径前缀改 `obs-studio/basic/profiles`（已是当前形态，但要确认 `config/` 那层是否还有残留）
- `UI/window-basic-main-profiles.cpp`、`window-basic-main-scene-collections.cpp`：所有 `"obs-studio/..."` 字符串前缀走 M1
- `libobs/util/platform-windows.c`：`os_get_config_path` 内部转调 M1（让插件 SDK 也透明走 portable 路径）
- 新增（待定具体位置）：`UI/obsredux-bootstrap.{h,cpp}` 承载 M2/M3/M4/M5/M6/M7

---

## Testing Decisions

### 测试哲学

只测外部行为，不测实现细节。给输入 → 检查输出，不检查中间状态。Deep modules（M1/M2/M3/M5）的接口稳定，正是单测最划算的地方。

### 推荐测试矩阵

| 模块 | 测试类型 | 重点用例 | 优先级 |
|---|---|---|---|
| **M1 Path Resolver** | 单元 | `name="global.ini"` → `<Install Root>/obs-studio/global.ini`；`name=""` → `<Install Root>/obs-studio`；空 name 与 NULL name；超长 name 触发 snprintf 截断 | 必做 |
| **M2 Write Probe** | 单元 | 注入 mock fs：返回 EACCES → SystemProtected；返回 EROFS → ReadOnlyVolume；返回成功 → Ok | 必做 |
| **M3 Encoder Probe** | 单元 | 注入 mock `obs_get_encoder_caps`：仅 jim_nvenc 可用 → 选 jim_nvenc；NVENC+AMF 都可用 → 选 NVENC；全不可用 → 选 obs_x264 + 软编码标志位 | 必做 |
| **M4 Bootstrap** | 集成 | 构造空 User Data Root → 触发 M2+M3+文件改写 → 验证 `recordEncoder.json` 的 encoder 字段被改写、`basic.ini` 的 `RecFilePath` 被改写为绝对路径、`FirstRunCompleted=true` 被写入 | 必做 |
| **M5 Log Sweeper** | 单元 | 注入 mock fs + mock clock：3 个文件 mtime 分别为 35/29/10 天前 → 仅删除 35 天前的；空目录 → 不报错；目录不存在 → 不报错 | 必做 |
| **M6 Write-Fail Dialog** | — | Qt UI 测试代价高，跳过；通过 M2 集成测试间接覆盖 | 跳过 |
| **M7 First-Run Dialog** | — | 同上 | 跳过 |

### 先验 / Prior Art

- 上游 `test/ostests/` 目录用 GTest 框架；可作为 M1/M2/M3/M5 单元测试的 prior art
- 上游 `libobs/util/platform-windows.c` 中 `os_get_config_path` 的 win32 实现可作为 M1 的对照参考
- 上游 `UI/window-basic-main.cpp::InitBasicConfigDefaults*()` 中"首次启动"的判定语义可作为 M4 的对照（M4 是 OBSRedux 自己的更早期且更全面的首次启动逻辑，但 OBS 自身的判定保留作为 fallback）

---

## Out of Scope

以下项明确不在 Step 1 范围，列在这里防止 scope creep：

- **INI → YAML 配置改造**（CLAUDE.md 原 Step 2，已撤回，沿用上游 INI）
- **左侧栏"场景"快捷切换**（CLAUDE.md 原 Step 3，已搁置）
- **Fragmented MP4 容器**（决策为 future roadmap，仅默认传统 MP4）
- **多轨音频分轨录制**（决策为 future roadmap，仅默认单轨混音）
- **HEVC 编码器支持**（决策为 future roadmap，仅 H.264 优先级）
- **FPS 游戏玩家用户群扩展**（PotPvP 战略收缩期内不开发）
- **首次启动期硬件能力的自动降档/分辨率自适应**（CONTEXT.md 已明确"默认是推荐而非锁定"，用户改即可）
- **APPDATA 旧数据迁移工具**（用户群不重叠，决策为不做）
- **i18n 五语言文案的实际翻译**（Step 1 仅占位英文 + 一份中文，其余语言后续 PR）
- **Replay Buffer 默认打开**（沿用 obscfg：`RecRB=false` 但热键 `W` 已绑，留口子等用户自己开）
- **CrashHandler / 自动错误上报**（上游已有，Redux 不改）

---

## Further Notes

### 关键术语速查

| 术语 | 物理形态 |
|---|---|
| Install Root | `<解压目录>/` |
| User Data Root | `<Install Root>/obs-studio/` |
| Global Config | `<User Data Root>/global.ini` |
| Profile（默认 `PotPvP`） | `<User Data Root>/basic/profiles/PotPvP/{basic.ini, recordEncoder.json, streamEncoder.json, service.json}` |
| Scene Collection（默认 `PotPvP`） | `<User Data Root>/basic/scenes/PotPvP.json` |
| Logs | `<User Data Root>/logs/` |
| Crashes | `<User Data Root>/crashes/` |
| Plugin Config | `<User Data Root>/plugin_config/<plugin>/` |
| Recordings | `<Install Root>/recordings/` |

### 预置配置关键字段（移植自 `C:/Users/ASUS/Desktop/obscfg`，含 360+25 覆写）

**`basic.ini` 关键段**：
- `[Video] FPSType=2 / FPSNum=360 / FPSDen=1`（**360 覆写**）
- `[Video] BaseCX/CY = OutputCX/CY = <显示器分辨率>`（运行时探测）
- `[Video] ScaleType=bilinear / ColorFormat=NV12 / ColorSpace=709 / ColorRange=Partial`
- `[AdvOut] RecEncoder=<M3 选中>`
- `[AdvOut] RecFormat=mp4 / RecTracks=1 / AutoRemux=false`
- `[AdvOut] RecFilePath=<Install Root>/recordings`（首次启动改写）
- `[Output] Mode=Advanced`
- `[Audio] SampleRate=48000 / ChannelSetup=Stereo`
- `[Hotkeys] OBSBasic.StartRecording=PageDown / OBSBasic.UnpauseRecording=PageDown / OBSBasic.StartReplayBuffer=W`

**`recordEncoder.json` 字段（CQP 25 覆写）**：
```json
{"bf":0,"bitrate":30000,"cqp":25,"multipass":"disabled","preset":"hp","preset2":"p1","profile":"high","psycho_aq":false,"rate_control":"CQP","tune":"ll"}
```

### 跨任务依赖

- 本任务无上游依赖（是项目首个 feature 任务）。
- 后续 Step 5/6 编码器档位精调（NVENC p1-p7 微调、HEVC 支持）建立在本任务定下的"硬件探测 + 一次性固化"机制之上。
- 后续 fMP4 / 多轨支持任务会复用本任务的预置 Profile 文件结构。

---

## Acceptance Criteria

- [ ] AC1：解压发行包到 `D:\OBSRedux\` 双击启动，无任何额外文件操作直接进入主界面
- [ ] AC2：`<Install Root>/obs-studio/` 下出现完整目录结构（global.ini + basic/profiles/PotPvP/ + basic/scenes/PotPvP.json + logs/ + crashes/）
- [ ] AC3：启动后 `%APPDATA%/obs-studio/` 未被创建（验证零 APPDATA 写入）
- [ ] AC4：解压到 `C:\Program Files\` 启动时弹出 A2 文案弹窗后退出
- [ ] AC5：首次启动看到三项确认弹窗（输出位置 / CQP 25 / 360 fps），点击后正常进入
- [ ] AC6：第二次启动不再弹三项确认
- [ ] AC7：用 NVIDIA 卡 / AMD 卡 / Intel iGPU 三种环境分别启动，`recordEncoder.json` 的 encoder 字段对应被改写为 jim_nvenc / amd_amf_h264 / obs_qsv11
- [ ] AC8：录像默认落到 `<Install Root>/recordings/`，文件名沿用上游 `%CCYY-%MM-%DD %hh-%mm-%ss.mp4` 模板
- [ ] AC9：手工放 35 天前的旧 `logs/2026-04-20.txt`，启动后被删；29 天前的保留
- [ ] AC10：M1/M2/M3/M5 单元测试 + M4 集成测试全部 PASS
