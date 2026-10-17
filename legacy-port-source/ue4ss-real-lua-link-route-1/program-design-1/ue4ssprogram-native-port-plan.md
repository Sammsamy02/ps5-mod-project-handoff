# UE4SSProgram Native Port Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` if this plan is approved for implementation. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Preserve the real UE4SS program and Lua mod lifecycle on the native target so `ExecuteInGameThread` queues onto scanned and installed Unreal hooks.

**Architecture:** Keep UE4SSProgram as the owner of mod inventory, the event queue, and Lua startup ordering. Move only OS-owned facts and services behind explicit platform inputs; use the real native CppModModule and existing Unreal initializer/hook sources. Unsupported input, crash, or UI settings must be reported or rejected explicitly.

**Tech Stack:** Pinned UE4SS source `e3ba1016562d6c0868c410d0a71e88bfcdbf691b`, C++23, retained Prospero SDK compiler, current native File/CppModModule ports, real Unreal scanner and hook sources.

**Spec:** Source and compile evidence in `.superpowers/native-dependency-spike/ue4ss-real-lua-link-route-1/`; current `UE4SSProgram.cpp` probe receipt is `probes/runtime-closure/ue4ssprogram-1/receipt.json`.

## Global Constraints

- Do not synthesize game/module addresses, scanner results, or callbacks; paths and module notifications come from the actual native loader.
- Preserve LuaMod and CppMod lifecycle and event-queue ordering; do not silently drop API behavior.
- Use real `UnrealInitializer`, `Function<>` storage, `Hook` detours, and `CppModModule`; no stub definitions or fake engine objects.
- A successful native compile/link is not evidence that Townfall resolves or invokes either engine hook.

## Review Focus

- Missing or mismatched module/game paths must fail startup before scanning or mod installation.
- A failed native module load must not emit a loaded-module event or appear installable.
- `ExecuteInGameThread` must refuse until the requested scanned hook is ready, then execute queued callbacks through that actual hook.
- Input, crash-dump, and debug-UI settings unavailable on the target must not appear to succeed silently.
- Shutdown must stop the program/event loop before destroying Lua mods, callbacks, and module owners.

---

## Source findings and exact closure

The current `LuaMod.o` directly imports `Mod::Mod(UE4SSProgram&, ...)`, 18 `UE4SSProgram` methods, `UE4SSRuntime::IsEngineTickAvailable` / `IsProcessEventAvailable`, `UnrealInitializer::IsInitialized`, and the EngineTick/ProcessEvent pre-callback registration functions. The program-method imports group as:

- Paths: `get_mods_directory`, `get_working_directory`, `get_game_executable_directory`, `get_object_dumper_output_directory` (`UE4SSProgram.cpp:2027–2037`, `2424`).
- Mod management: `find_mod_by_name_internal`, queue reinstall/uninstall by ID/name (`2333`, `1869–1964`).
- Input: key registration/query and input-event enumeration (`2290–2326`); these are called by Lua key-bind APIs in `LuaMod.cpp:1507–1741`.
- Lua helper operations: object dump and C++/Lua/UHT generation (`2165–2212`, `2531`), used by `LuaMod.cpp:1856–1885`.

`ExecuteInGameThread` is registered in `LuaMod.cpp:4334`; it checks the selected hook’s readiness at `4358–4373`, lazily registers the real callback, then enqueues the Lua function at `4376–4405`. `UE4SSRuntime.cpp:11,19` derives availability only from `UEngine::TickInternal.is_ready()` and `UObject::ProcessEventInternal.is_ready()`. The actual storage is defined by `UEngine.cpp:10` and `UObject.cpp:19`. Registering a callback is not proof of installation or invocation; `Hooks.cpp` routes registration through metadata and detour instances.

Startup ordering to preserve is: native program context and paths → discover/install/start C++ mods → `UnrealInitializer::Initialize(config)` → C++ `fire_unreal_init` and Unreal property setup → publish Lua shared functions → `update()` → set the default game-thread method → install Lua mods → `LuaMod::on_program_start()` → C++ program-start notification → start Lua mods (`UE4SSProgram.cpp:329–335`, `384–421`, `878`, `1084–1090`).

## Tasks

### Task 1: Supply truthful native program inputs

**Files:** `UE4SSProgram.hpp/.cpp` and the native loader integration.

- [ ] Replace implicit Windows process discovery in `setup_paths` (`UE4SSProgram.cpp:437–490`; `GetModuleFileNameW` at 448–450 and `AddDllDirectory` at 460–461) with an explicit platform context containing the actual UE4SS module path and game executable path. Keep the existing settings/legacy-directory derivation based on those values.
- [ ] Add an actual-loader test that rejects absent, relative, or inconsistent module/game paths before publishing `s_program` or creating mods. Do not default to the current working directory.

### Task 2: Preserve module and mod lifecycle across loaders

**Files:** `UE4SSProgram.cpp/.hpp`, native `CppMod` integration, existing `Platform/CppModModule`.

- [ ] Keep the `LoadLibrary*` IAT path (`UE4SSProgram.cpp:151–180`, `301–327`) as the Windows implementation. For the native path, forward a notification only after `CppModModule::Open` succeeds, using the existing `fire_dll_load_for_cpp_mods` event (`1427–1435`); do not report a failed or PE module as loaded.
- [ ] Preserve `install_cpp_mods` / `start_cpp_mods` and program-start notifications (`1378–1381`, `1414–1447`, `1644–1661`). Verify native `CppModModule` rejects PE and missing exports instead of marking the mod started.

### Task 3: Port diagnostics and optional platform services explicitly

**Files:** `UE4SSProgram.cpp/.hpp`, platform service owners, Lua-facing input capability boundary.

- [ ] Retain file logging through `Output::NewFileDevice` (`UE4SSProgram.cpp:211–213`) and the real File backend. Move console allocation (`535–543`) and `printf_s` (`496`) behind an explicit console provider.
- [ ] Keep Windows crash-dump behavior in its Windows implementation. `CrashDumper.cpp:27–66,84–95` depends on Win32 exception filters, `dbghelp`, and IAT hooks; a target without this service must reject enabled crash-dump settings or expose explicit unavailability.
- [ ] Respect `HAS_INPUT` guards (`1070–1082`, `1228–1230`, `2290–2330`), but make Lua key-bind registration report unsupported input rather than silently succeeding when no real input provider exists.
- [ ] Do not create a substitute GUI. If the target has no real renderer, validate debug-console/render settings and report the unsupported combination before registering GUI callbacks (`UE4SSProgram.cpp:209`, `228–247`, `976+`).

### Task 4: Prove the real startup-to-hook path

**Files:** native program integration tests and current actual-source composition.

- [ ] Build and compose the real `UE4SSProgram`, `LuaMod`, `Mod`, `CppMod`, `CppModModule`, `UE4SSRuntime`, `UEngine`, `UObject`, scanner, and hook objects with no test-only symbol definitions. Inventory every remaining undefined symbol.
- [ ] Test startup ordering and shutdown ownership around the actual platform context and event queue. Keep scanner readiness false until the real `Function<>` is resolved.
- [ ] Treat target gameplay as a separate acceptance gate: observe a nonzero real EngineTick or ProcessEvent resolution and one callback invocation on that game’s actual execution path. Native object compilation alone does not pass this gate.

## Current implementation boundary

The genuine `UE4SSRuntime.cpp` and `Hooks.cpp` TUs compile under the frozen profile. The genuine `UE4SSProgram.cpp` compile currently stops at its unconditional `#include <Windows.h>` on line 3, before later errors can be assessed. It also includes Win32 path lookup, DLL-directory setup, console allocation, and four IAT load hooks; `CrashDumper.cpp` is a separate Windows-only dependency. These are source-port boundaries, not grounds to define fake program methods.
