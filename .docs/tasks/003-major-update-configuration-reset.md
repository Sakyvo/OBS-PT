Status: done

## Parent

`.docs/prd/2026-07-20-unified-audio-routing.md`

## What to build

Deploy the incompatible unified-audio topology through a mandatory major-update configuration reset instead of runtime migration. Existing installs receive the packaged Scene Collection, source ownership, routing model, dock layout, video, audio, and encoder defaults; setup restores only stream settings, recording paths, hotkeys, and Replay Buffer settings. It covers PRD user stories 88-92.

## Acceptance criteria

- [x] Runtime startup contains no legacy mixer-bitmask or pure-audio Scene Item migration path.
- [x] An existing User Data Root shows the English reset page and requires explicit confirmation before installation can continue.
- [x] Rejecting confirmation stays on the reset page and writes no target files.
- [x] A fresh Install Root skips the reset page and receives the packaged defaults directly.
- [x] Setup moves the old User Data Root aside before copying and retains the backup if deployment or merge fails.
- [x] Stream settings, recording paths, hotkeys, and Replay Buffer settings are restored after packaged defaults are deployed.
- [x] Scenes, sources, routing, dock layout, video, audio, and recording encoder settings are not restored from the previous configuration.
- [x] Pure audio sources in the packaged model are Scene Collection-owned, persist outside Scene Items, and remain active across visual Scene changes.
- [x] Installer and portable-runtime smoke tests cover fresh install, confirmed reset, rejected confirmation, preservation scope, and source save/load.

## Blocked by

- `002-editor-tracks-source-identity.md`
