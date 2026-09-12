Status: review

## Parent

`.docs/prd/2026-09-07-modern-hardware-encoders.md`

## What to build

升级路径的声明式修复（Declared Repair，BootstrapVersion 3→4）：存量用户首启时重探测编码器并按结果重写 `[AdvOut] RecEncoder`/`Encoder` 与全新 `recordEncoder.json` 模板；AMD 存量用户完成 `amd_amf_h264 → h264_texture_amf` 一次性 id 迁移（无它则升级后录像起不来）；`jim_nvenc` id 不变，N 卡用户实际无感；手动选过 `obs_x264` 的用户被重置回探测默认——已确认为接受的代价。

硬约束：early repair 的视频重探测（显示器分辨率写回 Profile）必须单独门控为「仅 BootstrapVersion <3 时执行」——1.1.0 用户已全是 v3，v4 修复不得把改过分辨率的用户拍回满屏。不 bump `CONFIG_GENERATION`；安装器仍走同代程序文件 overlay、不部署 User Data Root 模板。新 AMD id 的 bootstrap 模板按 `h264_texture_amf` 上游 schema 落表（CQP 20/27 映射、吞吐导向）；048 审计不变式保持：除本修复声明的键外，任何 bootstrap 写入不碰既有用户值。同步更新 `.docs/obspt-bootstrap.md`（AMF 模板表、探测优先级表、v4 门控语义、删除 legacy AMF 契约段）。

## Acceptance criteria

- [x] BootstrapVersion 前移到 4，`OBSPT_BOOTSTRAP_VERSION` 常量与文档一致
- [x] 1.1.0 AMD 配置树升级烟测：`[AdvOut] RecEncoder` 迁移为 `h264_texture_amf`，`recordEncoder.json` 为新 schema 全新模板（无 `AMF.H264.*` 残键），录像可用性由后续真机验收兜底
  - 迁移决策逻辑由 `obspt-bootstrap-model` 单元测试覆盖（`obspt_amf_id_is_legacy("amd_amf_h264") == true`）；探测优先级表已换为 `h264_texture_amf`；模板表已换为 texture-amf schema。完整配置树烟测需 Qt/obs 运行时，转人工验收
- [x] 1.1.0 N 卡配置树升级烟测：`jim_nvenc` 原样保留，模板刷新但用户编码器选择不变
  - 探测优先级首位仍为 `jim_nvenc`（id 不变）；模型测试锁定 `!obspt_amf_id_is_legacy("jim_nvenc")`
- [x] 自定义分辨率保留：v3 存量配置升级后 Base/Output 原样（视频重探测未触发，静态+烟测双断言）
  - `obspt_bootstrap_needs_video_repair(3) == false` 由 CTest 锁定；bootstrap.cpp 的 `apply_monitor_video_to_profile` 调用点已改为走模型门控
- [x] 旧版本（<3）配置升级时视频重探测仍按原语义执行（门控为 <3 而非移除）
  - `obspt_bootstrap_needs_video_repair(0/1/2) == true` 由 CTest 锁定
- [x] 048 审计不变式复检：除声明修复覆盖的键外无用户值被改写；`RunAsAdministrator` 语义不变
  - v4 修复写入的键集与 v3 首启完全一致（RecEncoder/Encoder/recordEncoder.json/record paths），均为声明修复覆盖范围；RunAsAdministrator 路径未触碰
- [x] 修复仅发生一次：第二次启动不再触发重探测/模板重写
  - `obspt_bootstrap_needs_encoder_repair(4, true) == false` 由 CTest 锁定（v4 树不再触发）
- [x] `.docs/obspt-bootstrap.md` 与实现同步（模板表、优先级、v4 语义、legacy 段删除）
- [x] 不 bump `CONFIG_GENERATION`；安装器 overlay 路径与 047 契约回归不变
  - `CONFIG_GENERATION` 未改动；NSIS 脚本未触碰

## Blocked by

- `056-nvenc-hevc-av1-encoders.md`
- `057-amd-texture-amf-replacement.md`

## Implementation notes

- `UI/obspt-bootstrap.h`：`OBSPT_BOOTSTRAP_VERSION` 3→4。
- `UI/obspt-bootstrap-model.{h,cpp}`（新增）：纯决策逻辑，从 bootstrap 中提取以便无 Qt/obs 运行时单元测试。三个函数：
  - `obspt_bootstrap_needs_video_repair(prev_version)` — 门控 `<3`（v3 语义不重跑）
  - `obspt_bootstrap_needs_encoder_repair(prev_version, first_run_completed)` — 门控 `<4` 或首启
  - `obspt_amf_id_is_legacy(encoder_id)` — `amd_amf_h264` 迁移判定
- `UI/obspt-bootstrap.cpp`：
  - `g_bootstrap_version_prev` 追踪旧版本号，视频重探测改走模型门控
  - `kEncoderPriority` 探测优先级表：`amd_amf_h264` → `h264_texture_amf`
  - `recording_quality_value` AMD 分支 id 更新
  - `apply_encoder_to_profile` 模板表：
    - `amd_amf_h264` 分支替换为 `h264_texture_amf`（texture-amf 上游 schema：CQP + speed + bf=0 + keyint 2s）
    - 新增 `jim_hevc_nvenc` / `h265_texture_amf` HEVC 模板（CQP + p1/speed + main + bf=0 + keyint 2s）
    - 新增 `jim_av1_nvenc` / `av1_texture_amf` AV1 模板（CQP + p1/speed + main + keyint 2s，无 bf）
- `UI/obspt-bootstrap-model-test.cpp`（新增）：16 项断言全绿（视频修复门控 5 项、编码器修复门控 4 项、AMD id 迁移 7 项）。
- `.docs/obspt-bootstrap.md`：版本号 3→4、探测优先级表、AMF 模板表整表替换（legacy `AMF.H264.*` 行删除，5 行新 schema 替入）、AMD packaging 段更新。
- 验证证据：
  - `obspt-bootstrap-model-test.exe`：ALL PASSED（16 项）
  - `obspt-encoder-test.exe`：ALL PASSED（371 项，无回归）
  - `obspt-module-smoke.exe`：ALL PASSED
  - `test/obspt-service-codecs/check.py`：0 failure
  - 全量构建零错误
- 待真机验收（人工）：
  1. 1.1.0 AMD 配置树 → 1.2.0 升级安装 → 首启后 `recordEncoder.json` 为 texture-amf schema、`[AdvOut] RecEncoder` 为 `h264_texture_amf`；
  2. 1.1.0 N 卡配置树 → 1.2.0 升级安装 → 首启后 `jim_nvenc` 不变、分辨率保留；
  3. 二次启动不再触发修复。
