# Native status host console checkpoint

The reviewed ordinary-SDK status application executed on the user's PS5 at ps5-host-not-configured.invalid. This is the first native application execution evidence for this project. It verifies this narrow owned-process host route, ordinary imports and endpoint lifecycle; mod installation remains unavailable.

## Reviewed artifact and transfer

Source commits: implementation `70fe22f`, loader-prefix fix `08fdbf7`; independent review and scoped re-review clear. Host verification: 16 focused tests, real Chrome CSP/report/keyboard shutdown/desktop exit; wider 97 C++/96 Python tests and both existing browser suites passed.

Development artifact: 112,344 bytes, SHA-256 `e15b97b8a43154325f180d33354bc0c2ef65f8ab77f6b5f9a26e6794b4aae778`. Source/profile build ID `0f011382a9e320b9eef14af9b45f7506a8b07705f98e4e12a92f333f9fdb0391`. It binds only ps5-host-not-configured.invalid:8085 and has a 600-second monotonic lifetime. Permanent loopback artifact remains separate and was not launched.

Preflight manager port 8084 returned version 0.5.2; port 8085 refused connection. A fresh manager catalog had no matching owned payload group. Uploaded only `ModbridgeNativeHostDev_e15b97b8.elf` through the ordinary raw-body manager upload route, received HTTP 200 `OK`, then read back the exact stored file through existing anonymous FTP. The stored 112,344 bytes matched the reviewed hash at `/data/pldmgr/payloads/ModbridgeNativeHostDev_e15b97b8/ModbridgeNativeHostDev_e15b97b8.elf`.

Used the manager's ordinary launch route for that exact verified path. Its HTTP 200 `OK` is transport evidence only. Separately fetched the new application's complete 736-byte report: schema 1, matching build ID, origin `ps5_sdk_entry`, profile `development_lan`, phase `main_http_ready`, installation_supported=false.

## Native observations and lifecycle

| Imported API | First run | Second run |
|---|---:|---:|
| getpid |221|222|
| getuid |1|1|
| geteuid |0|0|
| sysconf(_SC_PAGESIZE) |16,384|16,384|

These are ordinary imported API results. UID/effective UID values do not grant game or translation capabilities. Page size is now a native observation, separately from linker alignment. Firmware and game report fields remain unavailable/not_queried; 13.60 and Stellar Blade 01.014.001 remain user reports.

Both runs accepted the owned shutdown POST, returned shutdown_requested and stopped their endpoint. A second launch bound the same port and returned a different native PID with the same source identity; its shutdown also closed the endpoint. Payload Manager continued returning version 0.5.2 afterward. This establishes response/endpoint shutdown and repeatable port reuse. No process table/wait result was collected, so process-exit attestation is not claimed solely from endpoint disappearance.

The temporary development server is stopped. Its clearly named payload remains in the manager catalog for review/retesting; no autoload setting or game file was changed. The unchanged SDK runtime initializes its existing loader-provided privileged state before main; the application adds no kernel/privilege/exploit operation.

Raw transfer/report/lifecycle receipts are local development scratch. The build manifests retain `console_executed:false` as their original build-time statement; this separate runtime receipt records the subsequent test. No native DDS conversion, Nexus account login/TLS/download, PS5 browser/controller execution, game acceptance or installation is inferred from this host checkpoint.

An independent read-only evidence review matched the retained ELF/build identity against upload, FTP read-back and both report receipts. It also matched the unexecuted C++ cross-link evidence. Its one wording finding was corrected: console context records `endpoint_listening:false`, separately from `process_exit_attested:false`. Root independently recomputed all 11 retained artifact checksums and matched the two-run context to raw receipts.

## Next native subsystem

Integrate the shared format/preparation core through the SDK's bundled C++ runtime, then native archive/TLS/account/cache services. Keep every unverified writer/runtime requirement Unsupported and non-installable. The runtime app must remain PS5-only; the external test client here was development tooling.
