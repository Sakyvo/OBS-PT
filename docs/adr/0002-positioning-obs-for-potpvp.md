# 项目定位收窄到 "OBS For PotPvP"

> **更新 2026-06-14 (task 06-05-obsredux-480fps-default-profile)**：品牌由 OBSRedux 重命名为 **OBS-PT**（exe `OBS-PT.exe`，v0.0.1）。默认值演进：Fractional FPS `360 → 480`；静态 `CQP 25` → **按分辨率自适应**（高度 `<1080 → 20`，`≥1080 → 26`）；编码器与颜色格式在首启按硬件自适应（jim_nvenc/ffmpeg_nvenc/obs_qsv11/amd_amf_h264/obs_x264 各自完整模板，NV12）；分辨率取主显示器原生（base=output，不封顶）。**定位与工作流链路（PotPvP → Frame Blending → Distribution Ceiling）不变**，仅默认值自适应化。详见 `.trellis/spec/obspt-bootstrap.md`。
>
> **更新 2026-06-20 (task 06-19-installer-package)**：首发安装包优先保证旧/问题 NVIDIA 驱动可录制。`jim_nvenc` 模板改为兼容优先的 `preset=hq`、`bf=0`，不再写当前编码器不读取的 `preset2/tune/multipass`。

OBS-PT 的对外定位明确为"Minecraft Java 1.7.10 / 1.8.9 PotPvP 录制玩家的定制 OBS 分支"，所有默认值——480/1 Fractional FPS、跟随显示器分辨率、NVENC H.264 优先（无 HEVC）、按分辨率自适应 CQP、兼容优先的 `preset=hq` + `bf=0`、传统 MP4 容器、单轨混音、输出到 `<Install Root>/recordings/`——均围绕 "High-FPS Source（MC 客户端）→ Frame Blending 后期工作流 → Bilibili Distribution Ceiling（约 20 Mbps）" 这条工作流链路收敛。en-US / zh-CN / zh-TW / ja-JP / ko-KR 五语言支持是 i18n 实施约束（宣传触达 + 翻译工作量），不是用户群定位。

## Considered Options

- **"轻量游戏高帧录制通用版"**：被否决，因为模糊定位会让默认值（如 CQP 25 + NVENC p1 + tune=ll 这种"为 360fps 编码不丢帧而牺牲单帧质量"的组合）显得诡异且无法解释；窄定位反而让激进默认值有正当性。
- **"东亚精简定制版"**：被否决，这是早期讨论用语，混淆了 i18n 范围与产品定位；语言列表不应被解读为用户群。
- **FPS 游戏玩家 / 职业电竞复盘工具**：列为未来扩展议程，当下不分散精力。

## Consequences

- Frame Blending 工作流不熟悉的读者看到高 CQP + 高 FPS + 兼容优先 NVENC preset 组合会想"为什么不是 cinematic 标准的 CQP 18 / 新式 preset p5 / tune hq"——本 ADR + [[Distribution Ceiling]] 是统一答案；同时首发安装包优先保证旧/问题 NVIDIA 驱动上能录制。
- 任何后续"为了通用性而调整默认值"的 PR 应被视为偏离定位，先审视是否值得调整定位再说。
- 营销/README 文案使用 "OBS For PotPvP" 作为 tagline，不使用"东亚版""高帧录制通用版"等表述。
