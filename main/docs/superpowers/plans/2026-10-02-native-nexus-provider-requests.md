# Native Nexus Provider Requests Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development for the independent transport, typed client, and fixture tasks below. The user has explicitly requested autonomous work while asleep and parallel agents; no new design approval is required for this local implementation.

**Goal:** Link a callable authenticated Nexus metadata client into the existing native payload, sharing its actual HTTPS runtime with archive downloads.

**Architecture:** The private C transport performs one bounded HTTPS transfer and invokes a complete-body consumer while the exclusive curl/PSA owner remains active. A C++ client generates fixed provider URLs and uses the existing owning metadata decoder. Credentials are borrowed for one call; owned header copies and raw response storage are erased before return. This implements a callable client, not an account session or HTTP dispatcher.

**Tech Stack:** C11, C++20, retained curl 8.18/MbedTLS profile, existing PS5 SDK build, Python owned HTTPS fixtures.

**Spec:** Design and constraints in this document; existing `native-host/nexus_bridge.h` and `download/curl_download.h` define the retained decoder and archive contracts.

## Global Constraints

- Keep archive ABI version 1 and existing status values 0..12; append consumer refusal as value 13.
- Fixed HTTPS provider origins: `api.nexusmods.com:443`, and Bearer-only `users.nexusmods.com/oauth/userinfo:443`. No arbitrary caller URL, redirect, proxy, cookie, or credential forwarding to archive servers.
- Credentials: 1..16384 ASCII bytes 33..126; raw body: 1..8388608 bytes; normalized report: 0..16777216 bytes. Validate disjoint storage before erasing anything.
- Preserve TLS peer/hostname checks, explicit CA bounds, transport framing/cancellation/deadlines, and lifetime owner counters across metadata and archive jobs.
- No console access, native execution, production account requests, real credential reads, game writes, or UI polish. Preserve paused title/catalog work.
- A decoded metadata report establishes no download entitlement, installation authority, or UE4SS/DLL compatibility. Current service capabilities remain unavailable until dispatcher/account/jobs exist.

## Review Focus

- Aliased credential/raw/report/result spans must fail before mutation (typed-client fixture).
- A timeout or late owner failure must discard an already decoded report (transport/client private controls).
- Redirects and non-provider routes must never receive an account header (TLS fixture request ledger and redirect trap).
- A bad final metadata record must produce one whole error envelope, without a positive record prefix (decoder fixture).
- The shared transport refactor must retain real archive framing, hash/readback, cancellation and post-failure reuse behavior (existing archive regression batch).

### Task 1: Shared provider transfer

**Files:** `download/curl_download.c/.h`, new `download/runtime.h`, new `download/provider_transfer.h`.

**Interfaces:** `mb_curl_provider_transfer_v1(const mb_curl_provider_transfer_params_v1 *, mb_curl_provider_transfer_result_v1 *)`; retain `mb_curl_download_v1` and runtime APIs.

- [x] Observe an actual caller link failure for the missing provider symbol.
- [x] Reuse the existing network attempt and owner lifetime; add bounded memory delivery and a complete-body callback before final cleanup/owner guard.
- [x] Erase owned auth-header copies and full valid body capacity on return; refuse unsupported URL/auth combinations before any network activity.
- [x] Run actual TLS body/expiry RED and GREEN and the existing 19-job archive regression batch. The first archive run exposed an unsupported explicit NETRC option; the pinned profile compiles NETRC out. Removing that option corrected setup, with every other setup failure still fatal.

### Task 2: Typed owning metadata client

**Files:** new `native-host/nexus_client.h/.cpp`; reuse `nexus_bridge.cpp/.h` and `src/nexus.cpp`.

**Interfaces:** `mb_nexus_client_request_v1(const mb_nexus_client_params_v1 *, mb_nexus_client_result_v1 *)`; six existing `mb_nexus_kind` values. API key=1, Bearer=2; statuses OK=0, BAD_ARGUMENT=1, TRANSPORT_FAILED=2, OUTPUT_TOO_SMALL=3, DECODE_FAILED=4.

- [x] Observe a missing-client link failure using the published public header.
- [x] Generate fixed URLs using `mb_nexus_metadata_url`; decode with `modbridge::decode_nexus_metadata` inside the transport consumer, with a no-throw C boundary.
- [x] Publish complete reports only after final transport success. Preserve required output length only for a final consumer short-buffer refusal; clear report capacity after any late fault.
- [x] Verify all six routes, mixed auth modes, malformed whole envelopes, short outputs, invalid aliases and callback failure using owned fixtures/private controls.

### Task 3: Reproducible local HTTPS verification

**Files:** new `native-host/tests/nexus_client_verify.py`, `nexus_client_driver.c`, and private socket-routing fixture shim if needed.

- [x] Attempt owned `127.0.0.1:443` bind; retain its PermissionError without privilege escalation.
- [x] Use a private verifier-only already-connected socket shim to an owned unprivileged loopback listener. Keep original production URLs, resolve entries, TLS hostnames, CA validation and origin checks; label this routing limitation in the receipt.
- [x] Record exact typed request headers/routes without secrets; reject invalid auth/context before HTTP. Pure private ABI controls cover alias rejection. The shared archive batch covers redirects and the trap's zero connections.
- [x] Verify metadata-specific body caps, HTTP errors, whole decoder error envelopes, short outputs and mixed metadata/archive owner lifetime. Final root replay: 14 request cases, 16 owner windows, 116 real host entropy callbacks, no terminal failure. Reuse the shared archive batch's TLS/framing/gzip/cancellation coverage rather than duplicate every negative case. Named-curve fixture CA/leaf encoding is required by the retained Mbed profile; preserve the initial explicit-parameter fixture failure.
- [x] Bind reused C++ objects to their complete current include closures as well as source/object hashes. Observe an owned header-only mutation accepted before the verifier correction and rejected afterward. Correct the verifier CA reader to close its descriptor after short reads; independent read-only review found no blocker, and the final HTTPS suite passed after both test-only corrections.

### Task 4: Actual application composition

**Files:** `native-host/build.py`, `tests/test_build.py`, `native-host/README.md`, `download/README.md`.

- [x] Add a failing composition test requiring client, bridge, and shared decoder objects only in the selected download build.
- [x] Add all new inputs to build identity and retain the callable Nexus symbol under native section collection.
- [x] Run the composition test; then build actual desktop and PS5 ELF artifacts in fresh private output roots. Inspect native symbols and per-TU include closure; retain unchanged capability claims. Final native composition: 13 objects, 6,206 include entries, zero host headers. Actual host startup/lifecycle: nine checks passed.
- [x] Review the final diff, run focused checks, and commit only completed provider/client/build/fixture files. Independent transport/client and final verifier/build reviews found no blockers. Eleven builder tests passed; current product sources and both artifact hashes still match the final build manifests. Only the completed plan, transport/client, builder, documentation and fixture files are selected; paused title/catalog work and user files are excluded.

Final receipts: metadata HTTPS replay `nexus-client-https-fixture-1/green-b9mtizmt/receipt.json`; archive `native-nexus-provider-requests-1/archive-regression-2/summary.json`; lifecycle `native-nexus-provider-requests-1/startup-1/summary.json`; native build `native-nexus-provider-requests-1/app-native-2/loopback-download/manifest.json`. Paths are relative to `.superpowers/native-dependency-spike/`. Owned header-only object-reuse RED/GREEN is retained under `native-nexus-provider-requests-1/reuse-header-control/`.

The full user goal remains active after this plan. Account login/session storage, native HTTP jobs and dependency resolution, live game bindings for UE4SS, arbitrary DLL translation, and console validation are later deliverables.
