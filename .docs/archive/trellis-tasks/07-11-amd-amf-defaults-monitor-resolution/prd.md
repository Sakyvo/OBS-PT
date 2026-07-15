# Fix AMD AMF defaults and monitor resolution

## Goal

Make a fresh OBS-PT installation adapt the PotPvP Profile to an AMD machine and
its actual primary-display resolution. The initial AMF settings must be clear,
editable, and immediately suitable for high-frame-rate recording instead of
showing a blank preset, exposing incompatible template choices, or retaining
the packaged 1920x1080 resolution on a 2560x1440 display.

## Background and Confirmed Facts

- The tester used an AMD GPU and a 2560x1440 display on a fresh installation.
  The recording path was changed manually after installation and is unrelated
  to the reported defaults.
- The fresh UI showed the AMF top-level **Preset** field as blank. Choosing its
  **Recording** item changed rate control to VBR and disabled several fields.
- The desired AMD baseline is H.264 High profile, CQP, I/P/B QP 27 at 1080p
  and above, I/P/B QP 22 below 1080p, and AMF **Quality Preset = Speed**.
- The legacy AMF plugin defines the top-level `Preset` as a template selector.
  Its `None=-1` item is deliberately rendered with an empty label at
  `plugins/enc-amf/Source/enc-h264.cpp:267-276`.
- `properties_modified()` applies the **Recording** template at
  `plugins/enc-amf/Source/enc-h264.cpp:678-714`: it sets High profile but also
  forces latency-constrained VBR and disables rate control, profile, profile
  level, QP bounds, keyframe interval, and other fields. The screenshot is the
  expected consequence of this upstream template, not a settings-widget bug.
- The existing OBS-PT AMF bootstrap branch attempted to write High, Speed, CQP,
  and QP values using unprefixed names such as `Profile` and
  `RateControlMethod`. The legacy plugin's property IDs are
  `AMF.H264.Profile`, `AMF.H264.RateControlMethod`, and so on, so the unknown
  JSON fields were silently ignored and the UI fell back to Balanced/Main/CBR.
- The previous AMD task intentionally kept `Preset=None` because every named
  upstream template can overwrite OBS-PT's explicit CQP values. This decision
  remains technically sound; the usability defect is the blank label and the
  continued exposure of overwrite-capable templates.
- On the accepted 2560x1440 defaults, AMF recording still failed before output
  creation with `ProfileLevel=249` and `AMF_INVALID_ARG`. The legacy automatic
  level table used a `-1` sentinel in a `uint8_t` enum, so inputs above H.264
  Level 5.2 walked past the table and returned arbitrary memory. Both 240 FPS
  and the AMF-only 256 FPS ceiling exceed that table at 2560x1440.
- Primary-display adaptation already exists at
  `UI/obspt-bootstrap.cpp:337-375`. It reads
  `QGuiApplication::primaryScreen()`, converts logical size to physical pixels
  with `devicePixelRatio()`, and writes Base and Output resolution before
  `ResetVideo()`.
- That function only runs when `g_late_bootstrap_needed` is true
  (`UI/obspt-bootstrap.cpp:396-397`). The repair flag is false when
  `FirstRunCompleted=true` and `BootstrapVersion` is current
  (`UI/obspt-bootstrap.cpp:293-303`).
- The packaged `UI/data/obspt-defaults/obs-studio/global.ini:39-41` currently
  contains `BootstrapVersion=3` and `FirstRunCompleted=true`. A fresh install
  therefore skips both monitor adaptation and the late encoder-template write,
  retaining the packaged 1920x1080 `basic.ini`. Git history shows these runtime
  completion markers entered the shipped default while preparing release
  `1.0.0`; they are machine/session state, not valid first-install defaults.
- The project remains in internal testing. Existing-user upgrades, migration,
  and preservation of already-installed settings are not requirements for this
  task.
- `plugins/enc-amf` is a git submodule whose parent gitlink currently targets
  local custom commit `0cebebc` (the AMF frame-rate hardening fix). That commit
  is not advertised by any upstream `obsproject/obs-amd-encoder` ref, and a
  `Sakyvo/obs-amd-encoder` repository does not currently exist. The present
  remote source therefore cannot reproduce even the already-shipped AMF build
  through a clean recursive clone; this task cannot add another unreachable
  submodule commit.

## Requirements

### R1. Restore genuine first-install bootstrap

- The packaged Global Config must represent an incomplete first run. It must
  not claim that the current bootstrap version has already completed.
- Preserve the approved packaged GUI geometry and dock layout; remove or reset
  only the first-run/bootstrap completion state that suppresses adaptation.
- On first launch, the existing early and late bootstrap phases must both run,
  and the existing welcome-dialog completion path must remain authoritative.

### R2. Use the primary display's native resolution

- On a fresh installation, set PotPvP Base and Output resolution to the primary
  display's physical pixel size before `ResetVideo()` reads `basic.ini`.
- On multi-display systems, follow the display Windows designates as primary;
  do not choose the highest-resolution connected display automatically.
- A 2560x1440 primary display must produce Base=Output=2560x1440 on the first
  launch. Keep the existing no-resolution-cap policy and 480/1 FPS baseline.
- A null/invalid screen may retain the existing safe fallback; do not replace a
  successfully detected 2K display with 1920x1080.

### R3. Apply the AMD recording baseline

- When `amd_amf_h264` wins encoder probing on a fresh install, write only AMF
  keys to `recordEncoder.json`.
- Set `[AdvOut] RecEncoder=amd_amf_h264` in the active Profile so the Settings
  UI selects AMD and loads `recordEncoder.json` without manual intervention.
- Write exact plugin property IDs: `AMF.H264.Preset=-1`, High profile
  (`AMF.H264.Profile=100`), CQP (`AMF.H264.RateControlMethod=0`), and Speed
  (`AMF.H264.QualityPreset=0`). Set the three `AMF.H264.QP.*` values to 27
  when `BaseCY >= 1080`, otherwise set all three to 22. Keep the existing
  performance-oriented B-frame and VBV settings unless code evidence proves
  they conflict with this baseline.
- Do not solve the AMD path by lowering the 480 FPS PotPvP default.
- Automatic AMF H.264 Profile Level selection must remain bounded for high-FPS
  inputs. When no table level satisfies the resolution/rate pair, select the
  highest supported table value (Level 5.2) instead of reading past the table.

### R4. Remove misleading AMF templates

- In the AMF encoder property UI, retain only the `Preset=-1` item and give it
  the exact visible name `OBS-PT` in every supported locale. Do not expose
  Reset, Recording, High Quality, Indistinguishable, Lossless, Twitch, or
  YouTube choices.
- Keep the actual **Quality Preset** control and its Speed/Balanced/Quality
  choices; it is a distinct AMF encoder setting and must default to Speed.
- Localize the single top-level preset name in OBS-PT's supported locales:
  en-US, zh-CN, zh-TW, ja-JP, and ko-KR.

### R5. Packaging and scope

- Rebuild `enc-amf`, `OBS-PT.exe` if bootstrap changes require it, staging, and
  the NSIS installer. Validate that the installer contains the modified AMF
  DLL, fresh-install Global Config, and supported locale files.
- Publish the AMF source changes through a remotely reachable dependency; a
  parent gitlink to a local-only commit is not an acceptable release state.
- Create and maintain a public `Sakyvo/obs-amd-encoder` fork, push the existing
  OBS-PT AMF hardening ancestry plus this task's changes there, and point the
  parent `.gitmodules` URL and gitlink to that fork.
- This task targets fresh installations during internal testing. Do not add a
  migration or overwrite existing user settings merely to support upgrades.

## Acceptance Criteria

- [ ] AC1: Packaged Global Config no longer suppresses first-run bootstrap, but
      retains the approved GUI geometry and dock layout.
- [ ] AC2: A clean install on a 2560x1440 primary display shows 2560x1440 for
      both Base and Output resolution on its first launch, at 480/1 FPS.
- [ ] AC2a: On a multi-display system, the chosen Base/Output size comes from
      the Windows primary display even when another display has more pixels.
- [ ] AC3: A clean AMD install writes `[AdvOut] RecEncoder=amd_amf_h264`, selects
      it without user action, and writes exact `AMF.H264.*` keys for High, CQP,
      Quality Preset Speed, and `Preset=-1`, with no foreign or unprefixed AMF
      keys. I/P/B QP are all 27 at 1080p or above and all 22 below 1080p.
- [ ] AC4: The AMF top-level Preset dropdown contains exactly one item named
      `OBS-PT`; selecting/opening it cannot switch rate control to VBR or lock
      the OBS-PT baseline fields.
- [ ] AC5: The separate AMF Quality Preset dropdown remains available and shows
      Speed by default.
- [ ] AC6: The welcome dialog still appears for a clean install and completion
      is persisted through the existing dialog-finished path.
- [ ] AC7: Focused static/runtime checks pass on the local machine, AMD tester
      validation confirms the UI/defaults, and a fresh installer passes `7z t`
      with a current timestamp and recorded SHA-256.
- [ ] AC8: A clean recursive clone can fetch the exact AMF gitlink and build the
      customized plugin source; no required AMF commit exists only locally.

## Out of Scope

- Existing-install migration, automatic repair, or installer overwrite-policy
  changes.
- Replay Buffer defaults or AMF replay-buffer stability.
- Lowering the global or AMD-specific FPS baseline.
- Porting the modern OBS AMF encoder, HEVC, or AV1.
- Reworking unrelated NVENC, QSV, x264, streaming, path, hotkey, scene, or GUI
  layout defaults.
