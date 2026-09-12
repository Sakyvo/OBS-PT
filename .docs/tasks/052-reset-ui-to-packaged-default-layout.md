Status: review

## What to build

The 「重置界面」 (Reset UI) dock-window action must reset to the OBS-PT
packaged default layout instead of the upstream preset behavior. Today
`on_resetUI_triggered()` restores the program-start snapshot
(`startingDockLayout`) and then force-resizes the five upstream docks
(scenes/sources/mixer/transitions/controls) with hard-coded proportions — it
never restores the OBS-PT custom docks (notably `audioRoutingDock`).

The packaged default layout already exists: the shipped
`obspt-defaults/obs-studio/global.ini` carries `[BasicWindow] DockState`, a
Base64-encoded `QMainWindow::saveState()` snapshot containing the OBS-PT
factory layout (scenes, sources, mixer, transitions, controls, and the
audio-routing dock).

Reset must therefore:
- load the packaged `[BasicWindow] DockState` from the bundled defaults
  (resolving it from the OBS-PT data path, not from the live user config),
- `restoreState()` with the decoded snapshot so every dock — including
  `audioRoutingDock` — returns to the factory position/visibility,
- no longer apply the hard-coded five-dock proportional resize.

Behavior: reset = "return to the factory UI". Users who never customized see
no change; users with a messed-up layout get the packaged default back. No
hard blocking while outputs are active (UI-level only).

## Acceptance criteria

- [ ] `on_resetUI_triggered()` restores the packaged default DockState
      (from `obspt-defaults/obs-studio/global.ini`), not the session snapshot.
- [ ] After reset, the audio-routing dock and the other OBS-PT docks are
      laid out at their factory positions (visible as in a fresh install).
- [ ] The hard-coded five-dock proportional resize no longer runs (the
      packaged snapshot governs the geometry).
- [ ] Reset works whether or not a User Data Root exists (packaged defaults
      are read from the install tree, not the user's global.ini).
- [ ] Static/smoke check asserts the reset handler loads the packaged
      DockState (no `restoreState(startingDockLayout)` in the reset path).
- [ ] Manual: customize the layout (move/close docks), run 重置界面, verify
      the factory layout including the 音频与路由 dock returns.

## Blocked by

None - can start immediately
