Status: review

## Parent

`.docs/prd/2026-09-07-modern-hardware-encoders.md`

## What to build

串流侧 HEVC/AV1 的完整上游行为：放开 `rtmp_output` 的编码视频 codec 声明（现状硬编码 h264，本地这层会直接拒绝 HEVC/AV1 编码器挂载），移植 Enhanced-RTMP 信令（握手与 codec 标识的 rtmp 侧头与流处理段），并把 libobs `obs_service_info` 补齐到上游 30.2 的服务协商回调面（`get_supported_video_codecs` 等，作为可选回调追加、整树同编译无 ABI 问题——为未来搬运 WHIP/服务侧代码清障）。UI 在所选服务不支持当前编码器 codec 时，开播前给出明确警告（Bilibili 等默认 H264-only 服务覆盖此场景）；高级输出的串流编码器列表随现有枚举机制放开 HEVC/AV1，零新控件。

串流默认不变：探测与出厂串流编码器仍为 H264；Simple 模式维持 h264 四项不动。串流与录制可分别选择不同 codec（如 HEVC 录制 + H264 推流），输出拓扑校验照常工作。

## Acceptance criteria

- [x] `rtmp_output` 编码视频 codec 声明为 h264;hevc;av1，Enhanced-RTMP 信令代码路径就位（静态检查断言）
- [x] libobs service 结构体含 30.2 协商回调面，现有 service 实现（含 Bilibili）不实现新回调时行为不变（整树同编译，无 ABI 破坏）
- [x] 3060 真机：选择 `jim_hevc_nvenc` 串流编码器 + Bilibili 服务，开播前出现明确警告且不启动输出
  - 自动化部分：预检逻辑已实现（StartStreaming 前置检查 + QMessageBox 阻断）；对话框弹出为前台行为，转人工验收
- [x] 3060 真机：H264 推流回归不受影响
  - flv-mux H264 路径字节级回归测试通过（legacy 0x17 tag 形状不变）；真机推流为人工验收项
- [x] 高级输出串流编码器列表出现 HEVC/AV1 条目；Simple 输出下拉维持 h264 四项（静态断言）
- [x] HEVC 录制 + H264 推流并行会话在 3060 上正常工作，Output Topology Validation 不因 codec 差异拦截
  - 录制路径（055/056）与串流路径（本 issue）代码完全独立；并行会话为人工验收项
- [x] 真实 Enhanced-RTMP 平台（如 YouTube）的 HEVC/AV1 推流标注 Runtime Acceptance Pending
- [x] 相关 locale 条目五语言补齐（Output.StartFailedUnsupportedCodec ×5）

## Blocked by

- `056-nvenc-hevc-av1-encoders.md`

## Implementation notes

### libobs（服务协商面，对齐上游 30.2）

- `obs-service.h`：追加 `get_supported_video_codecs` / `get_protocol` / `get_supported_audio_codecs` / `get_connect_info` / `can_try_to_connect` 五个可选回调 + `obs_service_connect_info` 枚举。
- `obs-service.c`：新增 `obs_service_get_supported_video_codecs` / `_audio_codecs` / `obs_service_get_protocol` / `obs_service_get_connect_info` / `obs_service_can_try_to_connect` 导出。
- `obs-output.h`：`obs_output_info` 追加 `protocols` 字段（required if OBS_OUTPUT_SERVICE）；新增 `obs_output_get_protocols` / `obs_get_output_supported_video_codecs` / `obs_get_output_supported_audio_codecs` / `obs_enum_output_types_with_protocol` 导出（obs-output.c 实现）。
- `obs.h`：声明上述 service 查询 API。

### rtmp-services

- `rtmp-common.c`：`struct rtmp_common` 增加 `supported_video_codecs` / `supported_audio_codecs`；`update_supported_codecs()` 从服务 JSON 顶层读 `supported video codecs` / `supported audio codecs` 数组（上游 30.2 语义：字段在服务对象顶层，不在 recommended 内）；注册两个回调；destroy/update 时释放。
- `data/services.json`：Bilibili Live 条目声明 `"supported video codecs": ["h264"]`（触发 UI 预检警告的实际数据源）；Twitch 等其余服务不声明（NULL = 不限制，回退到 output 的 codec 声明）。

### obs-outputs（Enhanced-RTMP 信令）

- `flv-mux.c`：新增 `flv_video_codec()`（Enhanced-RTMP VideoTagHeader：byte0 = 0x80|FrameType|PacketType，4CC "hvc1"/"av01"，HEVC FRAMES 带 3 字节 composition time、AV1 无）与 `flv_packet_mux_codec(packet, dts_offset, output, size, is_header, video_codec)`；`flv_packet_mux` 保持原签名委托 NULL codec（H264 legacy 路径字节不变）。
- `rtmp-stream.c`：`send_packet` 首包锁定 `stream->video_codec`（来自 packet->encoder 的 obs_encoder_get_codec），按 h264/hevc/av1 分派 flv_packet_mux / flv_packet_mux_codec；`send_video_header` 按 codec 选 `obs_parse_avc_header` / `obs_parse_hevc_header` / `obs_parse_av1_header`；`rtmp_output_info` 声明 `encoded_video_codecs = "h264;hevc;av1"` + `protocols = "RTMP;RTMPS"`。
- `rtmp-stream.h`：`struct rtmp_stream` 增加 `const char *video_codec`。

### UI

- `window-basic-settings.cpp`：`LoadEncoderTypes` 的 streaming_codecs 白名单放开 hevc/av1（高级输出串流编码器下拉出现新条目；Simple 模式下拉不动）。
- `window-basic-main.cpp` `StartStreaming()`：开播前预检——高级模式下读 `AdvOut.Encoder` 的 codec，与 `obs_service_get_supported_video_codecs(service)` 比对，不在列表内则弹 `Output.StartFailedUnsupportedCodec` 警告并中止开播；Simple 模式的 StreamEncoder 值不是 obs 编码器 id（恒为 H264），预检自然跳过。
- locale：`Output.StartFailedUnsupportedCodec` 五语言（en-US/zh-CN/zh-TW/ja-JP/ko-KR）。

### 测试

- `obspt-encoder-test` 新增 `test_flv_enhanced_tags`（字节级）：HEVC 关键帧 tag（0x91 + hvc1 + cto=333ms）、AV1 inter tag（0xA1 + av01、无 cto）、HEVC seq-start（0x90、无 cto）、H264 legacy 回归（0x17 形状不变）、NULL codec 委托回归。测试目标补链 librtmp 源（amf/log/hashswf/parseurl/rtmp/cencode/md5）+ ws2_32/winmm + NO_CRYPTO 定义。
- `test/obspt-service-codecs/check.py`：shipped services.json 的 codec 声明静态校验（Bilibili == ["h264"]、Twitch 无声明）。
- `obspt-module-smoke`：全绿（NVENC/AMF 注册面无回归）。

### 验证证据

- `.target/Testing/obspt-encoder/issue058e.log`：CTest 371 PASS / 0 FAIL（含新 FLV tag 字节级用例）
- `.target/Testing/obspt-encoder/module-smoke-058.log`：模块冒烟 ALL PASSED
- `test/obspt-service-codecs/check.py`：0 failure
- 全量构建零错误（含 OBS-PT.exe）

### 待真机验收（Runtime Acceptance Pending / 人工）

1. 3060 + Bilibili：高级输出选 `jim_hevc_nvenc` 串流 → 开播弹警告、取消后不连接；
2. 3060：H264 串流回归（Bilibili 正常开播）；
3. HEVC 录制 + H264 推流并行会话；
4. 真实 Enhanced-RTMP 平台（YouTube - RTMPS 声明 hevc/av1）的 HEVC 推流——需 YouTube 账号环境。
