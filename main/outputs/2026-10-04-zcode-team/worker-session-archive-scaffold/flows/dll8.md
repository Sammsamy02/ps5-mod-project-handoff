# FLOW ENTRY — dll8 (expected in the return session)

Source of law: `../worker-post-spawn-closure-prep/ACCEPTANCE-RECORD.md` §2 (run-dir
layout) and §4 Track B. Produced by `console_batch.py dll8` (fallback `dll7` is a
separate documented flow, not a re-run into this dir).

Run dir: `RUNS/<YYYYMMDD-HHMMSS>-dll8/` — stamped fresh by
`../session_run.sh start --flow dll8 --run-root <canonical RUNS>`.

## Expected-artifact checklist (advisory; receipt.json is the index)

- [ ] `dll8-preflight-receipt-check.json` — preflight identity receipt
      (candidate `79281191…` / 336552 B)
- [ ] upload identity artifact (`*upload*`) — ALLFAM8_DLL_TEST upload identity
- [ ] `<tag>-readback.elf` (`*-readback.elf`) — readback EXACT gate
- [ ] `allfam8-console-result.jsonl` — fetched copy of
      `/data/modbridge-owned-dll-candidate.jsonl`
- [ ] `allfam8-ledger-verdict.json` — verdict vs EXPECTED-DELTA-8FAM:
      four new records at **positions 37–40**; static_tls+seh64 family keys;
      consumer_imports 90; results [86,93]; known benign delta
      "stdout_error 32 with stdout_records 1" recorded, not papered over
- [ ] `allfam8-console-receipt.json` — console receipt
- [ ] `events.log` — append-only step log (archive-law minimum)
- [ ] `receipt.json` — verdict receipt / artifact index (archive-law minimum)

## Session usage

    ./session_run.sh start --flow dll8 --run-root <RUNS>
    ./session_run.sh capture --run-dir <RD> --src <fetched>/allfam8-console-result.jsonl ...
    ./session_run.sh verdict --run-dir <RD> --from <tool-output>.txt --label dll8-verdict
    ./session_run.sh close   --run-dir <RD> --verdict PASS|FAIL|INCONCLUSIVE|ABORTED

Honesty: if the documented SEH64 fallback runs, that is a `dll7` flow with its own
fresh dir (`allfam7-*`, records 41–44) — never re-labelled dll8. Failed/aborted
runs keep their run dirs; nothing is deleted or re-run into an existing dir.
