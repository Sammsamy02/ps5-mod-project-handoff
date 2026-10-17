# Lazy installation of reviewed PE imports

Stage `cg-lazy-reviewed-imports-20261013-17`, copied from isolated stage 16. Shared candidate and original DLLs remain untouched.

Add an opt-in executable-memory backend to `WinApiShim`. The backend allocates a writable region, seals it executable, and releases it on failure. If configured, `resolve` lazily installs a catalog row only when its signature and behavior are reviewed. The PE loader already invokes `ImportResolver::resolve` for each import, so no game-specific address table is added. Registered modules continue to take priority. Missing backend, unknown rows, unreviewed rows, allocation/seal failures, and recursive installation remain unresolved. Concurrent successful installs may race; only one published pointer wins and the loser is released. Published mappings are retained until process exit.

Write a failing host test first. Test no-backend and unsupported refusal, successful lazy RW->RX transition and Win64 call, idempotent resolution, invisibility during sealing, recursive refusal, failure cleanup and retry, and registered module priority. Then run the prior regressions and package an exact-base patch and evidence.

The platform backend must supply real allocation/cache/permission behavior on PS5; the Mac test only checks host behavior. Dynamic TLS/GS, CRT/UE4SS, target build correspondence, and all unsupported imports remain separate blockers.
