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

**Elevated Launch / 管理员启动**:
Windows 启动策略，位于 `设置 > 高级 > 常规`，由 Global Config 保存。shipped default 为开启：普通权限启动时请求管理员权限后重新进入 OBS-PT；用户可关闭，设置变更从下次启动生效。用户拒绝 UAC 时静默继续以普通权限运行，不阻止启动。
_Avoid_: 强制管理员清单（EXE 不永久要求管理员权限）、安装器提权（安装器仍按其独立权限策略运行）

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
OBS-PT 默认假设的来源类型——Minecraft Java 版 1.7.10 / 1.8.9 客户端（含 Vanilla、Lunar Client、Badlion Client、PvPLounge 等 PotPvP 圈常见客户端），集显即可稳定输出数千 fps。这类来源不是"未来要支持的目标"，而是 OBS-PT 默认值（480/1 fps、CQP 20/27（按分辨率）匹配 Distribution Ceiling、NVENC p1 / `tune=hq` / `multipass=disabled` / `bf=0` 让编码器跟得上 480fps）设定的**前提假设**。
_Avoid_: 游戏源、Game Capture（这些是 OBS 的输入类型名）、轻量级游戏（早期讨论用语，过于宽泛）

**Application Audio Source / 应用音频来源**:
界面名称为 `程序独立音频捕获`，用于把指定应用的声音从其他系统声音中分离出来。核心用途是在同一录像中分离 Minecraft 游戏声音与后台播放的音乐，也可用于语音软件或其他应用。`进程名匹配` 以可执行文件名作为稳定身份，覆盖所有同名应用实例的音频；窗口标题、窗口类、窗口是否存在以及实例启动顺序都不影响匹配。没有匹配会话时来源保持待命；同名实例出现即自动加入，退出即只移除该实例，其他同名实例不中断，且不要求打开属性或重启 OBS-PT。互不相关的同名实例分别进入捕获，同一父子进程树中的多个音频会话只混入一次。无法确认 exe 身份的会话不进入捕获，并明确提示权限不足；其他可识别的同名实例继续工作。它不保证分离同一浏览器中的窗口或标签页。新建时必须先显式选择一个 Track 2-6，未选择时不能继续；进程名不会触发自动重分类。已有来源不会通过新建对话框再次添加；改变其 Editor Track 归属只属于 Audio and Routing Panel 的路由操作。
_Avoid_: 应用音轨（容易与输出文件里的音轨混淆）、桌面音频（桌面音频会混合系统输出设备上的所有应用）

**Source Identity / 来源身份**:
一个 Scene Collection 内 OBS Source 实例的身份，用于区分同名来源并关联其路由选择。来源暂时失去目标进程或设备时身份仍保留；复制会产生新的身份；删除后以相同名称重新创建的来源不会继承旧身份的路由。不同 Scene Collection 中的同名来源各自拥有独立身份与路由。
_Avoid_: 来源名称（名称可重复或修改，不是身份）

**Audio Source Ownership / 音频来源归属**:
纯音频来源属于整个 Scene Collection，由 `音频与路由` 管理，不属于某个画面 Scene；因此切换画面 Scene 不应停止纯音频捕获。它们由 OBS-PT 持有强引用并标记为 libobs 的独立音频混音根，才能在没有 Scene Item 的情况下进入所有输出混音。带有画面输出的混合来源仍属于其 Scene Item，但其音频输出同时注册到统一路由模型。
_Avoid_: 场景音频（会误解为只在当前 Scene 有效）、全局音频（Global Config 是另一层级）

**Major-Update Configuration Reset / 大版本配置重置**:
破坏性配置边界变更(如统一音频拓扑)时的处理策略。升级路径由安装器完全不触 User Data Root;只有两条路径会对配置做重置：① 旧版本(< Config Generation 阈值的 legacy 根)升级时,安装器把旧配置备份为 `obs-studio.__obspt_backup` 并用保留合并流程恢复指定类别；② 用户显式选择「覆盖安装设置」恢复出厂。两条路径都属于 Force-Override 例外,与 Preset Setting 语义对照。
保留契约只恢复推流设置、录像路径、热键与回放缓存;旧的来源、路由、排版、视频、音频和编码配置不恢复。
覆盖配置只从覆盖前的活动 Profile 读取并分别原样保留简单输出、标准高级录像与自定义 FFmpeg 的三个录像目录,不从非活动 Profile 拼接或回退,不将其中一项同步到其他模式,也不因安装时目录不存在或磁盘离线而回退;每项独立判断,非空值视为用户选择,空值视为缺失并仅为该项初始化 `<Install Root>/recordings`。录像目录是用户拥有的 Profile 值:任何首次初始化或版本修复 bootstrap 都只能替换空值或打包占位值 `recordings`,不得覆盖已有的非默认路径。路径可用性仍由输出启动校验负责。
_Avoid_: 启动迁移(本版本不执行旧音频拓扑迁移)、兼容旧路由

**Track Role / 音轨角色**:
Editor Track 的稳定编号与默认语义：Track 2 `Game`、Track 3 `Music`、Track 4 `Microphone`、Track 5 `Voice Chat`、Track 6 `Aux`。角色名称可由用户改写；Track 1 `Mix` 固定不可改名。
_Avoid_: 音轨名称（用户可改写的文件元数据）、来源类别

**Audio Mode / 音频模式**:
Audio Routing Configuration 的顶层开关，随 Scene Collection 持久化，默认 `多音轨`。`多音轨` 下 Compatibility Mix 与 Editor Track 并存，路由可自由调整；`单音轨` 下只存在 Compatibility Mix，没有 Editor Track，所有输入混成一条，Audio and Routing Panel 只显示 Mix 一行，其余轨道与 Muted Sources Group 完全不显示；混音器中同样只保留归属 Mix 的来源标识，各来源残留的多音轨轨道标识与静音标识不显示。两种模式的设置彼此隔离：单音轨下不能更改任何来源归属或设置，只能静音 Mix，且静音只表现为 Mix 自身处于静音状态，不构成 Muted Sources Group。隔离通过输出拓扑推导实现而非改写状态——单音轨下忽略各来源的静音与监听选择，只认 Mix 自身的静音——因此多音轨的轨道分配与静音选择在切换期间始终不被触碰，切回时原样生效。录制或串流进行中不得切换 Audio Mode，因为它会改变输出轨道拓扑。Application Audio Source 在单音轨下依 Compatibility Mix 既有去重规则自然不重复纳入，来源对象仍然保留。
_Avoid_: 多音轨设置（旧 UI 的位掩码）、单轨录制（易与 Recording Track 数量混淆）

**Audio Routing Configuration / 音频路由配置**:
当前 Scene Collection 内所有音频来源的 Source Identity、Editor Track 归属、轨内顺序、Muted Sources Group 顺序、静音与监听状态，以及 Track 2-6 的用户名称。它与 Profile 的输出和编码参数分离；切换 Profile 不改变路由配置，切换 Scene Collection 则加载对应集合的配置。
_Avoid_: 多音轨设置（旧 UI 中按输出选择的位掩码）、Basic Config（承载 Profile 参数的文件）

**Visual Composition Panel / 画面**:
主窗口中负责 Scene 的视觉构图与画面来源管理的面板，界面名称为 `画面`。纯音频来源不在此处创建或显示；同时含画面与音频的来源仍属于视觉构图，其音频部分同时进入 Audio Routing Configuration。
面板边界按 Source 的输出能力判定：有视频输出的混合来源留在这里，纯音频来源移至 `音频与路由`，不按插件类型名称维护固定名单。唯一的成文例外是 `视频采集设备`：它只贡献画面，音频既不进任何输出也不进 Audio Routing Configuration，需要摄像头麦克风时在 `音频与路由` 单独添加音频输入设备。之所以按类型特判，是因为摄像头的声音来自一台独立物理设备、与画面内容无关；媒体源、浏览器源等来源的声音属于内容本身，照旧保留音频并参与路由。
_Avoid_: 来源（上游旧面板名，混合了视觉与纯音频来源）、音频来源列表

**Audio and Routing Panel / 音频与路由**:
主窗口中负责创建纯音频来源并管理 Audio Routing Configuration 的面板。它表达来源身份、Editor Track 归属、轨内顺序和 Muted Sources Group，不承担电平、推子等信号调节。
每个可管理音频来源在面板中只出现一次；将已有来源拖入其他 Editor Track 或使用“移动到音轨”会重新分配其归属，不创建来源副本。新建来源对话框只负责创建来源，不提供上游语义的“添加现有”。
“来源”列的显示名可通过右键、`F2` 或属性窗口修改，但重命名不改变 Source Identity、路由或输出拓扑；“名称”列的 `F2` 只修改 Track 2-6 的用户名称，Mix 与 Muted Sources Group 名称固定。已知旧版默认 Desktop Audio 乱码只在稳定默认身份与精确乱码值同时匹配时自动修复，绝不猜测或改写其他用户名称。
底部命令与 Visual Composition Panel 工具栏采用同一模型：始终可见、始终保持可交互外观，从不进入禁用态，因此鼠标悬停一律显示白色边框；当前上下文不适用时点击不产生任何操作，也不提示。单元格选中高亮占满整个单元格，而不只覆盖文字。右下角的音频模式入口是唯一视觉例外，以绿色标识当前生效的 Audio Mode。上下文不适用与模式锁定的区别在此体现：前者静默无操作，后者提示先切换到多音轨模式。
Settings > Audio 只保留引擎级参数；Desktop/Mic/Aux 设备来源不再从设置页创建。
_Avoid_: 音频路由（旧面板名）、高级音频属性、非量化音频处理、添加现有（会误解为创建第二个来源引用）

**Audio Mixer / 混音器**:
主窗口中负责显示音频信号并提供电平、音量、推子和静音等实时调节的面板；其来源顺序和 Track Role 标识与 Audio and Routing Panel 使用同一份 Audio Routing Configuration。
它显示路由模型中的全部可管理来源，包括断连、inactive、隐藏 Scene 与 Muted Sources Group 中的来源；运行状态只改变电平显示，不改变来源是否出现或其顺序。
_Avoid_: 音频与路由、可量化音频处理（“量化”在音频工程中另有含义）

**Output Topology Validation / 输出拓扑校验**:
每个输出开始前，对目标容器和编码器是否能完整承载当前 Recording Topology 的独立校验。任何已纳入拓扑的音轨无法初始化时，该输出无效；不得静默丢轨或自动降级为部分拓扑，也不得因此停止已经运行的其他输出。
_Avoid_: 编码器可用性（单个编码器的硬件能力检查）、音频电平检测

**Unified Audio Bitrate / 统一音频码率**:
所有 Compatibility Mix、Editor Track、VOD 与串流音频编码器共享的一项 Profile 输出参数，默认 `160 Kbps`。它属于编码设置而非 Audio Routing Configuration；任一相关输出活动时保持不变。
_Avoid_: 逐轨码率、来源音量（来源音量是实时混音控制）

**Mix Track Behavior / Mix音轨行为**:
一项 Profile 输出参数，取值为 `生成`（默认）或 `不生成`，决定多轨录像拓扑中是否写入 Compatibility Mix。`生成` 时普通播放器默认完整听到所有声音，但剪辑软件会同时导入 Mix 与各 Editor Track，音量重叠；`不生成` 时录像文件只含各非空 Editor Track，普通播放器只听到第一条（默认 `Game`），剪辑软件导入无重叠。它只作用于能承载多轨的本地录像——标准录像、自定义 FFmpeg 多轨容器与 Replay Buffer；串流、VOD Track 与单轨容器始终使用 Compatibility Mix，Audio Mode 为 `单音轨` 时该参数被忽略。任一输出活动期间不可更改。由于 Desktop Audio 固定属于 Compatibility Mix，`不生成` 时仅经桌面音频捕获、未被任何 Application Audio Source 覆盖的声音不会写入录像。它属于输出设置而非 Audio Routing Configuration。
_Avoid_: 混音开关（它决定录像文件的流构成，不是实时混音控制）

**Recording Track / 录制音轨**:
录制文件中的一条独立音频流。OBS-PT 将 Recording Track 分为固定的 Compatibility Mix 与可管理的 Editor Track；除固定属于 Compatibility Mix 的 Desktop Audio 外，一个来源在管理界面中只能归属一个 Editor Track 或 Muted Sources Group。
_Avoid_: 音频来源（来源是输入，音轨是输出）、声道（声道指一条音轨内部的 Mono/Stereo/5.1 布局）

**Compatibility Mix / 兼容混音**:
存在可录制且未静音的来源时，Compatibility Mix 固定为 Recording Track 1、以 `Mix` 为元数据名称，供普通播放器默认播放并用于串流；本地输出在全部来源静音时不生成任何音频流，串流则始终保留 Compatibility Mix 传输流并发送数字静音。它由系统生成，不属于用户对来源的 Editor Track 分配；会话启动时成功初始化且未静音的 Desktop Audio 设备共同构成系统播放权威组，固定显示在此处并纳入 Mix，同时纳入麦克风、采集卡、媒体源等非系统播放来源；只要权威组仍有一个未静音设备，Application Audio Source 就不重复纳入，仅在全部设备不可用或被用户主动静音时回退纳入，解除任一设备静音后恢复去重。该权威组不会因音量电平、设备断连或重连而自动切换。Compatibility Mix 有意与 Editor Track 同时写入文件，以保证播放器优先的兼容性；剪辑软件可能同时导入这些轨道。
Mix 的名称、顺序与来源归属固定；用户可选择其中显示的 Desktop Audio 来源并查看属性或切换静音，但不能将其移动到 Editor Track、重新排序或删除。
_Avoid_: 桌面音频（Desktop Audio 是输入来源）、编辑轨、主音轨

**Editor Track / 编辑轨**:
Recording Track 2-6，供剪辑软件分别处理声音；默认角色依次为 `Game`、`Music`、`Microphone`、`Voice Chat`、`Aux`，每个可录制来源最多归属其中一个 Editor Track。Application Audio Source 在创建对话框中由用户显式选择归属，系统麦克风与音频输入设备默认归属 Track 4 `Microphone`；其他未知音频来源默认选择首个空轨、无空轨时选择 Track 6 `Aux`。角色名称可改，轨道编号与上下顺序固定，来源在轨道内的显示顺序可独立调整。
_Avoid_: mixer、应用音频来源、独立声道

**Muted Sources Group / 已静音来源**:
与来源静音状态完全等价的管理分组；其中来源不进入 Compatibility Mix、Editor Track、串流或 VOD。若来源同时处于 Monitor Only，静音优先；解除静音时，普通来源返回先前的 Editor Track、位置与 Monitor Only 状态，Desktop Audio 则返回其固定的 Compatibility Mix。处于该分组的 Desktop Audio 可以参与分组内排序，但仍不能分配到 Editor Track 或删除。
_Avoid_: 静音轨道、不录制分组、Track 7、禁用音轨

**Monitor Only Source / 仅监听来源**:
监听模式为 `Monitor Only (mute output)` 的音频来源。它保留原 Editor Track 归属，但只发送到监听设备，不进入 Compatibility Mix、Editor Track、串流或 VOD，也不使所属 Editor Track 成为有效音轨；它与 Muted Sources Group 不同，若两者同时存在则由静音分组优先管理。
_Avoid_: 已静音来源（来源静音与仅监听是两种独立状态）、监听音轨

**Monitoring Feedback / 监听回送**:
来源的 `Monitor and Output` 信号经系统监听设备再次被 Desktop Audio 捕获而形成的潜在重复路径。OBS-PT 保留用户显式路由，不自动改写；设备重合时只提示风险。
_Avoid_: 音频回声（用户听感问题）、来源重复归属（来源仍只能归属一个 Editor Track）

**Private Audio Source / 私有音频来源**:
由转场或插件内部产生、没有可供用户管理的 Source Identity 的音频来源。它固定进入 Compatibility Mix，不生成 Editor Track，也不出现在 Audio Routing Configuration 面板；插件公开为普通 Source 后才适用常规路由规则。
_Avoid_: 隐藏来源（隐藏的用户来源仍可拥有 Source Identity）

**Recording Topology / 录制音轨拓扑**:
标准录像、自定义 FFmpeg 录像、Replay Buffer 或启用 VOD Track 的串流会话启动时确定的 Compatibility Mix、来源资格与非空 Editor Track 集合；任一此类会话活动期间保持冻结，并行会话全部结束后才可修改。活动期间新增的来源或会话开始时已静音来源延后到下一会话，删除来源不会移除当前文件的既有轨道；会话开始时已纳入资格的来源可以在会话内静音并恢复。来源是否激活、可见、连接或实际有声不改变资格；真正没有任何来源的 Editor Track 不纳入拓扑。仅使用 Compatibility Mix 的串流不冻结该拓扑。Editor Track 是否纳入输出只取决于用户的来源归属与静音选择，不根据音频电平、目标连接状态或实际内容判断。仅支持单音轨的输出使用 Compatibility Mix；无法承载该拓扑的输出不允许启动。
_Avoid_: 实时音轨列表、动态音轨、轨道路由

**VOD Track / VOD 存档轨**:
仅供支持独立存档音频的串流服务使用、从一个 Editor Track 选出的存档声音；Bilibili 等不支持该能力的主要串流场景不显示此概念。启用 VOD Track 时，启动前所选 Editor Track 必须包含至少一个未静音来源；空轨不会回退到 Compatibility Mix。开播后临时静音只会令 VOD Track 暂时静音，不停止串流。
_Avoid_: 第二串流音轨、多轨串流、录像音轨

**Java Detection / Java 判别**:
Global Config `[General] JavaDetection` 中的全局开关，控制 Java 家族可执行文件的匹配范围——**严格**（Strict，默认）保持精确的进程名匹配；**宽松**（Lenient）使 `javaw.exe` 与 `java.exe` 双向等价，画面捕获与应用音频捕获同时生效。用户在**设置 → 高级**底部选择，任一输出活动时不可修改（守卫同 Audio Mode）；安装/升级不得覆盖用户已设值。
_Avoid_: Java 兼容模式、Java 宽松匹配

**Preset Setting / 预置设置**:
绝大多数设置的默认回填方式——只在用户尚未设定(配置中缺少该键 User Value,或值为打包占位符)时写入默认值,已经存在的用户值(含用户显式改回与默认相同的值)一律不得被覆盖。预置语义贯穿全新安装首启 bootstrap 与升级路径:全新安装首启用预置逻辑补齐缺失默认,升级路径由安装器完全不触碰 User Data Root。保证用户自定义设置(多音轨状态、热键、视频输出位置、回放缓存状态等)在任何安装/升级/修复中不丢失。
_Avoid_: 重置为默认(指无条件覆盖,_预置_只在缺失/占位时写)

**Force-Override Setting / 强覆盖设置**:
少数的、语义等价于「恢复默认设置」的例外项,安装/升级时会被显式重置为出厂默认。当前唯一成员是 Global Config `[General] RunAsAdministrator`(`设置 › 高级 › 常规` 的「以管理员身份运行 OBS-PT」)。除恢复出厂外,它平时与预置设置无异——用户关闭后升级不被改回。与预置设置的差别只在于「恢复默认」动作会把它重置为默认值。仅在用户显式选择安装器的「覆盖安装设置」时才触发。
_Avoid_: 每次覆盖(它平时不覆盖用户值,只在恢复出厂时重置)

**覆盖安装设置 / Force-Reset Install**:
安装器为已有安装提供的显式「恢复出厂默认」入口:默认不勾选;勾选后弹窗确认,确认则把当前 User Data Root 备份为 `obs-studio.__obspt_backup`(改名而非删除,装完提示备份位置),再部署全新模板。全新安装不显示此选项(没有可覆盖的对象,直接部署模板)。选它表示放弃保留当前全部配置(含预置项),全部回到出厂模板。
_Avoid_: 覆盖安装(一般指正常升级,此选项是显式的重置动作)

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
> **领域专家**：对，Global Config 在 `<User Data Root>/global.ini`，存储跨 Profile 共享的状态，包括当前激活的 Profile 名与 Scene Collection 名。如果 Global Config 不存在，OBS-PT 会用内置的 PotPvP 录制默认值（480/1 Fractional FPS、跟随显示器分辨率、CQP 20/27 按分辨率匹配 Distribution Ceiling，编码器/颜色格式按硬件自适应）生成一份新的，并自动创建一个名为 `PotPvP` 的初始 Profile。
