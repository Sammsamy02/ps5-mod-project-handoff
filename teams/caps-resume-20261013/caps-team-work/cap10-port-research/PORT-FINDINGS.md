# #10 public Mono port ingredients — Mac-only

No genuine PS5 Mono runtime or build recipe was found in the local assets examined or the public PacBrew recipe inventory captured on 2026-10-13. The available public toolchain is useful, but its Mono library is an import stub. A source port remains required. This does **not** establish that proprietary Sony tools are required: those were neither used nor sought.

## Available inputs

| Input | Actual local finding |
|---|---|
| Community SDK | `what/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk`, public release v0.43, source `d9c9519116944a7f1c22d262012c53b80b3520b7`. Eleven examined wrappers/headers/library files match the pinned release ZIP exactly. |
| LLVM | `what/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin`. Existing public SDK wrapper selects `x86_64-sie-ps5`, its own sysroot, and emulated TLS. |
| SDK Mono library | `target/lib/libmonosgen-2.0.so`, 108,488 bytes, SHA256 `75014e43567671e8b8ab22935d6301450592f06d191880d66b1f4a714feeefa2`. All function exports inspected share one address; public source contains only assembler symbol labels. It cannot execute Mono. |
| Matching real host Mono | Existing extracted official 6.12.0.206 MDK, source `0cbf0e290c31adb476f9de0fa44b1d8829affa40`, x86_64 Darwin under Rosetta. This is useful for owner/API/AOT testing, not a target runtime. |
| Matching host AOT corlib | Extracted one existing package member into this scratch directory: `host-aot/mscorlib.dll.dylib`, 26,306,656 bytes, SHA256 `70e42f94884922f0c236a0790b9074f2047cfe1d4c8c332b81bc3f5aa9a91e0c`. Its full-AOT/interpreter compatibility is not established. No installer ran. |
| Target Mono source | Current `cap10/dependencies/mono-source` is a selected source/header subset, not a buildable full tree. Pinned portability/build slices in this directory supplement the investigation only. |

`LOCAL-INVENTORY.json` binds local paths, hashes and official ZIP equality. `SDK-MONO-STUB-NM.log` preserves the actual symbol read. `PUBLIC-SOURCES*.json`, `PUBLIC-DOCS.json` and `sdk-tree.json` preserve the upstream inputs. No target compilation, console contact, main-tree mutation, kernel work or mod edits occurred.

## The concrete interpreter mismatch

The production owner selects `MONO_AOT_MODE_INTERP`. In the exact [pinned public `driver.c`](https://github.com/mono/mono/blob/0cbf0e290c31adb476f9de0fa44b1d8829affa40/mono/mini/driver.c#L2941), this sets both `mono_aot_only=TRUE` and `use_aot_trampolines=TRUE`, alongside interpreter use. The stock CLI `--interpreter` selects `MONO_EE_MODE_INTERP`, whose [separate case](https://github.com/mono/mono/blob/0cbf0e290c31adb476f9de0fa44b1d8829affa40/mono/mini/driver.c#L2962) does not set either flag. The saved CLI success therefore does not cover the owner's execution mode.

The [pinned interpreter](https://github.com/mono/mono/blob/0cbf0e290c31adb476f9de0fa44b1d8829affa40/mono/mini/interp/interp.c#L1530) gets interpreter-to-native transitions from the AOT trampoline table when requested. Otherwise the [AMD64 implementation](https://github.com/mono/mono/blob/0cbf0e290c31adb476f9de0fa44b1d8829affa40/mono/mini/tramp-amd64.c#L942) reserves code space and emits native instructions. The native-to-interpreter direction has the same distinction. Turning off managed JIT compilation alone does not supply those transition functions.

The production owner was subsequently exercised by the root agent in actual `MONO_AOT_MODE_INTERP` with an owned matching corlib copy compiled using `--aot=full,interp`: Run/throw/recovery and both native imports passed (`cap10/evidence/OWNER-HOST-CHECK.json`). This minimal case did not need a separate Probe AOT module. It remains host proof; neither the stock loader nor Darwin AOT data proves the target port or production owned-loader patch.

## Genuine build and portability surfaces

The exact [configure source](https://github.com/mono/mono/blob/0cbf0e290c31adb476f9de0fa44b1d8829affa40/configure.ac#L1762) exposes `--enable-minimal=jit`; it sets `DISABLE_JIT` while leaving interpreter support available. Do not invent a `--disable-jit` switch. Preserve P/Invoke, dllmap, configuration and cleanup because the current owner uses them. Its `fullaotinterp` preset supplies `--aot=full,interp` for test assemblies; the AOT build must use the real target ABI and offsets when eventually producing target native data.

`--enable-cooperative-suspend --disable-hybrid-suspend` is a real configuration option. It is a candidate to investigate, not a proof that a game process needs no signals. The selected POSIX source still has suspend/restart/abort handlers, and `mini-posix.c` installs several process-wide handlers. Thread-policy selection alone does not establish that they are bypassed or coexist with the engine.

The public SDK provides declarations for the FreeBSD starting points (`pthread_attr_get_np`, `pthread_attr_getstack`, `thr_self`, `pthread_kill`, `sigaction`, semaphores, `mmap`, `mprotect`, ucontext). These declarations permit an honest compile investigation, but do not establish behavior under the loader or in Townfall. A fresh genuine compiler probe established both `__PROSPERO__=1` **and `__FreeBSD__=9`**, plus `_POSIX_VERSION=200112L`; the older project probe displayed only the Prospero macro. Thus existing Mono FreeBSD stack and POSIX backend selection already occurs, without defining a fake OS macro. Do not duplicate those backends. Validate actual stack limits, GC roots/TLS and process-wide memory-barrier behavior before admitting the runtime.

The concrete TLS gap now has a narrow staged fix in `port-stage/prospero-pthread-tls.patch`. The public wrapper emits emulated TLS, while pinned Mono's default compiler TLS route expects native TLS offsets. The patch detects the genuine compiler's Prospero macro, defaults that target to Mono's existing pthread TLS implementation and rejects an explicit incompatible compiler-TLS request. Actual SDK syntax/assembly probes and source policy regressions passed; this is a source configuration fix, not a target runtime build.

A buildable target source tree also needs its matching submodules, generated build inputs, System.Native implementation/configuration, AOT transition support and the staged owned-loader patch. Current shell PATH contains `make` and `xz`; Autoconf, Automake, Libtool, CMake, Ninja and pkg-config were not resolved on that PATH. No installation was attempted. The captured public PacBrew inventory has no Mono recipe; it does not supply this missing work.

The next implementation step is a real, explicit public target configuration/backend in a complete pinned source tree, followed by the integrator's compile and dependency closure. The minimum runtime proof is the unchanged Probe.dll executing Run, managed throw and recovery in Townfall through the actual owner/resolver. Until those ingredients exist, #10 remains incomplete and must not hold the ready capability builds or Stellar Blade.
