Status: review

## Parent

`.docs/prd/2026-09-07-modern-hardware-encoders.md`

## What to build

按 ADR 0004 整体替换 AMD 编码器实现：退役 legacy `enc-amf` 子模块（连同 CMake AMF SDK staging 桥与 MSVC `/FI` clamp 宏 hack），移植上游 30.2 的 `texture-amf` 插件与 `obs-amf-test` 能力探测器，deps 的 AMF SDK 头升级到含 AV1 组件头的版本。注册 `h264_texture_amf`、`h265_texture_amf`、`av1_texture_amf` 三个纹理编码器及隐藏的 `_fallback` 变体，全部由 `obs-amf-test` 的能力探测结果门控——无 AMD 硬件时零注册，对非 AMD 用户零影响。探测器随安装包分发。

legacy 的顶层 Preset 单条 `OBS-PT` 组合框契约与 `AMF.H264.*` 精确键模板体系不复刻；新插件的 `get_defaults` 按 OBS-PT 调优哲学设置（CQP/统一标尺、速度导向预设、B 帧关闭、keyint 2s），其中 H264 的 bootstrap 调优模板在 059 中落表。安装器必须显式删除旧 `enc-amf.dll` 与其 data 目录（程序文件 overlay 不删除文件，残留会导致双 AMD 编码器并存）。

## Acceptance criteria

- [x] 仓库不再引用 `plugins/enc-amf` 子模块：CMake staging 桥、MSVC `/FI` hack、父仓 gitmodules 条目全部移除
- [x] deps AMF SDK 头升级至 1.4.30（含 `VideoEncoderAV1.h`），configure 经 deps 供给，不再新建 `.target/_amf-sdk`
- [x] `texture-amf` 与 `obs-amf-test` 构建成功并被复制进 rundir
- [x] 3060（非 AMD 机）：`obs-amf-test.exe` 走否定路径（输出 error 段并正常退出），编码器零注册
- [x] 三个 texture 编码器的 `get_defaults` 符合 OBS-PT 调优表（CTest `obspt-amf-model` 单元覆盖：CQP、speed 预设、B 帧关闭、keyint 2s）
- [x] AMF 能力表/QVBR QP 换算（`qp/4`）用例进入 CTest 且全绿（常规、preset/RC 边界、NULL、超界、负数）
- [x] NSIS 脚本含显式删除 `enc-amf.dll` 与 data 目录的 `CleanupLegacyResidue` 条目
- [x] AMD 全系标注 Runtime Acceptance Pending，验收矩阵留募社区真机入口
- [x] H264 全链路回归不受替换影响（NVENC 模块冒烟与全量构建零错误）

## Blocked by

- `055-hevc-av1-container-and-codec-helpers.md`

## Implementation notes

- 移植基线上游 30.2 `texture-amf`/`obs-amf-test`；OBS-PT 27.2.4 libobs 无 `ConfigFile::OpenString`、`obs_encoder_gpu_scaling_enabled`、`obs_p010_tex_active`、`VIDEO_CS_2100_*`、`OBS_ENCODER_CAP_ROI`、`obs_encoder_enum_roi`/hdr metadata API——全部按本机 8-bit SDR 边界裁剪；旧 C 头 `pipe.h` 与 `opts-parser.h` 补上 extern "C" 守卫以便 C++ TU 使用。
- OBS-PT 调优在新文件 `plugins/obs-ffmpeg/obspt-amf-model.{h,c}`（与 `obspt-nvenc-model` 同构）：H264/HEVC/AV1 的 defaults 表（speed 预设、CQP、bf=0、keyint=2s）。texture-amf 的 defaults 注册点改为走模型表。QVBR 下 AV1 `qp/4` 的质量换算进模型并可测。
- `obs-amf-test.exe` 按其接口运行（3060 无 AMD）：输出 `[error] string=Failed to load AMF lib` 并退出 0——上游语义为「很可能无 AMD」，由 `amf_load` 静默捕获，不注册编码器。
- 删除旧 `plugins/enc-amf` 工作树与 `.gitmodules` 段、剥离 CMake staging 桥与 `/FI` hack；MSVC 兼容头 `enc-amf-msvc-compat.hpp` 一并删除。
- NSIS：`CleanupLegacyResidue` 加 `Delete $INSTDIR\obs-plugins\enc-amf.dll` 与 `RMDir /r $INSTDIR\data\obs-plugins\enc-amf`。
- Locale：上游 29.1 的 `AMF.Preset.*`/`AMFOpts*` 并入 en-US/zh-CN/zh-TW/ja-JP/ko-KR；ja-JP 上游缺 4 个 preset 键（Crowdin 现状），打回退英文——与上游装载约定一致。
- 验证证据：
  - `.target/Testing/obspt-encoder/issue057.log`（CTest 349 PASS / 0 FAIL）
  - `.target/Testing/obspt-encoder/module-smoke-057.log`（`legacy amd_amf_h264 absent`、`texture-amf encoders absent without AMD hardware`、ALL PASSED）
  - `obs-amf-test.exe` 直接运行：退出码 0，`[error] string=Failed to load AMF lib`
- 待真机验收（Runtime Acceptance Pending — 写入 1.2.0 发布说明）：
  1. RX 6000/7000 机上 `h264_texture_amf` 常规录制 10–30s 并停启无 error；
  2. `h265_texture_amf` 录制文件导入 Vegas 22；
  3. 有 RX 7000 时 `av1_texture_amf`（RDNA3 必不可少）录制→Vegas；
  4. 任一台旧 AMF 用户装迭代：检查旧 `enc-amf.dll` 被 NSIS 已删除。
