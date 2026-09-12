Status: review

## What to build

Make the installer's configuration reset generation-aware. Today the reset is
unconditional: whenever `<Install Root>/obs-studio/` exists, setup renames it
to a backup directory, installs the packaged defaults, and merges the four
preserved categories back with a PowerShell helper. That is correct for a
genuine pre-unified-audio upgrade, but wrong for a same-generation update or
reinstall — the configuration is already in the new format, and the whole
config gets destroyed and re-merged for no reason, betting the user's
settings on a single fragile script invocation. (This fired in the wild: the
merge helper died to a transient environment failure — sandbox reruns with
the same data pass — the warning box appeared, and hotkeys, recording paths,
and replay buffer settings were lost until re-entered.)

New rule: read `[OBSPT] BootstrapVersion` from the existing
`obs-studio/global.ini` (it is the configuration-format generation marker).

- **Current generation** (version present and ≥ the required generation):
  the reset page is not shown, `obs-studio/` is not renamed, merged, or
  otherwise touched; setup simply installs program files over the top.
  Preservation of every user setting — including but not limited to the four
  protected categories — holds by construction.
- **Legacy generation** (key missing, unreadable, or below the required
  generation): the existing reset + merge flow runs unchanged, with two
  hardening additions: a failed merge is retried once before declaring
  failure, and a persistently failing merge writes the script's error to a
  diagnosable log file that survives the install (the backup directory is
  already kept).

The required generation is a single `!define` in the NSIS script (currently
3), so a future format-breaking release raises the floor in one place. The
decision is made once and shared by the reset page, the rename step, and the
merge step — never three independent checks that can drift.

Update the NSIS Installer Contract in `.docs/obspt-bootstrap.md`: the
"no skip path for an existing User Data Root" clause was scoped to the 1.1.0
unified-audio migration and is replaced by the generation rule above,
including the retry/log hardening and the single-`!define` floor.

## Acceptance criteria

- [x] Installing over a current-generation User Data Root shows no reset page
  and leaves `obs-studio/` bit-untouched: hotkeys, recording paths, replay
  buffer, scenes, sources, audio routing, and dock layout all survive by
  construction, with no merge involved.
- [x] Installing over a legacy-generation User Data Root (no `[OBSPT]`
  section or a lower BootstrapVersion) still shows the reset page, runs the
  reset + merge, and restores the four preserved categories.
- [x] A missing or unreadable `global.ini` counts as legacy (fail-closed).
- [x] The merge helper gets one retry on failure; a persistent failure keeps
  the backup directory and leaves an error log file containing the script's
  actual error message.
- [x] The generation floor is one `!define`; the page, rename, and merge all
  consult the same precomputed decision.
- [x] `test/win/obspt-installer-config-reset-smoke.ps1` covers both
  generations (skip path leaves a disposable current-generation tree intact;
  legacy path still resets and merges).
- [x] `.docs/obspt-bootstrap.md` NSIS contract updated (reset condition,
  hardening, validation matrix).
- [ ] Installer rebuilt with the usual gate: no concurrent makensis, `7z t`,
  SHA-256, timestamp, and a manual launch to the Setup window.

## Blocked by

None - can start immediately
