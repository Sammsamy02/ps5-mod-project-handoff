# FLOW ENTRY — e2e (expected in the return session)

Source of law: `../worker-post-spawn-closure-prep/ACCEPTANCE-RECORD.md` §2 (run-dir
layout) and §4 Track C. Produced by `console_batch.py e2e`.

Run dir: `RUNS/<YYYYMMDD-HHMMSS>-e2e/` — stamped fresh by
`../session_run.sh start --flow e2e --run-root <canonical RUNS>`.

## Expected-artifact checklist (advisory; receipt.json is the index)

- [ ] cmd-SHASUMS preflight proof (`*shasum*`) — 13/13 cmd SHASUMS preflight
- [ ] `e2e-payload.readback.elf` — payload MODE=B identity gate
      (`a21771f8…` / 681888 B)
- [ ] per-dispatch readback artifacts (`readback-*`) — 15-dispatch stage 0..6
      cycle, verbatim ledger lines
- [ ] per-dispatch consumed-twin proof (`*.consumed`) — `.consumed` hash ==
      last cmdfiles.log line
- [ ] `stale.chk` — the stale-cmd check artifact
- [ ] verbatim fetched ledger copy per evid step (`*ledger*`)
- [ ] §7 full-tree reset proof (`*reset*`) — only `/data/modbridge/**` touched
- [ ] `MODBRIDGE_INST_E2E-receipt.json` — e2e receipt
- [ ] `events.log` — append-only step log (archive-law minimum)
- [ ] `receipt.json` — verdict receipt / artifact index (archive-law minimum)

## Session usage

    ./session_run.sh start --flow e2e --run-root <RUNS>
    ./session_run.sh capture --run-dir <RD> --src <fetched-ledger-copies>...
    ./session_run.sh verdict --run-dir <RD> --from <tool-output>.txt --label e2e-verdict
    ./session_run.sh close   --run-dir <RD> --verdict PASS|FAIL|INCONCLUSIVE|ABORTED

Honesty notes: the ONE permitted `recover.json` dispatch exists ONLY on an
interrupted stage-3 commit; patches.json promotion digest (252ad699→63c28979,
APPLIED per EVE-STATE) belongs in the receipt notes. Run from a game-closed
window; no completion claim beyond the flow's own verdict rules.
