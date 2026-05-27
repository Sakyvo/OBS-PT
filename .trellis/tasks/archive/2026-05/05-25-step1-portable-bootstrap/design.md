# Step 1: 技术设计

> 与 [`prd.md`](./prd.md) 配套：PRD 说"要什么"，本文说"怎么切分、模块间长什么样、为什么这么切"。术语沿用 [`CONTEXT.md`](../../../CONTEXT.md)。

---

## 模块依赖图

```
                            ┌─────────────────────────────┐
                            │  CMake install (预置发行包)  │
                            └──────────────┬──────────────┘
                                           ↓ 物理文件落到 <Install Root>/obs-studio/...
┌──────────────────────────────────────────────────────────────────┐
│ 进程启动入口（UI/obs-app.cpp::main → OBSApp::AppInit）            │
└──┬───────────────────────────────────────────────────────────────┘
   │
   ↓ 启动最早期，UI 起来之前
   ├─→ [M2] Install Root Write-Probe ───── 失败 ──→ [M6] 写权限失败弹窗 → exit(1)
   │       │
   │       ↓ 成功
   ├─→ [M5] Log Retention Sweeper（与 M2 同阶段，独立可跑）
   │
   ├─→ [M1] Portable Path Resolver（被以下所有模块复用）
   │       ↑
   │       └── libobs/util/platform-windows.c::os_get_config_path 内部转调
   │            （让上游 UI + 所有插件 SDK 透明走同一路径）
   │
   ↓ 进入 OBS 主流程：global.ini 加载、Profile 加载
   │
   ↓ 判定 FirstRunCompleted=false
   ├─→ [M4] First-Run Bootstrap
   │       ├─→ 调 [M3] Encoder Capability Probe（一次性、结果固化）
   │       ├─→ 改写 recordEncoder.json::encoder
   │       ├─→ 改写 basic.ini::RecFilePath / FilePath 为运行时绝对路径
   │       ├─→ 调 [M7] 首次启动确认弹窗
   │       └─→ 写 [OBSRedux] FirstRunCompleted=true
   │
   ↓ 进入 OBS Qt 主窗口
```

---

## 接口契约

### M1 — Portable Path Resolver

```c
/* 新增 API，替换 BASE_PATH "/config" + os_get_config_path 的旧链路 */
int get_portable_path(char *dst, size_t size, const char *name);
/* 语义：纯 passthrough 拼接 <Install Root>/<name>，不自动注入 "obs-studio/" 前缀。
   上游所有 caller 已经传入含 "obs-studio/..." 前缀的 name（如 "obs-studio/global.ini"、
   "obs-studio/basic/profiles"），passthrough 实现可零修改复用全部 20+ 调用点。
   返回：写入字符数（不含 \0）；缓冲区不足返回 -1。
   name=NULL 或 ""：dst = "<Install Root>"（裸根，给 XDG 兼容代码用）
   name="obs-studio"：dst = "<Install Root>/obs-studio"（即 User Data Root）
   name="obs-studio/global.ini"：dst = "<Install Root>/obs-studio/global.ini"
   Redux 新代码（M4 等）需显式传含 "obs-studio/" 前缀的 name；不依赖隐式注入。 */

char *get_portable_path_ptr(const char *name);  /* malloc 版本 */
```

**实现位置**：`libobs/util/platform-windows.c`（让插件 SDK 透明走同路径）+ 暴露在 `libobs/util/platform.h`。

**`os_get_config_path` 适配**：

```c
int os_get_config_path(char *dst, size_t size, const char *name)
{
    /* OBSRedux: forced portable, ignore XDG_CONFIG_HOME / %APPDATA% */
    return get_portable_path(dst, size, name);
}
char *os_get_config_path_ptr(const char *name) { /* 同上，malloc 返回 */ }
```

### M2 — Install Root Write-Probe

```c
typedef enum {
    WRITE_PROBE_OK = 0,
    WRITE_PROBE_SYSTEM_PROTECTED,   /* GetLastError() == ERROR_ACCESS_DENIED 且路径在 Program Files / Windows / System32 */
    WRITE_PROBE_READ_ONLY_VOLUME,   /* ERROR_WRITE_PROTECT */
    WRITE_PROBE_ANTIVIRUS_BLOCKED,  /* ERROR_VIRUS_INFECTED / ERROR_VIRUS_DELETED */
    WRITE_PROBE_UNKNOWN,
} write_probe_result_t;

write_probe_result_t probe_install_root_writable(const char *install_root);
```

**实现**：尝试 `CreateFileW(<install_root>/.write_probe_<uuid>, GENERIC_WRITE, ...)` + 立即 `DeleteFileW`，按 `GetLastError` 分流。

### M3 — Encoder Capability Probe

```c
/* 优先级硬编码，从高到低 */
static const char *kEncoderPriority[] = {
    "jim_nvenc",      /* NVIDIA NVENC（推荐） */
    "obs_qsv11",      /* Intel QSV */
    "ffmpeg_nvenc",   /* 旧版 NVENC fallback */
    "amd_amf_h264",   /* AMD AMF（实际 ID 见 plugins/enc-amf） */
    "obs_x264",       /* 软编码兜底 */
};

typedef struct {
    const char *encoder_id;     /* 选中的 ID，保证非 NULL */
    bool is_software_fallback;  /* true 表示落到 obs_x264，UI 应警示 */
} encoder_probe_result_t;

encoder_probe_result_t probe_record_encoder(void);
```

**实现**：遍历 `kEncoderPriority`，对每个 ID 调 `obs_get_encoder_caps`（或同等 API）判断是否注册且非 deprecated。

### M4 — First-Run Bootstrap

无独立 C API（在 `UI/obsredux-bootstrap.cpp` 内部使用），关键步骤：

1. `is_first_run()`：读 `<User Data Root>/global.ini` 的 `[OBSRedux] FirstRunCompleted`，缺失或不为 `true` 视作首次。
2. `apply_encoder_to_profile(encoder_id)`：JSON 解析 `<User Data Root>/basic/profiles/PotPvP/recordEncoder.json`，改写 `encoder` 字段（若与预置的 `jim_nvenc` 不同），保留其他字段。
3. `apply_record_path_to_profile(abs_path)`：INI 解析 `<User Data Root>/basic/profiles/PotPvP/basic.ini`，改写 `[AdvOut] RecFilePath` 与 `[SimpleOutput] FilePath`。
4. `mark_first_run_completed()`：写 `[OBSRedux] FirstRunCompleted=true` 到 Global Config。

### M5 — Log Retention Sweeper

```c
typedef struct {
    /* 注入点：单元测试可替换为 mock，生产用 NULL = 默认实现 */
    int64_t (*get_current_time_sec)(void);
    int (*list_files)(const char *dir, const char *glob, char ***out, size_t *count);
    int (*get_mtime_sec)(const char *path, int64_t *out);
    int (*unlink_file)(const char *path);
} sweep_deps_t;

int sweep_retention(const char *dir, int retention_days,
                    const char *glob, const sweep_deps_t *deps);
/* 返回：删除的文件数；目录不存在视作 0；deps=NULL 用默认 win32 实现 */
```

### M6 / M7 — Qt 弹窗

直接调 `QMessageBox`，无外部 C API。i18n 走上游 `obs-frontend-api` 的翻译机制（`QTStr` / `Str` 宏）。

---

## 数据流：启动序列

```
T0  进程入口 (WinMain)
       │
T1     │  ↓ 解析 argv（保留上游 --multi 等开关，删除 --portable）
       │
T2     │  ↓ 计算 Install Root（基于 GetModuleFileNameW，up 一级到 bin/ 之外）
       │
T3     │  ↓ [M2] probe_install_root_writable
       │       └── FAIL → [M6] QMessageBox::critical → exit(1)
       │
T4     │  ↓ [M5] sweep_retention(logs, 30) + sweep_retention(crashes, 30)
       │       └── 失败不阻塞启动（仅 log 警告）
       │
T5     │  ↓ 上游 OBSApp::AppInit
       │       │
T5.1   │  ↓ [M1] 加载 Global Config（<Install Root>/obs-studio/global.ini）
       │       └── 不存在 → 上游 InitGlobalConfigDefaults 创建（已走 M1）
       │
T5.2   │  ↓ 上游 InitLocale + 加载激活的 Profile / Scene Collection
       │       └── 全部路径走 M1
       │
T6     │  ↓ [M4] is_first_run?
       │       ├── YES：
       │       │     ├── [M3] probe_record_encoder
       │       │     ├── apply_encoder_to_profile
       │       │     ├── apply_record_path_to_profile(<Install Root>/recordings/)
       │       │     ├── （Qt main loop 启动后）[M7] 首次启动弹窗
       │       │     └── mark_first_run_completed
       │       └── NO：跳过
       │
T7     主窗口可见
```

**关键不变量**：
- T3 之前**不允许**任何文件写入（含 logs）。M5 在 T4，因为它先要确认 T3 通过。
- M3 探测在 T6 而非 T2，因为需要 OBS 模块（含编码器插件）已加载。
- M7 弹窗必须在 Qt main loop 之后，但 M4 的文件改写必须在主窗口读 Profile 之前 → 拆为"T6 前置改写 + T7 后弹窗 + 弹窗确认后写 FirstRunCompleted"三段。

---

## 关键 Tradeoffs

### T1：M1 实现层选择 — `libobs/util` vs `UI/`

**选 `libobs/util/platform-windows.c`**。

| 方案 | 优点 | 缺点 |
|---|---|---|
| 放 `libobs/util` | 上游 UI + 所有插件 SDK 自动透明走 portable 路径，零调用点改写 | 修改 libobs 核心，与上游冲突面大 |
| 放 `UI/obsredux-bootstrap` | 改动隔离在 UI 层 | 必须逐个改写 `os_get_config_path` 的 20+ 处调用，且插件 SDK（如 obs-browser 的 cache 目录）漏改会写到 %APPDATA% |

**决策依据**：CONTEXT.md 中 [[Portable Mode]] 是"内建不可关闭的运行前提"，必须保证**所有**写入都在 Install Root 内，零 APPDATA fallback。这是硬约束，不允许漏改风险。

### T2：M1 接口签名 — 复用 `os_get_config_path` 形态

**复用** `int func(char *dst, size_t size, const char *name)` 签名。

理由：上游 20+ 处调用点已经按这个签名调用，复用签名 → 改动量从 20+ 处降到 1 处（函数内部）。新增 `GetPortablePath` C++ 简化版本仅供 Redux 新代码用。

### T3：M3 一次性探测 vs 每次启动重测

**一次性固化**（写入 Profile 文件，后续启动只读不测）。

理由（来自 grill 决策）：
- 用户体验上的"推荐值锁定"——用户改了之后我们不应该再次自动改回去；
- 性能上 NVENC/QSV 注册检测是毫秒级，但 AMF 需要加载 amfrt64.dll，启动期开销可见；
- 用户换卡的情况罕见，且换卡后通常会主动到设置里换。

代价：用户换卡后需要手动改 Profile 或删 `FirstRunCompleted` 键。在 README 写明。

### T4：M5 与 M2 同阶段 vs 异步线程

**同步、同阶段**。

理由：M5 只扫两个目录、删几个文件，typical case 耗时 <100ms。异步引入线程同步复杂度不划算。最坏情况（用户长期不开 OBS 累积上千日志）也只是一次性首启卡顿，可接受。

### T5：JSON / INI 解析库选择

复用上游已有：
- INI：`libobs/util/config-file.c`（OBS 自己的 INI 读写器，已支持读改写）
- JSON：`deps/json11` 或 OBS 已用的 jansson（M4 改写 recordEncoder.json 用）

不引入新依赖。

---

## 兼容性与边界

### 与上游的兼容性

- **保留接口**：`os_get_config_path` 签名与返回值语义不变，仅实现改为转调 M1。第三方插件代码无需修改。
- **删除接口**：CLI 的 `--portable`、`BASE_PATH/portable_mode[.txt]` 标记文件检测。这两个是上游"可选便携"的入口，Redux 内不存在"非便携"形态，删之。
- **删除分支**：`OBS_UNIX_STRUCTURE` 整段（Linux/macOS 包管理路径）。Redux 仅 Windows 目标。

### Profile 文件格式兼容性

预置的 `basic.ini` / `recordEncoder.json` 必须能被上游 OBS 直接读，不引入 Redux 私有字段。M4 在改写时也只动 `encoder` 和 `RecFilePath` / `FilePath` 这几个上游已有字段。

### Global Config 私有命名空间

Redux 自身状态写在 `[OBSRedux]` section，与上游所有 section 隔离。当前键：
- `FirstRunCompleted` (bool)

### 跨平台

Redux 仅 Windows。M1 / M2 / M5 的 win32 实现是唯一实现，不留 Linux/macOS stub。

---

## Rollout / Rollback

### Rollout

无 feature flag（独立分支、PotPvP 定位收窄）。M1-M7 + CMake 改动一次性合入 `main`。

### 用户侧"回滚"

若用户想回上游 OBS 体验：使用上游 OBS 二进制即可，Redux 与上游可并存（不同安装目录）。Redux 不提供"切换到上游模式"的开关。

### Rollback 触发场景

| 场景 | 处理 |
|---|---|
| M2 写探测失败 | M6 弹窗 + `exit(1)`，不进入主流程 |
| M3 全部编码器探测失败 | 选 `obs_x264` + 软编码标志位，M7 弹窗加警示 |
| M4 改写 Profile 失败（JSON/INI 解析错） | log 错误、不写 FirstRunCompleted、保留默认值继续启动（下次启动还会重试） |
| M5 扫描失败（权限/IO） | log 警告，不阻塞启动 |
| 发行包缺预置 Profile | 上游 `InitBasicConfigDefaults` 兜底创建空 Profile，用户体验降级但不崩 |

---

## 与其他 Step 的接口

- Step 5/6（编码器档位精调、HEVC）将**新增** `kEncoderPriority` 条目和参数预设，复用本 Step 的 M3 探测机制。
- 未来 fMP4 / 多轨：仅改预置 `basic.ini` 的 `RecFormat` / `RecTracks` 字段与 CMake install，无需重做 M1-M7。
- 未来 i18n 五语言全量翻译：仅在 M6 / M7 文案条目上追加，无需改逻辑。

---

## 不在设计范围

- 测试用例的具体代码（见 [`prd.md`](./prd.md) 的 Testing Decisions 矩阵）
- 上游每个调用点的逐行 diff（见 [`implement.md`](./implement.md) 的 step 清单）
- Qt 弹窗的 UI 视觉细节（M6/M7 文案见 PRD，控件按 Qt 默认 `QMessageBox` 样式）
