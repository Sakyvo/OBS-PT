[简体中文](./README.md) | **English**

# OBS-PT

> **The OBS Premium Tuning for PotPvP**

OBS-PT is an independent OBS Studio fork for high-frame-rate Minecraft Java 1.7.10 / 1.8.9 PotPvP recording workflows. It is based on [OBS Studio 27.2.4](https://github.com/obsproject/obs-studio/releases/tag/27.2.4), the final Qt 5 release, and selectively ports newer recording features that benefit PotPvP players.

## Downloads and Links

| Item | Link |
|---|---|
| Download | [GitHub Releases](https://github.com/Sakyvo/OBS-PT/releases/latest) |
| Accelerated mirror | [OBS-PT 1.0.0 Installer](https://ghfast.top/github.com/Sakyvo/OBS-PT/releases/download/1.0.0/OBS-PT-1.0.0-Installer.exe) |
| Repository | [Sakyvo/OBS-PT](https://github.com/Sakyvo/OBS-PT) |
| Bug reports | [GitHub Issues](https://github.com/Sakyvo/OBS-PT/issues) |
| Documentation (pdir) | [Part 3 - Video / 3.1 OBS](https://pdir.cc.cd/#Part-3-Video/3.1-OBS) |

## Highlights

- **A streamlined, fully portable installation.** OBS-PT keeps all user data under its install root instead of `%APPDATA%`, and can coexist with the official OBS Studio build.
- **Pre-tuned for high-FPS PotPvP recording.** The default Profile uses 480 FPS and adapts to the primary display and available hardware encoder, providing a practical starting point without rebuilding an OBS configuration from scratch.
- **The OBS 28.1 P1-P7 preset system.** P1 is the default maximum-performance path and leaves more headroom for high-FPS recording than the legacy “Max Performance” preset.
- **Hybrid MP4 from OBS 30.2.** Record directly to `.mp4` while greatly reducing the risk that a crash, power loss, or forced shutdown makes the entire recording unusable.
- **Hardware-adaptive NVIDIA and AMD defaults.** OBS-PT selects an available encoder and writes a matching PotPvP recording configuration.
- **Per-application audio tracks.** Game audio and background music can be recorded on isolated tracks while retaining a complete compatibility mix for normal players.

## Isolating Game and Music Audio

This feature requires Windows 10 Build 19041 or newer. `Per-Application Audio Capture` captures the selected application's entire process tree.

1. Add a `Per-Application Audio Capture` Source, select the game window, and choose Track 2 `Game` in the creation dialog. Creation cannot finish until a track is selected.
2. Add another Source for the music player and choose Track 3 `Music`.
3. Open **Audio Routing** below the mixer to move, order, mute, or inspect Sources. Each Source belongs to one Editor Track.
4. Tracks do not need to be enabled separately under Output. OBS-PT generates a complete Track 1 `Mix` and writes every Editor Track that has an eligible Source.
5. Record to Hybrid MP4. Editors can read the isolated `Game` and `Music` streams, while normal players continue to play `Mix` by default.

Window matching can fall back by title, window class, or executable. Browsers normally use multiple processes, so capture follows the selected browser process tree and cannot guarantee per-tab isolation. Windowless helper processes outside the selected process tree cannot be selected or captured through this Source.

## In Progress

- H.265 encoding and decoding
- FFmpeg refresh
- Better AMF compatibility (Intel GPUs are deferred; AMD GPU pull requests are welcome)
- Streaming improvements

## SAKYVO PRESENT

A Star would be appreciated. Pull requests are even better.

> CQP 27 is the intended AMD AMF default at 1080p and above. Disk wholesalers are free to disagree.

## Upstream and Thanks

OBS-PT is based on [OBS Studio](https://github.com/obsproject/obs-studio) and remains licensed under GPL-2.0. Thank you to the OBS Project and every original developer and contributor whose long-term work on capture, compositing, encoding, recording, and streaming provides the foundation for this project.

OBS-PT is an independently maintained third-party fork and is not an official OBS Project release.
