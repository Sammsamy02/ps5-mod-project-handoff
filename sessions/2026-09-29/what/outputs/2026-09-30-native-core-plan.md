# Native shared-core integration implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Integrate the unchanged production DDS/hash module into the ordinary native host through a bounded C ABI, then obtain native runtime evidence from owned fixtures.

**Architecture:** Keep the existing C socket/HTTP lifecycle. A static C++ bridge owns all JSON, buffers and exceptions; it returns complete JSON into caller-owned buffers and caches startup fixture results. Separate C/C++ compilation links through the bundled SDK runtime, retaining current startup behavior.

**Tech Stack:** C11, C++20, SDK v0.43 release d9c9519116944a7f1c22d262012c53b80b3520b7, pinned LLVM 21.1.8, bundled libc++/libc++abi/libunwind, existing vendored JSON and picosha2, Python unittest and current Playwright fixture.

**Spec:** outputs/2026-09-30-native-core-design.md. Root selects this design/plan and subagent execution under the user's explicit unattended-work authorization; no human artifact review is inferred.

## Global constraints

- Finished runtime operations must execute entirely on the PS5, without PC, phone or AI runtime dependencies.
- Missing target writers or acceptance evidence remain Unsupported and non-installable; every report preserves installation_supported=false.
- No C++ containers, exceptions, game pointers or owned heap allocations cross the C boundary.
- MB_CORE_INPUT_LIMIT_BYTES=8388608; MB_CORE_REPORT_CAPACITY=2048. Preserve the exact signatures, status values and output-length contract in the spec.
- Keep unchanged SDK v0.43 wrapper/runtime, separate C11/C++20 compilation and ordinary owned-process socket-spawn/shutdown behavior. No section GC/custom runtime initialization in this baseline.
- Default loopback binding; explicit-address development profile has a 600-second lifetime. No new privilege/kernel/exploit action, game write/injection, account request or external service call.
- Self-tests use owned built-in fixtures once at startup, with scope owned_builtin_fixtures. Native claims require actual matching native reports; desktop/cross-link evidence is separate.
- Preserve old native-host artifacts. New reviewed outputs belong in outputs/native-core-artifacts; build-time console_executed=false is retained, with later console evidence recorded separately.
- Implementer performs no console contact, staging or commits; root serializes those operations. No implementer subagents.

## Review focus

- Null/zero/undersized/exact-capacity C buffers must not dereference null or expose partial JSON; canaries and actual C linkage cover this.
- Allocation failure or JSON/parser exceptions must return INTERNAL_ERROR without escaping into C or allocating in the catch path; isolated failing-allocator harness covers this.
- Malformed/opaque DDS and size contradictions remain parser facts with unresolved writer/acceptance requirements; actual ABI calls cover this.
- C++ driver/source-identity closure must include linked production/vendor bytes, keeping native and desktop origins separate; builder tests and manifests cover this.
- Self-test failure must leave the owned host responsive with explicit failed/unavailable core status; no remotely repeated tests or install enablement; actual host fixture covers this.

---

### Task 1: Integrate and verify the native C/C++ boundary

**Files:**

- Create outputs/ps5-mod-bridge/native-host/core_bridge.h and core_bridge.cpp.
- Create native-host/tests/test_core.py and minimal actual C/failing-allocator harness sources under native-host/tests.
- Modify native-host/host.c, http.h, http.c, build.py, Makefile, README.md.
- Modify native-host/tests/test_host.py, test_build.py and browser_fixture.cjs build plumbing and focused assertions.
- Create outputs/2026-09-30-native-core-implementation.md; root retains reviewed artifact/evidence docs separately.
- Read unchanged src/dds.cpp, src/hash.cpp, include/modbridge/dds.hpp/core.hpp, third_party JSON/picosha2 headers; do not rewrite them for the native port.

**Interfaces:**

Consumes modbridge::inspect_dds(span,path,total_size), normalize_dds(span,reason), DdsNeutral::lossless_dds(), sha256_bytes(span), existing host_info/http_handle/build profiles and SDK wrappers. Produces the two exact C interfaces in the spec, `host_info.core_report[MB_CORE_REPORT_CAPACITY]`, GET /core and additive /report.core. Existing report phase/schema/version and request gates remain compatible.

- [x] Write ABI contract tests first: actual C caller, output canaries, required length excluding NUL, size-query/too-small/exact-capacity boundary, invalid pointers and the 8 MiB input cap (oversized length must reject before dereferencing a one-byte fixture). Assert parser invalid/unsupported/arbitrary FourCC/total-size contradiction facts remain valid JSON and retain texture.ps5_writer and texture.game_acceptance requirements. Isolated allocation failure returns INTERNAL_ERROR, length zero and empty output; no exception escapes. Capture RED from missing bridge/API or actual-host missing /core behavior before implementation.
- [x] Implement core_bridge.h/cpp exact ABI with noexcept/catch-all and complete owning serialization. Implement the seven named built-in self-test checks from the spec using hand-owned fixtures and independent literal expected layout sizes. TLS value 41 changes to 42 across helper calls then restores 41; repeated self-tests deterministic. Nested exception unwinding destroys the owned guard exactly once. A false check gives failed status; internal exception returns INTERNAL_ERROR.
- [x] Integrate cached startup report in C host; fixed unavailable fallback on ABI failure, service continues. GET /core equals /report.core, root page renders additive object with unchanged script/CSP. Add an isolated test fixture that supplies a failed bridge return, proving host responsiveness and installation_supported=false without any production failure-injection switch. Preserve all fixed-route, Host/framing/deadline/Origin/shutdown/lifetime checks.
- [x] Update separate C/C++ builder for desktop and native profiles, verified clang++ driver and dependency closure/source identity. Run existing dependency verifier. Include every compiled project/vendor input in source_sha256 with stable repository-relative names. Add builder tests for identity changes when a bridge/shared/vendor source changes (temporary source-root fixture), clang++ pin failure and correct profile retention. Preserve ELF table/loader-prefix checks and SDK verification; final link uses unchanged C++ wrapper. Do not adopt --gc-sections here.
- [x] Run focused native-host `make test`, equivalent desktop `build.py --profile desktop_fixture`, unchanged shared `make test-dds`, focused ASan/UBSan actual C/bridge tests, and the actual browser fixture with the linked bridge. Record command, exit code and summarized counts, including any tool prerequisites/failures fixed. Broad test-all only if a shared source or broader product behavior changes.
- [x] Cross-build ordinary loopback and explicit-address development profiles. Inspect imports, retained EH/constructor/runtime roots, entry/load/table bounds; record both source/profile and artifact identities. Compare ordinary runtime assumptions with the written spec, without claiming native execution. Self-review full diff/spec, update implementation report with RED/GREEN/results/limits. Return report path and status; no staging/commits/device contact.
- [x] Root independently reviews production diff/artifact identity, commits exact files, supplies written review package and obtains separate spec/quality review. Resolve actionable findings through scoped regressions and re-review; no unreviewed console payload.
- [x] Root performs narrow reviewed-artifact console checkpoint if reachable: preflight, unique upload, stored-file read-back hash, exact manager launch, matching native /core and /report, owned shutdown and endpoint observation. Record actual outcomes and unresolved imports/TLS/EH/game limits; if target test is unavailable, keep implementation and report that limit.
- [x] Root syncs the reviewed checkpoint commits/evidence into the original codex/ps5-mod-bridge branch while preserving user files.

## Controller decisions

Root selected a caller-owned JSON C interface as a reusable inspection seam rather than a diagnostic-only fixed POD interface. It avoids cross-runtime ownership and makes later native archive callers possible; large inspection JSON still needs a bounded caller buffer/streaming work. Root selected the unchanged full SDK C++ link as baseline rather than immediate section GC; payload/import size is higher, but target validation precedes runtime trimming. These are autonomous controller choices, not human artifact approvals.
