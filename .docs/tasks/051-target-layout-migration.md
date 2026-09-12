Status: review

## What to build

Migrate the project's generated-output layout to the `migrate-target-layout`
contract. Today the staging tree lives at `.target/_pkg/OBS-PT/` and the
installer at `.target/_pkg/OBS-PT-<version>-Installer.exe`, with no
provenance record. The contract mandates:

- `.target/app/` — the latest runnable build at a fixed path (for OBS-PT this
  is the full staging tree: `bin\`, `obs-plugins\`, `data\`, `obs-studio\`
  templates).
- `.target/installer/` — flat distribution packages (the NSIS installer
  output), so `_pkg` no longer holds the installer.
- `.target/delivery.json` — provenance proving current source and every
  delivered file (installer + staged tree hashes).
- build-output contract documented in `.docs/build-output.md`, updated to the
  new paths and refresh points.
- Old `_pkg` content disposition: displaced accepted version goes to
  `.target/app-prev/` at most one; obsolete artifacts removed per the
  skill's cleanup lifecycle (never delete the last usable build without an
  explicit approval).
- Aggregate shortcut retargeted to the new layout (accepted primary entry's
  direct parent, per the skill).

The release/package entry change: NSIS `OutFile` moves from
`.target/_pkg/...` to `.target/installer/...`, and the staging prefix moves
from `_pkg/OBS-PT` to `app/OBS-PT` (the fixed app path under the same
release entry), keeping the same CMake install → makensis pipeline.

## Acceptance criteria

- [ ] Installer lands at `.target/installer/OBS-PT-<version>-Installer.exe`
      via the unchanged NSIS pipeline.
- [ ] Staging tree lands at `.target/app/OBS-PT/` (flat-exe layout from 049;
      runnable app at a fixed path).
- [ ] `.target/delivery.json` exists and proves current source + the staged
      tree and installer hashes on each refresh.
- [ ] `.docs/build-output.md` rewritten to the new contract (app/ installer/
      delivery.json refresh points, build-input set, no legacy roots).
- [ ] `_pkg` old content disposed per skill lifecycle; at most one
      `app-prev/` retained during an acceptance window.
- [ ] Aggregate shortcut survives to the accepted entry's parent under the
      new layout.
- [ ] Full release/package run reproduces a verified installer (NSIS clean
      exit, `7z t` OK, SHA-256 recorded in delivery.json).

## Blocked by

- `049-flatten-64bit-layout.md`