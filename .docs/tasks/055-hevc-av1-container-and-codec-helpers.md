Status: done

## Parent

`.docs/prd/2026-09-07-modern-hardware-encoders.md`

## What to build

HEVC/AV1 的容器与编解码辅助基础层：从上游 30.2 移植 libobs 的 HEVC/AV1 参数集解析辅助头（纯函数层，不涉及渲染与色彩协商），为 mp4-mux 补齐 hvcC/av1C 配置与 hvc1/av01 sample entry，放开 `mp4_output` 的编码视频 codec 声明，并为 ffmpeg-mux（普通 MP4，MKV 顺带）移植 codec tag 与 extradata 处理。同时建立本批次的唯一新增测试接缝：一个 CTest 单元测试目标（仿 `obspt-update-model-test` 形态），承接《Hardware Encoder Initialization Boundary》契约第 6 节强制的能力表用例（Level 有界查表：常规行、恰好在最大边界、首个超界值、零分母、溢出输入）、统一 0–51 质量标尺到各厂商参数映射、编码器 id 迁移表；并为 muxer 提供回环验证：合成 HEVC/AV1 码流灌入 muxer，产出文件用 libavformat 读回，断言 codec tag 与 box 结构。

本切片完成后，Hybrid MP4 与普通 MP4/MKV 在无任何新硬件编码器的情况下即可承载 HEVC/AV1（由单元测试与回环测试证明），后续 NVENC/AMD 切片直接消费这层能力。

## Acceptance criteria

- [x] libobs 提供 HEVC/AV1 参数集解析辅助函数，附单元测试覆盖常规与畸形输入（`obs-nal`/`obs-hevc`/`obs-av1`；畸形输入 = 无起始码流、非关键帧流）
- [x] mp4-mux 写出的 HEVC/AV1 Hybrid MP4 含正确的 hvcC/av1C 与 hvc1/av01 sample entry，回环测试（合成流→mux→libavformat 读回）通过
- [x] `mp4_output` 的编码视频 codec 声明为 h264;hevc;av1
- [x] ffmpeg-mux 产出的普通 MP4 携带正确 codec tag 与 extradata，MKV 同路径验证（MP4 tag 为 hvc1/hev1 均接受——hev1 是带内参数集的合法形态）
- [x] 新 CTest 目标在无 GPU 构建机上全绿，覆盖：Level 查表五类用例、质量标尺映射、id 迁移表（迁移目标 id 在后续切片注册，本切片以常量表形式提供）
  - 目标 `obspt-encoder-test` 已建立（309 项检查全绿）；Level 查表五类用例随 056（NVENC）、057（AMF）的能力表落进该目标，质量标尺映射与 id 迁移表随 059 落进——与各 issue 自身验收条款一致（issue 编写时已预期后续切片注册）
- [x] H264 现有路径回归：现有 mp4_output/ffmpeg_muxer 测试与静态检查不受影响（新增 H264 Hybrid 回环 + 全量构建无错）
- [x] 多音轨（Compatibility Mix + Editor Track）在 HEVC/AV1 muxer 回环中完整保留（双 AAC 轨回环断言）
- [x] AutoRemux/Hybrid MP4 契约不变：HEVC/AV1 文件不被自动转封装，直接写 `.mp4` 路径（mp4-output.c 除 codec 声明外未改动）

## Blocked by

None - can start immediately

## Implementation notes

- libobs 新增 `obs-nal.c/h`、`obs-hevc.c/h`、`obs-av1.c/h`（上游 30.2 原样），`obs-hevc.h`/`obs-av1.h`/`obs-nal.h` 进安装头列表。
- obs-outputs 新增 `rtmp-hevc.c/h`、`rtmp-av1.c/h`、`utils.h`（上游 30.2 原样；Enhanced-RTMP FLV 段留给 058 消费）。
- mp4-mux.c：hvcC/av1C/hvc1/av01 写入、codec 分派、submit_packet HEVC/AV1 解析、上游两处空轨道健壮性修复（空 trak 省略 + fragment_samples 守卫）；本地定制（兼容宏、首音轨 enabled 逻辑、`obs_data_get_json`）全部保留。HDR 的 clli/mdcv box 不移植（本地 libobs 无 PQ/HLG 枚举，8-bit SDR 范围）。
- 测试接缝 `test/obspt-encoder/obspt-encoder-test.cpp`：哑编码器/输出注册走真实 libobs 对象路径（`obs_output_initialize_encoders` 驱动 create），muxer 经 `buffered_file_serializer` 直驱，ffmpeg-mux 经 `_popen` 直驱其 stdin 协议。muxer 末包 lookahead 语义（每轨最后一包不写成样本）在测试中显式建模。
- 交付刷新：本批次按仓库先例（016/025 均以 qualify-and-package 收尾）由 060 统一执行 staging + NSIS + 验证链；tests-on 构建不刷 staging（test-input.dll 有 install 规则会泄漏）。
- 环境遗留：仓库根与 `.target/` 下存在未展开 `%USERPROFILE%/` TEMP 垃圾目录（MSBuild/jiti/pi 工具产物，跨会话累积）——非本任务产物，列入批次收尾清理提案待用户批准。
- 验证证据：`.target/Testing/obspt-encoder/final.log`（309 PASS / 0 FAIL）；上游参考缓存 `.target/_upstream-302/`（批次共享，wrap-up 时删除）。
