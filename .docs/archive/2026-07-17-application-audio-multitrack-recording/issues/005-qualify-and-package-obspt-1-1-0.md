Status: done

## Parent

`../prd/2026-07-16-application-audio-multitrack-recording.md`

## User stories

28-34, 37-40

## What to build

Qualify the completed game/music multitrack workflow under OBS-PT's real high-FPS workload, resolve any failures against the accepted performance and lifecycle limits, update all product version surfaces to 1.1.0, and produce a verified local installer from refreshed staging.

The release candidate must be tested through the same installed or staged Windows binary users will run. This slice prepares local release artifacts only and must not create, replace, or edit a GitHub Release.

## Acceptance criteria

- [x] The maintainer's explicit waiver of the 30-minute-only qualification is recorded without claiming that a long run passed.
- [x] Target-running, target-starts-later, target-exits, target-restarts, disconnected-stop, reconnecting-stop, and connected-stop scenarios all pass without modal UI, foreground activation, or an OBS-PT hang.
- [x] OBS-PT, Windows binary resources, updater model expectations, packaging metadata, and installer identity consistently report version 1.1.0.
- [x] The supported VS2022 v143 configuration and RelWithDebInfo build complete with tests disabled for final staging.
- [x] Staging is refreshed and the staged binary registers the Source, loads all five supported locales and its icon, and records the accepted three-track Hybrid MP4 workflow.
- [x] Exactly one NSIS process builds the installer and exits normally.
- [x] The installer has a current timestamp, passes `7z t`, has a recorded SHA-256, and launches to the Setup window.
- [x] A disposable fresh installation preserves the accepted Profile and Scene Collection defaults and completes the packaged end-to-end recording smoke test.
- [x] No GitHub Release or remote release asset is created, replaced, or edited.

## Waived criteria

On 2026-07-17, the maintainer directed this batch to ignore the long test and be archived. The following original criteria are waived, not passed:

- Native-resolution 480 FPS Hybrid MP4 recording runs for 30 minutes with two active Application Audio Sources.
- Rendering lag and encoding lag each remain at or below 0.5% under adequate, recorded resource headroom.
- Game and Music remain isolated, contain no crackle or corruption, and drift no more than 50 ms from video over the 30-minute run.

## Verification

- Final installer: `.target/_pkg/OBS-PT-1.1.0-Installer.exe`
- SHA-256: `6E1EFBD83494228EA2C2343DEA3752A5DB034390D2D889DC0672CBEC479ED9CA`
- Packaged E2E runtime: `.target/_tdd/application-audio/20260716-160348`
- Rejected 60-second qualification report: `.target/_tdd/application-audio/20260716-161323/qualification.json`; audio decode, isolation, silence-gap, and 13.97 ms drift checks passed, but rendering/encoding lag were 1.7%/16.1% while other processes drove system CPU to 48%.
- Passed 60-second qualification preflight: `.target/_tdd/application-audio/20260717-120016/qualification.json`; rendering/encoding lag were 0.1%/0.4%, all three tracks decoded and remained isolated, and maximum drift was 20.016 ms.
- Thirty-minute qualification runs `20260717-135146`, `20260717-142213`, and `20260717-145616` never started OBS-PT because the five-minute resource-headroom gate did not stabilize before its 30-minute deadline. Their final samples were respectively CPU 9.70% / free memory 8724 MB / GPU 3%, CPU 26.31% / free memory 8102 MB / GPU 14%, and CPU 100% / free memory 347 MB / GPU 31%; the last coincided with another parallel MSVC link workload consuming most system memory.
- Run `20260717-162826` was stopped at the user's direction while still waiting at the resource-headroom gate. It produced no media or qualification report. The maintainer subsequently waived the three 30-minute criteria and directed the batch to be archived; no 30-minute result is claimed.

## Blocked by

- `004-record-game-and-music-on-isolated-tracks.md`
