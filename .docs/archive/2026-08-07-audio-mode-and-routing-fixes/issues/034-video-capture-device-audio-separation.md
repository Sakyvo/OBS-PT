Status: done

## What to build

Adding a Video Capture Device from the Visual Composition Panel must contribute
video only. Its audio neither reaches any output nor appears in the Audio
Routing Configuration, so it cannot claim an Editor Track and cannot be mixed
into the Compatibility Mix.

A user who wants the camera's microphone adds it separately in the Audio and
Routing Panel, where audio input devices are already offered. This keeps
creation of picture and creation of sound separate, which is the panel boundary
the product already establishes elsewhere.

This is a deliberate exception scoped to the Video Capture Device alone. Media
sources, browser sources and any other source carrying both picture and sound
keep their current behaviour: their audio still reaches outputs and still
appears in the routing panel, because that audio is part of the content itself
rather than a separate physical device. The exception must be recorded in the
domain glossary, because the panel boundary is otherwise defined by output
capability rather than by a list of source types - that rule now has one
documented carve-out, and the reason is that a camera's microphone is an
independent device with no relationship to the picture.

Routing entries that already exist for Video Capture Devices are removed on
upgrade. Under the new rule they should never have been there, and leaving them
would strand entries the user can neither route nor delete.

## Acceptance criteria

- [x] Adding a Video Capture Device contributes video only; no audio from it
  reaches the recording or the stream.
- [x] A Video Capture Device never appears in the Audio and Routing Panel.
- [x] A Video Capture Device never occupies an Editor Track, and adding one
  leaves the existing track assignments untouched.
- [x] In single-track Audio Mode a Video Capture Device is not folded into the
  Compatibility Mix.
- [x] The camera's microphone can still be captured by adding an audio input
  device in the Audio and Routing Panel, and that source routes normally.
- [x] Media sources, browser sources and every other source combining picture
  and sound keep their existing audio behaviour, both in outputs and in the
  routing panel.
- [x] Routing entries belonging to a Video Capture Device that still exists are
  removed when the Scene Collection loads.
- [x] Entries whose source no longer exists at all are out of scope here - the
  model records only a coarse source kind, so a vanished entry cannot be
  attributed to a Video Capture Device. Those are removed by
  `035-cleanup-orphaned-routing-entries.md`, and the two together clear every
  stranded entry.
- [x] Removing those entries does not disturb any other source's track
  assignment, intra-track order, or mute state.
- [x] The glossary records the exception and why it exists, so the
  capability-based panel boundary is not read as unconditional.
- [x] Focused coverage runs offscreen, sends no real keyboard or mouse input,
  and shows no window.
- [x] The production VS2022 v143 build succeeds and staging is refreshed with
  tests disabled.

## Blocked by

None - can start immediately

## Implementation

Three separate places had to agree, and missing any one of them would have
left the camera audible.

The type test lives in the routing model as a pure predicate, so it is
assertable without libobs: `dshow_input` contributes picture only, and every
other type - media, browser, VLC, the capture sources, the audio captures -
does not. The test enumerates them explicitly, so widening the exception by
accident would fail.

Keeping the source out of the routing model was not enough on its own.
`ApplyAudioRouting()` returns early for sources it does not manage, which would
have left the camera on its default mixer and still feeding Track 1. Its audio
is now silenced explicitly before that early return.

Reconciliation drops entries a previous version stored for a Video Capture
Device, and re-silences the source each pass. Entries whose source is gone
entirely cannot be attributed to a device type - the model records only a
coarse source kind - so those are left to 035.

The glossary now carries the exception together with its reason, since the
panel boundary is otherwise defined by capability and explicitly disclaims
type-name lists.

## Remaining

Foreground-only: adding a Video Capture Device and confirming no audio entry
appears, that the previous stranded entry is gone, and that adding the camera's
microphone as an audio input device still routes normally.
