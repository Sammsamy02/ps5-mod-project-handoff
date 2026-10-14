# REVIEW-SHIP — #9 (C++ mods) + #11 (C++ mod windows), shipping code only

Independent reviewer, 2026-10-13. **Mac-only**: no console contact. Nothing was applied to the live candidate tree.
The suites ran on a scratch copy of this folder (team outputs untouched). The only file written here is this one.

## Verdict: PASS-WITH-FIXES

No BLOCK. With no `enabled.txt` for a C++ mod, R11 behaviour is unchanged apart from one cheap call per pump and one
directory listing at boot. Both are contained, and no exception can escape. The FIX-SOON items are small. F1-F4 are
worth doing before the #9 build, and F5-F7 before #11.

Checked and clean:
- No code detour: hooks use only `RegisterPreHook`/`RegisterPostHook`, the existing data-only Func-pointer swap.
- No new import: my re-run of `target_compile_check` found 0 new undefined symbols for cap09 and for the full #11
  composition.
- No hand-written offset or address. The GUI reads struct layouts from reflection.
- No game-specific or mod-specific name: only `/Script/Engine`, `/Script/UMG` and `/Script/CoreUObject` class names.
- No throwing filesystem overload in the staged files. F1 is a throwing call in an existing port header that the
  patch now makes reachable.
- Thread-local storage: the runtime already uses emulated TLS (`__emutls_get_address`, many `__emutls_t.*` in R11
  `runtime-l26-A.o`), so the new `constinit thread_local` adds no TLS segment or import.

## What was reviewed

- The `apply_anchored.py --check` result against the live tree: 16/16 pass.
- The live shas match the staged base: `UE4SSProgram.cpp` fe9fe7bc, `LuaMod.cpp` 1128b6eb.
- The 4 UE4SSProgram hunks and the 3 LuaMod hunks.
- The staged files: `CppMod.cpp` (cap11 version), `NativeModEngineServices.inc`, `NativeModHostCore.hpp`,
  `NativeCppModInventory.hpp`, `NativeModLoaderPolicy.hpp`, `HookAdmission.hpp`, `NativeModAbi.h` (identical to the
  SDK copy), `NativeGuiEngineBackend.inc`, `NativeGuiHost.hpp`, `NativeGuiImGuiUnity.inc`, `mbm_imconfig.h`,
  `ImGuiCanvasBatcher.hpp`, `StoredPng.hpp`.
- The mod side: `mbm_mod_module_entry.cpp`, `mbm_sdk_entry.cpp`, `build_mod_prx.py` (structure and fail-closed gates).
- The port code that the patch now reaches: `NativeCppModPath.hpp`, `CppModModule.cpp`, the input handler locking.

## Findings

### FIX-SOON

**F1. A throwing filesystem call becomes reachable at boot.**
- Where: port `include/Platform/NativeCppModPath.hpp:26`, which uses `std::filesystem::is_regular_file(path)` with no
  `error_code`. It is called from `CppMod.cpp:130` (cap11 candidate).
- Why it matters now: in R11 this was dead code on the console, because no CppMod was ever built under the snapshot.
  With the patch it runs during `module_start` for every enabled C++ mod. It sits in the fs_throw baseline as
  existing debt, but the team gate scans only the 17 staged files, so it never sees this call.
- Failure scenario: an OS error on `dlls/main.prx` (permission or I/O error, or a dangling entry) throws
  `filesystem_error` during `module_start`. The catch at `CppMod.cpp:161` contains it, so this is not a crash, but it
  breaks the rule the gate exists for.
- Fix: in the cap09 CppMod candidate, do the 6-name lookup with `is_regular_file(path, ec)`, or ship a candidate copy
  of `NativeCppModPath.hpp` that uses the `error_code` overload.

**F2. One bad C++ mod skips every C++ mod after it.**
- Where: `UE4SSProgram.cpp` staged lines 2177-2210. A single try block wraps the whole loop over
  `inventory.start_order`.
- Failure scenario 1: an exception from candidate N (`make_unique<CppMod>`, or `Output::send`/`widen_ascii` throwing
  `bad_alloc`) silently drops candidates N+1 onward.
- Failure scenario 2: if the throw comes after `emplace_back` but before `native_cpp_start_order().push_back`, mod N
  is loaded and installed but never started.
- Fix: put a try/catch around each candidate, and push to the start order right after `emplace_back`.

**F3. A hot reload makes C++ mods disappear and re-enters a resident image.**
- Where: `queue_reinstall_mods`, staged `UE4SSProgram.cpp:2759-2760`.
- What happens:
  - It calls `setup_mods()`, which dlopens the already-resident PRX again, outside `module_start`.
  - `mbm_mod_entry_v1` is called again on the same image.
  - It then calls `start_cpp_mods()` without `install_cpp_mods()`, so `find_mod_by_name(..., IsInstalled::Yes)`
    fails. The log says "not started", and the old instance was already destroyed.
  - Host records are never erased (`max_mods = 32`, `NativeModHostCore.hpp:142`), so after 32 reloads `attach`
    refuses.
- How likely: latent. `EnableHotReloadSystem=false` in all 56 ini files under `l25-prep`.
- Fix: under the snapshot, either leave C++ mods running across a reinstall (skip them in uninstall/setup) or refuse
  the reinstall while native C++ mods are loaded.

**F4. The host's callback table only grows, and every pump walks all of it.**
- Where: `NativeModHostCore.hpp`. Both `abi_unregister` (`:977-990`) and `revoke_locked` (`:696-712`) only set
  `active = false` and never erase the entry. `game_thread_frame` (`:405-420`) walks the whole map under the mutex on
  every pump call. `add_callback` caps only the active entries.
- Failure scenario: a mod that re-registers a tick or hook on every map load or menu open grows the map without
  limit. The per-pump cost then grows across a long session, which is the stutter class Townfall just fixed.
- Fix: erase the entry on unregister and revoke, after the engine hook has been removed. Alternatively, keep a
  separate list of active ticks.

**F5. The GUI renders once per pump call, not once per frame.**
- Where: `NativeModEngineServices.inc:461-474` calls `after_frame` (`runtime_gui_frame`) on every pump call, from
  both pump sources. With the window open, every call does:
  - ImGui `NewFrame`/`Render`;
  - `ClearRenderTarget2D`;
  - `BeginDraw`, then `K2_DrawTriangle` with 2-5k triangles, then `EndDraw`.
- Evidence, all from earlier sessions and not re-proven here:
  - R11 launch 14 `[PERF]` lines show 57-82 pump calls per second, about 1-1.3 per frame, so this is fine today.
  - The R10b launch 12 and B6p launch 10 harvests show 1700-2300 pump calls per second.
- Failure scenario: if the carrier cadence regresses, or the VMT Tick pump goes live alongside the marshal, the open
  window draws N times per frame and causes heavy stutter. Mod `GAME_THREAD_TICK` callbacks would also run N times
  per frame.
- Fix: gate the GUI frame to at most once per engine frame, for example skip if less than 8 ms has passed since the
  last GUI frame, or prefer source 1 once `vmt_seen` is true. Record `[PERF]` with the window open during the #11
  launch.

**F6. Host services still work for a mod that is disabled or stopped.**
- Where: `NativeModHostCore.hpp:805-880`. Every `abi_*` service checks only `known_token`. Records are never removed,
  so a token stays "known" forever.
- Failure scenario: a mod faults ("mod disabled for this session"), but a worker thread it started keeps calling
  `property_set` and `call_function`. The claim "an error switches the mod off" then holds only for callbacks.
- Fix: refuse every service call, with `MBM_E_STATE`, unless the mod is `Running` or `Starting`.

**F7. The ImGui sharing check does not cover the context layout.**
- Where: `mbm_sdk_entry.cpp:130-132` compares only the version and the sizes of IO, Style, Vec2, Vec4, DrawVert and
  DrawIdx.
- Failure scenario: a third-party mod is built from the SDK with a different ImGui config, for example without
  `IMGUI_DISABLE_DEBUG_TOOLS` or with `IMGUI_ENABLE_TEST_ENGINE`. It passes the check, then writes into the host's
  `ImGuiContext` with a different layout. That corrupts memory and crashes the game.
- Fix: add `sizeof(ImGuiContext)` and a hash of the config macros to `mbm_gui_v1`, and refuse on mismatch.

### NOTE

- **N1. The main console unknown is loading a PRX from inside another module's start.**
  - `dlopen` (`sceKernelLoadStartModule`) runs inside the runtime's `module_start`, while
    `m_native_startup_mutex` is held (`UE4SSProgram.cpp:742` → `setup_mods`).
  - The mod's own static constructors and its `start_mod` constructor also run there, with no time limit.
  - The default-off design and the `[NMOD] loading C++ mod ...` marker contain this. If it hangs, the game hangs at
    boot until the `enabled.txt` row and file are removed.
- **N2. Exceptions cannot cross the module boundary in either direction.** Each image has its own unwinder.
  - The host's `catch (...)` blocks around calls into a mod (`attach`, `fire`, `dispatch_*`) cannot catch anything.
    Containment depends entirely on the SDK's `contain`.
  - One small gap: `mbm_sdk_entry.cpp:99-100` (`tab_tramp`) calls `enable_imgui()`, which builds `std::string`s in
    `log()`, outside `contain`. A `bad_alloc` there terminates the game. Wrap that call.
- **N3. `valid()` reads freed memory once.** It dereferences the pointer to read its internal index
  (`NativeModEngineServices.inc:87-93`, backend `:55-59`). A freed object pointer is read once. This is the same risk
  class as upstream raw `UObject*` and is documented.
- **N4. Only `call_function` checks the thread.** It refuses off the game thread, which is good. `property_get`/
  `property_set`, `find_*` and hook registration run on whatever thread the mod uses (key callbacks run on the
  event-loop thread). This matches upstream UE4SS.
- **N5. "Default-off" is not quite zero change.**
  - Every pump call now constructs or locks the host. That costs about 1 µs per call and is negligible.
  - Every boot lists the mods directory.
  - More than 256 entries in the mods directory refuses all C++ mods (`NativeCppModInventory.hpp:49`).
- **N6. PC DLLs are refused safely.** `CppModModule::Open` rejects a PC `main.dll` (MZ check) before `dlopen`. An
  enabled PC C++ mod now prints one warning line where R11 printed nothing. That is safe.
- **N7. The GUI scans all objects every frame in one case.** While the window is open and there is no player
  controller, it runs `FindFirstOf("PlayerController")` every frame (`NativeGuiEngineBackend.inc:371`). That is a
  full object-array scan per frame in menus, and only while open.
- **N8. Two ImGui configs exist in the runtime.** `CppMod.cpp` defines `IMGUI_USER_CONFIG` differently from the
  other runtime units that include `imgui.h`, for example `IM_ASSERT` in inline `ImVector` members. The struct
  layout is unaffected. Keep the `nm` step in cap11 APPLY step 2 (exactly one unit defines `ImGui::`).
- **N9. A non-ASCII mods directory path fails closed.** The path is widened byte by byte (hunk at staged lines
  2193-2195) and then re-encoded as UTF-8 in the CppMod constructor. The path comes out wrong, the load fails, and
  the failure is contained.
- **N10. The mod module entry is fine.** `mbm_mod_module_entry.cpp`:
  - The init/fini arrays are bounds-checked, and constructors run under a catch.
  - It calls `__cxa_finalize(&__dso_handle)` on failure. `__cxa_atexit` and `__cxa_finalize` are both the console
    libc's, so they match.
  - The re-entry guard and the state machine are sound.
  - It has no host import. Its shape matches the bootstrap that is already console-proven.

## Test counts (re-run on a scratch copy, this session, Mac-only)

| Suite | Result | Counts |
|---|---|---|
| `cap09/tests/run.sh` | **8/8 PASS** | check_sources 79, apply_anchored 16, api_usage_check 30, test_host_core 97, test_sdk_mods 35, test_startup_path 27, target_compile_r11 921, target_compile_live 888 |
| `cap11/tests/run.sh` | **11/11 PASS** | test_math 19425, lint_patch 67, api_usage_gui 28, engine_names_check 55, test_gui_host 54, check_png 9, test_gui_sdk_mods 23, coverage_gate_v2 76, import_delta 17, target_compile_r11 2031, target_compile_live 1999 |
| `run_gates.sh` | **PASS** | no_detour: 17 files, 0 sites. fs_throw: 17 files, 2 fs calls, both with error_code, 0 violations. no_detour --patch: 4 diffs PASS. Full-copy diff: 0 sites added |
| `apply_anchored.py --check` (live tree) | **PASS** | 16/16 |

## In plain words

The C++ mod loader and the in-game window code are safe to build: they make no code patches, add no new system
functions, and with no C++ mod switched on the game should behave exactly as on R11. A few small fixes are
recommended first. The main thing only the console can answer is whether loading a mod's PRX during startup works or
hangs; if it hangs, removing the mod's `enabled.txt` row (and the file) undoes it.
