# Native SDK host implementation — September 30, 2026

The new `outputs/ps5-mod-bridge/native-host` target implements the written ordinary-SDK application design: a dependency-free C/BSD socket status server with loopback default, a distinct explicit-address development build, bounded HTTP parsing and owned-process shutdown. Installation and game/runtime capability remain unavailable. The existing freestanding diagnostic and its cache are unchanged.

Application code introduces no syscall assembly, custom resolver, kernel operation, privilege patch or process enumeration. Both ELF artifacts use the unchanged SDK v0.43 release runtime (`d9c9519116944a7f1c22d262012c53b80b3520b7`), whose existing startup initializes loader-provided privileged state before `main`. This is stated explicitly rather than described as privilege-free. Native cleanup calls the ordinary imported `_exit` in an owned child, including bind/startup failures; desktop tests return through the subprocess lifecycle.

## Implementation and decisions

`host.c` owns ordinary imports and socket lifecycle; `http.c`/`http.h` own fixed parser/routes, JSON and the embedded metadata UI. `build.py` performs offline verified-cache extraction, ordinary SDK builds, stable source/profile identity and external ELF inspection. The SDK archive and LLVM cache are read only. `.gitignore` excludes the owned build cache. Make targets have no console send/deploy action.

The default is exactly `127.0.0.1:8085`. The distinctly named development ELF is exactly `ps5-host-not-configured.invalid:8085` and expires after 600 monotonic seconds. A build-time development address must be an explicit private unicast IPv4 address; unspecified, loopback, multicast, public, link-local, reserved, IPv6 and hostname inputs fail. Desktop fixture origin is distinct from native `ps5_sdk_entry`, and desktop command arguments permit only loopback port/lifetime overrides.

Every request requires the exact bound authority in `Host`; localhost aliases are deliberately not accepted. Four fixed routes serve metadata only. Header input is limited to 4096 bytes, errors never echo raw peer bytes, duplicated/control framing fails, bodies/Transfer-Encoding/pipelining fail, and there is no filesystem or outbound-network route. Shutdown requires POST, `Content-Length: 0`, `X-Modbridge-Shutdown: 1`, and matching Origin if supplied. No permissive CORS is enabled. The page uses a focusable button, `textContent`, no-store/nosniff and a CSP hash for its embedded script. This is an origin gate, not arbitrary LAN-client authentication.

Ruling: reserve the final 100ms of the absolute two-second connection deadline for a timeout response — an error can be sent while keeping the original deadline, instead of adding a grace period afterward. Lifetime expiry also clamps connection work. Cost if wrong: a client taking over roughly 1.9 seconds to finish headers receives a timeout rather than consuming the complete two seconds.

A real oversized-header test initially exposed a TCP reset after response: the application closed with excess unread bytes already queued. The fix shuts down its write half and performs at most eight nonblocking 512-byte drains of already queued input before closing. It does not wait for future peer input or extend the deadline. Very large malicious leftovers can still cause a transport reset; no protocol error is guaranteed to arrive across a disconnected/broken transport. The application remains bounded and does not die from SIGPIPE; native socket-option behavior remains a target hypothesis.

The report preserves ordinary API boundaries for imported getpid/getuid/geteuid/sysconf results; UID zero is allowed and no capability follows from it. Unavailable page size retains errno separately. Firmware/game values are unqueried and explicitly unavailable. The source/profile `build_id` hashes sorted production source hashes plus fixed SDK/toolchain/profile metadata; external `artifact_sha256` records the final ELF afterward. Neither field impersonates the other.

## Test evidence

The first HTTP test run failed with 12 explicit assertions that the implementation was missing. A separate verified-build test run failed with three assertions that the wrapper was missing. Final commands and observed results:

- `make -C outputs/ps5-mod-bridge/native-host test all development desktop`: 16 tests passed after the scoped verifier fix, then both PS5 ELFs and a desktop executable built successfully with no compiler diagnostics.
- `make -C outputs/ps5-mod-bridge test`: existing project suite passed, 97 C++ tests and 96 Python tests.

The 12 real C-server subprocess tests cover report/version identity, actual process observations, accessible status UI/CSP, fragmented headers, Host/framing/control rejection, wrong route/method, shutdown Origin/custom-header gates, actual subprocess exit status and owned port reuse, incomplete/oversized peer deadlines, abandoned/broken receivers, lifetime expiry and bind failure preserving the original listener. Four builder tests cover ZIP traversal rejection before writes, symlink/duplicate rejection, invalid development addresses and a program table beyond the loader transmission prefix. Host tests never run a PS5 ELF. TDD logs and the upstream sample build fixture are retained in `.superpowers/native-sdk-build-check` as untracked development scratch, outside product artifacts.

## Retained native artifacts

Small usable native artifacts, external manifests, import listings, readelf reports, SDK license and attribution are retained under `outputs/native-host-artifacts`. Each external copied ELF hash was independently checked against its manifest.

| Profile | Source/profile build ID | External ELF SHA-256 | Bytes |
| --- | --- | --- | --- |
| Loopback | `fc98b7cad1fa318553eb51b782fcec9c090d99607b2d508aaf491977ae7ae82d` | `d37ecbf7e4f27aca72e45e0822515ae4adedb66c6c365017379767c373a048f7` | 112,344 |
| Development LAN | `0f011382a9e320b9eef14af9b45f7506a8b07705f98e4e12a92f333f9fdb0391` | `e15b97b8a43154325f180d33354bc0c2ef65f8ab77f6b5f9a26e6794b4aae778` | 112,344 |

Both are FreeBSD ELF64 x86-64 `ET_DYN` PIE with `_start`/entry at virtual offset `0x0`. Normal CRT is linked. Dynamic dependencies are `libkernel_web.sprx`, `libSceLibcInternal.sprx`, and `libSceNet.sprx`. Undefined dynamic imports include the ordinary socket, poll, clock, process/UID/page-size, `_exit`, errno and string/format APIs; complete lists accompany the artifacts. No new application syscall assembly is present.

Real loads are RWE/RW/RW, each aligned to 16 KiB. The unchanged SDK uses an RWE first load; this is not the diagnostic W^X policy. Loopback file-backed load ends are 58,293, 87,352 and 98,332; development ends are 58,325, 87,352 and 98,332. Section table offset is 110,936 with 22 entries, yielding a loader-consumed prefix of 112,344 bytes. The complete 64-byte ELF header, 288-byte program-table end and 112,344-byte section-table end are explicitly verified against both file length and the section-derived loader prefix. Every file-backed load fits the file/prefix, offsets are congruent with virtual addresses, and entry lies in an executable load. Complete numerical layout is in each manifest.

## Remaining review/runtime limits

Task 1 implementation and the scoped verifier fix are ready for the root's independent re-review. No console contact, native execution, upload, launch, staging or commit occurred. SDK 13.60 support and observed manager HTTP operation support the route, but they do not prove this new ELF's target import resolution, socket options, clock behavior or owned exit. A manager transport acknowledgment is not native execution evidence. A later console report must match the reviewed build identity, while exit observation is recorded separately. Account/TLS/download/conversion/install subsystems remain later work.

Root additionally ran `tests/browser_fixture.cjs` with bundled Node/Playwright and installed Google Chrome. It passed: the CSP permitted the exact embedded script, the page fetched/displayed its desktop report, keyboard Tab/Enter activated shutdown, browser-generated POST passed the header/Origin gate, and the owned desktop subprocess exited0. No page error or console contact occurred. This is browser behavior on the development host, not PS5 browser evidence.

## Final identity and CSP verification snapshot

The 353 script bytes served by the compiled desktop fixture hash to `34+FMFdO8GZKHHMLGOuuUMj6WjiwN0QLY71bTIiEYFs=`, exactly matching its response CSP. A local HTTP request also read the desktop report and sent an empty shutdown POST with matching Origin and custom header; it returned `shutdown_requested` and the subprocess exited 0. This verifies served bytes and parser interoperability; browser script execution remains a separate root check. The machine-readable receipt is `outputs/native-host-artifacts/local-verification.json`.

The current production source files independently match both native manifests, and recomputing canonical sorted manifest identity metadata reproduces each build ID. Exact source snapshot:

| Source | SHA-256 |
| --- | --- |
| `Makefile` | `9576ae393b7cdf6e1280a2a71daebe72ddf9be076192e04e040f9c3e3e63a873` |
| `build.py` | `9e0aaa5974549453e9202f3a3253485cef6312bbb7ddddeecd74d2270c2b640e` |
| `host.c` | `0a95c989623732254af49e0b4be7b2906ddd44d618444da5b2adb9808763a2ee` |
| `http.c` | `b55789498aafdec51471659bea18369330cf7fea266f63e3ecb77be99f0def4b` |
| `http.h` | `873a458a661f780864973caaeb8ee349225247de15ab3c2c5670b7217e3ba76c` |

`outputs/native-host-artifacts/checksums.json` records external hashes of both manifest files, both ELFs, inspection reports, attribution, exact upstream GPL text and the local receipt. Manifest hashes are loopback `5ae3690d132df319fc49fe0d614eb2694a16c53fe02a114ba9d163cd429bcd48` and development `f6ac623a687496c92870d730dd7528d6233bae8b115cd4acce60e07cbc9c614a`. SDK GPL text exactly matches the pinned source license bytes (`3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`); its contents differ from the diagnostic cache copy only by that copy’s extra trailing newline. Source and native artifacts are stable for independent review.

## Scoped review correction — loader table bounds

The independent review found that the prior verifier bounded the program-header table against the whole file without requiring it inside the section-derived transmitted prefix. A synthetic valid 256-byte ELF with its 56-byte program table copied to the appended tail reproduced the failure: the new rejection assertion failed because no ValueError was raised. The RED builder run had four tests, three passing and that one failing.

The fix validates the expected 64-byte ELF header size, computes the full program/section table ends, bounds all three extents against the file and then against the unchanged section-derived prefix. It rejects out-of-prefix tables rather than extending the launcher's transmission extent. Manifests now retain each checked extent. The exact reviewer mutation of the previous real loopback ELF (program-table end/file size 112,568; prefix 112,344) independently raises `ELF header or table exceeds loader prefix`. Current valid ELF layout and application C behavior are unchanged.

GREEN: `make -C outputs/ps5-mod-bridge/native-host test all development desktop` exited 0, with all 16 tests passing in 3.590 seconds and all three profiles rebuilt. The broader 97 C++/96 Python result above predates this scoped Python verifier change and was not repeated. Exact RED/GREEN logs remain in `.superpowers/native-sdk-build-check/native-loader-prefix-{red,green}.log`.

Because `build.py` participates in source identity, the source/profile build IDs and ELF hashes were regenerated. The current native values above replace the previously reviewed artifacts; the independent review report retains the earlier hashes as historical review evidence. The new desktop build ID is `a15d87ff6199a892c2c42d9c310d3e80874863306412768f4a4d47cecd5af11a`, executable SHA-256 `67ae35f910da44bcb9ce0afec8834af7679fcb1f11849fca1451d96d5eba226d` (35,312 bytes). Both retained native ELFs, manifests and inspections were refreshed. All 11 external checksums match; production source snapshots and canonical identities match both manifests. The compiled new desktop fixture again served CSP-matching script bytes and accepted an empty same-origin shutdown POST, exiting 0. Browser execution of this new fixture remains a root checkpoint. No SDK/cache mutation, console contact, index change or commit occurred.
