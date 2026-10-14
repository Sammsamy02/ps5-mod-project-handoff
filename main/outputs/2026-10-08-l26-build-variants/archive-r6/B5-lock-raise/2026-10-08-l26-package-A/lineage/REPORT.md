# Owned module entry alignment candidate

The predecessor log confirms the first callback takes the deferred queue branch with initialization false, empty size/capacity, and a nonnull callback. It reaches `before push_back` and never logs `after push_back`. The user reported FAIL863ms and Townfall remained playable. This establishes the insertion boundary, not an allocation or ABI failure by itself.

Local source inspection found that the unchanged Ploader call stub aligns its stack to16 and then subtracts40 before CALL, giving its callee a nonstandard entry alignment. Whether the OS module loader propagates that alignment into our module remains unobserved. The owned runtime's prior entry expects the ordinary SysV alignment. No malformed queue-path relocation or code-permission error was found; startup input allocation already succeeded.

This candidate changes only our exported `module_start` and `module_stop` to request Clang's `force_align_arg_pointer` prologue, and adds two startup values: incoming stack alignment bits (from our frame address) and outgoing call-stack alignment bits. No caller stack contents are read. The compiler preserves/restores the original stack. Ploader, game files, privilege handling, and injection behavior are unchanged. No game-memory mapping or game API is added.

The pinned SDK fixture demonstrates explicit `and rsp,-16` code generation. Native compile/composition completed11 commands; packaging/finalization/wrapper/check completed5. Same191 imports,232 constructors,2 finalizers and SDK0x11000000. Wrapper 30,151,623 bytes, SHA256 `3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654`. The old C++ registration markers are retained so the result can show both incoming alignment and progress beyond the failing insertion.

Module `/data/plugins/modbridge_runtime_aligned_entry.prx`, log `/data/townfall_runtime_aligned_entry.log`; existing owned88-byte input remains unchanged. Installed after fresh user closure confirmation; candidate/config readback matched, prior registration probe/log and input were preserved. Ploader execution remains manual and the target result is pending. Placement receipt: `../native-full-runtime-aligned-entry-placement-1/receipt.json`. This is a targeted ABI hypothesis test; it does not establish UE4SS or Item Spawner support.

Compiler reference: https://clang.llvm.org/docs/AttributeReference.html#force-align-arg-pointer

The local exact-byte entry fixture passed8 asserted cases: realignment gives ABI-correct callback entry for incoming0/8 and restores RSP/RBP. It does not model the OS module loader, full C++ vector, or firmware malloc. Evidence: `../native-vector-minimal-experiment-1/result.json`.
