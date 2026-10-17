# Independent native shared-core review — 2026-09-30

Latest verdict after scoped re-review of fix commit `a34fef5`: **NC-1 ADDRESSED; NC-2 ADDRESSED. Spec PASS; code quality PASS for the reviewed implementation checkpoint.** No residual finding or new regression was found in the fix. The original review below is preserved as history; the scoped re-review appendix records the corrected artifact identities and supersedes the earlier changes-required verdict and artifact table. Console execution remains unverified.

Reviewed commit package `29457e1..5d35ed6`, the exact Task 1 brief and native-core design, the implementation report, all production/test changes, and the actual ignored `native-host/build/{loopback,development_lan,desktop_fixture}` artifacts. This was a read-only production review: no product edits, staging, commits, device contact, launches, or subagents. Only this report was written. No previously verified test suite or build was repeated.

## Spec verdict: changes required

The C ABI, shared parser forwarding, hand-owned DDS fixtures, cached host report, HTTP compatibility, source/tool provenance, and ordinary SDK build structure satisfy the reviewed scope. Two required runtime checks do not survive the actual native `-Os` build. Resolve findings NC-1 and NC-2, rebuild identities, and re-review the emitted paths before treating a console report as the requested TLS/RAII evidence.

## Code-quality verdict: changes required

No additional ABI bounds, exception escape, HTTP lifetime/CSP, dependency identity, or ELF bounds defect was found. Both findings concern observability of diagnostic checks in optimized production artifacts, not an assertion that the compiler violates C++ semantics or that the ordinary exception runtime is broken. Tests that only assert the returned booleans cannot detect these missing operations.

## Findings

### NC-1 — [P2] Preserve the TLS mutation and observation in the native self-test

Location: `outputs/ps5-mod-bridge/native-host/core_bridge.cpp:40–47` (especially line 45).

The design requires an actual scalar TLS value of 41, a helper mutation to 42, an observation of 42 across helper calls, then restoration to 41. In both final native artifacts the helpers inline and the compiler deletes the 42 store and subsequent read. `thread_local_single_thread` therefore reports only that the first TLS address yields 41. It can pass without exercising the mutation/second-lookup behavior this checkpoint promises.

The loopback final `mb_core_selftest` has one `__emutls_get_address` call at virtual address `0xb45e`, a compare with 41 at `0xb466`, and a store of 41 at `0xb470`. Development has the same sequence at `0xb47e`, `0xb486`, and `0xb490`. There is no store of 42 or later read/compare of 42. Both native `core_bridge.o` files show the equivalent sequence at function-relative `0xd7e`, `0xd86`, and `0xd90`, with one emutls relocation.

```asm
callq __emutls_get_address
cmpl  $0x29, (%rax)
movl  $0x29, (%rax)
sete  %sil
```

Make the required memory transitions observable under the existing optimized native compiler. For example, use deliberate volatile accesses and non-inlined TLS helpers (or a separately compiled helper boundary) so the emitted artifact really mutates and observes 42 through the intended TLS path. Retain the 41 reset and repeated-call determinism. Add a narrow verification of the optimized native path, rather than relying only on a `true` JSON value.

### NC-2 — [P2] Preserve the exactly-once guard cleanup observation during unwinding

Location: `outputs/ps5-mod-bridge/native-host/core_bridge.cpp:49–60` (especially lines 49, 54, and 60).

The guard's only side effect is incrementing a nonvolatile local counter. The native optimizer proves `destroyed == 1`, removes the counter and its cleanup operation, and reduces `exception_raii` to the exception message comparison. The final object still executes allocation/construction, `__cxa_throw`, an inner catch/rethrow, and an outer catch. That is useful throw/catch evidence, but no runtime guard-side effect is observed, so it cannot prove exactly-once owned guard cleanup during unwinding as required.

In both native objects, the throw occurs at function-relative `0xc88`; the inner catch/rethrow is at `0x16d3–0x16f1`; the outer catch executes `what()` and `strcmp` at `0xcbf–0xce3`. The returned check at `0xcf1–0xcf8` tests only the saved `strcmp` return value. There is no destruction counter update or counter comparison on these cleanup/catch paths:

```asm
callq __cxa_begin_catch
callq *0x10(%rcx)       # error.what()
callq strcmp
movl  %eax, %r12d
callq __cxa_end_catch
testl %r12d, %r12d
sete  %sil             # exception_raii
```

Use an observable exactly-once cleanup side effect and an actual runtime read of it after the nested catch/rethrow, while preserving unexpected-exception containment. A carefully implemented volatile counter or an opaque non-inlined cleanup boundary is sufficient; no replacement runtime is needed. Confirm the side effect is on the unwind path in the optimized final object and ELF. Keep the fixed-message test as a separate part of the boolean.

## Exact reproduction

These commands inspect existing binaries only and exit 0. Run from the reviewed repository root `/WORKSPACE/.codex/worktrees/translation-core/what`:

```sh
outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-objdump -dr --section=.text.mb_core_selftest outputs/ps5-mod-bridge/native-host/build/loopback/core_bridge.o
outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-objdump -dr --section=.text.mb_core_selftest outputs/ps5-mod-bridge/native-host/build/development_lan/core_bridge.o
outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-objdump -d --disassemble-symbols=mb_core_selftest outputs/ps5-mod-bridge/native-host/build/loopback/modbridge-native-host-loopback.elf
outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-objdump -d --disassemble-symbols=mb_core_selftest outputs/ps5-mod-bridge/native-host/build/development_lan/modbridge-native-host-development-lan.elf
```

The same native object offsets apply to both profiles. The linked ELF's TLS sequence is shown above. Inspect the complete `mb_core_selftest` section, including throw/catch landing paths, to reproduce NC-2; symbol retention alone does not establish that the guard's counter exists.

## Reviewed contract and evidence

| Area | Review result |
| --- | --- |
| C ABI | Exact statuses 0–4; 8,388,608-byte cap; 2048-byte startup capacity; C translation-unit header; C++ declarations and definitions are `extern "C"`/`noexcept`. No containers, exception objects, or heap ownership cross C. |
| Caller bounds | Required length starts at zero; nonnull nonempty output starts empty; invalid pointer combinations reject; input cap precedes input dereference; dump completes before length/copy; capacity must exceed length; size-query/short buffers retain empty output and return length excluding NUL. Caller storage must be valid and disjoint as disclosed. |
| Exception boundary | Both public functions catch all exceptions from parser/self-test/serialization. Internal failure leaves zero length; no partial copy occurs before success. The isolated failing-global-new harness tests containment and recovery without a production hook. |
| Parser facts | Actual unchanged `inspect_dds` receives `native_input.dds`, the available span, and true total size. Empty, invalid, unsupported, arbitrary FourCC, contradiction, truncation, and prefix outcomes remain JSON. Writer and game acceptance remain unresolved. |
| Fixed DDS evidence | Production bridge constructs its own fixtures. Legacy sizes 128+32+3=163 and end 160; cube sizes 148+12×16+2=342 and end 340, with literal offset/mip/item checks; truncation is 159 bytes. Reconstruction compares every byte; no test fixture header or parser-derived expected size. |
| Seven startup booleans | SHA, nonempty allocation, DDS roundtrips/truncation, false-check aggregation, and deterministic repeated JSON are covered. TLS transition and exactly-once RAII observation need the changes above. |
| Host/report/CSP | Self-test is cached once before listen. Failure uses fixed `unavailable` JSON, service continues. `/core` emits the cache and `/report.core` embeds it. Existing report version/schema/phase and unknown firmware/game fields remain. No new script/CSP change; unchanged `textContent` renderer handles the additive object. Both installation flags remain false. |
| Request/lifecycle compatibility | Production diff leaves parsing, Host/Origin/framing gates, deadline reservation, fixed-route rules, lifetime, socket cleanup, owned shutdown, and native `_exit` behavior intact. New routes pass through the existing gates; no remote self-test rerun or inspection upload. |
| Builder/provenance | Separate C11/C++20 objects; final native link through unchanged SDK v0.43 C++ wrapper; full runtime retained; no section GC. Both wrapper files independently match verified SDK archive bytes. All six pinned tool paths, including clang++, independently match their hash pins. Existing offline dependency verifier precedes compilation. |
| Source closure | All 13 manifest source hashes independently match files, including bridge/shared/vendor inputs and transitive local headers. Shared parser/hash/header/vendor sources are unchanged in the reviewed commit range. The documented 11 compiled local inputs are covered. |
| Native ELF | Independently reran the read-only `elf_layout` inspection on both final ELFs and matched the complete manifest objects. ET_DYN x86_64, entry 0 in executable first load, three 16 KiB-aligned loads, valid file/memory/table/section-derived-prefix bounds. Ordinary RWE first load is explicitly disclosed. |
| Runtime roots/imports | Both profiles have 120 undefined dynamic imports, including `__divti3`, pthread/TLS, allocation and libc/file/locale functions. `_start`, `__crt_start`, EH/unwind/personality and emutls roots remain. Empty init/fini arrays are compatible with this bridge's absence of dynamic static initializers. Retention is build evidence; import resolution and runtime semantics require the later console receipt. |
| Execution/product limits | All three manifests explicitly have `console_executed=false`. Desktop remains a fixture; intended application runtime remains PS5-only. No account/outbound service action, custom kernel/privilege action, injection, game write, texture writer, or installability added. Historical artifacts are preserved; fresh retention and console evidence are controller follow-up work. |

## Independently checked artifact identities

| Profile | Build ID | Artifact SHA-256 | Bytes |
| --- | --- | --- | --- |
| loopback | `d77db496b69e5fdf2d463fe6b0d5742cac748703bf43b6e8e8dc1200721f8132` | `a0fc750514792105b03dc74620fa778bd56a779cd51b4e2b1766d5576bd5ea1d` | 1490128 |
| development_lan | `f85ba2ae679cfcec32aad09da063de9243121fa9dfa806fc03e6e021cf06bf17` | `89806c86ae9de95b228d2fc2fdbcbf503a39df93223ac6eb482f2071292c5923` | 1490128 |
| desktop_fixture | `f42e44184a85487ba8296bb84872aaca4efee14b224e5eb9a255a68136d42965` | `1e11ba7b739798c99f0ea204b42454356c9316258462433d65146d93cb03884c` | 271680 |

Default loopback remains `127.0.0.1:8085` with no automatic lifetime; explicit development LAN is `ps5-host-not-configured.invalid:8085` with a 600-second bound; desktop fixture is loopback with a 600-second default. No blanket 600-second requirement was imposed on the ordinary production loopback profile.

The supplied report records 25 host/core/builder tests, 5 sanitizer tests, 13 DDS regressions, the real browser fixture, and all three builds as passing. Review inspected the relevant harnesses and avoided redundant reruns. These passing booleans do not invalidate the two independently reproduced optimized-code findings. There is no console execution or process-exit attestation from this review. Existing imports, emulated TLS, allocator and EH behavior remain target assumptions until the corrected reviewed artifact has a matching native receipt; writer/game acceptance remain Unsupported and noninstallable regardless of that receipt.

## Scoped re-review — fix commit a34fef5

Reviewed `.superpowers/sdd/2026-09-30-native-core-plan/review-5d35ed6..a34fef5.diff`, the changed production bridge, new emitted-code inspector, added cleanup-mismatch test, implementation-report appendix, and all corrected build manifests. No source/SDK edits, builds, broad-suite reruns, staging, commits, console contact, or launches occurred. The root supplied an independent passing run of the new inspector (2 tests); this reviewer independently inspected the emitted instructions rather than repeating that command. The inspector's receipt-file writes were not invoked by this reviewer.

**Spec verdict: PASS for this implementation checkpoint. Quality verdict: PASS.** Both original findings are addressed; no residual/new regression was identified in this narrow fix. This approves the local implementation/build evidence for controller integration and the separately authorized target checkpoint, without asserting target execution, import resolution, worker-thread support, installation, or game acceptance.

### NC-1: ADDRESSED

Updated location: `outputs/ps5-mod-bridge/native-host/core_bridge.cpp:41–49`.

The scalar is now volatile TLS; change/observe/reset/check have retained non-inlined boundaries. In both optimized objects and both final native ELFs, self-test calls `tls_check`; its helper order is observe/change/observe/reset. Each helper calls `__emutls_get_address` for the same `tls_value` descriptor. Observe reads actual TLS memory, change stores 42, and reset stores 41. The check compares the two returned observations with 41 and 42 (emitted as XOR/OR/zero-test), then resets before returning. These are now real runtime operations, not compiler-substituted facts.

Independently inspected loopback ELF call sites: observe `0x1060e`, change `0x10618`, observe `0x1061d`, reset `0x10629`; corresponding comparisons at `0x10615` and `0x10625`. Observe reads `(%rax)` at `0x13bf1`; change stores 42 at `0x13c06`; reset stores 41 at `0x13c1f`. The same instructions/control flow are present in development with the local virtual addresses shifted by `0x20`, and in both `core_bridge.o` files through the retained helper/emutls relocations. Existing repeated-call tests still cover reset/determinism; the fix adds no threads or worker-thread claim.

### NC-2: ADDRESSED

Updated location: `outputs/ps5-mod-bridge/native-host/core_bridge.cpp:52–67`.

The volatile stack counter is initialized to zero. The non-inlined owned destructor performs an actual memory increment; the outer catch calls a non-inlined volatile observer and checks its returned counter against one before comparing the fixed message. The nested throw/catch/rethrow remains. In loopback ELF the original throw is at `0x10557`; the cleanup landing block calls the owned Guard destructor at `0x1057c`; its body executes `incl (%rax)` at `0x13b19`. The inner catch rethrows at `0x105e2`; the rethrow landing path ends that catch and jumps to `0x10587`, after the cleanup site. The outer catch calls `guard_observe` at `0x1059c`; the observer loads `(%rdi)` at `0x13b1c`, and the check compares eax with one at `0x105a1`. The message comparison remains independently conditional on that result. The object and development ELF preserve the same operations/control flow.

The added isolated cleanup-by-two fixture is a meaningful regression: it expects `exception_raii=false` and overall `failed` while the fixed exception message still matches. Production has no failure-injection switch. The public ABI catch-all remains unchanged, and the local volatile/non-inlined operations add no new runtime or ownership boundary.

### Independent read-only checks and corrected identities

All commands used `llvm-objdump` or a local read-only Python inspection and exited 0. The checks disassembled the complete relevant helper/exception/selftest bodies of both optimized objects and both final ELFs, verified the actual operations and linkage described above, manually traced the inner rethrow landing path, recomputed every manifest source/artifact SHA-256 and build ID, and compared the complete native `elf_layout` results with both manifests. `git diff --check` on this report also exited 0.

Exact revised bridge source SHA-256: `015a20a471e65465a887743d80f382960e1f6001c34d82da639d9c8a8b84e01e`. Both optimized native `core_bridge.o` files independently hash to `5187534fb6a025595e35c5b0a9a15b7fdf362fa109f9275d844dc4b673045434`. The other 12 production source-identity entries match the original review and current manifests.

| Profile | Corrected build ID | Corrected artifact SHA-256 | Bytes |
| --- | --- | --- | --- |
| loopback | `936b0e54607c61b06465247bb9882c7491e0c16e1333bf435462ea375e694e77` | `48e3db808129fd845807a19023da52ec4eacc61463af307965acede278f657eb` | 1490568 |
| development_lan | `c088db7ccd5bbcf16276fd25c74912f62230e06c86015549fca206fda3ed6462` | `39ca575e447399aeb3cbd4e5d831bfbd77177527414cab064601e771a5de07d3` | 1490568 |
| desktop_fixture | `d8fb7faee7007bda1c8675ae73c7f7ec9e0b92062141d8080bad4cf4a1fc36b1` | `0f05c621bf1139edd2edd3c513517e5117635d639fbc68410be29c0a6c01245f` | 272112 |

Both corrected native layouts retain entry 0, three 16 KiB-aligned loads, first-load RWE, valid file/memory/table/prefix bounds, section count 24, section-table offset 1,489,032, loader prefix/table end 1,490,568, and maximum load file end 1,049,160. Profile/default/lifetime configuration is unchanged. All three manifests still have `console_executed=false`. ABI/header/host/HTTP/builder/SDK/shared-parser sources are untouched by the fix, so their previously reviewed contracts remain applicable.

The native inspector is deliberately pinned to LLVM 21.1.8 x86_64 emitted shapes and run explicitly after native builds; it does not impose SDK build prerequisites on ordinary desktop tests. Its checks plus manual control-flow inspection establish retained probe operations. Actual matching console reports are still required for native execution evidence, and target writer/game acceptance stay Unsupported and noninstallable.
