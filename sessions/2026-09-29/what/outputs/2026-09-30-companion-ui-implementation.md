# Private companion implementation checkpoint

The shared desktop companion connects Nexus account lifecycle, games/latest mods/files, quarantined downloads and nonexecuting C++ archive analysis to a controller-oriented UI. Every title uses the same game-domain/API path; no mod ID selects a private production patch. It remains a development checkpoint toward the user's full native PS5 manager and universal translator.

## Implementation

- Persistent account refresh/rotation, one-401 retry, verified identity before replacement, legacy key migration and preservation on failed/cancelled authorization.
- Exact-loopback HTTP host with session/Origin/Host/CSRF gates, bounded requests and safe public records. Valid OAuth authorization returns to the UI. Outstanding authorization is invalidated by logout.
- Private queued jobs with idempotent intents, actual archive hash/size, safe durable receipts and interrupted phases. Storage failures stop new submissions without killing the worker or leaving the visible job running.
- Games, file selection, Activity, details and Installed views. Controller/keyboard focus, stale-response protection, safe integer IDs and public-ID-only navigation restoration. Official Nexus file-page links are derived from validated identifiers.
- Iterative planner SCC/order traversal and linear shared cycle membership. Long graph inputs no longer depend on recursive stack depth; causal blocking uses a shortest lexical witness.

## Review and evidence

Graph/account review independently checked 82 C++ tests, 42 Nexus tests, 11 account tests, 14 companion command tests, 14 core CLI tests and 250 random typed graphs against a separate SCC/blocking oracle. G1 regressions cover 6,500-node chains/reference rings/strict rings in the default, O0 sanitizer and O1 sanitizer builds.

Host review reproduced and closed four integration findings: account status field mismatch, repeat sign-in's nonexistent property, valid authorization-needed receipts rejected on restart, and callbacks reconnecting after logout. Root added receipt fault, path and interrupted-restart checks. Browser review reproduced and closed a slow-identity startup race and polling focus loss. Account verification now precedes catalog loading; unchanged jobs skip rendering, and changed receipts restore keyed controller focus.

Final combined `make test-all` passed **82 C++ tests, 96 Python tests and both browser suites**. Python coverage includes 42 provider, 11 account, 14 companion command, 15 host and 14 core CLI tests. Independent final browser review also passed both suites against the corrected sources. Native layout review independently passed **16 native tests**, a fresh cross-build/static inspection and eight additional layout boundaries. No sanitizer findings were reported in the tested G1 or native desktop fixtures. These are scoped local results, not gameplay or actual Nexus entitlement evidence.

The actual-host browser test uses real HTTP cookies/CSRF and a project-owned ZIP, then invokes the actual C++ analyzer and an unknown-target/no-backend plan. The result remains analyzed/unresolved. It does not use a real Nexus account or install anything. Current browser fixtures guard all off-origin navigation. One earlier draft navigated to an unauthenticated official authorization page because its mock covered only API routes; no credentials or user account were present. That fixture boundary was corrected.

## Native and provider boundaries

The user reports firmware 13.60, Payload Manager 0.5.2 and Stellar Blade 01.014.001. A bounded read-only GET to ps5-host-not-configured.invalid:8084/version returned 0.5.2. The underlying loader and firmware identity remain unverified. Native source review identified the candidate's getpid/resolver convention, page layout, output transport and spawned-process return issues. The layout correction is a separate static checkpoint; it does not establish native execution.

Live OAuth client registration, account/entitlement and actual downloads remain untested. A valid own client identity is needed for the requested sign-in page; private API-key support is already available as a development path. Full search/pagination, native title detection/hosting, free-download NXM handoff in the UI, automatic native mappings, platform asset conversion, UE4SS/Mod Engine 2/other runtimes, gameplay and optimization evidence remain open. The host's default plan never claims prepared or installed.

Shutdown does not forcibly preempt DNS, filesystem calls or an active download. Private file checks assume cooperative storage use. These limitations must remain visible until verified improvements exist.

## Saved local checkpoint

Reviewed files are now in the original workspace on `codex/ps5-mod-bridge`. A fresh build there passed82C++/96Python and both browser suites; native fixture tests passed16/16 and the copied ELF passed static acceptance with the recorded hash. Main remains unchanged. The managed worktree and its verified toolchain cache are retained for continued native work. The TV preview uses owned fixture metadata; it is not a live signed-in Nexus or PS5 screenshot. Preserved native files are in `outputs/native-probe-artifacts/`. No public release, push or device execution occurred.
