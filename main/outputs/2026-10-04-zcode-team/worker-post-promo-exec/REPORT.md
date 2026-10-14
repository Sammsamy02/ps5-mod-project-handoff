# REPORT.md — worker-post-promo-exec, 2026-10-04

## Deliverable

`EXECUTION-GUIDE.md` in this directory: the controller's literal, numbered command
sequence for the 30-minute window from promotion sign-off to Item Spawner evidence
check. 13 steps (0–10 + Z) plus 5 fallbacks (F1–F5), every step with the exact
command, expected output, failure action, and time budget.

## Step map (what was built and from which source)

| Step | Content | Source (verbatim commands/facts) |
|---|---|---|
| 0 | Promotion sign-off: complete PROMOTION-DECISION §4 block into a NEW file (`promotion-signoff-<TAG>.md`) + ledger addendum; C2 (igf6 byte anchors) is the hard signature precondition; HOLD ⇒ stop | PROMOTION-DECISION §2/§4 |
| 1 | Local freeze check: re-hash all 9 manifest files (all 9 full sha256 embedded) + `build-receipt.json` gaps `[]` assertion | MASTER-STAGING-PACKAGE §A/§H; REHEARSAL C1–C9 |
| 2 | Gate A0 STOR write-probe (exactly once, hard STOP) + pre-image RETR/hash of ploader.ini, PRX, loadlist + on-console PRX `.bak` | MASTER-STAGING-PACKAGE §B Step 0; REHEARSAL §6 #0/0b, G1 |
| 3 | mkdir of the 6 activation-tree dirs (5xx-on-existing tolerated; installer Route A preferred if reachable) | §B Step 1 |
| 4 | STOR in fixed order: activation tree (files 3–9) → PRX 2nd-to-last → loadlist LAST | §B Step 2; REHEARSAL C14 |
| 5 | Readback sha256 of all 9 + Gate A1 (readback MUST start `7f 45 4c 46`; `4f 15 3d 1d` = oracle broken ⇒ STOP) + MDTM freshness | §B Step 3, Gate A1 |
| 6 | ploader.ini: backup → local edit → STOR → readback eyeball; exact line `/data/plugins/modbridge_supplier_composition.prx = 2`; no-op path if already present | §C; STAGING-CHECKLIST §2 |
| 7 | Freshness reset: DELE the 3 runtime-created files (module log, UE4SS.log, runtime-status.txt) | §B Step 5 |
| 8 | STEP M — launch request to the user, exact verbatim message (close game → arm ploader → launch → report injection popup → title screen, one launch only) + the mode-2 entry re-send while waiting | §D; RUNTIME-STAGING-PLAN §5 |
| 9 | Evidence collection at T+2min: MDTM + RETR of the 3 logs into `$OUT/zcode-controller/console/` with hashes; Tier A (expected: `module_start_entered`…) vs Tier B (NOT expected with placeholder) vs all-or-nothing/mfa-UNMODDED sanity | §D/§E; RUNTIME-STAGING-PLAN §6 |
| 10 | Item Spawner evidence check: grep mod tag; honest expectation = zero `[TownfallItemSpawner]` lines with the placeholder PRX (wiring state, rehearsal G3); if Tier B appeared, the rung-4 phase-1 bar is the byte-exact IS:746 line exactly once | EVIDENCE-SPEC §4.1–4.3; REHEARSAL §4/§5 G3 |
| Z | Acceptance evaluation: writes `acceptance-verdict-<TAG>.md` running the EVIDENCE-SPEC rung checks with honest scope — NO rung advances from the placeholder launch; rungs 1–3/5 NOT EVALUABLE; rung-4 phase-1 pre-flight graded; harmless-query milestone six conditions stay unevaluated until the recomposed PRX | EVIDENCE-SPEC (all rungs + global non-evidence); INTEGRATION-CHECKLIST STEP 14 |
| F1–F5 | Readback mismatch; decrypt-oracle fail; Gate A0 fail; full rollback block (§F, loadlist master switch first, installer-journal caveat); injection-FAIL triage (one changed attempt max) | §F; FAILURE-TRIAGE references |

Time budget sums to 30 min and is stated per step; the guide instructs stopping (not
compressing verification) when a step doubles its budget.

## Honesty decisions (deliberate, per the operating contract)

- The guide keeps the rehearsal's central finding in view: Gate A0 is the only
  host-unverifiable unknown and is the first live command with a hard STOP.
- Because composition-4 has no PRX (rehearsal G3), the staged PRX is the
  composition-3 placeholder: the guide states explicitly that this launch can
  produce ONLY Tier A evidence, that Tier B / Item Spawner absence is a documented
  wiring state (not a runtime failure), and that Step Z advances no acceptance rung.
- The sign-off (Step 0) is recorded as a NEW file + ledger addendum — the raw
  PROMOTION-DECISION.md is never edited (raw provenance preserved).
- "No crash" is never success; no rung is claimed from drafts or host suites.

## Bounds compliance

No nested subagents; no network/console contact (all commands are documentation for
the controller, not executed); writes confined to
`outputs/2026-10-04-zcode-team/worker-post-promo-exec/` (CLAIM.json first, REPORT.md
last); all five input documents read read-only. 13 tool calls total, within the ≤40
budget.
