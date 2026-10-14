# cap09 NOTES — UE4SS C++ mods on the PS5 (Mac-only work)

Status: **PARTIAL** — the generic route is designed, implemented and host-tested
(4 suites, 128 compiled checks + 77 static items, all PASS, `tests/output/`);
the runtime binding has not been target-compiled and nothing has run on the
console. Every claim below is **Mac-only** unless it cites a console log line.

## 0. Fixer update (2026-10-13, after the BLOCK review; see ../FIXES.md)

- **B1 fixed**: under the activation snapshot (always on the console) the runtime now creates and starts C++ mods
  (`UE4SSProgram.cpp` hunks + `NativeCppModInventory.hpp`). Enabled = snapshot (`mods.txt` / `enabled.txt`),
  PRX = `<mods directory>/<Name>/dlls/`. Loaded during `module_start` (only phase with `/data` known reachable).
- **B2 fixed**: no `thread_local` with a destructor; the staged units now target-compile with R11's own commands
  and add 0 imports vs R11's runtime object (Mac-only, objects only).
- Plain-words corrections: a C++ mod error switches only that mod off, but a real crash in mod code still crashes
  the game (N1). PC C++ mods (DLLs) never run: they must be rebuilt from source with this SDK (N7).
  `GAME_THREAD_TICK` fires once per game-thread pump call, which can be a little more than once per frame (N5).
- Status stays **PARTIAL** until the mod PRX recipe (§4) has run and the first launch is done.

## 1. Findings that decide the route

1. **The runtime exports nothing a UE4SS C++ mod could import** (Mac-only, read
   from the R11 bare ELF `outputs/2026-10-08-l26-package-A/pass-1/bare-sdk/modbridge_supplier_composition.prx`
   with llvm-readelf): 210 dynamic symbols, only 2 defined (`BaOKcng8g88#F#A`,
   `KpDMrPHvt3Q#F#A` at 0x15a0 / 0x1e00 = the DT_INIT/DT_FINI module entries).
   A mod compiled against the real UE4SS headers (CppUserModBase ctor/dtor,
   typeinfo, UObject methods, Hook::Register*) has no symbol to bind to, and
   cross-module C++ (RTTI, exceptions, std:: layouts, operator new) between two
   separately built PRX modules is unproved. The earlier `native-cppmod-contract`
   route (17 host C++ imports) does not fit the shipped runtime.
2. **The loader mechanics are already in the shipped PRX** (Mac-only): R11 imports
   `sceKernelLoadStartModule`, `sceKernelDlsym`, `sceKernelStopUnloadModule`
   (EXACT-IMPORT-IDENTITIES.json), used by the retained `CppModModule.cpp`.
   They have **never run on the console**: launch 14 logged no native mod
   (no `dlls` folder existed). This is the main risk of the #9 launch.
3. **Hook backend facts on R11, console-proven** (`l25-prep/evidence/launch-l26-R11-launch14-20261013/ue4ss.decoded.log`):
   - line 623-625: `[R3] refused: EngineTick pre-callback registrations ...`, `[EU-VMT] arming refused: non-text-ent` → no UEngine::Tick route; ENGINE_TICK must be refused.
   - line 787-791, 938: R3 FuncPtr-marshal carriers armed; `marshal is the game-thread pump` → the game-thread tick route.
   - line 794: InitGameState/BeginPlay/EndPlay/StaticConstructObject/ULocalPlayerExec/CallFunctionByNameWithArguments/ProcessConsoleExec detours stay unregistered; Lua LoadMap/BeginPlay come from the data-only [WORLD-EV] route (line 1011).
   - line 823/836: `[RegisterHook] Registered native hook (5, 6) for Function /Script/Engine.PlayerController:ClientRestart` → the per-UFunction native hook registration the probe uses works on R11.
   - line 975-977: Blueprint-function hooks are refused for Lua (`bp-script-function-needs-processinternal-hook`); the R3 carrier shows the Func swap itself works on Blueprint overrides (787-791).
   - line 1041: `frames=57 ... pump_calls=57` → the pump runs about once per frame.
4. ChatGPT cap09 re-checked: its surface inventory was **reproduced** (21 files,
   350 calls, identical set: `tests/output/reverify/ACTUAL-SURFACE.rerun.json`).
   Its global-backend conclusion holds and matches the console lines above:
   EventViewerMod needs global ProcessEvent/ProcessInternal/ProcessLocalScriptFunction
   (code detours), KismetDebuggerMod needs EngineTick + GNatives writes + pausing
   the game thread. Its lifecycle wrapper / child-runtime route was not reused
   (it depends on host C++ imports, finding 1).

## 2. The route (what is staged)

- **ABI v1** (`PATCH/new/include/Mod/NativeModAbi.h`): one C export
  `mbm_mod_entry_v1(host_table, token, &mod_table)`. Host → mod: log, object
  lookup, typed property get/set/address, UFunction calls, hook admission
  query, per-UFunction hooks, game-thread ticks, key binds,
  execute_in_game_thread, unregister, GUI table (#11). Mod → host: metadata +
  lifecycle callbacks that return a status. Only C types cross the boundary.
- **Host core** (`NativeModHostCore.hpp`, engine-free): table validation;
  fault isolation (a failing callback disables that mod once, logged);
  host-owned registration ids (the engine only ever holds host code);
  **revoke → drain → destroy** on stop (cross-thread wait, deferred stop from
  inside the mod's own callback, timeout → mod retained, never destroyed under
  a running callback); bounded capacities; `fire_all` + `CallScope` + stop
  listeners for #11.
- **Hook admission** (`HookAdmission.hpp`): 17 families. DATA_ONLY:
  game-thread tick, object delete. DATA_ONLY_PARTIAL: per-UFunction
  (Blueprint-to-Blueprint calls bypass `UFunction::Func`), LoadMap / BeginPlay /
  InitGameState (after the event). REFUSED (code detour only, permanent):
  global ProcessEvent / ProcessInternal / ProcessLocalScriptFunction,
  CallFunctionByName, ProcessConsoleExec, LocalPlayer Exec, raw code address.
  PENDING_VALUE (data-only route exists, value lacks two sources): viewport
  tick slot, AddUObjectCreateListener, GNatives. ENGINE_TICK only when the
  VMT pump is armed (it is not on R11). Refusals are decided before anything
  is installed.
- **Runtime binding** (`candidate/src/Mod/NativeModEngineServices.inc`) uses only
  APIs the shipping code already calls (`tests/api_usage_check.py`: 30/30).
  Object liveness is the O(1) GUObjectArray slot check (UObject::IsReal walks
  every object — far too slow).
- **SDK** (`PATCH/sdk/`): upstream-shaped headers (`Mod/CppUserModBase.hpp`,
  `DynamicOutput`, `Unreal/UObject(Globals).hpp`, `Input/*`, `UE4SSProgram.hpp`)
  + one TU `mbm_sdk_entry.cpp`. The official UE4SS "Creating a C++ mod" example
  compiles **unchanged** and runs in the host test (`tests/mods/official_example`).

## 3. What a C++ mod author must do

1. Keep the upstream source shape: `class MyMod : public RC::CppUserModBase`,
   `extern "C" start_mod()` / `uninstall_mod()`.
2. Build against the **PS5 Mod Bridge SDK headers** (not UE4SS's), add
   `mbm_sdk_entry.cpp` once, build a PRX with the project's PS5 toolchain
   (`-fvisibility=hidden`, C++20, exceptions on, its own static C++ runtime),
   output `Mods/<Name>/dlls/main.prx`. A renamed Windows DLL is refused
   ("Windows PE mod requires ...").
3. Use only the SDK API (anything else is a compile error, by design):
   lifecycle, Output::send, FindFirstOf/FindAllOf/StaticFindObject,
   GetFullName/GetName/IsA, GetValuePtrByPropertyName for scalars/objects,
   Get/Set/Call, RegisterHook on UFunctions, register_keydown_event,
   register_game_thread_tick, Sdk::execute_in_game_thread, register_tab (#11).
4. Not available (documented gaps): global ProcessEvent/ProcessInternal hooks,
   detours / AOB patching, raw FFrame / bytecode access, GNatives, pausing the
   game thread, Lua state sharing (`on_lua_start` is never called), FString/
   struct parameters in Call (scalars, objects, FName read only), Windows APIs.
   The two real upstream mods found locally need these, so they stay refused.
5. Declare pad keys for keymap-gen (stub file with `RegisterKeyBind(Key.X)`).

## 4. Mod build recipe (open, integrator)

The mod PRX must be self-contained: its `module_start` runs its own init array
and registers its own `.eh_frame` with its **own** static unwinder (no host
imports). `outputs/ps5-mod-bridge/native-cppmod-source/child_runtime.cpp`
already does init/fini/unwind registration but binds the unwinder to the host
(`__unw_add_dynamic_eh_frame_section` import) — for ABI v1 it must link
libunwind statically. Fake-signed packaging like the runtime PRX. Imports
only from system libraries the runtime already imports.

## 5. Open questions

- Does `sceKernelLoadStartModule` of a second fake-signed PRX from inside the
  game process work (and with which path/flags)? First console contact answers it.
- Per-mod memory/thread policy: mods may create threads; modules stay resident.
- The user's note (Leon textures checkered gray/white, outdoor scene too bright):
  not a #9/#11 issue. Gray/white checker is what Unreal draws when a material
  cannot be used on this platform (the default/world-grid material); together
  with the "too bright" scene it points at the Leon pak's PC-cooked
  materials/shaders (and possibly assets it overrides) — capabilities #8/#12
  (pak materials, shader translation). Mac-only reasoning, not verified.
