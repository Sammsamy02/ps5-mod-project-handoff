# Real UE4SS Lua binding TU source port

This is a private local overlay only. The pinned UE4SS source snapshot and Unreal core port are unchanged. The source pin remains the official UE4SS archive at e3ba1016562d6c0868c410d0a71e88bfcdbf691b (archive SHA-256 `07437097b08461a68988d6a92f15a34046437dae0b74c56552a321171f9162e8`).

## Narrow source changes

At the time of the two binding-TU results below, `port.diff` contained the four dependency/type-boundary changes described here. The overlay was later extended for the real `LuaMod.cpp` compile attempt; the current merged `port.diff` is SHA-256 `d9e75317672ff59d1a837ab7f92e99b003dffd4a647a511cd0271788cb3cdc7b`. Its additional changes and the current actual `LuaMod.cpp` compile boundary are documented in `lua-mod-compile-report.md`.

- The real `LuaUObject.hpp`, its `.cpp`, and the real Unreal `UDataTable.hpp` include `DynamicOutput/Output.hpp` instead of the all-default-devices `DynamicOutput/DynamicOutput.hpp`. These consumers use `Output::send`; they do not declare or own `File::Handle`, nor require the umbrella's FileDevice/TestDevice declarations.
- UE4SS's two explicit `TCHAR` conversions in these consumer files use the target-owned `Unreal::TCHAR` type. The pinned PS5 platform profile intentionally does not expose the Windows system-global spelling.
- `LuaUObject.cpp` includes `<type_traits>` before `Helpers/Casting.hpp`, whose `std::is_pointer_v` use was otherwise relying on an unrelated transitive include.

No File handle, file backend, test-device implementation, Lua API, UObject representation, function behavior, hook, or game pointer is stubbed or changed. `LuaUFunction.cpp` is a byte-identical local source copy; it builds against the local copied header overlay.

## RED and GREEN evidence

The unmodified real `LuaUFunction.cpp` compile failed first in `FileDef.hpp` platform selection and DynamicOutput's missing print macros; it produced no object. The exact PS5 command profile is recorded in `probes/lua-ufunction/command.json`; the actual RED invocation/output are in `probes/lua-ufunction/red-1/` (receipt SHA-256 `398354abf356236f8aca9f514bc22658657ccbfc0c0763f0012a31bfa2a449cc`).

After applying the private overlay, final invocations used the same Prospero compiler, flags, SDK/core include roots, and real dependency headers. Only the `-I` overlay root and `-c` source/output paths differ. Both compiled successfully with no compiler errors and produced objects:

- `LuaUFunction.cpp`: exit 0, 152,600-byte object, 6 warning diagnostics. Receipt SHA-256 `47b45e70eeb638269f5c44a7edca61a36163ea221be3489fb30fcb9c5521ee77`; object SHA-256 `d6eadd21767b22115dcb4a4f31742cd9b4589c8990f0e5cc6ad88e6974ac162f`.
- `LuaUObject.cpp`: exit 0, 2,224,928-byte object, 26 warning diagnostics. Receipt SHA-256 `e325c897fa7259d7bc58be66470664be19fa07d5dc2b8801efb9ec9c8667645c`; object SHA-256 `312971a2052bd65b5af8cce80f023fd8891cacbd33e9f8b58732082e63aeb501`.

Full argv and bounded stdout/stderr are preserved beside each final receipt at `probes/lua-ufunction/final-1/` and `probes/lua-uobject/final-1/`. The compiler helper is `probes/run_real_tu.py`.

## Boundary

This advances two actual reflected Lua binding TUs to native object generation. It does not link them into a complete UE4SS runtime. `LuaMod.cpp` still owns the real `ExecuteInGameThread` registration/queue and requires the actual UE4SSRuntime hook registry and initialized hook availability; those are not proved by these objects. `LuaUObject.cpp` uses real Unreal APIs (`StaticFindObject`, UObject/UFunction/FProperty metadata, `ProcessEvent`) but no object is executed and no engine pointer is supplied. The result is source/ABI compile evidence, not gameplay or runtime integration evidence.
