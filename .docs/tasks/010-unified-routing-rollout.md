Status: done

## Parent

`.docs/prd/2026-07-20-unified-audio-routing.md`

## What to build

Make Audio Routing Configuration the only production audio-track workflow and qualify the complete feature for release. Finish the docked UI, inspector, accessibility, five-language presentation, default configuration, removal of legacy controls, ordinary-player/NLE acceptance, and the 480 FPS real-output matrix. It covers PRD user stories 4-5, 17-21, 36-38, 75, and 93-95.

## Acceptance criteria

- [x] The main mixer retains meters, faders, volume, and mute controls for every manageable routed source and has a persistent audio-routing command below it.
- [x] The routing panel docks, floats, restores layout, and displays the complete current Scene Collection as a `Track / Name / Source` table.
- [x] The former Advanced Audio Properties action reveals and focuses the routing panel; the old six-checkbox source matrix is no longer reachable.
- [x] Output settings no longer expose streaming-track, recording-track, FLV-track, custom-FFmpeg-mix, per-track bitrate, or per-track name controls.
- [x] One source inspector exposes volume, mono, balance, sync offset, and monitoring with no duplicate route model.
- [x] Drag-and-drop and the focusable source menu provide equivalent move, order, mute, and inspector commands with identical lock validation. User explicitly waived the remaining manual interaction check after the implementation, model tests, and five-language UI smoke passed.
- [x] Screen readers announce source name, track number/name, mute, `Monitor Only`, lock, warning, and pending-next-session states.
- [x] All new labels, errors, menus, tooltips, and accessible names are complete in `en-US`, `zh-CN`, `zh-TW`, `ja-JP`, and `ko-KR`.
- [x] Long translated text fits at supported desktop resolutions without overlap or clipping.
- [x] Shipped defaults use `Mix`, `Game`, `Music`, `Microphone`, `Voice Chat`, `Aux`, unified `160 Kbps`, and no obsolete track override.
- [x] A common Windows player and VLC select `Mix` by default and reproduce complete audio for a multi-track file.
- [x] VEGAS imports every stream in stable file order, the file preserves stable names, and the first audio stream is identifiable as `Mix` and can be muted/removed; VEGAS timeline auto-naming or hiding is not required.
- [x] The complete model-test suite and Windows real-output matrix pass for fresh-reset, Simple, Advanced, standard-container, custom, Replay Buffer, streaming, and VOD scenarios.
- [x] The default 480 FPS qualification passes rendering-lag, encoding-lag, A/V drift, resource-headroom, stream-count, and tone-separation thresholds without lowering resolution or quality.
- [x] RelWithDebInfo builds successfully with the required VS2022 v143 toolchain, staging is refreshed, and the packaged application launches with the production defaults.
- [x] Installer/package validation required by the project rules completes without introducing test artifacts into staging.

## Latest automated evidence

- The five-language UI smoke passed against the release staging tree with docking, floating, layout restoration, statistics visibility, focusable source items, and all six track groups.
- The short Hybrid MP4 application-audio smoke passed after adding the libobs
  audio-mix-root path: a pure `Game Audio` source remains outside every Scene
  Item, survives Source Identity/Track 2 persistence, and produces isolated
  `Mix` and `Game` tones (`Mix440=127.24`, `Game440=127.23`).
- Source items now explicitly accept drop targets and the drag handler retains the item captured at drag start; the user-waived manual drag/menu and recording-lock check remains documented as a review limitation.

## Blocked by

- `003-major-update-configuration-reset.md`
- `008-custom-ffmpeg-replay-output.md`
- `009-streaming-vod-routing.md`
