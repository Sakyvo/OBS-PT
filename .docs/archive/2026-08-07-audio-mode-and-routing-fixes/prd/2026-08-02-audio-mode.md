# Audio Mode: single-track and multi-track recording

## Problem Statement

OBS-PT ships a six-track Audio Routing Configuration built for PotPvP editing:
Compatibility Mix on Recording Track 1 plus five Editor Tracks carrying game
audio, music, microphone, voice chat and aux separately, so the editor can
process each one. That layout is the whole point of the product for anyone who
edits their footage.

It is also dead weight for anyone who does not. A user who just wants a
recording they can upload directly still gets six audio streams in the file,
still has to reason about which Application Audio Source belongs to which
Editor Track, and still sees a panel full of routing controls that do not
matter to them. Worse, the routing controls are the only way to understand what
is being recorded, so a user who does not want multi-track cannot simply ignore
them - a misrouted source means a missing sound in the output.

There is currently no way to say "just record everything into one track". The
only approximation is to manually mute every Editor Track source, which
silently changes what is captured, is easy to get wrong, and is tedious to
undo.

## Solution

A top-level Audio Mode switch on the Audio and Routing Panel, showing the
currently active mode rather than an action verb.

In `多音轨` (the default, and what ships preconfigured) nothing changes: the
Compatibility Mix and all five Editor Tracks exist, and the panel behaves as it
does today.

In `单音轨` the concept of an Editor Track disappears entirely. Every input -
system playback, microphone, capture card, media source - mixes into a single
Compatibility Mix, and the recording contains exactly one audio track. The
panel collapses to the Mix row alone: no Editor Tracks, no Muted Sources Group,
nothing to route. The user can mute the Mix, and that shows as the Mix being
muted, not as the Mix moving into some other group.

The two modes hold genuinely separate settings. Switching to single-track does
not destroy the multi-track layout; switching back restores it exactly, down to
which sources the user had muted. A user can therefore try single-track without
any fear of losing a carefully built routing configuration.

## User Stories

1. As a PotPvP player who does not edit, I want to record one audio track, so
   that I can upload the file directly without an editor merging tracks for me.
2. As a PotPvP player who does not edit, I want to switch to single-track in
   one click, so that I do not have to understand Editor Tracks at all.
3. As an editor, I want multi-track to stay the default, so that a fresh
   installation is already set up for my workflow.
4. As a user in single-track mode, I want the microphone included, so that my
   commentary is recorded.
5. As a user in single-track mode, I want game and music audio included, so
   that the recording sounds like what I heard.
6. As a user in single-track mode, I want capture cards and media sources
   included, so that no input is silently dropped.
7. As a user in single-track mode, I want the recording to contain exactly one
   audio track, so that players and upload sites behave predictably.
8. As a user in single-track mode, I want the panel to show only the Mix row,
   so that the interface matches the simplicity I asked for.
9. As a user in single-track mode, I want no Muted Sources Group in sight, so
   that I am not shown a concept that does not apply.
10. As a user in single-track mode, I want to mute the Mix, so that I can
    record video without any audio.
11. As a user in single-track mode, I want muting the Mix to look like the Mix
    being muted, so that I am not confused by a source appearing to move.
12. As a user who built a multi-track layout, I want switching to single-track
    to preserve it, so that experimenting costs me nothing.
13. As a user returning to multi-track, I want every source back on its
    original Editor Track, so that I do not have to rebuild the layout.
14. As a user returning to multi-track, I want my track ordering preserved, so
    that the layout is byte-for-byte what I left.
15. As a user returning to multi-track, I want sources I had deliberately muted
    to still be muted, so that the mode switch does not override my choices.
16. As a user who muted Desktop Audio in multi-track, I want it audible in
    single-track, so that the single track is not silent for a reason I cannot
    see.
17. As a user who muted Desktop Audio in multi-track, I want it muted again
    when I return, so that the automatic un-mute was a mode concern and not a
    permanent edit.
18. As a user, I want the mode to be remembered, so that reopening OBS-PT keeps
    the mode I chose.
19. As a user with several Scene Collections, I want each to remember its own
    mode, so that a collection built for editing and one built for direct
    upload can coexist.
20. As a user switching Scene Collections, I want the mode to switch with it,
    so that the loaded configuration is internally consistent.
21. As a user switching Profiles, I want the mode unchanged, so that encoder
    and output settings stay independent of routing.
22. As a user, I want the switch to show the current mode rather than the
    action, so that I can read the current state without clicking.
23. As a user, I want the mode control in a consistent, findable place, so that
    I do not hunt for it.
24. As a user in single-track mode, I want multi-track commands to tell me why
    they do nothing, so that I am not left thinking the application is broken.
25. As a user in single-track mode, I want that message to point me at the mode
    switch, so that I know exactly how to get the functionality back.
26. As a user, I want the mode switch blocked while recording, so that a
    running recording cannot end up with an inconsistent track structure.
27. As a user, I want the mode switch blocked while streaming, so that a live
    stream is not interrupted by an audio topology change.
28. As a user attempting to switch during recording, I want a clear reason, so
    that I know to stop the recording first.
29. As a user in single-track mode, I want Application Audio Sources to stay
    defined, so that returning to multi-track does not require recreating them.
30. As a user in single-track mode, I do not want application audio duplicated
    on top of system playback, so that sounds are not doubled.
31. As a user whose audio devices all fail, I want the existing fallback to
    still apply, so that single-track mode does not remove the safety net.
32. As an editor who switches to single-track by accident, I want to switch
    back and find everything intact, so that a mis-click is not destructive.
33. As a user, I want the mode to be visible at a glance while recording, so
    that I can confirm what I am about to capture.
34. As a new user, I want the shipped default to be multi-track, so that the
    product's core value is available without configuration.
35. As a user upgrading over an old installation, I want my existing routing
    treated as multi-track, so that nothing changes unexpectedly.

## Implementation Decisions

**Audio Mode belongs to the Audio Routing Configuration.** It is a field of the
routing model, serialised with it, and therefore persisted per Scene
Collection. It is deliberately not a Profile value and not a global preference:
the mode determines whether Editor Tracks exist at all, which is routing
topology, and the glossary already separates routing configuration from Profile
output parameters. Loading a Scene Collection loads its mode; switching
Profiles does not touch it.

**Single-track is defined as "Compatibility Mix only, no Editor Track".** It is
not a new topology, it is the existing Compatibility Mix with nothing else
present. This means the established Mix composition rules apply unchanged,
including the Desktop Audio authority group, the inclusion of microphones and
capture cards, and the Application Audio Source de-duplication and fallback
behaviour. No new mixing semantics are introduced.

**No Profile output setting is modified.** Recording track selection is already
derived dynamically from the routing topology rather than read from a stored
bitmask, so a topology containing only Track 1 produces a single-track file on
its own. The mode switch must not write any output configuration.

**Mode transition is expressed through existing state, not a snapshot.**
Entering single-track suppresses Editor Tracks; it does not rewrite each
source's track assignment. Because assignments are untouched, returning to
multi-track needs no restore step for routing. The one piece of state that does
need remembering is whether Desktop Audio was muted by the user before the
switch, since single-track un-mutes it automatically; that is restored on
return.

**Muted Sources Group does not exist in single-track mode.** Muting the Mix in
single-track sets the Mix's own muted state and is presented as such. The panel
must not render a Muted Sources Group row, and the model must not treat the
single-track mute as a group membership change.

**The panel renders a mode-dependent shape.** In single-track it shows the Mix
row only. Editor Track rows and the Muted Sources Group row are absent rather
than dimmed, since dimmed rows for tracks that do not exist would imply they
could be re-enabled in place.

**Toolbar commands follow ADR-0003.** Commands are never disabled. In
single-track mode, invoking a multi-track command produces a message directing
the user to switch modes. This is the mode-lock case the ADR explicitly
distinguishes from ordinary context unavailability, which stays silent.

**The mode control reuses the existing toolbar mechanism** in its current
bottom-right position, keeps its green treatment, and displays the active mode
as its label rather than an action verb.

**Switching is refused while an output is active.** Recording or streaming
blocks the transition and reports why. The check happens before any state
change so a refused switch is a no-op.

**Upgrade behaviour:** a routing configuration serialised without a mode field
loads as multi-track, so existing installations see no change.

## Testing Decisions

A good test here asserts what the user would observe - which tracks the
recording contains, what the panel shows, what survives a round trip - and not
how the model stores its state. Tests must not assert on private fields or on
the order of internal operations, so that the transition can be reimplemented
without rewriting them.

**Primary seam: the routing model.** This is the highest available seam and
already has the largest existing test in the codebase, which links only
Qt5::Core and needs no libobs, no windows, and no real audio devices. Everything
substantive lives here:

- Deriving the local and streaming topology in single-track mode yields exactly
  one track, and it is the Compatibility Mix.
- Multi-track topology is unchanged by the presence of the new field.
- Every input kind - system playback, microphone, capture card, media source -
  is present in the single-track Mix.
- Application Audio Sources do not double system playback in single-track mode,
  and the existing fallback still applies when the authority group is
  unavailable.
- A full round trip - build a multi-track layout, switch to single-track,
  switch back - restores track assignment, intra-track ordering and
  user-chosen mute states exactly.
- Desktop Audio muted by the user is audible in single-track and muted again on
  return.
- Sources the user muted deliberately remain muted across a round trip.
- Serialisation round-trips the mode, and a payload without the field loads as
  multi-track.

**Secondary seam: the panel widgets.** The existing offscreen widget tests
cover presentation, using the established pattern of rendering without showing
a window or sending real input:

- Single-track mode renders the Mix row only, with no Editor Track rows and no
  Muted Sources Group row.
- The mode control's label reflects the active mode.
- A multi-track command invoked in single-track mode reports the mode-switch
  message rather than acting silently.

**Not covered by automated tests:** whether a produced recording file actually
contains one audio stream. That requires a real encode and is left to the
acceptance package.

Prior art: the routing model test for model-level behaviour, the routing table
test and the toolbar test for offscreen widget behaviour.

## Out of Scope

- Any change to Profile output or encoder settings, including track bitmasks.
- Per-track audio settings, levels, faders, or monitoring behaviour.
- Changing what the Compatibility Mix contains in multi-track mode.
- Deleting, stopping or unloading Application Audio Sources when entering
  single-track mode; they remain defined and simply do not contribute.
- A third mode, or a user-configurable number of Editor Tracks.
- Migrating existing recordings or re-muxing previously captured files.
- Exposing the mode anywhere outside the Audio and Routing Panel, such as the
  Settings dialog or a hotkey.
- Switching modes while an output is running.

## Further Notes

The naming matters more than usual here. `单音轨` describes the resulting
recording, not a reduced feature set, and the switch shows the mode rather than
the transition so that the panel reads as a statement of current state.

The decision to define single-track as "Compatibility Mix only" is what keeps
this feature small. Every alternative considered during design - a parallel
routing configuration, a snapshot-and-restore mechanism, a per-source
"suppressed by mode" flag - introduced state that could disagree with the
routing model. Reusing an existing concept and simply removing the other one
avoids all of it.
