# OBS-PT Audio Routing Usability and Shipped Defaults

## Problem Statement

OBS-PT's unified Audio and Routing workflow is functional, but several remaining upstream interaction patterns still make it look inconsistent or behave ambiguously.

The pure-audio creation dialog still exposes `Add Existing`, even though every manageable audio source already appears exactly once in Audio and Routing. In this model, putting an existing source on another Editor Track is a route reassignment, not the creation of another source reference. Presenting that operation inside the creation dialog makes users unsure whether they are creating, duplicating, or moving a source. The dialog also commits a newly created source before its property editor is accepted, so canceling can leave an unwanted source and route behind.

Track-aware creation, command availability, destructive styling, process-capture status, and output-setting alignment also remain inconsistent. A selected Editor Track does not reliably seed the creation route, move commands remain enabled when they cannot change ordering, process-name capture can keep stale window fields, and a running process may continue to display a waiting state because the properties view is not refreshed when sessions change.

Finally, the packaged default configuration does not yet represent the approved Audio and Routing layout and common PotPvP split-audio workflow. A fresh or reset installation should start from the current approved dock layout with deterministic Minecraft and music Application Audio Sources, without leaking machine-specific paths, hotkeys, stale window selectors, or random source identities.

## Solution

OBS-PT will make Audio and Routing the only place where an existing manageable audio source is reassigned. Pure-audio creation will have one unambiguous mode: create a new source, select its initial route, configure its properties, and retain it only after the property editor is accepted. The upstream `New / Add Existing` mode selector and existing-source list will not appear for collection-owned pure-audio creation. Existing visual-source creation remains unchanged.

When a user begins pure-audio creation while Track 2-6 is selected, that Editor Track is proposed as the initial route and may still be changed before acceptance. Compatibility Mix, the Muted Sources Group, and no selection do not become creation routes. Existing source-type defaults continue to apply when no eligible Editor Track supplies a proposal, while Application Audio Sources still require an explicit Track 2-6 route and Desktop Audio remains fixed to Compatibility Mix.

Audio and Routing commands will reflect the selected source's real position. Existing sources move between Editor Tracks through drag-and-drop or `Move to Track`, retain their Source Identity and properties, and disappear from their old track without creating a copy. Move-up, move-down, and removal presentation will match the actual operation and current theme.

Process-name Application Audio Source properties will normalize mutually exclusive targets and refresh their capture state from actual matching audio sessions. The properties view will distinguish waiting, capturing, and permission failure without inferring activity from instantaneous audio level.

The packaged defaults will adopt the approved current GUI layout and seed stable process-name sources for Minecraft and background music. Deployment will continue to preserve only stream settings, recording paths, hotkeys, and Replay Buffer settings from an existing installation.

## User Stories

1. As a user creating a pure-audio source, I want one creation path, so that I do not have to decide between confusing upstream `New` and `Add Existing` meanings.
2. As a user, I want `Add Existing` absent from pure-audio creation, so that I do not accidentally expect a second reference to an already routed source.
3. As a visual-scene user, I want existing visual-source reuse to remain available, so that the audio-specific simplification does not change visual composition workflows.
4. As a user, I want every manageable audio source displayed exactly once in Audio and Routing, so that its ownership is unambiguous.
5. As a user, I want to drag an existing source to another Editor Track, so that route reassignment happens where the tracks are visible.
6. As a keyboard or menu user, I want `Move to Track` to perform the same reassignment as dragging, so that drag-and-drop is not required.
7. As a user moving a source, I want it removed from its old Editor Track, so that one Source Identity never receives two Editor Track assignments.
8. As a user moving a source, I want its name, capture properties, mute state, monitoring state, and Source Identity preserved, so that routing does not recreate the source.
9. As a user creating audio under a selected Editor Track, I want that track proposed in the creation dialog, so that the common action takes fewer steps.
10. As a user, I want to change the proposed Editor Track before confirming, so that the selection is a convenience rather than a forced route.
11. As a user with Compatibility Mix selected, I do not want it treated as an editable creation route, so that the fixed player-compatibility contract remains intact.
12. As a user with the Muted Sources Group selected, I do not want a new source silently created muted, so that creation intent remains explicit.
13. As a user with no eligible track selected, I want existing source-type route rules to remain deterministic, so that source creation still behaves predictably.
14. As a user creating an Application Audio Source, I want an explicit Track 2-6 selection required before acceptance, so that process identity never guesses routing intent.
15. As a user creating Desktop Audio, I want it fixed to Compatibility Mix, so that system playback is not duplicated into an Editor Track.
16. As a user canceling a new source property editor, I want the new source and route completely discarded, so that cancel means no configuration change.
17. As a user closing a new source property editor with the window close button, I want the same rollback as Cancel, so that dismissal behavior is consistent.
18. As a user canceling creation, I do not want an undo record or save/discard prompt, so that an uncommitted source does not enter normal history.
19. As a user editing an existing source, I want Cancel to restore its prior properties without deleting it, so that creation rollback does not alter existing-source semantics.
20. As a user viewing a one-source Editor Track, I want both reorder commands disabled, so that the UI does not offer no-op actions.
21. As a user viewing the first source in a multi-source track, I want Move Up disabled and Move Down enabled, so that command state reflects real ordering.
22. As a user viewing the last source in a multi-source track, I want Move Down disabled and Move Up enabled, so that command state reflects real ordering.
23. As a user viewing a middle source, I want both reorder commands available, so that I can place it precisely.
24. As a user managing muted sources, I want the same ordering rules in the Muted Sources Group, so that active and muted lists behave consistently.
25. As a user removing an audio source, I want the removal command shown as destructive, so that deletion is visually distinct from movement and properties.
26. As a user switching themes, I want Audio and Routing toolbar icons to remain legible, so that a dark-theme correction does not break light themes.
27. As a user, I want the redundant Audio and Routing button removed from the mixer footer, so that routing has one dock and menu entry rather than duplicated controls.
28. As a user configuring output, I want the audio bitrate label aligned with Output Mode, so that the settings form reads as one consistent layout.
29. As a user targeting an executable by process name, I want the saved executable target independent from a previously selected window, so that hidden stale window data cannot affect capture.
30. As a user switching back to window matching, I want stale executable data removed from the active configuration, so that mutually exclusive match modes remain deterministic.
31. As a user whose configured application is not running, I want a waiting state, so that the saved route remains visible and ready.
32. As a user whose matching application starts later, I want the properties status to change to capturing automatically, so that I do not need to reopen properties or restart OBS-PT.
33. As a user whose matching application exits, I want the properties status to return to waiting automatically, so that the displayed state remains truthful.
34. As a user whose matching session cannot be identified due to permissions, I want a clear permission state, so that failure is not confused with an absent process.
35. As a user, I want capture status based on whether a matching audio session is established, so that temporary silence does not make a valid source look disconnected.
36. As a fresh-install user, I want the approved Audio and Routing dock layout and statistics panel visible by default, so that the intended workflow is available immediately.
37. As a PotPvP recorder, I want a preconfigured `minecraft-sound` Application Audio Source targeting `javaw.exe` on Track 2 `Game`, so that game audio separation works without setup.
38. As a recorder who plays background music, I want a preconfigured `music` Application Audio Source targeting `vivaldi.exe` on Track 3 `Music`, so that music is ready for independent editing.
39. As a user starting OBS-PT before Minecraft or Vivaldi, I want both configured sources retained in a waiting state, so that later process startup activates them automatically.
40. As a fresh-install user, I want seeded sources to use stable OBS-PT Source Identities, so that packaged routing does not depend on one development machine's random identifiers.
41. As a fresh-install user, I want seeded process sources free of stale window title, class, and executable triples, so that only the approved process matcher controls them.
42. As an existing user receiving the major configuration reset, I want stream settings, recording paths, hotkeys, and Replay Buffer settings preserved, so that operational personal values survive.
43. As an installer user, I do not want a developer machine's recording path or hotkeys installed as defaults, so that the packaged configuration remains portable.
44. As a QA engineer, I want the routing workflow verified through the actual UI, so that tests cover what users can create, cancel, move, and see.
45. As a QA engineer, I want process capture verified through real audio sessions and output, so that a status label cannot pass while recording remains silent.
46. As a QA engineer, I want packaged defaults and preservation verified through installer deployment, so that source JSON and UI state are not validated in isolation.

## Implementation Decisions

- Treat this PRD as an incremental usability and packaging layer on the accepted Unified Audio Routing and executable-wide Application Audio Source contracts. Compatibility Mix, Editor Track numbering, Source Identity ownership, and Recording Topology remain unchanged.
- In collection-owned pure-audio creation only, remove the complete upstream `New / Add Existing` mode selector and existing-source list. Retain the upstream behavior for visual Scene Items.
- Make the pure-audio creation dialog responsible only for naming a new source and choosing its initial route. Existing-source reassignment belongs exclusively to Audio and Routing.
- Represent reassignment as one routing-model mutation. Moving an existing source changes its single Editor Track and optional position while preserving the same Source Identity and OBS Source instance.
- Use a selected Track 2-6 as the highest-priority initial-route proposal for every new pure-audio source. Mix, Muted Sources Group, and non-source selections do not provide a proposal. The user may change a proposed route before acceptance.
- Preserve established source-type rules when no eligible selection supplies a proposal. Application Audio Sources require an explicit Editor Track, audio-input sources default to `Microphone`, unknown pure-audio sources retain their deterministic fallback, and Desktop Audio remains Mix-only.
- Treat new-source creation as a transaction spanning the creation dialog and first property editor. Commit the source, collection ownership, route, persistence, and undo history only when properties are accepted.
- Canceling or closing the first property editor removes every provisional source and routing artifact without an undo record or secondary confirmation. Existing-source property editing retains its current rollback behavior and never deletes the source.
- Derive reorder command availability from the source's actual index and group size for both Editor Tracks and the Muted Sources Group. Apply the same state to toolbar buttons, keyboard actions, and context menus.
- Present removal as a destructive action and use theme-aware icons through the existing Qt theme mechanism rather than hard-coded light or dark artwork.
- Remove only the duplicate mixer-footer Audio and Routing button. Keep the dock, menu command, and accepted navigation behavior.
- Align the unified audio bitrate label using the same form-label geometry as Output Mode without changing bitrate storage or output locking.
- Normalize mutually exclusive Application Audio Source match fields whenever settings are committed: process-name mode retains the executable and removes the window target; window modes retain the window target and remove the executable target.
- Define `capturing` as one or more established matching audio sessions, independent of instantaneous level. Define `waiting` as a configured target with no current matching session, and retain a distinct permission failure state.
- Refresh open source properties when process-session reconciliation changes the established root count or permission state. Refreshes must not rewrite the user's current target or require reopening the dialog.
- Package the approved current GUI and dock state after filtering machine-specific operational values. Seed `minecraft-sound` on Track 2 and `music` on Track 3 with process-name targets, stable project-owned Source Identities, and no inactive window-selector fields.
- Continue the accepted major-update reset contract: packaged layout, sources, routing, video, audio, and encoder configuration replace prior values; only stream settings, recording paths, hotkeys, and Replay Buffer settings are merged back.
- Keep new or changed presentation text complete in the shipped locales. Runtime UI Automation qualification is limited to `zh-CN` by the final test decision.

## Testing Decisions

- Prefer user-visible behavior over internal implementation assertions. A test should prove the resulting source list, route, capture status, recording content, or deployed configuration rather than private widget or callback structure.
- Use the existing Windows Audio and Routing UI Automation smoke for the `zh-CN` installed layout, transactional creation and rollback, occupied-track proposal, process selector, routing movement, persistence, and accessibility behavior that can be driven without foreground activation.
- Use native creation/routing model tests, Qt table tests, source audits, and real-output smoke for invalid selection classes, permission transitions, command-state, and ownership cases that cannot be driven reliably through background UI Automation.
- Use the existing real Windows Application Audio smoke as the primary process-capture seam. It should prove audible output for deterministic executable targets and cover late start, exit, restart, and stale-window-field normalization.
- Extend the executable-session model test only for deterministic session-count, reconciliation, and permission transitions that are impractical to force through a real process. The properties presentation must still be covered at the UI seam.
- Use the existing installer configuration-reset smoke as the packaged-default seam. It should verify the approved dock state, stable seeded sources and routes, process-only targets, absence of development-machine paths and hotkeys, and preservation of the four accepted user categories.
- Run the existing routing model, player compatibility, real-output, `zh-CN` UI, and installer regressions after the focused tests. VEGAS automation is excluded because it creates visible foreground work and is not required for player-first compatibility.
- Runtime automation must keep OBS-PT out of the foreground, use non-activating window operations and posted messages rather than real keyboard or mouse input, reject unexpected existing instances by default, and terminate every helper process it starts.
- Final qualification retains production 480 FPS, resolution, quality, and encoder defaults. No verification may lower product settings to hide capture or performance defects.

## Out of Scope

- Making Compatibility Mix optional or introducing the future single-track/multi-track quick switch.
- Changing Recording Topology, player-first Mix behavior, output container support, Replay Buffer behavior, streaming, or VOD routing.
- Dynamically adding or removing Editor Tracks based on process presence, audio level, or whether a configured source is currently audible.
- Separating individual browser tabs or windows that share one executable identity.
- Changing Game Capture or Window Capture to use audio-session aggregation.
- Migrating incompatible pre-reset source or routing configuration at runtime.
- Removing `Add Existing` from visual Scene Item workflows.
- Publishing, replacing, or otherwise modifying a GitHub Release.

## Further Notes

- The existing routing and executable-capture issue batches are complete; this PRD records the concrete usability defects found during post-qualification use rather than reopening their architectural decisions.
- A configured but absent `javaw.exe` or `vivaldi.exe` remains an intentional eligible source. Its Editor Track can therefore appear in output according to the existing frozen-topology rules even while the process is not running.
- The term `Add Existing` is deliberately avoided in Audio and Routing. The canonical operation is moving or reassigning an existing source to an Editor Track.
- `CONTEXT.md` now records that each manageable source appears once and that existing-source reassignment belongs exclusively to Audio and Routing.
