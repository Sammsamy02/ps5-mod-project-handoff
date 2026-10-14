# cap09 APPLY — native C++ mod ABI v1 (integration steps)

Mac-only staging. Nothing here was linked, packaged or deployed. The staged units were
**target-compiled** (Mac-only, objects only) with R11's own compile commands and checked for new
imports against R11's runtime object (`tests/target_compile_check.py`).
`W` = `/WORKSPACE/.codex/worktrees/translation-core/what`, `T` = `W/outputs/2026-10-08-l26-runtime/typescan-r4`,
`P` = this `PATCH/` folder.

## What changes in the runtime

| File | Change |
|---|---|
| `T/candidate/src/Mod/CppMod.cpp` (NEW candidate of the `cppmod` unit) | Prospero only: a native mod module is admitted only if it exports `mbm_mod_entry_v1`; a `start_mod`-only module is refused with a rebuild message. Lifecycle goes through the host core. ABI-v1 modules stay resident. `CppMod.hpp` unchanged. |
| `T/candidate/src/Mod/NativeModEngineServices.inc` (NEW) | Runtime binding (reflection, properties, UFunction calls, per-UFunction data-only hooks, keys, game-thread pump). Fix N3: hooks pass the UFunction. Fix N4: an unreadable return type is refused before the call. |
| `T/candidate/src/UE4SSProgram.cpp` (5 anchored hunks, **B1**, ship fixes **F2**/**F3**) | Under the activation snapshot (always the case on the console) `setup_mods` now also creates a `CppMod` for every snapshot-enabled mod that has a `dlls/` folder, and `start_mods<CppMod>` starts them in startup order. **F2:** one try/catch per mod (a refusal or exception skips that mod only, never the mods after it or the Lua mods); the start-order row is written before the module is kept. **F3 (H5):** `queue_reinstall_mods` refuses, with a warning line and before anything is uninstalled, while any native C++ mod is present (no re-dlopen of a resident image, no silent drop); without C++ mods it is the R11 path. |
| `T/candidate/include/Platform/NativeCppModPath.hpp` (NEW candidate of a PORT header, 3 anchored hunks, **F1**) | The `dlls/` lookup the patch makes reachable at boot probes with `std::filesystem::status(path, ec)` only (no throwing `is_regular_file`); an OS error refuses that mod with its error number (`CppMod.cpp` passes the `error_code*`), a missing file / dangling link is "try the next name". CHANGES `files_rows` entry (original = port header, like the other port-header rows; the build's header overlay maps the port path to this copy). |
| `T/candidate/src/LuaMod.cpp` (3 anchored hunks) | Calls `NativeModsGameThreadFrame(0)` in the R3 marshal drain and `(1)` in the VMT Tick drain. Same hunks as before. |
| `T/include/Mod/NativeModAbi.h`, `T/include/Platform/{HookAdmission,NativeModHostCore,NativeModLoaderPolicy,NativeCppModInventory}.hpp` (NEW) | C ABI, hook admission, engine-free host core (**B2**: per-thread stack is now a trivially destructible fixed array, so no `__cxa_thread_atexit`), entry selection, **B1** startup planner. Ship fixes in the host core: **F3** stopped records pruned (no `max_mods` exhaustion); **F4** revoked / unregistered registrations erased after the engine hook is removed (bounded per-pump walk); **F5** `FrameGate` (the per-frame service = #11 GUI render runs at most once per engine frame; `NativeModEngineServices.inc` uses it); **F6** every service refuses (`MBM_E_STATE`) a mod that is not starting/running (log also open while `destroy` runs). ABI: `mbm_gui_v1.reserved` is now `sizeof_context`, plus `config_fingerprint` at the end (**F7**, #11). |

No new import (target delta 0 vs R11 `runtime-l26-A.o`), no code patch, no address, no game name.
`NativeModSource.cpp/.hpp` stay unchanged on purpose: changing the class would change its layout,
which the bootstrap's `make_shared` also uses.

## Which C++ mods start, and when (B1)

- **Enabled** comes from the activation snapshot only, upstream rule: `Name : 1` rows of `ue4ss/Mods/mods.txt`
  in file order, then mods whose `ue4ss/Mods/<Name>/enabled.txt` is in the snapshot, by name.
- **Where**: `<mods directory>/<Name>/dlls/main.prx` on disk (the absolute directory R11 already logs as
  `mods directories: [0] ...`, console-proven in launch 14 line 15). No absolute directory = no C++ mod.
- **Loadlist**: add ONE row per C++ mod, `file	config	ue4ss/Mods/<Name>/enabled.txt	0`.
  **Never list `dlls/*.prx` in the loadlist**: R11's activation refuses every role except
  lua_mod/config/script (string `Unsupported UE4SS activation file role` in the R11 PRX), and that would stop the
  whole runtime.
- **When the PRX loads**: inside `start_native_mods()`, i.e. during the runtime's `module_start`. That is the only
  phase in which `/data` is known to be reachable (backend comment; launch 14 line 891 shows a `/data` folder create
  failing later). A nested `sceKernelLoadStartModule` from `module_start` is unproven: the log line
  `[NMOD] loading C++ mod <Name> from ...` is printed right before it, so a hang there is visible.
- Kill switch without a rebuild: remove the `enabled.txt` row (and file). Then no PRX is opened.

## Steps

1. Re-verify the bases: `cap09/tests/run.sh` (8 suites). If the live `UE4SSProgram.cpp` / `LuaMod.cpp` moved on,
   `apply_anchored.py --check` reports DRIFT; it still patches only if every anchor occurs exactly once
   (3 files, 12 hunks, `items_checked=25` on `--check`).
2. Copy files (`P/CHANGES-ROWS.json`: `files_rows`, `added_files`), then patch the two live files:
   `python3 -I P/apply_anchored.py --apply-to T` (writes nothing unless all 12 anchors are exact-once; for the
   port header it writes `T/candidate/include/Platform/NativeCppModPath.hpp` and the `T/original/...` copy, and
   refuses if either already exists with other content).
   Refresh their CHANGES rows from `files_rows_refresh` (regenerate with `python3 -I P/make_rows.py`).
3. Gates: `cap09/tests/run.sh` PASS, `../run_gates.sh` PASS (no_detour + fs_throw on all staged sources, and
   since the ship fixes **fs_throw on every file the patch makes reachable**: the compiler include closure of the
   patched units + the module loader, 0 new sites vs the project baseline, 0 sites at all in never-executed code
   except baseline debt in a function the cppmod unit provably never uses),
   then the real compile/link of `cppmod`, `program`, `luamod`; one build; `verify_build.sh --manifest` for the
   exact runtime PRX; imports must stay 207.
4. Mod PRX: built (Mac-only) with `cap09/build-out/recipe/build_mod_prx.py`: current file
   `a394b76d17ac08abf47b3dbb29435a0959452c2d320e63eda088793379e90f95` (`cap09/build-out/BUILD-RECEIPT.md`, ship-fix
   rebuild). `verify_build` + manifest gate for **that exact mod PRX sha** (CLAUDE.md §2) before any upload.
5. Pad: F9 -> `L1+R2` (the only free chord; `P/keymap/proposed/PS5Pad.cap09.ini`, 1985 bytes, keymap-gen `--check`
   0 violations against the launch-3 map). Diff it against the console's current PS5Pad.ini first, re-pin the
   size row, row-check. (keymap-gen cannot add a key to a fully pinned map: it picks an occupied chord and
   stops with "duplicate controller chord"; integrator tool issue, outside this folder.)

Rollback: R11 `bfd95db6` (last clean build).
