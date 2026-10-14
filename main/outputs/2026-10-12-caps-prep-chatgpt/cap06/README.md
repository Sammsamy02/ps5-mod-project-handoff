# Read-only pad receipt adapter

Run from repository root. This adapter recognizes only the existing L3+R3/F7 diagnostic chain and always leaves end-to-end expected action INCONCLUSIVE.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap06/audit_pad_receipts.py --capture outputs/2026-10-08-l25-prep/evidence/tmp-preserved-20261010/tmp/zzdiag2-diag.jsonl --consumer-source outputs/2026-10-08-l25-prep/evidence/morning-readonly-20261012T114008Z/spool/rows/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua --consumer-lines 670,671,672,694,695,696 --expected-action 'F7 callback schedules toggleMenu; toggleMenu calls showMenu(not menuOpen)' --output outputs/2026-10-12-caps-prep-chatgpt/cap06/preserved-pad-audit.json
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/2026-10-12-caps-prep-chatgpt/cap06 -p 'test_*.py'
```

Replay exit 1 is expected: evidence INCONCLUSIVE. Exit 2 signals input/output failure. `--provider-source` and `--mapping` can select other read-only local source/config snapshots. Consumer references and expected-action text are caller-selected static assertions; the tool does not parse Lua semantics or verify action delivery.

All files are read-only inputs. Output is confined to cap06, cannot overwrite an input, and import/run disables bytecode. Tests temporarily cut actual raw receipt lines into cap06/tmp; no synthetic controller events are fabricated. The report records source/capture SHA256, physical line references, PID, sequence range, clock values and exact provider-stage associations; broader action and map deployment are explicitly unproved.

The original source emission string checks support static provenance only. They do not count as input behavior. Captured records must satisfy the full association chain. Even a matched chain establishes provider-reported enqueue only, not callback execution, menu visibility, no-repeat behavior or absence of game actions.
