# REPORT — worker-evidence-gap-fix (2026-10-04, LOCAL ONLY)

Scope: took ownership of `../worker-launch10-evidence-upgrade/evidence.sh` (+ its
host-test suite) and fixed the three rehearsal gaps from
`../worker-verdict-rehearsal/RUN-RECEIPT.md`. No console contact; all work via
`--from-dir` fixtures. Pre-patch snapshot kept at `evidence.sh.pre-patch`; full
unified diff in `evidence.sh.diff` (also appended to DIFF.md).

## Script identity
- sha256 BEFORE: `5a034d460e642a51a7689e7cd498f9367609c9cddf64fcb31cdaed101601ba52`
- sha256 AFTER (new recorded sha): `76559126074c28dad936905a8d9c2c3c3d7fffedc47907cc7f0f356616ae858a`
- 55 changed lines; header comment documents the revision and provenance.

## GAP-1 (HIGH) — conversion success-path tags
- `stage_of_tag` in BOTH classifiers (text stream + supplier-diag jsonl) now
  stages `lmf_extent_base_low/high`, `lmf_extent_end_low/high`,
  `lmf_anchor_count`, `lmf_intervals_found` as `S6_CONVERSION`; the bare-tag
  text regex gained the same six alternatives. Success chains now include
  conversion instead of only the D-style refusal fan-out.
- SDIAGSUM now carries `anchor_count=%s intervals_found=%s` directly after
  `lease_spans=%s lease_reg=%s` (order preserved so the pre-existing
  `lease_spans=6 lease_reg=6` contract substring stays intact). The controller's
  predicted-pair check (anchor_count=7, intervals_found=1 next to spans=6) is
  now readable straight off the one-page summary — rehearsal A/B/C/E show
  `lease_spans=6 lease_reg=6 anchor_count=7 intervals_found=1`; D correctly
  shows `intervals_found=0` (geometry-refusal fixture).

## GAP-2 (MEDIUM) — BEST-decisive mod lines verbatim
- The classifier now emits `MODLOADLINE` records for `[TownfallItemSpawner]`
  lines (same match condition as the existing PROGRESS counter, so count ==
  printed set); a dedicated bounded section (capped 6) is emitted between the
  notable-lines and other-lines sections:
  `mod-load prints ([TownfallItemSpawner]): N — BEST-decisive, verbatim…`.
- The MODS_STARTED NEXT text now points at that section instead of "(count
  above)"; JSON tail gains `"mod_load_lines":N`. G2 (load prints) vs G3
  (mod-error lines, also BEST) is decidable from the page: A/E show all 4
  verbatim lines (`start_mods…`, `Loaded v1.0…F7 opens the menu.`,
  `42 spawnable pickups found…`, `Menu created.`); B/C/D correctly show none.

## GAP-3 (LOW, cheap) — S7-before-S4/S5 chain-order quirk
- First-seen order is NOT reordered (it is the honest record); when
  `S7_CFG_ATTACH` appears before `S4_LEASE_REG`/`S5_IDENTITY_ANCHORS` in a
  chain, a one-line note explains `bridge_exepath_source` is emitted mid-walk.
  Correctly fires on A/B/C/D and stays absent on E (S7 after S4, no S5 — the
  condition only tests stages that actually appear after S7). GAP-4
  (crash_tail benign-note) and GAP-5 (STIME cap 24) left untouched as scoped.

## Regression verification (both suites green)
- Host fixture suite (`host-test/run_fixture_tests.sh`): pre-patch 84/0,
  post-patch **94 passed / 0 failed, rc=0**. Added additive case 17
  `l10_success_conversion` (10 assertions) that locks GAP-1/2/3: success chain
  contains `S4_LEASE_REG -> S6_CONVERSION -> S5_IDENTITY_ANCHORS`, SDIAGSUM
  pair, verbatim load prints, `mod_load_lines` JSON field, quirk note, zero
  WARN. No existing case or fixture expectation was modified.
- Rehearsal five sets (A-E, unmodified regression corpus, same `--from-dir
  … --baseline-dir fixtures/_baselines --json-tail --out` pattern): all
  **rc=0, empty stderr, ZERO WARN**. All five verdicts are unchanged
  (A/E MODS_STARTED, B/C FAILED_STATUS, D FAILED_S6_CONVERSION) — the patch
  changes page visibility only, never classification decisions.
- Fixture immutability proven: sha256 manifest of the whole rehearsal fixtures
  tree identical before/after the runs (`runs/fixtures-pre.sha256` ==
  `runs/fixtures-post.sha256`).
- No re-baselining: diff of pre/post script filtered for `BASE_*`, `PIN_*_SHA`,
  `BASE_SESS_*`, `check_baseline_ref`, `head_window_drift` is EMPTY; the four
  pinned launch10-prep baselines and session 273/44372083 constants are
  byte-for-byte as before, and no fixture file was touched.

## Files
- Patched: `../worker-launch10-evidence-upgrade/evidence.sh` (in place)
- Patched: `../worker-launch10-evidence-upgrade/host-test/run_fixture_tests.sh` (+case 17)
- Deliverables: `CLAIM.json`, `DIFF.md` (+`evidence.sh.diff`), `TEST-RECEIPT.txt`, this report
- Raw rehearsal outputs kept: `../worker-verdict-rehearsal/runs/{A..E}.pre|post.summary.txt`,
  `{A..E}.pre|post.stdout.txt`, `{A..E}.pre|post.stderr.txt`, `runs/fixtures-{pre,post}.sha256`

Not claimed: live-console behavior (untestable locally); no verdict-grammar
changes; JSON tail only gained an additive field. Not done (out of scope):
GAP-5 STIME row cap and NOTE-2 `module_start_return`→S11_STOP label quirk.
