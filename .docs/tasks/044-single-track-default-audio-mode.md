Status: review

## What to build

Ship the audio mode default as single-track. Today the packaged
`PotPvP.json` `obspt_audio_routing` object omits `audioMode`, and the model
defaults absent `audioMode` to multi-track (for backward compatibility with
configs written before the mode existed). New installs should come up in
single-track mode, so the shipped scene collection must set it explicitly.

Change: add `"audioMode":"single"` to the `obspt_audio_routing` object in
`UI/data/obspt-defaults/obs-studio/basic/scenes/PotPvP.json`. Existing
installs are never touched (issue 039 same-generation skip), so this only
affects fresh installs.

## Acceptance criteria

- [x] Shipped `PotPvP.json` `obspt_audio_routing` carries
      `"audioMode":"single"`.
- [x] `test/win/obspt-installer-config-reset-smoke.ps1` static-asserts the
      shipped scene's `audioMode` value.
- [x] Manual (fresh install): the Audio Mode toolbar shows single-track on
      first launch.

## Blocked by

None - can start immediately
