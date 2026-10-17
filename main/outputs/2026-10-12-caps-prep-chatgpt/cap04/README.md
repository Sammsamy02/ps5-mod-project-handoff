# Offline early-loading evidence verifier

Mac-only. Run from repository root; no console or compiler access is used.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap04/verify_milestones.py --raw-log outputs/2026-10-08-l25-prep/evidence/claude2-menu-session-20261010.log --output outputs/2026-10-12-caps-prep-chatgpt/cap04/preserved-log-audit.json
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/2026-10-12-caps-prep-chatgpt/cap04 -p 'test_*.py'
```

Raw-log mode imports the existing NUL-interleaved decoder read-only. It lists search-term line numbers, never interprets a LoadMap word/refusal as a delivered callback. This retained replay is INCONCLUSIVE (exit 1), as expected. No new runtime marker format is claimed.

For future captured evidence:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap04/verify_milestones.py --input NORMALIZED_EVIDENCE.json --output outputs/2026-10-12-caps-prep-chatgpt/cap04/verdict.json
```

Input contract: object with `schema: 1` and `events` array. Each event has `kind`, finite numeric `t`, nonempty `run_id` and `clock_id`, lowercase 64-hex `build_sha256`, source filename and positive integer decoded `line`. All identities must agree. Exactly one event each: `coverage`, `module_start`, `observer_armed`, `loadmap_entry`, `begin_play`. Coverage additionally requires `from_process_start: true`, `lossless: true`, and finite numeric `through` at least as late as every event. Its timestamp is coverage start and cannot follow any event. LoadMap entry additionally requires integer `ordinal: 1`. Times must strictly order module start < observer armed < LoadMap entry < BeginPlay. Equal times are inconclusive.

Coverage declarations must be backed by actual capture/session instrumentation, including drop counters or equivalent integrity proof, and independently reviewed source references. The verifier checks the supplied contract; it cannot attest that a declaration is truthful, that cited lines exist, or that a hook observed every event. A human writing `ordinal: 1` without startup coverage is not proof. No automatic raw-to-normalized adapter exists yet because the inspected preserved log lacks this coverage contract.

Exit 0: supplied contract PASS. Exit 1: FAIL or INCONCLUSIVE. Exit 2: input/output error. Output is restricted to cap04 and cannot overwrite the input. The tool disables bytecode; tests write no fixtures outside their own folder.

Fresh tests: 8/8. Preserved raw-log replay: INCONCLUSIVE, recorded with input/decoder hashes in preserved-log-audit.json. Remaining blocker: first-entry instrumentation and same-run lossless coverage receipt, provided by a later authorized launch; no runtime or executable change is included here.
