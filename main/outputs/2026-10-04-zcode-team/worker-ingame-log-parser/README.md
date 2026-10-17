# modbridge_facts — in-game facts log parser (worker-ingame-log-parser)

Durable host-side parser/summarizer for `/data/modbridge-ingame-facts.jsonl`.
Python 3 stdlib only, no network, no engine/editor writes.

## Quick start

```sh
# parse (writes <stem>.parsed.jsonl, <stem>.repairs.json, <stem>.errors.jsonl
# next to each input, or into --out-dir). Exit 1 if anything is unrepairable.
python3 modbridge_facts/cli.py parse /data/modbridge-ingame-facts.jsonl

# summarize (human table; add --json for machine output). Exit 0; anomalies shown.
python3 modbridge_facts/cli.py summarize /data/modbridge-ingame-facts.jsonl
python3 modbridge_facts/cli.py summarize log.jsonl --expect-build-id ash-2609a --json
```

Run tests: `python3 -m unittest discover -s tests` (from this directory).

## Behavior contract

- **v1/v2 missing-comma repair**: a line failing `json.loads` is retried after
  deterministically inserting `,` wherever the parser reports
  `"Expecting ',' delimiter"` and a new JSON value starts where a previous value
  ended (object members, array items, string/string, number/key). Capped at 64
  repairs/line. Every insertion offset is written to `<stem>.repairs.json` and
  counted in the summary — repairs are never silent.
- **Never silently drop**: unrepairable lines (truncated, invalid token,
  top-level "Extra data", repair cap) are preserved byte-for-byte in
  `<stem>.errors.jsonl` and printed in the summary; `parse` exits 1. Lines that
  parse but violate the envelope (non-object top level, missing/empty
  `build_id`, missing/non-integer `seq`) are also fatal for `parse` and kept in
  the errors file.
- **Envelope per build_id**: per-file per-build_id record count, seq min/max,
  repaired-line count. Duplicate `(build_id, seq)` pairs are flagged (all
  copies kept). `--expect-build-id ID` (repeatable) flags unknown build_ids;
  without a registry nothing is called "unknown" (distinct ids are listed so a
  controller can eyeball them) — this avoids falsely flagging legitimate
  multi-build logs.
- **Extraction**: entry/startup presence (member keys, or
  `event/kind/type/phase/stage` equal to those tokens); per-experiment verdicts
  = every nested object carrying an integer/float `rc`, reporting `errno`,
  `fields` (dict), `needs_base` (bool), `candidates` (list), and a name from
  `experiment|name|id|exp`, else the parent key, else `exp#L<line>#<n>`.
  Extraction is heuristic by necessity (no writer source exists in this
  workspace to give the exact schema); it is centralized in
  `parser.py::_collect_experiments` so it can be tightened against real schema
  evidence later.
- **parse exit codes**: 0 clean, 1 any unrepairable/invalid-envelope line.
  **summarize** always exits 0 and reports anomalies in the table.

## Provenance / honesty (read before first production use)

The dispatch required deriving repair rules from two real logs in
`outputs/2026-10-04-zcode-controller/console/`. That directory (and the
controller handoff) did not exist when this tool was built — verified
exhaustively and recorded in CLAIM.md; three sibling workers independently hit
the same absence. The repair therefore implements the *documented
malformation class*, not log-specific quirks. Acceptance against reality:
drop the two real logs into place (or set `INGAME_FACTS_REAL_LOGS`), run
`python3 -m unittest discover -s tests` — `RealLogTests` then parses both and
fails if any line is unrepairable, which is the signal to refine
`_repair_missing_commas` from the actual bytes. Until then that test SKIPs with
the exact expected path (visible skip, never a silent pass). Synthetic-only
results are labeled as such; no claim is made that the rules match the real
writer's bugs yet.

## Files

- `modbridge_facts/parser.py` — repair, line parsing, envelope validation, extraction
- `modbridge_facts/summarize.py` — table + machine-JSON summary
- `modbridge_facts/cli.py` — `parse` / `summarize` CLI (direct-script or `-m` invocation)
- `tests/test_parser.py` — 20 tests: both-real-log acceptance (auto-discovered)
  + synthetic edge cases (truncated, unknown build_id, duplicate seq, comma
  variants, non-object, blank lines, no-silent-drop, CLI exit codes)
- `demo/` — labeled synthetic smoke file + its real parse outputs
- `CLAIM.md`, `REPORT.md` — protocol documents
