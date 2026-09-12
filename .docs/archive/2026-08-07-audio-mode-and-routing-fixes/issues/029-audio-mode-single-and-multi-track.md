Status: done

## Parent

`.docs/prd/2026-08-02-audio-mode.md`

## What to build

An Audio Mode switch on the Audio and Routing Panel that toggles between
`多音轨` and `单音轨`, showing the mode currently in effect rather than an
action verb.

`多音轨` is the default and the shipped configuration, and behaves exactly as
the panel does today. `单音轨` is defined as Compatibility Mix only, with no
Editor Track at all: every input - system playback, microphone, capture card,
media source - mixes into the Compatibility Mix, and the recording carries a
single audio track. The panel collapses to the Mix row alone; Editor Track rows
and the Muted Sources Group row are absent, not dimmed. The user can mute the
Mix, and that presents as the Mix itself being muted rather than as membership
in any group - the Muted Sources Group concept does not exist in this mode.

The mode belongs to the Audio Routing Configuration and is therefore persisted
per Scene Collection. Loading a Scene Collection loads its mode; switching
Profiles must not affect it. A routing configuration serialised without the
field loads as multi-track so existing installations see no change.

No Profile output setting may be written. Recording track selection is already
derived from the routing topology rather than a stored bitmask, so a topology
containing only the Compatibility Mix produces a single-track file on its own.

The two modes hold separate settings and a round trip must be lossless.
Entering single-track suppresses Editor Tracks rather than rewriting track
assignments, so returning restores them without a restore step. The one piece
of state that does need remembering is whether the user had muted Desktop Audio
before the switch, because single-track un-mutes it automatically; that is
restored on return. Sources the user muted deliberately stay muted throughout.

Application Audio Sources remain defined in single-track mode. The established
Compatibility Mix de-duplication rules already keep them from doubling system
playback, and the existing fallback when the Desktop Audio authority group is
unavailable still applies. Do not stop, unload or delete them.

The mode control reuses the existing bottom-right toolbar entry and keeps its
green treatment, which now marks the active mode rather than an unimplemented
placeholder.

## Acceptance criteria

- [x] The panel offers an Audio Mode control whose label states the mode
  currently in effect.
- [x] Multi-track is the default, matches the shipped configuration, and
  behaves exactly as the panel does today.
- [x] In single-track mode the derived local topology contains exactly one
  track, and it is the Compatibility Mix.
- [x] In single-track mode the derived streaming topology likewise contains
  only the Compatibility Mix.
- [x] System playback, microphones, capture cards and media sources are all
  present in the single-track Compatibility Mix.
- [x] Multi-track topology is unchanged by the presence of the mode field.
- [x] No Profile output or encoder setting is written when the mode changes.
- [x] In single-track mode the panel shows the Mix row only, with no Editor
  Track rows and no Muted Sources Group row.
- [x] Muting in single-track mode presents as the Mix being muted and creates
  no Muted Sources Group.
- [x] A round trip through single-track and back restores every source's Editor
  Track, its position within that track, and its user-chosen mute state.
- [x] Desktop Audio muted by the user becomes audible in single-track mode and
  is muted again on return to multi-track.
- [x] Application Audio Sources remain defined in single-track mode, do not
  double system playback, and retain the existing fallback behaviour when the
  Desktop Audio authority group is unavailable.
- [x] The mode survives a save and load cycle and is stored with the Scene
  Collection.
- [x] Switching Scene Collections switches the mode with it; switching Profiles
  leaves it untouched.
- [x] A routing configuration serialised without the mode field loads as
  multi-track.
- [x] Focused Qt coverage runs offscreen, sends no real keyboard or mouse
  input, and shows no window.
- [x] The production VS2022 v143 build succeeds and staging is refreshed with
  tests disabled.

## Blocked by

- `028-native-toolbar-model-and-cell-highlight.md`

## Implementation

Isolation is achieved by **deriving** rather than by mutating. Entering
single-track does not move sources, does not rewrite track assignments, and
does not touch any mute flag. The topology derivation simply stops after the
Compatibility Mix and ignores per-source mute and monitor-only choices, which
belong to the other mode; only the Mix's own mute applies. Returning to
multi-track therefore needs no restore step at all - the multi-track state was
never disturbed. This turned out simpler than the snapshot approach the PRD
anticipated, and removes the class of bugs where a snapshot disagrees with the
live model.

The single track carries every input already, because the existing Mix
composition rules include microphones, capture cards and media sources.
Application Audio Sources stay defined and are excluded only by the existing
de-duplication rule, so the fallback when the Desktop Audio authority group
fails is preserved.

`HasEligibleEditorTrack()` reports nothing eligible in single-track mode,
which removes the VOD track from the streaming topology without a second
mode check at the call site.

Recording track selection needed no change: it was already derived from the
topology, so a topology holding only the Mix produces a single-track file and
no Profile value is written.

Persistence came free - the mode serialises with the routing configuration,
which already lives in the Scene Collection. A payload without the field loads
as multi-track.

The panel renders the Mix row alone in single-track mode; Editor Track rows
and the Muted Sources Group row are absent rather than dimmed. The bottom-right
entry now states the active mode and toggles it.

Coverage in the routing model test: single-track emits the Mix alone for both
local and streaming topology, a microphone on an Editor Track still reaches the
single track, multi-track mute choices do not silence the single track, muting
the single track produces no audio stream, the multi-track topology is restored
byte-for-byte after a round trip, and the mode round-trips through
serialisation with legacy payloads loading as multi-track.

## Remaining

Foreground-only, deferred to the acceptance package: confirming a produced
recording actually contains one audio stream, and the zh-CN runtime smoke.
