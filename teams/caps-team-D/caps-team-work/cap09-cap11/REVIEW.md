# Independent review: Team D, #9 (C++ mods) and #11 (C++ mod GUI windows)

Reviewer: one independent reviewer (Claude), 2026-10-12. Everything here is **Mac-only**: I made no console contact,
built no PRX and edited nothing in the deliverable. Where I cite console evidence, it is the existing launch-14 log
`l25-prep/evidence/launch-l26-R11-launch14-20261013/ue4ss.decoded.log` (R11 bfd95db6).

## Verdict: BLOCK (as staged)

In plain words: the code is careful and the tests are real. But as staged, the #9 launch would show **nothing**: the
R11 runtime never creates or starts C++ mods at all. The build would also fail its own import gate. Both need fixing
before the one build. After that, two #11 bugs should be fixed before the #11 launch.

## Host tests re-run by me

I ran them on a copy in my scratchpad so the team's output files stay untouched. Same counts as the team reported, all PASS:

- cap09 `tests/run.sh`, 4 suites: check_sources 47, api_usage_check 30 (2949 files scanned), test_host_core 93
  (ASan+UBSan), test_sdk_mods 35. 128 compiled checks plus 77 source items.
- cap11 `tests/run.sh`, 9 suites: test_math 19425, lint_patch 39, api_usage_gui 28, engine_names_check 55/55,
  test_gui_host 42, check_png 9, test_gui_sdk_mods 23, coverage_gate_v2 76/76 (host only, 0 on console), import_delta 17
  (0 new). 65 compiled checks plus 19659 script items.

The tests are real: they fail when nothing is checked, and they cover refusals, faults and cross-thread stop. Their gaps
are what hides B1, B2, F1 and F2 below. Nothing tests the UE4SSProgram startup path. The import check covers only the
ImGui block, not the host core. The fake GUI backend never has an unresolved PlayerController and never rebuilds its surfaces.

## Checks I reproduced that hold

- `diffs/CppMod.cpp.diff` applies cleanly to the port `CppMod.cpp` (sha 5976afcf). That is the exact source of the `cppmod`
  unit in `l25-build-variants/l25-A/pass1/COMPOSITION-RECEIPT.json`, and the result is byte-identical to the staged
  candidate. The cap11 diff applied on top is byte-identical to the cap11 candidate. `LuaMod.cpp.diff` applies to today's
  candidate `LuaMod.cpp` (96e8ff4d; as the team said, this is not the R11 row 95591da5).
- `CHANGES-ROWS.json`: all 23 rows match the sha of the staged file.
- `no_detour_gate.py --root <copy of the 14 staged sources>`: PASS, 14 sources scanned, 0 sites.
  `fs_throw_gate.py`: PASS, 14 files checked, 0 std::filesystem sites.
- R11 link inputs (395 objects/archives, 341,959 defined symbols, scanned with llvm-nm): none defines or references `ImGui::`
  or any ImGui type. So the cppmod unit would be the only ImGui definer, and there is no ODR clash.
- The `imgui.h` and `imconfig.h` that R11's cppmod unit already depends on have the team's pinned shas (fa6d0dff…, a92534d9…).
- All 17 ImGui C symbols are already in the baseline undefined set.
- The GUI builds its window the same way as the console-proven item spawner (a raw UserWidget plus WidgetTree/CanvasPanel
  made with StaticConstructObject). Launch 14, line 1191: `[TownfallItemSpawner] Menu created.`
- The "official UE4SS example" test mod is genuine: all 41 of its non-comment lines appear in the local upstream guide
  `ue4ss/docs/guides/creating-a-c++-mod.md`.
- Hard rules: I found no game- or mod-specific names, offsets or addresses in the runtime code. There are no detours
  (code-detour families are always refused), no SDK material and no console steps for the team, and no mod files are touched.

## Findings

### B1 (BLOCK): R11 never creates or starts a C++ mod, so the #9 launch would log no `[NMOD]`/`[NPROBE]` line at all
- The shipped backend (`2026-10-04-zcode-team/worker-composition-3/.../ue4ss_backend.cpp:90,102,53`) always sets
  `context.mod_source` (the activation snapshot) and calls `start_native_mods()`.
- With `mod_source` set, `setup_mods()` builds **only LuaMods** and returns (candidate `UE4SSProgram.cpp:2096-2109`). The only
  `make_unique<CppMod>` is at :2150, after that return. `start_mods<CppMod>()` returns `{}` without starting anything
  (:2306-2317: `if constexpr (LuaMod)` only). `NativeModSource` itself lists only mods that have `Scripts/main.lua`
  (`port/ue4ss/src/Platform/NativeModSource.cpp:90-111`), so a `dlls`-only mod is invisible to it.
- Console-proven that R11 takes this branch: launch-14 log line 21 says `Setting up mods...`, and the log has 0
  `Loading mods from:` lines (the filesystem branch prints that line at :2120).
- What happens: the user uploads `Mods/NativeLifecycleProbe/dlls/main.prx` and launches. The patched CppMod code never runs,
  and the launch is wasted.
- Fix: a `UE4SSProgram.cpp` hunk, PROSPERO only. In the `mod_source` branch, create a `CppMod` for each enabled mod
  (enabled.txt or mods.txt) that has a `dlls/` folder, and make `start_mods<CppMod>` start them in startup order. Add a host
  test for this path. The fix must also choose **when** the PRX loads. Today `start_native_mods` runs inside the backend
  constructor, i.e. during the runtime PRX's `module_start` (backend :94 says "/data is still reachable" only then). A nested
  `sceKernelLoadStartModule` from `module_start` is unproven. The launch card then needs a FAIL marker for a boot hang
  (the log stops after `Setting up mods...`).

### B2 (BLOCK): the build's raw import gate will fail
- `NativeModHostCore.hpp:153` declares `inline thread_local std::vector<uint64_t> t_active_tokens`. A thread_local with a
  destructor makes clang emit a call to `__cxa_thread_atexit`. I checked this Mac-only: `/usr/bin/clang++ -target
  x86_64-unknown-freebsd -femulated-tls` on a minimal file shows `U __cxa_thread_atexit`.
- That symbol pulls `cxa_thread_atexit.cpp.o` out of the SDK's `libc++abi.a`, and that member carries a weak undefined
  `w __cxa_thread_atexit_impl` (llvm-nm).
- R11's `runtime-l26-A.o` has 0 references to `__cxa_thread_atexit` today, so the symbol set of the partial (`-r`) link
  changes. The baseline already counts one `w` line, so weak symbols are compared. `build_variants.py:1074-1075`
  (G7-import-delta-none) and `build_runtime.py`'s equal-set assert then refuse the build.
- The team's `import_delta.py` compiled only the ImGui block, so it could not see this.
- Fix: make the per-thread stack trivially destructible, e.g. `thread_local uint64_t t_active[32]; thread_local uint8_t
  t_depth;` with an overflow refusal. Then extend `import_delta.py` to the host core.

### F1 (FIX-SOON, before #11): the first time the window opens, gameplay input is not blocked; closing it can cancel a block the game owns
- `GuiHost::frame()` calls `set_open(true)` (NativeGuiHost.hpp:238, 313-314) **before** `prepare()` resolves `m_pc` (:246).
  `set_gameplay_input_blocked(true)` then returns early because `!valid(m_pc)` (NativeGuiEngineBackend.inc:517).
  On close, `SetIgnoreMoveInput(false)` / `SetIgnoreLookInput(false)` run on a valid PlayerController, but no matching
  `true` was ever sent.
- What happens: on the first F10, the character still walks and turns, so card step 2 fails. The close then sends a
  `false` with no matching `true`, which can undo an input block the game set itself (a cutscene or menu).
- Fix: resolve the PlayerController in the input-block call (or apply the block after `prepare`), and track whether the
  block was actually applied so the close is only sent if it was. Add a fake-backend test with an unresolved PlayerController.

### F2 (FIX-SOON, before #11): after a map change the window stays empty for the rest of the session
- When surfaces are rebuilt, the backend clears `m_atlas` (inc:439), but the host keeps `m_atlas_tex` (NativeGuiHost.hpp:249),
  so `create_atlas` never runs again and `submit()` returns at `!valid(m_atlas)` (inc:503).
- What happens: the user opens the window, then goes through a level or door transition (the old PlayerController is
  destroyed). From then on the window is blank.
- Fix: let `prepare()` report that its surfaces were rebuilt, so the host resets `m_atlas_tex`. Add a rebuild test.

### F3 (FIX-SOON): both launch cards skip a hard rule for the mod PRX
CLAUDE.md §2 says "No PRX upload without a fresh verify_build PASS and manifest gate for that exact sha". The cards only
ask for a decrypt-off readback of `main.prx`. Add an explicit verify and manifest step for each mod PRX's sha. Also say
plainly that neither launch can be scheduled until a mod PRX has actually been produced: the self-contained, fake-signed
build recipe has never been run.

### F4 (FIX-SOON): the launch cards are not specific enough for the user
The #9 card says "press the pad chord shown for F9 in PS5Pad.ini". The integrator should write the real buttons into the
card after running keymap-gen. In #11, only move and look input are ignored, so CROSS (and the d-pad) also do their normal
in-game action while the window is open. Either warn the user or bind keys the game doesn't use.

### Notes
- N1: "isolates crashes" overstates it. The host contains exceptions and error returns from a mod. A real crash in mod code
  (a bad pointer) still takes down the game. Say this plainly to the user.
- N2: `facts()` hard-codes `delete_listener_admitted=false` and `world_event_route_live=false` (inc:241-242), although both
  routes are live on R11 (launch 14: DeleteListener `Add returned`, line ~786; WORLD-EV routing, line 794). The probe
  will print `object-delete route=REFUSED` and `load-map live=0`. That is fine for ABI v1, but those lines must not be
  read as failures.
- N3: `mbm_hook_call.function` is always `nullptr` (inc:379), while the ABI header says it is the UFunction. Pass `fn`.
- N4: `call_function` checks the return type only after `ProcessEvent` has run, so a function returning an FString or
  array leaks that value. This is minor.
- N5: `GAME_THREAD_TICK` fires once per R3 carrier pump call, not once per frame (4 carriers armed, launch 14 lines
  787-790), and `dt` is shared between those calls. Document this.
- N6: cap11 APPLY.md says "8 suites"; run.sh has 9.
- N7: The goal is "unmodified PC mods". C++ mods have to be rebuilt from source against this SDK, and PC DLLs never run.
  The user should be told this plainly.
- N8: The ImGui block's `#define strncpy/strncmp/sqrtf` only works if `<cstring>`/`<cmath>` were included earlier in the
  same file (they are, today). The first target compile, which the team already listed as a blocker, will confirm.
- N9, the user's note (checkered Leon, outdoor scene too bright): I agree it is not caused by #9/#11. Nothing in this patch
  draws anything unless the F10 window is open. Mac-only suggestion for the #8/#12 owner: list which assets the Leon pak
  overrides (from its index, on the Mac). If it overrides any material, post-process or fog asset the level uses, the
  engine's default checker material and the missing darkness have one shared cause.

## What unblocks this
1. B1: add the UE4SSProgram.cpp hunk, decide when the PRX loads, and add a host test of the `mod_source` startup path.
2. B2: make the thread_local trivially destructible, and extend the import check to the host core.
3. Then the integrator's first target compile of `cppmod` and `luamod` (the team's blocker #1) and the mod PRX build recipe.
4. F1, F2, F3 and F4 before the #11 launch (F3 and F4 apply to #9 too). Rollback for both launches stays R11 `bfd95db6`.
