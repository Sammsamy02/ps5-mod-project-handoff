# Native shared-core console checkpoint — September 30, 2026

The reviewed ordinary-SDK C/C++ application ran twice on the user's PS5. Both actual `/report` responses match development build `c088db7ccd5bbcf16276fd25c74912f62230e06c86015549fca206fda3ed6462`, native origin `ps5_sdk_entry`, development profile and `main_http_ready`. All seven owned built-in core checks passed on both launches; `/core` exactly matches `/report.core`. This advances shared DDS/hash code from cross-link evidence to narrow console runtime evidence. It does not enable conversion, installation or game acceptance.

Implementation commit `5d35ed6`, corrected runtime-observability commit `a34fef5`. Independent review found compiler-elided TLS transition and guard-counter observations; the original implementer fixed both, emitted-code regressions and a cleanup-mismatch fixture passed, and scoped re-review returned spec PASS/quality PASS. Root independently ran six actual C ABI/core tests and two optimized native inspection tests (eight artifact subtests), checked all 13 source identities and profile/build/ELF snapshots. A later fresh full native-host integration run passed26 tests with zero failures in21.765 seconds. The implementation report retains the earlier 26 full host/core/builder tests, six sanitizer tests and real browser fixture results.

The retained development ELF is 1,490,568 bytes, SHA-256 `39ca575e447399aeb3cbd4e5d831bfbd77177527414cab064601e771a5de07d3`. Before launch, ordinary FTP read-back of the manager's stored file matched every byte count and hash (0.363 seconds). Unique payload: `/data/pldmgr/payloads/ModbridgeNativeCoreDev/ModbridgeNativeCoreDev_39ca575e.elf`. Manager chose the observed group name; no existing payload was overwritten and no autoload configuration changed. The loopback artifact remains unexecuted. Build-time manifests retain `console_executed=false`; separate [runtime receipts](native-core-artifacts/console-receipt.json) establish the later development execution.

| Observation | First run | Second run |
| --- | --- | --- |
| Native PID | 290 | 291 |
| UID / effective UID | 1 / 0 | 1 / 0 |
| Page size | 16,384 | 16,384 |
| Seven core checks | All true | All true |
| Owned shutdown | HTTP 200, shutdown_requested | HTTP 200, shutdown_requested |
| Endpoint after shutdown | Connection refused | Connection refused |
| Manager process listing after shutdown | PID absent | PID absent |

Checks are `allocator_roundtrip`, `exception_raii`, `thread_local_single_thread`, `sha256_known_vector`, `dds_legacy_roundtrip`, `dds_cube_mip_roundtrip`, and `dds_truncation_detected`. They exercise actual retained volatile TLS 41→42→41 helper accesses, nested throw/rethrow and exactly-once guard cleanup, production DDS parser/neutral reconstruction with independently sized owned fixtures, and incomplete payload detection. These prove the used paths under this SDK/loader/application profile. They do not prove worker-thread TLS, every retained runtime import's behavior, dynamic constructors or every DDS topology on-console.

The second immediate report request raced startup and received connection refused after dispatch. Root did not dispatch another child: a later bounded report read found PID 291, all seven checks passed, and the manager independently listed that matching PID as `payload.elf`. The changed PID proves a fresh application instance and port reuse. Both shutdowns used the exact owned Host/Origin, Content-Length 0 and X-Modbridge-Shutdown 1 contract. The manager's normal process enumeration shows the PIDs absent afterward; no `waitpid`/exit-code attestation is claimed.

After the final shutdown, manager `/version` remained HTTP 200/0.5.2 and FTP remained open with `ftpsrv.elf` PID 267, version v0.21.1. The owned development endpoint is stopped; its clearly named payload remains available. Root did not start, restart, stop or kill FTP, inject a process, modify a game, obtain keys, contact Nexus or change account state. Development tools sent and observed the checkpoint; the payload's fixture work itself ran entirely on the PS5.

Firmware/game fields stay `not_queried`; reported firmware 13.60 and autoloader/menu versions are not running identity attestation. Both top-level and core `installation_supported` remain false. Native HTTPS/archive/title services, official Nexus sign-in/downloads, target writers, engine/runtime ports, controller product UI and representative gameplay validation remain unfinished. Finished-app operations must remain PS5-only.
