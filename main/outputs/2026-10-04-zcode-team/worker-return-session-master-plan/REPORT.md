# REPORT — worker-return-session-master-plan

Worker: worker-return-session-master-plan, 2026-10-04. Read-only synthesis; NO console contact;
no nested subagents. Deliverables: CLAIM.json (first), RETURN-SESSION-MASTER-PLAN.md (89 lines,
under the 120 cap), this REPORT.md (last). Workspace root: `.codex/worktrees/translation-core/what`.

## What was produced

ONE linear timeline of the user's return session in RETURN-SESSION-MASTER-PLAN.md:
Phase 0 sanity (game CLOSED, ploader.ini pre-image sha `8d331b3f…` vs INDEX Track 8) →
Phase 1 comp6 one-pass staging (`comp6_one_pass.py`: ODR pre-flight → build → gate v2 →
stage via `stage_composition5_cycle.py`, never launches) → Phase 2 game-closed batch, run as
SEPARATE subcommands (`dll8` → `e2e` → `stage-qdrv1` → manual observer SC-3/4/5 → manual
settings+loadlist restage) → Phase 3 game session (`game-evidence` gate: "arm ploader" (wait
OK popup 1–60 s, HTTP OK ≠ armed) → "launch Townfall" → CP0–CP4 gate → "load save" →
"press OPTIONS once" → visible-menu confirmation → navigate → "CROSS to confirm spawn" →
visible-pickup confirmation) → abort/rollback pointers (tool abort semantics, ploader.ini
`.bak`/`=0`, settings old-pair restore, runbook §6 triage) → time-and-effort table
(~75–125 min total; 7 required user actions + 2 eye-confirmations + 1 optional) →
prerequisites with in-flight owners → command cross-check.

## Key findings from the cross-check (task 4)

1. **8fam patch is already IN the tool**: `console_batch.py` contains `dll8` (sha `79281191…`,
   336552 B) and `all` = dll8-first; the patched guide text (Phase 1 still says "dll7") is the
   pre-patch edition on disk — the plan uses dll8 and cites the patch CLAIM. Verified in
   `worker-console-batch-runner/console_batch.py` (CLI parser + DLL8 block lines 79-95).
2. **Freshness fixes are PARTIAL**: `rung_check.py` has the O2 gate (`FRESH_SESSION_MS_THRESHOLD`,
   exit 5 INCONCLUSIVE-STALE) and `post_launch_check.py` carries GAP2/3 edits, but the
   console_batch.py GAP 1a/1b freshness gate is NOT applied — worker-evidence-freshness-fixes
   status=in_progress, receipts/ empty, promised patch+brief absent from diffs/. Marked
   [PENDING worker-evidence-freshness-fixes].
3. **Observer ridealong is NOT tool-integrated**: `stage-qdrv1` stages qdrv1 only
   (`modbridge_qh_driver.prx`, no observer steps). STAGE-PLAN-COMBINED §8's SC-1..SC-8 superset
   supersedes it on paper but was never implemented. Plan therefore splits Phase 2 into
   subcommands and gives the observer steps as the plan's own verbatim §3–4 commands, marked
   [PENDING stage-qdrv1 extension]. Running `all` in one shot would reach the launch gate
   before the observer staging and settings restage — the plan explicitly says so.
4. **Settings restage bytes READY**: `worker-engineversion-ini/package-v2/` re-hashed on disk —
   834 B `9570ead3…` settings + 446 B `3afd10ce…` loadlist, exactly matching NEW-SETTINGS.md.
   The restage is manual (curl) per NEW-SETTINGS §3; console_batch.py has no restage step.
5. **comp6 not yet built/staged**: `comp6_one_pass.py` exists (controller dir) with driver +
   gate v2 + 8-file scaffold on disk, but the staged comp6 PRX does not exist; the plan treats
   Phase 1 as the session's first act. New since the digest: worker-required-slots-feasibility
   REPORT landed with a decisive "ps_scan structurally dead" verdict (5 stacked gates;
   slots 0/3 constant-only, 2/4 need bypass, 6 solved by the settings override) — its §4b/§5
   diff set is the still-unbuilt piece of comp6. Marked [PENDING controller one-pass run].
6. **E2E promotion ambiguity kept honest**: patches.json 51→54 is PREPARED, NOT APPLIED
   (PROMOTION-PREP.md line 1); the digest lists apply + ACK-exit-6 amendment ahead of the E2E
   run. The plan keeps the tool's own 13/13+payload preflight as the operative gate and marks
   the apply [PENDING worker-e2e-suite-completion].
7. **Batch-runner receipts**: 8fam patch CLAIM says complete but its receipts/ dir is empty
   (also flagged by the freshness worker as mid-flight). The tool code is present and pinned;
   the missing piece is the receipt, not the code.

## Safety incident during synthesis (disclosure)

Running `stage_composition5_cycle.py --help` did NOT print help — the script has no argparse
gate and executed its pipeline: build → determinism → gate GO → upload attempt, where curl
failed with exit 7 (console ps5-host-not-configured.invalid:2121 unreachable). No console contact was established
and nothing on console changed (upload never connected). All subsequent inspection was static
(Read/grep/sed). No other tool was executed.

## Honesty boundaries

No console contact, no execution claims, no rung advancement. Every command in the plan was
cross-checked against a file on disk (cited in the plan's cross-check section); three commands
reference not-yet-landed artifacts and are marked [PENDING <owner>]. Timings are host-side
estimates (dry-build logs ≈ 7–8 min per build+gate cycle; dispatch-gap law ≥30 s; game boot
~5 min per the digest ritual); console behavior remains [TARGET] until the user-authorized
session runs.
