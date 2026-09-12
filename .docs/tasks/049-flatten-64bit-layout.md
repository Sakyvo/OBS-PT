Status: review

## What to build

Eliminate the `64bit` directory layer from the shipped layout, so a finished
install is fully flat: `bin\OBS-PT.exe` (with all its DLLs) and
`obs-plugins\*.dll` with no `64bit` subfolder anywhere. OBS-PT is permanently
x64 and does not track upstream, so the architecture-suffixed folders
(`bin\64bit`, `obs-plugins\64bit`) carry no meaning and only create the
"套娃" nested-folder confusion seen in live installs.

This is a full-chain change, not a packaging-only rename: moving the EXE out
of `bin\64bit` breaks the runtime relative paths that resolve the plugin
directory, so the build, the runtime loader, and the auxiliary tools must all
agree on the new flat layout.

## Acceptance criteria

- [x] CMake Windows install destinations no longer emit `64bit` subfolders:
      executables/libraries install to `bin`, plugins to `obs-plugins`, and
      `OBS_RELATIVE_PREFIX` is `../` so data paths resolve from `bin`.
- [x] libobs runtime loader resolves plugins from the flat location: the
      default module bin path is `../obs-plugins` (relative to the exe
      directory) and `find_libobs_data_file` resolves `../data/libobs`.
- [x] The runtime adds the exe-adjacent module path without an arch suffix
      (`bin` not `bin/64bit`), and the `ARCH_BITS` conditional that selected
      `bin/64bit` is removed (permanently x64).
- [x] Updater and frontend-tools no longer reference `obs-plugins\64bit`.
- [x] A full install produces a layout with no `64bit` directory at any level
      (`bin\OBS-PT.exe`, `obs-plugins\*.dll`, `data\...`).
- [ ] OBS-PT still launches, loads all plugins, and records/streams after the
      layout change.

## Blocked by

None - can start immediately

## Regression fix (post-implementation)

The flat layout moved the exe from `bin\64bit\OBS-PT.exe` to `bin\OBS-PT.exe`,
but `resolve_install_root()` in `libobs/util/platform-windows.c` still walked up
**3** levels from the exe path to find the Install Root. With the flat layout
that resolves to the drive root (`D:\`) instead of `D:\OBS-PT`, producing the
startup error `D:/obs-studio/global.ini`. Fixed by walking up **2** levels
(`bin/OBS-PT.exe -> bin -> <Install Root>`). This is the only up-walk point;
all other `strrchr` uses are filename extraction, unrelated.

## Regression fix 2 (post-implementation)

`libobs/obs-config.h` is a **hardcoded source file** (not CMake-generated) that
defines `OBS_DATA_PATH "../../data"` and `OBS_RELATIVE_PREFIX "../../"` (old
values). Issue 049 changed the CMake variables in `ObsHelpers.cmake` but forgot
to update this hardcoded file. At compile time `OBS_DATA_PATH` was still
`"../../data"`, so `GetDataFilePath` resolved `../../data/obs-studio/` from
`bin\` up 2 levels to the drive root (`D:\`), then into `data/obs-studio/` =
`D:\data\obs-studio\` (nonexistent) → locale loading failed → UI showed raw
key names instead of translated display names. Fixed by changing the hardcoded
values to `"../data"` and `"../"` (flat layout: exe in `bin/`, data one level
up).

## Regression fix 3 (post-implementation)

`libobs/obs-windows.c` `module_data` was still `"../../data/obs-plugins/%module%"`
(old value). Issue 049 changed `module_bin` from `"../../obs-plugins/64bit"` to
`"../obs-plugins"` but forgot to change `module_data`. From `bin\` up 2 levels
to the drive root (`D:\`), then into `data/obs-plugins/<plugin>` =
`D:\data\obs-plugins\<plugin>` (nonexistent) → plugin data files
(services.json, locale) failed to load → streaming page showed empty/garbled
service list, output page showed raw key names instead of translated display
names. Fixed by changing `module_data` to `"../data/obs-plugins/%module%"`
(flat layout: exe in `bin/`, plugin data one level up).
