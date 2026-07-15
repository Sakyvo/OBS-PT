# OBS-PT 1.0.0

**OBS Premium Tuning for PotPvP**

## 更新内容

- 基于 OBS Studio 27.2.4 的独立 PotPvP 录制分支，保留 Qt 5 架构。
- 完全便携化运行，用户数据保存在安装目录，可与官方 OBS Studio 共存。
- 默认 480 FPS，并根据 Windows 主显示器分辨率和可用硬件编码器生成 PotPvP 配置。
- 为 NVIDIA 移植 P1-P7 预设体系；为 AMD 提供 OBS-PT、Speed、High、CQP 等自适应默认值。
- 默认使用 Hybrid MP4，降低蓝屏、断电或异常退出导致整段录像损坏的风险。
- 优化高帧录制的启动、停止和多音轨交错处理。
- 完善帮助菜单、关于页面以及 GitHub 稳定版手动/自动更新流程。
- 安装器默认保留现有用户配置；手动选择覆盖时，仍会保留推流、录像路径、热键和回放缓存配置。

## 相关链接

- [使用文档](https://pdir.cc.cd/#Part-3-Video/3.1-OBS)
- [问题反馈](https://github.com/Sakyvo/OBS-PT/issues)
- [源代码](https://github.com/Sakyvo/OBS-PT)

<details>
<summary><strong>English changelog</strong></summary>

## Highlights

- An independent PotPvP recording fork based on OBS Studio 27.2.4 and its Qt 5 architecture.
- Fully portable operation with user data stored under the install root, allowing coexistence with official OBS Studio builds.
- A 480 FPS default Profile that adapts to the Windows primary display and available hardware encoder.
- The P1-P7 preset system for NVIDIA and adaptive OBS-PT, Speed, High, and CQP defaults for AMD.
- Hybrid MP4 by default, reducing the risk that a crash, power loss, or forced shutdown makes an entire recording unusable.
- High-FPS recording start/stop and multi-track interleave improvements.
- Updated Help and About pages with manual and automatic stable GitHub update checks.
- Installer behavior that preserves existing settings by default; configuration replacement still keeps streaming, recording paths, hotkeys, and Replay Buffer settings.

## Links

- [Documentation](https://pdir.cc.cd/#Part-3-Video/3.1-OBS)
- [Issue tracker](https://github.com/Sakyvo/OBS-PT/issues)
- [Source code](https://github.com/Sakyvo/OBS-PT)

</details>
