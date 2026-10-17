# Native Nexus Download Resolution Implementation Plan

> **For agentic workers:** Use parallel agents for the independent provider/core, resolver and owned HTTPS fixture tasks. The user explicitly authorized autonomous development while asleep and a lighter local experiment process; do not request another design approval. Root owns application composition and the final commit.

**Goal:** Resolve a selected Nexus file through live account and mod checks, then use its provider-issued URI with the existing credential-free downloader.

**Architecture:** A no-throw C ABI sequences the existing typed user/mod requests and one new bounded download-link request on the current exclusive HTTPS runtime. It accepts borrowed credentials and an optional structured website NXM grant; it never trusts a caller premium flag. URI storage remains private and is published only after all records, transport cleanup, total deadline and grant expiry checks succeed. The ordinary archive downloader remains credential-free. The resulting callable operation is linked into the actual PS5 app; HTTP job dispatch and official login acquisition remain later work.

**Tech Stack:** C11/C++20, existing Nexus owning decoder, pinned curl 8.18/MbedTLS runtime, Python owned TLS fixtures.

**Spec:** This document, existing `companion/modbridge_nexus/client.py:download_links`, `downloads.py:NXMAuthorization`, and native client/transport contracts.

## Global Constraints

- No console contact, native execution, live account API requests, production credential reads, or game/system writes while the user sleeps. Preserve paused title/catalog work, the root Makefile diff and user files.
- Do not retry/reroute the specifically stopped native archive-placement caller, game-memory read or curl 8.22 upgrade. This operation only resolves a provider URI; test downloads use owned development quarantine files.
- Domains follow `[a-z0-9][a-z0-9_-]{0,79}`. Mod/file/user IDs and NXM expiry are positive signed-63-bit integers. Credentials are borrowed ASCII 33..126, 1..16384 bytes; NXM keys use the same character range, 1..1024 bytes.
- The grant carries domain, mod/file/user IDs, key and expiry. Every source identity must match the selected file; its user must match the freshly authenticated profile, and expiry must remain in the future before link fetch and final publication.
- Live premium identity or a valid website grant is required. Mod visibility requires `available=true`, `status=published`, and a boolean adult marker; adult content additionally requires the live profile's explicit true preference. Unknown fields/preferences are restrictive.
- Fixed GET link route: `https://api.nexusmods.com/v1/games/{domain}/mods/{mod}/files/{file}/download_link`. Only this exact canonical path may carry generated `?key={percent-encoded-key}&expires={positive-integer}`; reject other authenticated queries, ordering, duplicate/extra fields and malformed encodings. User-info remains Bearer-only with no query.
- Link response is a complete array of 1..32 `{URI,name,short_name}` records. Validate every record before selecting the first URI. URI must satisfy the native producer's HTTPS/443, no-userinfo/fragment/backslash, ASCII and fewer-than-8192-byte contract. No credential headers accompany its archive request.
- Raw body 1..8 MiB, metadata scratch 1..16 MiB, URI output 1..8192 bytes. Validate all specified input/output spans and aliases before mutation; wipe full valid raw/report scratch capacities and all failed URI capacity. No URLs, grants or credentials in scalar receipts/logs.
- One total monotonic deadline, 1..300000 ms, covers the whole resolution. Per-call deadlines use remaining time; cancellation and exceptions fail closed. Neither resolution nor download proves preparation, installation or engine execution.

## Review Focus

- Valid first mirror followed by an invalid record must publish no URI.
- Account, source or expiry mismatch must never reach the link endpoint.
- An expired total deadline or grant after successful body decoding must revoke URI output.
- Nonpremium, hidden or unknown-adult records must remain non-downloadable.
- Existing metadata query refusal, credential-free archive behavior and full buffer wiping must survive the narrow exception.

### Task 1: Typed URL and complete link selection

**Files:** `include/modbridge/nexus.hpp`, `src/nexus.cpp`, `native-host/download/curl_download.c`; focused controls in private evidence.

**Interfaces:**

```cpp
bool nexus_download_link_url(std::string_view domain, std::uint64_t mod_id,
    std::uint64_t file_id, std::string_view nxm_key, std::uint64_t expires,
    std::span<char> output, std::size_t& written) noexcept;
bool nexus_select_download_uri(std::span<const std::uint8_t> body,
    std::span<char> output, std::size_t& written);
```

The selector returns true and the URI byte count (excluding NUL) only after all mirrors validate. A fully valid response with short output returns false and its required byte count without writing a prefix; malformed/resource failures return false and zero. The resolver's public failure always revokes that count. A provider-issued URI can itself contain signed query credentials; only the downstream archive request's account authentication headers are absent.

- [x] Observe genuine missing-symbol/query-refusal RED controls.
- [x] Implement bounded canonical URL writing without retaining key copies; select a URI only after all mirrors validate using the existing bounded JSON parse gate. Scrub owned parsed URI strings before release where accessible.
- [x] Admit only the exact canonical download-link query in the private provider transport; preserve all other origin/auth/transport behavior.
- [x] Run focused malformed/query/multi-record controls and retain results.

### Task 2: Live account-bound resolver

**Files:** new `native-host/nexus_download.h/.cpp`, focused private resolver boundary controls.

**Interfaces:** `int mb_nexus_resolve_download_v1(const mb_nexus_download_params_v1 *, mb_nexus_download_result_v1 *) noexcept`. Public header is C-only; ABI=1. Params carry credentials/auth mode, selected source, optional typed grant, CA/resolve mappings for both provider origins, total/connect timeout, cancellation, raw/report scratch and private URI storage. Result carries scalar status/stage/profile/source facts and URI length only. No caller premium/visibility input.

- [x] Publish exact C contract and observe missing implementation RED.
- [x] Implement live user → entitlement/grant → visible mod → complete link response. Keep URI inside the final transport consumer and revoke it on any later clock/owner/expiry failure.
- [x] Verify alias-before-mutation, wrong account/source, unknown adult, cancellation/exception, short URI storage and final-revocation controls.

### Task 3: Actual HTTPS and app composition

**Files:** new `native-host/tests/nexus_download_driver.c`, `nexus_download_verify.py`; modify `native-host/build.py`, `tests/test_build.py`, native/download READMEs and the existing verifier's app-profile selection only as needed.

- [x] Require resolver object/header identity and retained native resolver symbol only in the actual download composition; keep legacy composition and HTTP service capability claims unchanged.
- [x] Build fresh actual host and PS5 ELF compositions, then exercise premium and website-grant resolution over owned TLS with fake credentials. Use the selected URI for a real credential-free sink transfer and byte/readback comparison in a private fixture directory.
- [x] Test nonpremium without grant, mismatched/expired grant, hidden/unknown-adult mod, invalid final mirror, malformed/non-200 response, total timeout and healthy reuse after failures. Reuse transport's existing general framing/CA/redirect/cancel checks; do not duplicate every archive test.
- [x] Independent focused review, root replay, manifest/source/hash checks, selective commit and saved next action. Keep the full goal active: official login, native queue/HTTP dispatch, dependency acquisition, installation and UE4SS/DLL bindings remain unfinished.

Official primary references: [download route](https://github.com/Nexus-Mods/node-nexus-api/blob/master/src/Nexus.ts#L786), [response schema](https://github.com/Nexus-Mods/node-nexus-api/blob/master/src/types.ts#L381). A later prerequisite resolver can use the separate official GraphQL requirement endpoint; this GET operation does not claim to implement it.

## Verified checkpoint

The root replay passed 25 owned TLS requests and 11 assertions, including
resolved-URI archive sink/readback. All nine refusal return codes now match
expected nonzero statuses; a synthetic mismatched return/status control fails.
The actual host and PS5 compositions build; root verifies artifact, all source
and object hashes, full include closures and retained callable symbols. Native
composition: 14 objects, 7,108 include entries, no host headers. Existing
metadata TLS (14 requests), startup (nine checks) and builder (11 checks)
regressions pass. No console/native execution or live Nexus account was used.

Independent scoped reviews covered core/transport, resolver and fixture/build
composition. Cleanup-budget, final wall-check budget and refusal return-code
findings were fixed with focused retained controls. The full goal remains
active. The next bounded increment is official native login/account context
and its concrete app routes, then authorized download/job and prerequisite
integration. Neither this checkpoint nor the earlier asset trials establish
UE4SS/DLL execution or universal game compatibility.
