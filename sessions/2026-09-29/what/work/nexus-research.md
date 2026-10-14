# Nexus Mods integration research

Checked official sources on 2026-09-29. Research only; no registration, contact, or application implementation performed.

## Recommendation

Keep the mod manager's native UI for discovery, compatibility information, file selection, download progress, and installation. Hand account sign-in and authorization to a real Nexus Mods page. Use a registered public OAuth client with PKCE if Nexus supports a callback usable on the chosen PS5 homebrew runtime; retain registered websocket SSO/API-key authorization as a practical alternative for console-to-phone pairing. Treat the latter QR design as an inference from the documented protocol, not an officially documented console feature. Neither Nexus authentication nor its catalog establishes that a mod works on PS5: installation must use a separate game/version/platform compatibility layer.

## Registration and API keys

The official API AUP permits personal API keys during testing or personal use, but requires a public application to be reviewed once a testing build demonstrates intended API use. The request includes a test build; subsequently Nexus asks for a name, description, and logo and issues an SSO slug. Users can revoke per-application keys. Identify requests with consistent `Application-Name` and accurate `Application-Version`. The AUP lists server storage of user API keys, user-uninitiated use, public apps using personal keys, metadata impersonation, and bulk fetching to rehost data as unacceptable. Do not build the distributed app around one developer's key.

Source: [API Acceptable Use Policy](https://help.nexusmods.com/article/114-api-acceptable-use-policy). The page remains the official policy but its last-updated date is December 2020; ask Nexus during future registration how it maps to current OAuth clients.

## OAuth exists; do not assume API-key-only integration

Nexus's official demo documents authorization-code OAuth with PKCE for public applications: create a verifier and S256 challenge, open the authorization page, receive a code through a registered custom URI or local callback, and exchange that code with the verifier. It recommends a custom protocol handler and also shows a localhost callback. Its code has no client secret. The demo source uses the development domain and `public_test`; these are examples, not production app credentials. Do not reuse another application's client ID.

Sources: [official OAuth demo](https://github.com/Nexus-Mods/oauth2-demo-app), [demo source](https://github.com/Nexus-Mods/oauth2-demo-app/blob/master/bin/index.js).

The current official API library uses the production user-service origin `https://users.nexusmods.com`, sends OAuth JWTs as bearer credentials, and refreshes at `/oauth/token` with `grant_type=refresh_token`, the app's client ID, and a refresh token. A client secret is optional in this code. New access and refresh credentials are emitted for persistence. Its fingerprint handling has commented-out transmission in the current source; clarify the exact production contract rather than copying old fingerprint instructions blindly.

Sources: [API library source](https://github.com/Nexus-Mods/node-nexus-api/blob/master/src/Nexus.ts), [production origins](https://github.com/Nexus-Mods/node-nexus-api/blob/master/src/parameters.ts).

Engineering recommendation: public console binaries cannot reliably conceal a shared client secret. Keep per-user keys/refresh tokens in local private storage, protect them using available platform facilities, redact them from logs and crash reports, overwrite rotated credentials, and erase them on disconnect. Choose minimum scopes. This is integration advice, not a claim of a PS5 credential-store capability.

## Official-page sign-in, browser, and QR

The official SSO protocol uses `wss://sso.nexusmods.com`, a random request UUID, and protocol 2. The initial message contains the ID and a null token; the service returns a `connection_token` for reconnects. The app opens `https://www.nexusmods.com/sso?id=<uuid>&application=<registered_slug>`. Nexus performs login and app authorization, then returns the user's API key to the waiting websocket client.

Source: [official SSO integration demo](https://github.com/Nexus-Mods/sso-integration-demo).

Inference: encoding that exact official URL in a QR code should let a phone browser perform authorization while the console receives the result over its own websocket; a browser callback to the console is unnecessary for this particular flow. Runtime support for TLS/websockets and Nexus acceptance must be tested. This is ordinary URL pairing, not evidence that Nexus implements OAuth's device authorization grant.

For OAuth, opening its URL on a phone does not make a localhost redirect reach the PS5: localhost belongs to the phone. A phone custom-URI handler also returns to the phone. An approved companion/cross-device callback or a documented device grant would be required. No official Nexus device-grant documentation was found. The official examples open the default browser. I found no primary evidence guaranteeing embedded iframe/webview sign-in support; do not make a console embedded login page a project dependency before verifying it with Nexus and the target runtime.

## Browsing and search are possible, with API-version caveats

The official GraphQL v2 documentation provides `mods` with pagination, filters/facets/sorting; filters include game, name/stemmed name, description, author, category, tags, status, and adult content. This supports native discovery UI instead of scraping web pages. Most v2 operations are accessible without authentication; some require OAuth. The documentation explicitly calls the API a work in progress with possible breaking changes, says v1 REST remains supported, and directs developers needing OAuth to support. Self-service OAuth registration is described as a future capability, so do not assume it is available now. Its `directDownloadEnabled` metadata describes download eligibility but is not proof that free-account website authorization can be skipped.

Source: [official GraphQL v2 reference](https://graphql.nexusmods.com/).

The official newer REST v3 documentation exists, with API-key and bearer-JWT security schemes. The checked-in schema includes mod/file detail endpoints and a download-repacked operation, many marked Experimental; its published stable/beta/experimental distinctions matter. It is not a reason to assume every legacy download or search feature has a stable v3 replacement. Put Nexus access behind an adapter and confirm the preferred supported endpoints when registering.

Sources: [current v3 documentation](https://api-docs.nexusmods.com/), [official v3 schema](https://github.com/Nexus-Mods/Vortex/blob/master/packages/nexus-api-v3/schema/openapi.yaml).

## Downloads: Premium versus free

The current official API library's `getDownloadURLs` still uses the v1 `/games/{game}/mods/{mod}/files/{file}/download_link` operation. Its documentation says non-Premium users need the website-generated `key` and `expires` values contained in an `nxm://` URL from the Download with Manager button. Premium users can obtain the URL directly. A login token alone therefore does not deliver a seamless free-account download flow. For a console MVP, support website download plus manual local-file transfer for free accounts, or later add an approved browser/companion handoff that supplies the matching account's NXM authorization.

Sources: [official download method documentation](https://github.com/Nexus-Mods/node-nexus-api/blob/master/docs/classes/_nexus_.nexus.md#getdownloadurls), [current library implementation](https://github.com/Nexus-Mods/node-nexus-api/blob/master/src/Nexus.ts).

The April 2026 official speed-cap article lists approximately 1.5 MB/s for regular users with ad blocking, 3 MB/s without ad blocking and for supporters/mod authors, and uncapped Premium speed. Non-Premium bandwidth is shared across downloads. Speed caps are server policy; a client should not promise a different cap.

Source: [current download speed limits](https://help.nexusmods.com/article/96-download-speed-caps-adblockers-and-different-types-of-membership).

## Limits and content handling

The official rate-limit help, updated June 2026, states 20,000 requests per day, followed by 500 per hour after exhausting the daily allowance; reset is 00:00 GMT daily and on each hour. It says remaining requests and reset data are in response headers. Respect actual response headers and 429s, use a modest bounded queue, cache relevant metadata locally, and fetch on user actions. Do not extrapolate this article into a promise that GraphQL has no quotas or that all v3 endpoints share identical limits.

Source: [current official API rate-limit help](https://help.nexusmods.com/article/105-i-have-reached-a-daily-or-hourly-limit-api-requests-have-been-consumed-rate-limit-exceeded-what-does-this-mean).

User-authorized download and installation on that user's device differ from running a mirror or redistributing console-converted packages. Nexus's terms leave author content subject to the author's license/permission settings and permit distribution of modified reproductions where the author allows it. API access does not itself confer a blanket redistribution or porting license. Preserve authorship and mod-source links. Current terms also make third-party API clients responsible for filtering returned adult content, so the UI needs to honor the relevant preferences/age restrictions.

Source: [Terms of Service, especially sections 10, 16, and 24](https://help.nexusmods.com/article/18-terms-of-service).

## Open points before implementation/release

- Nexus review of a public PS5 homebrew client and which registration route issues the approved OAuth client and/or SSO slug.
- Allowed callback URIs, scopes, production PKCE/token/fingerprint requirements, and whether a cross-device/device flow is supported.
- Embedded browser compatibility and any Nexus restrictions; target firmware/runtime TLS, websocket, URL-opening, and private-storage capabilities.
- Supported search/detail/download endpoint versions and applicable quotas; v2/v3 experimental contracts require fallback/error handling.
- Free-account NXM handoff on the actual runtime or a manual transfer flow; ordinary sign-in QR does not solve file download authorization.
- Selected game's actual PS5 mod formats and required conversion/install rules, established separately from Nexus metadata.
