Status: done

## What to build

A source that carried both picture and sound must disappear from the Audio
Routing Configuration once it is removed from every scene. Today it does not:
deleting it from the Visual Composition Panel leaves a routing entry the user
can neither route nor delete, and that entry persists with the Scene
Collection, so it survives a restart and is still counted when the output
topology is derived.

The cause is that removing a source from a scene does not announce the same
event as deleting a source outright, and the reconciliation pass only ever adds
entries - it never drops entries whose source is gone. Reconciliation gains the
missing half: an entry whose source is no longer referenced by any scene is
removed.

Pure audio sources must be exempt. They deliberately have no Scene Item - that
is what lets them keep capturing across scene switches - so judging them by
scene membership would delete exactly the sources the panel exists to manage.

Undo must stay coherent. Restoring a deleted source has to bring its routing
entry back; it does not need to preserve the previous track assignment, since
reconciliation reassigns it the same way a fresh source is assigned.

## Acceptance criteria

- [x] Removing a source that carries both picture and sound from every scene
  removes its Audio Routing Configuration entry.
- [x] The entry is gone from the panel, from the mixer, and from the derived
  output topology.
- [x] The removal survives a save and reload of the Scene Collection - no
  entry reappears.
- [x] A source still present in another scene is not removed, even when it is
  deleted from the current one.
- [x] Pure audio sources are never removed by this pass, despite having no
  Scene Item.
- [x] Sources temporarily hidden or deactivated are not treated as removed.
- [x] Undoing the deletion restores a routable entry for the source.
- [x] No other source's track assignment, intra-track order, or mute state
  changes as a side effect of a cleanup pass.
- [x] Cleanup runs on scene changes rather than on a timer, and repeated runs
  with no change produce no visible churn in the panel.
- [x] Focused coverage runs offscreen, sends no real keyboard or mouse input,
  and shows no window.
- [x] The production VS2022 v143 build succeeds and staging is refreshed with
  tests disabled.

## Blocked by

- `034-video-capture-device-audio-separation.md`

## Implementation

Split across the two layers that can actually own each half. The model gets a
pure pass that drops every entry outside a caller-supplied reachable set - it
has no idea what a scene is, and an empty set correctly means "nothing is
reachable" rather than "skip". That half is fully assertable without libobs,
including that pruning leaves other sources' track assignment, ordering and
mute state alone, and that a second pass with no change drops nothing.

The application layer computes reachability from three sources, because scene
membership alone would delete exactly what the panel exists to manage:

- sources owned as pure audio, which deliberately have no Scene Item
- device sources bound to an output channel, which likewise have none
- every source referenced by any scene, descending into groups

The trigger was the missing piece: scenes announced `item_add`, `reorder` and
`refresh` but never `item_remove`, so nothing downstream could react to a
source leaving. That signal is now handled, queued so the pass runs after the
removal settles.

Hidden or deactivated sources keep their Scene Item, so they are reachable and
survive. Undo re-adds the item, and reconciliation re-registers the source the
same way it would a fresh one.

## Remaining

Foreground-only: deleting a media source from the panel and confirming its
audio row disappears and stays gone after a restart, and that undo brings it
back.
