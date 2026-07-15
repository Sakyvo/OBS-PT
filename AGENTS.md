# OBS-PT

## 项目定位

OBS-PT 是基于 OBS Studio 27.2.4 的独立 Qt 5 分支，面向 Minecraft
Java 1.7.10/1.8.9 PotPvP 高帧录制。项目永久采用便携化运行，默认配置以
480 FPS 素材采集和后期帧混合为核心；领域术语与边界见 `CONTEXT.md`。

## 技术栈与结构

- 技术栈：C/C++、Qt 5.15.2、CMake、Visual Studio 2022 v143、NSIS。
- `UI/`：Qt 前端、启动流程、默认配置与 OBS-PT 功能。
- `libobs/`、`libobs-*`：采集、渲染、编码和输出核心。
- `plugins/`：NVENC、AMF、捕获、输出等插件。
- `UI/installer/`：Windows NSIS 安装器及配置迁移脚本。
- `.target/`：本地构建、测试、staging、安装包和运行验证产物。

## 常用命令

- 配置：
  `& "K:/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" -S . -B .target -G "Visual Studio 17 2022" -A x64 -D "DepsPath:PATH=K:/Projects/dev/OBS-PT/deps2019/win64" -D "QTDIR:PATH=C:/Qt/5.15.2/msvc2019_64" -D "OBS_VERSION_OVERRIDE:STRING=1.0.0" -D "AMDAMF_SDKDir:PATH=K:/Projects/dev/OBS-PT/.target/_amf-sdk" -D "BUILD_BROWSER:BOOL=OFF" -D "BUILD_TESTS:BOOL=OFF" -D "BUILD_VST:BOOL=OFF" -D "COPY_DEPENDENCIES:BOOL=ON" -D "DISABLE_UPDATE_MODULE:BOOL=TRUE" -D "ENABLE_QSV11:BOOL=ON" -D "ENABLE_WINMF:BOOL=FALSE"`。
- 构建：
  `& "K:/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build .target --config RelWithDebInfo --parallel`。
- 更新器模型测试：先用 `-D "BUILD_TESTS:BOOL=ON"` 重新配置，再运行
  `& "K:/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build .target --config RelWithDebInfo --target obspt-update-model-test --parallel`。
- 打包前用 `-D "BUILD_TESTS:BOOL=OFF"` 重新配置，避免测试插件进入 staging。
- 刷新 staging：
  `& "K:/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --install .target --config RelWithDebInfo --prefix K:/Projects/dev/OBS-PT/.target/_pkg/OBS-PT`。
- 打包：
  `& "C:/Program Files (x86)/NSIS/makensis.exe" "UI/installer/obspt-setup.nsi"`。

## 常驻法则

- Windows 正式构建只使用 VS2022 Build Tools v143 及其内置 CMake 3；
  不使用 Visual Studio 18 2026 或系统 CMake 4。
- 所有本地构建与验证产物写入 `.target/`；保持
  `K:/Projects/targets/OBS-PT.lnk` 指向该目录。
- 不得通过降低默认 480 FPS、分辨率或录制质量掩盖编码器、捕获或性能问题。
- 不得重新引入 `%APPDATA%` 或上游可选 Portable Mode；User Data Root 恒为
  `<Install Root>/obs-studio/`。
- 修改预置 Profile 或编码器配置时，同步更新 shipped JSON 与
  `apply_encoder_to_profile()` bootstrap 模板，并验证实际选中的编码器。
- 任何影响 EXE 或插件 DLL 的改动在构建验证后必须刷新 staging 和安装包。
  运行 NSIS 前确认没有 `makensis.exe`，只启动一个进程并等待正常退出；随后
  检查安装包时间戳、`7z t`、SHA-256，并实际启动到 Setup 主窗口。

## 按需读取索引

- 涉及领域术语、便携目录或配置层级时，读 `CONTEXT.md`。
- 修改启动、默认 Profile、编码器映射、Hybrid MP4、更新器或安装器时，读
  `.docs/obspt-bootstrap.md`。
- 修改 Windows Display Capture 时，读 `.docs/obsredux-display-capture.md`。
- 修改 MSVC 图形代码、工具链或分析高帧性能时，读
  `.docs/obsredux-graphics-msvc.md`。
- 继续未完成工作时，读 `.docs/issues/` 中 `Status: open` 的 Issue。
- 追溯历史决策时，从 `.docs/archive/trellis-tasks/INDEX.md` 进入只读归档。

## 优先级

1. 用户当前明确指令。
2. 更近目录的 `AGENTS.md`。
3. 本文件。
4. 本文件路由到的 `CONTEXT.md` 与 `.docs/*.md`。
