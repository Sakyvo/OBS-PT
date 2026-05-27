# Step 1: 实施清单

> 配套 [`prd.md`](./prd.md) 与 [`design.md`](./design.md)。按顺序执行；每步含**改动文件**、**验证命令**、**review gate**、**rollback point**。

---

## 执行总图

```
S1 M1 路径解析      ─┐
S2 M2 写探测        ─┤
S3 M3 编码器探测    ─┤── 各自独立、可并行实现；S5 依赖 S1+S2+S3
S4 M5 日志清理      ─┘
S5 M4 首启 + M6/M7 弹窗
S6 CMake 预置发行包资源
S7 单元测试 + 集成测试
S8 端到端手工验证 → 走 AC1-AC10
```

S1 / S2 / S3 / S4 实现侧独立，但代码合入顺序仍按编号（M1 是被依赖最广的，最先合）。

---

## S1 — M1 Portable Path Resolver

**目标**：把上游 `os_get_config_path` 替换为转调 `get_portable_path`，砍掉 `BASE_PATH "/config"` 中间层。

### 改动文件
- `libobs/util/platform-windows.c` — 新增 `get_portable_path` + 改 `os_get_config_path` / `os_get_config_path_ptr`
- `libobs/util/platform.h` — 暴露 `get_portable_path` 声明
- `UI/obs-app.cpp` — 删 `CONFIG_PATH` 宏 (#L748)；保留 `GetConfigPath` / `GetConfigPathPtr` 的现有签名，内部转调 `get_portable_path`
- `UI/obs-app.cpp` — `portable_mode` 全局变量改为 `static const bool portable_mode = true`（保留供老代码引用，但删除 setter / CLI 解析）

### 移除项
- `UI/obs-app.cpp` 中 `--portable` argv 解析
- `UI/obs-app.cpp::AppInit` 中 `portable_mode.txt` / `portable_mode` marker 文件检测（约 #L481-503）
- 整段 `OBS_UNIX_STRUCTURE` `#if`/`#endif` 分支

### 验证
```bash
# 编译通过
cd K:/Projects/dev/OBS-Redux && cmake --build build64 --target obs --target libobs 2>&1 | tail -20
# 单测（S7 一起跑）
```

### Review gate
- 全局 grep 确认 `BASE_PATH "/config"` 已无残留：
  ```bash
  grep -rn 'BASE_PATH "/config"' UI/ libobs/
  ```
- 全局 grep 确认 `portable_mode = ` 仅剩声明：
  ```bash
  grep -rn 'portable_mode\s*=' UI/ libobs/
  ```

### Rollback
git revert 本 step commit；M2/M3/M4/M5 都依赖 M1，rollback 后整任务回退到 planning。

---

## S2 — M2 Install Root Write-Probe

**目标**：启动入口最早期做写探测，失败弹 M6 弹窗并 `exit(1)`。

### 改动文件
- 新增 `UI/obsredux-bootstrap.h` — 声明 `probe_install_root_writable` + `write_probe_result_t`
- 新增 `UI/obsredux-bootstrap.cpp` — win32 实现
- `UI/obs-app.cpp::WinMain` 或 `OBSApp::AppInit` 早期 — 插入调用：
  ```cpp
  auto result = probe_install_root_writable(GetInstallRoot().c_str());
  if (result != WRITE_PROBE_OK) {
      ShowWritePermissionFailureDialog(result);  // M6
      return 1;
  }
  ```
- `UI/CMakeLists.txt` — 把 `obsredux-bootstrap.{h,cpp}` 加进源文件列表

### 验证
```bash
# 编译通过
cmake --build build64 --target obs
# 手工：在 C:/Program Files/OBSRedux/ 启动 → 应弹窗后退出
# 手工：在 D:/OBSRedux/ 启动 → 不弹窗，正常进
```

### Review gate
- 确认 M2 的调用点在**任何**配置文件读写之前（用 grep `os_get_config_path` 在 `WinMain` 之后的第一次出现位置交叉验证）。
- 确认弹窗后 `exit(1)` 路径不会执行任何后续清理（避免触发副作用）。

### Rollback
单独 revert 本 commit。M1 不受影响，依然便携。仅失去启动期写权限保护，回归"上游裸跑遇到 ACL 拒绝时报隐晦错误"的旧行为。

---

## S3 — M3 Encoder Capability Probe

**目标**：枚举可用 H.264 编码器，按优先级选第一个。

### 改动文件
- `UI/obsredux-bootstrap.h` / `.cpp` — 新增 `probe_record_encoder` + `encoder_probe_result_t` + `kEncoderPriority` 常量数组

### 注意
- 必须在 `obs_load_all_modules()` 之后调用（这之前编码器插件未注册）。
- AMF 实际 encoder ID 需要校对 `plugins/enc-amf/Source/` 的注册代码，可能是 `amd_amf_h264` 或 `amf_h264_simple`。
- 仅 H.264，HEVC 完全跳过。

### 验证
```bash
# 单测：mock obs_get_encoder_caps，覆盖三种硬件组合
# 手工：N 卡机器跑应选 jim_nvenc；卸载 NVENC 驱动跑应选下一档
```

### Review gate
- 确认 `kEncoderPriority` 数组顺序与 ADR-0002（NVENC > AMF > QSV > x264，无 HEVC）一致。
- 确认软编码兜底时设置了 `is_software_fallback=true`，M7 弹窗能读到。

### Rollback
独立可 revert。回退后 M4 无法获取选中编码器 → M4 应有"M3 不可用时保留 `jim_nvenc` 默认"的兜底，启动不崩但 AMD/Intel 用户体验降级。

---

## S4 — M5 Log Retention Sweeper

**目标**：扫 `logs/*.txt` 与 `crashes/*.log`，删除 mtime > 30 天的文件。

### 改动文件
- `UI/obsredux-bootstrap.h` / `.cpp` — 新增 `sweep_retention` + `sweep_deps_t`
- `UI/obs-app.cpp` 启动早期（M2 之后、M1 路径解析之后）插入两个调用：
  ```cpp
  sweep_retention(GetPortablePath("logs").c_str(), 30, "*.txt", nullptr);
  sweep_retention(GetPortablePath("crashes").c_str(), 30, "*.log", nullptr);
  ```

### 注意
- 目录不存在视作成功（首次启动 logs/ 还没生成）。
- 失败不阻塞启动，只 log 警告。
- `os_glob` / `os_unlink` 在 `libobs/util/platform-windows.c` 已存在，直接复用。

### 验证
```bash
# 单测：mock fs + mock clock，覆盖 35/29/10 天三档
# 手工：放一个 SetFileTime 到 40 天前的 fake log，启动后确认被删
```

### Review gate
- 确认默认 `deps==NULL` 时走的是 `os_glob` + `os_unlink` 而不是某个仍是 stub 的函数。

### Rollback
独立可 revert。回退后日志/崩溃 dump 无限累积，但不影响其他功能。

---

## S5 — M4 First-Run Bootstrap + M6/M7 Qt 弹窗

**目标**：首次启动序列：读 Global Config 判定首启 → 调 M3 → 改写 Profile → 弹 M7 → 写 FirstRunCompleted。

### 改动文件
- `UI/obsredux-bootstrap.h` / `.cpp` —
  - `bool is_first_run()`
  - `void apply_encoder_to_profile(const char *profile_name, const char *encoder_id)`
  - `void apply_record_path_to_profile(const char *profile_name, const char *abs_path)`
  - `void mark_first_run_completed()`
  - `void ShowWritePermissionFailureDialog(write_probe_result_t reason)` — M6
  - `void ShowFirstRunRecommendationsDialog(bool is_software_encoder)` — M7
- `UI/obs-app.cpp::AppInit` 末尾、Qt main loop 启动后：
  ```cpp
  if (is_first_run()) {
      auto enc = probe_record_encoder();
      apply_encoder_to_profile("PotPvP", enc.encoder_id);
      apply_record_path_to_profile("PotPvP",
          (GetInstallRoot() / "recordings").string().c_str());
      ShowFirstRunRecommendationsDialog(enc.is_software_fallback);
      mark_first_run_completed();
  }
  ```
- `UI/data/locale/en-US.ini` 与 `zh-CN.ini` — 新增 M6 / M7 文案条目（其余三语言留占位）

### 文案规范
M6 / M7 文案见 [`prd.md`](./prd.md) 的 "M6 / M7" 段，**逐字照抄**，不要在实施时改文案（任何文案调整都应回到 PRD 改）。

### 验证
```bash
# 集成测试：构造空 User Data Root，调 AppInit 子流程，验证文件改写
# 手工：删 global.ini 中 [OBSRedux] 段 → 重启应弹 M7
# 手工：第二次启动应不弹
```

### Review gate
- 确认 M4 改写 `recordEncoder.json` 时**保留**其他字段（CQP/preset/tune/bf 等），只动 `encoder`。
- 确认 M4 改写 `basic.ini::RecFilePath` 时使用 win32 反斜杠（`D:\OBSRedux\recordings`）。
- 确认 `[OBSRedux] FirstRunCompleted=true` 写在 Global Config 而不是 Profile 内。

### Rollback
依赖 S1/S2/S3。仅 revert S5 → 首启逻辑不跑，但所有路径 / 写探测仍在；用户体验回退到"必须自己改 RecFilePath"。

---

## S6 — CMake 预置发行包资源

**目标**：发行包解压即包含 PotPvP Profile / Scene Collection / Global Config / recordings 占位目录。

### 改动文件
- `cmake/Modules/ObsHelpers.cmake` 或 `UI/CMakeLists.txt` 的 install 段
- 新增源文件目录 `UI/data/obsredux-defaults/`：
  - `obs-studio/basic/profiles/PotPvP/basic.ini`（从 `C:/Users/ASUS/Desktop/obscfg/basic.ini` 移植 + 改 `FPSNum=360`）
  - `obs-studio/basic/profiles/PotPvP/recordEncoder.json`（移植 + 改 `cqp=25`）
  - `obs-studio/basic/profiles/PotPvP/streamEncoder.json`（移植，空对象）
  - `obs-studio/basic/scenes/PotPvP.json`（新建：单 Scene + Game Capture 来源匹配 `Minecraft|Lunar Client|Badlion Client`）
  - `obs-studio/global.ini`（新建：`Language=en-US`、`[Basic] SceneCollection=PotPvP`、`[Basic] Profile=PotPvP`）
  - `obs-studio/recordings/README-recordings.txt`（占位）

### CMake 片段示意
```cmake
install(DIRECTORY UI/data/obsredux-defaults/
        DESTINATION ${OBS_DATA_DESTINATION}/../  # 落到 Install Root 同级
        FILES_MATCHING PATTERN "*")
```

### 验证
```bash
cmake --install build64 --prefix dist-test
# 检查 dist-test/obs-studio/basic/profiles/PotPvP/ 是否存在所有文件
# 检查 dist-test/obs-studio/recordings/ 是否存在
```

### Review gate
- 确认 `basic.ini` 的 `RecFilePath` 是占位（如 `RecFilePath=./recordings`），M4 会运行时改写。
- 确认 `recordEncoder.json` 的 `encoder` 字段为 `jim_nvenc`，M4 会按机器探测改写。
- 确认 `global.ini` 的 `[OBSRedux] FirstRunCompleted` **不存在**或为 `false`（保证首启会触发 M4）。

### Rollback
独立 revert。回退后用户解压发行包后首启会被上游 `InitBasicConfigDefaults` 创建空 Profile，需要手动配。

---

## S7 — 单元测试 + 集成测试

### 改动文件
- `test/ostests/test-portable-path.cpp` — M1 单测（GTest）
- `test/ostests/test-write-probe.cpp` — M2 单测
- `test/ostests/test-encoder-probe.cpp` — M3 单测
- `test/ostests/test-log-sweeper.cpp` — M5 单测
- `test/ostests/test-first-run-bootstrap.cpp` — M4 集成测试

### 测试矩阵
见 [`prd.md`](./prd.md) "Testing Decisions" 表。

### 验证
```bash
cmake --build build64 --target ostests
ctest --test-dir build64 -R "portable|write_probe|encoder_probe|log_sweeper|first_run" --output-on-failure
```

### Review gate
- 全部 5 个测试文件 PASS。
- 覆盖 PRD 测试矩阵中所有"必做"用例。
- M6/M7 UI 测试明确跳过（不在矩阵内）。

---

## S8 — 端到端手工验证（AC1-AC10）

按 [`prd.md`](./prd.md) Acceptance Criteria 逐条手测：

| AC | 验证步骤 |
|---|---|
| AC1 | 解压发行包到 `D:\OBSRedux\` 双击启动 → 直接进主界面 |
| AC2 | 启动后检查 `D:\OBSRedux\obs-studio\` 目录结构完整 |
| AC3 | 检查 `%APPDATA%\obs-studio\` 不存在或未被本次启动修改 |
| AC4 | 复制到 `C:\Program Files\OBSRedux\` 启动 → 弹 M6 后退出 |
| AC5 | 删 `global.ini` 中 `[OBSRedux]` 段后启动 → 弹 M7 |
| AC6 | 第二次启动 → 不弹 M7 |
| AC7 | N 卡 / A 卡 / Intel iGPU 三台机器分别启动 → `recordEncoder.json::encoder` 分别为 jim_nvenc / amd_amf_h264 / obs_qsv11 |
| AC8 | 按 PageDown 录一段 → 录像出现在 `<Install Root>\recordings\` |
| AC9 | 手工 `SetFileTime` 一个 35 天前的 `logs/fake.txt` → 启动后被删；29 天前的保留 |
| AC10 | `ctest` 全部 PASS（S7 已验证） |

### Review gate
- 全 10 项 AC 通过。
- 录一份简短的"AC 验证截图/日志"附在最终 commit message 里。

### Rollback
若任意 AC 失败，回到对应 S1-S7 step 修复后重测。

---

## 总 commit 顺序

```
commit 1: [Step 1 / S1] M1 portable path resolver
commit 2: [Step 1 / S2] M2 install root write-probe + M6 dialog stub
commit 3: [Step 1 / S3] M3 encoder capability probe
commit 4: [Step 1 / S4] M5 log retention sweeper
commit 5: [Step 1 / S5] M4 first-run bootstrap + M7 dialog
commit 6: [Step 1 / S6] CMake install PotPvP defaults
commit 7: [Step 1 / S7] Unit tests for M1/M2/M3/M5 + integration test for M4
```

每个 commit 自身能编译过、相关测试 PASS，便于后续 bisect。
