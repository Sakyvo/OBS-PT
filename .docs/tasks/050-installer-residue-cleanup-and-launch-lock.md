Status: review

## What to build

After 049 makes the shipped layout flat, the installer must also clean up
historical residue from the live Install Root and lock every launch path to
the new flat executable, so automatic launch and shortcuts can never hit a
stale empty exe.

Today a real installation root can carry legacy leftovers that came from
mixed installs: a top-level `64bit\` directory (a full copy of the runnable
tree), source-tree debris (`obs-*.h`, `libobs\`, `media-io\`, etc.), and
scattered `obs-plugins\` plugin subdirectories at the top level, plus the
old `bin\64bit\`. These confuse shortcuts/Auto-launch and produce "命中的
exe 是空的" symptoms.

Cleaning is strictly allow-list based: only known non-standard top-level
paths are removed; anything user-owned (`obs-studio\`, `recordings\`) and any
unknown directory is left untouched.

## Acceptance criteria

- [x] Installer removes the top-level `64bit\` directory (when present and
      containing only OBS-PT program files) after a successful install.
- [x] Installer removes known source-debris top-level entries from previous
      dev installs: `obs-*.h`, `libobs\`, `media-io\`, `audio-monitoring\`,
      `callback\`, `graphics\`, `obs-scripting\` (allow-list, only when
      present), and scattered top-level `obs-plugins\` plugin subfolders.
- [ ] Old `bin\64bit\` is removed after a successful install when it contains
      only OBS-PT program files.
- [x] `obs-studio\` (User Data Root) and `recordings\` are never touched by
      the cleanup.
- [x] All launch paths point at the flat exe: finish-page launch, Desktop /
      Start Menu shortcuts, uninstall display icon, and the HKCU install
      location check all use `bin\OBS-PT.exe`.
- [ ] After an upgrade install, a launch (shortcut or finish-page) opens the
      flat `bin\OBS-PT.exe` and never a stale path.
- [x] `test/win/obspt-installer-config-reset-smoke.ps1` extended: the cleanup
      allow-list is present, launch paths use the flat exe, and the legacy
      layout checks are gone.

## Blocked by

- `049-flatten-64bit-layout.md`

## Regression fix (post-implementation)

The uninstaller used `RMDir /r "$INSTDIR"` which recursively deleted the
entire Install Root, including `recordings\` (user output) and `obs-studio\`
(User Data Root) — causing recording loss on uninstall. Fixed by removing only
program files (`bin\`, `data\`, `obs-plugins\`, `cmake\`, `include\`,
`uninstall.exe`) and using non-recursive `RMDir "$INSTDIR"` (removes the root
only when empty, i.e. when recordings/ and obs-studio/ are absent).