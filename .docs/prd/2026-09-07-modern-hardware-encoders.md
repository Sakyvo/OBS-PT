# Modern Hardware Encoders — HEVC / AV1 录制与串流（1.2.0）

**Status**: Accepted
**Date**: 2026-09-07
**Grill session**: 现代硬件编码器移植（参考上游 OBS 30.2.0）
**Related**: ADR 0004（AMD 编码器切换）；CONTEXT.md 术语 `Declared Repair`、`Runtime Acceptance Pending`、`Preset Setting`；`.docs/obspt-bootstrap.md`（Hardware Encoder Initialization Boundary Contract）

## Problem Statement

OBS-PT 的录制能力停留在 H.264 一代：NVIDIA 只走 `jim_nvenc` H264、AMD 只走已归档的 legacy `enc-amf` H264、Intel 走旧 MSDK 插件。拥有新硬件（GTX 10 系+/RTX 40 系、RX 6000/7000 系、Intel Arc）的 PotPvP 玩家，无法利用 HEVC/AV1 在同码率下更清晰的画质红利；而后期端（Vegas 22）已具备 HEVC 与 AV1 的硬解消费能力，录制端的代差成为瓶颈。同时 legacy AMD 插件带来持续的 fork 维护税（MSVC clamp hack、AMF SDK staging 桥、私有预设契约）。

## Solution

把上游 OBS 30.2 的现代编码器实现移植进 OBS-PT：NVENC 的 HEVC/AV1、AMD texture-amf 的 H264/HEVC/AV1、以及承载它们的输出容器（Hybrid MP4、普通 MP4、MKV）与串流信令（Enhanced-RTMP + 服务 codec 协商）。默认编码器策略**不变**（全新安装与升级后仍为各厂 H264 探测结果），HEVC/AV1 纯 opt-in；存量用户通过一次**声明式修复**（BootstrapVersion 3→4）获得 AMD 编码器 id 迁移，其余用户无感升级。Intel Arc/QSV 推迟到 1.3.0。

## User Stories

1. 作为拥有 RTX 30/40 系显卡的 PotPvP 玩家，我想在设置里手动选择 NVENC HEVC 或 AV1 录制编码器，以便用同样的磁盘空间录出更高画质的 480fps 素材。
2. 作为拥有 GTX 10/16 系显卡的玩家，我想选择 NVENC HEVC 录制，以便老卡也能吃到 HEVC 编码红利。
3. 作为拥有 RX 6000/7000 系显卡的玩家，我想选择 AMD HEVC 或 AV1 录制编码器，以便不换 N 卡也获得现代编码能力。
4. 作为 RTX 40 系玩家，我想选择 NVENC AV1 录制，以便利用 AV1 的编码效率在 Distribution Ceiling 之下最大化源端质量。
5. 作为 OBS-PT 1.1.0 的存量 AMD 用户，我升级 1.2.0 后想录像直接继续工作（自动迁移到新 AMD 插件），不需要手动重建配置。
6. 作为 OBS-PT 1.1.0 的存量 NVIDIA 用户，我升级后想默认编码器保持不变、模板刷新但不丢我选过的编码器，除非那是被声明修复覆盖的键。
7. 作为改过分辨率的存量用户，我升级 1.2.0 后想我的 Base/Output 分辨率原样保留，不被修复路径拍回显示器分辨率。
8. 作为普通玩家，我想在 Hybrid MP4（默认格式）里直接录出 HEVC/AV1 文件，不被迫切到 MKV。
9. 作为手动选择普通 MP4 的用户，我想普通 MP4 也能承载 HEVC/AV1，保持"后备格式与主格式能力一致"。
10. 作为 MKV 用户，我想 MKV 录制 HEVC/AV1 正常工作，容器不成为限制。
11. 作为推流用户，我想在高级输出里选 HEVC/AV1 串流编码器，以便向支持 Enhanced-RTMP 的平台（如 YouTube）推更高效的流。
12. 作为推 Bilibili（H264-only 服务）的用户，我在选了 HEVC/AV1 串流编码器时想在开播前收到明确警告，而不是开播后黑屏。
13. 作为手动切换编码器的用户，我想 HEVC/AV1 编码器的落地默认值符合 OBS-PT 吞吐哲学（CQP、速度导向预设、B 帧关闭），一选即能跑 480fps，而不是一选就掉帧。
14. 作为 Vegas 22 用户，我想 OBS-PT 录出的 HEVC/AV1 Hybrid MP4 能被时间线直接导入并用 NVDec 硬解，帧混合工作流不中断。
15. 作为剪辑工作流用户，我想录制文件里的多音轨（Compatibility Mix + Editor Track）在 HEVC/AV1 容器里照常完整保留。
16. 作为 Replay Buffer 用户，我想回放缓冲在 HEVC/AV1 编码器选择下照常工作（跟随 ffmpeg-mux 能力），行为与 H264 一致。
17. 作为不支持 HEVC 的老卡用户，我的设置列表里不出现我硬件跑不了的编码器（caps 门控注册，无假选项）。
18. 作为全新安装用户，我想首次运行探测仍然选中各厂 H264 硬件编码器，默认体验与 1.1.0 一致。
19. 作为五语言用户，我想新编码器的名称与设置项在我使用的语言里正常显示（locale 随上游条目补齐）。
20. 作为从旧版升级的用户，我想安装器不触碰我的 User Data Root（同代 overlay），我的热键、路径、场景全部原样。
21. 作为系统管理员 / 高级用户，我想日志里能看清每个新编码器的初始化、能力探测与厂商边界值（请求值 vs 生效值），便于排障。
22. 作为使用 8-bit NV12 的用户，我想录制与串流的颜色行为（ColorFormat/ColorSpace）与现有 H264 契约一致，无意外色彩漂移。
23. 作为同时录制 + 推流的用户，我想选 HEVC 录制 + H264 推流互不干扰，输出拓扑校验照常工作。
24. 作为社区 AMD/40 系用户，我想从发布说明里看清哪些编码器路径已真机验收、哪些 Runtime Acceptance Pending，再决定是否尝鲜。

## Implementation Decisions

### 范围与里程碑

- 版本 1.2.0；三个编码器移植面（NVENC、AMD）+ 输出容器 + 串流信令 + 声明式修复，一批交付。
- **Intel Arc / QSV 推迟到 1.3.0**，触发条件：获得 Arc 验收硬件（自购或社区远程验收）。推迟原因：唯一需动 libobs `encode_texture2`/P010 基底、完全无真机可测、对主力工作流零收益；旧 MSDK 插件在 Arc 上无法初始化，无更便宜的中间门。
- StreamFX 不作参考（其 NVENC 已被上游 29/30 超越，项目已归档）；唯一主参考为上游 30.2.0 对应实现。

### 编码器移植

- **NVENC**：以 OBS-PT 现有 `jim-nvenc` 文件为底、从上游 30.2 `obs-nvenc` 移植 HEVC/AV1 路径；SDK 头已为 12.0（含 AV1 结构），无需换 SDK。按驱动 caps 门控注册 `jim_nvenc_hevc`/`jim_nvenc_av1`（不支持即不注册）。保留 OBS-PT 现有 `jim_nvenc` id 与现代 preset2 读取路径。
- **AMD**：整体替换 legacy `enc-amf`（ADR 0004）——退役子模块、CMake AMF SDK staging 桥、MSVC `/FI` hack；移植上游 `texture-amf` + `obs-amf-test` 能力探测；deps AMF SDK 头升级到含 AV1 组件头的版本。注册 `h264_texture_amf`/`h265_texture_amf`/`av1_texture_amf` + 隐藏 `_fallback` 变体，随能力门控。legacy 的顶层 Preset 单条 `OBS-PT` 组合框契约**不复刻**，新插件用上游自带属性面板。
- **默认值调优**（所有新编码器 `get_defaults`）：CQP/统一 0–51 标尺、吞吐导向预设（p1 级/速度级）、B 帧关闭、keyint 2s；与现有 CQP 20/27（按 BaseCY <1080 与否）映射哲学一致。AMF H264 探测结果另有 bootstrap 调优模板；HEVC/AV1 仅靠 `get_defaults`（默认不选）。
- **位深**：v1 仅 8-bit NV12 / 4:2:0，与现有 H264 契约一致；10-bit/P010 独立 issue 另议（收益受 Distribution Ceiling 压制，且需动 libobs 视频格式）。

### 输出容器

- **Hybrid MP4**：mp4-mux 从上游移植 HEVC/AV1 的参数集解析与 hvcC/av1C/hvc1/av01 sample entry；libobs 侧补 `obs-hevc.h`/`obs-av1.h` 辅助头（解析/寻参函数，纯函数层，不涉及渲染）。`mp4_output` 的 codec 声明放开为 h264;hevc;av1。
- **普通 MP4 / MKV**：ffmpeg-mux 移植上游的 codec tag / extradata 处理，使 MP4 写出正确的 sample entry；MKV 基本免费。Replay Buffer 维持现有 `replay_buffer` 契约（跟随 ffmpeg-mux 能力，不 Hybrid 化）。
- 多音轨拓扑（Compatibility Mix + Editor Track）在新 codec 下照常完整写入，Output Topology Validation 不因 codec 变化而放宽。

### 串流

- **完整上游行为**（Q15a）：移植 Enhanced-RTMP 信令（HEVC/AV1 的 rtmp 侧头与流握手段）、`rtmp_output_info` codec 声明放开；libobs `obs_service_info` 补齐 30.2 的服务协商回调面（`get_supported_video_codecs` 等，加可选回调、整树同编译无 ABI 问题）；UI 在服务不支持所选 codec 时开播前警告。对齐上游结构体的理由：为未来搬运 WHIP/服务侧代码清障，避免半对齐结构体的长期维护税。
- 串流**默认**不变（仍为 H264 探测结果）；Simple 模式维持 h264 四项不动；Advanced 串流+录像编码器列表靠现有枚举机制自动放开（零新控件）。

### 升级与配置（Declared Repair）

- BootstrapVersion 3→4 触发一次声明式修复：重探测编码器、按探测结果重写 `[AdvOut] RecEncoder`/`Encoder` 与全新 `recordEncoder.json` 模板（含 AMD `amd_amf_h264 → h264_texture_amf` 迁移；`jim_nvenc` id 不变无感；手动选过 obs_x264 的用户被重置回探测默认——已确认接受）。
- **硬约束**：early repair 的视频重探测（`apply_monitor_video_to_profile`）必须单独门控为"仅 BootstrapVersion <3 时执行"，否则 1.1.0 存量用户的自定义分辨率会被拍回满屏。
- 不 bump `CONFIG_GENERATION`；安装器走同代程序文件 overlay，不部署 User Data Root 模板；**NSIS 必须显式删除**旧 `enc-amf.dll` 与其 data 目录（overlay 不删文件，残留会双 AMD 并存）。
- 048 审计不变式保持：除声明修复覆盖的键外，任何 bootstrap 写入不碰既有用户值。

### 交付姿态

- 验收矩阵逐项标注三态：3060 已真机验收 / 已构建待验收（Runtime Acceptance Pending）/ 不适用。发布说明同口径；募社区 AMD/40 系用户做远程验收，通过后摘标。
- 3060 可真机验收范围：H264 回归、NVENC HEVC 完整链路（录制 → Vegas 22 帧混合消费）；NVENC AV1、AMF 全系、HEVC 串流信令为待验收。
- 480fps 契约不变：任何厂商边界限制（如厂商级帧率上限）只允许改传给该厂商 API 的生效值并记录日志，Profile 保持 480/1；HEVC Level 6.2 / AV1 Level 6.3 足以表达 1080p@480。

## Testing Decisions

测试只断言外部可观察行为（文件内容、注册结果、日志、UI 可见列表），不断言内部实现细节。四个接缝（已与用户确认）：

- **Seam A（唯一新增）— 编码器能力单元测试目标**：仿现有 updater-model-test 的 CTest 形态建新测试目标，覆盖：Level 有界查表（常规行、恰好在最大边界、首个超界值、零分母、溢出输入——边界契约第 6 节强制）、统一质量标尺到各厂商参数映射、AMD id 迁移表。纯逻辑、无 GPU 依赖、任何构建机可跑。
- **Seam B（既有）— 打包链验证**：构建 → staging 刷新 → 单进程 NSIS → `7z t` → 启动到 Setup 主窗口；断言包内无 `enc-amf.dll` 残留、新 AMD 插件与 `obs-amf-test` 在包内、卸载路径清残留。
- **Seam C（既有）— 运行时烟测 + 日志断言**：3060 上 HEVC 录制启停 10–30 秒、日志无厂商 invalid/unsupported 参数错误、产出 MP4 被 Vegas 22 正常导入；升级路径烟测（1.1.0 配置树 → 1.2.0，断言 AMD id 迁移、分辨率保留、无 enc-amf 残留）。无真机项按 Runtime Acceptance Pending 标注，不宣称可用。
- **Seam D（既有）— 静态检查 / 配置断言**：grep 断言（codec 声明、探测优先级、安装器删除逻辑、串流警告存在）+ 模板 JSON 内容断言（逐编码器键表、bootstrap 只在 v4 repair 时改写、视频重探测门控 <3）。

Prior art：`.docs/issues/` 既有 issue 的验证矩阵模式、Hybrid MP4 契约的安装器/哈希验证流程、bootstrap 的 launch-once 断言模式。

## Out of Scope

- Intel Arc / QSV（含 `encode_texture2` 与 P010 的 libobs 反向移植）——1.3.0。
- 10-bit / HDR / P010 / 4:2:2 色域——独立 issue。
- Simple 模式输出的编码器扩展（维持 h264 四项）。
- Replay Buffer 的 Hybrid MP4 化（维持现有契约）。
- 默认编码器策略变更（任何形式的新装机/升级默认 HEVC/AV1）。
- 自定义 FFmpeg 输出路径改动。
- AMD legacy 契约复刻（顶层 OBS-PT 预设组合框、`AMF.H264.*` 精确键模板体系）。
- FFmpeg 依赖大版本升级（deps 维持 4.4）。
- 安装器保留合并 / CONFIG_GENERATION 变更。

## Further Notes

- Vegas 22 事实（已查官方文档）：HEVC 完整支持；AV1 4:2:0 8-bit 在 GA10x（含 RTX 3060）NVDec 硬解 ✓，另有 DAV1D 软解兜底——v1 的 8-bit NV12 范围恰好落在硬解支持格内。
- 3060 无 AV1 编码器（Ada 才有），NVENC AV1 路径在本机只能构建级验证。
- AMD 探测器（obs-amf-test）需要随包分发；其能力探测结果驱动 `texture-amf` 的注册门控。
- 上游 30.2 的 `rtmp_output_info` 本身就是 codec 协商形态，移植时以其为准；OBS-PT 现有 `rtmp_output_info` 已声明 `encoded_video_codecs = "h264"`（mp4_output 移植带进来的机制），放开这层是串流支持的前置。
- 移植完成后需同步更新 `.docs/obspt-bootstrap.md`：AMF 模板表、探测优先级表、v4 repair 门控语义、删除 legacy AMF 契约段——随对应 issue 落地，不单开任务。
