# Native shared-core implementation evidence — 2026-09-30

Implementation is ready for controller review. This is local implementation/build evidence, not a reviewed artifact package or a console execution receipt. Baseline worktree HEAD is `29457e1`; files are unstaged/uncommitted. No console contact, dependency fetch, index write, commit, runtime replacement or section garbage collection occurred.

## Implemented behavior

`native-host/core_bridge.h/cpp` exposes the exact C statuses 0–4, 8,388,608-byte available-input cap and 2048-byte startup report capacity. Both C++ entry points are `extern "C"` and `noexcept`, reset caller state before work and catch every exception. The public inspection calls unchanged production `inspect_dds` with `native_input.dds` and the true `total_size`. JSON is completely owned/serialized before copying. Query/short-buffer paths return the required byte count excluding NUL, preserve empty output and never copy partial JSON. Success requires capacity greater than length. Malformed/unsupported/truncated parser outcomes stay JSON and retain unresolved PS5 writer/game acceptance requirements.

Seven built-in fixture booleans cover the empty SHA-256 literal, nonempty owned vector/string allocation, exactly-once nested exception guard destruction, scalar TLS 41→42 observed across helper calls then reset to 41, 5×7 legacy DXT1 (128+32+3=163 bytes), DX10 BC7 cube with two mips (148+12×16+2=342 bytes), and 159-byte legacy truncation. Fixture expected lengths/offsets are independent literals; no shared test fixture header is included. Any false boolean produces `failed`; an unexpected exception produces INTERNAL_ERROR with empty output. The actual C caller repeats self-test inside one process to exercise TLS reset and deterministic JSON. A temporary mutated bridge expected hash tests the false-check aggregation without adding a production hook.

The C host runs self-test once before listening, caches the result in `host_info.core_report`, and continues service with a fixed unavailable JSON fallback on ABI failure. `GET /core` returns the cached object and `/report.core` embeds it. Schema/version/phase, unknown firmware/game fields, script bytes/CSP, request framing/Host/Origin/deadline/lifetime/shutdown gates remain compatible. Both top-level and core `installation_supported` are false. An isolated C failed-bridge fixture verifies report/core responsiveness and owned shutdown on INTERNAL_ERROR.

Builder keeps host/http as C11 and bridge/dds/hash as C++20, with separate objects for desktop and SDK profiles; final native linking uses the unchanged SDK v0.43 C++ wrapper. Existing bundled bootstrap verification runs offline before compilation (7 dependency files). Cached clang++ is the existing relative alias to clang, and its bytes verify to `8501bb9d3deb0250f23422a759b94651c89479937a0af29a7c7f0b98eaf3ca5a`. SDK archive size/hash verification, safe extraction and existing LLVM/ELF validation remain. Manifests identify 13 stable repository-relative files. A compiler `-M` closure audit found exactly 11 compiled local project/vendor inputs, all included; builder and Makefile account for the other two identity entries.

## Verification

Commands below use the native-host working directory unless otherwise stated. Every final verification command exited 0.

| Command | Result |
| --- | --- |
| `make test` | 25 tests, 0 failures (6 builder, 5 core, 14 actual host); final run 20.110 s |
| `make sanitize-test` | 5 core tests, 0 failures; actual C caller and production bridge/dds/hash compiled with ASan+UBSan; final run 15.411 s |
| `make test-dds` from `outputs/ps5-mod-bridge` | 13 tests, 0 failures; unchanged shared regression checkpoint |
| `python3 build.py --profile desktop_fixture` | Separate C/C++ desktop build; 7 vendored files verified |
| `python3 build.py --profile loopback` | Ordinary full SDK C++ cross-build; 7 vendored files verified; ELF/import/section/symbol inspection |
| `python3 build.py --profile development_lan --ps5-address ps5-host-not-configured.invalid` | Explicit-address 600-second development profile; same ordinary build checks |
| `NODE_PATH=/WORKSPACE/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules /WORKSPACE/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node tests/browser_fixture.cjs` | 1 real Chromium fixture passed: unchanged CSP, displayed additive core object/all seven booleans, core/report equality, keyboard shutdown, actual exit, no page errors |
| Compiler `cc/c++ -M` closure audit for all five translation units | 11 compiled project/vendor inputs; all covered by source identity |
| `git diff --check` | No whitespace errors |

Meaningful RED preceded production implementation: `python3 -m unittest discover -s outputs/ps5-mod-bridge/native-host/tests -p test_host.py -k cached_core -v` exited 1 with 1 actual-host test failing (`404 != 200`). The analogous `test_core.py` command exited 1 with missing public C bridge assertion in class setup (0 tests, 1 error). Focused builder identity and clang++ verifier commands each exited 1 with a missing API error. GREEN followed implementation. An earlier interim native-host run passed 23 tests; added failure/false-check coverage and final boundary checks bring the final suite to 25.

Prerequisite/tool errors were fixed and are not omitted: initial plain `node tests/browser_fixture.cjs` exited 1 because Playwright was not on the local module path; the existing bundled runtime resolved it without installing/fetching. One test-file generation script ran from the native-host directory with repository-relative paths and raised FileNotFoundError before writing; it was rerun from the repository root. One `python3 build.py --profile loopback` invocation from the repository root exited 2 because that file is under native-host; the final correct-directory build passed. These did not alter production behavior. No broad `test-all` run was needed because shared sources and broader product behavior were unchanged; the complete native-host suite and designated shared DDS checkpoint ran.

## Exact source/profile and artifact identity

All three final manifests have `console_executed=false`. Build outputs remain under ignored `native-host/build/<profile>` for controller review; the controller owns subsequent retention in `outputs/native-core-artifacts`. Historical `outputs/native-host-artifacts` was preserved.

| Profile | Build/source/profile ID | Artifact SHA-256 | Bytes |
| --- | --- | --- | --- |
| loopback | `d77db496b69e5fdf2d463fe6b0d5742cac748703bf43b6e8e8dc1200721f8132` | `a0fc750514792105b03dc74620fa778bd56a779cd51b4e2b1766d5576bd5ea1d` | 1490128 |
| development_lan | `f85ba2ae679cfcec32aad09da063de9243121fa9dfa806fc03e6e021cf06bf17` | `89806c86ae9de95b228d2fc2fdbcbf503a39df93223ac6eb482f2071292c5923` | 1490128 |
| desktop_fixture | `f42e44184a85487ba8296bb84872aaca4efee14b224e5eb9a255a68136d42965` | `1e11ba7b739798c99f0ea204b42454356c9316258462433d65146d93cb03884c` | 271680 |

Loopback binds `127.0.0.1:8085`, lifetime 0; development binds `ps5-host-not-configured.invalid:8085`, lifetime 600; desktop binds loopback, lifetime 600 by default and reports desktop origin. Manifests include SDK v0.43 commit `d9c9519116944a7f1c22d262012c53b80b3520b7`, archive SHA-256 `a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb`, LLVM 21.1.8 and pinned tool hashes.

| Repository-relative input | SHA-256 |
| --- | --- |
| `outputs/ps5-mod-bridge/include/modbridge/core.hpp` | `32ed69fe0adbb3925b0cfe6418508eec08bcf52e72351c3b8eb06013821250d7` |
| `outputs/ps5-mod-bridge/include/modbridge/dds.hpp` | `7d5f84cd9c226d14c7e4d1fc963564073274d3271b42ee55fe8fe40f72438b1e` |
| `outputs/ps5-mod-bridge/native-host/Makefile` | `05986873e8262a25addefba1d653abf6364faa90a656bcf4ecf71206da5c5609` |
| `outputs/ps5-mod-bridge/native-host/build.py` | `629676eed06e3fd981317ce3dca0eb60eb963a347b1d29a17a334d3fbad4eef0` |
| `outputs/ps5-mod-bridge/native-host/core_bridge.cpp` | `a1df6da95b7ba35d13f462737fb26e051e2b6da6a22cec98bca5b96bb7454d96` |
| `outputs/ps5-mod-bridge/native-host/core_bridge.h` | `7e59290b365f1aa7d9412862d0da0fc7cc7d08d22334efb70e4cb4c610b11e0d` |
| `outputs/ps5-mod-bridge/native-host/host.c` | `a7a8afdec81a159a18a40c8574b91a38f3146a77ef2ec2f8aeca3671e273943d` |
| `outputs/ps5-mod-bridge/native-host/http.c` | `bc0cbe7987341f4c0ac6eaee1be47d03d3820e33ad17577a151083e45583c8bf` |
| `outputs/ps5-mod-bridge/native-host/http.h` | `86ebc81012b6131abef7699727216ebd1632837c693d523c35cb8e8d2c4b02d0` |
| `outputs/ps5-mod-bridge/src/dds.cpp` | `d9acfee4ff4fb1fcbbe7abfdc2cec07a4cc867a13477c291d5c824214cab60b4` |
| `outputs/ps5-mod-bridge/src/hash.cpp` | `97414506d67824e7b6de42f58a3fb08bde32f201b60d31efd9da838f4de517f3` |
| `outputs/ps5-mod-bridge/third_party/nlohmann/json.hpp` | `9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6` |
| `outputs/ps5-mod-bridge/third_party/picosha2/picosha2.h` | `b13c180161ffac8d0adc81e033e493c409457c4d1258ab9781ac80579ba3bdd8` |

## Native ELF/runtime inspection and limits

Both ordinary x86_64 ET_DYN ELFs are 1,490,128 bytes. Entry is 0 within the executable first load at vaddr 0; ELF header end=64, program-table end=288, section-table offset=1,488,592, section count=24, section-table end and loader prefix=1,490,128, maximum load file end=1,049,160. All three loads have 16,384 alignment and valid file-backed/file≤memory bounds. Loads 2/3 have flags 6; first load flags 7 (RWE), retaining the ordinary SDK contract. First load file end is 674,869 for loopback and 674,901 for development. This is not the separate freestanding diagnostic's W^X contract.

Both profiles retain 120 dynamic undefined symbols versus 28 in historical ordinary C host (92 additions, none removed). Imports include allocation, file/locale/stdio, `__divti3`, `__cxa_atexit`, pthread synchronization and pthread key/get/set helpers. They are retained by the full unchanged SDK C++ baseline, including unchanged `hash.cpp` file-hash implementation; application code adds no remote file route or worker threads. Cross-link success does not prove that the console resolves or correctly implements each import.

Full symbol/section inspections retain `_start`, `__crt_start`, public `mb_core_*`, `__cxa_throw`, `__gxx_personality_v0`, `_Unwind_RaiseException`, `__emutls_get_address`, and explicit TLS `tls_value` descriptor/template. `.eh_frame_hdr` is 0x56c4 bytes, `.eh_frame` 0x18a68, `.gcc_except_table` 0x5efc; `.init_array` and `.fini_array` roots exist with size zero, consistent with this module needing no dynamic static initialization. No custom constructor/EH/TLS support was added or replaced. The scalar TLS test proves only the desktop single-thread path here; all native TLS/EH/allocator/selftest claims require matching console reports from a separately reviewed artifact.

Caller memory must remain valid/disjoint for advertised lengths; this ABI cannot validate arbitrary dangling pointers. An 8 MiB valid DDS with many subresources can produce large owning JSON; the caller must bound allocation/output strategy. There is no PS5 texture writer, native archive/network/account integration, installation, game acceptance, or worker-thread guarantee. The result is a reusable inspection seam plus local build evidence.


## Round 1 review fixes — NC-1 and NC-2 (base `5d35ed6100cea3cd662afd9d6b5098cd8e2d9a65`)

Status: fixes complete for independent re-review. This appendix supersedes the earlier artifact identities and native TLS/RAII observation claims; the initial implementation's true booleans did not prove that those operations survived native optimization. The independent review findings were reproduced before edits. The reviewer-owned `outputs/2026-09-30-native-core-review.md` was read and not changed. No staging/commits, console contact, subagents, dependency fetching, runtime replacement, or section garbage collection occurred.

NC-1 root cause was optimizer visibility of a nonvolatile scalar TLS value plus inlined helpers. The bridge now declares a scalar `thread_local volatile int` initialized to 41. Non-inlined observe/change/reset/check helpers preserve separate emutls lookup boundaries and the actual 41→42→41 operations at native `-Os`. Both observed values still determine the boolean. Reset still happens before the check returns, retaining repeated-call determinism.

NC-2 root cause was an unobserved nonvolatile local guard counter. The bridge now uses a volatile counter, a non-inlined destructor that increments it by one, a non-inlined volatile observer, and a non-inlined exception check. The nested runtime_error throw/catch/rethrow still unwinds the owned guard once; the outer catch reads the actual counter and requires it to equal one before checking the fixed exception message. Unexpected failures remain contained by the existing public ABI catch-all. No runtime or linking assumptions changed.

New `native-host/tests/native_code_check.py` disassembles both optimized native objects and final ELFs, without executing them. Its two tests cover four artifacts each. TLS assertions require the public selftest to call the check; exact helper call order observe/change/observe/reset; emutls lookups of the same `tls_value` descriptor; actual memory store of 42, memory read, memory reset to 41; and both comparisons (LLVM emits XOR+OR+sete). RAII assertions require the public selftest/check linkage; actual destructor memory increment and observer memory load; throw, inner rethrow, catch paths; one emitted owned-destructor call on cleanup before catch handling; a rethrow landing path that jumps back into outer observation after cleanup; actual comparison of returned counter with one; and the independent fixed-message comparison (final ELF's indirect call is checked against its `strcmp` dynamic relocation). Full inspection receipts are in `build/<native-profile>/optimized-object.txt` and `optimized-elf.txt`. The test is an explicit native-build inspection command rather than an automatic desktop unittest, so it does not silently require SDK artifacts for ordinary desktop tests. It intentionally targets pinned LLVM 21.1.8 x86_64 instruction output.

The core suite now also compiles an isolated temporary bridge whose cleanup increments by two. It verifies status failed and exception_raii=false even while the exception's fixed message still matches; all six other checks remain true. This guards against reducing RAII to message-only evidence. Existing same-process repeated determinism, false SHA-check aggregation, failing allocator/recovery, actual C bounds/parser evidence, failed host fallback and browser behavior remain covered.

### RED/GREEN commands and results

Commands use `outputs/ps5-mod-bridge/native-host` unless another working directory is named.

| Command/check | Exit and evidence |
| --- | --- |
| Reviewer reproduction `llvm-objdump -dr --section=.text.mb_core_selftest build/loopback/core_bridge.o` and `llvm-objdump -d --disassemble-symbols=mb_core_selftest build/loopback/modbridge-native-host-loopback.elf` | 0 each; preserved as `review-before-object.txt`/`review-before-elf.txt`; original native object had only one TLS lookup, compare/store 41, no store/read 42; guard counter/cleanup observation absent |
| `python3 tests/native_code_check.py` before production edits | RED: exit 1; 2 tests, 8 failing artifact subtests; original objects/ELFs lacked the retained operation boundaries, consistent with the independently inspected deleted operations |
| Native loopback and development builds after NC-1 alone | Both exit 0; then TLS-only inspection became GREEN on all four artifacts |
| `python3 tests/native_code_check.py OptimizedNativeChecks.test_tls_41_to_42_observation_and_reset_survive_native_optimization` | 0; 1 test, 4 artifact subtests, 0 failures |
| `python3 tests/native_code_check.py` after both fixes | GREEN: 0; 2 tests, 8 artifact subtests, 0 failures; 0.556 s |
| `make test` | 0; 26 tests, 0 failures (6 builder, 6 core, 14 actual-host), 22.823 s |
| `make sanitize-test` | 0; 6 focused core tests, 0 failures, 17.896 s; actual C caller/production bridge/shared sources instrumented with ASan+UBSan |
| `python3 build.py --profile loopback` | 0; final ordinary SDK artifact rebuilt; offline dependency verifier passed 7 files |
| `python3 build.py --profile development_lan --ps5-address ps5-host-not-configured.invalid` | 0; final explicit-address ordinary SDK artifact rebuilt; offline dependency verifier passed 7 files |
| `python3 build.py --profile desktop_fixture` | 0; final separate C/C++ desktop fixture rebuilt; offline dependency verifier passed 7 files |
| `NODE_PATH=/WORKSPACE/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules /WORKSPACE/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node tests/browser_fixture.cjs` | 0; 1 real browser fixture passed, all seven corrected core checks displayed; core/report equality, CSP, keyboard shutdown and actual desktop exit |
| Final source/artifact SHA checks against every manifest plus `git diff --check` | 0; all three source/profile/artifact snapshots still match; no whitespace errors |

Inspection test development exposed two overly specific test assumptions after the fix, which were corrected after inspecting emitted code: LLVM compares the TLS observations via XOR/OR/zero-test rather than cmpl, and it places the outer-catch block before the inner-rethrow block in file order. The inspector now checks actual operations/control-flow targets instead of assuming file order equals execution order, distinguishes the application's Guard destructor from an SDK LockGuard symbol, and resolves the final indirect strcmp through its relocation. Interim TLS inspection returned 1 with four assertion failures from the comparison mnemonic assumption; interim full inspection returned 1 with four assertion failures from symbol/file-order assumptions. No extra production fix was added in response to these test assumptions. A mistaken repository-root `python3 build.py --profile loopback` invocation exited 2; the correct-directory native builds above passed. All are disclosed; final tests are green. The unchanged broader 97/96/UI suites and shared DDS sources were not rerun/changed for this narrow bridge fix.

### Final corrected source/profile and artifact identities

All manifests still record `console_executed=false`; files remain under ignored build directories for controller retention/review. The revised core source SHA-256 is `015a20a471e65465a887743d80f382960e1f6001c34d82da639d9c8a8b84e01e`. Both native optimized `core_bridge.o` files have SHA-256 `5187534fb6a025595e35c5b0a9a15b7fdf362fa109f9275d844dc4b673045434`. Other 12 production identity inputs remain unchanged; tests/report files do not alter production build identity.

| Profile | Corrected build ID | Corrected artifact SHA-256 | Bytes |
| --- | --- | --- | --- |
| loopback | `936b0e54607c61b06465247bb9882c7491e0c16e1333bf435462ea375e694e77` | `48e3db808129fd845807a19023da52ec4eacc61463af307965acede278f657eb` | 1490568 |
| development_lan | `c088db7ccd5bbcf16276fd25c74912f62230e06c86015549fca206fda3ed6462` | `39ca575e447399aeb3cbd4e5d831bfbd77177527414cab064601e771a5de07d3` | 1490568 |
| desktop_fixture | `d8fb7faee7007bda1c8675ae73c7f7ec9e0b92062141d8080bad4cf4a1fc36b1` | `0f05c621bf1139edd2edd3c513517e5117635d639fbc68410be29c0a6c01245f` | 272112 |

| Repository-relative production input | Final SHA-256 |
| --- | --- |
| `outputs/ps5-mod-bridge/include/modbridge/core.hpp` | `32ed69fe0adbb3925b0cfe6418508eec08bcf52e72351c3b8eb06013821250d7` |
| `outputs/ps5-mod-bridge/include/modbridge/dds.hpp` | `7d5f84cd9c226d14c7e4d1fc963564073274d3271b42ee55fe8fe40f72438b1e` |
| `outputs/ps5-mod-bridge/native-host/Makefile` | `05986873e8262a25addefba1d653abf6364faa90a656bcf4ecf71206da5c5609` |
| `outputs/ps5-mod-bridge/native-host/build.py` | `629676eed06e3fd981317ce3dca0eb60eb963a347b1d29a17a334d3fbad4eef0` |
| `outputs/ps5-mod-bridge/native-host/core_bridge.cpp` | `015a20a471e65465a887743d80f382960e1f6001c34d82da639d9c8a8b84e01e` |
| `outputs/ps5-mod-bridge/native-host/core_bridge.h` | `7e59290b365f1aa7d9412862d0da0fc7cc7d08d22334efb70e4cb4c610b11e0d` |
| `outputs/ps5-mod-bridge/native-host/host.c` | `a7a8afdec81a159a18a40c8574b91a38f3146a77ef2ec2f8aeca3671e273943d` |
| `outputs/ps5-mod-bridge/native-host/http.c` | `bc0cbe7987341f4c0ac6eaee1be47d03d3820e33ad17577a151083e45583c8bf` |
| `outputs/ps5-mod-bridge/native-host/http.h` | `86ebc81012b6131abef7699727216ebd1632837c693d523c35cb8e8d2c4b02d0` |
| `outputs/ps5-mod-bridge/src/dds.cpp` | `d9acfee4ff4fb1fcbbe7abfdc2cec07a4cc867a13477c291d5c824214cab60b4` |
| `outputs/ps5-mod-bridge/src/hash.cpp` | `97414506d67824e7b6de42f58a3fb08bde32f201b60d31efd9da838f4de517f3` |
| `outputs/ps5-mod-bridge/third_party/nlohmann/json.hpp` | `9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6` |
| `outputs/ps5-mod-bridge/third_party/picosha2/picosha2.h` | `b13c180161ffac8d0adc81e033e493c409457c4d1258ab9781ac80579ba3bdd8` |

### Corrected optimized native instruction receipts

In final loopback ELF, `tls_check` at `0x10607` calls observe `0x1060e`, change `0x10618`, observe `0x1061d`, reset `0x10629`. Its observation checks XOR 41 at `0x10615` and 42 at `0x10625`, OR at `0x1062e`, then sete at `0x10631`. `tls_observe` at `0x13be0` calls emutls at `0x13beb` then loads `(%rax)` at `0x13bf1`. `tls_change` at `0x13bf5` calls emutls at `0x13c00` then stores 42 to `(%rax)` at `0x13c06`. `tls_reset` at `0x13c0e` calls emutls at `0x13c19` then stores 41 to `(%rax)` at `0x13c1f`. All use TLS descriptor `0xfc020`. Development code has the same sequence with these local helper/check addresses shifted by 0x20. Object relocations require the same retained helpers and emutls descriptor.

Loopback `exception_check` at `0x1050e` initializes counter zero, calls `__cxa_throw` at `0x10557`, and its cleanup landing block calls the owned Guard destructor at `0x1057c`. The destructor at `0x13b16` loads the counter pointer and executes `incl (%rax)` at `0x13b19`. The inner catch starts at `0x105dd`, rethrows at `0x105e2`, and its landing block ends the catch at `0x105f6` then jumps to `0x10587`, after the destructor site. The outer catch calls `guard_observe` at `0x1059c`; the observer at `0x13b1c` loads `(%rdi)` into eax, and `0x105a1` compares eax with one before message comparison. The same paths/operations remain in the development ELF shifted by 0x20 and in the optimized objects. One cleanup call site plus the actual volatile load/check and runtime mismatch/repeat tests establishes the intended optimized local probe; it remains local evidence until console execution.

Both corrected native ELFs retain 120 dynamic imports and the unchanged ordinary RWE/full-runtime contract. ELF header end=64, program-table end=288, entry=0, three 16 KiB-aligned valid loads, section count=24, section-table offset=1,489,032, table end/loader prefix=1,490,568, max load file end=1,049,160. First load file ends are 674,917 (loopback) and 674,949 (development). EH/unwind/emutls/CRT roots remain; `.eh_frame_hdr` size is now 0x56fc, `.eh_frame` 0x18b48, `.gcc_except_table` 0x5efc, init/fini arrays remain size zero. No native allocator/import/TLS/EH runtime resolution or worker-thread behavior is claimed. Installation/game acceptance and target writers remain unavailable. Large caller-owned JSON and valid/disjoint caller storage requirements remain as disclosed.

Self-review reread both findings and the changed bridge/test code, inspected both optimized objects/final ELFs and fixed control-flow blocks, checked final identities/imports/bounds, preserved existing failure/determinism coverage and all unrelated controller work (including console-context/roadmap documents). Independent re-review and any console checkpoint remain controller-owned. Historical native-host artifacts and reviewer report were preserved.
