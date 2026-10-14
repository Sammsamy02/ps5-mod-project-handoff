# SESSION INDEX — return-session run archive

Law: `outputs/2026-10-04-zcode-team/worker-post-spawn-closure-prep/ACCEPTANCE-RECORD.md` §1 (runs/<YYYYMMDD-HHMMSS>-<flow>/; receipt.json is each run's index; failed/aborted runs keep their dirs; nothing is deleted).

| utc_closed (UTC) | flow | run dir | verdict | artifacts | missing expected |
|---|---|---|---|---|---|
| 2026-10-05T06:19:07Z | dll8 | 20261005-011907-dll8 | ABORTED | 0 | dll8-preflight-receipt-check.json, *upload*, *-readback.elf, allfam8-console-result.jsonl, allfam8-ledger-verdict.json, allfam8-console-receipt.json |
| 2026-10-05T06:19:07Z | dll8 | 20261005-011906-dll8 | PASS | 3 | dll8-preflight-receipt-check.json, *upload*, *-readback.elf, allfam8-ledger-verdict.json, allfam8-console-receipt.json |
| 2026-10-05T06:19:08Z | game-launch10 | 20261005-011908-game-launch10 | ABORTED | 1 | S-runtime-status.txt, UE4SS.log, *observer*, *igf*, *xwp*, *fc1*, probe-verdicts.json, cp0-cp4-verdict.json, cp5-verdict.json, cp6-verdict.json, rung_check-RUNG1.json, rung_check-RUNG2.json, *post_launch*, game-evidence-receipt.json, stability-gate-verdict.md, spawn-count-table.csv |
