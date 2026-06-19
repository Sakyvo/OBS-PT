# Implement — rebuild OBS-PT on VS2022 v143 + confirm

## Toolchain reality (verified 2026-06-17)

- **No install needed.** VS2022 Build Tools (v143) already present at
  `K:/Microsoft Visual Studio/2022/BuildTools`, MSVC **14.44.35207**, with the
  VC++ x64 compiler component. (The "update to 17.14.35" prompt is optional —
  skip it.) The buggy compiler is VS Community 2026 at `C:/…/18/Community`.
- cmake: use the bundled one (supports VS17 generator):
  `K:/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe` (3.31.6).
- Mirrored build64 vars: `DepsPath=K:/Projects/dev/OBS-PT/deps2019/win64`,
  `QTDIR=C:/Qt/5.15.2/msvc2019_64`, `OBS_VERSION_OVERRIDE=0.0.1`,
  `BUILD_BROWSER=OFF`, `BUILD_VST=OFF`, `COPY_DEPENDENCIES=ON`,
  `DISABLE_UPDATE_MODULE=TRUE`, `ENABLE_QSV11=ON`, `ENABLE_WINMF=FALSE`.
  (deps2019/Qt-msvc2019 link fine into v143 = v142/v143 ABI compat — confirmed
  at configure.)

## Ordered steps

1. ~~Install v143~~ — **DONE/N/A** (already installed, see above).
2. **Configure fresh `build-v143/`** (DONE 2026-06-17, "Build files written"):
   `cmake -S . -B build-v143 -G "Visual Studio 17 2022" -A x64 -DCMAKE_GENERATOR_INSTANCE="K:/Microsoft Visual Studio/2022/BuildTools" -DDepsPath=… -DQTDIR=… -DOBS_VERSION_OVERRIDE=0.0.1 -DBUILD_BROWSER=OFF -DBUILD_VST=OFF -DCOPY_DEPENDENCIES=ON -DDISABLE_UPDATE_MODULE=TRUE -DENABLE_QSV11=ON -DENABLE_WINMF=FALSE`
3. **Build**: `cmake --build build-v143 --config RelWithDebInfo --target obs --parallel`
   → `build-v143/UI/RelWithDebInfo/OBS-PT.exe` + `obs.dll`/`libobs*.dll`. [RUNNING]
4. **Deploy** to `K:/Projects/finished/OBS-Redux` (back up VS18 binaries first):
   bin/64bit/{OBS-PT.exe, obs.dll, libobs.dll, libobs-d3d11.dll, libobs-winrt.dll};
   keep data/ as-is.
5. **A/B record** (user): same PotPvP scene, ~2 min @480fps; locate the new log.
6. **Measure**: read `skipped frames due to encoding lag` % from the new log;
   compare vs 4–5% baseline and official 0.1–0.5%.
7. **Reconcile pragma**: keep matrix4.c/matrix3.c guards (harmless on v143).
8. **Spec update**: add encoding-lag manifestation + "build with v143, not VS 18
   2026" requirement to `.trellis/spec/obsredux-graphics-msvc.md`.

## Validation commands

- Build success: `OBS-PT.exe` exists under `build-v143/UI/RelWithDebInfo/`.
- Fix success: post-rebuild log encoding-lag ≤ ~0.5% on the same scene.

## Risky points / rollback

- Step 5 needs a real-machine recording (cannot be automated).
- Rollback: redeploy VS18 binaries from `build64/` (untouched).
