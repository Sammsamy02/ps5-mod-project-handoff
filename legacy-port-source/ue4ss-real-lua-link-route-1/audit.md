# UE4SS real Lua-to-Unreal provider route audit

Scope: read-only inspection of the pinned UE4SS source plus a compile-only attempt for the real `LuaUFunction.cpp` translation unit. No mock Unreal objects, product edits, game/process access, or execution of native objects.

## Source route

The source snapshot is the official UE4SS archive at commit `e3ba1016562d6c0868c410d0a71e88bfcdbf691b`, pinned by `source-dependencies/dependency-receipt.json` (archive SHA-256 `07437097b08461a68988d6a92f15a34046437dae0b74c56552a321171f9162e8`). The Git metadata of the unpacked directory is not the source pin.

- `UE4SS/src/Mod/LuaMod.cpp:4324-4399` implements `ExecuteInGameThread`: checks `UE4SSRuntime` hook availability, registers the real EngineTick or ProcessEvent callback, captures the Lua callback into the hook state's registry, and queues it. `:3792-3820` drains callbacks; `:4129-4164` and `:4092-4127` are the engine-tick and ProcessEvent callback entry points. `:4177-4195` registers callbacks through `Unreal::Hook`.
- `UE4SS/src/Mod/LuaMod.cpp:1244-1265` binds `StaticFindObject` to `Unreal::UObjectGlobals::StaticFindObject`.
- `UE4SS/src/LuaType/LuaUObject.cpp:92-257` contains the real `call_ufunction_from_lua` path. It reads the actual `UFunction` parameter/return metadata and `FProperty` fields, marshals via the registered property handlers, and invokes `calling_context->ProcessEvent(func, ...)`. `:342-385` wraps real returned UObject/UFunction/class pointers as Lua userdata; `:2207-2230` resolves UFunction-valued properties using the real `GetFunctionByNameInChain` path.
- `UE4SS/src/LuaType/LuaUFunction.cpp:29-72` constructs the real UFunction wrapper and binds its Lua `__call` metamethod to `call_ufunction_from_lua`.
- The corresponding real core call is `port/unreal/src/UObject.cpp:448-451`, which dispatches `ProcessEvent` through UE4SS's virtual-wrapper machinery. `port/unreal/src/UObjectGlobals.cpp:231-267` resolves objects through the real initialized hash-table route when available and otherwise falls back to the object search.

Thus the narrow functional chain is real `StaticFindObject` → real UObject/UFunction userdata → reflected parameter marshalling → real virtual `ProcessEvent`. `ExecuteInGameThread` is a separate hook-backed path in the large `LuaMod.cpp` runtime unit; it is not established by merely compiling the reflection binding. Both paths require the real Unreal initializer/resolver and actual engine-owned object pointers before runtime use.

## Compileability evidence and blocker

The attempted TU was the actual pinned `UE4SS/src/LuaType/LuaUFunction.cpp`, compiled with `prospero-clang++` from the same retained PS5 command profile used by `consolidated-1/unreal-uobject/command.json`, adding only the real UE4SS include roots and `RC_UE4SS_EXPORTS`. Exact argv and compiler diagnostics are retained in `probes/lua-ufunction/command.json` and `stderr.txt`; no `LuaUFunction.o` was produced.

Compilation currently stops in the upstream `File`/`DynamicOutput` include chain before reaching the binding TU logic:

- `deps/first/File/include/File/FileDef.hpp:5-22` only recognizes `_WIN32` and `__linux__`; the Prospero profile matches neither, so the `Handle` and platform-header selection static assertions fail.
- `deps/first/DynamicOutput/include/DynamicOutput/Macros.hpp` and `TestDevice.hpp:7-11` define print macros only for Windows/Linux, leaving `RC_DEVICE_PRINT_FUNC` and `RC_TEST_DEVICE_PRINT` undefined in this profile.
- Forcing the Linux macro would not be a supported fix: `deps/first/File/include/File/FileType/LinuxFile.hpp:12-14` labels `LinuxFile` a stub with file operations unimplemented, while `deps/first/File/CMakeLists.txt` still builds `WinFile.cpp`. This is a genuine platform-provider gap, not a missing harmless define.

The native core's 15 Unreal objects plus fmt in `consolidated-1/final-result.json` do not include UE4SS Lua binding objects and do not remove this upstream dependency blocker. A real binding TU cannot yet be built through its normal include path under the retained PS5 profile.

## Smallest source-backed next step

Port the upstream `File`/`DynamicOutput` dependency boundary for the actual target (including a real target `File::Handle`/operations implementation or a deliberate, supported logging backend that removes the file-device dependency; do not alias the unimplemented Linux stub). Then compile the unchanged real `LuaUFunction.cpp` and `LuaUObject.cpp` with the pinned UE4SS headers and the already-built Unreal/Lua dependency objects. Their direct dependencies include LuaMadeSimple/LuaRaw, Helpers/fmt, DynamicOutput/File, and the reflected UObject/FProperty/UFunction core. Keep `LuaMod.cpp`/`ExecuteInGameThread` as a later, separate runtime integration target because it additionally needs the hook registry, Mod/Lua runtime ownership, and real hook availability.

A successful object compile would establish source/ABI compatibility only. Linking must use the actual Unreal/Lua provider objects and platform dependencies; native link is still not runtime/gameplay evidence. Do not use synthetic UObject pointers or claim the hook works until the real initializer and engine object source are provided by the separately owned runtime work.
