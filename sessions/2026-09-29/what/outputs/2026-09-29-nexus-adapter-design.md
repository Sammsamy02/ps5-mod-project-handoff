# Nexus adapter design

This is the next reversible local build under the user's request for unattended progress. It advances the universal manager's provider connection across Nexus game domains. It does not narrow the translation goal to one game, equate downloading with compatibility, or claim a PS5 runtime.

The desktop preparation companion will implement the official provider protocol in Python 3 using the standard library. The C++ core remains independently buildable. This companion is a protocol implementation and test harness; the final console app still needs a validated native HTTPS/browser/storage adapter. A permanent PC requirement is not established by this design.

## User flow

The app's own interface selects a Nexus game domain and browses mod/file metadata. Sign-in opens the real Nexus authorization page when this project has its own registered public client ID and approved callback. No password form is implemented by us. A personal API key may be used locally during development, through private local storage; a distributed release cannot reuse a developer's key.

The API resolves a selected file's download links. Free-account downloads require the matching account's unexpired website NXM authorization. Premium eligibility comes from verified provider user information, never a client flag. A downloaded archive lands in a private quarantine folder with a streamed SHA-256 and source provenance. Analysis and target-dependent preparation follow separately. All game domains use the same provider code; provider mod IDs never select translation patches.

## Interfaces and boundaries

Files live in `outputs/ps5-mod-bridge/companion/modbridge_nexus/` and `tests/nexus_tests.py`.

`Response(status, headers, body)` and an injected transport separate HTTP from provider decisions. Production transport permits HTTPS only, applies timeouts and a bounded response body, and refuses redirects for authenticated requests. Provider credentials go only to the fixed API/user-service origins. External download streams never inherit API headers. Errors expose a bounded classification/status/retry hint, never raw response bodies, credential values or signed URLs.

`OAuthConfig(client_id, redirect_uri, scope="")` requires explicit project registration values; reject the provider demo's `public_test`. `OAuthSession` generates a cryptographically random verifier/state and S256 challenge, limits session lifetime, binds callbacks to the configured redirect, rejects duplicate parameters, and consumes a valid callback once. Token exchange and refresh use the official user-service endpoint without a shared client secret. No configured client means login is unavailable with an explicit reason. The configured production endpoint is an inference from the official development demo and production API library; live registration/callback acceptance remain unverified.

`Credentials` separates personal API keys from OAuth access/refresh tokens and redacts their representation. A private, caller-selected credential store rejects symlinked paths, uses restricted permissions and atomic replacement, and supports disconnect. The initial implementation handles sequential companion operations; cross-process refresh coordination and a console credential vault remain future work.

`NexusClient` provides user validation, game list, latest mods for an arbitrary game domain, mod details/files and download-link resolution. Validate identifiers and response shapes. Preserve author/source links and hide adult/unknown-content records by default where metadata permits; do not promise comprehensive content-policy enforcement before official account preference integration. Expose rate-limit headers and HTTP429 without a retry storm or background catalog mirroring. Broader search can be added through a versioned GraphQL adapter; REST metadata access is the first provider implementation.

`parse_nxm` accepts the documented game/mod/file URL shape, rejects duplicate/missing query parameters, and validates positive numeric IDs, expiry and account binding. Collection links are explicitly unsupported until a collection resolver exists; they cannot masquerade as ordinary files.

`download_to_quarantine` writes bounded streams through a temporary file in an explicitly chosen project folder, hashes the actual received bytes, publishes only a complete file, and cleans interrupted partials. A result records game domain, mod/file IDs, source webpage and hash, and remains at the downloaded stage. It cannot set prepared/deployed/game-working states. Scope is cooperative local storage, not a guarantee against arbitrary concurrent external filesystem writers.

## Verification and release limits

Tests use synthetic HTTP transports and owned local files. Cover PKCE vectors/state/replay/expiry, wrong redirect and duplicate callbacks, credential isolation/redaction, registered-client requirement, private-store permissions/symlinks, arbitrary game domains, invalid provider payloads, provider errors/rate limits, NXM account/expiry rules, Premium and free authorization paths, redirect refusal, oversized/truncated downloads and byte/hash provenance. A separate local HTTPS transport test may use an owned test certificate if necessary; no provider login or real download is needed for offline verification.

Passing tests establishes protocol behavior against fixtures. It does not establish production client approval, live login, provider contract acceptance on PS5, translated assets, UE4SS/Mod Engine 2 operation, or console deployment. Those statuses remain unknown until measured.

## Primary source baseline

Checked 2026-09-29. [Official OAuth demo](https://github.com/Nexus-Mods/oauth2-demo-app/tree/809f4518143e7cfd3f34a5296ded54dafa1ec5c9) documents the authorization-code PKCE flow using development credentials. [Official API library](https://github.com/Nexus-Mods/node-nexus-api/tree/da9b8eda8a3f381f04d87c7290595066ccbdbc5d) supplies production origins, user validation, game/mod/file/download endpoints and refresh semantics. [API acceptable-use policy](https://help.nexusmods.com/article/114-api-acceptable-use-policy) covers personal testing keys and public application review. The earlier Nexus research artifact records API-version, callback, content-preference and free-account caveats.
