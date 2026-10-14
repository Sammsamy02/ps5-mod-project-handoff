# ACCEPTANCE-RECORD — the artifact manifest of a COMPLETE acceptance

Worker: worker-post-spawn-closure-prep, 2026-10-04. Read-only synthesis + spec. PROPOSAL:
this document defines what a complete acceptance record WOULD contain — it never
certifies that any of it happened (the acceptance goal, visible menu + successful spawn,
is still [TARGET] per EVE-STATE). Sources: `EVIDENCE-SPEC.md` (rung artifact laws),
`MASTER-VERIFICATION-RUNBOOK.md` (CP0–CP7), `CONTROLLER-DECISION-BRIEF.md` (flows,
verdicts), `console_batch.py` (run-dir law, read on disk), `EVE-STATE.md` (identities).

`R = /WORKSPACE/.codex/worktrees/translation-core/what`;
`W = R/outputs/2026-10-04-zcode-team`;
`RUNS = W/worker-console-batch-runner/runs/` (the tool's default `--run-root`);
`C = R/outputs/2026-10-04-zcode-controller/console/` (controller backup dir; pass
`--run-root` to write there).

---

## 1. Archive law (how the session files itself)

1. **One fresh run dir per flow run.** `console_batch.py make_run_dir` creates
   `RUNS/<YYYYMMDD-HHMMSS>-<flow>/` — flow ∈ {`dll7`, `dll8`, `e2e`, `stage-ridealongs`,
   `stage-observer`, `stage-qdrv1`, `stage-settings`, `rollback-ridealongs`,
   `game-evidence`, `all`}. Failed or aborted runs KEEP their run dirs (forensics);
   nothing is ever deleted or re-run into an existing dir.
2. **Every run dir carries at minimum:** `events.log` (append-only step log) and
   `receipt.json` (per-flow verdict receipt). All fetched console bytes land in the run
   dir as files (sinks, readbacks, ledgers, verdict JSONs) — a verdict that exists only
   in stdout is not archived and therefore not part of the record.
3. **Controller-owned backups** (slot readbacks, ini pre-images, crash evidence dirs,
   C4 capture) live under `C/`, each in a timestamped subdir, hashed at collection time.
4. **The controller transport ledger is authoritative** for what actually happened
   (local mirrors are PROVISIONAL); `CONSOLE-LOG.md` steps are the narrative of record.
5. **Eyes facts are recorded as receipts, not memories:** the two eye confirmations
   (menu visible, pickup visible) are written into `game-evidence-receipt.json` by the
   flow's handoff steps (its own honesty note: the user's eyes decide; log lines never
   certify). A screenshot/capture without the underlying file artifacts is not evidence
   (EVIDENCE-SPEC §0.2.4).

## 2. Canonical run-dir layout (what lands where)

```
RUNS/<ts>-dll8/            dll8-preflight-receipt-check.json · ALLFAM8_DLL_TEST upload identity
                           · <tag>-readback.elf (EXACT gate) · fetched results copy of
                           /data/modbridge-owned-dll-candidate.jsonl · verdict vs
                           EXPECTED-DELTA-8FAM · events.log · receipt.json
                           (allfam7-console-receipt.json / dll7-preflight-receipt-check.json
                           in <ts>-dll7/ when the documented SEH64 fallback ran)
RUNS/<ts>-e2e/             13/13 cmd-SHASUMS preflight proof · e2e-payload.readback.elf
                           (MODE=B identity gate) · per-dispatch cmd/readback/consumed-twin
                           artifacts (readback-<label> files; stale.chk for the stale-cmd
                           check) · fetched verbatim ledger copy per evid step · §7 reset
                           proof (only /data/modbridge/**) · events.log · receipt.json
RUNS/<ts>-stage-ridealongs/  ploader.ini pre-image (local) · console .bak proof
                           (ploader.ini.<TAG>.bak) · ini-bootstrap-verdict.txt when the ONE
                           sanctioned whole-file STOR ran · end-state readback (byte-exact
                           canonical 400 B bbc34c16…, exactly 7 registered) · four PRX
                           §3 SHT-strip readbacks (67832/67848/87112/30143976 B) ·
                           freshness baselines (UE4SS.log stat, observer log absent,
                           probe sinks if present) · receipt.json
RUNS/<ts>-stage-settings/  v2 freeze classification (834 B 9570ead3… + 446 B 3afd10ce…) ·
                           v3 lockstep readbacks (1448 B d8faa4ec… + 447 B 4d98ecd7…) ·
                           doc/rollback note · receipt.json
RUNS/<ts>-rollback-ridealongs/  both ridealong lines flipped to = 0, fresh .bak,
                           byte-verified readback · receipt.json
RUNS/<ts>-game-evidence/   S-runtime-status.txt · UE4SS.log · D1 aligned-entry log ·
                           D2 observer log · C2 / igf facts jsonl · the two probe verdict
                           sinks · probe-verdicts.json (ge.3b freshness gate) ·
                           cp0-cp4-verdict.json (ge.3) · cp5-verdict.json · cp6-verdict.json ·
                           rung_check-RUNG1.json / rung_check-RUNG2.json (ge.4/4b) ·
                           post_launch_check output · game-evidence-receipt.json (ge.10,
                           incl. the two eye confirmations) · [PROPOSED additions]
                           stability-gate-verdict.md + spawn-count-table.csv (SG window) ·
                           events.log · receipt.json
C/<tag>-*/                 slot readbacks · ini pre-images/.bak copies · comp-sg-crash-<TAG>/
                           crash evidence dirs (all hashed) · C4 video/burst capture per
                           rung-5 attempt
```

Exact filenames inside a flow are whatever that flow's `ctx.save` wrote; the run's
`receipt.json` is the index (its `cp_results`/`probe_sinks` fields name the files). Do
not guess a filename — read the receipt.

## 3. Track A — runtime ladder: CP0–CP7 and rungs 1–5 (game session)

Producing step: `console_batch.py game-evidence` (+ the user's 7 required actions,
2 eye confirmations, 1 optional). Every row needs the fresh-window evidence law
(`INCONCLUSIVE-STALE` can never PASS) and the EVIDENCE-SPEC global NON-EVIDENCE
appendix behind it.

| # | Claim | Producing step / gate | Artifacts (run dir unless noted) | PASS law |
|---|---|---|---|---|
| CP0 | composed PRX ran | ge.1–ge.3 | D1 copy; `cp0-cp4-verdict.json` | one new `module_start_entered…return 0x0` pair + `probe_lifecycle_result 0x0` |
| CP1 | activation consumed; pad provider up | same | S copy; L banner lines; D1 `mfa_activated_*`, `runtime_start_ok` | fresh `activated` + banner + `Setting up mods...`, no `module_start failed:` line |
| CP2 | engine-ready; Tick at predicted address | same | S `state 2 engine-ready 0`; L tick line | address equality `0x33ac8c0` (+base) per corrected gate (EVE-STATE); outcome #2 vtable-rescue recorded, never silently OK |
| CP3 | mods-started + loop alive | ge.4 | S `state 3 mods-started 0`; `rung_check-RUNG1.json` | RUNG1 strict baseline; `UNVERIFIED_DRIVER` honest when no driver (G2) |
| CP4 | Item Spawner loaded (rung-4 gate) | ge.3 | L: byte-exact `Loaded v1.0…` line once | EVIDENCE-SPEC §4.3 phase 1 (+ phase-2 no-save line as designed) |
| CP5 | menu opens (half the goal) | ge.5–ge.7 | L `<N> spawnable pickups found.` + `Menu created.`; `cp5-verdict.json`; eye confirmation in `game-evidence-receipt.json` | G4: VISIBLE overlay (title, 12 rows, controls line, input locked); T3/T4 shapes per §4.3 |
| CP6 | spawn works (half the goal) | ge.8–ge.9 | L `Spawned <Item> x<N> ID=<id>`; `cp6-verdict.json`; eye confirmation | G5: visible pickup at aim point AND the log line; §5.3 both-of-two law |
| CP7 | deeper rungs (optional; never blocks) | ge.10 + | `rung_check-RUNG2.json` (terminal NQH1 record); optional RUNG3 via separate Rung3Probe per RUNG3-RECIPE | EVIDENCE-SPEC §2.2 / RUNG3-RECIPE §7 |
| RUNG1 | genuine game-thread callback | qdrv1 records + `rung_check --rung RUNG1` re-runs ~20–30 s straddling the 8 s ramp | C2 snapshots in the fetched sinks; `rung_check-RUNG1.json` | strict baseline: ticks strictly increase, one stable non-zero `game_thread_id_key`, ≥1 counted guard refusal (EVIDENCE-SPEC §1.2) |
| RUNG2 | valid object query | same driver, terminal record | terminal NQH1 record in C2 copy; `rung_check-RUNG2.json` | `status:completed`, `counts.recorded==2`, both >10000, `monotonic ∈ {equal, non_decreasing}` (§2.2); `inconclusive_below_threshold` = honest FAIL |
| RUNG3 | harmless reflected access | separate Rung3Probe mod (own recipe) | `[Rung3Probe]` L lines + same-launch FC-1 C2 records | recipe §7; `valid=false` = F1/F2 discrimination; `UNREACHABLE` = F11 STOP |
| RUNG4 | unchanged mod activation | CP4 (+ phase-2 no-save F7 test) | the two designed mod lines, byte-exact, exactly once each | §4.3; any edited main.lua voids the rung |
| RUNG5 | visible spawn through real input | CP6 + operator checklist | C4 capture in `C/` (continuous, timestamps); spawn-count table: press # → item id → frame timestamp, N ≥ 5, negative controls | §5.4 items 1–6 ALL hold; negative controls behaved |

## 4. Tracks B–E — the non-game-session tracks

| Track | Producing step | Artifacts | Archive |
|---|---|---|---|
| **B. DLL families** (7/8-family) | `console_batch.py dll8` (fallback `dll7`) | preflight identity receipt (`79281191…`/336552 B); `<tag>-readback.elf` EXACT gate; fetched `/data/modbridge-owned-dll-candidate.jsonl`; verdict vs EXPECTED-DELTA-8FAM — four new records at **positions 37–40** (dll8) or **41–44** (dll7 documented fallback); static_tls+seh64 family keys; consumer_imports 90; results [86,93]; known benign delta "stdout_error 32 with stdout_records 1" recorded, not papered over | `RUNS/<ts>-dll8|dll7/` + EXPECTED-DELTA reference doc; ledger identity in receipt |
| **C. Installer E2E** | `console_batch.py e2e` | 13/13 cmd SHASUMS preflight; payload `a21771f8…`/681888 B MODE=B gate; 15-dispatch stage 0..6 cycle with verbatim ledger lines + per-dispatch consumed-twin proof (`.consumed` hash == last cmdfiles.log line); the ONE permitted `recover.json` dispatch ONLY on interrupted stage-3 commit; §7 full-tree reset proof (only `/data/modbridge/**`); patches.json promotion digest (252ad699→63c28979, APPLIED per EVE-STATE) | `RUNS/<ts>-e2e/` |
| **D. App track** (host-only; never touches the console) | worker-app-* deliveries | `worker-app-backend-integration/` receipts: `api-tests.log` 42/42, `host-build.json` (build_id `85fe7f02…`), vendored tree + applied patch proofs; `worker-app-modmanager` 18/18; `worker-app-ui-flows/` `UI-FLOW-RESULTS.md` + `FIXES.md`; `worker-app-download-runtime` receipts; uninstaller fault suite `worker-uninstall-fault-expansion/test/*` 186+75 green on patched binary `46d87345…` | each worker's own dir; this record indexes them (no console-side artifact exists or is expected) |
| **E. Staging & state receipts** (preconditions of the session) | Phase 1–2 flows + host gates | comp6 gate approval 13/13 (candidate `76ee6af3…`, preserved at `C/comp6-staged-fself.prx`); ini end-state proof (7 registered; INI-BOOTSTRAPPED verdict file when taken); settings v3 readbacks; r11 receipts (`worker-fresh-r11-gate-receipt/live-r11-receipt.json`, `v2-r11-receipt.json`); EVE-STATE L1–L12 identities re-hashed at open | run dirs of the staging flows + `worker-*-receipt` dirs |

## 5. Cross-cutting records and the completeness rule

- **Session manifest (PROPOSED):** at session close the controller writes
  `RUNS/SESSION-MANIFEST-<date>.md` listing every run dir with sha256 of each
  `receipt.json` + the four sink tails copied into `C/`. Until that lands, the record is
  implicit across run dirs — the manifest is what makes it ONE record.
- **The SG window extends, never replaces:** `stability-gate-verdict.md` +
  `spawn-count-table.csv` in the `game-evidence` run dir (see STABILITY-GATE §2–§3).
- **Completeness rule:** Track A rows are complete only from ONE real launch window with
  fresh evidence; Tracks B/C from their game-closed windows; Track D from host receipts.
  **Any row `NOT-ATTEMPTED` keeps the acceptance honestly INCOMPLETE — that is a
  legitimate terminal state** and is recorded as such (never back-filled from host
  evidence, never re-rated from stale windows).
- **The record never contains:** ">8 quality" or completion claims; rung advancement
  from host tests; frozen historical artifacts re-presented as live; screenshots without
  file artifacts; any replay/re-rate of failed or unchanged evidence (EVIDENCE-SPEC
  appendix; QUALITY-RUBRIC law).
