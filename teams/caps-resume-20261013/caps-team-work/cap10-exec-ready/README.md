# Executable-memory readiness observation

The actual final signal-ready owner completed initialization, two genuine managed Runs, one deliberate managed exception, recovery and cleanup using the fixed v2 source-built host runtime and matching `full,interp,static` corlib. A forwarded Darwin API trace observed **33 mmap calls and seven mprotect calls** during that single owner lifetime.

There was **one executable request**: a successful 4096-byte mmap with `PROT_WRITE|PROT_EXEC` from `mono_valloc`, at host dylib offset `0x29f309`. The source and compiled instructions identify it as Mono's Darwin hardened-runtime capability probe. No other executable mmap and no executable mprotect appeared. All 40 events were retained, with no dropped events or failed owner-phase requests.

This is not a zero-executable-memory claim. It is also not target execution. The observed Darwin probe is compiled out in the actual target `mono_valloc` object. Source correlation supports the expectation that this exact interpreter/AOT workload uses already compiled trampolines without requesting new executable memory on the target. It does not prove console behavior or remove the general executable-memory provider requirement.

| Evidence | Result |
|---|---|
| `evidence/EXEC-HOST-CHECK.json` | Actual final owner lifecycle and signal/mask preservation pass; all inputs unchanged |
| `evidence/EXEC-SOURCE-CORRELATION.json` | Host/target source hashes, source paths and line excerpts for mode, interpreter bridges, AOT pools, allocation and optional executable-protection paths |
| `evidence/COMPILED-CALLSITE-CORRELATION.json` | Host caller address and target function disassembly bound to actual v2 archives |
| `native/ExecTrace.c` | Host-only dyld interposer forwarding real mmap/mprotect unchanged; fixed storage and atomic recording |
| `native/ExecControls.hpp` | Four genuine control calls verify interception, executable flag recording and failures; counters reset before owner |
| `logs/exec-check.stdout.log` | Complete actual managed result plus all 40 memory events |

The control uses a normal writable mapping and read-only protection, then two executable-flag syscall requests that fail without creating executable memory: zero-length mmap and mprotect on the unmapped NULL range. They return EINVAL and ENOMEM respectively. No signal is raised and no CPU fault is caused. The diagnostic generates or writes no machine code and executes no newly generated code. The genuine runtime later performs its own normal Darwin permission probe and executes its precompiled runtime/corlib code.

The actual v2 Mono dylib imports `_mmap` and `_mprotect`. It has no Mach VM allocation, protection or remap imports. The Mach AOT page-remap implementation is under `MONOTOUCH`, which is inactive; non-MONOTOUCH `USE_PAGE_TRAMPOLINES` is zero. The tracer therefore covers the actual directly imported Mono memory APIs. It does not audit dyld mappings before the measured window, kernel VM syscalls, or libc-private/internal VM mechanisms. All recorded owner-phase callers belong to the genuine Mono dylib, confirming coverage beyond the harness controls.

`MONO_AOT_MODE_INTERP` sets `mono_aot_only`, `mono_use_interpreter` and `use_aot_trampolines`. Interpreter/native bridges choose the precompiled corlib trampoline functions. Non-MONOTOUCH argument trampolines select precompiled pool entries and write arguments into GOT data; exhausted pools report an error rather than silently generating replacements. The AOT-only domain code manager is marked read-only. The general code allocator still contains real RWX requests if other routes reach it.

The remaining dependency is precise: the target loader must map the already compiled image text executable and perform its relocations. Any genuine `PROT_EXEC` mprotect request still reaches the public SDK's unchecked unresolved weak `kernel_mprotect` hook. The optional `collect-pagefault-stats` AOT debugging handler explicitly requests RWX mprotect, and other code-generating/profiler modes or workloads are outside this proof. #1/#3 provider and kernel work remains ZCode's assignment. No provider behavior was implemented here.

Pinned measured inputs:

- Final owner `cap10-signal-ready/native/MonoOwner.cpp`: SHA256 `03edcc27dced6d2a1fa03162bbe3fbd74345e9d2207653688422a78d086c0bad`.
- Actual v2 host runtime dylib: SHA256 `57f7b88375e3c7e9be91423db3fbf7e6bd86fb970a3be0b6ea4387d725fbf35a`.
- Matching static corlib object and IL are verified against `cap10-static-corlib-host/evidence/STATIC-CORLIB-BUILD.json`.
- All source, artifact, compiler/link/run commands and environment identities are in the receipts and `HANDOFF-MANIFEST.json`.

`tools/run_exec_check.py` assembles a private observation harness around the unchanged real owner/boundary and genuine provider fixture, builds serially, verifies the actual imports/interpose section, and runs one measured process. It refuses to overwrite its completed run receipt. Reproduction requires a fresh owned sibling component carrying these sources/tools. `tools/audit_exec_source.py` inspects the saved observation without repeating runtime execution.

There was no PS5 contact, target execution, game/PRX build, fault injection, global install, frozen-source edit or arbitrary managed-library compatibility claim. The completed native PAL folder remains untouched. The host runtime has JIT code compiled in but the actual owner selects AOT-only interpreter mode; the target additionally has `DISABLE_JIT=1`. Those differing configurations and the absence of console execution remain explicit limits.
