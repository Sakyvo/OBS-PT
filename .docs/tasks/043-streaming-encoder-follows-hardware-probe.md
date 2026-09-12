Status: review

## What to build

On first-run bootstrap, the streaming encoder (Output → Streaming, advanced
mode) must follow the same hardware probe as the recording encoder. Today
`run_first_run_bootstrap_if_needed()` writes the probed encoder only to
`[AdvOut] RecEncoder`, leaving `[AdvOut] Encoder` at the OBS upstream default
(x264); a machine with an NVIDIA GPU should come up with NVENC selected for
streaming exactly like recording does.

Change: in `run_first_run_bootstrap_if_needed()`, after writing
`[AdvOut] RecEncoder`, also write `[AdvOut] Encoder` with the same probed
`encoder_id`. First-run only — the bootstrap already skips when
`FirstRunCompleted` is set, so existing users and any user-chosen streaming
encoder are never touched. NVENC sub-settings (rate control, 4500 kbps
bitrate, etc.) keep their current defaults; only the encoder selection
changes.

Update `.docs/obspt-bootstrap.md`: the encoder-probe contract now covers the
streaming encoder key as well as the recording encoder key.

## Acceptance criteria

- [x] First-run bootstrap writes the probed encoder to both
      `[AdvOut] RecEncoder` and `[AdvOut] Encoder` (one probe, two keys).
- [x] No streaming bitrate / rate-control / preset keys are written — the
      streaming sub-settings stay at their current defaults.
- [ ] Non-first-run boots do not touch `[AdvOut] Encoder` (bootstrap gating
      unchanged).
- [x] `test/win/obspt-installer-config-reset-smoke.ps1` static-asserts the
      bootstrap writes both keys.
- [x] `.docs/obspt-bootstrap.md` encoder-probe contract updated.
- [ ] Manual (fresh install on an N-GPU machine): Settings → Output →
      Streaming shows NVENC selected on first launch.

## Blocked by

None - can start immediately
