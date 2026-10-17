# REPORT — worker-runbook-eighth-sink (2026-10-04)

Task: sync `worker-menu-visibility-runbook/MASTER-VERIFICATION-RUNBOOK.md` (the CP0-CP7
evidence procedure) to the adopted ninth-slot law — cc1 as the EIGHTH evidence sink + the
ge.3c census oracle — then verify the runbook's remaining cites. Documentation-only; no
console contact; no nested subagents. 20-40 tool calls budget: used 25.

## What was done

1. **Read the three required sources.** The runbook as-found (sha `e362dc98…`, 519 lines);
   the adopted law in `worker-console-batch-runner/console_batch.py` rev `456f9372` (CC1 dict
   :243-269, `carrier_evidence_verdict` :3647-3775, ge.2 eight-sink fetch :3821-3849, ge.3b
   :3897-3910, ge.3c step :3928-3946, Flow WARN/FAIL + stop-on-failure semantics :1813-1856);
   the cc1 `STAGE-PLAN.md` record shapes (§5-§7, §9). Cross-checked the ninth-slot adoption
   REPORT, the prior cheatsheet-ninth-sync REPORT, the tick CORRECTION-REGISTER (B9), and the
   ue4ss-docs FIDELITY-REPORT §1.4.
2. **Synced the runbook in place** (9 edits, structure preserved, edit log with per-edit
   detail): header REV banner; merged-sources table (stale 0x37AC8C0 row fixed + 3 new source
   rows); §1 retitled "the eight sinks" with X/F/CC rows and the full cc1 census grammar
   (envelope, success sequence, census_match fields, CENSUS:VALIDATED PASS shape with
   controls/negative-hits/summary oracle, CENSUS:REFUSED shape with every reason code incl.
   grace class, the ONE `CC1:` L line, WARN-on-refusal rule); §2 census-window timing, new
   Step C (tool ge.2/ge.3b/ge.3c flow + PASS/WARN/FAIL semantics + never-blocks-CP5), 3 new
   fallback curls; §3 new CP4b evidence-only row between CP4 and CP5; CP2 + G3 tick
   parentheticals corrected per B9; T9 added to the pending-tests ledger.
   New sha `648a5c89…` (612 lines). As-found copy + both hashes preserved in `as-found/`.
3. **Task-2 drift sweep.** Tick constant and §4.3a v3 tripwire were already fixed by the
   prior pass (mtime evidence: the runbook was edited ~70 s after FIDELITY-REPORT.md) —
   verified current. Found and fixed TWO residual drifts the prior pass missed:
   (a) the base-convention derivation parentheticals ("linked `0x33ac8c0` + base `0x400000`")
   survived at CP2 and G3 even though the constants were corrected — CORRECTION-REGISTER B9
   explicitly ordered that phrasing dropped; (b) §1's L entry-failure grammar still said
   "no timestamp" though entry r4 prefixes `[t=<monotonic ms>ms pid=<pid>] ` — fidelity
   item 3, source-verified at `runtime_module_entry.c:198-202`. Also fixed the merged-sources
   row still citing the retracted `0x37AC8C0`. Everything else re-verified CURRENT: error-code
   table (r13 STOP_EXN renumber does not touch it), D1 tags, status ladder, rung baselines,
   §4/§5 mod-line shapes, §6 triage, G1/G2/G4-G7.

## Key findings for the controller

- **The "eight sinks" counting is S, L, D1, D2, C2, X, F, CC** (pre-ninth-slot tool said
  "seven"; cc1 is the eighth). ge.2 itself fetches SEVEN files (S, L, D1, C2 + the three
  probe JSONLs); D2 (observer log) is covered by the staging flow's baseline/absent check —
  the runbook now states this exactly, so no one looks for a D2 curl inside ge.2.
- **ge.3c semantics encoded honestly:** WARN (CENSUS:REFUSED, zero fresh records, incomplete
  sequence, write_failures≠0, ≠1 `CC1:` line) never halts and never blocks CP5 — the tool
  gates the OPTIONS handoff on CP0-CP4 green only. FAIL is structural only (unparseable /
  ≥1 KiB / foreign build_id) and halts via stop-on-first-failure. A census_truncated:1
  VALIDATED verdict is a BOUNDED census. The census evidences which of the 53 fixed names
  are interned in the LIVE FName pool for that launch only.
- **Cross-doc tension FLAGGED, not edited (owner's file):** `post_launch_check.py`'s
  `FAILURE_LINE_RE` anchors `^modbridge module_start failed:` WITHOUT the entry-r4 timestamp
  prefix the l11-a entry emits — if the console emits the r4 shape, the classifier may miss
  failure lines. Same discipline as the cheatsheet-ninth-sync worker: flagged in the runbook
  §1 grammar note + here; the tool owner decides.
- **Fidelity CP2 note preserved as an observation:** under the armed seed path the scan arm
  is skipped; the both-addresses-equal pass criterion is kept because the live ge.3 still
  pins `GAME["tick_line"]` with both slots equal — resolution belongs to the CP2 owner.
- **Historical provenance untouched:** runbook worker's own CLAIM.json/REPORT.md still cite
  the retracted 0x37ac8c0 (register category b, B7/B8) — owner's records, not rewritten.

## Honesty boundaries

- Nothing has executed on the console; every cc1/census shape added is a prediction from the
  STAGE-PLAN + 24/24 host tests. No rung advanced, no pending test marked done; T9 records
  the census as console-only. No completion or >8-quality claim.
- All constants were read back from the pinned sources this run (tool rev `456f9372` cited by
  the cheatsheet-ninth-sync worker as live-hashed; CC1/XWP/FC1POOL identities re-read from
  the dict during this sync).
- Deliverable order kept: CLAIM.json first, this REPORT.md last.
