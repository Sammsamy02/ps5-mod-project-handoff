# Actual `LuaMod.cpp` source-port boundary

This is a private source overlay against the pinned UE4SS snapshot (`e3ba1016562d6c0868c410d0a71e88bfcdbf691b`, archive SHA-256 `07437097b08461a68988d6a92f15a34046437dae0b74c56552a321171f9162e8`). The official snapshot and shared Unreal port were not edited. Current local source patches are represented in `port.diff` (SHA-256 `d9e75317672ff59d1a837ab7f92e99b003dffd4a647a511cd0271788cb3cdc7b`); private include overlays are separately hashed below.

## Current compile result

The GUI include dependency is resolved from exact public tags: Dear ImGui `v1.92.1` and UE4SS-RE/ImGuiColorTextEdit `v1.2.0`. Their source commits, archive checksums, pinned-manifest evidence and compile receipt are in `gui-dependency-closure-report.json`.

The latest actual PS5 `LuaMod.cpp` translation-unit compile is port-16: Prospero Clang 21.1.8 produced a 6,795,496-byte object with no errors. This is a compile-only result; it does not claim link closure, hook activation, or runtime/game behavior. The final derived command profile uses the correct private overlay include root and the upstream version defines described below. Port-12 is preserved as the pre-fix diagnostic receipt; port-14 is the first correctly overlaid attempt and exposed LuaMadeSimple’s native `long` integer-template gap; port-15 and port-16 compile successfully.

Version defines come from the pinned source, not an assumed release: `UE4SS/generated_src/version.cache` contains `3.0.1.0.0` (SHA-256 `53de33e2b9548bd830dea05d1d79f727ff2202404dc948f1e7887a150339d220`). `UE4SS/xmake.lua:39–52` extracts its first five digit groups as major/minor/hotfix/prerelease/beta, and lines 86–95 add those as public `UE4SS_LIB_VERSION_*` defines. Port-16 passes `MAJOR=3`, `MINOR=0`, `HOTFIX=1`, `PRERELEASE=0`, `BETA=0` in derived profile `probes/lua-mod/command-gui-file-version-2.json` (SHA-256 `e322903fc3eeaddd59633d2ec0fea389fcd1de51aed2fa5bb8abb5e6aaafd150`).

The 12 native callback/conversion type references in the private `LuaMod.cpp` copy now name `Unreal::TCHAR`. This is the pinned target alias `typedef FPlatformTypes::TCHAR TCHAR` in `RC::Unreal`, declared in `port/unreal/include/Unreal/Core/HAL/Platform.hpp:1138–1140`; function signatures and engine character type are unchanged. The same source pass also fixed the compiler’s actual instantiation at `LuaMod.cpp:766`: its `int64_t` is native `long`, but pinned LuaMadeSimple `Table::add_value` only recognized `int`/`long long` before instantiating `ValueType::InnerType`. The private overlay adds `long` to that integer branch. Pinned dependency and overlay SHA-256 values are recorded in the identities section.

The bounded ErrorObject copy has an actual host control: the upstream header fails on Apple Clang because `strncpy_s` is unavailable; the private portable copy passes short, 1999-byte, exact-2000-byte and 2001-byte fallback checks, preserving the old limit behavior and NUL termination. The two former `enumerate_view` loops now index the same vectors after their existing equal-size check. The Windows print path still uses `printf_s`; native uses `std::printf` with standard `%ls` wide-string formats.

The GUI layout requires complete real types: `UE4SSProgram` embeds `GUI::DebuggingGUI`; `GUI.hpp` embeds `Console` and `LiveView`; `LiveView::Watch` owns `Output::Targets<Output::FileDevice>` by value. The local LiveView include overlay narrows the DynamicOutput umbrella to the actual `Output` and `FileDevice` headers and does not replace File or GUI types. Port-7 is the historical missing-TextEditor attempt; port-8 is a malformed-command attempt; port-9/10 record GUI closure; port-11 adds File headers; port-12 records missing version/TCHAR config; port-14 reaches the native LuaMadeSimple `long` issue; port-16 is the successful actual TU compile.

## CppMod header separation and include cleanup

The earlier PS5 compile stopped at `Mod/CppMod.hpp` importing Windows-only `MinimalWindowsApi.hpp`. The local CppMod header overlay now preserves the complete Windows path (`MinimalWindowsApi.hpp`, HMODULE and DLL-directory cookie) under `_WIN32`. Its non-Windows branch holds only `std::unique_ptr<Platform::CppModModule>` with an opaque forward declaration; all real `CppMod` lifecycle methods remain declared, and its destructor remains out-of-line so the native module type can be completed by the platform loader implementation. This is ownership separation only; it does not implement or fake a loader. Header SHA-256: `f33e2eb80fb391a6ad238651e056db91596b136c3f38a1de52ebfa876ef1b8ed`.

I also removed the unused `File/File.hpp` include from `Mod.hpp`, changed `LuaMod.hpp` and `SettingsManager.hpp` to the existing type-only `File/Macros.hpp` header where they use only `File::StringType` aliases, and forward-declared `CppUserModBase` in `CppMod.hpp` because the header needs only its pointer type. Those are include-boundary reductions, not File or GUI implementations. The full `LuaMod.cpp` compile now passes those Windows and File include boundaries and compiles against the pinned GUI headers and actual FileDevice declaration.

Earlier receipts are preserved: `port-4` shows the Windows-only CppMod diagnostics; `port-5` shows the next missing `TextEditor.h`; `port-6` shows the additional File backend include errors before the type-only include reductions; `port-12` records the prior missing version/TCHAR config; and `port-16` is the final successful compile.

## Registration and runtime boundary

The real Lua `StaticFindObject` registration is in `LuaMod.cpp:1244`; its overloads call `Unreal::UObjectGlobals::StaticFindObject` at lines 1264 and 1338. The real `ExecuteInGameThread` registration is in lines 4314–4403 and checks actual `UE4SSRuntime` EngineTick/ProcessEvent availability before registering hooks and queueing callbacks. `UE4SSRuntime.cpp:11–24` defines those checks against `UEngine::TickInternal` and `UObject::ProcessEventInternal`. These source paths are now compiled as part of the real `LuaMod.cpp` TU. This remains compile evidence only: there is no UE4SS link, hook activation, game pointer, executable, or runtime/gameplay evidence.

The standalone compile now supplies the pinned version values and uses the actual target TCHAR alias. The next technical boundary is link closure for the real UE4SSRuntime and hook registry symbols; compile success alone does not prove hook initialization or gameplay behavior.

## Identities

- Local `LuaMod.cpp`: SHA-256 `72adad1d5c54b53d3ba73e141d19aadb3b3d86670118b24937628def05a7fc9b`.
- Pinned upstream `LuaMod.cpp`: SHA-256 `113d1372c955e981c9772a8b6f9b45bd426868f03252e701fc18ac518729c6ad`.
- Current merged `port.diff`: SHA-256 `d9e75317672ff59d1a837ab7f92e99b003dffd4a647a511cd0271788cb3cdc7b`.
- `port-7/receipt.json`: SHA-256 `5b5fd3a96cb79e879bb5d5146b7dcc38f8077b7e86263f13717021f8d10ab468`.
- Version-defined compile profile `command-gui-file-version-2.json`: SHA-256 `e322903fc3eeaddd59633d2ec0fea389fcd1de51aed2fa5bb8abb5e6aaafd150`.
- `port-16/receipt.json`: SHA-256 `bfd4078c2256093f70a757df6db030f2e237cda733eb3fad3f89e1df8b652995`; `LuaMod.o`: SHA-256 `526cdd0fb4306849952bbbeeaf505809ec3d697ce7d55a02c69acfc95ad927ff`.
- LuaMadeSimple pinned header: SHA-256 `660c4884c75398e66c250fd28deab1fa719e2114b2f6559de2ba9e9191aef475`; local integer-branch overlay: SHA-256 `19ba8e3475ce5ecc923d6177c06d349b878a5ba4b633f95ee4e348efba20314d`.
- `port-7/stderr.txt`: SHA-256 `7009c834c7eb1c0cd4cfb541c38eadf7c8d228d2ac3151b6962b0cb327a6c97d`.

No tracked product source was changed by this experiment.
