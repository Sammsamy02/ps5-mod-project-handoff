# REPORT — worker-rung-evidence-reader (2026-10-04)

Task: pin the EXACT on-console file path of every acceptance-rung observable by
tracing writers in the composed runtime source, and deliver `rung_check.py` so
the controller has one command per rung assertion for composition-5 r3
(FTP: fresh curl per file, anonymous ftp://ps5-host-not-configured.invalid:2121/).

No console contact was made; no nested subagents; writes only in this
directory; `worker-post-launch-tools/post_launch_check.py` was read as the
pattern but NOT modified.

## 1. Answer to the controller's question (one command per rung)

```
python3 rung_check.py                      # all rungs, strict RUNG1+RUNG2 gate
python3 rung_check.py --rung RUNG1         # rung-1 assertion only (two C2 snapshots 3 s apart)
python3 rung_check.py --rung RUNG2         # rung-2 terminal-record assertion
python3 rung_check.py --discover           # FTP LIST fallback when files are missing
```
Exit codes: 0 PASS, 1 FAIL/UNVERIFIED, 2 usage, 3 CANNOT_ASSERT (evidence
missing), 4 FETCH_FAILED (retry ~60 s; records appear ~1 min after launch).
`--json` for machine output; `--dry-run` prints the exact curl commands;
`--from-dir DIR` is host-test fixture mode (never for console evidence).

## 2. Pinned paths (full cites in PATH-MAP.md)

| Observable | Console path |
|---|---|
| C2 QueryResultRecord JSONL (IgSink channel) | `/data/modbridge-ingame-facts.jsonl` |
| UE4SS.log (banner/init/mods + entry failure lines) | `/data/plugins/UE4SS.log` |
| runtime-status.txt | `/data/modbridge/roots/PPSA33286/runtime-status.txt` |
| Entry diagnostic channel (composition-5 lineage) | `/data/townfall_runtime_aligned_entry.log` |
| Standalone observer PRX channel | `/data/townfall_runtime_aligned_observer.log` |

Key writer constants: `record_sink.c:15` (`IG_LOG_PATH_DEFAULT`), generated
`ingame_config.h:4` (both determinism runs, `IG_LOG_PATH`), `UE4SSProgram.cpp:256-258`
(file device named `UE4SS.log`) with the `__PROSPERO__` branch at
`UE4SSProgram.cpp:778-784` (log dir = working dir = `/data/plugins`),
`modforge_activation.c:601` (joins literal `runtime-status.txt` under the mfa
root) with `runtime_module_entry.c:65-66` (`MODBRIDGE_MOD_ROOT
"/data/modbridge/roots/PPSA33286"`), lineage-overlay `probe_config.h:3`
(`/data/townfall_runtime_aligned_entry.log`) vs observer-PRX
`probe_config.h:6` (`/data/townfall_runtime_aligned_observer.log` — the
comment there explicitly says the two must not interleave).

Lua print destination (task 1d): the port has NO console device on
`__PROSPERO__` (`UE4SSProgram.cpp:317-318`). `print` = `global_print`
(`LuaLibrary.cpp:36-102`), which prepends `[Lua] ` and calls `Output::send`
-> the UE4SS.log file device; the mod's `log()` adds `[TownfallItemSpawner] `,
so mod lines read `[Lua] [TownfallItemSpawner] ...` in
`/data/plugins/UE4SS.log` (playbook row 26). EVIDENCE-SPEC's alternate paths
(`/data/townfall_runtime_buffered_lua.log`, `/data/townfall_mod.log`) are
older-lineage candidates, not writers in this composed source; rung_check
reports them as notes only.

## 3. LOUD caveat: the rung-1 driver is not wired into the composed PRX

The rung-1 evidence ("C2: 128-byte seqlock QueryResultRecord ->
FormatRecordJsonLine, controller-driver-written JSONL") has a path (C2 above)
but NO WRITER in composition-5 as staged:

- `FormatRecordJsonLine` has exactly one definition
  (`worker-query-harness/src/NativeQueryHarness.cpp:402`) and NO target-side
  caller; tree-wide grep finds only host tests and docs.
- `worker-query-s2-arm/integration-patch/WIRING.md:120-124`: prerequisite 3
  "(controller driver: TrySnapshot / SnapshotDispatchCounters /
  FormatRecordJsonLine) remains the controller's console-evidence step and
  needs no target code."
- The composition-5 unit list
  (`build_runtime_composition5.py:153-158`) stages lmf-* + native-query-harness
  + module-runtime-ack-supplier; the staging overlay contains no IgSink /
  record_sink / ingame_facts unit.
- C2 itself is written only by the probe-PRX family (modbridge_ingame_facts /
  ig_run_* add-ons) via `record_sink.c`; the r2/r3 gate receipts show the
  composed PRX does not include it.

Consequences, implemented in rung_check.py: RUNG1/RUNG2 verdict
`UNVERIFIED`/`CANNOT_ASSERT` (never PASS) when no `native_query_harness` line
appears in C2, plus an explicit `UNVERIFIED_DRIVER` warning; `--discover`
FTP-LISTs `/data`, `/data/plugins/`, `/data/modbridge/`,
`/data/modbridge/roots/`, `/data/modbridge/roots/PPSA33286/` as the fallback
when the C2 file itself is absent. Before r3, the controller must either (a)
stage a driver add-on that snapshots the harness and emits the
`FormatRecordJsonLine` line through IgSink under its own build_id, or (b)
accept that rung-1's C2 condition cannot be evidenced this launch.

## 4. Rung definitions implemented

- RUNG1 (strict): status last state engine-ready/mods-started (stopped
  accepted only with a prior engine-ready row; failed never) AND C2 present
  AND `ticks_accepted` (from `.ticks.accepted` of native_query_harness lines,
  or a `ticks_accepted` key at top level or in `.fields`) strictly increases
  between two snapshots (`--snapshot-gap`, default 3 s) AND exactly one
  distinct non-zero `game_thread_id_key` (`0x`+16 lowercase hex) AND >=1
  counted guard refusal (`refused_wrong_thread`/`refused_identity_unknown`/
  `refused_bindings_absent`/`refused_layout_gate_closed`, also accepted inside
  IgSink `.fields`) AND a native_query_harness line seen (else UNVERIFIED).
- RUNG2 (strict): terminal `native_query_harness` record with
  `status=completed`, `count_source=engine_uobject_array`,
  `counts.recorded=2`, both counts > 10000, `monotonic in {equal,
  non_decreasing}`, `identity_checks=2`, non-zero key,
  `ticks.refusals_between=0`, `threshold=10000`, non-empty `gate_source`.
  `inconclusive_below_threshold` is reported as the honest negative and FAILs
  the rung with a do-not-re-rate note (EVIDENCE-SPEC 2.3).
- RUNG3-5: placeholders (count-only, verdict PLACEHOLDER, never affect exit
  code): scan tails + FC1 records in C2; the rung-4 load line; rung-5
  `Spawned`/`Failed at`. C4 (video capture) is explicitly out of scope.
  Context info: `Using engine version: 5.6` detection in UE4SS.log
  (`UnrealInitializer.cpp:989`).

## 5. Host-test results (receipts/host-test-receipt.txt)

14/14 PASS on Python 3.9.6 (Darwin arm64), stdlib only:
rung1 pass; rung1 FAIL isolating each of the five conditions (status stuck at
activated, C2 missing, ticks flat, key flip, no refusal); UNVERIFIED-driver
case (counter snapshots move, no pinned record); rung2 pass; rung2
inconclusive honest-negative; empty-directory CANNOT_ASSERT (exit 3);
placeholders with fc1/load-line counts; discovery with fake FTP listings;
--dry-run. `FETCH_FAILED` (exit 4) is FTP-only by construction and was not
exercised (no console contact allowed).

## 6. Non-claims and open items

- No rung is claimed passed; this worker produced classification tooling and
  path facts only.
- The counter-snapshot line format (dispatch counters other than the terminal
  record) is not pinned anywhere in source; rung_check tolerantly accepts
  `ticks_accepted`/`refused_*` keys anywhere in a parsable C2 line but
  requires the pinned native_query_harness record for any strict PASS.
- The five FETCH_FILES assume the composition-5 lineage (entry diag = D1).
  If staging changes which modules are resident, re-derive from PATH-MAP.md
  cites; `--discover` is the runtime fallback.
- post_launch_check.py fetches D1 under key `observer_log`; its path is
  correct for the composition-5 entry lineage (D1) — D2 is a different
  module's file, now fetched separately.

Deliverables: `CLAIM.json` (first), `PATH-MAP.md`, `rung_check.py`,
`host-test/run_host_tests.sh` + `host-test/fixtures/*`,
`receipts/host-test-receipt.txt`, this `REPORT.md` (last).
