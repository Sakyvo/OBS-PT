# Build Output Contract

All project-controlled generated output lives under the project-root `.target/`.
No legacy roots (`build/`, `target/`, `dist/`, `out/`) exist or may be created.

## Subtrees

- `.target/` (root) — CMake/VS2022 build tree (`cmake -B .target`), including
  `rundir/RelWithDebInfo/` with the runnable unpacked build.
- `.target/_pkg/OBS-PT/` — staging root refreshed from `rundir` plus the
  shipped `UI/data/obspt-defaults/` tree (see `.docs/obspt-bootstrap.md` §4).
- `.target/_pkg/OBS-PT-<version>-Installer.exe` — NSIS installer output.
- `.target/Testing/`, per-test binaries — validation evidence; keep only the
  latest run per issue, clean at batch wrap-up.
- `.target/_amf-sdk/` — staged AMF SDK headers (stable build input cache).

## Release/package entry

`cmake --install .target --config RelWithDebInfo --prefix .target/_pkg/OBS-PT`,
then exactly one `makensis.exe UI/installer/obspt-setup.nsi` (verify no prior
makensis process; wait for normal exit; then `7z t`, SHA-256, timestamp, and a
real launch to the `OBS-PT <version> Setup` window).

## Primary form and entry

The installer EXE above is the delivered form. The day-to-day runnable form is
`.target/_pkg/OBS-PT/bin/64bit/OBS-PT.exe`. This project ships through the
staging→installer pipeline; the generic `.target/app/` layout is not used.

## Refresh points

Any change affecting an EXE or plugin DLL: after build verification, refresh
staging AND the installer (AGENTS.md 常驻法则). Packaging runs with
`BUILD_TESTS=OFF` so test plugins never enter staging.

## Build-input set

`UI/`, `libobs/`, `libobs-*/`, `plugins/`, `cmake/`, `CMakeLists.txt`,
`deps2019/`, `C:/Qt/5.15.2/msvc2019_64` (external), NSIS script. Provenance is
recorded per candidate via installer timestamp + SHA-256 + `7z t` file count.

## Budget / shortcut

No budget override: default thresholds apply (report at ≥20 GiB, approval for
second full cache at ≥50 GiB). Current baseline ~1.82 GiB.
Shortcut `K:/Projects/targets/OBS-PT.lnk` must keep pointing at `.target/`.
