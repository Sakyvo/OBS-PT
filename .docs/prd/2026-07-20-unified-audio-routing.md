# OBS-PT Unified Audio Routing

## Problem Statement

OBS Studio 27.2.4 exposes audio routing through two independent layers: each source has a six-checkbox mixer matrix in Advanced Audio Properties, while each output separately decides which tracks are written. A source can therefore be assigned correctly but omitted from the recording because `RecTracks` or `FFAudioMixes` was not enabled. The inverse is also possible: an enabled track can contain only a muted source and become a useless silent stream.

For OBS-PT's PotPvP workflow, this model is unnecessarily difficult. The common case is to separate Minecraft, background music, microphone, and voice chat for editing while preserving one complete audio stream for ordinary players and Bilibili streaming. Users need one visible source-to-track model, deterministic output behavior, and errors that prevent silent data loss.

The current Advanced Audio Properties entry is also hidden in a mixer submenu, sorts sources by name, cannot persist a user-defined order, and requires clicking track checkboxes one by one. These behaviors make the primary audio workflow hard to discover and easy to misconfigure.

## Solution

OBS-PT will replace the upstream per-source/per-output track matrix with a Scene Collection-owned Audio Routing Configuration shown in a dockable routing panel next to the main audio mixer.

The main window separates visual composition from audio ownership. `Visuals` creates and lists only sources with video output. `Audio & Routing` creates pure-audio sources, keeps them alive at Scene Collection scope without Scene Items, and presents fixed rows `1` through `6` plus `M` in a three-column `Track / Name / Source` table. Sources that output both video and audio remain visual Scene Items while their audio is registered in the same routing model.

Track 1 is a system-generated Compatibility Mix named `Mix`. It carries complete audio for ordinary players and streaming. Tracks 2-6 are Editor Tracks with fixed default roles: `Game`, `Music`, `Microphone`, `Voice Chat`, and `Aux`. Each manageable source belongs to at most one Editor Track. Muted sources move into a dedicated Muted Sources Group and do not create output tracks.

Output topology is derived automatically from the routing configuration. Independent recording, streaming, Replay Buffer, and custom FFmpeg track selectors are removed. Supported multi-track containers receive `Mix` plus non-empty Editor Tracks; single-track containers receive `Mix`. Every output validates the complete topology before starting and fails clearly instead of silently dropping audio.

The design explicitly prioritizes ordinary-player compatibility. `Mix` remains in multi-track files and is the default audio stream even though VEGAS may import it alongside Editor Tracks. Editors can mute or remove `Mix` on the timeline.

## User Stories

1. As a PotPvP recorder, I want Minecraft and background music on separate Editor Tracks, so that I can adjust them independently in post-production.
2. As a recorder without a microphone, I want to configure multiple application sources without needing unavailable hardware, so that the workflow remains testable and useful.
3. As a recorder, I want one complete `Mix` stream in every normal multi-track recording, so that ordinary players reproduce all intended sound.
4. As a VEGAS user, I want each emitted audio stream to keep a stable file order and metadata name, so that `Mix`, `Game`, `Music`, `Microphone`, `Voice Chat`, and `Aux` remain deterministic even though VEGAS does not expose container stream names as timeline track names.
5. As a VEGAS user, I want the product to acknowledge that `Mix` may be imported with Editor Tracks, so that the duplicate is deliberate rather than an unexplained bug.
6. As a user, I want source routing and output track inclusion to be one model, so that assigning a source cannot be silently defeated by a second checkbox elsewhere.
7. As a user, I want each source to belong to at most one Editor Track, so that accidental multi-track duplication cannot amplify audio in an editor.
8. As a user, I want multiple sources to share one Editor Track when needed, so that several microphones or related applications can form one editing stem.
9. As a user, I want Editor Tracks displayed from Track 2 through Track 6 in fixed order, so that their numbers remain predictable across sessions.
10. As a user, I want Track 2 to default to `Game`, Track 3 to `Music`, Track 4 to `Microphone`, Track 5 to `Voice Chat`, and Track 6 to `Aux`, so that common sources have obvious destinations.
11. As a user, I want Track 1 fixed as `Mix`, so that player compatibility cannot be disabled or renamed accidentally.
12. As a user, I want to rename Editor Tracks, so that file metadata can match my editing vocabulary.
13. As a multilingual user, I want default file metadata to remain stable English values while the UI explains roles in my selected language, so that projects remain portable between machines.
14. As a user, I want custom track names to survive language and Profile changes, so that switching settings does not rewrite file metadata.
15. As a user, I want duplicate custom track names allowed while track numbers remain visible, so that naming flexibility does not remove identity.
16. As a user, I want invalid empty or control-character track names rejected, so that containers receive usable metadata.
17. As a user, I want the main mixer to retain meters, faders, volume, and mute controls, so that frequent live adjustments stay compact.
18. As a user, I want a persistent audio-routing entry below the main mixer, so that routing is not buried in a submenu.
19. As a user, I want the routing panel dockable, floatable, and layout-persistent, so that it fits both compact and multi-monitor workspaces.
20. As a user, I want the former Advanced Audio Properties command to focus the routing panel, so that familiar navigation remains useful.
21. As a user, I want a single-source inspector for volume, mono, balance, sync offset, and monitoring, so that advanced controls remain available without restoring the old matrix.
22. As a user, I want the routing panel to show all audio sources in the current Scene Collection, so that disconnected, inactive, hidden-scene, and muted sources remain manageable.
23. As a user, I want the main mixer to show the same complete ordered source set as Audio & Routing, so that a source never disappears merely because it is currently inactive.
24. As a user, I want hidden, disconnected, or currently silent sources to keep their explicit routing, so that OBS-PT never guesses intent from audio level or scene visibility.
25. As a user, I want a source with an explicit non-muted assignment to keep its Editor Track even if it produces no samples yet, so that late-starting applications preserve topology.
26. As a user, I want a truly source-less Editor Track omitted, so that files do not contain structural empty streams.
27. As a user, I want muted sources grouped separately, so that exclusion from recording is visible and controlled from the same panel.
28. As a user, I want newly muted sources placed at the top of the Muted Sources Group, so that recent actions are easy to find.
29. As a user, I want to reorder muted and active source rows manually, so that the panel follows my workflow instead of alphabetical order.
30. As a user, I want unmuting to restore the source's previous Editor Track and position, so that temporary privacy actions do not destroy organization.
31. As a user, I want muting to take precedence over `Monitor Only`, so that a muted source is visibly excluded while preserving its monitoring state for later restoration.
32. As a monitoring user, I want `Monitor Only` to remain distinct from source mute, so that I can hear a source locally without recording it.
33. As a monitoring user, I want a `Monitor Only` source excluded from effective-track calculation, so that it cannot create a silent output stream.
34. As a monitoring user, I want a warning when the monitoring device is also captured by Desktop Audio, so that I can recognize a potential duplicate path.
35. As a monitoring user, I want the warning to preserve my explicit route and link to monitoring settings, so that OBS-PT does not silently remove audio.
36. As a keyboard user, I want every source row focusable and movable through a menu, so that drag-and-drop is not the only routing method.
37. As a screen-reader user, I want source name, track, mute, monitoring, lock, and pending state announced, so that routing is operable without visual inference.
38. As a user, I want drag, menu, and keyboard actions to call the same route commands, so that they enforce identical validation and locking.
39. As a user, I want application-audio creation to require an explicit Editor Track choice, so that no source silently enters `Game` and no process-name heuristic guesses my intent.
40. As a user, I want microphone and audio-input sources to default to Track 4 `Microphone`, so that input devices are routed sensibly.
41. As a user, I want unknown audio source types to use the first empty Editor Track and then `Aux`, so that every source receives a deterministic initial route.
42. As a user, I want duplicated sources to inherit routing while receiving an independent Source Identity, so that copies start predictably but can diverge.
43. As a user, I want adding an existing pure-audio source through Audio & Routing to preserve its route, so that reuse does not create a second assignment.
44. As a user, I want deleting and recreating a same-named source treated as a new identity, so that stale routing is not applied by name.
45. As a user, I want pure-audio capture owned by the Scene Collection rather than a visual Scene, so that scene switching cannot stop capture or change its recording track.
46. As a user, I want same-named sources in different Scene Collections to remain independent, so that collections do not leak routing into one another.
47. As a user, I want routing configuration to follow the Scene Collection rather than the Profile, so that source assignments and track names remain aligned.
48. As a user, I want Desktop Audio devices fixed under Compatibility Mix, so that system playback cannot be duplicated into Editor Tracks accidentally.
49. As a user with multiple output devices, I want all configured Desktop Audio devices treated as one authority group, so that intended system playback reaches `Mix`.
50. As a user, I want Application Audio Sources excluded from `Mix` while any Desktop Audio authority is unmuted, so that game and music are not doubled.
51. As a user, I want muting all Desktop Audio devices to make eligible Application Audio Sources feed `Mix`, so that player audio remains complete when I intentionally disable desktop capture.
52. As a user, I want unmuting a Desktop Audio device to restore authority and de-duplicate Application Audio Sources immediately, so that explicit control has predictable effect.
53. As a user, I want device disconnects and reconnects not to switch Mix authority mid-session, so that files do not change composition unexpectedly.
54. As a user, I want microphones, capture cards, media sources, and other non-system sources included in `Mix`, so that ordinary playback remains complete.
55. As a user, I want private transition and plugin audio included in `Mix` without anonymous Editor Tracks, so that internal sounds remain audible but manageable routing stays clean.
56. As a recorder, I want recording topology frozen when the session starts, so that audio stream count and identity cannot change within a file.
57. As a recorder, I want route movement and track renaming blocked during recording, so that a file never changes topology halfway through.
58. As a recorder, I want the blocked action to tell me to stop and restart recording, so that the required recovery is explicit.
59. As a recorder, I want volume and mute controls available during recording, so that I can react to private or disruptive audio immediately.
60. As a recorder, I want a source that was eligible at session start to resume its existing route after temporary mute, so that live mute does not require a new file.
61. As a recorder, I want a source that was already muted at session start deferred until the next session when unmuted, so that it cannot create a new stream mid-file.
62. As a recorder, I want newly created or duplicated audio sources marked for the next session, so that source management remains available without altering the current file.
63. As a recorder, I want deleting a source to leave the current file's existing stream in place, so that topology remains structurally valid.
64. As a recorder, I want changing a source's capture target to preserve Source Identity, so that retargeting does not silently move tracks.
65. As a recorder, I want Scene Collection switching blocked while topology-bearing outputs are active, so that the route model cannot be replaced mid-session.
66. As a streamer, I want Bilibili and other ordinary streams fixed to Compatibility Mix, so that live viewers receive complete audio.
67. As a streamer, I want to mute every source without stopping or reconnecting the stream, so that private moments can remain private.
68. As a streamer, I want a silent AAC Compatibility Mix transported while every source is muted, so that the live protocol remains stable.
69. As a Bilibili streamer, I do not want irrelevant VOD controls shown, so that the main streaming UI reflects the service in use.
70. As a Twitch streamer, I want an optional VOD Track chosen from one Editor Track, so that excluded music does not enter the archive.
71. As a Twitch streamer, I want an empty selected VOD Track to block startup instead of falling back to `Mix`, so that excluded content cannot leak into VOD.
72. As a Twitch streamer, I want temporary mute after startup to silence the VOD Track without ending the stream, so that live privacy controls remain usable.
73. As a Replay Buffer user, I want saved replays to use the same frozen topology as normal recording, so that editing behavior is consistent.
74. As a Replay Buffer user, I want route changes locked while the buffer is active, so that saved clips have stable tracks.
75. As a Simple Output user, I want the same automatic routing as Advanced Output, so that output mode does not reintroduce a hidden track selector.
76. As a recorder, I want Hybrid MP4, MP4, MOV, MKV, and TS to contain `Mix` plus non-empty Editor Tracks, so that supported containers preserve editing stems.
77. As a recorder, I want FLV and M3U8/HLS to use only `Mix`, so that single-track delivery remains compatible without deleting my Editor Track configuration.
78. As a custom FFmpeg user, I want known single-track formats to use `Mix`, so that a compatible output can still start safely.
79. As a custom FFmpeg user, I want unknown or incompatible container capability to block startup with a clear error, so that tracks are never silently lost.
80. As a user, I want every required audio encoder and container stream validated before output starts, so that a partial topology is never accepted.
81. As a user, I want an initialization error to identify the track, container, and underlying cause, so that I can repair the configuration.
82. As a user with another output already active, I want a failed second output to leave the first output running, so that one incompatibility does not destroy ongoing work.
83. As a user, I want two compatible topology-bearing outputs to share one frozen route and audio bitrate, so that they cannot disagree about source assignment.
84. As a user, I want a Mix-only stream allowed alongside a topology-bearing output, so that Bilibili streaming and local multi-track recording can coexist.
85. As a user, I want local recording with every source muted at startup to create a video-only file, so that no useless silent track is stored.
86. As a user, I want one global AAC bitrate defaulting to `160 Kbps`, so that track quality is consistent without six repeated settings.
87. As a user, I want the unified bitrate locked while any related output is active, so that encoders do not diverge mid-session.
88. As an existing user, I want setup to explain that the unified-audio topology requires a configuration reset, so that destructive scope is explicit before installation.
89. As an existing user, I want only stream settings, recording paths, hotkeys, and Replay Buffer settings restored, so that personal operational values survive without carrying incompatible topology forward.
90. As an existing user, I want scenes, sources, routing, dock layout, video, audio, and recording encoder settings replaced by packaged defaults, so that the new model starts from one known state.
91. As an existing user, I want rejecting the reset confirmation to prevent installation writes, so that setup never performs a partial unapproved reset.
92. As a fresh-install user, I want setup to skip the reset page when no User Data Root exists, so that a warning about nonexistent old data is not shown.
93. As a multilingual user, I want routing controls and errors translated in the five supported locales, so that the complete workflow is understandable.
94. As a QA engineer, I want deterministic tones and real output files to verify track separation, so that tests validate user-visible artifacts rather than internal flags.
95. As a QA engineer, I want 480 FPS qualification retained during audio tests, so that multi-track support cannot hide a high-frame performance regression.

## Implementation Decisions

- Introduce a single Audio Routing model owned by the active Scene Collection. It is the only authority for source assignment, order, mute-group restoration, track naming, monitoring exclusion, and output topology derivation.
- Persist a routing-model version with the Scene Collection. Profile configuration retains output format, output path, unified bitrate, and other encoder parameters, but no source-to-track matrix.
- Keep Track 1 fixed as Compatibility Mix with metadata name `Mix`. Track 2-6 have fixed numeric roles and user-editable metadata names.
- Store user track names independently of UI locale. New roles use stable English metadata; localized text describes the role in the UI.
- Sanitize custom names by trimming outer whitespace and rejecting empty strings and control characters. Duplicate names remain valid because the track number is always shown.
- Make each manageable Source Identity belong to exactly one Editor Track or the Muted Sources Group. Multiple Source Identities may share an Editor Track.
- Treat source mute and `Monitor Only` as orthogonal state. Mute controls group membership; `Monitor Only` preserves the prior Editor Track but excludes the source from every output and from effective-track calculation.
- Preserve the prior Editor Track, source order, and monitoring state while muted. New muted entries default to reverse chronological order and remain manually reorderable.
- Include all user-manageable audio sources in the current Scene Collection, including inactive and hidden-scene sources. Do not use scene activation, connection, audio level, or actual samples as eligibility signals.
- Keep pure-audio sources outside Scene Items. The Scene Collection holds their strong references and active lifetime; saving and loading the Scene Collection must preserve them even when no visual Scene references them.
- Filter the `Visuals` add menu by video capability and the `Audio & Routing` add menu by pure-audio capability. Do not maintain a plugin-name allowlist.
- Keep private transition and plugin audio Mix-only unless the plugin exposes it as a normal manageable Source Identity.
- Treat all configured Desktop Audio sources as the system-playback authority group. They are fixed under Compatibility Mix and cannot be dragged into Editor Tracks.
- If at least one authority source initialized successfully at session start and is currently unmuted, exclude Application Audio Sources from Compatibility Mix to prevent duplicate system playback.
- If every authority source is explicitly muted, feed session-eligible Application Audio Sources into Compatibility Mix. Restore authority and de-duplication as soon as any Desktop Audio source is unmuted.
- Do not switch authority because of level, disconnect, or reconnect after session start. Log and surface a warning; a new topology-bearing session reevaluates device availability.
- Always include eligible microphone, input-device, capture-card, media, and other non-system sources in Compatibility Mix. This deliberate duplication with their Editor Tracks is required for ordinary-player compatibility.
- Preserve explicit `Monitor and Output` routing. Detect monitoring-device overlap with Desktop Audio and warn with a route to monitoring-device settings; never auto-remove the source.
- Require an explicit Track 2-6 selection before creating an Application Audio Source. Keep confirmation disabled until a track is selected, preserve the choice through undo/redo, and do not classify by process or executable name. Adding an existing Source Identity preserves its current route.
- Default microphones and audio-input devices to Track 4 `Microphone`. Default other unknown audio types to the first empty Editor Track, falling back to Track 6 `Aux`.
- Source reuse preserves routing. Source duplication copies routing into a new Source Identity. Deletion removes the identity; same-name recreation is a new source and follows defaults.
- Place a permanent audio-routing command below the main mixer. The routing panel is dockable, floatable, and included in normal layout persistence.
- Retain meters, faders, mute, and volume in the main mixer. Enumerate every manageable route in routing order, including disconnected, inactive, hidden-scene, and muted-group sources; activity changes meter state rather than row existence. Replace the old Advanced Audio Properties matrix with the routing panel plus a single-source inspector window.
- Make the old Advanced Audio Properties action focus and reveal the routing panel rather than opening a separate matrix dialog.
- Support drag-and-drop and an equivalent focusable command menu for moving sources, ordering sources, muting, and opening the inspector. All entry points use the same command validation.
- Announce source name, track number/name, mute, monitoring, output-lock, and pending-next-session state through accessibility metadata.
- Remove streaming-track, recording-track, FLV-track, and custom FFmpeg mixer controls from output settings. Remove per-track bitrate and per-track name controls.
- Remove Desktop Audio and Mic/Aux source-slot selectors from Settings > Audio. Keep sample rate, channel layout, meter, and monitoring engine settings there; all source creation and device selection belongs to Audio and Routing.
- Keep one `Audio Bitrate` control in output settings. It applies to Compatibility Mix, Editor Tracks, VOD, recording, Replay Buffer, and streaming, defaults to `160 Kbps`, and is locked while a related output is active.
- Derive effective local-file topology at session start from `Mix` plus Editor Tracks containing at least one assigned, unmuted, non-`Monitor Only` source. A source remains eligible even when inactive, disconnected, or silent.
- Generate a video-only local output when no effective source exists at startup. Do not create silent local Mix or Editor streams.
- Keep a silent Compatibility Mix AAC stream for live output when all sources are muted. Mute and unmute must not reconnect or stop the stream.
- Freeze topology and source eligibility for normal recording, custom FFmpeg recording, Replay Buffer, and VOD-enabled streaming. A Compatibility Mix-only stream does not freeze Editor Track routing.
- Block route movement, track rename, monitoring-mode changes that alter output eligibility, and Scene Collection switching while a topology-bearing output is active. Tell the user to stop and restart the relevant output.
- Allow volume and mute changes during active output. A source eligible at startup may mute and resume immediately. A source muted or `Monitor Only` at startup cannot join until the next topology-bearing session.
- Allow source creation, duplication, deletion, and capture-target changes during active output. New identities are marked for the next session; deletion leaves the current stream slot intact; retargeting preserves identity.
- Assign each output its own Output Topology Validation. A required track or muxer initialization failure rejects that output and names the failed track, container, and underlying error.
- Never silently drop an Editor Track, fall back to a partial topology, change container, split the file, or restart another output.
- Validate a second topology-bearing output against the already frozen route. Failure leaves existing outputs untouched. Compatible outputs share the frozen routing snapshot and unified bitrate.
- Emit `Mix` plus non-empty Editor Tracks for Hybrid MP4, MP4, MOV, MKV, and TS. Mark only `Mix` as the default/enabled playback audio stream where the container supports that distinction.
- Emit only `Mix` for FLV and M3U8/HLS while preserving the user's Editor Track routing for later compatible outputs.
- Use capability detection for custom FFmpeg. Known single-track formats use `Mix`; unknown or incompatible formats reject startup rather than guessing.
- Keep normal recording and Replay Buffer on the same topology rules. Do not redesign Replay Buffer's container implementation as part of this feature.
- Stream through Compatibility Mix. Expose VOD Track only for services that support it, select it from one Editor Track, and hide it for Bilibili.
- Reject VOD-enabled startup if the selected Editor Track has no eligible source. Do not fall back to `Mix`. Temporary mute after startup keeps the encoder and sends silence.
- Do not migrate legacy mixer bitmasks or pure-audio Scene Items at runtime. This breaking release installs the packaged Scene Collection and Audio Routing Configuration through the major-update reset.
- Existing User Data Roots require explicit reset confirmation. Rejecting confirmation remains on the installer page and performs no installation writes; fresh installs skip the reset page.
- After packaged defaults are deployed, restore only stream settings, recording paths, hotkeys, and Replay Buffer settings. Do not restore legacy scenes, sources, routing, dock layout, video, audio, or recording encoder settings.
- Provide all new labels, errors, status text, accessible names, and service-specific controls in `en-US`, `zh-CN`, `zh-TW`, `ja-JP`, and `ko-KR`.

## Testing Decisions

- Tests verify external routing state, emitted media artifacts, user-visible errors, and lock behavior. They must not assert private widget layout or internal helper calls.
- Use two primary seams: a focused C++ Audio Routing model test and the existing Windows application-audio smoke harness that launches a real portable OBS-PT process.
- The model seam covers Scene Collection serialization round trips, routing-model versioning, Source Identity behavior, fixed Track Roles, name validation, source ordering, muted-state restoration, `Monitor Only`, Desktop Audio authority, output-lock commands, and topology derivation.
- The portable-runtime seam verifies that a pure-audio source exists outside every Scene Item, remains active, and round-trips with its Source Identity and route. The installer seam verifies fresh-install page skipping, mandatory existing-config confirmation, and the four preserved categories.
- The model seam follows the existing standalone model-test pattern and builds only when project tests are enabled.
- The real-output seam extends the existing deterministic Windows tone harness instead of creating another OBS launcher.
- The real-output seam creates distinguishable Game, Music, and input-device frequencies, starts OBS-PT hidden, records through real encoders and muxers, and inspects files with `ffprobe` plus signal analysis.
- Verify exact stream count, codec, metadata name, stream order, default disposition, sample rate, bitrate, duration, and expected tone presence for every emitted track.
- Verify that `Mix` contains the correct union while Editor Tracks contain only their assigned sources, with and without Desktop Audio authority.
- Verify explicit Desktop Audio mute fallback and unmute de-duplication. Verify device disconnect does not trigger an authority switch.
- Verify inactive, hidden-scene, late-starting, disconnected, and actually silent sources remain structurally eligible when explicitly assigned.
- Verify a truly source-less track and a track containing only muted or `Monitor Only` sources are absent from local output.
- Verify a source eligible at startup can mute and resume without changing stream count. Verify a source excluded at startup remains pending until the next session.
- Verify local video-only output when all sources are muted at startup, including Hybrid MP4 finalization and ordinary MP4/MKV probing.
- Verify live output retains a silent AAC stream while every source is muted and resumes audio without reconnecting. Use a local controllable streaming endpoint for automation; reserve real Bilibili confirmation for manual acceptance.
- Verify the full standard-container matrix: Hybrid MP4, MP4, MOV, MKV, and TS multi-track; FLV and M3U8/HLS Mix-only.
- Verify custom FFmpeg known single-track fallback, supported multi-track output, unknown-capability rejection, and clear error propagation.
- Verify any required track initialization failure aborts that output without leaving a misleading partial artifact.
- Verify a failed second output does not stop the first. Verify compatible recording and Replay Buffer share topology; verify incompatible second-output startup is rejected.
- Verify VOD service visibility, empty-track startup rejection, no Mix fallback, and temporary post-start mute through the model seam plus a controllable service/output integration test.
- Verify both Simple Output and Advanced Output derive the same topology and no obsolete output-track option can override it.
- Verify the one global `160 Kbps` default and user-selected bitrate reaches every encoder. Verify active-output locking prevents bitrate changes.
- Preserve the existing high-frame qualification checks. The final real-output matrix includes the default 480 FPS path and fails on rendering lag, encoding lag, A/V drift, insufficient resource headroom, or track-count regression.
- Perform manual UI acceptance for docking/floating/layout persistence, table drag and ordering, source inspector controls, lock/pending states, menus, focus order, keyboard-only operation, and screen-reader announcements.
- Perform manual locale acceptance in all five supported locales. Check that long labels fit without overlap and file metadata remains stable after language changes.
- Perform manual playback acceptance with a common Windows player and VLC to confirm `Mix` is selected by default.
- Perform VEGAS import acceptance to confirm all streams retain file order and that the first audio stream, `Mix`, can be muted and removed. Verify metadata names independently; do not require VEGAS to expose container names on timeline tracks or hide non-default streams.
- Do not add pixel-level GUI automation; it would be brittle and would test rendering details rather than routing behavior.

## Out of Scope

- Automatically hiding or muting `Mix` in VEGAS or any other NLE.
- Automatically naming VEGAS timeline tracks when VEGAS does not expose container stream names to its importer.
- Producing a second editor-only companion file.
- Inferring source roles from executable names, window titles, current audio level, signal content, or machine-learning classification.
- Assigning one source to multiple Editor Tracks.
- Letting users reorder Track 1-6 or rename Track 1.
- Per-track codec, bitrate, channel layout, or sample-rate controls.
- HLS alternate-audio master-playlist generation; M3U8 remains Mix-only.
- Redesigning Replay Buffer storage or container internals beyond consuming the unified topology.
- Splitting or restarting recordings automatically when routing changes are requested.
- Dynamically changing Desktop Audio authority after device disconnect or reconnect.
- Separating multiple windows or tabs inside one browser process when the OS process tree cannot distinguish them.
- Exposing private transition or plugin-internal audio as Editor Tracks unless the plugin publishes a normal Source Identity.
- Automatically correcting monitoring feedback by changing user routing.
- Replacing the main mixer, source filters, or existing per-source volume controls.

## Further Notes

- The accepted architecture is recorded in ADR 0001, `Unified Audio Routing With Player-First Mix`.
- A container probe confirmed that MP4 default disposition can select `Mix` for players but does not prevent VEGAS from importing non-default Editor Tracks. This is the basis for the explicit player-first trade-off.
- An isolated VEGAS Pro 23 import confirmed that all three sample streams retain order and can be edited, while VEGAS leaves timeline track names empty even for MP4 `handler_name` and MKV `title` controls. File metadata names and fixed order are therefore the supported identification contract.
- The feature must preserve OBS-PT's 480 FPS baseline. Audio simplification is not allowed to reduce default frame rate, resolution, recording quality, or encoder performance settings.
- The PRD intentionally separates routing semantics from output encoding parameters: Scene Collections own Audio Routing Configuration; Profiles own output path, format, and Unified Audio Bitrate.
- Replay Buffer participates in the routing contract but can be scheduled after normal recording is stable if implementation risk requires staged delivery.
