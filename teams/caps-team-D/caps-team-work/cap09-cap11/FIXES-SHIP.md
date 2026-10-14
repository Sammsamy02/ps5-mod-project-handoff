# FIXES-SHIP — Team D (#9 C++ mods, #11 C++ mod windows), answer to REVIEW-SHIP.md (PASS-WITH-FIXES)

Fixer, 2026-10-13. **Everything here is Mac-only.** There was no console contact. No runtime PRX was built. Nothing
was written outside this team folder: the candidate tree, the frozen builds, the mod files and the other teams'
folders were not touched. Planted-failure runs used scratch copies. No new review was done, as instructed.
Re-staged against the live candidate tree R13c (`UE4SSProgram.cpp` fe9fe7bc, `LuaMod.cpp` 1128b6eb, unchanged since
the review). `apply_anchored.py --check` **PASS, items_checked=25, failed=0**.

## Status: F1-F7, N2 and N7 are fixed. N1 and N3-N6, N8-N10 are deferred or documented (reasons below).

| Finding | Status | What changed | Evidence (this session, Mac-only) |
|---|---|---|---|
| **F1** throwing `is_regular_file` reachable at boot | **fixed** | The port header `NativeCppModPath.hpp` is now **staged**: 3 exact-once anchored hunks in `apply_anchored.py`, base = the pinned port file 4286deac, output `cap09/PATCH/candidate/include/Platform/NativeCppModPath.hpp` (cb77ce6f), with a CHANGES `files_rows` entry like the other port-header rows. The build's header overlay maps the port path to the candidate copy. The probe is now `status(path, ec)`. A missing file or dangling link means "try the next name". Any other OS error refuses that mod with `dlls folder could not be checked (OS error N)`: `CppMod.cpp` passes an `error_code*` in both the cap09 and the cap11 versions. **Gate extension:** `run_gates.sh` §4 + new `reach_files.py` run fs_throw_gate on **every file the patch makes reachable**: the clang `-MM` closure of the patched cppmod/program/luamod units built with R11's own commands, plus the module loader `CppModModule.cpp/.hpp`. That is 458 files. §4a: 0 new sites against the project baseline across the closure. §4b: the 358 never-executed files (cppmod closure + loader) must have 0 sites. The only exception is baseline debt inside a function the cppmod unit provably never uses (an `-O0` compile + `llvm-nm`, with a positive control that does see `FindNativeCppModModule`, and a pinned file sha). | `test_startup_path` 33/33. New F1 block: EACCES gives no throw, no module, and errno reported. Control: the old throwing overload *does* throw on this host. A dangling link and `<Name>.prx` fallback are not errors. The first attempt with `is_regular_file(path, ec)` was **caught** by this suite: it reports ENOENT through `ec`, so every mod would have been refused. `run_gates.sh` **PASS**: 19 staged files, 3 fs calls, all with error_code. Closure 456 files, 0 NEW. Newly executed 358 files, 3 flagged, 3 accepted (`ValidateNativeProgramContext`, proven unused by the cppmod unit), 0 failed. **Planted** (staged header with the throwing call put back, old §1 scope): §1 PASS (the old blind spot), §4a FAIL, §4b FAIL naming `NativeCppModPath.hpp:35` (`cap09/tests/output/planted-ship/`). |
| **F2** one bad C++ mod skips the rest | **fixed** | H3 loop: one `try`/`catch (std::exception)`/`catch (...)` **per candidate**. The start-order row is written before the module is created and kept, so a mod in `m_mods` is always in the start order. A row whose module was not kept is reported by the start step. The outer try now covers only the planner. | `check_sources` 93/93, including the order check row → create → keep → catch. Target compile of `program` PASS, 0 new imports. |
| **F3** hot reload drops C++ mods / re-enters a resident image / 32-reload exhaustion | **fixed (refuse loudly)** | New hunk **H5** in `queue_reinstall_mods`. While any native C++ mod is present it logs `[NMOD] refused: hot reload of all mods while N native C++ mod(s) are loaded; nothing was reloaded, every mod keeps running (restart the game to reload)` and returns **before** `uninstall_mods()`. There is no re-dlopen, no silent drop, and every mod keeps running. Without C++ mods it is the R11 path. The host also prunes `Stopped` records at attach, so stop/attach cycles never exhaust `max_mods` (`Retained` records are kept). | `check_sources`: the refusal comes before `uninstall_mods()`. `test_host_core` (+3): 40 attach/stop cycles with `max_mods=2` all succeed, records stay ≤ 2, and the limit still counts live mods. **Planted** (no prune): 1 FAIL. |
| **F4** callback table only grows | **fixed** | `revoke_locked` and `abi_unregister` now **erase** entries after the engine hook has been removed. A late engine call with an erased id finds nothing, which is the same outcome as an inactive entry. Key binds the input handler keeps resolve to nothing. The per-pump walk is bounded by active registrations. A second unregister of the same id now returns `MBM_E_NOT_FOUND` (ids are never reused). | `test_host_core` (+7): 1000 register/unregister cycles leave the table at 0. Unregister removes the engine hook first, then erases. A fault erases all 11 entries and removes the hook. Late hook and key calls reach nothing. **Planted** (no erase): 5 FAIL. |
| **F5** GUI rendered per pump call | **fixed** | New engine-free `FrameGate` (`NativeModHostCore.hpp`) used by `NativeModsGameThreadFrame` around `after_frame` (the #11 GUI render). Once the copied-vtable Tick pump has been seen in the last 250 ms, only it drives the frame (4 ms floor). Otherwise the marshal pump drives it, at most once per 8 ms. `dt` is the time since the previous frame that ran. Mod `GAME_THREAD_TICK` callbacks are unchanged (still per pump call, as documented in N5 of the first review). The #11 card adds a `[PERF]`-with-window-open check. | `test_host_core` (+8): 2000 marshal calls/s give 125 frames. With both pumps live, only the 61 VMT ticks draw. Handover after 250 ms. At the R11 cadence (1 call per frame) 60/60 frames are drawn and none are lost. **Planted** (no gate): 6 FAIL. |
| **F6** services work for a stopped/faulted mod | **fixed** | `service_gate(token)`: an unknown token returns `MBM_E_INVALID`. A known mod that is not Starting/Running gets `MBM_E_STATE` (counted as a refusal) on every `abi_*` service that takes a mod token. `log` stays open while `destroy` runs (Stopping), so a destructor can still log, and is dropped after a fault or stop. `unregister` stays allowed (it only removes). The GUI's `register_tab` already checked state. | `test_host_core` (+7): after a fault, 8 services all return `MBM_E_STATE`, the property write does not reach the object, the log is dropped, and an unknown token stays INVALID. A log inside `destroy` is allowed. **Planted** (gate off): 4 FAIL. |
| **F7** ImGui check ignores the context layout | **fixed (fail closed)** | ABI `mbm_gui_v1`: `reserved` → `sizeof_context` (same offset; an old host's 0 is refused). New `config_fingerprint` + `reserved2` at the end (an old host's shorter `struct_size` is refused). New `Platform/mbm_imgui_fingerprint.h` (546aeffa) hashes 29 config macros, the version, and the size/alignment of `ImGuiContext` and every other shared ImGui struct. The host publishes it. The SDK refuses `UE4SS_ENABLE_IMGUI()` with the two values in the log line. Both copies of `NativeModAbi.h` are identical (42f8fd12). | `test_gui_sdk_mods` 27/27, including a new 4th mod built **without `IMGUI_DISABLE_DEBUG_TOOLS`** (the reviewer's example). It passes the old six-size check but is refused: `host ImGuiContext size 10744 / config fingerprint 1732667309 differ from the mod's 10776 / 4280214417`. Its tab is disabled. The host table publishes both values. The other 3 mods behave as before. |
| **N2** `enable_imgui()` outside the SDK protection | **fixed** | `tab_tramp` runs `enable_imgui()` inside `Sdk::contain`. A `bad_alloc` there becomes `MBM_E_MOD_FAULT` instead of terminating. | Both suites pass. Both mod PRX files rebuilt (below). |
| **N7** `FindFirstOf("PlayerController")` every frame in menus | **fixed (cheap)** | With no player controller, the window retries surface building at most every 500 ms. | `api_usage_gui`, `lint_patch`, target compile of the #11 composition all PASS. |
| N1 nested `sceKernelLoadStartModule` in `module_start` | deferred (console-only) | Unchanged. The `[NMOD] loading C++ mod ...` marker, default-off, and the `enabled.txt` kill switch stay. | — |
| N3 `valid()` reads a freed pointer once | deferred (documented) | Same risk class as upstream raw `UObject*`. | — |
| N4 only `call_function` checks the thread | deferred (documented) | Matches upstream UE4SS. | — |
| N5 "default-off" is not zero change | documented | About 1 µs per pump call, one directory listing per boot, more than 256 entries refuses C++ mods. Unchanged. | — |
| N6, N8, N9, N10 | no change needed | N8: keep the `nm` step in cap11 APPLY step 2 (still there). | — |

## Mod PRX files (rebuilt: their SDK sources changed for N2/F7). Recipe unchanged.

| File | new sha256 | bytes | imports | pass-1 == pass-2 | verify_build --manifest |
|---|---|---|---|---|---|
| #9 `NativeLifecycleProbe/dlls/main.prx` | `a394b76d17ac08abf47b3dbb29435a0959452c2d320e63eda088793379e90f95` | 961193 | 92: the same names and the same mod contract (2a249707) as before, **0 new** | yes (13 files) | PASS 19/19 markers, both passes |
| #11 `NativeGuiProbe/dlls/main.prx` | `ba4675085c601a6ce8dbaafdc4cb62e244344c8484f23d080918c4d72cc0b5ad` | 1774841 | 99: the same names and the same contract (8053d96b), **0 new** | yes (14 files) | PASS 14/14 markers (new marker = the F7 refusal text), both passes |

The old files `40e8b85a…` and `c319ccba…` are superseded. The receipt and both cards say "do not upload".
`mbm_mod_module_entry.cpp` is unchanged (object 101ca663… identical) and still needs its one review before the first
upload. Details: `cap09/build-out/BUILD-RECEIPT.md` (ship-fix section), `REPRO-CHECK.ship.txt`, `verify_build.*-pass-*.txt`.

## Test counts (final run, real folder; every runner fails on a zero count or any failure)

- `cap09/tests/run.sh`: **8/8 PASS**. check_sources 93, apply_anchored 25, api_usage_check 30, test_host_core 122,
  test_sdk_mods 35, test_startup_path 33, target_compile_r11 921 (0 new undefined), target_compile_live 888 (0 new).
- `cap11/tests/run.sh`: **11/11 PASS**. test_math 19425, lint_patch 72, api_usage_gui 28, engine_names_check 55,
  test_gui_host 54, check_png 9, test_gui_sdk_mods 27, coverage_gate_v2 76, import_delta 17 (none new),
  target_compile_r11 2031 (0 new), target_compile_live 1999 (0 new).
- `run_gates.sh`: **PASS**. no_detour: 19 staged files, 0 sites. fs_throw: 19 files, 3 fs calls, all error_code.
  no_detour `--patch` on 5 diffs: PASS. Full-copy differential: 0 sites added. Reachable closure: 458 files, 0 new
  against the baseline. Newly executed: 358 files, 0 unaccepted.
- `apply_anchored.py --check` on the live tree: 25/25. An `--apply-to` rehearsal on a scratch copy of the two live
  files produced byte-identical candidates and the port-original copy. A second apply was refused (fail closed).
- Planted regressions (`cap09/tests/output/planted-ship/SUMMARY.txt`): F3 1, F4 5, F5 6, F6 4 checks FAIL; the
  F1 gate run FAILs.

## Shas for the integrator (regenerated rows: `cap09/PATCH/CHANGES-ROWS.json`, `cap11/PATCH/CHANGES-ROWS.json`)

- cap09: `CppMod.cpp` 3276791f, **`NativeCppModPath.hpp` cb77ce6f (new port-header row)**, `UE4SSProgram.cpp`
  fe9fe7bc → **ec96165e**, `LuaMod.cpp` 1128b6eb → c2977068 (unchanged), `NativeModAbi.h` 42f8fd12,
  `NativeModHostCore.hpp` 7428151e, `NativeModEngineServices.inc` e0fef6f7 (the others are unchanged).
- cap11: `CppMod.cpp` fcc5e79d, `NativeGuiHost.hpp` ca902109, `NativeGuiEngineBackend.inc` eaa356b9,
  **`mbm_imgui_fingerprint.h` 546aeffa (new)**.

## Still open

1. Nested PRX load from `module_start` (N1): only the console can answer this.
2. One review of `mbm_mod_module_entry.cpp` before the first mod upload (unchanged requirement).
3. Test order (#11 before #9 in CLAUDE.md, but #11 needs #9's loader): integrator/user decision, as before.
4. Real compile, link and package of the runtime, plus `verify_build --manifest`: the integrator's one build.

## In plain words

All seven fixes the reviewer asked for are in, plus two small extras. One bad C++ mod can no longer stop the others,
a reload can no longer silently drop C++ mods, and the mod window is drawn at most once per frame. A mod built with
different window settings is now refused instead of risking a crash. Both mod files were rebuilt and still use only
functions the game already loads. Nothing has been tried on the console yet.
