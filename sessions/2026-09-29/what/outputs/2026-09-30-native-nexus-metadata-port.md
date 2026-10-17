# Native Nexus metadata port

The reusable C++ Nexus metadata decoder and its C interface now compile and link for the PS5 target. The decoder is in the product source tree; it has not yet been connected to the native HTTPS client, console UI or account flow, and the target executable was not run.

The decoder handles six record kinds: games, mod lists, one mod, files, OAuth user and API-key user. It checks exact types and positive signed-63-bit identities, constructs source URLs from validated identities, preserves authorship and content facts, and discards unknown provider fields including credentials and download URLs. Unknown adult-content preferences remain `null`. Every report has `scope=nexus_metadata_only` and `installation_supported=false`; parsing a user record establishes no account authority.

The native module also constructs the corresponding six read-only metadata request URLs used by the existing desktop client. It uses fixed HTTPS Nexus API/user origins, bounded validated game-domain segments and exact decimal IDs. It rejects wrong-kind context, path/query/control injection, unknown kinds and out-of-range IDs. Its C interface returns one complete URL within a 256-byte output ceiling and contains allocation failures. This constructs no credential, download link or request and has not been exercised against live Nexus.

The JSON parser bounds input to 8 MiB, nesting to 64 containers, values plus keys to 200,000 and each array to 10,000 items. It rejects duplicate decoded keys, invalid UTF-8, trailing data and disallowed controls, including in otherwise discarded fields. A late invalid item rejects the complete collection; no accepted prefix is published. The C boundary contains exceptions and copies only complete NUL-terminated reports into disjoint caller-owned storage. Its output ceiling is 16 MiB. The input/body ceilings do not establish an exact heap-memory bound for the JSON DOM.

Focused verification observed:

- The metadata tests first failed against an empty-result scaffold; the new routing behavior also failed against an empty-URL scaffold. The final product `make test-nexus` target passed: **21 tests, 0 failures**, with C++ warnings treated as errors.
- The C-interface size-query assertions failed against deliberate error-only stubs. The implemented interface passed **2 host tests**, exercising an actual C caller, exact/short buffers, identities, secret omission, routing and allocation-failure recovery through the actual C++ decoder. The allocation fixture was then extended and passed its focused case with URL construction also failing cleanly and recovering on the next call.
- The C caller, C++ interface and decoder compiled and linked with SDK v0.43 and LLVM 21.1.8 for `x86_64-sie-ps5`. The final ELF is 839,112 bytes. Dependency-file inspection found no headers outside the project, target SDK and pinned compiler resource roots. The ELF declares the SDK's libc, network and kernel-web libraries; those imports were not exercised by this new executable.

The initial target compilation failed because the owned build recipe placed C headers before libc++'s C-header wrappers. Putting the SDK's libc++ include directory first corrected it without editing the SDK or vendor source. The original compiler diagnostics and corrected command receipt are retained in the ignored experiment scope.

Sources: [decoder](ps5-mod-bridge/src/nexus.cpp), [C interface](ps5-mod-bridge/native-host/nexus_bridge.h), [metadata tests](ps5-mod-bridge/tests/nexus_records_tests.cpp) and [C-interface tests](ps5-mod-bridge/native-host/tests/test_nexus_bridge.py). Development evidence is in `.superpowers/native-dependency-spike/native-nexus-records-port-1/`, `native-nexus-bridge-port-1/` and `native-nexus-routing-port-1/`; it is not a reviewed deployable artifact.

This work made no console, FTP, Nexus account, game or system operation. Native transport integration and live authentication remain separate work. A decoded mod/file record neither demonstrates PS5 compatibility nor permits installation.
