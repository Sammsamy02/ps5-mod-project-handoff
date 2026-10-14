# REHEARSAL-RECEIPTS — evidence-pipeline rehearsal (synthetic fixtures only)

worker-evidence-pipeline-rehearsal, 2026-10-04. NO console contact was made:
every fixture byte is fabricated on the host from the runbook grammars and real
in-tree line shapes (launch-8 ledger entry `zcode_composition5_launch8_result`,
ENGINE-READY-PATH row 80, worker-post-launch-tools / worker-rung-evidence-reader
host-test fixture shapes). Nothing here advances a rung or certifies the console
(runbook do-not-claim guard). Raw tool outputs: `runs/<scenario>/`.
Machine summary: `rehearsal-results.json`. Fixtures: `fixtures/<scenario>/`
(`from-dir/` = tool-consumed flat files; `console-tree/data/...` = same bytes
under the console paths).

## 1. Per-scenario verdict table

| Check | s01 full success | s02 PS-scan timeout | s03 lease-gate refusal | s04 script-error trap | s05 stale launch |
|---|---|---|---|---|---|
| post_launch_check (exit) | `MODS_STARTED` (0) | `FAILED_STATUS` (0) | `FAILED_runtime_start_failed` (0) | `MODS_STARTED` (0) | `NOT_STARTED` (0) |
| plc NEXT names | rung-4 print check: **4** TownfallItemSpawner hits | status-is-truth + "report detail verbatim" (mislabelled CONVERSION — gap 2) | bridge stage 2 CONTEXT text (mislabel for lease gate — gap 3) | same MODS_STARTED text but **0** hits (the hook; CP4 names F9) | "stale… mid-launch window or F0 — verify staging + ploader" |
| probe repeats counted | 0 | 0 | 0 | 0 | 3 (correctly never "new") |
| rung_check (exit / overall) | PASS (0) | CANNOT_ASSERT (3) | CANNOT_ASSERT (3) | CANNOT_ASSERT (3) | CANNOT_ASSERT (3) |
| RUNG1 / RUNG2 | PASS / PASS | UNVERIFIED / CANNOT_ASSERT | UNVERIFIED / CANNOT_ASSERT | UNVERIFIED / CANNOT_ASSERT | UNVERIFIED / CANNOT_ASSERT |
| UNVERIFIED_DRIVER warning | absent (driver staged) | present + "C2 fetched but EMPTY" | present + --discover routing | present | present |
| RUNG4 load-line / RUNG5 spawned counts | 1 / 1 | 0 / 0 | 0 / 0 | **0 / 0** (despite mods-started) | 0 / 0 |
| CP0 | PASS | PASS | FAIL(**mislabelled "F1 probe lifecycle"** — gap 1b) | PASS | **PASS (BUG — stale D1 lines; gap 1a)** |
| CP1 | PASS | PASS | FAIL(entry failure line, correct) | PASS | **FAIL (BUG — labels F2-F6 for a stale session)** |
| CP2 | PASS | FAIL(no engine-ready; outcome-#8 detail in S) | FAIL | PASS | **PARTIAL (BUG — stale engine-ready)** |
| CP3 | PASS | FAIL | FAIL | PASS | **PASS (BUG — stale mods-started)** |
| CP4 | PASS | FAIL(F9/no banner) | FAIL(F9/no banner) | **FAIL(F9/no banner: exact `Error loading script:` text surfaced)** | FAIL(F9/no banner) |
| CP5 / CP6 (s01 L-diff t1→t2→t3) | PASS(1 new pickups line + Menu created.) / PASS(Spawned Antibiotics x1 ID=DeprecatedItem) | n/a (gate closed) | n/a | n/a | n/a |
| CP7 (`--rung RUNG2`) | PASS (delegated check exercised) | not runnable (honest) | not runnable | not runnable | not runnable |
| CP overall | PASS → "gate CP5: user must load a save and press OPTIONS once" | INCOMPLETE → "fix the failing CP" | INCOMPLETE | INCOMPLETE (the trap caught at CP4) | INCOMPLETE (but with false PASS cells — gap 1a) |
| --baseline-file variant | same verdict | same verdict | same verdict | same verdict | same verdict |
| rung_check --discover (s03) | — | — | exit 0; `/data` candidates listed; C2 correctly absent | — | — |

**Discrimination: PASS.** All five trees produce distinct chain signatures:
s01 all-green; s02 `FAILED_STATUS` (silent init-thread death: status failed, NO
L failure line) vs s03 `FAILED_runtime_start_failed` (L failure line present,
bridge stage 2) — the two early-failure shapes are separated exactly as the
verdict precedence intends; s04 shares s01's `MODS_STARTED` **by design** (the
status ladder is truth and stays mods-started — the trap) and is caught by CP4
(exact Lua error surfaced) + RUNG4 count 0 + "0 found" in the plc NEXT text;
s05 is `NOT_STARTED` + 3 counted probe repeats post_launch-side, but is
**misclassified by the console_batch CP classifier** (gap 1a below).

## 2. Tool-gap list (documented only; NO tool was edited)

### GAP 1a (BUG — wrong PASS verdicts) — worker-console-batch-runner/console_batch.py `cp01234_verdict`
No freshness/delta gate. s05 (stale session: status `session 234 8812000` <
freshness threshold 8866785; D1 holds only prior-launch lines) yields
CP0=PASS + CP3=PASS + CP2=PARTIAL from stale evidence, contradicting
post_launch_check's `NOT_STARTED` and runbook CP0/CP1/CP3 ("exactly one NEW
`module_start_entered` per launch" O1; "fresh S session" O2). On the real
console this could green-light pad interaction (CP5 gate hint) after a launch
that never ran the PRX. The CP0/CP3 PASS vs NOT_STARTED contradiction is itself
the cross-check that exposes it.
**Proposed one-line fix:** give `cp01234_verdict` a pre-launch `d1_entered_before=0`
param and require `d1_info["module_start_entered"] - d1_entered_before >= 1` for
CP0 (and reuse post_launch_check's `FRESH_SESSION_MS_THRESHOLD` to require
`session_ms` freshness for CP1/CP3).

### GAP 1b (label-only) — same function, CP0 fallback
s03: module_start entered, probe lifecycle fine, but return-0x0 absent (bridge
`runtime_start_failed code=2`) → label `FAIL(F1 probe lifecycle)`. F1 is the
probe-refusal signature; the correct family is F3/F4/F5 (runbook CP1).
**Proposed one-line fix:** before the F1 fallback, branch on
`d1_info["runtime_start_failed"]` → `"FAIL(F3/F4/F5 runtime_start_failed: %s)"`.

### GAP 2 (guidance family mislabel) — worker-post-launch-tools/post_launch_check.py `stage_hint` (~:564-574)
s02: status `failed code=1 detail="PS scan timed out"` is init-thread outcome
#8 (conversion already succeeded: banner + `runtime_start_ok 0x0` in D1), but
stage_hint maps any code∈BRIDGE_GUIDANCE to `runtime_start_failed`, so the
code-specific guidance reads "bridge stage 1 CONVERSION … vm-scan image span".
The verdict `FAILED_STATUS` and "report detail verbatim" still route triage to S
first, so this misleads the hypothesis, not the first action.
**Proposed one-line fix:** in `stage_hint`, test the detail for init-thread
signatures (`"PS scan timed out" in detail or "AOB scans" in detail`) and return
an `init_thread_failed` hint before the bridge-code lookup.

### GAP 3 (stale guidance vs known signature) — same file `BRIDGE_GUIDANCE[2]` (~:159-160)
s03 (real launch-8 signature: detail `Owned read range includes unregistered
bytes`, lease threw DESPITE `bridge_lease_reg=6` — page-quantized registration
gap, r11 fix) gets "context validation threw — …Verify the PRX path exists via
FTP." The PRX is present; following this literally sends the controller to
re-stage a good PRX.
**Proposed one-line fix:** append to the BRIDGE_GUIDANCE[2] text: `"; if detail
contains 'unregistered bytes' this is the known launch-8 read-lease gate (r11
fix) — report verbatim, do not re-stage on this signature alone."`

### GAP 4 (missing freshness gate — DEMONSTRATED false-PASS) — worker-rung-evidence-reader/rung_check.py `status_engine_ready_or_later` (~:270-283)
s05: the RUNG1 `status_engine_ready_or_later` condition PASSES on the stale
session (last state mods-started, ms 8812000 < threshold 8866785). RUNG1 stayed
`UNVERIFIED` in the five-scenario run only because the driver line was absent.
**Demonstrated** with the derived probe tree `fixtures/probe_gap4_stale_driver/`
(same stale UE4SS.log/status/D1 as s05 + stale NQH1 records with advancing
ticks 5→7 across snap1/snap2): `rung_check.py --from-dir … --rung RUNG1`
returns `RUNG1: PASS (all 5 conditions + driver line seen)`, overall
`VERDICT: PASS`, exit 0 — a rung-1 liveness PASS on evidence from a session
that never happened. Raw output: `runs/probe_gap4_stale_driver.rung_check.RUNG1.txt`.
**Proposed one-line fix:** add a freshness condition
(`st["session_ms"] > FRESH_SESSION_MS_THRESHOLD`, constant shared with
post_launch_check) to `status_engine_ready_or_later`.

### Designed handoff (NOT a bug — recorded so the real session reads the chain)
s04: post_launch_check alone stays `MODS_STARTED` (correct: the status ladder is
per-launch truth and stays mods-started through a script error). Its NEXT text
carries the hook ("0 found in new lines"); the F9 naming happens at the CP4
check in the game-evidence flow. Single-tool reads are insufficient by design —
always run Step A + CP checks together.

### Runner note (console_batch game-evidence as-found)
The `game-evidence` flow on disk is complete enough to mirror: its Step A/B
invocations, CP classifier, and CP5/CP6 L-diff classifier all behaved exactly as
the flow documents. This rehearsal drove `post_launch_check.py` /
`rung_check.py` / the imported `cp_*` classifiers directly (reproducible without
the interactive user-gate runner); the runner itself was not executed end-to-end
because its real path waits on live FTP + user confirmations. Its CP labels
inherit gaps 1a/1b/2-family wording noted above.

## 3. Confirmed command sequence for the real session

Fixture-mode rehearsal proof is in `runs/`; the real session uses the same tools
over FTP (never `--from-dir`):

```bash
T=outputs/2026-10-04-zcode-team
# ~60 s after the user starts the game (records appear ~1 min after launch):

# Step A — post-launch classifier (always first; S+L+D1 in one pass)
python3 $T/worker-post-launch-tools/post_launch_check.py
#   FETCH_FAILED (exit 4) -> wait ~60 s, re-run; twice -> check FTP/network.

# Step B — rung assertions (strict gate; honest UNVERIFIED_DRIVER without a driver add-on)
python3 $T/worker-rung-evidence-reader/rung_check.py
#   files missing -> python3 $T/worker-rung-evidence-reader/rung_check.py --discover

# Step C — runbook CP0-CP7 (game-evidence flow encodes these):
#   CP0-CP4 from S + L + D1; CP4 gate = exactly one
#   '[Lua] [TownfallItemSpawner] Loaded v1.0. …' line AND S ends mods-started;
#   CP2 tick line = 'GameEngine::Tick address (vtable: 0x37ac8c0; scan: 0x37ac8c0)'.
#   Only after CP1-CP4 PASS:
#     CP5: user loads a save, presses OPTIONS once; re-fetch L; PASS needs
#          '<N> spawnable pickups found. Pages=<P>' + 'Menu created.' new lines
#     CP6: user presses CROSS; re-fetch L; PASS needs 'Spawned <Item> x<N> ID=<id>'
#          PLUS the user's eyes on the new pickup (log without actor = investigate)
#     CP7: python3 $T/worker-rung-evidence-reader/rung_check.py --rung RUNG2
#          (only with the rung-1 driver staged; otherwise honest UNVERIFIED)

# Fixture-mode equivalents proven this rehearsal (HOST TESTS ONLY — never for console):
python3 $T/worker-post-launch-tools/post_launch_check.py --from-dir <dir> [--baseline-file <file>] [--json]
python3 $T/worker-rung-evidence-reader/rung_check.py --from-dir <dir> [--discover] [--rung RUNG2] [--json]
```

Real-session expectations this rehearsal pins down: (1) until a driver add-on is
staged, RUNG1/RUNG2 will honestly read UNVERIFIED/CANNOT_ASSERT — not a launch
failure (gap G2); (2) a script error after mods-started keeps post_launch at
MODS_STARTED — the CP4 check is mandatory before claiming rung 4; (3) if status
ends `failed` with detail "PS scan timed out", that is outcome #8 regardless of
the CONVERSION wording in the guidance; (4) if L shows
`runtime_start_failed code=2 detail=Owned read range includes unregistered bytes`,
that is the launch-8 lease-gate signature — reconcile, do not blind re-stage.
