# UE4SS native virtual-dispatch experiment

Epic-linked GitHub access is now working. The exact UEPseudo dependency required by retained UE4SS commit `e3ba1016562d6c0868c410d0a71e88bfcdbf691b` was downloaded through authenticated GitHub access: commit `38e7171e9e8c4a871ff84765848f0290ca34a44e`, tree `4e74af09f3b7be5c275c21a4243f63ad5eba44b8`, 2,757 files. Its canonical source remains private and unchanged under `.superpowers/native-dependency-spike/ue4ss-unreal-source-access-1/`.

A copied upstream header now has a working native ABI variant for its four virtual-function wrapper macros. The original wrapper fails to compile for the PS5 target because it bit-casts an 8-byte code address into a 16-byte member-function pointer. The patch calls the vtable entry through a typed function pointer with an explicit receiver on non-Microsoft targets. It retains the Microsoft branch and the existing offset lookup and error handling. `UObject::ProcessEvent` uses this wrapper, so this addresses actual UE4SS engine code rather than adding a substitute Lua API.

## Evidence

All paths below are relative to `.superpowers/native-dependency-spike/ue4ss-virtual-dispatch-port-1/`.

- `native-red-1/receipt.json`: the original header fails with the expected member-pointer size mismatch on the PS5 compiler.
- `host-green-2/receipt.json`: 11/11 behavior checks pass on ARM64 macOS.
- `host-x86-green-1/receipt.json`: 11/11 behavior checks pass on x86-64 macOS through installed Rosetta.
- `native-green-3/receipt.json`: both fixture translation units compile for the PS5 target. Object disassembly shows indirect dispatch in all seven exercised wrapper bodies. These objects were not executed on the PS5.
- `virtual-dispatch-native-abi.patch`: the reproducible local patch. Patched-header SHA-256: `0bf29a3cc2c44fa43403fba408c973429a5e0e8a9ed0de8aa1c62551440ec91b`.

The fixtures use compiler-created C++ vtables in separate translation units with LTO disabled. They exercise all four macro forms, a ProcessEvent-shaped void call, pointer/reference arguments, scalar and aggregate returns, mixed floating-point/integer arguments, a real secondary-base adjusting thunk, and missing/null refusals. Real pinned ASMHelper, Zydis and Zycore headers are included. No engine API stubs are supplied. The Microsoft branch was retained but not compiled here.

The initial ARM64 fixture used an incorrect member-pointer decoder; that failed run is retained as `host-green-1`. Two native postprocessing attempts misclassified disassembly labels/instruction spelling; their failed receipts remain preserved. The corrected final evidence does not erase these earlier results.

## Engine discovery and remaining work

The exact pinned PatternSleuth ELF tick resolver was also checked against the saved Townfall executable. Its `causeevent=` anchor has one direct reference, but none of the 13,300 actual decoded unwind function ranges contains it. The resolver therefore cannot identify a function from this capture. The gap between neighboring unwind entries is not a function boundary. Details are in `.superpowers/native-dependency-spike/townfall-causeevent-resolver-1/report.md`.

This is a source-port experiment, not a complete or deployed UE4SS runtime. Platform headers, strings/files, object discovery and lifetime handling, and actual game-thread hooks still need integration. The current source also relies on an atomic shared-pointer specialization absent from the retained SDK. No Item Spawner gameplay success, general DLL compatibility, or universal game support is established. Download/UI work remains paused while the engine backend is developed.

The next build step is to apply this private header overlay to the actual UObject/UEPseudo source build after resolving its platform dependencies. Existing generated Microsoft vtable offsets must not be treated as verified PS5 layouts. A focused independent review found no blocking issue in the changed call wrappers; the experiment's final evidence index is `result.json` alongside the patch.

No console operations were performed in this experiment. Existing game files, paused work, and canonical captures were preserved. The previously rejected memory-read build and archive-placement caller were not retried.
