# Native Nexus file selection and download analysis

The user authorized autonomous development while asleep, focused local checks and parallel agents. Continue in the existing translation-core worktree from e43b4c8. Root integrates and selectively commits; preserve the paused title provider, Makefile and user files.

**Goal:** Connect the actual native application's existing Nexus selection UI to an account-authorized download of an unchanged archive and the existing native archive reader. This is a download/analysis slice, not installation or Windows-runtime compatibility.

**Design:** Reuse the private auth context for metadata and local session admission. Add a bounded, process-owned queue (eight jobs), cookie-scoped status and idempotency. Send the queued HTTP response before processing one job on the existing serialized main loop. The same curl/PSA owner resolves the selected file, then transfers its provider-issued HTTPS URI without account headers into a fresh app-owned scratch file. Close/reopen that file read-only, bind its identity and complete SHA receipt, then scan and group it with existing archive code. Reports expose scalar analysis and candidate mod types, with installation/runtime false. No worker-thread claim or live progress polling during synchronous work; durable jobs, cancellation and installation remain later work.

**Scope boundaries:** No console contact, upload, execution, game/system writes, production credential reads or live account calls. Do not retry/reroute the stopped placement caller, game-memory-read or curl upgrade. Scan only; no materialization. Configuration selects an existing app-owned private scratch directory explicitly; disabled by default. OAuth registration, CA and PS5 browser/runtime remain unverified. Download credentials, signed URIs and raw provider responses remain private. No per-game or per-mod translation recipes.

## Task 1 — Auth-owned metadata and admission

Worker owns auth.h/.cpp and focused component controls only. Publish the header first. Add local admission for cookie-only reads and cookie/CSRF/Origin mutations, optionally requiring an unexpired configured account. Add a catalog wrapper for the four existing typed metadata kinds, with no credential getter or caller trust/auth override. Reuse the real metadata client; bounded whole owning JSON matches the existing UI's {items}/{item} contract. Keep hidden/rejected metadata fail closed and account expiry checked at publication.

- [x] Genuine missing API RED, implement, focused guards and real decoder/client composition.
- [x] Peer review and recorded local evidence, no live service claim.

## Task 2 — Process-owned download jobs

Worker owns jobs.h/.cpp and focused job controls only. Publish the header first. Fixed queue and record limits, session-scoped IDs/status, strict intent and duplicate-key validation, idempotency conflict detection. Queue accepts only download_analyze plus a safe domain and positive IDs. Initially reject target_ref and NXM rather than silently ignoring them. Free-account authorization has a specific needs_nexus_authorization state; premium is fetched live by the existing resolver, never asserted by callers. Private cookie/CSRF bindings persist only while queued; erase after execution. Provider URI stays private and is wiped on every exit. Download uses existing credential-free producer and positional sink adapter. Scratch leaf names are generated IDs, exclusive/no-follow, never archive paths; failures clean up and success remains app-owned until shutdown. The root opens/validates the private directory. No materialization or placement.

- [x] Genuine missing job RED and focused queue, session, idempotency, cleanup and failure controls.
- [x] Real resolver/producer/reader composition; no fake positive job in production.

## Task 3 — Archive summary and dependency composition

Worker owns new analysis.h/.cpp, archive dependency_build.py and their focused controls. Root additionally delegates the private groups.c sort-function rename after the integrated headers expose a BSD libc symbol collision. Reuse intake.scan and groups directly on a bound read-only fd. Bounded owning summary records source SHA/bytes, member/group counts and filename-derived candidates for PAK/IoStore/INI/Lua/DLL; candidate recognition establishes no runtime or install compatibility. No source paths, member contents or raw errors in public reports. Validate exact retained host/native libarchive, zlib and LZMA inputs; add no extra crypto owner.

- [x] Genuine missing analysis RED and real ordinary/invalid archive controls.
- [x] Host/native dependency inputs validated and caller headers documented.

## Root integration and evidence

Root owns http.c/.h, host.c, build.py, builder/framing controls, documentation and actual owned HTTP/TLS fixture. Consume frozen headers. Add owned bounded metadata response storage, strict job POST framing, minimal existing catalog/file/job endpoints and capabilities that reflect selected configuration. The existing UI changes only to accept and label native PS5 analysis context; layout stays unchanged. Main loop advances one queued job only after the response is sent. All error/abandon/shutdown paths free response and job resources.

- [x] Actual missing catalog/job socket RED, then integration GREEN.
- [x] Fake owned account login → catalog/file selection → POST job → unchanged archive TLS download → native reader summary through actual host app. Include idempotency, wrong bindings, invalid archive and no account headers on archive transfer. Truncated ZIP refusal is checked through the real archive component controls.
- [x] Actual PS5 ELF compile/link only, current input identity, focused shared regressions if changed, independent review and selective commit.

Full goal remains active: free-user browser/NXM handoff, durable account/jobs, dependency installation, deterministic adapters, actual native execution and UE4SS engine bindings/DLL ports remain unfinished.

## Completed local evidence

Actual desktop-app owned TLS flow:26 checks/21 transfers passed. Final host5/native3 compositions compile/link from64 current inputs; native include closure has no host headers. Final job65 and archive43 component controls pass with sanitizers; retained dependency18, shared47 and selection UI13 checks pass. The final auth review found and fixed publication after large private cleanup, with cancellation-after-staging RED/GREEN and independent closure review. Source archives remain unchanged; no console or game/system writes. Detailed scope and receipts: `outputs/2026-10-02-native-nexus-download-analysis.md` and ignored SDD/dependency-spike ledgers.
