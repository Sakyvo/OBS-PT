Status: review

## What to build

Ship the replay buffer preset keys in the packaged PotPvP profile
(`UI/data/obspt-defaults/obs-studio/basic/profiles/PotPvP/basic.ini`,
`[AdvOut]` section) so a fresh install opens Settings → Output → Recording
with the replay buffer parameters already filled in:

```
RecRB=false
RecRBSize=4096
RecRBTime=180
```

Off by default (the user flips the replay buffer on themselves), with the
memory cap (4096 MB) and duration (180 s) preset so turning it on produces a
usable configuration immediately.

Stream settings (`[Stream1]`, encoder through max B-frames) are NOT preset —
streaming bitrates are pinned by the live platform's limits, so those values
remain user-owned with no shipped default.

User protection holds by construction: same-generation installs (issue 039)
never touch the User Data Root, and the legacy-generation merge helper
already preserves `RecRB*` profile keys.

## Acceptance criteria

- [x] Shipped PotPvP `basic.ini` `[AdvOut]` carries exactly
      `RecRB=false`, `RecRBSize=4096`, `RecRBTime=180`.
- [x] `test/win/obspt-installer-config-reset-smoke.ps1` static-checks the
      three keys in the packaged profile (alongside the existing portable
      recording-path assertions).
- [ ] Manual: fresh install defaults show replay buffer off with
      4096 MB / 180 s prefilled; a same-generation reinstall leaves an
      existing profile's `RecRB*` values untouched.

## Blocked by

None - can start immediately
