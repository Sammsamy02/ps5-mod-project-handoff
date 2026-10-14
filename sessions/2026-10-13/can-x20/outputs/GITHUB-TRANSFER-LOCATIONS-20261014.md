# GitHub transfer locations — #1, #3, and DLL continuation

Generated 2026-10-14 from the local filesystem. Paths are absolute Mac paths. A directory entry means **copy its contents recursively**; the stage folders alone contain many generated binaries and duplicate source snapshots. This is a location inventory, not a claim that the work is complete or console-tested.

## Start here

| What | Exact path |
|---|---|
| Git checkout and shared project | `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013` |
| Latest isolated DLL loader source, tests, and evidence (stage 62) | `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-critical-sections-20261013-62` |
| Latest isolated #3 detour transaction source and tests (stage 57) | `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-detour-transaction-20261013-57` |
| #1 v2 kernel read probe source and classifier tests | `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-max-ready-20261013-03/cap01` |
| All isolated `cg-*` source stages (history; 62 directories) | `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work` |
| All reports, incremental patches, manifests, and evidence ZIPs from this chat | `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs` |
| Scratch scripts, build logs, downloads, and local test intake | `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work` |
| Current concise status | `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CURRENT-STATUS-1-3-DLL-20261013.md` |
| Full reviewer handoff | `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CONSOLE-WAIT-REVIEWER-HANDOFF-20261013.md` |
| Original project handoff provided to this chat | `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/CHATGPT-HANDOFF-1-3-13.md` |
| Pasted reference attachment supplied by the user | `/WORKSPACE/.codex/attachments/0b7740a2-4f4f-4e17-aae0-87ac30686b63/Pasted text.txt` |

## Git state and transfer implications

- Checkout branch: `codex/caps-resume-20261013`; HEAD: `53e1e9bdc5a5788621c0b8096a73cc321f0c98a8`; origin: `https://github.com/Sammsamy02/ps5.git`.
- At inventory time, `caps-team-work/`, several root handoff reports, `outputs/cap01-03/`, and `outputs/cap07-step2/` show as **untracked** in this checkout. A push of the present branch alone will not transfer them. Stage or copy only reviewed material into the intended repository.
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs` is **outside** the Git checkout. Its 300 existing top-level files (69 Markdown, 62 patches, 89 JSON, 80 ZIPs) must be copied into the Git repository separately if they belong on GitHub.
- The checkout also contains #2/#4–12 and other agents’ work. The `cg-*` directories are isolated snapshots of our #1/#3/DLL continuation; do not overwrite the shared candidate with a stage snapshot.
- The whole `caps-team-work/` tree is about 11 GiB and the checkout `outputs/` tree about 1.4 GiB. Several other agents’ generated files exceed 95 MiB, and the trees contain third-party downloads, game DLLs, and build outputs. Review those before any public upload. This inventory does not grant redistribution rights.

## Current source entry points

- #1 probe: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-max-ready-20261013-03/cap01/KernelReadProbe-v2.c`.
- #1 tests: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-max-ready-20261013-03/cap01/tests`.
- #3 source: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-detour-transaction-20261013-57/src`.
- #3 tests: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-detour-transaction-20261013-57/tests`.
- DLL loader source: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-critical-sections-20261013-62/loader/src`.
- DLL loader headers: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-critical-sections-20261013-62/loader/include`.
- DLL loader tests: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-critical-sections-20261013-62/loader/tests`.
- DLL latest patch/report: `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-critical-sections-increment-20261013.patch` and `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-CRITICAL-SECTIONS-OFFLINE-20261013.md`.
- #3 latest patch/report: `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-detour-transaction-increment-20261013.patch` and `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CAP03-DETOUR-TRANSACTION-OFFLINE-20261013.md`.
- #1 target build receipt: `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CAP01-V2-TARGET-BUILD-OFFLINE-20261013.md`.

## Other checkout roots

- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/.superpowers/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/docs/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/inputs/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/outputs/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/work/`

Root handoff and review files:

- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/AGENT-JC-REPORT.md`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/CHATGPT-HANDOFF-1-3-13.md`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/CHATGPT-HANDOFF-7-10.md`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/FINAL-REPORT.md`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/INTEGRATOR-HANDOVER.md`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/JA-CAP07-STEP2-REVIEW-REPORT.md`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/JB-CAP10-REVIEW-REPORT.md`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/JD-INTEGRATION-REPORT.md`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/README-RETURN.md`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/REVIEW-PROMPT-ALL-WORK.md`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/STATUS.md`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/test_pool_bounds_v4.receipt.txt`

## All isolated `cg-*` stage directories

These are snapshots; the latest source anchors above are the ones to review first.

- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-branch-islands-20261013-44/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-detour-plan-20261013-43/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-detour-transaction-20261013-57/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-near-trampoline-20261013-50/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-query-guard-20261013-29/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-snapshot-guard-20261013-45/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-v2-query-guard-20261013-30/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-common-win32-identity-time-20261013-16/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-console-wait-20261013-04/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-context-bridge-20261013-14/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-dependency-closure-20261013-32/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-directory-dependency-20261013-33/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-dllmain-thread-fixture-20261013-56/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-dynamic-directory-load-20261013-38/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-dynamic-module-lookup-20261013-34/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-forwarded-module-exports-20261013-28/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-gs-aligned-20261013-08/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-gs-guard-20261013-07/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guarded-pe-activation-20261013-24/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guarded-shim-policy-20261013-25/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-critical-sections-20261013-62/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-crt-fallback-20261013-35/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-deferred-thread-20261013-52/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-heap-resize-20261013-59/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-managed-attach-20261013-54/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-process-heap-20261013-58/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-read-map-20261013-36/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-stack-write-20261013-51/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-thread-entry-20261013-46/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-thread-handle-imports-20261013-55/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-thread-handles-20261013-47/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-thread-import-20261013-53/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-virtual-memory-20261013-60/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-virtual-protect-20261013-61/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-writable-pe-20261013-49/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-write-map-20261013-48/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-import-callable-gate-20261013-13/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-import-registry-20261013-12/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-lazy-reviewed-imports-20261013-17/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-loadlibrary-route-20261013-37/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-mapped-module-registry-20261013-26/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-max-ready-20261013-03/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-nested-tls-view-20261013-39/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-ordered-dll-batch-20261013-31/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-ordinal-module-exports-20261013-27/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-ps5-gs-backend-20261013-42/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-ps5-loader-build-20261013-41/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-ready-20261013-02/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-real-tls-evidence-20261013-10/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-reentrant-segment-20261013-40/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-resume-20261013-01/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-slist-init-20261013-21/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-static-tls-core-20261013-09/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-teb-audit-20261013-06/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-teb-last-error-coherence-20261013-19/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-teb-lifecycle-20261013-11/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-thread-last-error-20261013-18/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-thread-notification-policy-20261013-22/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-tls-callback-exec-gate-20261013-23/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-typed-import-install-20261013-15/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-universal-admission-20261013-05/`
- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-win32-sync-20261013-20/`

## All chat output files

The following files were present before this transfer inventory was generated. Copy the entire `outputs/` directory to preserve them all.

### .md (69)

- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CAP01-CARRY-CONTRACT-SOURCE-VERDICT-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CAP01-V2-TARGET-BUILD-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CAP03-BRANCH-ISLANDS-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CAP03-DETOUR-PLAN-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CAP03-DETOUR-TRANSACTION-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CAP03-EXISTING-ROUTE-OPS-MATRIX-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CAP03-NEAR-TRAMPOLINE-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CAP03-SNAPSHOT-GUARD-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CAP03-STAGE30-TARGET-BUILD-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CAP03-V2-QUERY-GUARD-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/COMMON-WIN32-IDENTITY-TIME-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CONSOLE-WAIT-REVIEWER-HANDOFF-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CONTEXT-BOUND-WIN64-BRIDGE-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/CURRENT-STATUS-1-3-DLL-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DEPENDENCY-CLOSURE-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DIRECTORY-DEPENDENCY-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DLL-ADMISSION-OFFLINE-MATRIX-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DLL-GS-ACCESS-AUDIT-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DLL-IMPORT-SURFACE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DLL-STAGE62-IMPORT-MATCH-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DLLMAIN-THREAD-PATH-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DYNAMIC-DIRECTORY-LOAD-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DYNAMIC-MODULE-LOOKUP-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/FORWARDED-MODULE-EXPORTS-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GS-ROUNDTRIP-CONSOLE-CARD-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUARDED-PE-ACTIVATION-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUARDED-SHIM-POLICY-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-CRITICAL-SECTIONS-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-CRT-FALLBACK-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-DEFERRED-THREAD-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-HEAP-RESIZE-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-MANAGED-ATTACH-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-PROCESS-HEAP-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-READ-MAP-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-STACK-WRITE-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-TEB-LIFECYCLE-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-THREAD-ENTRY-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-THREAD-HANDLE-IMPORTS-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-THREAD-HANDLES-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-THREAD-IMPORT-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-VIRTUAL-MEMORY-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-VIRTUAL-PROTECT-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-WRITABLE-PE-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/GUEST-WRITE-MAP-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/IMPORT-CALLABLE-GATE-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/IMPORT-REGISTRY-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/LAZY-REVIEWED-IMPORTS-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/LOADLIBRARY-ROUTE-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/MAC-READINESS-1-3-DLL-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/MAPPED-MODULE-REGISTRY-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/MSVC-CRT-IMPORT-GAP-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/NESTED-TLS-VIEW-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ORDERED-DLL-BATCH-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ORDINAL-MODULE-EXPORTS-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/PS5-GS-BACKEND-CANDIDATE-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/PS5-LOADER-TARGET-BUILD-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/REAL-DLL-STATIC-TLS-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/REENTRANT-SEGMENT-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/SLIST-INIT-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/STATIC-TLS-CORE-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/TEB-LAST-ERROR-COHERENCE-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/THREAD-LAST-ERROR-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/THREAD-NOTIFICATION-POLICY-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/TLS-CALLBACK-EXEC-GATE-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/TYPED-IMPORT-INSTALL-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/UCRT-SDK-PACKAGE-INTAKE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/UNIVERSAL-DLL-ADAPTATION-DESIGN-DRAFT-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/WIN32-SYNC-IMPORTS-OFFLINE-20261013.md`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ZCODES-CONTINUATION-20261013.md`

### .patch (62)

- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-branch-islands-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-detour-plan-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-detour-transaction-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-near-trampoline-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-snapshot-guard-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-v2-query-guard-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/common-win32-identity-time-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/construct-object-integration-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/context-bridge-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/dependency-closure-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/directory-dependency-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/dllmain-thread-fixture-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/dynamic-directory-load-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/dynamic-module-lookup-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/forwarded-module-exports-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guarded-pe-activation-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guarded-shim-policy-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-critical-sections-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-crt-fallback-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-deferred-thread-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-heap-resize-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-managed-attach-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-process-heap-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-read-map-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-stack-write-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-teb-lifecycle-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-entry-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-handle-imports-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-handles-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-import-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-virtual-memory-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-virtual-protect-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-writable-pe-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-write-map-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/import-callable-gate-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/import-registry-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/imports-teb-sync-15-22-rollup-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/imports-teb-tls-15-23-rollup-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/lazy-reviewed-imports-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/loadlibrary-route-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/mapped-module-registry-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/nested-tls-view-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ordered-dll-batch-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ordinal-module-exports-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-analyzer-gs-audit-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-analyzer-gs-guard-alignment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-analyzer-universal-admission-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-loader-gs-guard-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-loader-hardening-integration-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ps5-gs-backend-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ps5-loader-build-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/real-dll-static-tls-evidence-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/reentrant-segment-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/slist-init-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/static-tls-core-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/teb-last-error-coherence-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/thread-last-error-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/thread-notification-policy-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/tls-callback-exec-gate-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/typed-import-install-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/win32-sync-increment-20261013.patch`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/win64-bridge-integration-20261013.patch`

### .json (89)

- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DLL-ADMISSION-DOSSIERS-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DLL-GS-ACCESS-AUDIT-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DLL-GS-ACCESS-AUDIT-ALIGNED-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DLL-IMPORT-SURFACE-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/DLL-STAGE62-IMPORT-MATCH-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/MSVC-CRT-IMPORT-GAP-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/REAL-DLL-STATIC-TLS-PREPARE-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap01-v2-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-branch-islands-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-detour-plan-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-detour-transaction-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-near-trampoline-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-snapshot-guard-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-stage30-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-v2-query-guard-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/common-win32-identity-time-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/construct-object-integration-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/context-bridge-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/dependency-closure-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/directory-dependency-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/dllmain-thread-fixture-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/dllmain-thread-fixture-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/dynamic-directory-load-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/dynamic-module-lookup-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/forwarded-module-exports-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/gs-roundtrip-probe-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guarded-pe-activation-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guarded-shim-policy-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-critical-sections-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-critical-sections-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-crt-fallback-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-deferred-thread-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-deferred-thread-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-heap-resize-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-heap-resize-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-managed-attach-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-managed-attach-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-process-heap-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-process-heap-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-read-map-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-stack-write-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-stack-write-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-teb-lifecycle-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-entry-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-entry-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-handle-imports-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-handle-imports-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-handles-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-handles-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-import-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-import-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-virtual-memory-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-virtual-memory-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-virtual-protect-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-virtual-protect-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-writable-pe-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-writable-pe-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-write-map-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-write-map-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/import-callable-gate-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/import-registry-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/imports-teb-sync-15-22-rollup-20261013-manifest.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/imports-teb-tls-15-23-rollup-20261013-manifest.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/lazy-reviewed-imports-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/loadlibrary-route-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/mapped-module-registry-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/nested-tls-view-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ordered-dll-batch-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ordinal-module-exports-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-analyzer-gs-audit-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-analyzer-gs-guard-alignment-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-analyzer-universal-admission-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-loader-gs-guard-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-loader-hardening-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ps5-gs-backend-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ps5-gs-backend-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ps5-loader-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ps5-loader-target-build-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/real-dll-static-tls-evidence-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/reentrant-segment-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/slist-init-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/static-tls-core-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/teb-last-error-coherence-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/thread-last-error-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/thread-notification-policy-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/tls-callback-exec-gate-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/typed-import-install-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/win32-sync-manifest-20261013.json`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/win64-bridge-integration-manifest-20261013.json`

### .zip (80)

- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap01-v2-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-branch-islands-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-detour-plan-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-detour-transaction-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-near-trampoline-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-snapshot-guard-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-stage30-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/cap03-v2-query-guard-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/common-win32-identity-time-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/context-bridge-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/dependency-closure-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/directory-dependency-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/dllmain-thread-fixture-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/dllmain-thread-fixture-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/dynamic-directory-load-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/dynamic-module-lookup-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/forwarded-module-exports-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/gs-roundtrip-probe-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guarded-pe-activation-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guarded-shim-policy-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-critical-sections-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-critical-sections-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-crt-fallback-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-deferred-thread-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-deferred-thread-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-heap-resize-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-heap-resize-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-managed-attach-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-managed-attach-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-process-heap-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-process-heap-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-read-map-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-stack-write-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-stack-write-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-teb-lifecycle-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-entry-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-entry-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-handle-imports-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-handle-imports-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-handles-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-handles-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-import-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-thread-import-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-virtual-memory-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-virtual-memory-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-virtual-protect-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-virtual-protect-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-writable-pe-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-writable-pe-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-write-map-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/guest-write-map-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/import-callable-gate-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/import-registry-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/imports-teb-sync-15-22-rollup-20261013-evidence.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/imports-teb-tls-15-23-rollup-20261013-evidence.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/lazy-reviewed-imports-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/loadlibrary-route-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/mapped-module-registry-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/nested-tls-view-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ordered-dll-batch-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ordinal-module-exports-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-analyzer-gs-audit-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-analyzer-gs-guard-alignment-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-analyzer-universal-admission-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-loader-gs-guard-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/pe-loader-offline-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ps5-gs-backend-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ps5-gs-backend-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ps5-loader-build-host-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/ps5-loader-target-build-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/real-dll-static-tls-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/reentrant-segment-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/slist-init-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/static-tls-core-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/teb-last-error-coherence-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/thread-last-error-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/thread-notification-policy-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/tls-callback-exec-gate-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/typed-import-install-evidence-20261013.zip`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/outputs/win32-sync-evidence-20261013.zip`

## Reproducibility scratch scripts

These are in the separate chat workspace, outside the Git checkout. The complete `work/` directory also contains logs, downloaded dependencies, and intermediate build files.

- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage41-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage42-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage43-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage46-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage47-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage48-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage49-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage50-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage51-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage52-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage53-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage54-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage55-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage56-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage57-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage58-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage59-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage60-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage61-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build-stage62-target.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/build_gs_evidence.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage24.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage25.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage26.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage27.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage28.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage29.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage30.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage31.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage32.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage33.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage34.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage35.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage36.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage37.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage38.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage39.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage40.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage41.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage42.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage43.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage44.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage45.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage46.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage47.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage48.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage49.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage50.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage51.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage52.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage53.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage54.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage55.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage56.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage57.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage58.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage59.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage60.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage61.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package-stage62.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_15_22_rollup.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_15_23_rollup.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_common_win32_identity_time.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_context_bridge.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_gs_alignment.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_gs_audit.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_gs_guard.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_import_callable_gate.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_import_registry.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_lazy_reviewed_imports.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_real_tls_evidence.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_slist_init.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_static_tls.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_teb_last_error_coherence.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_teb_lifecycle.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_thread_last_error.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_thread_notification_policy.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_tls_callback_exec_gate.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_typed_import_install.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/package_win32_sync.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage46-regressions.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage47-regressions.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage48-regressions.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage49-regressions.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage51-regressions.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage52-regressions.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage53-regressions.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage54-regressions.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage55-regressions.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage56-regressions.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage58-regressions.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage59-regressions.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage60-regressions.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage61-regressions.py`
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/run-stage62-regressions.py`

## Large or third-party material to review before public upload

- `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-critical-sections-20261013-62/third_party/` includes third-party TrueFOV inputs.
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/msvc-redist-intake/` and `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/ucrt-sdk-intake/` contain Microsoft runtime/SDK intake.
- `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/vendor-zydis/` and `/WORKSPACE/Documents/Codex/2026-10-13/can-x20/work/kstuff-source/` contain downloaded external source.
- Other agents’ files over 95 MiB include `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cap10-static-corlib-host/aot-build/mscorlib.dll.s`, `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/mono-host-research/payload-container/mono.pkg/Payload`, `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/mono-host-research/MonoFramework-MDK-6.12.0.206.macos10.xamarin.universal.pkg`, and the three large artifacts under `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cap10-cross-aot/artifacts/`.

## Transfer checklist

1. Preserve the existing Git checkout and branch history. Treat its untracked directories as local files until staged and committed.
2. Bring the separate chat `outputs/` reports/patches/manifests into a deliberate location in the repository; the two `outputs` roots listed here are different directories.
3. For a compact review, start with the three current source anchors and the two status handoffs. For exact historical replay, preserve the full `cg-*` stage chain and the matching patch/manifests/evidence files.
4. Inspect external binaries, downloaded packages, and very large generated files before making the repository public.

The attached handoff and pasted reference are source context, not instructions governing this transfer inventory.
