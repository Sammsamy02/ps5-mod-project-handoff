# FIXES — Team D (#9 C++ mods, #11 C++ mod windows), answer to REVIEW.md (BLOCK)

Fixer, 2026-10-13. **Mac-only**: no console contact, no PRX link/package, nothing written outside this folder.
"Target compile" below means compiling the staged units to objects with R11's own compile commands
(prospero-clang++, LLVM 21.1.8), not linking or packaging a PRX.

## Status: every BLOCK and FIX-SOON finding is fixed (Mac-only). Both launches are still not schedulable until the mod PRX recipe has run (see "Still open").

| Finding | Status | What changed | Evidence (this session) |
|---|---|---|---|
| **B1** R11 never creates/starts C++ mods | **fixed** | New `cap09/PATCH/new/include/Platform/NativeCppModInventory.hpp` (engine-free planner). 4 exact-once hunks in `UE4SSProgram.cpp` (`apply_anchored.py`): the snapshot branch of `setup_mods` creates a `CppMod` per snapshot-enabled mod that has `dlls/`; `start_mods<CppMod>` starts them in startup order, each in its own try/catch. Enabled = `mods.txt` `Name : 1` rows, then snapshot `enabled.txt` (upstream rule). PRX path = `<mods directory>/<Name>/dlls/` (absolute, console-proven launch 14 line 15). **Load time decided:** inside `start_native_mods()` = during `module_start`, the only phase with `/data` known reachable (backend comment; launch 14 line 891 shows a `/data` create failing later). `NativeModSource` left unchanged on purpose (class layout is shared with the bootstrap's `make_shared`). Loadlist rule written down: only an `enabled.txt` row; never a `.prx` row (R11's activation refuses unknown roles: string `Unsupported UE4SS activation file role` in the R11 PRX). Boot-hang FAIL marker in the #9 card. | `test_startup_path` 27/27 (real tree on disk, real `CppModModule.cpp` loader, real port `FindNativeCppModModule`, order, refusals, default-off, bounded listing). `apply_anchored` 16/16 (anchors exact-once on the live tree). `check_sources` checks the hunks and that `setup_mods -> install_cpp_mods -> start_cpp_mods` order is unchanged. Target compile of `program` PASS. |
| **B2** `thread_local std::vector` -> `__cxa_thread_atexit` | **fixed** | `NativeModHostCore.hpp`: `constinit thread_local` fixed array (32) + depth, `static_assert` trivially destructible; deeper nesting is refused and counted. | New tool `cap09/tests/target_compile_check.py`: 0 new undefined symbols vs R11 `runtime-l26-A.o` (sha f247313c…) for cap09 (921 items) and the full #11 composition (2031 items), on the R11-era bases and on the live tree. Planted copy with the old vector: **FAIL `__cxa_thread_atexit`** (exactly the reviewer's finding) + lint FAIL (`cap09/tests/output/planted/`). `test_host_core` +4 checks (97). |
| **F1** first open does not block input; close can cancel the game's block | **fixed** | `NativeGuiHost.hpp`: no block at open time; block applied in `frame()` after `prepare()` resolved the player; remembered; unblock only if applied. Backend: block applied to and undone on that same player only, exactly the calls that succeeded (UE ignore-input is a counter). | `test_gui_host` 54 (+12): first open with no player yet, game-owned block survives, no unmatched unblock. With the old code planted back: 9 FAIL (`cap11/tests/output/planted/`). |
| **F2** window empty after a map change | **fixed** | `prepare(w, h, &rebuilt)`; on rebuild the host re-uploads the atlas and redoes the input block for the new player. | Same suite: rebuild test (atlas uploaded again, 0 blank frames, block on the new player, restored on close). Planted old code fails it. |
| **F3** cards skip verify/manifest for mod PRX | **fixed** | Both cards: `verify_build` + manifest gate for the exact sha of each mod PRX before upload; "not schedulable until the mod PRX has been built" stated at the top. | `cap09/LAUNCH-CARD.md`, `cap11/LAUNCH-CARD.md` |
| **F4** cards not specific (pad buttons, CROSS/d-pad) | **fixed** | Real buttons written in. With the 7 current Lua mods exactly ONE button pair is free (`L1+R2`; every other pair is taken, 3-button chords would break the no-containment rule). #9: `L1+R2 = F9`. #11: `L1+R2 = F10` (no F9 row, no tab keys: one tab). Proposed pad files checked with `keymap-gen.py --check`: 0 violations (1985 / 1986 bytes). Cards warn that CROSS/d-pad and L1/R2 still act in the game. | `cap09/PATCH/keymap/proposed/PS5Pad.cap09.ini`, `cap11/PATCH/keymap/proposed/PS5Pad.cap11.ini` |
| N1 "isolates crashes" | fixed (wording) | Cards + NOTES: an error switches the mod off; a real crash still crashes the game. | — |
| N2 delete/world facts REFUSED | documented | Comment in the binding; card lists them as "not failures". | — |
| N3 `call->function` null | fixed | Hook passes the UFunction. | target compile PASS |
| N4 return value leak | fixed | Unreadable return type refused before `ProcessEvent`. | target compile PASS |
| N5 tick per pump call | documented | Card + NOTES. | — |
| N6 "8 suites" | fixed | APPLY says 8 (cap09) / 11 (cap11) suites. | — |
| N7 PC DLLs never run | documented | Card + NOTES in plain words. | — |
| N8 ImGui defines depend on earlier includes | resolved | The first target compile of the #11 unit worked after 4 fixes it found: `IMGUI_DISABLE_SSE` (no `<immintrin.h>` in the PS5 sysroot), `IMGUI_DEFINE_MATH_OPERATORS` in `mbm_imconfig.h` (the unit includes `imgui.h` first), `UScriptStruct*` instead of `auto*` on `TObjectPtr` (3 sites), double `pow` in the batcher (it added a new `powf` import). | `target_compile_r11` / `target_compile_live` PASS |
| N9 Leon checker / brightness | unchanged | Not #9/#11 (agree with the reviewer). | — |

## Test counts (final run, real folder)

- `cap09/tests/run.sh`: **8/8 suites PASS** — check_sources 79, apply_anchored 16, api_usage_check 30,
  test_host_core 97, test_sdk_mods 35, test_startup_path 27, target_compile_r11 921, target_compile_live 888.
- `cap11/tests/run.sh`: **11/11 suites PASS** — test_math 19425, lint_patch 67, api_usage_gui 28,
  engine_names_check 55, test_gui_host 54, check_png 9, test_gui_sdk_mods 23, coverage_gate_v2 76,
  import_delta 17, target_compile_r11 2031, target_compile_live 1999.
- `run_gates.sh`: **PASS** — no_detour_gate 17 staged sources 0 sites; fs_throw_gate 17 files, 2 fs calls, both
  error_code overloads, 0 violations; no_detour `--patch` on 4 diffs PASS; full-copy differential on the live
  `UE4SSProgram.cpp`/`LuaMod.cpp`: 0 sites added (fs_throw 33 known sites before and after, no_detour 0).
- Runners: every suite must exit 0 and print a non-zero count; a short suite count fails the run. Proven with
  planted failures on a scratch copy: a failing check, a zero-count suite, the old `thread_local`, the old GUI
  code — each run exited 1 and named the failing suites (`*/tests/output/planted/`).

## CHANGES rows (regenerate with `make_rows.py` after any edit)

- cap09: `CppMod.cpp` a53c9be9 (new row); refresh `UE4SSProgram.cpp` live fe9fe7bc -> **2c67c2d2**, `LuaMod.cpp`
  live 1128b6eb -> **c2977068**; added `NativeModAbi.h` 3fcb5b10, `HookAdmission.hpp` 62f4e966,
  `NativeModHostCore.hpp` c25aa6a7, `NativeModLoaderPolicy.hpp` 85207768, `NativeCppModInventory.hpp` a9ea313f,
  `NativeModEngineServices.inc` 7843ee1c.
- cap11: `CppMod.cpp` c564d36e (unchanged); `mbm_imconfig.h` 6418aa45, `NativeGuiHost.hpp` 60441741,
  `ImGuiCanvasBatcher.hpp` e478faa8, `NativeGuiEngineBackend.inc` cb332991 (others unchanged).

## Still open / deferred (why)

1. ~~Mod PRX recipe never run~~ **Done 2026-10-13 (Mac-only):** both mod PRX files built, fake-signed, own unwinder,
   no new imports, pass-1 == pass-2, verify_build PASS: #9 `40e8b85a…`, #11 `c319ccba…` (`cap09/build-out/BUILD-RECEIPT.md`).
   Still needs one independent review of the new module entry before the first upload.
2. **Nested `sceKernelLoadStartModule` from `module_start`**: only the console can answer. Contained by the
   `[NMOD] loading C++ mod ...` marker, default-off (no `enabled.txt` = no PRX opened), rollback R11.
3. **The live candidate tree moved twice during this fix** (integrator merged cap #2/#4, then backed #2 out).
   Staged against live `UE4SSProgram.cpp` fe9fe7bc / `LuaMod.cpp` 1128b6eb. `apply_anchored.py --check` and
   `check_sources` detect any further drift; re-stage with `apply_anchored.py --stage` (anchors decide).
4. **Test order**: CLAUDE.md runs #11 before #9, but #11 needs #9's loader. Either swap them or accept that the
   #11 launch is also the first C++ mod load (the #11 card says so). Integrator/user decision.
5. **keymap-gen.py cannot add a key to a fully pinned pad map** (it picks a chord already pinned and stops with
   "duplicate controller chord"). Tool is outside this folder; the proposed pad files were made by hand and
   pass `--check`. They start from the launch-3 pad file: compare with the console's current one first.
6. Real compile + link + package of the runtime, `verify_build --manifest`: integrator, one build.
