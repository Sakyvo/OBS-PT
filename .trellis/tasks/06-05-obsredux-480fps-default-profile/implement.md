# Implement: 480fps default profile + hardware-adaptive bootstrap

Execution checklist. See `prd.md` (requirements/AC) and `design.md` (contracts,
call-order facts, per-encoder templates).

## Step 0 — Confirm build target

- [ ] Read `UI/CMakeLists.txt` to confirm the UI/main app target name (OBS 27 is
      typically `obs`; this fork may differ). Record it for Step 5 commands.

## Step 1 — Sync static defaults to user's current runtime config (PR1)

Selective sync (see `design.md` "Static edits"; `<d>` =
`UI/data/obspt-defaults/obs-studio`). Verified via diff against
`finished/OBS-Redux` runtime config + user decisions Q1/Q2/Q3.

- [ ] `<d>/basic/profiles/PotPvP/basic.ini`: `[Video] FPSNum` 360 → `480`; `[Hotkeys]`
      clear all 5 bindings to `{"bindings":[]}` (Q1). Leave `RecFilePath`/`FilePath`
      = `./recordings` (do NOT bake machine path); do NOT add `[Panels] CookieId`.
- [ ] `<d>/basic/profiles/PotPvP/recordEncoder.json`: canonical `jim_nvenc` CQP-26
      template (`rate_control:"CQP"`, `cqp:26`, `preset:"hp"`, `preset2:"p1"`,
      `profile:"high"`, `tune:"ll"`, `multipass:"disabled"`, `bf:0`, `psycho_aq:false`).
- [ ] `<d>/basic/profiles/PotPvP/streamEncoder.json`: `{"bitrate":20000,"profile":"high"}` (Q2).
- [ ] `<d>/basic/profiles/PotPvP/service.json`: Bilibili Live rtmp_common, `key:""` (Q2).
- [ ] `<d>/basic/scenes/PotPvP.json` (Q3): reorder items → Minecraft (top) then
      Display (bottom); `Minecraft.visible=true`, `Display.visible=false`; Display
      item `pos`→`0,0`; `Display.settings.method=0`. Keep other scene structure.
- [ ] `UI/obspt-bootstrap.h:7`: `OBSPT_BOOTSTRAP_VERSION` 2 → 3.
- [ ] Do NOT sync runtime `global.ini` additions (FirstRunCompleted/BootstrapVersion/
      window geometry) — would break first-run.

## Step 2 — Early resolution/fps write (PR2)

- [ ] `UI/obspt-bootstrap.cpp`: add `#include <QScreen>` / `<QGuiApplication>`.
- [ ] Add `static void apply_monitor_video_to_profile(const char *profile_name)`
      per `design.md` contract: primary QScreen → physical px (× devicePixelRatio,
      **no >1080p cap**), guard null / `<8`, open `<profile>/basic.ini`
      `CONFIG_OPEN_EXISTING`, set `Video` Base/Output CX/CY = monitor (base==output),
      `FPSType=2/FPSNum=480/FPSDen=1`, `config_save_safe`, `config_close`. Mirror
      the ini-edit pattern of `apply_record_path_to_profile` (`:543-562`).
- [ ] In `run_obspt_early_bootstrap` (`:324`), after the recordings-dir block,
      add `if (g_late_bootstrap_needed) apply_monitor_video_to_profile(POTPVP_PROFILE);`
      (gated on repair — runs before main window / ResetVideo).

## Step 3 — Per-encoder template + CQP (PR3)

- [ ] `UI/obspt-bootstrap.h:50` + `.cpp:521`: change signature to
      `int apply_encoder_to_profile(const char *profile_name, const char *encoder_id, int cqp)`.
- [ ] Rewrite body: create a **fresh** `obs_data_t` (not load-and-mutate),
      `obs_data_set_string("encoder", encoder_id)`, then branch on `encoder_id`
      writing the exact key set from `design.md`'s table (jim_nvenc / ffmpeg_nvenc /
      obs_qsv11 / amd_amf_h264 / obs_x264; default → jim_nvenc). `obs_data_save_json`
      to the profile's `recordEncoder.json` path.
- [ ] `run_first_run_bootstrap_if_needed` (`:601`): after `probe_record_encoder`,
      read `int base_cy = (int)config_get_uint(active_config, "Video", "BaseCY")`,
      `int cqp = (base_cy > 0 && base_cy < 1080) ? 20 : 26;`, call
      `apply_encoder_to_profile(active_profile_name, enc.encoder_id, cqp)`.

## Step 4 — Self-review gate

- [ ] `grep -n "encoder-mismatched"`-equivalent mental check: confirm fresh
      `obs_data_t` per encoder (no stale NVENC keys reachable for QSV/x264/AMF).
- [ ] Confirm early write is repair-gated (no unconditional per-launch override).
- [ ] Confirm signature change compiles: header + single caller both updated.
- [ ] Re-read `apply_monitor_video_to_profile` for the omitted >1080p cap (must
      write full monitor res) and base==output.

## Step 5 — Build & deploy

- [ ] `cmake --build build64 --config RelWithDebInfo --target <ui-target> -- /m`
      (target from Step 0).
- [ ] Deploy built binary (e.g. `obs64.exe`) to the `finished/OBS-Redux` install
      `bin/64bit/`, and sync the edited `data/obspt-defaults/...` files to the
      install's data dir.

## Step 6 — First-run verification

- [ ] The version bump (2→3) auto-triggers repair on the existing
      `finished/OBS-Redux` install — launch it.
- [ ] Inspect the new log: FPS `480`, base/output = this machine's monitor
      resolution, record encoder = `jim_nvenc`, CQP `26` (1080p test machine).
- [ ] Confirm `recordEncoder.json` post-launch contains only jim_nvenc-valid keys.
- [ ] Record a short clip → non-black, plays at 480fps @ monitor res.
- [ ] (If feasible) simulate a sub-1080p primary monitor → base/output match it,
      CQP `20`.

## Step 7 — Spec + commit (Phase 3)

- [ ] Update the bootstrap spec with the two-phase adaptation contract
      (resolution/fps early, encoder/CQP late, repair gate, per-encoder templates).
- [ ] Commit to local git with a descriptive message.

## Rollback points

- After Step 1: static-only (480fps fresh installs) is independently shippable.
- After Step 3 build failure: revert signature change; encoder stays id-only.
- Full revert: single commit revert restores BootstrapVersion 2 (no migration trap).
