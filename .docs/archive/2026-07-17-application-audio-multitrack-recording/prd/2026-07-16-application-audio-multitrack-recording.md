# OBS-PT 1.1.0 Application Audio Multitrack Recording

Status: archived

On 2026-07-17, the maintainer explicitly waived the 30-minute-only qualification in User Stories 28-31 and directed the completed implementation batch to be archived. The passing 60-second 1920x1080 480 FPS preflight remains the strongest performance evidence; this PRD does not claim that a 30-minute run passed.

## Problem Statement

OBS-PT can already encode up to six Recording Tracks, and its recording settings allow users to select them, but the shipped Scene Collection contains only Desktop Audio routed to every mixer. Enabling more Recording Tracks therefore produces copies of the same desktop mix instead of separating Minecraft audio from background music. PotPvP recorders cannot independently rebalance, replace, or remove game and music audio during editing.

Users need application-level audio routing that works with the existing Hybrid MP4 recording path without reducing the native-resolution 480 FPS recording target. It must remain predictable when an application has not started, exits, or restarts, and must not introduce foreground dialogs, stop-recording hangs, audio drift, or high-FPS performance regressions.

## Solution

OBS-PT 1.1.0 will add the OBS 28-style Application Audio Capture (BETA) Source on supported Windows systems. A user selects an application window and OBS-PT captures audio from the matched process tree. Desktop Audio remains the complete compatibility mix on Recording Track 1, while Application Audio Sources can provide isolated game, music, voice, microphone, and auxiliary tracks for editing.

A fresh installation will still record only Track 1 by default. Desktop Audio will enter only Track 1, and each newly created Application Audio Source will initially enter only Track 2. Users explicitly enable additional Recording Tracks and route later sources through Advanced Audio Properties. No application is guessed or added to the default Scene Collection.

The implementation will reuse the proven upstream process-loopback path and selected released fixes while preserving OBS-PT's Qt 5, OBS 27.2.4, portable User Data Root, Hybrid MP4, and high-FPS constraints.

## User Stories

1. As a PotPvP recorder, I want game audio separated from background music, so that I can rebalance them during editing.
2. As a PotPvP recorder, I want to remove or replace background music without losing Minecraft audio, so that I can change the soundtrack after recording.
3. As a PotPvP recorder, I want a complete mixed first track, so that ordinary media players play the recording with all expected sound.
4. As a PotPvP recorder, I want isolated tracks in the same Hybrid MP4 file, so that I do not need separate synchronized audio recordings.
5. As a new OBS-PT user, I want recording to remain usable before I configure multitrack audio, so that the default experience does not produce silent or empty primary audio.
6. As a new OBS-PT user, I want only Recording Track 1 enabled initially, so that unused tracks are not written by default.
7. As a new OBS-PT user, I want Desktop Audio routed only to the complete mix, so that enabling isolated tracks does not duplicate the desktop mix into every track.
8. As a user adding an Application Audio Source, I want it initially routed only to the Game track, so that it does not double the same audio in the complete mix.
9. As a user adding a second Application Audio Source, I want to route it to Music through Advanced Audio Properties, so that game and music remain independent.
10. As an editor, I want stable track names, so that editing software identifies the purpose of each stream.
11. As an editor, I want the track names to remain stable across UI language changes, so that an existing workflow does not rename media streams.
12. As an editor, I want `Mix`, `Game`, `Music`, `Voice Chat`, `Microphone`, and `Aux` metadata, so that common audio roles are immediately recognizable.
13. As a user, I want to choose a currently visible or minimized application window, so that selecting the intended game or music player is straightforward.
14. As a Minecraft user, I want title, window-class, and executable match priorities, so that I can distinguish multiple `javaw.exe` processes.
15. As a Minecraft user, I want the selected process and its descendants captured, so that audio produced by the client process tree stays together.
16. As a music listener, I want a browser or music player captured as one application, so that its output can be assigned to the Music track.
17. As a browser user, I accept that all audio in the browser process tree is captured together, so that the implementation does not promise unreliable tab-level isolation.
18. As a user, I want a configured source to wait silently when its application is not running, so that starting OBS-PT first does not trigger an error dialog.
19. As a user, I want capture to resume when the matching application appears, so that startup order does not require rebuilding the source.
20. As a user, I want capture to recover after the target application restarts, so that a game or music-player restart does not permanently silence the isolated track.
21. As a user, I want only the first connection failure logged, so that expected waiting does not flood the log.
22. As a user, I want inactive Application Audio Sources to stop reconnect work, so that unused scenes do not cause periodic system or game hitches.
23. As a user, I want stopping a recording to complete even when an application source is reconnecting, so that OBS-PT does not become unresponsive.
24. As a Windows 10 Build 19041 or newer user, I want Application Audio Capture available in the Sources menu, so that I can configure application routing.
25. As a user on an older Windows build, I want the unsupported Source omitted while existing audio capture still works, so that an unavailable API does not break OBS-PT.
26. As a multilingual user, I want the Source name and match-priority controls translated in OBS-PT's supported languages, so that the feature fits the existing UI.
27. As a user, I want the Source labeled BETA, so that Windows process-loopback limitations are not hidden.
28. As a high-FPS recorder, I want native-resolution 480 FPS recording preserved, so that audio routing does not compromise OBS-PT's primary workflow.
29. As a high-FPS recorder, I want rendering and encoding lag to remain at or below 0.5%, so that application routing does not cause a performance regression.
30. As a long-session recorder, I want audio and video drift to remain within 50 ms over 30 minutes, so that isolated tracks stay usable in post-production.
31. As a long-session recorder, I want no crackle, corruption, or track bleed, so that the separated audio is reliable.
32. As a Hybrid MP4 user, I want multiple AAC streams preserved through normal stop and recovery metadata, so that the default resilient container remains useful.
33. As a user of another non-FLV standard recording format, I want existing multitrack muxer behavior preserved, so that this feature does not regress alternative formats.
34. As an FLV user, I accept the existing single-track limitation, so that this work does not pretend the container supports the same workflow.
35. As an existing internal-test user, I want my current Scene Collection left untouched unless I request configuration replacement, so that implementation work does not rewrite user choices.
36. As an OBS-PT maintainer, I want the feature derived from merged upstream code and released fixes, so that local behavior follows proven Windows audio handling.
37. As an OBS-PT maintainer, I want actual Windows process loopback tested instead of mocked internals, so that passing tests demonstrate the real user path.
38. As an OBS-PT maintainer, I want the installed and staged builds tested, so that a source that works only in the build tree cannot be released accidentally.
39. As an OBS-PT maintainer, I want version surfaces updated consistently to 1.1.0, so that the application, updater model, installer, and package agree.
40. As an OBS-PT maintainer, I want GitHub Releases left unchanged during implementation, so that local development does not publish an unapproved release.

## Implementation Decisions

- Use the independent OBS 28-style Application Audio Source rather than integrating audio controls into Game Capture or Window Capture.
- Keep the upstream Source identifier and Windows process-loopback architecture so Scene Collection data remains structurally compatible with upstream semantics.
- Promote the existing Windows window-enumeration and matching helpers to a shared utility surface usable by both visual capture and process audio capture. Existing Game Capture and Window Capture behavior must remain unchanged by this prefactoring.
- Dynamically load `ActivateAudioInterfaceAsync` and use process-loopback activation with `PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE`.
- Register the Source only on Windows 10 Build 19041 or newer. Older systems retain Desktop Audio and other existing Sources without a fallback implementation.
- Present a window selector plus title, class, and executable match priorities. Store all three window attributes and use the selected priority when reacquiring a process.
- Treat a missing target window as an idle state. Log the initial failure once, retry in the background, and never show a modal dialog or take foreground focus.
- Stop reconnect work while the Source is inactive and include the released stop-hang correction when coordinating capture, reconnect, and teardown.
- Include the released process-audio corrections for `WAVEFORMATEXTENSIBLE` sizing, WASAPI timestamps, silent buffers, timestamp-error handling, discontinuities, and noisy logging.
- Do not wholesale transplant the OBS 30.2 implementation because it contains later Game Capture and Window Capture audio-hook integration that is out of scope.
- Add the process-audio Source icon and translated display/property strings for en-US, zh-CN, zh-TW, ja-JP, and ko-KR.
- Keep `(BETA)` or its translated equivalent in the Source display name.
- Do not add an Application Audio Source to the shipped Scene Collection and do not guess `javaw.exe` or any music application.
- Keep the shipped Profile's selected Recording Tracks bitmask at Track 1 only.
- Route shipped Desktop Audio only to mixer 1, making Track 1 the complete compatibility mix.
- Route each newly created Application Audio Source only to mixer 2. Duplicating or reusing an existing Source preserves its stored mixer assignment.
- Do not automatically enable Recording Track 2 when a Source is created. Users explicitly enable and reassign tracks.
- Set stable Recording Track metadata names to `Mix`, `Game`, `Music`, `Voice Chat`, `Microphone`, and `Aux`.
- Retain the existing 160 Kbps AAC bitrate defaults unless testing reveals an unrelated defect; audio bitrate tuning is not part of this feature.
- Do not migrate or rewrite existing Scene Collections or Profiles. Shipped defaults apply to fresh installations or an explicitly requested installer configuration replacement.
- Guarantee the complete multitrack workflow on Hybrid MP4. Preserve existing behavior for MP4, MKV, MOV, TS, and M3U8, and retain FLV's single-track selection.
- Update product, binary, updater-model, packaging, and installer version surfaces to 1.1.0 without publishing or editing GitHub Releases.
- Update user-facing project documentation to describe game/music separation, the Windows requirement, configuration model, and known application-process limitations.
- Refresh staging and the installer after implementation because the change affects the OBS executable, libobs, and the win-wasapi plugin.

## Testing Decisions

- Use one primary testing seam: an end-to-end run of the actual Windows OBS-PT binary from a fresh portable User Data Root through a real Hybrid MP4 output file.
- Use two deterministic audio-producing processes that play distinguishable fixed-frequency signals. Configure one as Game and one as Music, while Desktop Audio supplies Mix.
- Validate only externally observable behavior: Source availability and controls, serialized mixer defaults, captured stream content, automatic reacquisition, recording completion, logs, lag counters, and packaged runtime behavior.
- Inspect the output with `ffprobe` to verify three AAC streams and their metadata names. Decode or analyze each stream with FFmpeg to prove that Game and Music contain only their expected signal and Mix contains both.
- Run target-lifecycle scenarios with the target already running, started after OBS-PT, stopped during capture, and restarted with a changed window title.
- Verify that missing targets produce no modal UI, no foreground activation, and no repeated log spam.
- Run a 30-minute native-resolution 480 FPS Hybrid MP4 recording with two active Application Audio Sources. Rendering lag and encoding lag must each remain at or below 0.5%.
- Compare the 30-minute audio streams for no more than 50 ms audio/video drift and check for crackle, silence gaps, corruption, or cross-track bleed.
- Stop recording while a target is connected, disconnected, and reconnecting. Every case must complete without an OBS-PT hang or orphaned process.
- Smoke-test the non-FLV standard recording formats supported by the current muxer path and verify FLV still exposes and writes only one selected track.
- Load a fresh shipped configuration to verify only Recording Track 1 is enabled, Desktop Audio uses only mixer 1, all six metadata names exist, and no Application Audio Source is pre-created.
- Build with the supported VS2022 v143 toolchain and the existing Windows SDK requirement. No private WASAPI mock seam will be added because it cannot validate OS process-loopback behavior.
- Refresh staging, build the installer once, validate its timestamp and archive integrity, compute SHA-256, and launch it to the Setup window before considering the batch complete.
- Re-run a packaged binary from staging or an installed disposable root so plugin registration, locale data, icons, and shared helper linkage are verified outside the build tree.

## Out of Scope

- Capturing or testing a physical microphone in this batch.
- Automatically configuring microphone, voice-chat, or auxiliary Sources.
- Integrating application audio into Game Capture or Window Capture.
- Replay Buffer behavior or acceptance testing.
- Streaming and stream-track redesign.
- Browser tab-level or browser-window-level audio isolation.
- Capturing windowless audio processes or helper processes outside the selected process tree.
- Virtual audio-device, driver, or audio-cable fallbacks.
- Supporting Application Audio Capture below Windows 10 Build 19041.
- Automatically selecting Minecraft, `javaw.exe`, a browser, or a music player.
- Automatically enabling Recording Tracks 2-6.
- Migrating existing user mixer assignments, Profiles, or Scene Collections.
- Container-specific feature or recovery work outside Hybrid MP4.
- Adding multitrack support to FLV.
- Changing AAC codec or bitrate policy, adding H.265, or performing broader FFmpeg work.
- Publishing, replacing, or editing a GitHub Release.

## Further Notes

- Primary upstream implementation evidence: [OBS PR #5218](https://github.com/obsproject/obs-studio/pull/5218), merged for OBS 28.
- The inactive-source performance correction is [OBS PR #7505](https://github.com/obsproject/obs-studio/pull/7505), released in OBS 28.0.3. Reports in [issue #7230](https://github.com/obsproject/obs-studio/issues/7230) confirm that it removed periodic game FPS and input-lag regressions from inactive Application Audio Sources.
- Later released corrections cover stop hangs, audio-format sizing, timestamps, silent-buffer handling, and timestamp-error handling. The relevant process-source code remained materially unchanged through OBS 32.1.2.
- [Issue #8776](https://github.com/obsproject/obs-studio/issues/8776) shows that restart matching remains an upstream risk; OBS-PT therefore treats automatic recovery as an explicit acceptance test rather than assuming the upstream code is sufficient.
- [Issue #9838](https://github.com/obsproject/obs-studio/issues/9838) documents the non-descendant helper-process limitation. This is a Windows process-ownership boundary and is explicitly excluded from 1.1.0.
- Long-session crackle and drift reports such as [issue #8064](https://github.com/obsproject/obs-studio/issues/8064) make the 30-minute real-device test mandatory. Maintainers later attributed the reported resolution to Windows changes, so a local mock would not provide meaningful confidence.
- `Application Audio Source` and `Recording Track` are defined in the project glossary. No ADR is required because this is an incremental upstream-derived capability and the later integrated-capture path remains available as a future extension.
