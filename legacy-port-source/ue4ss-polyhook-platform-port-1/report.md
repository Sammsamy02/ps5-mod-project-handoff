# PolyHook Prospero header port

The exact public PolyHook pin `298d56210b9d9e66cde8f96481d6053925c6ae15` now has an explicit Prospero x64 header branch in three local copies. Put `patched/include` before the immutable canonical PolyHook root. `PolyHookOsIncludes.hpp` selects the real SDK `sys/mman.h` and `unistd.h` declarations; `IHook.hpp` uses the target's native SysV free-function and explicit-receiver callback types. It does not select the Linux, Windows, or Apple backend or redefine their calling-convention macros. The original MIT license is retained at `patched/LICENSE`.

`red-2/receipt.json` captures the unchanged upstream's native compile failure: its SDK `__fastcall` collision and unsupported `__stdcall`/`__thiscall` attributes. `green-3/receipt.json` captures exit 0 with the same final actual-header control. The compiled closure includes the real x64Detour, MemAccessor/MemoryProtector declarations, Zydis/Zycore, and pinned AsmJit headers; the fixture instantiates free-function, member-function and `HOOK_CALLBACK` conversions. The one remaining diagnostic is the original AsmJit umbrella-header deprecation pragma. Earlier `green-1` and `green-2` are retained harness failures (non-constexpr predicate; unused macro-generated typedef), corrected only in the control.

The actual compiler defines `__PROSPERO__=1`, `__FreeBSD__=9`, x86_64, 8-byte pointers and 2-byte wchar_t. The control verifies the 16-byte member-function pointer and typed native callback result. LLVM identifies the output as an x86_64 ELF relocatable object. Its `owned_receiver_callback` disassembly passes the receiver in RDI and the integer in ESI. No native object was linked or executed.

The compiler dependency file contains 994 inputs: four local overlay/control files, 829 SDK headers and 161 immutable public dependency headers, with no unexpected include path or host C++ header. All three canonical header hashes match both the baseline and final compile receipts. `polyhook-prospero-headers.patch` is the exact unified diff, SHA-256 `2ee905bf2360c57ab72b717c8121c193c2adb8f0607be0e68abc41db87b1524d`.

Reproduction:

```sh
python3 .superpowers/native-dependency-spike/ue4ss-polyhook-platform-port-1/verify_header.py --variant baseline --output .superpowers/native-dependency-spike/ue4ss-polyhook-platform-port-1/red-2
python3 .superpowers/native-dependency-spike/ue4ss-polyhook-platform-port-1/verify_header.py --variant patched --output .superpowers/native-dependency-spike/ue4ss-polyhook-platform-port-1/green-3
```

The verifier requires fresh output directories, sets the retained LLVM_CONFIG itself, compiles with the actual SDK wrapper, and never links or runs the artifact. Existing recorded output paths are immutable.

The remaining runtime contracts are explicit. PolyHook's `sources/MemAccessor.cpp` provides only Windows, Linux and Apple definitions. The Linux implementation depends on `/proc/self/maps`, while its `mem_protect` must retrieve and restore the old protection. The new Prospero branch intentionally has no definition for those operations; the SDK's mmap/mprotect declarations do not prove permission, graceful fault behavior or original-protection lookup. No supported declaration for Windows process copying/query, Linux process_vm copying or sceKernelVirtualQuery/QueryMemory was found in this SDK's target headers. This is a missing verified contract, not proof that the console cannot offer one.

Likewise `sources/Misc.cpp` has no Prospero near-address executable allocator/free/page-size definitions, `MemProtector.cpp` has no Prospero protection translation, and `PolyHookOs.cpp` has no Prospero DebugBreak definition. AsmJit's `JitRuntime` declaration compiling does not establish that its executable-memory provider works on PS5. Concurrent patch publication/unhooking, protection restoration and instruction publication remain unimplemented and untested. No executable-page operation, game memory read, hook activation or console contact occurred.

The separate actual root `Hooks.cpp` compile at `ue4ss-unreal-native-build-1/hooks-baseline-1` reached genuine callback-GC and File include dependencies after this port. Its first blockers were the SDK's supported experimental jthread opt-in, the File backend's missing OS selection and an integer `NULL` initialization. That compile is an additional source investigation, not header/runtime support evidence.

After root supplied the real libc++ experimental opt-in, changed the integer initializer to zero and isolated the existing File-dependent cache definition, `hooks-port-2` compiled the actual copied Unreal `src/Hooks/Hooks.cpp` successfully (exit 0, six existing warnings). This agent made no root hook source edits. The earlier non-static `Base::InstallHook()` diagnostic was cascading and disappeared. `hooks-compile-receipt.json` records the actual source/object hashes and warning locations; the root helper retains the complete command. The object remains unlinked and unexecuted, and the File cache backend is still required by actual cache consumers.
