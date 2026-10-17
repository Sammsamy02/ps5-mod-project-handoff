# Native shared-core integration design

## Intent and authority

The finished private Nexus manager must perform account, download, analysis, translation and installation entirely on the PS5. Shared format/engine/runtime implementations serve many games; no mod ID selects a private patch. Missing target writers or acceptance evidence remain Unsupported and non-installable. The computer is development tooling only.

This architectural checkpoint integrates the existing shared DDS/hash code into the already console-tested ordinary SDK C host. It resolves the next dependency in the full app, not the full app itself. The user authorized unattended development, routine reversible decisions and Sol/Luna agents. Root selects this design and its execution method under that authorization; no new artifact is represented as human-reviewed.

## Approach

Use one static C++ module inside the owned application, with a C ABI and caller-owned output buffers. Keep C socket/HTTP code compiled as C; compile the bridge and unchanged production `dds.cpp`/`hash.cpp` as C++20, then link through the unchanged SDK v0.43 C++ wrapper and bundled static runtime. Alternatives are moving the whole host into C++ or rebuilding libc++; neither is needed to integrate the existing code. Dynamic module loading remains outside this checkpoint.

Baseline retains the ordinary wrapper's runtime, CRT, constructors, exception/unwind and emulated-TLS behavior. Do not add section garbage collection or custom C++ initialization here. Later optimization needs an import/map comparison and the same target fixtures. The extra baseline runtime size is the tradeoff for validating the existing route first.

## C interface

Create `native-host/core_bridge.h` and `core_bridge.cpp`. C and C++ can include the header. No C++ containers, exceptions, game pointers or owned heap allocations cross the boundary.

Define statuses `MB_CORE_OK=0`, `MB_CORE_INVALID_ARGUMENT=1`, `MB_CORE_BUFFER_TOO_SMALL=2`, `MB_CORE_INTERNAL_ERROR=3`, `MB_CORE_INPUT_LIMIT=4`; constants `MB_CORE_INPUT_LIMIT_BYTES=8388608`, `MB_CORE_REPORT_CAPACITY=2048`.

Interfaces:

```c
int mb_core_inspect_dds(const unsigned char *bytes, size_t available,
                        uint64_t total_size, char *output, size_t capacity,
                        size_t *length);
int mb_core_selftest(char *output, size_t capacity, size_t *length);
```

Both functions contain every C++ exception and return a status; use `noexcept` on their C++ declarations/definitions. `length` is required. Set it to zero before work, and set `output[0]=0` whenever output is nonnull and capacity is nonzero. A null output is allowed only with zero capacity; a null input is allowed only with zero available bytes. Do not inspect any input when `available > MB_CORE_INPUT_LIMIT_BYTES`; return the input-limit status. Invalid argument/internal failure/input limit leaves length zero and no partial JSON. On valid output size query or insufficient capacity, set length to the complete UTF-8 JSON byte count excluding NUL and return buffer-too-small, leaving output empty. Success requires capacity greater than length, writes exact complete JSON plus NUL, and returns OK. Never write beyond capacity. Parser outcomes for malformed/unsupported/truncated DDS are valid JSON results, not ABI failures.

Inspection calls the actual shared `inspect_dds` with path `native_input.dds`, available span and true total_size; it preserves every unresolved requirement and fact. Serialize through the existing JSON implementation. Owning the result before copying allows deterministic buffer behavior. Input/layout bounds come from the existing parser; large output may require a caller's larger bounded buffer. This interface provides reusable inspection, not a PS5 texture writer.

## Native startup evidence

`mb_core_selftest` executes only hand-owned built-in fixtures and returns a compact JSON object with schema_version 1, core_version, status (`passed` or `failed`), scope `owned_builtin_fixtures`, and installation_supported=false. A `checks` object reports booleans for:

- `sha256_known_vector`: SHA-256 of empty input equals `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.
- `allocator_roundtrip`: an owned nonempty vector/string allocation preserves known bytes and releases normally.
- `exception_raii`: nested throw/catch of an owned runtime_error destroys an owned guard exactly once during unwinding, contains the exception, and records the expected fixed message. Catch-all covers unexpected exceptions at the ABI boundary.
- `thread_local_single_thread`: an explicitly declared scalar `thread_local` starts at 41; one helper call changes it to 42 and a second helper call observes 42; restore 41 before returning so repeated self-tests stay deterministic. This observes only this single-thread TLS path; no worker-thread claim follows.
- `dds_legacy_roundtrip`: 5x7 DXT1, 32 block bytes and a three-byte opaque trailer; parsed block extent and trailer match independent literal sizes/offsets, neutral reconstruction equals every input byte.
- `dds_cube_mip_roundtrip`: DX10 BC7 UNORM (DXGI 98), 4x4, two mips, one cube (six faces), 12 subresources of 16 bytes, 148-byte header and two-byte trailer; independent total 342 bytes and expected offsets, lossless reconstruction.
- `dds_truncation_detected`: one byte removed from required legacy block data yields a noneligible incomplete/invalid layout and no neutral object; complete header inspection must retain unresolved writer and game acceptance requirements.

Fixture construction belongs to the bridge; it must not include the test fixture header or derive expected layout sizes by calling the parser. A failed check yields status failed, keeping individual booleans. An unexpected exception returns INTERNAL_ERROR with empty output; the C host supplies a fixed unavailable fallback object. Normal malformed-input tests use the public inspection function as well.

The C host computes this report once before listening, stores it in `host_info.core_report[MB_CORE_REPORT_CAPACITY]`, and continues bounded HTTP service even if the bridge reports failure. `GET /core` returns that stored JSON. Existing `/report` adds the same object under `core`; schema/version/current phase stay compatible. The status page's existing report fetch renders it; no new script or CSP relaxation is needed. Unknown firmware/game fields and installation_supported=false remain. Self-tests are not remotely rerun and no HTTP body/file service is added.

## Build and artifact provenance

Expand source/profile identity to include every bridge file, compiled production source and transitive project/vendor header used by this checkpoint: core_bridge.cpp/h, host.c/http.c/http.h/build.py/Makefile, shared dds.cpp/hash.cpp, dds.hpp/core.hpp, JSON header and picosha2 header. Use stable repository-relative keys. Verify bundled project dependencies through the existing bootstrap verifier before compiling. Verify local clang++ driver bytes against the already pinned clang binary; preserve SDK archive/tool verification. The SDK wrapper and source are unchanged. Build C objects with the C wrapper, C++ objects with its C++ wrapper, and final link with the C++ wrapper; desktop fixture follows equivalent separate C/C++ compilation with system tools.

Retain fresh profiles and ELF imports/layout/manifests in a separate `outputs/native-core-artifacts` directory after review; preserve old native-host artifacts as historical evidence. Build manifests retain console_executed=false at build time. Any later target execution has separate runtime receipts. Default production binds loopback; the explicit-address LAN development profile keeps its 600-second lifetime and owned-process launch/shutdown contract. No custom kernel/exploit/privilege action, game injection, game write, account request or external service call is added.

## Verification and acceptance

Before implementation, meaningful RED tests cover the C-callable ABI and new actual-host core routes. Compile an actual C caller and link to production C++ sources. Verify invalid/null arguments, zero-capacity size query, exact capacity boundary, output canaries, input cap, empty/malformed/arbitrary FourCC JSON, parser size contradiction and repeated deterministic result. Use an isolated test-only allocation-failure harness to make the bridge's catch-all return INTERNAL_ERROR without an escaping exception or partial output; do not add a production failure-injection API. Self-test report booleans and unresolved writer/acceptance requirements are asserted independently.

Update existing real host tests and browser fixture build plumbing to include the bridge. Verify `/core` equals `/report.core`, all existing request/deadline/shutdown checks remain, and the unchanged CSP script renders the core object. Run focused host/bridge/builder tests, one appropriate shared DDS regression checkpoint and focused ASan/UBSan for the bridge. Cross-build both native profiles, inspect ordinary imports, load/table bounds and runtime roots, then obtain independent spec/code review before any console launch.

Root may perform the already authorized narrow development checkpoint: verify current manager responsiveness and unused owned port, upload a distinctly named reviewed artifact, read it back and hash-match, launch that exact path through Payload Manager, fetch the application's matching build/native/core report, perform owned shutdown, and record endpoint closure separately from process-exit attestation. Only actual matching native reports can prove this module's console execution; no successful host or C++ fixture grants installation/game compatibility. If the console is unavailable, keep the verified implementation and report the unexecuted checkpoint; continue independent dependency work.
