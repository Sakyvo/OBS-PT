Status: done

## What to build

Ship the Video Capture Device source, which serves webcam capture.

This is not a new feature - it is a build gap. The DirectShow plugin's own
source is present and complete in this fork, but the capture library it depends
on is a git submodule whose directory is empty here. The plugin's build script
checks for that library and silently skips the entire plugin when it is
missing, which is why the source type never appears and no plugin binary is
produced. OBS 27.2.4 ships this source; this fork simply never fetched the
dependency.

Fetch the library and pin it to the commit OBS 27.2.4 itself used,
`8e7a75f2bf50dce4c9ebccf362023ecc567cece1`, rather than the library's current
master, so the implementation matches 27.2.4 exactly and no API drift is
introduced. The FFmpeg components the plugin needs are already present in the
dependency set.

Keep VirtualCam disabled. It is the opposite direction - an output that
publishes OBS-PT as a camera to other applications - and enabling it would
require a registered COM component and installer changes. It is not part of
this need.

The plugin binary must reach staging and the installer alongside the others.

## Acceptance criteria

- [x] The DirectShow capture library is present at the pinned 27.2.4 commit.
- [x] The plugin's build script no longer skips the plugin, and the build
  produces its binary.
- [ ] `视频采集设备` appears in the source type list of the Visual Composition
  Panel.
- [ ] Creating the source and opening its properties works; with no camera
  attached the device list is simply empty and nothing crashes or hangs.
- [x] The plugin binary and its data are present in staging and inside the
  regenerated installer.
- [x] VirtualCam remains disabled, and no COM registration or installer change
  is introduced.
- [x] No other plugin's build status changes as a side effect.
- [x] The plugin's locale files are present for every shipped language.
- [x] The production VS2022 v143 build succeeds and staging is refreshed with
  tests disabled.

## Blocked by

None - can start immediately

## Notes

Actual camera capture cannot be verified on the development machine, which has
no camera. Automated and background verification stops at: the plugin builds,
the binary ships, the source type is offered, and its properties dialog opens
with an empty device list. Confirming a live picture belongs to the acceptance
package and needs a machine with a camera.

## Implementation

The dependency was cloned and pinned to `8e7a75f2`, the commit OBS 27.2.4
itself used, rather than the library's current master. Confirmed by the
build's own output: the "libdshowcapture submodule not found ... plugin
disabled" message is gone, and `VirtualCam GUID not set! VirtualCam disabled`
still appears, so the input source is built and the output feature stays off.

No source change was needed anywhere - the plugin's code was already complete
in this fork. The installer grew from 1470 to 1529 files, the difference being
the plugin binary and its locale set.

## Remaining

Foreground and hardware-dependent: the source type appearing in the list, its
properties dialog opening with an empty device list, and - on a machine with a
camera - an actual picture. Neither can be verified here.
