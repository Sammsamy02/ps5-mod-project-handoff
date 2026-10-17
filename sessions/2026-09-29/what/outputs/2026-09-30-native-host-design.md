# Ordinary SDK native host design

## Intent and decision

The finished private Nexus manager must execute on the PS5 alone. Its native application bootstrap should therefore use the documented community SDK application runtime, rather than wait for a custom resolver in the intentionally freestanding diagnostic. The independent native SDK audit and successful upstream sample cross-link establish a concrete build route; hardware execution remains a separate test.

This is an architectural native application target under the user's unattended authorization. Root reviews this written design/plan and delegates implementation/review without requesting another routine approval. It does not claim human review of these newly written artifacts.

Choose an ordinary C `main`, unchanged SDK v0.43 runtime and a small dependency-free BSD-socket HTTP status server. Importing microhttpd as Payload Manager does is a reasonable later option, but is unnecessary for three fixed metadata routes and would add a cross-built dependency to this checkpoint. Extending the custom syscall diagnostic adds an unresolved ABI while supplying no app host. Keep that diagnostic intact.

The SDK already initializes loader-supplied privileged state. This application does not add kernel operations, exploitation, new privilege changes, arbitrary process control or SDK patches. Do not apply the freestanding diagnostic's no-CRT/no-import/W^X policy to the ordinary SDK artifact; record its real load flags.

## Boundary and files

New standalone directory `outputs/ps5-mod-bridge/native-host` owns `host.c`, `http.c`, `http.h`, `Makefile`, pinned `build.py`, host tests and README. It uses the already verified SDK release archive and LLVM tools in native-probe's development cache. Extract unchanged SDK into the native-host ignored build directory with validated archive members; do not globally install tools. Record v0.43 release `d9c9519116944a7f1c22d262012c53b80b3520b7`, archive SHA `a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb`, LLVM 21.1.8 pins and source/artifact hashes. No ELF dispatch/deploy target in the build scripts.

Separate C request/response logic from socket lifecycle so host tests exercise the same routes. App version is `0.1.0`. A deterministic build_id is SHA-256 of sorted source bytes plus fixed SDK/toolchain/profile metadata; it does not pretend to be the artifact's own hash. The artifact SHA is recorded externally after linking. Do not embed environment-specific absolute build paths or secrets into public reports.

Default target binds IPv4 `127.0.0.1:8085`; do not conflict with Payload Manager 8084. A distinctly named development target may bind **only the explicit configured PS5 LAN address** for bounded external test (currently ps5-host-not-configured.invalid:8085), report `development_lan`, and stop automatically after 600 monotonic seconds. It is a development artifact, not a permanent runtime requirement. Desktop test binary accepts an explicit loopback port and lifetime; it cannot impersonate the PS5 report origin. No address-general or all-interface bind option is needed.

## Fixed HTTP contract

One request per connection; always close after response. Header bound 4096 bytes and absolute accepted-connection deadline two seconds. Listener polls so lifecycle deadlines/shutdown remain actionable. Use checked short read/write handling, EINTR/EAGAIN support within deadline, and platform-supported suppression of SIGPIPE. No request body, chunked framing, pipelining, filesystem serving, dynamic script execution or outbound network in this checkpoint.

- `GET /` serves a compact embedded TV-readable status page with title, build/runtime status and a button that performs the owned shutdown request. Keyboard/controller browser input should reach the button. The page obtains report data from the same server.
- `GET /version` returns JSON version/build_id/profile/origin.
- `GET /report` returns schema1, the same identities, `phase: main_http_ready`, ordinary imported getpid/getuid/geteuid/sysconf(_SC_PAGESIZE) results and explicit `installation_supported:false`. UID zero and page size are observations, not capability evidence. Firmware/game information is unavailable unless independently observed; do not fill it from the user's report or build constants. Desktop origin is `desktop_fixture`, target origin `ps5_sdk_entry`.
- `POST /shutdown` requires `Content-Length: 0`, expected Host, and custom header `X-Modbridge-Shutdown: 1`. Reject conflicting/duplicate framing or Host fields, Transfer-Encoding, nonzero body, malformed lines, oversized headers, wrong method/target. If Origin is present it must exactly match the bound origin; allow absent Origin for direct developer test. No permissive CORS. This gate prevents ordinary cross-origin browser requests, not arbitrary LAN clients.
- Return correct bounded JSON errors and HTTP status for malformed/unsupported requests. Error responses never echo raw peer input. Routes exact; no query decoding or path filesystem access.

For loopback target accept the exact `127.0.0.1:8085` authority (and `localhost:8085` if deliberately documented); development only its explicit IPv4 authority. Desktop tests supply loopback actual port. Host validation applies to every route. Set no-store, nosniff and a restrictive CSP compatible with the embedded status page.

## Owned lifecycle

This artifact is **owned-process socket-spawn only**, as Payload Manager's exact launcher source uses. It must never be injected into a game/shared process. Do not infer ownership from pointers or PID. Bind failure closes this application's resources and returns a distinct small failure status; it never kills the existing listener/process. Once a shutdown response is sent (or reaches its deadline), close connection/listener and call normal imported `_exit(0)` on the PS5; desktop build returns normally. Do not fall into the SDK CRT's context-detection termination heuristic. Lifetime expiry follows the same owned cleanup. No background worker or persistent file mutation is needed.

## Test and evidence

TDD host tests launch the actual desktop build on loopback and exercise fixed routes, identity/origin, malformed/framing/Host/Origin gates, fragmented headers, short/incomplete peers, deadline behavior, shutdown and port reuse. Add pure parser edge tests if needed. Report process exit from the subprocess handle separately from endpoint disappearance. Host tests never execute PS5 ELF.

Cross-link with unchanged SDK wrapper. Static report records architecture/type/entry, imports, actual load flags/alignment, section-derived loader send extent, program-header and PT_LOAD file bounds, source/build/artifact hashes. The payload-manager send prefix must contain everything the loader needs. Existing diagnostic verifier is not applicable to normal CRT/imports.

A later console test uses the manager's public upload/owned launch route for our distinctly named artifact only, preserving existing payload/config/game files. Verify the exact uploaded bytes, fetch the new app's matching build report, capture imported observations, then shut down and verify a subsequent launch can reuse its port. HTTP response and disappearance are limited lifecycle evidence; do not claim full process-exit attestation from them alone. No console launch is performed until host tests, cross-link and independent review succeed.

Success at this stage is a native status host and a testable console launch artifact. Account login, Nexus TLS/downloads, archive conversion and game installation remain following native subsystems, with Unsupported gates in place.
