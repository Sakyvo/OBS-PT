# OBS-PT

OBS-PT 是 OBS Studio 的独立分支（fork），对外定位为 **"OBS For PotPvP"** ——专为 Minecraft Java 版 1.7.10 / 1.8.9 的 PotPvP 录制玩家提供的定制版，永久强制便携化运行，默认配置面向高帧率素材采集 + 后期帧混合工作流调优。与上游主线不存在合并/升级关系。

界面语言支持 en-US / zh-CN / zh-TW / ja-JP / ko-KR——这是**宣传渠道触达 + 翻译工作量**的实施约束，不是用户群定位（项目不强调"东亚定制"概念）。

## Language

**OBS-PT**:
本项目；独立 GitHub 仓库，参考 syncthing-fork 模式与上游 OBS Studio 平行演进，不参与 Crowdin 翻译协作。对外宣传口径为 "OBS For PotPvP"。
_Avoid_: OBS、OBS Studio（这两个词专指上游主线）、东亚精简版（早期讨论用语，已废弃）

**PotPvP**:
Minecraft Java 版的一种 PvP 子玩法（Potion PvP），使用末影珍珠瞬移 + 治疗药水近战，主要流行于 1.7.10 / 1.8.9 版本及其衍生客户端（Lunar Client、Badlion Client、PvPLounge 等）。这是 OBS-PT 的**目标用户场景**——所有默认值、UI 文案与发行策略都围绕这一场景收敛。
_Avoid_: PvP（PvP 是泛指，PotPvP 是具体玩法分支）

**Install Root**:
OBS-PT 解压/安装后所在的物理目录，包含 `bin/`、`data/`、`obs-plugins/`、`obs-studio/`。所有用户数据与程序自身**必须同盘**。
_Avoid_: 程序目录、exe 目录（在上游语境里这两个词指 `bin/64bit/`，含义更窄）

**User Data Root**:
所有用户可修改数据的根目录，物理路径恒为 `<Install Root>/obs-studio/`。承载 Global Config、所有 Profile、所有 Scene Collection、日志、崩溃转储与插件配置。**永远不会**落到 `%APPDATA%`。
_Avoid_: Profile 文件夹（Profile 是另一个已存在的概念）、config 目录（这是被砍掉的中间层）、便携目录

**Profile**:
一组录制/推流参数（分辨率、码率、编码器预设、输出路径等），物理形态为 `<User Data Root>/basic/profiles/<Name>/` 下的若干文件（`basic.ini`、`service.json`、`streamEncoder.json`、`recordEncoder.json`）。一个 OBS-PT 实例可拥有多个 Profile，运行时仅激活其中一个。
_Avoid_: 配置文件、用户数据根

**Scene Collection**:
一组场景的画面排版与来源定义，物理形态为 `<User Data Root>/basic/scenes/<Name>.json`。与 Profile 正交——同一 Profile 可切换多个 Scene Collection，反之亦然。
_Avoid_: 场景（单数 Scene 是 Scene Collection 内部的元素）、画面配置

**Global Config**:
跨 Profile 与 Scene Collection 共享的全局设置（语言、主题、UI 偏好、当前激活的 Profile 名与 Scene Collection 名等）。物理形态为 `<User Data Root>/global.ini`，沿用上游 INI 格式。
_Avoid_: 全局配置（中文里易与 Profile 混淆）

**Basic Config**:
单个 Profile 内的核心配置文件，物理形态为 `<User Data Root>/basic/profiles/<Name>/basic.ini`，沿用上游 INI 格式。
_Avoid_: profile 配置（易与整个 Profile 包混淆）

**Portable Mode（OBS-PT 语义）**:
在 OBS-PT 中，便携模式**恒为开启状态**，不可关闭，无 CLI 开关、无标记文件。这与上游 OBS 的"可选便携模式"语义不同。
_Avoid_: --portable、portable_mode.txt（这些是上游可选机制的术语，在 OBS-PT 中均不适用）

**Fractional FPS / 分数帧率**:
OBS-PT 默认且推荐的画布帧率模式，对应上游 `[Video] FPSType=2`，由 `FPSNum` / `FPSDen` 两个整数表示（默认 `480/1`）。OBS-PT 不使用上游的 `FPSCommon` 预设档（30/60/120…）作为默认形态，因为目标工作流的帧率落点（360、480 等）不在预设档内。
_Avoid_: FPSCommon、整数帧率（这些指代上游另一种 FPSType）

**Frame Blending / 帧混合**:
**消费侧后期工作流概念**，指在 Premiere / After Effects / DaVinci Resolve 等剪辑软件中将 N 个高帧率源帧合成 1 个目标帧，以获得电影级动态模糊（如 480fps → 60fps，每 8 帧合 1 帧）。OBS-PT **自身不实现**帧混合，但默认值（高 Canvas FPS + 匹配 Distribution Ceiling 的录制码率）是为满足此工作流的输入要求而设计。
_Avoid_: 动态模糊（"动态模糊"是渲染效果名，"帧混合"是实现手段）

**Distribution Ceiling / 分发平台码率天花板**:
最终视频上传到目标平台（默认假设为 Bilibili，上限约 20 Mbps）后，平台会对视频做二次压缩。任何超出此上限的源端码率在终端观众侧都会被抹平，仅消耗本地盘空间而无视觉收益。**这是 OBS-PT 默认 CQP / CRF 值的上游约束**——录制端质量只需让"帧混合后再被压到 20Mbps"的最终成片视觉可接受即可，不追求源端绝对无损。
_Avoid_: 上传码率、平台压缩（这些是过程描述，"天花板"强调它对源端决策的反向约束作用）

**High-FPS Source / 高帧率来源**:
OBS-PT 默认假设的来源类型——Minecraft Java 版 1.7.10 / 1.8.9 客户端（含 Vanilla、Lunar Client、Badlion Client、PvPLounge 等 PotPvP 圈常见客户端），集显即可稳定输出数千 fps。这类来源不是"未来要支持的目标"，而是 OBS-PT 默认值（480/1 fps、CQP 20/26（按分辨率）匹配 Distribution Ceiling、NVENC p1 / `tune=ll` / `bf=0` 让编码器跟得上 480fps）设定的**前提假设**。
_Avoid_: 游戏源、Game Capture（这些是 OBS 的输入类型名）、轻量级游戏（早期讨论用语，过于宽泛）

## Flagged ambiguities

- **"Profile 文件夹"**：用户在前期讨论中曾用此词指代整个用户数据根目录。已澄清为 **User Data Root**（物理目录名 `obs-studio/` 沿用上游、不重命名）。**Profile** 一词仅保留其上游原义——一组录制/推流参数包。
- **"配置文件"**：单一中文词覆盖了 Global Config、Profile、Basic Config 三个不同层级。沟通时应使用上述精确英文术语。
- **"便携模式"**：上游与 OBS-PT 同名不同义。上游是"用户可启用的可选特性"，OBS-PT 是"内建不可关闭的运行前提"。在 OBS-PT 项目内文档/讨论中提及 Portable Mode 默认指 OBS-PT 语义。
- **"东亚"**：早期讨论曾用作产品定位描述，**已废弃**。en-US / zh-CN / zh-TW / ja-JP / ko-KR 五语言支持是 i18n 实施范围而非用户群定位，对外口径是 "OBS For PotPvP" 而不是"东亚定制版"。
- **"FPS 玩家"**：早期讨论曾出现，**在当前项目语境中指"未来可能扩展的射击游戏玩家用户群"**，不是当前用户群。当下战略明确收缩在 PotPvP，FPS 玩家拓展是后期议程。

## Example dialogue

> **开发者**：用户报告启动报错"Failed to open basic.ini"。
>
> **领域专家**：先确认是哪个 Profile。Basic Config 是 Profile 内部的文件，路径为 `<Install Root>/obs-studio/basic/profiles/<当前 Profile 名>/basic.ini`。如果 User Data Root 不存在或无写权限，启动期就会拦截并弹窗退出，根本走不到 Basic Config 加载这一步。
>
> **开发者**：那 Global Config 呢？里面记录了"当前 Profile 名"对吧？
>
> **领域专家**：对，Global Config 在 `<User Data Root>/global.ini`，存储跨 Profile 共享的状态，包括当前激活的 Profile 名与 Scene Collection 名。如果 Global Config 不存在，OBS-PT 会用内置的 PotPvP 录制默认值（480/1 Fractional FPS、跟随显示器分辨率、CQP 20/26 按分辨率匹配 Distribution Ceiling，编码器/颜色格式按硬件自适应）生成一份新的，并自动创建一个名为 `PotPvP` 的初始 Profile。
