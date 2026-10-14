# Nexus adapter implementation evidence

Implemented the offline-testable Python 3.9+ standard-library provider adapter under `ps5-mod-bridge/companion/modbridge_nexus/`. No live account authentication, provider registration, or Nexus download was performed. Account login, PS5 networking/storage/browser acceptance, translated assets, and console operation remain unverified.

## Implemented behavior

- HTTPS metadata/token traffic is limited to the fixed Nexus API and user-service origins. TLS verification is required; safe errors omit response bodies, signed URLs, and credentials. Provider redirects are surfaced and refused by the client. CDN streams carry no API key, bearer header, or cookie and refuse every redirect.
- Credentials have redacted representations, mutually exclusive API-key/OAuth modes, private owner-only local files, symlink checks, bounded JSON reads, atomic replacement, and disconnect. They are stored locally as protected plaintext; this does not claim encryption or a console vault.
- Explicit registered-client OAuth configuration rejects the official `public_test` demo ID. PKCE uses a random verifier/state and S256 challenge. Sessions check the exact configured callback, state, duplicate parameters, lifetime, and one-use consumption. Cross-process records must use the provided private JSON helpers. Token exchange and refresh omit a shared secret and preserve rotating access/refresh credentials.
- A single REST client implements verified account information, game listing, latest-added mods, mod details, file metadata, and download-link resolution across ordinary Nexus game domains. UI-facing account records exclude key/token/email fields even when a successful provider response contains them. Premium entitlement comes from the authenticated provider endpoint. Adult, unknown-content, unpublished, and unavailable mod records are hidden by default.
- Free downloads require an account- and game/mod/file-bound, unexpired NXM key. Unsupported collection NXM links, missing/duplicate fields, path tricks, and incorrect account bindings are rejected. Premium links do not accept a caller-provided entitlement flag.
- Bounded streams publish private quarantine files only after completion, hash the actual bytes with SHA-256, record safe source webpage/game/mod/file provenance, preserve existing files/sidecars, and clean interrupted partials. The result remains `stage="downloaded"`; no prepared/deployed/game-working state is produced.

## Public interface

All public classes/functions below are exported from `modbridge_nexus`:

```python
Credentials(api_key=None, access_token=None, refresh_token=None, expires_at=None)
CredentialStore(path).save(credentials)
CredentialStore(path).load()             # Credentials or None
CredentialStore(path).disconnect()
load_private_json(path)                  # bounded private record or None
save_private_json(path, record)

OAuthConfig(client_id, redirect_uri, scope="")
OAuthSession(config, *, clock=time.time, lifetime=300)
session.authorize_url
session.consume_callback(url)            # one-use authorization code
session.to_record()                       # secret-bearing private-storage record
OAuthSession.from_record(record, *, clock=time.time)
exchange_code(session, callback_url, transport=None)
refresh_credentials(config, credentials, transport=None, *, clock=time.time)

Response(status, headers, body)
HTTPSTransport(*, timeout=30.0, max_bytes=1048576, context=None)
NexusClient(credentials, transport=None, *, app_name="PS5ModBridge", app_version="0.1.0")
client.user()                             # UserProfile; safe normalized fields
client.games()
client.latest_mods(game_domain)
client.mod(game_domain, mod_id)
client.files(game_domain, mod_id)
client.download_links(game_domain, mod_id, file_id, nxm=None)
client.rate_limits

parse_nxm(url)                            # redacted NXMAuthorization
DownloadSource(game_domain, mod_id, file_id)
DownloadLink(uri, name, short_name, source)
download_to_quarantine(link, destination, source=None, *,
    transport=None, max_bytes=1073741824, timeout=300.0)
```

`destination` is a private directory. `DownloadResult` supplies `path`, `sha256`, `bytes_downloaded`, `source`, `stage`, and the `provenance_path` property. Source override, if supplied, must match the link's source. Metadata methods return sanitized dictionaries; account information is `UserProfile(user_id, display_name, is_premium, is_supporter, adult_content)`. `NexusError` exposes bounded `code`, `status`, and `retry_after` values with fixed safe descriptions.

## Verification on 2026-09-29

TDD record: transport/store 9 initially failing tests → passing; OAuth 7 initially failing tests → passing; client 8 initially failing tests → passing; NXM/quarantine 8 initially failing tests → passing. Follow-up failing regressions caught an unsafe custom TLS context, missing refresh token acceptance, and a blocking metadata read that could exceed its total deadline under continuous trickle input. All were corrected and re-run.

- `python3 outputs/ps5-mod-bridge/tests/nexus_tests.py`: 39 tests passed, including five real HTTPS tests against an owned localhost TLS server. Only socket routing is redirected locally; production TLS, HTTP parsing, and redirect/deadline behavior execute. The embedded self-signed certificate/private key are public synthetic fixtures, never provider credentials.
- `make test` from `outputs/ps5-mod-bridge`: 77 C++ tests and 11 existing CLI tests passed. At this verification point the Make target does not yet include the Nexus/companion tests; root owns that wiring.
- `python3 -m unittest discover -s outputs/ps5-mod-bridge/tests -p '*test*.py'`: 59 Python tests passed at this point, including the 39 Nexus tests and root's then-current command tests. Root is adding further command coverage before final integrated verification.
- `git diff --check`: passed.

## Decisions and limits

Every provider/CDN redirect is refused, which may reject a future legitimate redirect-based provider contract. CDN URLs are validated for HTTPS and no userinfo/fragment/nonstandard port; this implementation does not maintain a guessed Nexus CDN host allowlist. All origins/endpoint behavior are based on pinned public sources, not a live contract test.

The absolute deadline spans setup, network headers, streaming, and publication, with decreasing socket timeouts and bounded `read1` operations. Standard-library DNS resolution and kernel filesystem calls are not forcibly preemptible; the deadline rejects completion after expiry but cannot guarantee interruption of an OS resolver/filesystem stall at an exact instant.

Local storage assumes cooperative sequential companion operations. Cross-process refresh/session replay coordination, encrypted credential storage, adversarial concurrent filesystem writers, and crash-atomic publication of the archive/sidecar pair are future work. Publication never overwrites an existing destination, but an OS/process failure between its two links can leave an unprepared quarantine archive without its sidecar. No output becomes installable on that basis.

REST latest-added browsing is implemented; broader GraphQL search, collections, preference integration, SSO/phone pairing, and console protocol adapters are not. OAuth `userinfo` preferences are normalized only if returned; the adapter does not yet call the separate GraphQL preference endpoint. Metadata filtering is conservative and does not claim comprehensive Nexus content-policy enforcement.

Primary protocol baseline: [official OAuth demo at 809f4518143e7cfd3f34a5296ded54dafa1ec5c9](https://github.com/Nexus-Mods/oauth2-demo-app/tree/809f4518143e7cfd3f34a5296ded54dafa1ec5c9), [official API library at da9b8eda8a3f381f04d87c7290595066ccbdbc5d](https://github.com/Nexus-Mods/node-nexus-api/tree/da9b8eda8a3f381f04d87c7290595066ccbdbc5d), and its fetched [pinned types.ts](https://github.com/Nexus-Mods/node-nexus-api/blob/da9b8eda8a3f381f04d87c7290595066ccbdbc5d/src/types.ts) for the distinct API-key and OAuth user response fields. The production OAuth origin is inferred from the production library, while the original demo uses a development origin.
