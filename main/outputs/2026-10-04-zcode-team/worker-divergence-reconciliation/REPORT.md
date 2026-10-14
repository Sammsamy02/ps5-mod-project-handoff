# REPORT — worker-divergence-reconciliation (2026-10-04)

## Status: COMPLETE

Both divergences flagged by worker-post-spawn-closure-prep are resolved from
local ground truth. Session docs now have one number/identity each, delivered
as ready-to-apply doc-notes (no other worker's file was edited).

## Divergence 1 — dll8 JSONL append position: **37–40**

- True locally-known baseline: **36 records** in
  `/data/modbridge-owned-dll-candidate.jsonl`, verified from the controller
  mirror `outputs/2026-10-04-zcode-controller/console/allfam-dll-console-result.jsonl`
  (counted this pass; composition db1c40e6×4, f72faea5×4, 6d295f56×4,
  1a6b37b7×4, 6f5bf85a×4, 664f83a5×8, 53808597×4, 133c5e9e×4 = 36; the
  6-family PASS run `133c5e9e` holds positions 33–36, corroborated by
  CONSOLE-LOG step 13 and worker-dll-7fam-stage-prep's independent recount).
- 41–44 docs (RETURN-SESSION-MASTER-PLAN) assumed the dll7-first era;
  37–40 docs (CONSOLE-STAGE-PLAN-8FAM, CONTROLLER-DECISION-BRIEF,
  console_batch.py) assume the recommended dll8-first order. Single correct
  answer: **37–40 (dll8-first); 41–44 only if the dll7 fallback runs first.**
- Both-ways rule confirmed correct; citation corrected: it lives in
  `worker-8fam-console-prep/CONSOLE-STAGE-PLAN-8FAM.md` Step 5 and
  `worker-console-batch-runner/console_batch.py` ("positional expectation …
  yields to the append-only invariant"), **not** in EXPECTED-DELTA-8FAM.md
  (content oracle only).
- Fixture cross-check: `runs/fake-drive/dll8/` = 36 baseline + 4 `c3aba79f`
  records at 37–40; selftest covers the fallback order.

## Divergence 2 — r11 slot identity: **0911af0c is THE staged slot fself**

- `0911af0cd5b4a1f2…` (30,792,151 B) = composition-5 **r11** fself:
  cycle-receipt `staged_sha256` (utc 2026-10-05T01:47:03Z), CONSOLE-LOG step
  19/20 ("slot still r11"), EVE-STATE S1, INDEX.md line 118, fresh-r11 gates
  8/8 both — and re-hashed by me on BOTH candidate dirs at the exact paths the
  rollback runbook cites. The console went down before comp6 staging, so this
  is the last successful slot state.
- `04869ab3…` (30,786,775 B) = the superseded composition-5 **r2-era** upload
  (CONSOLE-LOG step 15; INDEX track "comp5 r2"). Not a bare/wrapped variant of
  r11 (sizes match neither wrapped 30,792,151 nor bare 30,774,360) and no
  current file matches it.
- Task-hypothesis corrections: the cycle receipt says 0911af0c (not 04869ab3);
  the fresh-r11 variant-naming note explains bare-original `7774b663` vs
  bare-sdk `a0df5d9b` — a different pair. post-spawn's "different artifact
  classes" framing is superseded by the real time/revision split.

## Deliverables

- `CLAIM.json` (written first) — full claim + evidence map
- `RECONCILED.md` — both answers, citations, exact doc-notes for the owners of
  worker-rollback-runbook, worker-post-spawn-closure-prep, and the as-found
  master plan; explicit "no change needed" list for already-correct docs
- `REPORT.md` (this file, last)

## Honest limits

- 36 records is the last locally-verified count; the Step-0 readback at the
  console session remains the per-run ground truth (the both-ways rule's
  invariant-first clause already encodes this).
- The slot identity is as of the last successful staging/readback; the live
  slot can only be confirmed by the Step-0 fresh RETR when the console is
  back (it may hold comp6 `76ee6af3` only after a future successful staging —
  none occurred).
- No console contact was made (no RETR attempted; local ground truth was
  decisive on both points). No nested subagents. No files outside
  `worker-divergence-reconciliation/` were created or modified.
- Tool budget: ~20 calls, within the 20–40 bound.
