## Problem Statement

The Audio and Routing Panel currently has two severe Qt lifetime defects. Using
`Move to Track` closes the context menu and then crashes OBS-PT. Native dragging
can also crash, or can remove a newly rebuilt track row from the presentation
until restart even though the Audio Routing Configuration was persisted
correctly. Muting, unmuting, reassignment, ordering, selection, and presentation
refreshes are performed as multiple overlapping operations, so a single user
command can rebuild the item view while Qt still owns menu, drag, item, or model
index state.

The same panel also exposes ambiguous interaction boundaries. The first source
shares a row with its Editor Track anchor, Compatibility Mix sources appear
draggable even though their assignment is fixed, command buttons are separated
from the properties button they depend on, and the dock exposes an unwanted
close control. A future single-track/multi-track recording switch needs a clear
place in the panel without pretending that the unimplemented mode already
works.

## Solution

Make every Audio Routing Configuration command operate on copied Source
Identity and target values as one atomic transition. Rebuild the Audio and
Routing presentation once, only after any nested Qt menu or drag interaction has
fully ended. Keep all six numbered tracks and the Muted Sources Group visible,
and preserve the moved source selection across every successful transition.

Separate track selection from source selection by cell: the track and name
columns select an Editor Track, while the source column selects and manipulates
the source shown in that cell. Lock Compatibility Mix assignment while retaining
the valid property and mute controls for Desktop Audio. Reorganize the toolbar,
reserve a disabled right-aligned placeholder for the future multi-track recording
mode, and remove only the Audio and Routing dock's close button.

## User Stories

1. As an OBS-PT user, I want `Move to Track` to complete without crashing, so that I can manage routes through the context menu.
2. As an OBS-PT user, I want native dragging to complete without crashing, so that direct manipulation is reliable.
3. As an OBS-PT user, I want a successful route change visible immediately, so that I do not need to restart the application.
4. As an OBS-PT user, I want Track 1-6 and the Muted Sources Group to remain visible after every move, so that no destination appears to vanish.
5. As an OBS-PT user, I want a moved source to remain selected, so that I can continue operating on it without finding it again.
6. As an OBS-PT user, I want one route command to produce one state transition, save, and presentation refresh, so that reentrant callbacks cannot partially apply it.
7. As an OBS-PT user, I want a failed route command to leave both routing and presentation unchanged, so that failures cannot expose partial state.
8. As an OBS-PT user, I want `Move to Track` to append the source to the destination Editor Track, so that menu movement has deterministic ordering.
9. As an OBS-PT user, I want dragging to insert the source at the indicated destination position, so that I can choose exact ordering.
10. As an OBS-PT user, I want moving an active source to its current Editor Track to be a no-op, so that an accidental selection does not save or refresh.
11. As an OBS-PT user, I want the current Editor Track shown as selected and disabled for an active source, so that the menu communicates the no-op destination.
12. As an OBS-PT user, I want muting a source to preserve its prior Editor Track and position, so that muting does not discard routing intent.
13. As an OBS-PT user, I want unmuting a source to return it to its prior Editor Track and position, so that ordinary mute control is reversible.
14. As an OBS-PT user, I want an atomic move from the Muted Sources Group to an Editor Track to reassign and unmute the source, so that one command reaches the requested destination.
15. As an OBS-PT user, I want the prior Editor Track shown as selected but actionable for a muted source, so that choosing it visibly restores the source.
16. As an OBS-PT user, I want Monitor Only state preserved through mute, unmute, and route movement, so that routing does not alter monitoring intent.
17. As an OBS-PT user, I want recording topology locks to continue blocking unsafe route movement with the existing stop-and-restart guidance, so that active output files remain stable.
18. As a streamer, I want mute and unmute to remain available during streaming according to the existing frozen-topology contract, so that private audio can still be suppressed.
19. As an OBS-PT user, I want Mix and its name fixed, so that Compatibility Mix remains the stable default playback stream.
20. As an OBS-PT user, I want Desktop Audio in Mix selectable for properties and mute control, so that fixed assignment does not remove useful controls.
21. As an OBS-PT user, I do not want Desktop Audio in Mix to be draggable, reassignable, reorderable, or removable, so that fixed Compatibility Mix ownership cannot be violated.
22. As an OBS-PT user, I want muted Desktop Audio to remain ineligible for Editor Tracks and deletion, so that muting cannot bypass Mix ownership.
23. As an OBS-PT user, I want muted Desktop Audio reorderable within the Muted Sources Group, so that the management list remains orderly.
24. As an OBS-PT user, I want unmuted Desktop Audio to return to Mix, so that its fixed destination remains deterministic.
25. As an OBS-PT user, I want clicking an occupied track's track or name cell to select the track, so that its first source does not steal track selection.
26. As an OBS-PT user, I want clicking the source cell to select the source, so that source commands have an explicit target.
27. As an OBS-PT user, I want source context menus to open only from source cells, so that track cells do not expose unrelated source commands.
28. As an OBS-PT user, I want dragging to start only from an eligible source cell, so that track headers and Mix cannot accidentally initiate route movement.
29. As an OBS-PT user, I want Delete, Properties, Move Up, and Move Down disabled when no source is selected, so that the toolbar reflects command applicability.
30. As an OBS-PT user, I want Properties enabled for a selected Mix source while Delete and ordering remain disabled, so that the Mix lock is visible in command state.
31. As an OBS-PT user, I want Move Up and Move Down disabled for a one-source group, so that no-op ordering commands are unavailable.
32. As an OBS-PT user, I want Move Up and Move Down enabled only where the selected source can actually move, so that boundary state remains accurate.
33. As an OBS-PT user, I want Add, Delete, Properties, Move Up, and Move Down grouped on the left, so that related source commands are adjacent.
34. As an OBS-PT user, I want Move Up and Move Down immediately to the right of Properties, so that ordering controls no longer float at the opposite edge.
35. As an OBS-PT user, I want a right-aligned multi-track recording placeholder, so that the future primary mode control has a stable location.
36. As an OBS-PT user, I want the placeholder visibly disabled and non-persistent, so that it cannot imply a mode change that does not exist.
37. As an OBS-PT user, I want the placeholder to use the approved green settings icon and an accessible tooltip, so that its future purpose is identifiable.
38. As an OBS-PT user, I do not want a close button on the Audio and Routing dock, so that the primary routing workspace is not dismissed accidentally.
39. As an OBS-PT user, I want the dock's float/dock control retained, so that I can use it as a separate small window.
40. As an OBS-PT user, I want the dock to remain movable and restorable through the layout system, so that removing Close does not freeze its placement.
41. As an OBS-PT user, I want the View menu to retain manual Audio and Routing visibility control, so that hiding and recovering the dock remains possible.
42. As an OBS-PT user, I want locking and unlocking the main UI to preserve the dock's no-Close contract, so that feature recomputation cannot restore the unwanted button.
43. As an OBS-PT user, I want destructive source removal to remain visually distinct without destabilizing the application style, so that safety does not cause another crash.
44. As a QA engineer, I want context-menu creation, action triggering, destruction, and subsequent main-menu use covered together, so that borrowed-style deletion is caught.
45. As a QA engineer, I want a full Qt drag lifetime covered through `startDrag()` completion, so that a direct `dropEvent()` test cannot conceal post-drop cleanup.
46. As a QA engineer, I want active, muted, Desktop Audio, same-track, cross-track, and reorder transitions covered, so that every atomic route path is verified.
47. As a QA engineer, I want selection and all seven group rows asserted immediately and after restart, so that presentation-only corruption is detected.
48. As a QA engineer, I want the installed EXE used for reproduction tied to the matching build and symbols, so that crash evidence cannot be dismissed as a stale install.
49. As a user running other foreground work, I want runtime verification to remain non-activating, so that OBS-PT tests do not steal focus or send real input.
50. As a PotPvP recorder, I want all qualification to retain the production 480 FPS, resolution, quality, and routing defaults, so that a UI fix cannot conceal a recording regression.

## Implementation Decisions

- Treat the menu and drag failures as two independent Qt lifetime defects even
  though they affect the same user workflow.
- Never give a borrowed widget or application style to an owning proxy style.
  Preserve the destructive-action presentation through an independently owned
  style or a non-owning rendering mechanism.
- Route commands consume copied Source Identity, destination track, destination
  Source Identity, and relative position values. Qt item pointers and model
  indexes never survive beyond the immediate event extraction step.
- Represent route assignment, optional unmute, and optional position change as
  one atomic routing-model transition. The transition either commits completely
  or leaves the prior model unchanged.
- Coalesce model-initiated, mute-signal, mixer, persistence, and presentation
  work so one user command produces one effective save and one Audio and Routing
  rebuild.
- Delay the presentation rebuild until nested menu and drag dispatch has ended.
  The model transition remains immediate; only unsafe presentation destruction
  is deferred.
- Preserve Source Identity, source instance, capture properties, mute state when
  applicable, monitoring state, collection ownership, and user-visible name.
- Keep all fixed group anchors present on every rebuild, including empty Editor
  Tracks and an empty Muted Sources Group.
- Preserve the selected Source Identity and restore its source-cell selection
  after a successful rebuild. Track-cell selection remains a track selection.
- Use source-column eligibility for source commands, context menus, and drag
  initiation. Track and name columns retain their creation-route proposal role.
- Disable the active source's current Editor Track destination. For a muted
  source, retain its prior track indicator while allowing that destination to
  perform unmute and restoration.
- Append menu moves to the target track, use the drag target for exact insertion,
  and preserve the prior position for plain unmute.
- Maintain the fixed Desktop Audio contract across both Mix and the Muted Sources
  Group. Muting changes management group, not destination eligibility.
- Arrange source toolbar commands as Add, Delete, Properties, Move Up, and Move
  Down before the expanding spacer. Place the disabled future-mode placeholder
  after the spacer.
- The multi-track recording placeholder is disabled, does not read or write
  configuration, and does not alter Recording Topology.
- Permanently remove the Audio and Routing dock's Closable feature while
  preserving Floatable, Movable, layout persistence, and the View-menu toggle.
  UI lock-state recomputation must reapply this special contract.
- Preserve all accepted topology-lock, output, streaming, VOD, player-first Mix,
  and major-update reset behavior.

## Testing Decisions

- Tests assert user-visible state, durable routing state, process survival, and
  absence of unexpected saves rather than private callback counts alone.
- Extend the routing-model seam with atomic move scenarios covering append,
  exact insertion, same-track no-op, unmute restoration, muted reassignment,
  Desktop Audio rejection, ordering, and rollback.
- Replace the incomplete direct-drop evidence with a Qt interaction test that
  covers completion of the full drag lifetime and verifies that Qt performs no
  second presentation removal afterward.
- Add a Qt menu-lifetime regression that creates the routing menu, triggers an
  action, destroys the temporary menu, and then exercises the main menu bar or
  shared style. Process survival is a required assertion.
- Cover track/name versus source-cell selection and corresponding command,
  context-menu, and drag eligibility at the Qt presentation seam.
- Extend the existing Windows Audio and Routing UI Automation smoke only for
  behavior that can be driven without foreground activation: toolbar state,
  row persistence, source selection restoration, menu movement, dock controls,
  placeholder state, and restart persistence.
- Runtime UI Automation is limited to `zh-CN`. Tests use non-activating window
  operations and posted messages, never real keyboard or mouse input.
- Native OLE drag is not falsely claimed by message-level UI Automation. Its
  complete lifecycle belongs to the focused Qt test; final human verification
  may exercise actual dragging after the automated suite passes.
- Re-run existing routing, source creation, executable-session, output topology,
  installer preservation, player compatibility, and 480 FPS qualification
  regressions after focused tests.
- Build and package only with the approved VS2022 v143 toolchain. Refresh staging
  with tests disabled if executable behavior changes, without launching visible
  setup or application windows during unattended verification.

## Out of Scope

- Implementing single-track recording mode or the functional single-track/
  multi-track quick switch.
- Making Compatibility Mix optional.
- Changing Recording Topology, output container support, Replay Buffer,
  streaming, VOD, or player-first default-stream behavior.
- Changing executable-wide Application Audio Source capture or session
  reconciliation.
- Changing source creation, default seeded sources, track roles, encoder values,
  resolution, frame rate, recording quality, or installation reset policy.
- Publishing or modifying a GitHub Release.
- Visible foreground automation or real keyboard/mouse injection.

## Further Notes

- Eleven crash reports from the installed 1.1.0 build form two stable signatures:
  eight at `Qt5Widgets + 0x168216` inside `QMenuBar::eventFilter`, and three at
  `Qt5Widgets + 0x29282E` inside `QTreeView::mousePressEvent`.
- The installed executable, build executable, and staged executable have the same
  SHA-256, ruling out a stale installed binary for the reproduced failures.
- Qt 5.15 documents that `QProxyStyle(QStyle *)` takes ownership of the supplied
  base style, while `QWidget::setStyle()` does not take ownership. The existing
  temporary context menu violates that ownership boundary by passing its borrowed
  shared style into the proxy.
- The prior drag test directly invoked `dropEvent()` and therefore did not cover
  cleanup after the nested `startDrag()` call returned. Its PASS result was not
  evidence for native drag safety.
- Restart recovery confirms that the disappearing-track symptom is presentation
  corruption rather than lost Audio Routing Configuration.
