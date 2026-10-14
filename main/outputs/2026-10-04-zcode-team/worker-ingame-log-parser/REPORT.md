# REPORT — worker-ingame-log-parser (2026-10-04)

## Status: DELIVERED (tool complete and tested on synthetic evidence); BLOCKED-INPUT note for the two real logs

CLAIM on record: CLAIM.md (this directory is the sole output location for this role).

## Delivered

- `modbridge_facts/` package: durable host-side parser for
  `/data/modbridge-ingame-facts.jsonl` — deterministic v1/v2 missing-comma
  repair with per-insertion logging, envelope validation per build_id
  (required non-empty string build_id + integer seq, duplicate-seq detection,
  optional unknown-build registry via `--expect-build-id`), experiment-verdict
  extraction (rc/errno/fields/needs_base/candidates), entry/startup presence
  detection. Stdlib only.
- CLI `modbridge_facts/cli.py` with `parse` and `summarize` modes. `parse`
  writes `<stem>.parsed.jsonl`, `<stem>.repairs.json`,
  `<stem>.errors.jsonl` and exits nonzero (1) on any unrepairable or
  invalid-envelope line; unrepairable lines are preserved verbatim — never
  silently dropped. `summarize` prints the required table (line counts,
  entry/startup presence, per-build_id envelope stats, per-experiment
  rc/errno/fields/needs_base/candidates, unrepairable lines with raw text) or
  `--json`.
- `tests/test_parser.py`: 20 tests. Synthetic coverage per dispatch: truncated
  line (unrepairable, preserved, exit 1), unknown build_id (registry-flagged;
  unflagged without registry), duplicate seq (flagged, all copies kept), plus
  comma-malformation variants (object-internal, string/string, nested array,
  multiple per line), non-object top level, blank lines, repairs-logged
  assertions, CLI exit codes, no-silent-drop byte-for-byte check. The
  dispatch's "both real logs" acceptance is implemented in `RealLogTests` with
  auto-discovery (`INGAME_FACTS_REAL_LOGS` env var, else
  `outputs/2026-10-04-zcode-controller/console/*.jsonl`); it asserts zero
  unrepairable lines and non-empty build_id envelopes per log.

## Verified evidence (actual runs, this workspace)

```
Ran 20 tests in 0.011s
OK (skipped=1)
```

The single skip is `RealLogTests.test_both_real_logs_parse_fully`, skipping
with reason: real logs not delivered (expected directory does not exist).
That is a visible, reasoned skip — not a pass and not a drop.

CLI smoke on a labeled synthetic file (`demo/`): 4 lines -> 3 records,
1 repaired, 1 unrepairable (truncated line echoed verbatim in table and
`synthetic-demo.errors.jsonl`), parse exit=1, summarize exit=0.

## Honest limitation (must be resolved by the controller)

The two real console logs and `ZCODE-FORK-HANDOFF.md` were absent
(`outputs/2026-10-04-zcode-controller/` never existed here; no
`*ingame-facts*` file or `modbridge` symbol exists anywhere in the repo;
`/data` does not exist on this host). Therefore the repair rules were derived
from the documented malformation class, NOT from real log bytes, and the
extraction heuristics are schema guesses grounded only in the dispatch text.
The tool is built so that confronting reality is one command: once the logs
exist, `python3 -m unittest discover -s tests` either passes (rules hold) or
fails with the exact unrepairable line to refine `parser.py::_repair_missing_commas`.
No claim is made that the rules match the real writer's bugs yet; do not adopt
this as the standard collection tool for real logs until that test passes.

Sibling-worker cross-check: worker-e2e-console-staging, worker-f-r2-fix and
worker-dll-mod-acquisition-prep independently documented the same missing
controller handoff/inputs, corroborating this is a controller delivery gap,
not a local search failure.

## Handoff to controller

1. Deliver the two real logs to `outputs/2026-10-04-zcode-controller/console/`
   (or pass paths via `INGAME_FACTS_REAL_LOGS`).
2. Re-run the suite; if green, run
   `python3 modbridge_facts/cli.py parse <log>` per log and adopt outputs as
   the standard collection artifacts. If any line is unrepairable, send that
   raw line back to this role for a rule refinement revision (new revision ID;
   no silent in-place rule mutation).
