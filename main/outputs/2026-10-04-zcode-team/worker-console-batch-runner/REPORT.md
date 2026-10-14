# REPORT — worker-console-batch-runner (2026-10-04)

## Delivered

**`console_batch.py`** (2134 lines, stdlib-only Python 3, sha256 `870ee937…41`) — a single
controller that executes each staged console plan step-by-step with gates. Subcommands:

| subcommand | executes |
|---|---|
| `plan` | dry-run: prints every step, gate and exact command for any flow; touches nothing |
| `dll7` | 7-family DLL payload-only stage: local sha/receipt pre-flight → G0 → upload ONCE (`ALLFAM7_DLL_TEST.elf`) → manage:check → FTP readback identity gate → dispatch EXACTLY ONCE → wait 15 s → results readback → full EXPECTED-LEDGER-7FAM verdict (4 records, per-field divergences, -120..-127 triage table) → `allfam7-console-receipt.json` |
| `e2e` | E2E installer: 13/13 SHASUMS + payload identity pre-flight → G0 → upload ONCE → **MODE=B identity gate** (MODE=A and foreign bytes both STOP) → corpus STORs with sha gates → **15-dispatch** stage 0..6 cycle (`cmd_put` stale-guard/readback → dispatch once → `cmd_gone` consumed-hash proof → `evid` fetch → verbatim expected-line verdict per dispatch) → interrupted-install contingency (the ONE `recover.json` dispatch, guarded so it can never fire on a clean stage 3) → 35-line count ledger → Step C evidence + receipt → §7 full-tree reset confined to `/data/modbridge/**` |
| `stage-qdrv1` | qdrv1 PRX identity check → live `ploader.ini` pre-image + console `.bak` → minimal append `2=/data/plugins/modbridge_qh_driver.prx` (no-op if present, STOP on unknown loader contract) → PRX + ini STOR with byte-exact readbacks — strictly BEFORE the game session |
| `game-evidence` | launch handoff → `post_launch_check.py` → five-sink bounded fetches → CP0-CP4 file checks (module_start pair, activation, engine-ready + Tick `0x37ac8c0`, mods-started, mod banner) → `rung_check.py` RUNG1+RUNG2 (honest UNVERIFIED_DRIVER handled) → CP5 OPTIONS handoff + shape a-d classification → visual confirmation gate → CP6 CROSS handoff + spawn classification → CP7 note + session receipt |
| `all` | the recommended single session in the 7fam plan's Sequencing order: dll7 → e2e → stage-qdrv1 → game-evidence (one game-closed payload window, one game launch total) |
| `selftest` | the host/fixture test suite (no console contact) |

`--stop-on-first-failure` is **default true**; `--continue-on-failure` still skips every dependent
group. Dispatch steps are exactly-once; ambiguity reconciles, never re-sends. User handoffs (arm
ploader, launch, load save + OPTIONS, CROSS) print exact prompts at the correct gates and wait.

## Host testing (two layers, both green; zero console contact)

1. `console_batch.py selftest` — **122 checks, 0 failed**: plan-mode anchor checks against the real
   plan content (all 13 e2e cmd files, every URL/sha/path/gate, exactly 15 dispatch cycles, 4 user
   handoffs); fixture tests of every verdict function (7FAM record ledger incl. triage mapping, E2E
   verbatim blocks + halt classification + 35-line ledger, MODE gate, cmd_gone, ploader.ini edit
   branches, CP0-CP7 classifier shapes).
2. `tests/fake_console_drive.py` — drives the REAL engine against an in-process simulated console
   (curl/user-gate interception, no network): all four flows PASS end-to-end with evidence + receipts
   written; exactly 1 dll7 dispatch + exactly 15 e2e dispatches; rescue recovery dispatches NOTHING on
   a clean stage 3; stop-on-first-failure proven (corrupt transcript → FAIL → halt → later steps
   SKIPPED_HALT).

Bugs the tests caught and fixed before delivery (details in `receipts/HOST-TEST-RECEIPT.md`):
line-vs-char cursor slicing in the E2E verifier; the S2b 6-line block spans dispatches 4-6 combined
(d06 expects the prepare pair); the recovery handler was attached to d15 instead of the rescue step;
three readback paths hashed content incorrectly; SHASUMS check lacked `cwd=CMDDIR`; CP6 diffed a
fetch against itself.

## Honest scope

- **No console contact was made by this worker.** `plan`/`selftest` cannot reach the console; the
  execute subcommands run only when the user returns and authorizes (the pause directive is
  respected — nothing here launches, polls, or pre-stages anything).
- All pinned local artifacts were re-verified on host this session (7fam ELF `83ee2ae9…`/334656,
  E2E payload `a21771f8…`/681888, qdrv1 PRX `52c32216…`/26157, movspeed zip + both mfb fixtures).
  The composition-5 packaged PRX was observed (`0911af0c…`/30792151) and recorded but is NOT asserted
  by the tool — the game session's own evidence is its oracle.
- What remains [TARGET] until the user's session: on-console STOR/readback/consumed-rename timings,
  the payload runs themselves, every expected line on real console bytes, and all CP visual facts.
  The tool is built so one authorized session covers everything with the user doing only: keep game
  closed → arm + launch → load save → OPTIONS → CROSS (+ eyeball confirmation).
- Evidence defaults to `worker-console-batch-runner/runs/`; one `--run-root` flag redirects to the
  plans' canonical `outputs/2026-10-04-zcode-controller/console/`.

## Files (all under `outputs/2026-10-04-zcode-team/worker-console-batch-runner/`)

- `CLAIM.json` — worker claim (written first)
- `console_batch.py` — the tool
- `tests/fake_console_drive.py` — simulated-console engine drive
- `receipts/BUILD-RECEIPT.json`, `receipts/HOST-TEST-RECEIPT.md` — build + test receipts
- `USER-SESSION-GUIDE.md` — user-facing handoff script (who does what, in order)
- `REPORT.md` — this report (written last)
