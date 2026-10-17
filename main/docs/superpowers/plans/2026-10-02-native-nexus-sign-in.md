# Native Nexus Sign-in Implementation Plan

> **For agentic workers:** Use Superpowers subagent-driven development for the separate token, account and owned-fixture components. The user explicitly authorized autonomous development while asleep and focused local experiments. Root owns HTTP/configuration/build integration and the selective commit; do not ask for another design approval or commit other workers' paused files.

**Goal:** Make the actual native app's existing Sign in flow perform public-client PKCE, verify the returned account and keep its credentials private for subsequent native requests.

**Architecture:** Reuse the exclusive curl/PSA runtime for a fixed token POST and existing typed userinfo GET. A process-owned auth context manages browser sessions, one-use PKCE and account replacement. Existing app HTTP routes bootstrap a session, start authorization, process the callback, report live account status and disconnect. Initial credentials are held only for the process lifetime; registered configuration and console browser validation remain required before a live sign-in claim.

**Tech Stack:** Existing C11/C++20 native app, bounded Nexus owning parser, picosha2 S256, checked platform entropy, pinned curl 8.18/Mbed TLS, owned HTTP/TLS fixtures.

**Spec:** This plan; existing companion `modbridge_nexus/auth.py`, `account.py`, `modbridge_host.py` browser routes, and `web/app.js` sign-in/account contract. User intent remains PS5-only official browser login and automatic downloads/install, with no per-mod AI or manual credential UI.

## Global Constraints

- Work in the existing translation-core worktree from `13b9e4870ed9c1428d93d4f7578dd9568ac0ccc6`. Preserve paused title/catalog, root Makefile and user files.
- No console contact/execution/writes, live account calls or production credential reads while the user sleeps. Public primary-source reads and fake owned HTTP/TLS fixtures are allowed.
- Do not retry or reroute the specifically stopped native archive-placement caller, game-memory-read or curl 8.22 upgrade. This phase performs no game/system installation or engine execution.
- No Nexus password capture, client secret, borrowed `public_test` identity, `.dev` endpoint, manual-key UI, external companion dependency or credential getter. Production client identity, exact registered callback and explicit CA must be supplied; absent configuration keeps auth capability unavailable.
- Client ID `[A-Za-z0-9_-]{1,128}` excluding `public_test`; scope at most 1024 printable ASCII bytes. Exact callback configuration is the local app origin plus `/auth/callback`; reject userinfo/query/fragment/backslash. No caller-supplied provider URL or arbitrary auth header.
- Checked entropy produces fresh 32-byte samples. State, browser cookie and CSRF are 43-character base64url tokens. Verifier uses fresh entropy and RFC7636 S256; no deterministic production fallback or caller time claims.
- Browser sessions: maximum 16, fixed monotonic lifetime 1800 seconds. One pending PKCE transaction lasts 300 seconds; same-session restart replaces it, another active session receives BUSY. Use `mb_session` with `HttpOnly; SameSite=Lax; Path=/`; callback requires the pending cookie and matching one-use state. Start/disconnect additionally require exact Origin and CSRF.
- Token exchange, account verification and resolver wrapper use total monotonic budgets of 1..30000 ms, with nonzero connect budget at most total. Clock/cancellation faults fail closed. Full temporary cleanup and final expiry/deadline checks precede publication/replacement.
- Token access/refresh values are private printable ASCII33..126, 1..16384 bytes each; token type is case-insensitive bearer; expires_in is integer 1..31536000 seconds. Anchor TTL conservatively at exchange start with checked arithmetic. Do not decode JWT claims to infer live identity or premium access.
- Reject invalid/aliased specified spans before mutation. Wipe complete valid form/raw scratch and failed token/output capacity. Erase accessible owned parsed secrets; make no secure-erasure claim for opaque JSON/curl internals. No token, code, verifier, signed URI, provider body or request target in public receipts/logs.
- HTTP header limit stays 4096; auth POST JSON body limit 16. Require one canonical Content-Length, no Transfer-Encoding, exact total frame and empty JSON object for start/disconnect. Existing other routes retain bodyless framing and no new CORS authority.
- Provider work has its own bounded deadline. Establish a fresh bounded response-send deadline after handling, so provider timeout produces an HTTP response. Successful callback redirects to `/app`; `/` remains the native status page.
- Account summary matches existing UI: verified `status:"connected",live_service:"verified",account.display_name`; account GET verifies live userinfo. Failed sign-in replacement preserves the previous account. Disconnect erases active/pending credentials. No automatic refresh or persistence in this first vertical slice.
- Catalog/jobs/download/install capabilities stay false. Sign-in and account capabilities reflect actual configured routes only; all console execution/translation support remains unverified.

## Review Focus

- Valid code with wrong cookie/state, duplicate callback query, replay or expired transaction must make no token request and must not replace the account.
- A token reply accepted before late owner/clock/cleanup failure must be completely revoked.
- A candidate token whose userinfo fails must preserve the previous account; stored credentials alone must never produce live verified UI status.
- Cross-site browser return must work with the chosen Lax cookie, while start/disconnect still require Origin and CSRF.
- Split/oversized bodies, duplicate length/auth headers and delayed provider completion must not break the existing HTTP server or suppress its error response.

### Task 1: Fixed token exchange in the existing owner

**Files:** new `native-host/oauth_code.h/.cpp`, `download/oauth_code_transfer.h`; modify `download/curl_download.c`, `include/modbridge/nexus.hpp`, `src/nexus.cpp`. Focused controls use ignored evidence.

**Interfaces:**

- `mb_nexus_oauth_code_fields_v1`: borrowed explicit spans for client_id, redirect_uri, code, code_verifier. Code limit 2048; verifier `[A-Za-z0-9._~-]{43,128}`.
- `mb_nexus_oauth_write_code_form_v1(fields, output, capacity, written)` writes canonical order `grant_type=authorization_code&redirect_uri=...&client_id=...&code=...&code_verifier=...`; unreserved bytes raw, others uppercase `%HH`, no plus or extra/duplicate fields. Form limit 65536.
- `mb_curl_oauth_code_transfer_v1(params,result)` is private: no URL/auth/header input, fixed `POST https://users.nexusmods.com/oauth/token`, form-urlencoded/JSON headers, HTTP200 only, raw body 1..131072, same busy/entropy owner and consumer/late guards. Existing GET provider ABI stays unchanged. This accommodates the complete escaped maximum token pair plus envelope; unknown fields still share the independent resource cap.
- `mb_nexus_oauth_exchange_code_v1(params,result)` accepts code fields, CA/users resolve, deadline/cancel, caller form/raw scratch and private fixed token block. Result is scalar only. Tokens have access/refresh arrays of 16385, lengths, expiry seconds and checked monotonic expiry nanoseconds. Reuse existing ParseGate and accessible string wiping. Stage tokens until full cleanup/final guards; no short-output code refetch.
- Exchange statuses: OK0, BAD_ARGUMENT1, BAD_STATE2, TRANSPORT_FAILED3, TOKEN_REJECTED4, CANCELLED5, TIMEOUT6, INTERNAL_ERROR7; result also retains underlying transport status. No runtime start/close/reset/retry/refresh.

- [x] Genuine missing API/form/POST RED; bounded form/parser controls and retained transport admission controls.
- [x] Implement the typed exchange and narrow shared-owner POST without widening existing GET/archive authority.
- [x] Focused cancellation, malformed/duplicate/oversized token, private output alias/wipe and late deadline/revocation controls; freeze header for Task2/root.

### Task 2: Process-owned PKCE/account context

**Files:** new `native-host/auth.h/.cpp`; focused ignored boundary controls.

**Interfaces:** opaque `mb_nexus_auth_context_v1`; C-only ABI1 PODs with struct_size/abi_version and explicit spans; no-throw functions:

`mb_nexus_auth_create_v1(create_params*, context**, result*)`, `mb_nexus_auth_free_v1(context**)`, and `mb_nexus_auth_session_v1`, `start_v1`, `callback_v1`, `account_v1`, `disconnect_v1` with `(context*, operation_params*, result*)`.

Create owns validated public config/trust/address copies and a borrowed checked entropy callback/context. Session accepts optional cookie and separate caller-owned cookie/CSRF output spans (each 44 minimum). Start takes cookie/CSRF/Origin and URL output. Callback takes cookie/exact target and bounded provider options; account takes cookie/provider options; both return public summary JSON only. Disconnect takes cookie/CSRF/Origin. Result contains status/complete/configured/new_session/credentials_present/output length, never a secret. Publish exact header before root HTTP integration.

Include `mb_nexus_auth_resolve_download_v1` with session authorization and selected-file inputs to inject the private active bearer/CA into the existing resolver. Reject caller credentials/auth override; private URI output remains subject to existing resolver wiping and final stored-token expiry checks. This is a callable bridge for the later job dispatcher, not an HTTP download route.

- [x] Genuine missing-context RED; PKCE known vector, browser cap/expiry, wrong bindings, state grammar/replay/cancellation, no secret output controls.
- [x] Implement session bootstrap, pending transaction and live token→userinfo verification using Task1; consume before POST, preserve old account on every failed replacement, atomically replace only after final guards.
- [x] Implement fresh account summary, disconnect/full erasure/free and auth-owned selected-file wrapper. Verify with actual metadata/resolver boundaries in focused fake-provider controls; no live-service claim from those controls.

### Task 3: Actual HTTP flow and owned TLS proof

**Files:** root owns `host.c`, `http.c/.h`, `build.py`, new `oauth_config.py`/config controls, focused builder/framing regressions and READMEs; fixture worker owns new `tests/nexus_auth_socket_shim.c`, `nexus_auth_verify.py` only.

Root adds public OAuth configuration/trust to generated build config/identity; defaults are unconfigured. The owned fixture supplies a fake public client, exact loopback callback, owned CA and users address mapping. Native configuration must come from explicit compiled public settings, never silently from desktop argv/environment. Root/fixture agree the exact configuration interface before use.

HTTP consumes the frozen Task2 header. Wire existing `/api/capabilities` session/CSRF bootstrap, `/api/auth/start`, exact `/auth/callback`, `/api/account`, `/api/auth/disconnect`. Add checked Set-Cookie and fixed Location support. Receive complete bounded auth bodies before dispatch; refuse excess/pipelining, duplicate headers and nonempty/invalid JSON. Keep unrelated legacy routes/capabilities unchanged and set fresh send budget after dispatch.

Fixture links actual current app host/HTTP and manifest-verified C++ objects; only its C transport call-site hook maps resolved127.0.0.1:443 to the owned ephemeral TLS listener. Original production HTTPS URL, SNI/Host and CA/name checks remain active. All secrets are fake; reports only contain scalar/boolean checks.

- [x] Observe current auth-start/body/callback unavailable RED through actual app socket.
- [x] Integrate exact HTTP/configuration/build flow and compile actual host/native compositions; no UI polish or catalog work.
- [x] Replay actual app socket→official-shaped PKCE URL→owned token POST→Bearer userinfo→account summary. Verify callback replay/wrong cookie/CSRF, disconnect, malformed token, failed replacement preserving old account and delivered timeout followed by healthy reuse. Exercise auth-owned resolver separately using the same retained account without a token getter.
- [x] Focused independent task reviews and final integration review, root replay, current source/artifact/include identity checks and selective commit. Save unfinished production registration/trust/browser validation, persistence/refresh, native jobs/dependencies/install and engine execution as next actions; keep full goal active.

Primary references: [Nexus public PKCE flow](https://github.com/Nexus-Mods/oauth2-demo-app/blob/master/README.md), [official Vortex OAuth implementation](https://github.com/Nexus-Mods/Vortex/blob/master/src/renderer/src/extensions/nexus_integration/util/oauth.ts), [RFC7636](https://www.rfc-editor.org/rfc/rfc7636). Local bounds and Lax-cookie binding are application decisions.

Execution evidence: ignored SDD ledger at `.superpowers/sdd/2026-10-02-native-nexus-sign-in/progress.md`; final verification at `.superpowers/native-dependency-spike/native-nexus-sign-in-1/root-final-verification.json`. Auth-owned resolver uses actual metadata/resolver with explicit private provider/exchange substitutes; separate actual resolver/archive TLS regression passes25 requests. Actual app sign-in/account TLS passes32 checks. Root final host/native builds bind51 current product inputs. This plan completes a local vertical slice; the full mod-manager goal remains active.
