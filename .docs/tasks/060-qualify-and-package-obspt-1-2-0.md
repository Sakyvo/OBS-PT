Status: review

## Parent

`.docs/prd/2026-09-07-modern-hardware-encoders.md`

## What to build

1.2.0「现代硬件编码器」批次的资格验收与打包：按 AGENTS 常驻法则完成完整发布链——`BUILD_TESTS` 关闭后重配置、构建、staging 刷新、确认无 makensis 进程后单进程运行 NSIS 并等待正常退出、`7z t` 完整性测试、SHA-256、实际启动安装包到 `OBS-PT 1.2.0 Setup` 主窗口；发布说明按三态验收矩阵逐项标注（3060 已真机验收 / 已构建待验收 Runtime Acceptance Pending / 不适用），并附社区 AMD/40 系远程验收的招募口径。此前各切片的运行时烟测（3060 HEVC 全链路、升级烟测、Bilibili 警告演示）在此汇总复核，未清项按 pending 落入验收矩阵而非阻塞发布。

## Acceptance criteria

- [x] 测试插件未进入 staging（`BUILD_TESTS=OFF` 重配置断言），staged DLL 与构建输出哈希一致
- [x] 打包前确认无 `makensis.exe` 进程；NSIS 单进程运行并正常退出
- [x] 安装包时间戳更新；`7z t` 通过；SHA-256 记录
- [x] 包内容断言：`enc-amf.dll` 与其 data 目录不在包内；`texture-amf`（obs-ffmpeg.dll 内嵌）、`obs-nvenc-test.exe`、`obs-amf-test.exe` 在包内
- [x] 安装包 1.2.0 版本号正确（NSIS `APP_VERSION` 更新）
- [x] NSIS `SOURCE_DIR` 修正为 `.target/_pkg/OBS-PT`（原指向废弃的 `.target/app`）
- [x] 480/1 高帧契约复核：默认 Profile 帧率与分辨率未被任何本批次改动改写
- [ ] 实际启动出现 `OBS-PT 1.2.0 Setup` 主窗口（人工——前台操作）
- [ ] 升级安装烟测（当前代 User Data Root）：无重置页、用户设置原样、旧 `enc-amf.dll` 被删除（人工）
- [ ] 3060 烟测矩阵汇总：H264 回归、NVENC HEVC 录制→Vegas 22 导入、HEVC 串流编码器 + Bilibili 警告、Replay Buffer 回归（人工）
- [ ] 发布说明含三态验收矩阵（待人工撰写并发布）

## Blocked by

- `055-hevc-av1-container-and-codec-helpers.md`
- `056-nvenc-hevc-av1-encoders.md`
- `057-amd-texture-amf-replacement.md`
- `058-streaming-hevc-av1-ertmp.md`
- `059-declared-repair-v4-encoder-migration.md`

## Implementation notes

- `BUILD_TESTS=OFF` 重配置（`OBS_VERSION_OVERRIDE:STRING=1.2.0`），全量构建零错误，无测试目标生成。
- `cmake --install` 刷新 staging；`rm -rf .target/_pkg/OBS-PT` 后重新 install 以清旧残留（首次刷新残留了 enc-amf data 目录）。
- NSIS `SOURCE_DIR` 从 `.target/app`（旧路径，含 1.1.0 enc-amf 残留）修正为 `.target/_pkg/OBS-PT`。
- `APP_VERSION` 从 1.1.0 更新至 1.2.0。
- 打包验证结果：
  - `7z t`：1507 文件，Everything Ok
  - SHA-256: `b341b1f36437451b823c819a03da79d1db7aa0959afd6b08e81b8297eab87bc7`
  - enc-amf.dll + data 目录：不在包内 ✓
  - obs-nvenc-test.exe + obs-amf-test.exe：在 bin/ 内 ✓
  - 安装包大小：44.7 MB（压缩后）/ 220.5 MB（解压后）
- NSIS 修复后需注意：`install_obs_core(obs-amf-test)` 已加入 CMakeLists（原仅 POST_BUILD copy），下次构建会自动进 staging。
- 人工验收项（前台操作）：
  1. 双击安装包 → `OBS-PT 1.2.0 Setup` 主窗口出现
  2. 升级安装 → 无重置页、User Data Root 原样、旧 enc-amf.dll 删除
  3. 3060 真机烟测矩阵（见 056/057/058 各自的待真机验收项）
  4. 发布说明撰写与发布
- 验收矩阵三态标注（发布说明口径）：
  - **3060 已真机验收**：NVENC H264 回归（构建+模块冒烟确认注册无回归）
  - **3060 已自动验证**：libobs 服务协商 API、FLV Enhanced-RTMP tag 字节级、bootstrap 迁移模型、hybrid MP4 HEVC/AV1 回环、shipped services.json codec 声明
  - **Runtime Acceptance Pending**：NVENC HEVC 真实编码→Vegas 22 导入、NVENC AV1（3060 无 AV1 编码硬件）、AMD texture-amf 全系（无 AMD 卡）、QSV/Arc（推迟 1.3.0）、Enhanced-RTMP 真实推送（需 YouTube 账号）
