# REPORT — worker-parser-validation (2026-10-04)

## Status: VALIDATED — adopted parser confirmed against all 9 real logs; 2 format-variant bugs found and fixed in vendored copy

CLAIM on record: CLAIM.json (this directory is the sole output location).

## Scope

Adopted parser: `outputs/2026-10-04-zcode-team/worker-ingame-log-parser/modbridge_facts/`
(parser.py, cli.py, summarize.py). This worker had never seen real log bytes
(its own REPORT documented the missing-controller-input gap); today's real
console logs exist in `outputs/2026-10-04-zcode-controller/console/` and were
used here in full, read-only.

## Inputs validated (all 9 .jsonl logs, 3450 lines total)

| file | lines | marker family | build_ids |
|---|---|---|---|
| ingame-v1-facts-2026-10-04.jsonl | 7 | ingamefacts | igf (1) |
| ingame-v2-facts-2026-10-04.jsonl | 63 | ingamefacts | igf, igf2 |
| ingame-v3-full-results.jsonl | 1099 | ingamefacts | igf, igf2, igf3, igd1 |
| anchor-validation-results.jsonl | 1109 | ingamefacts | igf, igf2, igf3, igf4, igd1 |
| igf6-results.jsonl | 1116 | ingamefacts | igf..igf6, igd1 (7) |
| livefacts-probe-result-2026-10-04.jsonl | 8 | livefacts | lfp-… |
| 1a6b37b7-console-result-2026-10-04.jsonl | 16 | owned-dll | 4 hex ids |
| 6d295f56-console-result-2026-10-04.jsonl | 12 | owned-dll | 3 hex ids |
| mutex-console-result.jsonl | 20 | owned-dll | 5 hex ids |

13 distinct build_ids across 3 marker schema families.

## Verification method

Independent cross-checker `verify_parse.py` (this dir, stdlib only) re-parses
each raw file and checks against the parser's run outputs:

1. no-silent-drop: every raw line is either a parsed record or an errors entry;
2. build_id set parsed == build_id set harvested from raw text;
3. `exp` values parsed == `exp` values harvested from raw text;
4. comma repair introduces no duplicate JSON keys (object_pairs_hook) and
   loses no keys (every `"key":` token in the raw repaired line survives parse);
5. verdict cross-check: every rc/errno in every record matches the extracted
   ExperimentVerdict (rc, errno, name, count);
6. envelope anomaly inventory; duplicate-seq report; entry/startup flags.

Runs: `run1/` = original parser; `run2/` = fixed vendored parser.
Reports: `verify_report_run1.json`, `verify_report_run2.json`.

## Results

### Original parser (run1) — correct on the ingamefacts/livefacts family, wrong on owned-dll

- All 6 ingamefacts/livefacts logs: **fully clean**. 3410 lines, 259 repaired
  (all the documented missing-comma malformation, deterministically, with
  insertion offsets logged), **0 unrepairable**, 0 invalid-envelope, all
  build_ids/exp values/verdicts cross-checked exactly, 0 dup-keys, 0 lost keys.
- All 3 owned-dll logs (48 lines): **every line rejected as invalid-envelope**
  — a format variant the parser did not handle (see Variant 1).
- The parser's own `RealLogTests.test_both_real_logs_parse_fullly` acceptance
  test **FAILS** on real bytes (igf6 + mutex pair): FAILED (failures=1) at
  `assert rep.build_ids` for the owned-dll log. Reproduced and kept as the
  regression demonstration.

### Vendored parser after fixes (run2) — ALL GREEN

- 9/9 files: **3450 lines -> 3450 records, 259 repaired, 0 unrepairable,
  0 invalid-envelope, 3432 experiment verdicts, 0 missing lines.**
- CLI `parse` exit code 0 on every file; unrepairable/errors files all empty.
- Unit suite: `Ran 20 tests ... OK` (skipped=0) with real logs provided — the
  previously-skipped real-log acceptance test now runs and passes on real
  bytes of both format families.

## Format variants found (honest list)

1. **owned-dll `sequence` envelope key (BLOCKING — FIXED).** modbridge.owned-dll
   records use `"sequence":<int>` where ingamefacts/livefacts use `"seq"`: 48
   real lines. Original parser rejected all of them (exit 1, no build_id
   stats). Fix: `SEQ_KEYS = ("seq", "sequence")` + `_envelope_seq()` helper;
   `seq` preferred, `sequence` fallback; lines with neither are still
   invalid-envelope; records are never mutated. Upstream `parse_lines`/`parse_line`
   updated to use the helper.
2. **entry/startup presence missed (FIXED).** Real igf logs encode lifecycle
   experiments as `"exp":"entry"` / `"exp":"startup"`; `exp` was not in
   FLAG_EVENT_KEYS, so entry_present/startup_present were FALSE on logs that
   contain both. Fix: `"exp"` added to FLAG_EVENT_KEYS. Now True/True on all
   four igf logs that carry them.
3. **Nested verdicts make experiments-per-line > 1 (not a bug — documented).**
   e.g. v3 line 4 `g4_module_list` carries 7 rc-bearing dicts (top verdict +
   6 module-level verdicts). The parser extracts all of them faithfully
   (1115 verdicts from 1109 anchor lines, etc.); nested dicts without a name
   key get parent-key/fallback names. Verified count-exact per file.
4. **owned-dll `entry_present=True` caveat (NOT fixed — semantic, upstream
   decision).** With envelopes now valid, flag scanning runs on owned-dll
   records, which contain a nested zeroed struct literally keyed `"entry":{...}`;
   the parser's FLAG_KEYS semantics ("any key named entry") marks entry_present
   True although owned-dll logs carry no lifecycle experiments. Harmless for
   igf collection; if it matters, gate flag scanning on the record's `marker`.
5. **Genuine data duplicate correctly flagged (no action).** igf6-results.jsonl
   has `igd1-fc26d6c03038 seq=1` twice (count=2); parser reports it in
   duplicate_seqs and keeps both copies. Correct behavior, real finding.
6. **No other variants found.** No non-JSON lines, no truncated lines, no
   top-level non-objects, no unrepairable commas, no blank lines in any of the
   9 files; every line starts with `{`.

## Fixes applied (vendored copy only)

`vendored/modbridge_facts/parser.py` — deltas vs upstream, documented in the
module docstring "VENDORED PATCH" note:
- Variant 1: `_envelope_seq()` + SEQ_KEYS; used in `parse_line` and `parse_lines`.
- Variant 2: `exp` added to FLAG_EVENT_KEYS.
Everything else is byte-identical to the adopted parser. `cli.py` and
`summarize.py` unchanged; `tests/` copied unchanged plus an empty `__init__.py`
(for unittest discovery from this directory).

## Recommendation

1. Adopt `vendored/` as the standard collection parser for all three marker
   families; keep `run2/` as the reference artifacts.
2. Upstream the two patches to worker-ingame-log-parser (their directory is
   read-only for this worker) as a new revision ID, per its own handoff note
   ("no silent in-place rule mutation").
3. When collecting owned-dll logs, note caveat 4 (entry_present is
   marker-ambiguous) and that owned-dll records carry no `exp`/`rc` verdicts.

## Artifacts (this directory)

- `CLAIM.json` — claim on record (written first)
- `vendored/` — patched parser + unchanged cli/summarize/tests
- `run1/`, `run2/` — parse outputs (.parsed.jsonl/.repairs.json/.errors.jsonl) for all 9 logs, original vs fixed
- `verify_parse.py` — independent cross-checker
- `verify_report_run1.json`, `verify_report_run2.json` — machine verification reports
