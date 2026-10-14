# HOST-TEST-RECEIPT — worker-batch-final-ini-adoption, 2026-10-04

All suites re-run green AFTER the final code state. Zero console contact; the fake drive
intercepts `curl`/user gates in-process; the final-layout verifier subprocess is a pure
host-side checker and genuinely executed inside the drive runs.

## Suites

| Suite | Command | Result |
|---|---|---|
| selftest | `python3 console_batch.py selftest` | **372 ok / 0 failed** (prior 271 kept: 268 untouched + 3 updated to the definitive law; +101 new) |
| fake console drive | `python3 tests/fake_console_drive.py` | **90 green checks, 0 failed** (56 prior check names kept identical; 5 prior stage-ridealongs checks rewritten for the definitive flow; +34 new scenario checks) |
| plan mode | `python3 console_batch.py plan` (+ each flow via `--only`) | **OVERALL GREEN**, all 10 flows green individually |
| adopted law proofs (re-run by the adopter) | `verify_final_ini_layout.py --selftest` / `simulate_ploader_ini_final.py` / verifier on `ploader.ini.canonical` | **9/9**, **SIMULATION_PASSED (13 variants incl. the 40320-permutation byte-exact convergence)**, **FINAL-INI-OK** — raw outputs: `verify_selftest_adopter_rerun.txt`, `simulate_final_adopter_rerun.txt` |

## Prior-check accounting (honesty)

- selftest: 268 of the prior 271 checks are byte-identical and green. 3 prior checks were
  UPDATED because they pinned the superseded 6-entry law: the two `stage-ridealongs` anchor
  checks ("exactly 5" / composition-line anchor) now pin "EXACTLY 7" / "composition = 2" and
  the new law's anchors, and the `all`-order check now demands
  `e2e < stage-ridealongs < stage-settings < game-evidence`.
- drive: 56 of the prior 61 named checks kept identical and green (dll7, e2e, stage-qdrv1,
  the four stage-observer scenarios, the readback-law negative, both rollback runs,
  game-evidence, dll8, stale-7fam halt, dll7-fail halt). The 5 rewritten checks were the
  stage-ridealongs scenario checks of the superseded combined 6-entry staging.

## New coverage (definitive 8-entry law + settings)

- selftest J — FINAL law: canonical template identity (400 B / bbc34c16…), end-state gate
  accepts canonical, ALL 24 orders of the 4 stager ensure ops from a canonically-ordered
  pre-image converge byte-exact (+ reversed + repeated idempotent applications); appends from
  the HISTORICAL pre-image converge to one DEGRADED non-canonical order (proving the bootstrap
  whole-file STOR is required); plan statuses append/noop/bootstrap; bootstrap returns the
  canonical bytes with the INI-BOOTSTRAPPED note; 15 STOP cases (no pre-image, empty, duplicate
  header, duplicate line, missing pre-existing, missing composition, composition mode flip,
  igf=2 MUST-NEVER, rolled-back mode-0, mode-1, unknown line, mode 3, unparseable line,
  canonical-ordered CRLF form, missing trailing NL) with the rollback-runbook pointer; 8
  end-state rejection cases (unknown line, degraded probe-after-composition ORDER, igf=2,
  missing line, mode flip, duplicate, empty).
- selftest G extension — SHT-strip law round-trips for BOTH new probes: xwp artifact prefix ==
  67832 == pinned, body sha == daea2ff4…; fc1 prefix == 67848, body sha == 7e2dce27…; law
  accepts the law-shaped readback and rejects the full bare file (SHT tail not embedded).
- selftest K — probe verdict sink classifier: PASS on green fixtures; WARN on zero fresh
  growth; fresh-growth-vs-baseline isolation; FAIL on unparseable line / foreign build_id /
  baseline mismatch (rotated sink); WARN (not FAIL) on an honestly recorded fc1 refusal with
  its reason_code; fc1pool_result validated 11/11 detection.
- selftest L — settings pair lockstep law: accepts the shipped v3 pair; rejects the stale v2
  size row and a missing settings row; pinned bytes match the shipped package files
  (1448/d8faa4ec…, 447/4d98ecd7…); recorded inventories consistent.
- drive — stage-ridealongs definitive scenarios: BOOTSTRAP on the historical pre-image stores
  the canonical 8-line bytes ONCE with the INI-BOOTSTRAPPED verdict + .bak + byte-exact RETR;
  4 PRX STORs exactly once in file order (xwp, fc1, qdrv1, observer) with all four law verdicts
  OK (67832/daea2ff4…, 67848/7e2dce27…, 87112/71406d3c…, 30143976/3f89a5b6…);
  ensure-only run on a canonically-ordered 6-entry pre-image appends ONLY xwp+fc1 and reaches
  the SAME byte-exact canonical end-state; a DIFFERENT staging history (observer pre-staged
  early) converges to the same bytes (content-anchored ensure lands qdrv1 between composition
  and observer); canonical re-run is a FULL no-op (no ini STOR, no PRX upload, readback-only
  law verification); unknown-line STOP uploads nothing, stores nothing, bootstrap never fires.
- drive — stage-settings: freeze classifies the staged v2 pair (834/9570ead3… + 446/3afd10ce…)
  and proceeds; v3 pair (1448/d8faa4ec… + 447/4d98ecd7…) read back byte-exact; re-run on an
  already-v3 console is an honest classified no-op; a divergent staged root FAILs the freeze
  and uploads nothing.

## Honesty

Host-side logic tests only. No console contact; no on-target byte is claimed — the console
ini bytes, probe readbacks, settings readbacks and probe verdict records remain [TARGET] until
the session runs. The final verifier subprocess executed inside the drive is the frozen
worker-final-ini-layout law file, run unmodified.
