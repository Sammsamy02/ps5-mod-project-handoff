# Native app asset serving Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development. Steps use checkbox syntax.

**Goal:** Serve the same game-independent controller UI directly from the native application, with accurate unavailable service capabilities.

**Architecture:** Embed only three named trusted assets as immutable read-only spans at build time. Native HTTP sends those spans without copying them through its 8 KiB JSON buffer. Read-only bootstrap endpoints report actual native capabilities; no account/download/job/session writer is introduced.

**Tech Stack:** Existing C native HTTP host, Python offline builder, generated C arrays, owned loopback browser fixture, pinned PS5 SDK compile/link only.

**Spec:** `docs/superpowers/specs/generic-title-adapter-ui-contract.md`, native-wiring paragraph, plus this bounded refinement.

## Constraints and review focus

- Only exact `/app`, `/app.js`, `/style.css` routes serve embedded assets; preserve `/` diagnostics and `/report`, `/version`, `/core`, `/shutdown` behavior and frozen old artifacts.
- Native app bootstrap returns mode `console_preview`, console execution unverified, all Nexus/account/auth/download/translation/deploy flags false. Running a native library does not make an unfinished native service available.
- Inventory returns the same unavailable schema-v1 shape; no SDK guesses or device reads.
- GET routes only, current exact Host/Origin/query/request-framing policy preserved. No credentials, CSRF session store, posted jobs, account or download integration in this slice.
- Static responses borrow bounded compiled arrays, JSON remains capped at 8192. Error responses clear a previously selected span; no arbitrary file/path reads. Asset total cap 256 KiB; any missing/symlink/oversize/invalid asset fails the build.
- Add scoped app CSP (`script-src 'self'; style-src 'self'`, same-origin connect, no frames/objects), retain existing diagnostic inline-script hash policy for diagnostic routes. Image sources remain none in native preview unless independently needed.
- Asset bytes/hashes enter build identity. Generate into the selected build directory; never overwrite retained checkpoint artifacts or ignored source caches.
- Local focused checks only; no console/payload-store/game/system/FTP/account actions, no stopped curl work, no deployment/review gate.

## Task — Luna owns native serving seam

Own `native-host/http.c`, `http.h`, `host.c`, `build.py`, new `native-host/web_assets.py` plus focused test/caller/browser files under `native-host/tests/`. Do not touch shared web assets or Sol's format directory. Coordinate tests using real app files only after Sol UI declares them stable.

- [x] Read native request/response/build contracts and record a focused failing route/large-span/build-identity test before implementation.
- [x] Generate immutable arrays for exact index.html/app.js/style.css with strict names, sizes and file safety checks. Include their actual digests in source/build identity.
- [x] Add separate `http_response` static-span fields; send only validated static spans, retaining the JSON/body path. Preserve existing diagnostics as `/`; app index `/app` references absolute named assets.
- [x] Add session-free read-only unavailable bootstrap/title endpoints for preview. No posted-body handling or install authority.
- [x] Exercise full content lengths above 8192, exact body bytes, no traversal/encoded/query routes, stale span cleared on error, CSP per response, unavailable capabilities, build failure for missing/oversize assets, no disk reads during requests.
- [x] Build only a fresh desktop fixture directory; run owned loopback app browser check with no outbound service calls, accurate disabled actions and unchanged diagnostic endpoints.
- [x] If ordinary pinned-SDK native compile/link can run from verified local inputs without changing dependency profile, record compile/link only. Otherwise preserve host proof and explicit native-link limitation; do not retry blocked curl or install tools.
- [x] Write concise source/proof note. Root owns Git/index/commit/synchronization and optional Makefile integration.

## Root follow-through

- [x] Inspect implementation and meaningful tests, integrate with stable UI and verify once.
- [x] Report native app serving separately from native Nexus signin/downloads and game acceptance, which remain unfinished.

Implementation checkpoint: embedded app desktop build and owned Playwright check passed; root independently passed three static-route checks and verified exact frozen asset/artifact identities. Native build stopped before compile on the missing pinned LLVM archive; no native app ELF or execution is claimed. A separate investigation owns that offline build prerequisite.
