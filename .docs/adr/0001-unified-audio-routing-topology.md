---
status: accepted
---

# Unified Audio Routing With Player-First Mix

OBS-PT replaces the upstream per-output mixer bitmask with a Scene Collection-owned Audio Routing Configuration: one fixed Compatibility Mix (`Mix`, Track 1) plus single-assignment Editor Tracks (Tracks 2-6). Recording, custom FFmpeg output, Replay Buffer, and VOD sessions freeze their topology and validate each output independently. Compatibility Mix is intentionally retained alongside Editor Tracks and marked as the default because ordinary players must always receive complete audio; VEGAS may import all tracks, so editors may mute or remove `Mix` when editing. Single-track containers use `Mix`, unsupported or unknown custom containers fail clearly, and live streams keep a silent AAC transport when the user mutes every source.

## Considered Options

- Omit `Mix` whenever Editor Tracks exist: rejected because ordinary players would often play only one incomplete source.
- Keep independent per-output track checkboxes: rejected because they allow a configured source route to be silently absent from the file.
- Keep pure audio inputs as Scene Items: rejected because their capture lifetime would follow whichever visual Scene is active, while routing is owned by the whole Scene Collection.
- Migrate legacy pure-audio Scene Items at startup: rejected for this internal-stage breaking release because the compatibility branches would make the new topology harder to reason about. The major update ships a fresh configuration instead.

## Source Ownership

Pure audio inputs remain ordinary OBS Sources so existing plugins, settings, and Source Identity behavior stay intact, but they are not represented by Scene Items. The Audio and Routing panel owns their creation, lifetime activation, routing membership, and removal. OBS-PT holds each such source and marks it as a libobs audio mix root; the audio callback mixes those roots directly, so removing the Scene Item does not remove the signal from recording, streaming, or monitoring. Visual sources that also emit audio remain Scene Items in the Visual Composition panel; their audio output is registered with the same routing model. This keeps visual scene switching independent from pure audio capture without creating a second source system.

The panel boundary is capability-based: an input with audio output and no video output is created in Audio and Routing; an input with any video output remains a visual Scene Item and its audio output is registered automatically. The UI does not maintain a hard-coded allowlist of plugin type names.

Audio and Routing and the Audio Mixer enumerate the same complete ordered set of manageable sources. Disconnected, inactive, hidden-scene, and muted-group sources remain visible; runtime activity changes meter state, not row existence or ordering.

Settings > Audio retains only engine-wide parameters (sample rate, channel layout, meter and monitoring configuration). It no longer creates Desktop/Mic/Aux source slots; source creation and device selection are owned by Audio and Routing.

This release intentionally does not migrate legacy audio-only Scene Items. A major-update configuration reset installs the new Scene Collection and routing defaults; any preserved user values are handled by the installer/bootstrap contract rather than by runtime topology migration. The reset preserves only stream settings, recording paths, hotkeys, and Replay Buffer settings; all scene/source/routing, layout, video, audio, and encoder configuration comes from the packaged defaults.

Because the old topology is not compatible with this model, the major-update installer requires the configuration reset instead of offering a skip path. The four preservation categories above are merged after the packaged defaults are installed.

## Consequences

The route model is stable across outputs and Scene Collections. This breaking release deploys the new source and routing topology through the installer reset rather than carrying legacy routing into runtime. NLE import behavior cannot be made universal through MP4 default flags; player compatibility is the explicit priority.
