# CLAIM — worker-ingame-log-parser (2026-10-04)

CLAIM: this directory (`outputs/2026-10-04-zcode-team/worker-ingame-log-parser/`) is the
sole output location for role worker-ingame-log-parser. Deliverables claimed here:
parser package (`modbridge_facts/`), CLI (`modbridge_facts/cli.py`), unit tests
(`tests/test_parser.py`), README.md, REPORT.md.

## Input status (verified read-only before any work)

| Required input | Expected location | Actual |
|---|---|---|
| ZCODE-FORK-HANDOFF.md | outputs/2026-10-04-zcode-controller/ | directory does not exist (also verified absent by sibling workers worker-e2e-console-staging, worker-f-r2-fix, worker-dll-mod-acquisition-prep) |
| Real log #1 and #2 | outputs/2026-10-04-zcode-controller/console/ | directory does not exist; no `*ingame-facts*` file anywhere in repo (`mdfind` + `find` + grep = 0 hits) |
| `/data/modbridge-ingame-facts.jsonl` | filesystem root `/data` | `/data` does not exist on this host |
| modbridge writer source (schema ground truth) | repo | zero grep hits for `modbridge`/`ingame-facts` outside worker notes |

## Consequence and mitigation

The two real logs were unavailable, so the missing-comma repair rules could NOT be
derived from them. Instead the repair is a deterministic, fully logged transformation
for the documented malformation class ("a value immediately followed by another value
with no separating comma", writer v1/v2). Every insertion offset is recorded; nothing
is silently dropped; unrepairable lines exit nonzero. Tests run over synthetic fixtures
now and auto-include the real logs when they appear at the expected path (or via
`INGAME_FACTS_REAL_LOGS`); until then those two test cases SKIP with an explicit reason,
never a silent pass. See README.md "Provenance" and REPORT.md.
