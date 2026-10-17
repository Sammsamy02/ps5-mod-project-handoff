# REPORT — worker-tick-correction-propagation (2026-10-04)

Task: sweep every downstream artifact for the retracted tick value `0x37ac8c0` (worker-tick-address-validation
base-convention error) and the corrected runtime Tick `0x33ac8c0` (worker-seed-table-derivation byte-decode
proof), classify, fix live tools in place, leave history intact, draft the ledger annotation.
Mode: read-only audit + bounded one-constant tool fixes. **No console contact, no nested subagents, no model calls.**

## Result summary

- **Sweep:** case-insensitive `37ac8c0` across the whole worktree → **93 occurrences in 49 files**, all under
  `outputs/2026-10-04-zcode-team/`. None in `docs/`, `work/`, other `outputs/` trees, or the runtime zips.
  Full file:line evidence: `raw-occurrences-wrong-value.txt`; classified register: `CORRECTION-REGISTER.md`.
- **Category (a) — live tools with verdict logic: 2 files fixed in place**, each a one-line expected-address
  constant change (`0x37ac8c0` → `0x33ac8c0`), no logic/args touched:
  1. `worker-console-batch-runner/console_batch.py` — 4 lines: `GAME["tick_line"]` (the CP2 expected-evidence
     constant consumed by `classify_l_log` → `cp01234_verdict`), ge.3 step label, CP2 gate text, selftest
     anchor token. Pre-fix lines 197/2289/2292/2553; after a concurrent in-place merge (see below) the same
     constants sit at 197/2735/2738/3005.
  2. `worker-evidence-pipeline-rehearsal/build_fixtures.py:48` — `TICK_LINE` generator constant; its own header
     (line 8) states it mirrors the console_batch GAME dict, so it must follow the correction.
- **Verified clean (no occurrence):** `worker-rung-evidence-reader/rung_check.py` (validates tick *counters*,
  never an address — the task's "rung_check?" question resolves to NO exposure),
  `worker-post-launch-tools/post_launch_check.py` (no address reference),
  `worker-tick-callback-audit/` (directory is empty; its rung-1 C2 shapes survive only as a header reference in
  rung_check.py).
- **Category (b) — documentation-only: 16 register rows (B1–B16)** left unedited, tabulated with required
  corrections in CORRECTION-REGISTER.md — including the originator (`worker-tick-address-validation`
  REPORT.md + LAUNCH2-PREDICTIONS.md), menu-visibility-runbook (incl. the CP2 pass criterion), controller
  decision brief :190, return-session master plan :52, tick-witness-design, and console-batch-runner docs.
  Notable: **B16 — `worker-seed-table-derivation/CLAIM.json:24` still asserts "Tick 0x37AC8C0 (predicted)"**,
  contradicting its own SEED-TABLE correction; flagged to owner/controller (the seed table governs).
- **Category (c) — ledger/history: untouched.** Transport-ledger zcode entries (3 mirror files,
  `tick_predicted` rows :11153 + merged-audit summary :11267), all as-found diff snapshots (incl. the
  freshness-fixes shadow copy, deliberately left as provenance), synthetic rehearsal fixtures, and run
  receipts keep the old value verbatim. The **draft annotation entry the controller adds to the ledger** is in
  CORRECTION-REGISTER.md (not applied by this worker).

## Verification

- Fixed bytes (pre-merge): `console_batch.py selftest` → **162 ok / 0 failed**; `build_fixtures.py` re-run on a
  throwaway `/tmp` copy → 7× corrected tick line, 0 stale (real `fixtures/` untouched on disk).
- Behavioral probes: corrected `cp01234_verdict` vs the committed stale fixture (`0x37ac8c0`) → CP2
  `PARTIAL(Tick line absent — G7)`, overall `INCOMPLETE` — the retracted value no longer grounds a PASS;
  vs regenerated fixtures (`0x33ac8c0`) → **CP2 PASS, overall PASS**. Generator and checker now agree.
- Consequence recorded for the controller: any future `run_rerehearsal.py` re-run must regenerate fixtures
  with the FIXED `build_fixtures.py` first (committed rehearsal fixtures still carry the old value and will
  honestly read CP2 PARTIAL against the corrected tool — do not re-rate them as runtime evidence).
- Post-fix grep of both fixed files: 0 residual `37ac8c0`.

## Concurrency event (reconciled, not expanded into)

At 22:33:46–22:34:56 local, after my fixes were applied and verified, a concurrent in-place merge landed on
`worker-console-batch-runner/console_batch.py` (→3443 lines): freshness gates (`FRESH_SESSION_MS_THRESHOLD`,
`cp_session_fresh`, INCONCLUSIVE-STALE) plus a new `selftest G` (SHT-strip readback law) were merged in,
per its own ownership-transfer header. The merge **preserved the corrected constant** (0 residual wrong
value). Re-verification on the merged bytes: compiles clean; selftest sections A–F **200 ok**, including
`ok game-evidence anchor '0x33ac8c0'`; the newly merged **selftest G crashes**
(`TypeError: can only concatenate str (not "int") to str` — an int `detail` passed to `check(...)`) — that
defect was introduced by the concurrent merge, is unrelated to the tick-address correction, and was left for
its owner/controller per one-constant scope discipline. All behavior probes were re-run on the merged bytes.

## Deliverables (this directory)

- `CLAIM.json` (written first) — scope, non-claims, verification receipts.
- `CORRECTION-REGISTER.md` — full occurrence register (A/B/C), doc correction table B1–B16, draft ledger
  annotation, concurrency addendum.
- `raw-occurrences-wrong-value.txt` — raw grep evidence (93 lines / 49 files).
- Applied fixes (in owners' locations): `worker-console-batch-runner/console_batch.py`,
  `worker-evidence-pipeline-rehearsal/build_fixtures.py`.
- `REPORT.md` (this file, written last).

## Honest limits

- No runtime/console re-verification of `0x33ac8c0` was performed (out of scope; the byte-decode proof is
  worker-seed-table-derivation's).
- Docs (category b) and ledger/history (category c) were intentionally not rewritten; until their owners or
  the controller apply the table, those documents still display the retracted value.
- The post-merge `selftest G` crash in `console_batch.py` is owned by the concurrent merge, not this worker;
  until it is fixed, `console_batch.py selftest` does not run to completion (sections A–F all pass).
