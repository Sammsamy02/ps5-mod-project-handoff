# Initializer/signature native link-gap measurement

Both requested baseline compiles used the actual current root helper, copied Unreal source and the same `UE_BUILD_SHIPPING=1` / `OVERRIDE_PLATFORM_HEADER_NAME=Prospero` flags. `initializer-baseline-1` and `signatures-baseline-1` exited 1 and emitted no object. The first real errors were RC::File's unsupported OS selection/owning Handle and DynamicOutput's missing device-print macros; initializer also includes Windows.h unconditionally at line 37. These actual commands and logs are retained, with private copies of the diagnostic receipts here. No providers were fabricated.

Root then authorized one exact include cleanup in copied `src/Signatures.cpp`: use `DynamicOutput/Output.hpp` instead of the aggregate that constructs dependency closure for unused file/test devices, and remove unused `File/File.hpp`. All function bodies are byte-for-byte preserved against `Signatures.baseline.cpp`. The canonical private source remains unchanged, SHA-256 `ae80e56de76101ec6c4531f4be4294908488720dd511655253d0e498014c3b71`.

`signatures-port-2` compiled successfully to a native object (exit 0, two existing Atomic.hpp warnings). The command is at `ue4ss-unreal-native-build-1/signatures-port-2/command.json`. Focused LLVM symbol inspection confirms the object still imports the real `RC::SinglePassScanner::start_scan` function; it was neither replaced nor resolved. `receipt.json` records current source/object hashes, exact commands and all measured diagnostic categories. `signatures-includes.patch` contains only the two include edits.

This advances actual orchestration compilation; a full linked/initialized backend still has the following concrete gaps:

- Initializer's real file/output-device dependency closure needs a genuine supported file backend. The canonical LinuxFile is explicitly an unimplemented stub, so selecting it or substituting an integer Handle does not satisfy the contract.
- `LoadExport` at initializer line 123 and `SetupUnrealModules` at line 142 call GetProcAddress and K32 module APIs and assume Win64 DLL naming. The initializer has no implemented Prospero module/export provider. It was not edited in this task.
- The actual SinglePassSigScanner implementation includes Windows.h/Psapi.h and scans ranges supplied through WIN_MODULEINFO. Compiling the signature caller does not port, link or validate that scanner/image-ownership contract.
- Initializer declares the actual `ps_scan` C entry point at line 446 and invokes it at line 487. This task supplies no PatternSleuth substitute, native library or runtime resolver proof.
- Live executable-image provenance, resolved engine globals/allocator/reflection layout, the actual game-thread callback and PolyHook memory/protection/executable-allocation implementations remain unverified. The successful UObject/Hooks/reflection objects do not initialize these dependencies.

No link attempt, native execution, module/process lookup, game memory read, executable-page operation or console contact occurred. No initializer, common platform profile, compiler helper or canonical source was modified.
