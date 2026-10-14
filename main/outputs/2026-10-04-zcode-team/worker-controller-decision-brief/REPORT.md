# REPORT — worker-controller-decision-brief, 2026-10-04

Status: **COMPLETE.** Read-only synthesis; writes confined to this packet
(`CLAIM.json`, `CONTROLLER-DECISION-BRIEF.md`, this REPORT). No console
contact, no nested subagents, no source/tool edits.

## Deliverables

- `CLAIM.json` — claim written first, before any other write.
- `CONTROLLER-DECISION-BRIEF.md` — the deliverable: standing law; phase-by-phase
  controller actions with exact commands (Phase 0 sanity → Phase 1 comp6
  one-pass → Phase 2 dll8/e2e/stage-qdrv1/observer/settings → Phase 3
  game-evidence); per-verdict branch tables for Step A, CP0–CP4, CP5, CP6, CP7
  (success / each failure shape / stale / fail-open), each with decision, next
  command, and abort-vs-adapt; timing discipline table (30 s dispatch gaps,
  ~60 s record window, 15 s dll wait, 5-min StartupWait, 120 s PS-scan and
  injector bounds, 60 s/2 min ridealong windows, 8 s rung ramp at ~20–30 s
  re-runs); the exact user-handoff moments; the session-earned do-not items;
  the fail-open meaning of every status string; and the command cross-check
  table against tools on disk.

## Key findings from the cross-check (corrections to the master plan's state)

1. **console_batch.py freshness gate (GAP 1a/1b) is APPLIED on disk** —
   `FRESH_SESSION_MS_THRESHOLD`/`cp_session_fresh`/`INCONCLUSIVE-STALE` present,
   file sha `92b56889cbfe77ce…`. The master plan still marks it [PENDING].
2. **worker-batch-runner-8fam-patch verification receipts have LANDED**
   (REPORT complete; 162 selftest + 31 fake-drive checks green; DLL8 identity
   re-verified). Master plan prereq 3 is cleared.
3. **`comp6_one_pass.py` is a pre-flight skeleton only** — it hashes the
   scaffold, writes `comp6-one-pass-state.json`, and stops ("awaiting driver/
   gate argv finalization"); it never builds, gates, or stages. Phase 1 must
   follow worker-comp6-driver's ONE-PASS-CHECKLIST §1–4 commands instead.
4. **Comp6 one-pass preconditions P1/P6/P8/P9 verified green on disk** (port
   header edited `0a068bb9…`; bridge r12 sha `feed5254…` matches pin; rehearsal
   leftover already moved; no run-1/candidate dirs) — Phase 1 is runnable as
   soon as the session opens.
5. **Observer staging is still NOT in console_batch.py's stage-qdrv1** — the
   STAGE-PLAN-COMBINED §3–4 manual commands remain the operative path
   ([PENDING] extension preserved).
6. **worker-exec-writability directory is EMPTY** (in flight) — the writability
   decision-tree branch is marked **[PENDING]** in the brief; default rule
   stands (verify hashes before replacing owned files; never touch divergent/
   unowned content).
7. patches.json 51→54 promotion remains PREPARED, NOT APPLIED (unchanged);
   settings package-v2 bytes re-hashed exact (`9570ead3…` / `3afd10ce…`);
   qdrv1 and observer PRX sizes match pins.

## Sources synthesized (all read directly)

- `worker-return-session-master-plan/RETURN-SESSION-MASTER-PLAN.md`
- `worker-menu-visibility-runbook/MASTER-VERIFICATION-RUNBOOK.md` (CP0–CP7, F-signatures, §4.3 a–d, §5.4)
- `worker-console-batch-runner/console_batch.py` (flows, gates, cp01234_verdict,
  cp56_verdict, GE_PROMPTs, CLI, pinned identities) + `USER-SESSION-GUIDE.md`
- `worker-post-launch-tools/post_launch_check.py` (verdicts, exit 0/4, mfa guidance)
- `worker-rung-evidence-reader/rung_check.py` (exit 0/1/2/3/4/5, O2 gate, UNVERIFIED_DRIVER)
- `worker-evidence-freshness-fixes/PATCH-BRIEF.md` (GAP 1a/1b semantics)
- `worker-game-session-ridealongs/STAGE-PLAN-COMBINED.md` (§3–4 manual ridealong, §5 timing, §7 rollback, §8 pending extension)
- `worker-engineversion-ini/NEW-SETTINGS.md` (§2 bytes, §3 lockstep restage)
- `worker-comp6-driver/ONE-PASS-CHECKLIST.md` + `comp6_one_pass.py` (controller dir)
- `worker-batch-runner-8fam-patch/REPORT.md`, `worker-project-state-index/CURRENT-STATE-DIGEST.md`,
  `worker-console-safety/` (crash-1 timing laws)

## Honesty

Everything here is host-side document/tool synthesis. No console behavior is
claimed or predicted as proven; console timings remain [TARGET] until the
session runs. The brief routes failures; it never certifies success.
