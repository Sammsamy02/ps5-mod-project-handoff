# CORRECTION-REGISTER — Tick address base-convention error (0x37ac8c0 → 0x33ac8c0)

Worker: worker-tick-correction-propagation · 2026-10-04
Corrected fact being propagated (worker-seed-table-derivation, byte-decode proof):
**runtime `UGameEngine::Tick` = `0x33ac8c0`** (linked `0x2fac8c0`; linked `0x33ac8c0` decodes mid-instruction and contains `ud2`).
The circulated value **`0x37ac8c0`** ("= base `0x400000` + linked `0x33ac8c0`") was a base-convention
error introduced by worker-tick-address-validation's launch-2 prediction: the recorded `0x33ac8c0`
was already a RUNTIME address.
Expected console evidence line: `GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x33ac8c0)`.

Sweep: case-insensitive `37ac8c0` across the whole worktree → **93 occurrences in 49 files, all under
`outputs/2026-10-04-zcode-team/`** (none in `docs/`, `work/`, other `outputs/` trees, or archives).
Raw grep evidence: `raw-occurrences-wrong-value.txt` (this directory).

---

## Category (a) — LIVE TOOLS with verdict logic — FIXED IN PLACE

| # | File:line (pre-fix) | Role | Fix applied |
|---|---|---|---|
| A1 | `worker-console-batch-runner/console_batch.py:197` | `GAME["tick_line"]` — the CP2 expected-evidence constant consumed by `classify_l_log`/`cp01234_verdict` (CP2 PASS/FAIL verdicts) | `0x37ac8c0` → `0x33ac8c0` (both occurrences in the string) |
| A2 | `worker-console-batch-runner/console_batch.py:2289` | ge.3 step label (fed the selftest "game-evidence anchor" token check at :2553) | same constant |
| A3 | `worker-console-batch-runner/console_batch.py:2292` | CP2 gate text shown to the operator | same constant |
| A4 | `worker-console-batch-runner/console_batch.py:2553` | selftest anchor token list (must match A2 or selftest fails) | same constant |
| A5 | `worker-evidence-pipeline-rehearsal/build_fixtures.py:48` | `TICK_LINE` — synthetic-evidence generator; its header (line 8) states it mirrors the console_batch GAME dict. Left stale it would fabricate future "success" logs the corrected checker rejects | `0x37ac8c0` → `0x33ac8c0` |

Each fix is a one-line expected-address-constant change; no logic, args, or control flow touched.

Verification (all host-only, zero console contact):
- `python3 worker-console-batch-runner/console_batch.py selftest` → **162 ok, 0 failed**.
- Corrected `cp01234_verdict` vs committed s01 fixture (still carries `0x37ac8c0`) → CP2 `PARTIAL(engine-ready yes, Tick line absent — G7)`, overall `INCOMPLETE` — the stale value no longer grounds a PASS.
- `build_fixtures.py` re-run on a throwaway `/tmp` copy → fresh fixtures carry 7× corrected tick line, 0 stale; same verdict path → **CP2 PASS, overall PASS**. Real `fixtures/` and `runs/` untouched.
- Post-fix grep of both files: 0 remaining `37ac8c0`.

Verified CLEAN (no occurrence, no fix needed):
- `worker-rung-evidence-reader/rung_check.py` — validates tick **counters** (`ticks_accepted` strictly increasing, refusals), never a Tick address constant.
- `worker-post-launch-tools/post_launch_check.py` — no address reference.
- `worker-tick-callback-audit/` — directory is empty (0 files); its rung-1 C2 shapes live on in `rung_check.py`'s header. Nothing to sweep.

Follow-up for the controller (consequence of A1+A5): a future `worker-evidence-freshness-fixes/run_rerehearsal.py`
re-run loads the live `console_batch.py` bytes and the COMMITTED rehearsal fixtures; the fixtures still carry
`0x37ac8c0`, so `cp_checks_*` would honestly read CP2 PARTIAL / INCOMPLETE. Regenerate fixtures with the FIXED
`build_fixtures.py` first (verified to produce PASS) — do not "re-rate" the stale fixtures as if the runtime were wrong.

---

## Category (b) — DOCUMENTATION-ONLY — correction table (NOT edited; owners/controller decide)

| # | File:line | Wrong statement (summary) | Required correction |
|---|---|---|---|
| B1 | `worker-tick-address-validation/REPORT.md:33` | "Predicted derivation result: `0x400000 + 0x33ac8c0 = 0x37AC8C0`" | Prediction withdrawn: `0x33ac8c0` is the runtime address; linked is `0x2fac8c0` |
| B2 | `worker-tick-address-validation/REPORT.md:91` | "The 0x37AC8C0 prediction is derived from recorded facts…" | Same withdrawal; superseded by SEED-TABLE byte decode |
| B3 | `worker-tick-address-validation/REPORT.md:35` | "if … records `0x33ac8c0` as a RUNTIME address, it is below the exec span start" | That is in fact the case; the exec-span window check must use runtime `0x33ac8c0` directly |
| B4 | `worker-tick-address-validation/LAUNCH2-PREDICTIONS.md:16,19,24` | Route A/B compute runtime = base + `0x33ac8c0` = `0x37AC8C0` | Drop the +base step; expected scan/vtable value is `0x33ac8c0` |
| B5 | `worker-tick-address-validation/LAUNCH2-PREDICTIONS.md:39,40` | Outcome rows 1–2 expect logged/patched `0x37AC8C0` | Expect `0x33ac8c0` |
| B6 | `worker-tick-address-validation/LAUNCH2-PREDICTIONS.md:56` | Launch-2 acceptance "(b) logged scan/vtable Tick addresses equal to `0x37ac8c0`" | Acceptance literal must be `0x33ac8c0` |
| B7 | `worker-menu-visibility-runbook/CLAIM.json:13,40` | Cites "tick predicted 0x37AC8C0" / "linked 0x33ac8c0 + base 0x400000" | Cite corrected runtime `0x33ac8c0` (SEED-TABLE) |
| B8 | `worker-menu-visibility-runbook/REPORT.md:23,65` | "prediction 0x37ac8c0" / "0x37ac8c0 is a structural prediction only" | Replace with corrected value; the byte-compare gap (G3) is now CLOSED by SEED-TABLE |
| B9 | `worker-menu-visibility-runbook/MASTER-VERIFICATION-RUNBOOK.md:22,178,179,449,480` | CP2 pass criterion + triage row keyed to `0x37ac8c0` | Pass criterion = both logged addresses equal `0x33ac8c0` (runtime; linked `0x2fac8c0` — drop "+ base 0x400000" phrasing at :180) |
| B10 | `worker-controller-decision-brief/CONTROLLER-DECISION-BRIEF.md:190` | CP2 decision-table row expects `…vtable: 0x37ac8c0; scan: 0x37ac8c0` | Expect `0x33ac8c0` (decision logic unchanged) |
| B11 | `worker-return-session-master-plan/RETURN-SESSION-MASTER-PLAN.md:52` | Launch-2 success picture "+ Tick `0x37ac8c0`" | "+ Tick `0x33ac8c0`" |
| B12 | `worker-console-batch-runner/REPORT.md:14` | Game-evidence flow prose "engine-ready + Tick `0x37ac8c0`" | Prose now lags the fixed tool; update to `0x33ac8c0` |
| B13 | `worker-console-batch-runner/USER-SESSION-GUIDE.md:79` | User-facing CP description "+ Tick `0x37ac8c0`" | Same |
| B14 | `worker-tick-witness-design/REPORT.md:98` | "The predicted tick value 0x37AC8C0 remains tick-address-validation's…" | Witness design should target runtime `0x33ac8c0` |
| B15 | `worker-tick-witness-design/TICK-WITNESS-DESIGN.md:376` | "prediction 0x400000 + 0x33ac8c0 = 0x37AC8C0 available as a cross-check" | Cross-check constant is wrong; use `0x33ac8c0` |
| B16 | `worker-seed-table-derivation/CLAIM.json:24` | "Validated in-game facts: … Tick 0x37AC8C0 (predicted)" | INTERNAL INCONSISTENCY: this correcting worker's own claim line still carries the retracted value while `SEED-TABLE.json` (correction_recorded) and `SEED-INTEGRATION.md:261` state runtime `0x33ac8c0`. Flag to owner/controller; the seed table governs. |

Correct-by-design (no action): `worker-seed-table-derivation/REPORT.md:44`, `SEED-TABLE.json:98`,
`SEED-INTEGRATION.md:261` cite `0x37AC8C0` only to refute it. `worker-tick-address-validation/CLAIM.json:32`
refers to "the 0x33ac8c0 Townfall Tick address", which is correct under the corrected convention.

---

## Category (c) — LEDGER / HISTORICAL RECORDS — NEVER REWRITE; annotation entry below

| Group | Files:lines (occurrences of `37ac8c0`) |
|---|---|
| Transport-ledger zcode entries (mirrors) | `worker-composition-5-rebuild/transport-ledger.composition5-mirror.json:11153,11267` · `worker-comp6-driver/transport-ledger.composition6-mirror.json:11153,11267` · `worker-comp6-drybuild/transport-ledger.composition5-mirror.json:11153,11267` (`"tick_predicted": "0x37AC8C0 runtime (= base 0x400000 + linked 0x33ac8c0 …)"` and the merged-audit summary `engine-ready+Tick@0x37ac8c0`) |
| As-found diff snapshots (provenance) | `worker-batch-runner-8fam-patch/diffs/console_batch.py.orig:118,1626,1629,1842` · `worker-batch-runner-8fam-patch/diffs/USER-SESSION-GUIDE.md.orig:57` · `worker-evidence-freshness-fixes/diffs/console_batch.py.asfound-c691cdd4cb8b.py:141,1903,1906,2162` · `worker-evidence-freshness-fixes/shadow/console_batch.py:141,1968,1971,2227` (preserved freshness-patch reference copy — deliberately not fixed) · `worker-observer-staging-extension/diffs/console_batch.py.asfound-92b56889cbfe.py:141,1969,1972,2233` · `worker-observer-staging-extension/diffs/USER-SESSION-GUIDE.md.asfound:79` |
| Run receipts / selftest transcripts | `worker-batch-runner-8fam-patch/receipts/selftest-out.txt:474,477,950,953,1564,1567,2038,2041,2485,2488,2568` · `receipts/plan-all-dryrun.txt:405,408` · `receipts/plan-all-dll7-dryrun.txt:405,408` · `receipts/fake-drive-out.txt:164` · `worker-console-batch-runner/receipts/HOST-TEST-RECEIPT.md:16` · `worker-evidence-pipeline-rehearsal/REHEARSAL-RECEIPTS.md:139` |
| Synthetic rehearsal fixtures (recorded inputs) | `worker-evidence-pipeline-rehearsal/fixtures/s01_full_success/{from-dir/UE4SS.log:11, from-dir/l_t1_UE4SS.log:11, from-dir/l_t2_UE4SS.log:11, from-dir/l_t3_UE4SS.log:11, console-tree/data/plugins/UE4SS.log:11}` · `fixtures/s04_script_error_trap/{from-dir/UE4SS.log:11, console-tree/data/plugins/UE4SS.log:11}` |
| Rehearsal/selftest run outputs (recorded evidence) | `worker-evidence-pipeline-rehearsal/runs/s01_full_success/post_launch_check{,.baselinefile}.json.txt:42` · `runs/s04_script_error_trap/post_launch_check{,.baselinefile}.json.txt:42` · `worker-evidence-freshness-fixes/runs/s01_full_success/post_launch_check{,.baselinefile}.json.txt:42` · `runs/s04_script_error_trap/post_launch_check{,.baselinefile}.json.txt:42` · `worker-console-batch-runner/runs/fake-drive/game/{L-UE4SS.log:5, L-after-OPTIONS.log:5, L-after-CROSS.log:5, events.log:11}` |

These are records of what was believed/generated at the time. The `fixtures/` and fake-drive logs are
SYNTHETIC (built by `build_fixtures.py` from the then-current GAME dict), not console evidence; they stay
byte-identical as provenance. None of them may ground a launch-2 PASS after this correction.

---

## DRAFT — ledger annotation entry (for the CONTROLLER to add; this worker did NOT apply it)

> **[2026-10-04] TICK-ADDRESS CONVENTION CORRECTION — supersedes, without rewriting, the rows below.**
> Entries recording runtime `UGameEngine::Tick` as `0x37AC8C0` ("= base `0x400000` + linked `0x33ac8c0`",
> from worker-tick-address-validation's launch-2 prediction) — including `tick_predicted` rows at
> `transport-ledger.composition5-mirror.json:11153` and the merged-audit summary
> `engine-ready+Tick@0x37ac8c0` at `:11267` (identical text mirrored in comp6-driver
> `transport-ledger.composition6-mirror.json` and comp6-drybuild `transport-ledger.composition5-mirror.json`) —
> are SUPERSEDED by worker-seed-table-derivation's byte decode (`SEED-TABLE.json` → `correction_recorded`):
> linked `0x2fac8c0` carries the canonical prologue `554889e54157415641554154534881ec` (file_off `0x2fb08c0`),
> while linked `0x33ac8c0` decodes mid-instruction (contains `ud2`). Therefore
> **runtime Tick = `0x33ac8c0`** (linked `0x2fac8c0`); the earlier `0x33ac8c0` record was already a RUNTIME address.
> Expected console line: `GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x33ac8c0)`.
> Actions (worker-tick-correction-propagation): fixed in place `worker-console-batch-runner/console_batch.py`
> (expected-address constant, 4 lines; selftest 162 ok / 0 failed) and
> `worker-evidence-pipeline-rehearsal/build_fixtures.py` (`TICK_LINE`); regenerated fixtures PASS CP0-CP4,
> stale fixtures honestly read CP2 PARTIAL. Historical ledger rows, as-found snapshots, rehearsal fixtures and
> run receipts keep the old value verbatim as provenance — do not re-rate them as console evidence and do not
> regenerate over them. Doc-only occurrences: see
> `outputs/2026-10-04-zcode-team/worker-tick-correction-propagation/CORRECTION-REGISTER.md` (category b).

---

## ADDENDUM — concurrent in-place merge observed during this assignment (reconciliation note)

At 22:33:46–22:34:56 local, WHILE this worker's fixes were already applied and verified, an in-place
merge landed on `worker-console-batch-runner/console_batch.py` (3443 lines now): the header documents
in-place ownership transfer (worker-batch-runner-8fam-patch → worker-evidence-freshness-fixes
"freshness gate + CP0 fallback label" → worker-observer-staging-extension), and the freshness gates
(`FRESH_SESSION_MS_THRESHOLD`, `cp_session_fresh`, INCONCLUSIVE-STALE labels — previously only in the
preserved shadow copy) plus a new `selftest G: SHT-strip readback law` were merged into the live tool.
The merge PRESERVED this worker's corrected constant (0 residual `37ac8c0`; corrected value now at
lines 197, 2735, 2738, 3005). Re-verification on the merged bytes:
- Sections A–F: **200 ok**, including `ok game-evidence anchor '0x33ac8c0'` and all CP2 gate texts printing the corrected line; file compiles clean.
- The newly merged `selftest G` crashes with `TypeError: can only concatenate str (not "int") to str`
  (an int `detail` passed to `check(...)`) — introduced by that concurrent merge, UNRELATED to the
  address correction; flagged for its owner/controller. Not touched by this worker (scope discipline:
  one-constant fixes only).
- Behavior probes re-confirmed on merged bytes: committed stale fixture (0x37ac8c0) → CP2 PARTIAL,
  overall INCOMPLETE (tick_line not detected) — the retracted value still grounds no PASS.

Register line numbers for A2/A3/A4 above therefore shifted (2289/2292/2553 → 2735/2738/3005); the
constant and the verdict logic consuming `GAME["tick_line"]` are unchanged by the merge.

---

*End of register. Raw evidence: `raw-occurrences-wrong-value.txt`.*
