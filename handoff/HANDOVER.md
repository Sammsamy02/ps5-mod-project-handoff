# Successor handover — 2026-10-14

This export was performed entirely on the Mac. No console upload, launch, network
probe, or game modification was performed to create it. Source and notes are
preserved; builds and console proof are not recreated by this handoff.

## Where to begin

1. Read the source map and retained team handoffs. Prefer the current main tree
   for integration, with team snapshots as separate proposed changes.
2. Review the exported TV UI, its screenshots and tests; integrate web changes and
   asset delivery adapters into the current app without replacing newer native work.
3. Reconstruct pinned dependencies and a portable clean runtime build. The old
   composition drivers depend on omitted artifacts, personal paths and historical
   hash receipts. Regenerate source selections and receipts from this clean tree.
4. Resume C# startup investigation and Stellar Blade prep with an independent
   review before target testing. DLL work is a separate incomplete capability.

## Evidence boundaries and open work

| Area | Preserved work / next step |
| --- | --- |
| UI/app | Controller-first screens, keyboard, plain-language states, per-game installer preview, mock plan/apply/restore contract, static asset bundling, screenshots and tests. Live installer integration, complete Nexus search/images/NXM/cache flows and native PS5 end-to-end proof remain separate work. |
| Runtime | Latest main source overlays and preparation tools plus historical source variants. Frozen binaries and console logs intentionally omitted. Read main prep's build ledger and latest dated notes; earlier status documents describe earlier revisions. |
| #7 step 2 | Independent review/fixes, mount/session ownership source and host regressions in the resume team's `cap07-step2`, `cap07-mac-ready` and related folders. Integration and console claims must be checked against their exact revision; Mac proof is not target proof. |
| #10 C# | Mono owner/provider/port source, patches, bootstrap recipes and owned managed probe source. Later main work attempted PS5 integration; the latest attached handoff reports a C# startup crash and C# switched off for the current launch. No new successful C# target proof is asserted here. Full dependency trees and compiled assemblies were omitted. |
| #1/#3 | Separate kernel/provider and patching work in team/ZCode source snapshots. Historical owner approvals are not permission for a new operator. Preserve the planned three passing tests across restarts before claiming readiness. |
| #13 DLLs | App-side Windows services/execution source and resume-team DLL/guest work, including the late `cg-guest-critical-sections-20261013-62` stage. This is not universal compatibility for downloaded Windows DLLs; target execution, imports, ABI, and framework/game compatibility still require evidence. |
| #9/#11/#12 | Team implementations/review work and main staged source are retained. Last reported material issue and Stellar Blade startup/profile work require the new integrator to check the latest dated documents and source, not assume all capabilities are finished. |

The conversation attached by the owner reports the loader timed out and C# disabled;
that is historical context, not a live console status checked during this export.

## Console work when separately authorized

Only one actor may use the console at a time. Keep modded games offline. Do not
install while a game is running. Review a concrete plan before applying it, verify
exact source/build identities, and keep rollback originals local. Do not upload game
executables from an unsigned or decrypted copy. The older ftpsrv route can decrypt
executables during transfer; an executable backup requires its explicit raw/decrypt-off
mode and verified readback. Preserve the recovery discipline in the original project
without carrying earlier user approvals forward to a new person.

No collaborator was invited by this export; the owner can grant GitHub access to the
successor's account. Private-repository creation does not choose an open-source license.
