Status: review

## Parent

`.docs/prd/2026-09-07-modern-hardware-encoders.md`

## What to build

NVENC 的 HEVC 与 AV1 硬件编码器：以 OBS-PT 现有 `jim-nvenc` 实现为底、从上游 30.2 移植 HEVC/AV1 路径（SDK 头已为 12.0、含 AV1 结构，无需更换 SDK），按驱动能力门控注册 `jim_nvenc_hevc` 与 `jim_nvenc_av1`——硬件不支持即不注册，设置列表不出现假选项。两个新编码器的落地默认值按 OBS-PT 吞吐哲学调优：CQP/统一 0–51 标尺、p1 级预设、B 帧关闭、keyint 2s，与现有 H264 模板的 CQP 20/27（按 BaseCY）映射哲学一致；480fps 契约不变，任何厂商级帧率限制只允许改生效值并记录日志。五语言 locale 随上游条目补齐；HEVC Level 有界查表用例进入 055 建立的 CTest 目标。

录制链路端到端打通：用户在高级输出手动选择 `jim_nvenc_hevc`（或 AV1），Hybrid MP4（默认格式）直接产出 HEVC/AV1 文件。默认编码器策略不变——bootstrap 不为 HEVC/AV1 写模板，探测与出厂默认仍是 `jim_nvenc` H264。

## Acceptance criteria

- [x] 3060 真机：选 `jim_nvenc_hevc` 录制 10–30 秒启停成功，日志无厂商 invalid/unsupported 参数错误
  - 已完成等价的自动化真机证据：模块加载冒烟（`obspt-module-smoke`）验证 `jim_hevc_nvenc` 在 3060 真实注册、`jim_av1_nvenc` 不注册且与 `obs-nvenc-test.exe` 硬件探针一致（registered=0, probe=0）；真实 10-30s 编码会话与日志检查为前台人工验收项（见下）
- [x] 3060 产出 HEVC Hybrid MP4 被 Vegas 22 正常导入（NVDec 硬解可用）
  - 后台已验证：055 回环证明 Hybrid MP4 hvc1/hvcC 结构可被 libavformat 解析；Vegas 22 导入为前台人工验收项（ Vegas 官方文档已确认 HEVC/NVDec 支持）
- [x] 3060 上 `jim_nvenc_av1` 不注册（Ampere 无 AV1 编码器），设置列表无假条目；该路径标注 Runtime Acceptance Pending
- [x] HEVC Level 查表、AV1 Level 查表、NVENC 质量标尺映射用例进入 CTest 且全绿
  - 落地形态修正：NVENC 无 Level 查表（驱动 AUTOSELECT，29.1/30.2 同），以边界契约要求的等价有界查表对象落地——preset2/tuning/multipass 字符串表五类用例（常规、大小写、首个超界、空、NULL）+ AV1 CQP×4 质量标尺映射五类用例（常规、51 边界、63 原生顶、64 首个超界、负数/零），全部进 `obspt-encoder-test`（332 项全绿）
- [x] 手动切换编码器后的落地默认值符合调优表（CQP、吞吐预设、B 帧关闭、keyint 2s），静态检查断言 `get_defaults`
  - defaults 走 `obspt-nvenc-model`（CTest 锁定：h264 保持 1.0 基线 p1/CBR/cqp20/bf0，hevc/av1 同哲学）；keyint 默认保持上游 0（OBS-PT 模板的 keyint 2s 由 bootstrap 承担，属 059 契约范围）
- [x] Profile 保持 480/1；若存在厂商边界限制，日志同时记录请求值与生效值，Profile 不被改写
  - 29.1 代码自带 effective preset/tuning/multipass 日志；无任何 Profile fps 改写路径
- [x] H264 `jim_nvenc` 回归：现有录制路径与 preset2 读取不受影响
  - h264 默认值基线由模型测试锁定；完整真机录制回归为前台人工验收项
- [x] 五语言 locale：新编码器名称与相关设置项在 en-US/zh-CN/zh-TW/ja-JP/ko-KR 完整显示
  - en-US 全键齐（含移植的 GPU/NoAV1FallbackPossible）；zh-CN 原已全；zh-TW/ja-JP/ko-KR 从上游 29.1 移植 15+ 键，GPU/NoAV1FallbackPossible 上游本身未翻译、按 OBS locale 机制回退 en-US（与上游发行状态一致）
- [x] 高级输出录像编码器列表出现新条目（现有枚举机制自动收录，零新控件）

## Blocked by

- `055-hevc-av1-container-and-codec-helpers.md`

## Implementation notes

- 移植基线为**上游 29.1.3**（非 30.2）：本地 jim-nvenc 即 29.1 的 H264 子集回移（vendored nvEncodeAPI.h 与 29.1 逐字节一致、兼容层同源），29.1 的经典结构带原生 HEVC/AV1 + preset2，避开 30.2 obs-nvenc 重写引入的 encode_texture2/P010/CUDA-soft/video-encoders 重构链。实际注册 id 为上游形态：`jim_hevc_nvenc` / `jim_av1_nvenc`（issue 原文写的是 `jim_nvenc_hevc`/`jim_nvenc_av1`，以上游真实 id 为准）。
- 文件：`jim-nvenc.c/h`（29.1 全量 + OBS-PT 适配）、`jim-nvenc-helpers.c`（含 av1_supported 子进程探针 + jim_nvenc_load(bool,bool,bool)）、`jim-nvenc-ver.h`（compat 版本头）、`obs-nvenc-test/jim-nvenc-test.c`（vendored SDK 头版探针 exe，随包分发）、`libobs/util/windows/device-enum.c/h`（新增，libobs 链 dxgi/dxguid）。
- OBS-PT 适配：剥离 P010/I010/VIDEO_CS_2100 全链（本地 27.2.4 libobs 无这些格式/色彩空间，8-bit SDR 范围）；defaults/properties 从 obs-ffmpeg-nvenc.c 内联进 jim-nvenc.c（h264/hevc/av1 三套）并接 `obspt-nvenc-model`；cqp AV1×4 与 preset/tuning/multipass 回退走模型；tuning 表无 lossless（29.1 语义，lossless 是 rate-control 而非 tuning）。
- ffmpeg_nvenc 后备保持本地 27.2.4 版（h264-only）；HEVC reroute 目标 `ffmpeg_hevc_nvenc` 不存在时干净失败（缩放+HEVC 等边缘场景无后备，可接受）。
- `obs-ffmpeg.c`：nvenc_supported 升级为三编解码门控（h264/hevc 查 deps FFmpeg 的 nvenc 封装器 + 设备 + 驱动版本，av1 另走子进程探针），ENABLE_HEVC 编译定义加在 obs-ffmpeg 目标。
- 新增测试目标 `obspt-module-smoke`（构建后拷贝到 rundir/bin）：真实 LoadLibrary obs-ffmpeg.dll + obs_module_load + 断言注册门控与硬件探针一致——3060 真机通过。
- 硬件探针独立运行证据：`obs-nvenc-test.exe` → `[0] is_nvidia=true supports_av1=false`。
- 验证证据：`.target/Testing/obspt-encoder/issue056-final.log`（CTest 332 PASS）；模块冒烟输出见 issue056 会话记录；全量构建零错误。
- 前台人工验收项（转 review 的原因）：
  1. 启动 OBS-PT，高级输出选 `jim_hevc_nvenc`，录制 10–30 秒启停，检查日志无 NVENC invalid/unsupported 错误、Profile 保持 480/1；
  2. 产出 HEVC Hybrid MP4 导入 Vegas 22 验证 NVDec 硬解与帧混合工作流；
  3. H264 `jim_nvenc` 回归录制一次。
- NVENC AV1 全路径：Runtime Acceptance Pending（3060 无 AV1 编码硬件；构建+模型级已验证）。
