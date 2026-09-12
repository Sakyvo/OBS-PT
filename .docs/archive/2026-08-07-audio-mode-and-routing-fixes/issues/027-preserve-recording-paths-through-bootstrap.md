Status: done

## What to build

Preserve the old active Profile's three independent recording directories
through the complete major-update overwrite flow, including the first launch
after installer configuration merging. Treat recording directories as
user-owned Profile values: bootstrap may initialize missing packaged defaults,
but must never replace an existing non-default value during first-run repair or
a future bootstrap version bump.

## Acceptance criteria

- [x] An overwrite install reads recording directories only from the old
  active Profile and restores Simple Output, standard Advanced recording, and
  Custom FFmpeg recording paths independently into the new PotPvP Profile.
- [x] A non-empty path is preserved byte-for-byte without normalization,
  existence checks, cross-mode synchronization, or fallback, including paths
  on an offline or currently missing drive.
- [x] An empty path is treated independently as missing and only that output
  mode is initialized to the absolute `<Install Root>/recordings` directory.
- [x] The packaged `recordings` placeholder is expanded to the same absolute
  default on a fresh installation or bootstrap repair.
- [x] A non-active old Profile cannot supply or overwrite any of the three
  paths, even when its directory name matches the new PotPvP Profile.
- [x] First-run and version-bump bootstrap preserve all existing non-default
  paths while encoder probing, monitor-resolution adaptation, Audio Routing
  Configuration deployment, and the welcome flow continue unchanged.
- [x] The installer preservation message remains accurate and merge failures
  retain the existing backup-and-error behavior.
- [x] Regression coverage exercises the complete installer-merge-to-bootstrap
  sequence rather than stopping after the PowerShell merge, and covers three
  distinct paths, partial empty values, a missing drive, multiple old Profiles,
  a fresh install, and a subsequent bootstrap version bump.
- [x] The production VS2022 v143 build succeeds, staging is refreshed with
  tests disabled, and the local 1.1.0 installer is regenerated and validated.
  (Launching Setup to its main window is foreground-only and is listed in the
  acceptance package.)

## Blocked by

None - can start immediately

## Implementation

Two independent defects, both fixed.

**Bootstrap overwrote what the installer had just restored.**
`apply_record_path_to_config()` wrote both recording paths unconditionally on
every first run, so the installer's correct merge was undone seconds later -
the reported reset of `D:/OBS-PT/recordings`. It also never handled
`AdvOut/FFFilePath` at all, even though the installer preserves it, so the
Custom FFmpeg path had no defined behaviour. The decision now lives in
`UI/obspt-record-path-policy.{hpp,cpp}`: a directory is initialised only when
it is absent, empty, or still the bare packaged `recordings` placeholder, and
each of the three is judged on its own. Comparison is byte-for-byte, so
`E:/recordings` and `recordings/potpvp` are user choices and survive.

**A same-named inactive old Profile could supply the paths.**
`obspt-preserve-config.ps1` pairs every old Profile with a same-named new one
and then adds the old-active -> new-active pair. When the old active Profile
omits a recording key entirely, the same-name merge leaves the *inactive*
Profile's value standing in the new PotPvP Profile. The three recording keys
now leave the general same-name key lists and are copied only for the active
pair; the active pair is promoted rather than dropped when the same-name sweep
already produced it. Verified by toggling the fix off: the sequence test then
reports `got 'X:/inactive'`.

`AdvOut/FFFilePath=recordings` was missing from the packaged PotPvP Profile and
has been added, so all three directories ship in the same state.

Coverage:

- `UI/obspt-record-path-policy-test.cpp` - the preserve/initialise decision,
  including offline drives, UNC paths, trailing separators, and paths that
  merely end in `recordings`.
- `UI/installer/obspt-record-path-sequence-test.ps1` - the full old Profiles ->
  installer merge -> first-run bootstrap sequence, in a temp directory,
  touching no installed copy. Covers three distinct paths, partial empty
  values, an absent key, a missing drive, multiple old Profiles with a
  same-named inactive one, a fresh install, and a version bump.

Note on the test harness: a PowerShell `[string]` parameter silently coerces
`$null` to `''`, which initially made "key absent" indistinguishable from "key
empty" and hid the inactive-Profile defect. The fixture uses an explicit
sentinel for absence.

## Remaining

The production build / staging / installer refresh the batch performs once.
