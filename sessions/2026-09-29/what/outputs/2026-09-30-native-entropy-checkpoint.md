# Native raw entropy checkpoint — September 30, 2026

The ordinary SDK host now collects one cached, metadata-only `sysctl(KERN_ARND)` observation before listening. This is the first OS entropy API gate for eventual official Nexus HTTPS. It does not enable DRBG, TLS, account access, downloads, conversion or installation. `installation_supported` remains false. The reviewed development artifact ran once on the user’s PS5 with an exact 32-byte successful API result, seven passing core checks and owned shutdown; runtime receipts remain separate from local builds.

## Implementation and bounded design

The caller owns a private 32-byte stack buffer and a distinct metadata record. The target path uses the pinned SDK's `{CTL_KERN, KERN_ARND}`, selector length 2, initial output length 32 and no input value. It clears errno immediately before one call, captures return code/errno/in-out length before any later clock operation, and clears the entire 32-byte capacity through retained volatile writes on every returned path. Neither bytes, a pointer, a hash nor an encoding enters the report. This establishes owned-buffer cleanup, without claiming erasure of kernel storage or every compiler temporary.

Classification is deterministic: nonzero return → `api_return`; zero return with nonzero captured errno → `errno_nonzero`; zero length → `no_progress`; 1–31 → `short_result`; above 32 → `oversized_result`; only zero return/zero errno/exact 32 → `passed`/`exact_result`. The errno-zero check is a conservative checkpoint policy, not a claim that Unix APIs define errno on success. Failure length is the captured in/out value, not proof of bytes written. No retry, fallback, repeated HTTP-triggered collection or additional route exists. Desktop builds report unavailable, make zero target API/clock calls and render result/timing fields null.

`/report.observations.entropy` contains exactly status, reason, api, scope, requested_bytes, call_count, return_code, errno, returned_length_bytes, elapsed_ms and hard_deadline_enforced. Scope is `api_result_only`. Checked monotonic nanosecond timestamps are subtracted before flooring to milliseconds; failed, invalid or backward clock readings leave timing unavailable. The status page displays exact response text with `textContent`, preserving full-width integer digits; its CSP script hash was updated.

The raw-only design minimizes new startup dependencies. Its cost is that this synchronous syscall has **no hard deadline**: it precedes the host lifetime clock and can prevent report/shutdown availability if blocked. The existing socket deadline cannot interrupt it. A missing report cannot identify import failure, a blocked API, another startup failure or bind failure. No helper-thread/cancellation guarantee is assumed. The new `sysctl` import is eagerly bound; older native core runs did not test it.

## Local verification and retained candidate

Meaningful failing tests preceded fixes: the actual desktop host lacked the additive entropy record; a 1 ns forward interval incorrectly reported 1 ms and a 1 ns backward interval reported 0 ms; the actual Chrome page rounded wire `18446744073709551615` to `18446744073709552000`. All three regressions are now covered and corrected.

Root's final full native-host suite passed 35 tests with zero failures in 22.265 seconds. The fixture author and fresh independent reviewer passed eight entropy tests under ASan/UBSan. The actual C observer is compiled against a test-only API boundary: error after partial writes, positive/nonzero return, missing/nonzero errno, zero/short/exact/oversized/SIZE_MAX lengths, exact API arguments, one call, full-capacity wipe with intact canaries, post-clock mutation, invalid timing, and the actual HTTP serializer's cached field allowlist. These deterministic nonsecret fixtures are local behavior evidence, not native entropy or quality evidence.

Root's final emitted-code checks passed three tests/twelve artifact subtests across both object and linked native profiles: retained scalar TLS transition/reset, exception guard cleanup/observation, and the single `sysctl` call followed by a fixed 32-byte volatile wipe loop. Chrome passed CSP, exact full-width integer rendering, keyboard shutdown and actual desktop subprocess exit. Its synthetic oversized reply is a UI fixture, not a console observation.

| Profile | Build ID | Artifact SHA-256 | Bytes |
| --- | --- | --- | ---: |
| Development LAN | `799f24791574def26a3a696f9dbb18d5c36ccaeb9bb2115a7453092042d52d96` | `7673fa5c80a997549954cbd1808146fe1ff7a035234a7eb3e1011938c127dd65` | 1,490,712 |
| Loopback | `4f1a38ccba4523fd94ba31af2766e3a7f87b49a9b059c138efef9634126fb222` | `5c52461fa08f2585b9ad752e6b2d135a33e6bdd700c4655914041ebca1d0eb05` | 1,490,712 |

Root recomputed all 15 compiled source/header identities, profile identities and complete artifact digests. Native imports are the earlier 120 plus `sysctl`, with no removal. SDK/runtime/linking remain unchanged; the standard RWE first load and full C++ runtime are retained. [Artifacts and notices](native-entropy-artifacts/ATTRIBUTION.md), [manifests and local checks](native-entropy-artifacts/local-verification.json) remain separate from historical native-core artifacts. Build-time manifests retain `console_executed:false`; later receipts are authoritative for execution.

## Console validation

The [fresh independent review](2026-09-30-native-entropy-review.md) returned ready with no remaining product blockers after the exact-display correction. Root uploaded the unique `ModbridgeNativeEntropyDev_7673fa5c.elf` through the normal manager route. Normal FTP RETR read-back of `/data/pldmgr/payloads/ModbridgeNativeEntropyDev/ModbridgeNativeEntropyDev_7673fa5c.elf` matched all 1,490,712 bytes and the reviewed digest in 0.365 seconds. No existing payload was overwritten and no autoload configuration changed.

A single manager dispatch returned HTTP 200 `OK`. Two initial bounded report reads raced startup and received connection refused; root did not dispatch again. The matching native report then observed PID 300, origin `ps5_sdk_entry`, development profile, phase `main_http_ready` and the exact reviewed build ID. All seven core checks passed; `/version` matched identity and `/core` matched `/report.core`. Repeated complete reports were identical, including the cached entropy record. The manager independently listed the same PID.

| Native API-result observation | Value |
| --- | --- |
| Status/reason | `passed` / `exact_result` |
| Requested / returned length | 32 / 32 bytes |
| Call count | 1 |
| Return code / captured errno | 0 / 0 |
| Floored elapsed observation time | 0 ms |
| Scope / hard deadline | `api_result_only` / false |

Zero elapsed milliseconds is a floored measurement, not an instantaneous-call or deadline claim. This proves one API-return path, not entropy quality, every API behavior, a 64-byte source or DRBG/TLS readiness. No random content was inspected, published, encoded or hashed. Firmware and game fields remain `not_queried`.

Owned shutdown returned HTTP 200; the 8085 endpoint refused connections and the manager’s subsequent process list lacked PID 300. No `waitpid` exit status was observed. Manager version remained 0.5.2 and FTP remained open as `ftpsrv.elf` PID 267, v0.21.1. Root did not start/restart/stop/kill FTP, inject a process, contact Nexus, change account state or modify a game. The development endpoint is stopped and the uniquely named owned payload remains available. The loopback artifact is unexecuted. [Console receipt](native-entropy-artifacts/console-receipt.json) and [actual native report](native-entropy-artifacts/native-report.json) retain the separate execution evidence.

## Next gates and scope

The isolated [bounded Mbed TLS source feasibility](2026-09-30-bounded-entropy-feasibility.md) establishes compilation/linking and deterministic callback control flow with defaults disabled, including a 64-byte source and one-poll default seeding. Its 64-byte target request remains unexecuted; the production observer uses 32. A future shared total syscall budget must cover raw collection and seeding, and effective configuration must apply to rebuilt libraries. A per-callback bound is not a lifetime bound. Controlled trusted-chain success, hostname/untrusted-chain rejection, CA provenance, native resolver/thread behavior and bounded transfers precede official Nexus integration.

The independent [native read-at feasibility](2026-09-30-native-read-at-feasibility.md) and [audit](2026-09-30-native-read-at-review.md) establish local checked 64-bit read behavior and SDK linking, including above 4 GiB. Those throwaway providers are not integrated or console-tested. No game file, engine runtime, target writer or gameplay compatibility follows from these milestones. The full private PS5-only Nexus manager goal remains active; unimplemented or unverified backends remain Unsupported and non-installable.
