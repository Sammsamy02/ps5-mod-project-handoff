# HOST-TEST-RECEIPT — console_batch.py (worker-console-batch-runner, 2026-10-04)

OVERALL: PASS. Two independent test layers, both fully green, ZERO console contact.

## Layer 1 — `console_batch.py selftest` (122 checks, 0 failed, exit 0)

A. Plan-mode host test against the real plan content (every command present + gated):
   - all 5 flows plan cleanly; every step carries gates; every non-handoff step lists commands
   - dll7 anchors: upload URL ALLFAM7_DLL_TEST.elf, loadpayload URL, manage:check, results JSONL path,
     sha 83ee2ae9…, 334656, EXPECTED-LEDGER-7FAM
   - e2e anchors: all 13 staged cmd files by name, SHASUMS.txt, 3 corpus shas, payload sha a21771f8…,
     MODE=B gate, upload + loadpayload URLs, e2e-cmd.json.consumed, intake paths, DELE reset, 35-line
     ledger; EXACTLY 15 dispatch cycles
   - qdrv1 anchors: PRX name + sha 52c32216…, ploader.ini, .bak backup, /data/plugins target, arm handoff
   - game-evidence anchors: post_launch_check.py, rung_check.py, all 4 evidence-sink paths, CP0-CP4,
     OPTIONS, CROSS, Tick 0x37ac8c0, USER ACTION handoffs (arm, LOAD A SAVE)

B. 7FAM verdict fixtures (EXPECTED-LEDGER-7FAM shapes):
   - 4-record PASS fixture green (incl. honest stdout_error-32 note, append-only position check)
   - FAIL fixtures: stale consumer_imports 78 (field named), missing static_tls at record1, wrong
     worker.begin, cleanup retired:0 at phase5, stale memory.last.length 98304, first_failure -122
     (triage names the TLS fixture meaning), 1-record crash shape, family key present at census-zero
   - triage map: -126 → per-thread persistence; -77 → mutex range

C. E2E verdict fixtures (EXPECTED-LEDGER verbatim blocks):
   - all 13 dispatch blocks PASS against the assembled 35-line transcript; green total == 35
   - halt classification: missing 'auto-bundle composed mfb', e2e-cmd REFUSAL line, end exit=2
   - stage-6 missing F-R2 status line fails; line-count ledger rejects 33, accepts 35

D. MODE gate + cmd_gone: MODE=B on a21771f8/681888; MODE=A refuses cmd files (STOP); NO-GO on foreign
   bytes; cmd_gone pass / not-consumed / consumed-hash-mismatch

E. ploader.ini edit fixtures: create path = exact STAGE-PLAN-QDRV1 §1 shape; append next free index;
   no-op when already staged; STOP on unknown loader contract ([PPSA33286] checklist shape) and on
   unparseable ini; verify flags a lost composition line

F. CP0-CP7 classifier fixtures: green S/L/D1 → CP0-CP4 PASS with CP5 gate hint; stuck-at-activated →
   CP2/CP3 FAIL; D1-empty → F0 named; CP5 success / shape (c) load-a-save / shape (b) UI build error /
   shape (d) no reaction; CP6 Spawned detection and 'Failed at <stage>' naming

## Layer 2 — `tests/fake_console_drive.py` (in-process simulated console, 0 failed checks, exit 0)

Drives the REAL execution engine with curl/user-gate interception (no network, no input):
- dll7: all steps PASS; EXACTLY ONE dispatch; receipt + PASS ledger verdict written to the run dir
- e2e: all 15 dispatch cycles PASS against the 35-line transcript revealed incrementally per dispatch;
  the d07-recover rescue step dispatches NOTHING when stage 3 completes cleanly (SKIP_NOT_NEEDED);
  ledger green (35); Step C receipt written; reset DELEs confined to /data/modbridge/**
- stage-qdrv1: pre-image + .bak + minimal edit (=2 qdrv1 entry, composition line preserved) + PRX
  readback hash verify + ini readback equality all PASS
- game-evidence: post_launch_check/rung_check wired (canned verdicts); CP0-CP4 PASS on green fixtures;
  CP5/CP6 evolve correctly as the fake UE4SS.log gains menu/spawn lines; session receipt written
- stop-on-first-failure: corrupt results transcript → dll7.7 FAIL with divergent-field detail,
  dll7.8 SKIPPED_HALT, no further console writes

## Real engine bugs the tests caught and were fixed before delivery

1. verify_e2e_block iterated the raw string char-by-char instead of lines (cursor semantics).
2. Per-dispatch expectation for stage2b-3-prepare wrongly used the combined 6-line S2b block;
   the ledger's S2b block spans dispatches 4-6 combined — d06 expects the prepare pair (2 lines).
3. `fl.steps[-1]` attached the recovery handler to d15 instead of the rescue step (d15 would have
   dispatched nothing); now attached by step id.
4. cmd_put/cmd_gone/corpus readbacks treated fetched CONTENT as a sha; now hash the fetched bytes.
5. SHASUMS 13/13 check ran without cwd=CMDDIR (plan says `cd $CMDDIR && shasum -c`).
6. cp6_fn diffed the same fetch against itself (spawn diff always zero).

## Honesty

Host-tested logic only. No console HTTP/FTP packet was sent by this worker. On-console behavior
(timings, STOR/consumed-rename semantics, payload run results) remains [TARGET] until the user's
authorized session; the tool's own gates and receipts are what the session will produce.
