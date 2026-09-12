# 0004 — AMD 编码器实现切换为上游 texture-amf

OBS-PT 1.0/1.1 通过 vendored legacy `enc-amf`（Xaymar 1.4.3.7 fork，`obs-pt` 分支）提供 AMD H.264 编码（id `amd_amf_h264`），代价是 MSVC `/FI` clamp 宏 hack、CMake AMF SDK staging 桥、以及一套 OBS-PT 专属的 AMF 预设组合框 / `AMF.H264.*` 精确键契约。为了在 RDNA2+/RDNA3+ 上交付 HEVC 与 AV1 录制，我们决定**整体退役** legacy 插件，移植上游 30.2 的 `texture-amf.cpp` + `obs-amf-test` 能力探测（新 id `h264_texture_amf` / `h265_texture_amf` / `av1_texture_amf` + 隐藏 `_fallback` 变体，随能力门控注册），并通过声明式修复（BootstrapVersion 3→4）做一次性 id 迁移 `amd_amf_h264 → h264_texture_amf`；安装器 overlay 时显式删除旧 `enc-amf.dll` 与其 data 目录，防止双 AMD 编码器并存。

## Considered Options

- **双插件并存**：id 不冲突但设置列表出现两套 AMD H.264，UX 混乱，否决。
- **在 legacy 代码里手写 AV1/HEVC 组件**：等于在 2020 年归档代码上自研编码器，且 deps 的 AMF SDK 头连 `VideoEncoderAV1.h` 都没有，否决。
- **AMD 整体推迟**：让 AMD 用户在 HEVC/AV1 里程碑里继续停留在 fork 维护税上，否决。

## Consequences

- 删除 `plugins/enc-amf` 子模块、CMake AMF-SDK staging 桥、MSVC `/FI` hack；deps 的 AMF SDK 头升级到含 `VideoEncoderAV1.h` 的版本。
- legacy 专属契约（顶层 Preset 组合框仅暴露 `OBS-PT` 条目、`AMF.H264.*` 精确键模板）**不再复刻**；新插件按 OBS-PT 调优哲学（CQP、吞吐导向预设、B 帧关闭）设置 `get_defaults`，探测到的 `h264_texture_amf` 走 bootstrap 调优模板。
- 存量 AMD 用户升级后由声明式修复迁移到新 id；开发机无 AMD 硬件，AMD 全系以「已构建、待真机验收」姿态交付。
