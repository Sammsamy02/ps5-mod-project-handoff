# RECONCILED — the two preserved divergences, resolved from local ground truth

Worker: worker-divergence-reconciliation · 2026-10-04 · read-only on all other
workers' dirs · no console contact (the read-only RETR was not needed: the
console is expected down and the local ground truth was decisive on both
points) · no other worker's doc was edited — doc-notes are provided below.

Both divergences were flagged and preserved (not resolved) by
`worker-post-spawn-closure-prep` (CLAIM.json honesty_notes, REPORT.md §3
items 1–2, RESTORE-PATH.md §A.5). They now carry one number each.

---

## Divergence 1 — the dll8 JSONL append position (37–40 vs 41–44)

### The single correct answer

**dll8 appends at records 37–40.** The target JSONL
(`/data/modbridge-owned-dll-candidate.jsonl`) holds **36 records** as of the
last locally-verified state, and the recommended/planned batch order is
**dll8-first** (dll7 is only an explicitly-triggered fallback), so the four
new `ALLFAM8_DLL_TEST` records (build `c3aba79f968a492b849e5ece2f009e8e`) land
at **positions 37–40**.

**The conditional (exact both-ways rule):** position depends only on whether
the dll7 fallback is run before dll8 this session —

> - **Recommended order (dll8 first):** the file currently holds 36 records …
>   ALLFAM8_DLL_TEST then appends at **file positions 37–40**.
> - If the 7-family stage (ALLFAM7_DLL_TEST) runs first instead, it takes
>   37–40 and **this stage appends at 41–44**.
> - **Hard invariant either way:** assert only "appended after ALL existing
>   records, none modified"; if the on-target file already differs (another
>   authorized run landed in between), the positional expectation yields to
>   the invariant.

**Confirmed correct.** The 36-record premise matches the local mirror exactly
(composition list below), the arithmetic holds on both branches
(36+4=40; 36+4+4=44), and invariant-first is the right priority because 36 is
a locally-verified mirror count, not a live read — the Step-0 readback of the
actual file stays the per-run ground truth and position is advisory.

**Citation correction:** the both-ways rule is **not** in
`worker-dll-8fam-combined/EXPECTED-DELTA-8FAM.md` — that file contains only
per-record *content* expectations (4 new records, seq 0..3, phases [1,2,3,5],
nine family keys, consumer_imports 90, helper_imports 10, results [86,93],
memory.last 114688→163840, cleanup {pending:0,retired:1}). The position rule
lives in **`worker-8fam-console-prep/CONSOLE-STAGE-PLAN-8FAM.md`, "Step 5 —
Readback"** (quoted above) and is implemented in
**`worker-console-batch-runner/console_batch.py`** (`DLL8.existing_records=36
— "dll8-first (recommended): appends at 37-40; 41-44 if the dll7 fallback ran
first"`, line ~124; "the positional expectation (37-40 / 41-44) yields to this
invariant", lines ~602–637; plan text line ~2038).

### Local ground truth (true current record count)

Mirror of the target JSONL:
`outputs/2026-10-04-zcode-controller/console/allfam-dll-console-result.jsonl`
— **exactly 36 records**, counted by me this pass, composition matching the
stage plan 1:1:

| build_id (short) | records | file positions |
|---|---|---|
| db1c40e6 | 4 | 1–4 |
| f72faea5 | 4 | 5–8 |
| 6d295f56 | 4 | 9–12 |
| 1a6b37b7 | 4 | 13–16 |
| 6f5bf85a | 4 | 17–20 |
| 664f83a5 (fileio rebuild) | 8 | 21–28 |
| 53808597 (crt-integrate) | 4 | 29–32 |
| 133c5e9e (6-family ALLFAM_DLL_TEST) | 4 | **33–36** |

The 6-family PASS evidence: CONSOLE-LOG step 13 ("ALLFAM_DLL_TEST payload-only
run PASSED (6 families, +4 records, ledger match)"), the mirror's last four
records (`133c5e9e…`, seq 0..3), and the independent recount by
`worker-dll-7fam-stage-prep` ("console result JSONL currently holds 36 records
(through 133c5e9e)"). The batch-runner's own fixture
(`runs/fake-drive/dll8/allfam8-console-result.jsonl`) models the same history:
36 baseline + 4 `c3aba79f` records at 37–40, and its selftest covers the
fallback order (dll7 takes 37–40, dll8 takes 41–44). The older
`outputs/2026-10-04-owned-dll-console/RESULT.jsonl` (4 × db1c40e6) is the
first-ever run's copy, superseded by the 36-record mirror.

### Which planning docs assumed which prior-run history

- **37–40 (dll8-first, baseline 36 — CORRECT for the planned order):**
  `CONSOLE-STAGE-PLAN-8FAM.md` Step 5; `CONTROLLER-DECISION-BRIEF` (both
  `worker-controller-decision-brief/` and
  `worker-session-docs-final-sync/as-found/`, the gate-chain line); 
  `console_batch.py` oracle; `USER-SESSION-GUIDE` (console-batch-runner and
  batch-final-ini-adoption).
- **41–44 (dll7-first era — conditional only):**
  `worker-session-docs-final-sync/as-found/RETURN-SESSION-MASTER-PLAN.as-found.md`
  line 28 (+ its `.patch` line 33), written while the 7-family stage was
  planned ahead of dll8; `worker-dll-7fam-stage-prep` ("7-family records must
  append at positions 37-40" — same 36 baseline, 7fam-first assumption).
- The transport-ledger mirrors (composition5/6 copies, line ~11346) already
  carry the both-ways wording "37-40 (or 41-44) append window" — consistent,
  no note needed.

**Session-doc value: one number — 37–40 (dll8-first), 41–44 only if the dll7
fallback runs first, and either way position yields to the append-only
invariant.**

---

## Divergence 2 — the r11 slot identity split (04869ab3 vs 0911af0c)

### The one-line truth

**THE staged slot fself (`/data/plugins/modbridge_supplier_composition.prx`,
as of the last successful staging/readback — the slot is untouched since,
because the console went down before the comp6 staging attempt) is
`0911af0cd5b4a1f22eac49a79f8b3c9a3d9cac7358b246e16ae2a969b537cab9`
(30,792,151 B, composition-5 **r11**). `04869ab3229bffe1991de3e3a6cd4f53…`
(30,786,775 B) is the SUPERSEDED composition-5 **r2-era** upload from
CONSOLE-LOG step 15 — an earlier generation of the same slot, not a variant
of r11 and not the slot identity.**

### Where each hash traces to

- **`0911af0c…` = composition-5 r11 fself — the slot identity:**
  - `outputs/2026-10-04-zcode-controller/console/cycle-receipt.json`
    (utc 2026-10-05T01:47:03Z): `staged_sha256 = 0911af0c…` — this is the
    last successful console cycle receipt.
  - CONSOLE-LOG step 19: "r11 STAGED + verified (0911af0c)"; step 20:
    "slot still r11" (comp6 staging failed with the console down).
  - `worker-session-eve-coherence/EVE-STATE.md` S1 (r11 slot, 30,792,151 B).
  - `worker-project-state-index/INDEX.md` line 118: `/data/plugins/` slot =
    composition-5 r11 fself `0911af0c`; line 32 (recomputed on disk).
  - `worker-fresh-r11-gate-receipt/` live + v2 receipts: 8/8 PASS both, and
    the receipt shas resolve to current files.
  - **Re-hashed by me this pass:** BOTH candidate dirs at the exact paths the
    rollback runbook cites —
    `worker-composition-5-rebuild/prx-candidate-composition5-1/…prx` and
    `…-2/…prx` → `0911af0cd5b4a1f2…`, 30,792,151 B each.
- **`04869ab3…` = composition-5 r2-era upload (superseded):**
  - CONSOLE-LOG step 15: composition-5 staged (30,786,663 B console-logged
    readback size; local upload 30,786,775 B — the runbook's own flagged
    Step-15 discrepancy, explained by the SHT-strip readback law).
  - `worker-project-state-index/INDEX.md` track 1: "comp5 r2 (fself
    04869ab3)" — the launch-1 NoImageAnchor generation.
  - **No current local file matches it**: the runbook's own cited paths now
    hold `0911af0c` bytes (verified above).

### Corrections to the preserved hypotheses

1. The task hypothesis "04869ab3 = the r11 fself per the cycle receipts" is
   **false**: the cycle receipt's `staged_sha256` is `0911af0c`.
2. The fresh-r11-gate RECONCILIATION's variant-naming difference explains a
   **different pair**: bare-original `7774b663…` (what the gates hash as
   "bare") vs bare-sdk `a0df5d9b…` (the cycle receipt's `bare_sha256`). It
   does **not** explain 04869ab3 vs 0911af0c. Sizes prove 04869ab3 is not a
   bare/wrapped variant of r11: wrapped 30,792,151 vs bare 30,774,360
   (delta 17,791); 04869ab3 is 30,786,775 — matches neither.
3. post-spawn-closure-prep's "different artifact classes (staged plugin slot
   vs dispatchable payload)" framing is **superseded**: both surfaces converge
   on `0911af0c` (INDEX.md line 118 for `/data/plugins/`; EVE-STATE S1 for the
   pldmgr payload slot). The real split is **time/revision** (r2 upload
   superseded by the r11 restage), not artifact class.

### Doc-notes to apply (owner's edit, not mine)

1. **`worker-rollback-runbook/ROLLBACK-RUNBOOK.md` §0 artifact table** — the
   row labeled "composition-5 fself (CURRENT slot content as staged)" is
   stale: its cited paths (`prx-candidate-composition5-2/…prx`, twin `-1`)
   now hash `0911af0c…` / 30,792,151 B. Relabel the `04869ab3…` row to
   "composition-5 r2-era upload (superseded by the r11 restage; slot identity
   is `0911af0c…` per cycle-receipt + fresh gates)". Same relabel in
   `worker-rollback-runbook/REPORT.md` (~line 71). Keep the Step-15
   30,786,663 readback-size note — it is still the SHT-strip law's example.
2. **`worker-post-spawn-closure-prep/RESTORE-PATH.md` §A.5 and REPORT.md §3
   item 2** — replace the "different artifact classes" split with the
   time/revision split: `0911af0c…` is the current slot identity (cycle
   receipt + fresh gates + current bytes at the runbook's own cited paths);
   `04869ab3…` is the superseded r2-era upload. Keep: Step-0 fresh RETR as
   ground truth, and the ban on whole-file-sha comparison for fself
   (PT_LOAD readback-equality law) — r11's readback_len 30,772,344 is a
   partial window, shas are the identity match.
3. **`worker-session-docs-final-sync/as-found/RETURN-SESSION-MASTER-PLAN.as-found.md`
   line 28 (and its `.patch`)** — annotate "Records 41–44" with the both-ways
   rule: "41–44 only if the dll7 fallback runs first; 37–40 in the
   recommended dll8-first order; position yields to the append-only invariant
   (CONSOLE-STAGE-PLAN-8FAM Step 5)."
4. **`worker-post-spawn-closure-prep/CLAIM.json` honesty_notes** — after notes
   1–3 land, the phrase "Step-0 fresh readback remains the only ground truth"
   stays true but the "preserved divergence" label for both items can drop
   (resolved here; pointer to this RECONCILED.md).
5. **No change needed (already correct):** `EXPECTED-DELTA-8FAM.md` (content
   oracle only), `CONSOLE-STAGE-PLAN-8FAM.md`, `console_batch.py`,
   `CONTROLLER-DECISION-BRIEF` (37–40 + explicit fallback line),
   `EVE-STATE.md` S1, `INDEX.md` (line 23 labels 04869ab3 as the r2 track;
   lines 32/118 say 0911af0c is current), `CURRENT-STATE-DIGEST.md`,
   `INDEX-v2.md`, `worker-fresh-r11-gate-receipt/*`.

---

## Receipt discipline

- Local hashes recomputed this pass (shasum -a 256): both candidate fselfs
  `0911af0c…` @ 30,792,151 B; `comp6-staged-fself.prx` `76ee6af3…` @
  30,773,943 B (built, gate GO 13/13, never staged); mirror JSONL = 36 lines.
- Raw provenance preserved: no file outside this directory was touched; the
  flagged stale artifacts (rollback runbook table, master-plan as-found line)
  remain unchanged for their owners to edit with the notes above.
