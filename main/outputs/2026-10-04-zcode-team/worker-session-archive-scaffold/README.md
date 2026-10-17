# worker-session-archive-scaffold

Local scaffold so the return session's artifacts land organized from the first
command. Implements the archive law of
`../worker-post-spawn-closure-prep/ACCEPTANCE-RECORD.md` (§1–§2), matching the
run-dir behavior of `../worker-console-batch-runner/console_batch.py`.

## Layout

    runs/                 local run-root (pristine; INDEX.md header only).
                          Real session: pass --run-root <canonical RUNS>.
    flows/                pre-created expected flow entries with
                          expected-artifact checklists:
                            dll8.md  e2e.md  game-launch9.md  game-launch10.md
    session_run.py        helper core (python3, stdlib only)
    session_run.sh        wrapper: ./session_run.sh <subcommand> ...
    tests/                host test + fixtures + TEST-RECEIPT.json
                          (example run receipts under tests/tmp-runroot/)

## Usage

    ./session_run.sh start --flow dll8 --run-root <RUNS>        # prints new run dir
    ./session_run.sh note  --run-dir <RD> "step 3 readback EXACT"
    ./session_run.sh capture --run-dir <RD> --src /fetched/UE4SS.log --src /fetched/S-runtime-status.txt
    ./session_run.sh verdict --run-dir <RD> --from tool-out.txt --label dll8-verdict
    ./session_run.sh close   --run-dir <RD> --verdict PASS --notes "..."
    ./session_run.sh checklist --flow game-launch9

Flows: the console_batch set (`dll7 dll8 e2e stage-ridealongs stage-observer
stage-qdrv1 stage-settings rollback-ridealongs game-evidence all`) plus the
session's expected `game-launch9` / `game-launch10` (any `game-launch<N>` is
accepted and mapped to the game-evidence checklist).

## How it satisfies the archive law

| Law (ACCEPTANCE-RECORD §1) | Scaffold behavior |
|---|---|
| One fresh run dir per flow run; nothing re-runs into an existing dir | `start` stamps `<YYYYMMDD-HHMMSS>-<flow>` (same format as `make_run_dir`), refuses collisions; `capture`/`verdict` refuse overwriting an existing file; `close` refuses rewriting a CLOSED receipt. No delete path exists. |
| Failed/aborted runs KEEP their run dirs | `--verdict ABORTED` records it; the dir and all artifacts are kept. |
| Minimum events.log + receipt.json in every run dir | Both exist from the moment of `start` (receipt starts `status: OPEN`). |
| All fetched bytes land as files; stdout-only verdicts are not archived | `capture` copies evidence in; `verdict` archives the raw tool output as `verdict-input-<label>.txt` and the extracted lines as `verdict-lines.txt`. |
| receipt.json is the index | Fields: `flow`, `utc`/`date_utc`, `verdict`, `artifacts[]` (name, sha256, bytes, absolute source path), `verdict_lines[]`, `expected_artifacts[]`, `missing_expected_artifacts[]`, `honesty`. |
| Eyes facts are receipts, not memories | The game-evidence checklist names `game-evidence-receipt.json` incl. the two eye confirmations; the helper can carry but never synthesize them. |
| Record never contains completion/>8 claims | Fixed honesty note; checklists report missing rows honestly; INCOMPLETE is a legitimate terminal state. |

Controller-owned backups (slot readbacks, ini pre-images, crash dirs, C4
capture) stay under `C/` per the record — this scaffold does not relocate them;
capture their hashed copies into the run dir if the flow's receipt should index
them.

## Canonical roots for the real session

- tool default: `W/worker-console-batch-runner/runs/`
- controller: `R/outputs/2026-10-04-zcode-controller/console/` (pass `--run-root`)

## Test

    bash tests/test_session_run.sh     # all green → tests/TEST-RECEIPT.json
