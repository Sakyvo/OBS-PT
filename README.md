**简体中文** | [English](./README.en.md)

# OBS-PT

> **The OBS Premium Tuning for PotPvP**

OBS-PT 是面向 Minecraft Java 1.7.10 / 1.8.9 PotPvP 高帧录制工作流的 OBS Studio 独立分支。项目 fork 自 [OBS Studio 27.2.4](https://github.com/obsproject/obs-studio/releases/tag/27.2.4)，即最后一个采用 Qt 5 架构的正式版本，并在此基础上持续移植新版本中适合 PotPvP 高帧录制的功能。

## 下载与链接

| 项目 | 地址 |
|---|---|
| 下载链接 | [GitHub Releases](https://github.com/Sakyvo/OBS-PT/releases/latest) |
| 镜像加速 | [OBS-PT 1.0.0 安装包](https://ghfast.top/github.com/Sakyvo/OBS-PT/releases/download/1.0.0/OBS-PT-1.0.0-Installer.exe) |
| 仓库地址 | [Sakyvo/OBS-PT](https://github.com/Sakyvo/OBS-PT) |
| 问题反馈 | [GitHub Issues](https://github.com/Sakyvo/OBS-PT/issues) |
| 文档「pdir」 | [Part 3 - Video / 3.1 OBS](https://pdir.cc.cd/#Part-3-Video/3.1-OBS) |

## 核心特色

- **精简安装与完全便携化。** 不再依赖系统盘的 `%APPDATA%`，所有用户数据均位于安装目录，可与官方 OBS Studio 共存。
- **为 PotPvP 高帧录制提前调优。** 默认 480 FPS，并根据主显示器分辨率和可用硬件编码器自动生成配置；录海克斯战墙都不会编码过载，开箱即用，无需从零折腾参数。
- **移植 OBS 28.0 的 P1-P7 “预设” 体系。** 默认 P1 追求最大编码性能，比旧版“最大性能”预设留出更多高帧录制余量。
- **移植 OBS 30.2 的 Hybrid MP4。** 默认直接录制 `.mp4`，同时显著降低蓝屏、断电或进程异常导致整段录像无法使用的风险。
- **面向 NVIDIA 与 AMD 显卡的自适应默认值。** 自动选择可用编码器，并为不同硬件写入对应的 PotPvP 录制参数。

## 筹备中

- H.265 编解码支持
- 多音轨录制优化
- 翻新 FFmpeg
- 更好的 AMF 编码器兼容性（I 卡暂缓）
- 推流体验优化

## SAKYVO PRESENT

点点 Star 谢谢喵，能提 PR 就更好了喵。

> 1080p+ 用 CQP 27 是对的我跟你说。当然家里批发硬盘的当我没说（

---
## 上游项目与致谢

OBS-PT 基于 [OBS Studio](https://github.com/obsproject/obs-studio) 开发，并沿用其 GPL-2.0 许可。感谢 OBS Project 及所有原开发者和贡献者长期维护的采集、合成、编码、录制与推流模块，为本项目打下了坚实基础。

OBS-PT 是独立维护的第三方分支，与 OBS Project 官方发行版相互独立。