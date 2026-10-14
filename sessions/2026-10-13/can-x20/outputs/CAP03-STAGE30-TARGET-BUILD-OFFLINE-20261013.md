# #3 stage 30 PS5-target build receipt

**Status:** compiled and linked offline; not run on a console.

Source: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-v2-query-guard-20261013-30`

The pinned ps5-payload-sdk v0.43 wrapper initially could not find LLVM because `LLVM_CONFIG` was unset. Supplying its already bundled LLVM 21.1.8 `llvm-config` made `prospero-clang++` report target `x86_64-sie-ps5`. I compiled the unchanged stage 30 `CodePatch.cpp` and `PatchDiagnostic.cpp` with the SDK driver and linked them into `cap03-diagnostic.elf` in the workspace's intermediate folder. Both object compiles and the link exited 0 with no diagnostic output. No shared candidate or kernel source was edited.

The resulting files are x86-64 ELF objects and a FreeBSD/PS5-style PIE executable. The ELF contains `_start`, `main`, `run_cap03_diagnostic`, and `cap3_diag_probe_fn`; its dynamic imports include `sceKernelQueryMemoryProtection` and `sceKernelVirtualQuery`. The generic `llvm-readelf` tool reports a `PT_DYNAMIC` table warning for this PS5 ELF format; it reports the same warning on the earlier pinned #3 diagnostic ELF. This build receipt establishes target compilation and linking, not runtime compatibility.

In a scratch copy of stage 30, the host protocol passed **112/112** checks. The runner's static disassembly checks passed on both host and PS5 objects: `kekcall_check` contains CHECK sub-op `0xFFFFFFFF00000027` and a `syscall` instruction. The source and binary hashes, host log, target disassembly, objects, and ELF are in the [manifest](cap03-stage30-target-build-manifest-20261013.json) and [evidence bundle](cap03-stage30-target-build-evidence-20261013.zip). The ZIP passed CRC verification.

Console mapping, memory-protection behavior, cache coherence, restoration, and in-game effect remain unverified. This closes the stage 30 target-build gap but does **not** make #3 a console-only wait: generic detour/trampoline construction and integration still need offline work. The ELF has not been uploaded or executed.
