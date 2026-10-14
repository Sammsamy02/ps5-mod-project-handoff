# Game-independent selection and format dispatch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Select any Nexus catalog game/mod independently from a trusted console target, preserve that target in analysis jobs, and dispatch native archive inspection by bytes rather than game or mod identity.

**Architecture:** The host owns immutable title snapshots and resolves opaque browser references. The native sealed adapter registry selects a format operation over bounded captures and wraps the existing PAK validator. Engine/build acceptance, asset semantics and installation remain separate from archive integrity; the UI reports those boundaries accurately.

**Tech Stack:** Python companion/local HTTP fixtures, JavaScript controller UI/Playwright, native C v1 POD API, existing C++ planner and C PAK library.

**Spec:** `docs/superpowers/specs/generic-ps5-title-inventory-design.md`, `generic-engine-format-dispatch-design.md`, and `generic-title-adapter-ui-contract.md` in the same directory.

## Global Constraints

- No game/title/mod-ID branches. Nexus identity is provenance and catalog selection, never target or engine evidence.
- Finished runtime must run on PS5 without PC, phone, Windows tooling or AI dependencies; this development companion remains labeled desktop preparation.
- Console title provider defaults to unavailable. Conflicting SDK app-info samples do not authorize guessed metadata reads.
- A PAK probe is Unverified. Successful validation supports only its explicit format scope; compatibility cannot become Supported through this slice. Installation remains unavailable.
- No account, console, FTP, payload-store, game or system operations. Root owns Git and synchronization.
- Focused meaningful RED/GREEN checks; no repeated broad gates, sanitizer bundles or deployment checks for these local experiments. Strict review remains required for future console/game/system writes.

## Review Focus

- Stale or browser-injected target data must be rejected before provider downloads.
- A retry key with another target must conflict; restart must preserve the original receipt without manufacturing a selectable snapshot.
- Official-looking URLs with credentials, alternate hosts, encoded paths, invalid IDs or unsupported parameters must not produce arbitrary fetches.
- Ambiguous/incomplete format probes and stale bindings must never produce Supported or fall through to another adapter.
- Controller navigation must include the new target/URL controls without intercepting text editing or losing selection during asynchronous responses.

## Ownership and tasks

### 1. Generic direct mod route — root

- [x] Add behavioral route tests for arbitrary game domain/mod ID, invalid identity, local session and provider denial.
- [x] Execute RED (three tests, missing route produced five failures).
- [x] Implement `BridgeApplication.mod_details` and GET `/api/games/{domain}/mods/{id}` returning `{item: normalized_mod}`; fixed Nexus provider origin retained.
- [x] Execute focused GREEN with adjacent catalog/session checks: five tests passed. Test denial code corrected to the provider's documented `content_hidden` code; unknown codes normalize to `provider`.

### 2. Trusted title snapshot module — Luna

Own `companion/modbridge_inventory.py` and `tests/title_inventory_tests.py` only.

- [x] Write focused fixtures and run actual RED before product implementation.
- [x] Implement bounded provider capture validation and immutable materialization matching schema-v1 fields in the inventory spec. Expose `TitleInventory(provider=None)`, `snapshot(refresh=False)`, and `resolve(reference)`. Default status unavailable/reason `platform_provider_unavailable`; no native metadata reads.
- [x] A provider capture supplies metadata only, without browser keys. Server generates opaque snapshot/title keys; `resolve` requires exact `{snapshot_id,title_key}` from current complete observation and raises a typed stale/invalid error. Deep copies prevent caller mutation. Repeated reads preserve the current reference until explicit refresh.
- [x] Keep strict measured SHA256 fingerprint provenance outside descriptive labels; identity/version/name alone supply no planner fingerprints. Define a resolved record's server-only projection clearly for root integration.
- [x] Verify complete/empty/partial/unavailable/malformed/cap-overflow captures, stale references, record mutation, browser facts, changed measured build, and unrelated titles.

### 3. Native format dispatch — Sol native

Own `native-host/format/dispatch.h`, `dispatch.c`, `pak_adapter.c`, focused test/driver under that directory, and `native-host/format/README.md`. Root owns Makefile integration if needed.

- [x] Write and compile meaningful missing/stub RED fixtures.
- [x] Implement the spec's <=16 sealed built-in registry, generation/context, bounded prefix/tail probe, shared work ledger, deterministic ambiguity/incomplete handling, strict v1 validation and owning scalar results.
- [x] Implement classic-v11 probe and typed wrapper around the current PAK candidate operation, binding registry/operation/source token+size, no permissive fallback, no positive engine/runtime/install projection.
- [x] Exercise both retained real PAKs, actual selected payload/metadata successes, invalid/stale/alias/ambiguity/budget/cancel/error controls. Reuse existing fixture tools instead of duplicating the PAK parser or broad matrix.
- [x] Compile/link focused caller for the pinned PS5 SDK; never deploy. Record host-fixture proof and compile/link-only PS5 evidence separately. Five host groups/163 assertions passed; root also passed the integrated `make test-format` target. Native ELF was not executed.

### 4. Host target-bound analysis — root

Own `companion/modbridge_host.py`, `companion/nexus.py`, `companion/modbridge_reports.py`, `tests/host_tests.py`, and `tests/analysis_report_tests.py`.

- [x] Inject `TitleInventory` and return its full schema at existing session-protected GET `/api/detected-titles`; add CSRF-protected POST `/api/detected-titles/refresh` for explicit fresh capture.
- [x] Accept optional exact `target_ref` on `download_analyze`; resolve before enqueue; return stale 409. Never accept target facts/fingerprints/platform capabilities from the browser.
- [x] Include target reference in retry identity, public job and durable receipt with strict shape validation. Older records stay unbound analysis-only. Resolve a trusted private target context again before analysis; stale context fails without pretending compatibility.
- [x] Pass trusted exact target fingerprints into the existing planner only when independently measured. Publish a bounded allowlisted summary of actual planner blockers, never raw paths, arbitrary backend assertions or an install flag.
- [x] Run focused target selection, stale/bad input, retry/restart and actual core-analysis fixtures. Preserve prior source/hash/session behavior.

### 5. Controller UI — Sol UI

Own `web/app.js`, `web/index.html`, `web/style.css`, and separate `tests/generic_selection_ui_tests.cjs` (existing UI test edits only if required to update a legitimate fixture).

- [x] Write focused Playwright RED controls: arbitrary official mod URL/domain+ID, stale response, malformed URLs, unavailable/complete title snapshot, target-ref-only job intent and controller focus/text editing.
- [x] Add independent Console target panel. Only current complete snapshots are selectable; display title ID, optional label/version and explicit build confidence. Revalidate restored references, include only opaque reference on jobs, clear/refresh on stale 409.
- [x] Add direct Nexus mod URL/ID lookup via existing host route; no arbitrary URL fetch or invented provider search endpoint. Preserve generic catalog/manual domain association.
- [x] Show real analysis blockers when supplied, otherwise explicit Unverified compatibility; never imply format proof grants engine compatibility or installs.
- [x] Run focused tests and adjacent existing UI checks once if changed flows require them. Thirteen focused UI cases and the actual-host/C++ analysis browser fixture passed; no live account/browser service/device access.

### 6. Root integration and handoff

- [x] Inspect exact changes and focused receipts; fix integration issues, then run appropriate combined checks once.
- [x] Update user-facing checkpoint with implemented functions and explicit unproved runtime boundaries.
- [x] Commit only owned product/tests/docs; preserve experimental evidence and all original user files during guarded fast-forward synchronization. UI/format completion travels separately from the ongoing native metadata/provider work.
- [x] Keep the overall goal active: native signin/download/UI integration and actual game-engine/runtime acceptance still require further work.
