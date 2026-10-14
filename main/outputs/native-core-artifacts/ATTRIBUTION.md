# Native core artifact attribution

These private ELF artifacts contain the unchanged ps5-payload-dev SDK v0.43 runtime by John Törnblom and contributors, GPL-3.0-or-later. SDK-GPL-3.0.txt retains the previously verified exact pinned SDK license bytes. Exact SDK source: https://github.com/ps5-payload-dev/sdk/tree/d9c9519116944a7f1c22d262012c53b80b3520b7 . Archive/tool/source identities are in each manifest.

The SDK-bundled C++ headers identify libc++ 18.1.0 (`_LIBCPP_VERSION 180100`), while the development compiler is LLVM 21.1.8. LLVM-libcxx-LICENSE.txt, LLVM-libcxxabi-LICENSE.txt and LLVM-libunwind-LICENSE.txt notices are retained here as their LLVM 18.1.0 upstream reference texts; this header version does not independently attest every bundled archive's build provenance. They use Apache-2.0 with LLVM exceptions and retained legacy notices. No runtime source or archive is replaced.

The production DDS/hash implementation links the unchanged bundled nlohmann JSON and PicoSHA2 headers; their exact vendored MIT license notices are included. Source and offline build instructions: ../ps5-mod-bridge/native-host, shared source: ../ps5-mod-bridge/src. All 13 production/vendor source hashes are recorded. Complete private application source remains in this repository; nothing was published or pushed.

These artifacts are for an owned socket-spawned child only. Development binds ps5-host-not-configured.invalid:8085 for 600 seconds; production binds 127.0.0.1:8085. Build manifests preserve console_executed=false; later target receipts, if any, are separate. Every installation-supported flag remains false. Historical native-host artifacts remain unchanged.
