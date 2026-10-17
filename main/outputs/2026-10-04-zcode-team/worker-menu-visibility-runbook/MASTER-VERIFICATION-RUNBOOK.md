# MASTER-VERIFICATION-RUNBOOK — Townfall: launch → visible Item Spawner menu → spawned item

worker-menu-visibility-runbook, 2026-10-04. PROPOSAL / integration doc only.
**REV 2026-10-04b — ninth-slot sync (worker-runbook-eighth-sink):** the adopted ninth-slot
law (console_batch rev `5ca8d1c7`) makes cc1 the **EIGHTH** console evidence sink
(`/data/modbridge-carrier-facts.jsonl`) with the ge.3c census oracle — added to §1, §2
Step C, and a new CP4b row; §1/CP2/G3 tick parentheticals corrected to the runtime
`0x33ac8c0` (linked `0x2fac8c0`) convention per worker-tick-correction-propagation B9; §1
L entry-failure grammar prefixed per entry r4 (fidelity item 3). Edit log:
`outputs/2026-10-04-zcode-team/worker-runbook-eighth-sink/EDIT-LOG.md`.
This is the SINGLE ordered procedure from "user launches Townfall" to "item spawned".
It MERGES the already-delivered audits (no audit is duplicated; each checkpoint cites its
source for depth). No console contact was made by this worker; every pass criterion below is
a prediction from static traces plus host tests — nothing beyond the 3 baseline UE4SS.log
lines has ever been observed on console.

Completion goal: **visible Item Spawner menu + successful in-game spawn** (= rung 4 + rung 5
of the acceptance ladder, worker-first-run-playbook §5).

## Merged sources

| Source audit | What it contributes here |
|---|---|
| worker-pad-chord-audit `PAD-DISPATCH-PATH.md` | OPTIONS→F7 dispatch chain, edge-trigger semantics, 6 keybinds, pad-provider proof (§4) |
| worker-lua-mod-smoke `SMOKE-RESULTS.md`/`REPORT.md` | 90/90 host pass; exact mod log shapes; F7 risk rank 1-3 (§4, §5) |
| worker-engine-tick-fallback `TICK-FALLBACK-DECISION.md` | KEEP EngineTick decision; EngineTick failure shapes (§4a, CP2) |
| worker-tick-callback-audit | **EMPTY at read time.** Detour mechanics reconstructed from worker-tick-address-validation; rung-1 evidence shapes from worker-query-harness §5 (see §7 gaps) |
| worker-lua-bootstrap-audit `MODS-STARTED-PATH.md` | module_start ordering; mods-started gated on engine-ready; main.lua load failure modes (CP1, CP4) |
| worker-tick-address-validation `TICK-VALIDATION-PATH.md` + `LAUNCH2-PREDICTIONS.md` | 9-outcome launch-2 table (CP2). Its `0x37AC8C0` prediction is RETRACTED — runtime Tick = `0x33ac8c0` (linked `0x2fac8c0`), byte-evidenced by worker-seed-table-derivation and propagated by worker-tick-correction-propagation (B9) |
| worker-carrier-probe-build `STAGE-PLAN.md` | cc1 census record shapes, refusal reason codes, grace class, 9-entry ini law (§1 CC, §2 Step C, CP4b) |
| worker-console-batch-runner `console_batch.py` rev `5ca8d1c7` | ge.2 seven-fetch fetch; ge.3b/ge.3c verdict-gate semantics (WARN ≠ FAIL, stop-on-failure groups); CP5 gates on CP0-CP4 only (§1, §2, CP4b) |
| worker-tick-correction-propagation `CORRECTION-REGISTER.md` + worker-ue4ss-docs-fidelity `FIDELITY-REPORT.md` | Tick base-convention correction (B9); entry-r4 L-line timestamp grammar (§1) |
| worker-first-run-playbook `FIRST-RUN-PLAYBOOK.md` | 26-row observable trace (condensed into CP0-CP4), F0-F10 signatures, freshness oracle O1-O5, rung ladder |
| worker-post-launch-tools `post_launch_check.py` | The post-launch classifier command + baselines (§2, step A) |
| worker-rung-evidence-reader `rung_check.py` + `PATH-MAP.md` | One command per rung; the five pinned console paths (§1, §2 step B) |
| worker-rung3-reflection-prep `RUNG3-RECIPE.md` | Rung-3 recipe + guard layers (§3 CP7, gaps) |
| worker-itemspawner-bundle `main.lua` | Exact `Spawned`/`Failed at`/`Menu created`/`Scan failed` strings (§4, §5) |
| worker-real-input-pad `PS5Pad.ini` | The 6 chord mappings staged as `ue4ss/PS5Pad.ini` |

---

## 1. Console evidence files (the eight sinks)

Full writer cites: worker-rung-evidence-reader `PATH-MAP.md`; the three probe-verdict
sinks X/F/CC per their stage plans (cc1: worker-carrier-probe-build `STAGE-PLAN.md` §5-§7).
FTP base `ftp://ps5-host-not-configured.invalid:2121` (anonymous), ONE fresh curl per file, bounded
(`--connect-timeout 10 --max-time 60 --max-filesize 32M`) — the adopted tool flow (ge.2)
fetches S, L, D1, C2 + the three probe JSONL sinks in ONE pass each; D2 is covered by its
staging-flow baseline/absent check (the adopted law counts eight evidence sinks total:
S, L, D1, D2, C2, X, F, CC).

| # | Console path | What it holds |
|---|---|---|
| S | `/data/modbridge/roots/PPSA33286/runtime-status.txt` | Session status ladder. `O_TRUNC` per open — last session wins, per-launch truth. |
| L | `/data/plugins/UE4SS.log` | Append-only. Entry failure lines + UE4SS banner/init lines + all `[Lua]` mod lines + the probes' bounded verdict mirror lines. Holds the 3-line baseline. |
| D1 | `/data/townfall_runtime_aligned_entry.log` | Composition-5 entry diagnostic channel. Append-only; primary per-launch counter. |
| D2 | `/data/townfall_runtime_aligned_observer.log` | Separate standalone observer PRX only (does not interleave with D1). |
| C2 | `/data/modbridge-ingame-facts.jsonl` | Query-harness/probe JSONL (rung-1/2 evidence). **No writer in the composed PRX** — see gap G2. |
| X | `/data/modbridge-exec-write-probe.jsonl` | Exec-write probe verdict sink (build_id `igd1-6ebab07f2111`): per-launch exec-writability verdict records from the staged probe add-on PRX; classified by tool gate ge.3b. |
| F | `/data/modbridge-fc1pool-facts.jsonl` | fc1pool probe verdict sink (build_id `fc1pool1-78508a2e2940`): `fc1pool_result` verdict records — success = `validated`, `reason_code none`, checks **11/11** — plus exactly ONE `FC1POOL:` mirror line in L; classified by ge.3b. |
| CC | `/data/modbridge-carrier-facts.jsonl` | **cc1 carrier census sink (ninth slot, build_id `cc1-0864d65c1baf`)**: FName-pool census evidence from the `modbridge_carrier.prx` ridealong (ini line 9 of the 9-entry layout); classified by ge.3c — shapes below. Exactly ONE `CC1:` line in L per launch. |

Unlike C2, X/F/CC have staged writers THIS session (the ninth-slot `ploader.ini` registers
them; cc1 appended after the igf `= 0` line, mode `= 2`). If a probe PRX is absent/not
registered, the honest ge.3b/ge.3c result is a zero-records WARN — not a launch failure.

Line-shape reference (exact grammars in the source audits):

- S (modforge_activation.h:137-148): `modforge-runtime-status<TAB>1` / `session<TAB><pid><TAB><ms>` / `state<TAB><seq><TAB><state><TAB><code><TAB><detail>`; states exactly `activated` → `engine-ready` → `mods-started`, terminal `stopped` or `failed`.
- D1 (diagnostic.c:146-157): `[PPSA33286] modbridge-runtime-aligned-observer/v1 build=e9a58a9a8740438ebcb59136252a0556 <tag> value=0x<8 hex>`; tags include `module_start_entered`, `constructor_begin`/`constructor_return`, `probe_lifecycle_result`, `runtime_wiring_begin`, `mfa_activated_low/high`, `runtime_start_ok`/`runtime_start_failed`, `module_start_return`, `runtime_stop_*`.
- L entry failure line (entry r4): `[t=<monotonic ms>ms pid=<pid>] modbridge module_start failed: <stage> code=<n> detail=<...>` — the r4 emit prefixes the timestamp at `runtime_module_entry.c:198-202` (fidelity item 3; stage 1=CONVERSION 2=CONTEXT 3=START 4=INTERNAL; mfa codes negative). CAUTION: `post_launch_check.py`'s `FAILURE_LINE_RE` still anchors `^modbridge module_start failed:` WITHOUT the prefix — a prefixed line may classify as unknown; flag to the tool owner, do not self-patch.
- L mod lines: Lua `print` → `[Lua] ` prefix, mod `log()` adds `[TownfallItemSpawner] ` ⇒ final shape `[Lua] [TownfallItemSpawner] <message>` in `/data/plugins/UE4SS.log`.
- C2 terminal record: `{"record":"native_query_harness","magic":"NQH1","status":"completed",...}` (exact field shape in `PATH-MAP.md` RUNG2).
- X/F/CC envelope law (v3 lineage writer, all three sinks): every line strict JSON **< 1 KiB**, shape `{"marker":"modbridge.ingamefacts","schema":1,"build_id":"<id>","seq":N,"exp":"…","rc":0,"errno":0,"fields":{…}}`; each channel is disambiguated by its OWN path + its `build_id` prefix (the marker is the writer's lineage brand, not a channel id).
- CC success sequence (per-launch, cc1 STAGE-PLAN §6-§7): seq1 `entry` → seq2 `startup` (`polls:30, poll_s:4`) → seq3 `mapscan` → 30 `qh_counter` lines (gate opens once admission + object count > 10000) → `census_match` lines → `census_result` → `summary` (`write_failures:0, polls_run:30`); remaining polls continue as `terminal` counters until the ~2-min window closes (30 polls × 4 s).
- CC `census_match` record: one line per matched name carrying `name, table_index, kind, id, block, offset, block_ptr, entry_addr, bytes16` (split out of `census_result` exactly like fc1's entries split, to keep every line < 1 KiB). Matches ride the fixed 53-name table (3 carrier candidates + 19 walk class-strings + 27 ctor-family decoder controls + `None`/`ByteProperty` positive controls + `PostRender`/`ZZZNotARealFunctionQQ` negative controls); expected ≥ 3: `ReceiveTick`, `Tick`, `BlueprintUpdateAnimation` — live truth decides.
- CC `census_result` PASS shape: `verdict:"CENSUS:VALIDATED"`, `reason_code:"none"`, `controls:{None_id_zero:1, ByteProperty_ascii_ok:1}`, `negative_hits:{PostRender:0, ZZZNotARealFunctionQQ:0}`, `matched_count:<n>`, `wide_entries_skipped:<n>`, `census_entries_scanned:<n>`, `census_progress_markers:<n//4096>`, `census_truncated:0`, `budget_entries:1048576`, plus `base_matches_observed_bias:1`, `base_derived`, `pool_runtime`, `current_block:>=1`, `id_none:0`, `id_byteproperty:<u32>`. A `census_truncated:1` VALIDATED verdict is a **BOUNDED census**: `budget_entries` + `census_entries_scanned` carry the real coverage — never read it as full-pool coverage.
- CC refusal shape: `verdict:"CENSUS:REFUSED"` + `reason_code` — census codes `pool_window_uncovered` / `none_ascii_mismatch` / `current_block_insane` / `pool_uninitialized_after_grace`; fc1-chain codes (`pool_window_not_readable`, `blocks0_invalid`, `blocks0_uncovered`, `handles_window_uncovered`, `bootstrap_ids_degenerate`, `handle_block_index_oob`, `block_pointer_invalid`, `block_pointer_window_uncovered`, `entry_window_uncovered`, `entry_wide_flag_set`, `byteproperty_ascii_mismatch`); census-walk codes (`census_block_pointer_invalid`, `census_entry_window_uncovered`); grace-class `map_untrusted` / `anchor_not_admitted` / `census_gate_closed_after_grace` (the bounded window itself was the grace). Then the ONE L line `CC1: build=cc1-0864d65c1baf verdict=…` and the `summary` record.
- L probe mirror lines: exactly ONE `CC1: build=cc1-0864d65c1baf verdict=…` line per launch (≤ 224 B) and one `FC1POOL:` line (fc1); recorded counts, never re-derived.

**Freshness oracle (playbook O1-O5, condensed):** D1 `module_start_entered` count = number of
attempted launches (O1). S header `session <pid> <ms>` identifies the launch; a session is
fresh if its ms > `8866785`-scale PATH_TEST monotonic base (O2). Any L line not matching the 3
baseline prefixes (`PATH_TEST:` / `ACTIVATION_TEST:` / `DIAG: module_start reached`) belongs to
a composition-5+ launch (O3). Failure-line count in append-only L = failed attempts (O5).

---

## 2. Controller commands (run after EVERY launch, ~60 s after the user starts the game)

Records appear ~1 min after launch; the probe census windows are slower — the cc1 window is
30 polls × 4 s (~2 min), and the tool's handoff text allows the observer log/jsonl and the
cc1 census ~2-3 min post-launch. If fetches fail, wait ~60 s and re-run before touching staging.

### Step A — post-launch classifier (always first)

```
python3 outputs/2026-10-04-zcode-team/worker-post-launch-tools/post_launch_check.py
```

- Fetches S + L + D1 in one pass; prints an evidence summary and ends with exactly one
  `VERDICT:` line plus `NEXT:` guidance.
- Options: `--json` (machine output) · `--dry-run` (print the 3 curl commands, fetch nothing) ·
  `--baseline-file FILE` (override embedded baseline) · `--from-dir DIR` (fixture mode, never
  for console) · `--host ps5-host-not-configured.invalid --port 2121 --timeout 60 --max-bytes 33554432`.
- Exit codes: `0` verdict produced · `4` FETCH_FAILED (all three unreachable → retry, check FTP).
- Verdicts and decisive next action: `FAILED_<stage>` (mfa code -2 → /data timing window,
  relaunch once before re-staging; -6/-8 → staged-vs-final divergence, detail names the file;
  -3/-4/-5/-12 → loadlist regenerate from final-package; bridge 1/2/3/4 →
  CONVERSION/CONTEXT/START/INTERNAL) · `FAILED_STATUS` (F6/F7 silent-trap shapes) ·
  `MODS_STARTED` (then check `[TownfallItemSpawner]` prints = CP4 gate) ·
  `ENGINE_READY`/`ACTIVATED` → re-run in 30-60 s; frozen = F7/F8 bounded-wait hang check ·
  `STOPPED` → F10 · `UNKNOWN_NEW_LINES` / `NOT_STARTED` → wait, then F0 (staging/ploader).

Embedded baseline (verbatim self-checked by the tool; L2 reconstructed — see gap G6):

```
PATH_TEST: /data/plugins/UE4SS.log writable at 8866785633337 monotonic-ns
ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: 6d6f64666f7267652d6c6f61646c6973
DIAG: module_start reached at 1791155755.879 pid=298 build=sdg2-6f404cee6b94 variant=imp
```

### Step B — rung assertions (one command per rung)

```
python3 outputs/2026-10-04-zcode-team/worker-rung-evidence-reader/rung_check.py                 # RUNG1+RUNG2 strict gate
python3 outputs/2026-10-04-zcode-team/worker-rung-evidence-reader/rung_check.py --rung RUNG1    # 2 C2 snapshots, --snapshot-gap 3.0 s apart
python3 outputs/2026-10-04-zcode-team/worker-rung-evidence-reader/rung_check.py --rung RUNG2    # terminal NQH1 record
python3 outputs/2026-10-04-zcode-team/worker-rung-evidence-reader/rung_check.py --discover      # FTP LIST fallback when files missing
```

- Exit codes: `0` PASS · `1` FAIL/UNVERIFIED · `2` usage · `3` CANNOT_ASSERT (evidence missing) ·
  `4` FETCH_FAILED (retry ~60 s). Also `--json`, `--dry-run`, `--from-dir` (fixtures only).
- **Current baselines:**
  - RUNG1 (strict): status last state engine-ready/mods-started AND `ticks_accepted` strictly
    increases between snapshots AND exactly one non-zero `game_thread_id_key`
    (`0x`+16 lowercase hex, stable) AND ≥1 counted guard refusal
    (`refused_wrong_thread`/`refused_identity_unknown`/`refused_bindings_absent`/
    `refused_layout_gate_closed`) AND a `native_query_harness` line present.
  - RUNG2 (strict): terminal record `status=="completed"`, `count_source=="engine_uobject_array"`,
    `counts.recorded==2`, both counts `> 10000`, `threshold==10000`,
    `monotonic in {equal, non_decreasing}`, key non-zero, `identity_checks==2`,
    `ticks.refusals_between==0`, non-empty `gate_source`. `inconclusive_below_threshold` is an
    honest negative — FAIL, never re-rate unchanged evidence.
  - RUNG3-5: rung_check placeholders (count-only; never affect exit code). RUNG3 recipe in §3 CP7.
- **LOUD baseline caveat:** with no driver add-on staged, RUNG1/RUNG2 verdict will be
  `UNVERIFIED`/`CANNOT_ASSERT` with `UNVERIFIED_DRIVER` — that is the honest result, not a tool
  failure (gap G2).

### Step C — probe verdict sinks X/F/CC (tool ge.2 fetch + ge.3b/ge.3c classify; ninth slot)

The tool's game-evidence flow order is: ge.1 Step A → **ge.2 fetches all eight sinks in one
pass** (S, L, D1, C2, X, F, CC — one bounded curl each) → ge.3 CP0-CP4 → **ge.3b** (X + F) →
**ge.3c** (CC census) → ge.4/ge.4b rungs → ge.5 CP5 handoff. Both verdict gates run AFTER the
CP gate and BEFORE any OPTIONS press; they are EVIDENCE gates about probe observations, not
launch gates.

- ge.3b (X + F): freshness vs the staging baselines proves THIS launch's records; strict-JSON
  envelope law; fc1 success = `fc1pool_result` verdict `validated`, `reason_code none`,
  checks **11/11**, + exactly one `FC1POOL:` mirror line in L (recorded). Recorded refusals
  are honest terminal evidence per the probes' stage plans — never relaunch an identical
  rejected layout.
- ge.3c (CC census): success = the §1 CC sequence ending `census_result` `CENSUS:VALIDATED`
  with controls {`None_id_zero:1`, `ByteProperty_ascii_ok:1`} + negative_hits
  {`PostRender:0`, `ZZZNotARealFunctionQQ:0`} + `summary` `write_failures:0` + exactly ONE
  `CC1:` line in L.
- **Honest WARN-on-refusal rule (the core semantics):** a `CENSUS:REFUSED` verdict — any
  reason_code, including the grace-class `map_untrusted` / `anchor_not_admitted` /
  `census_gate_closed_after_grace` — is honest TERMINAL WARN evidence. WARN never halts the
  flow and never blocks CP5 (the tool gates CP5 on CP0-CP4 green only). The remedy for a
  grace-class refusal is a NEW staged window with NEW ids — never re-rate unchanged evidence,
  never replay the identical rejected layout. Zero fresh records = WARN too (probe did not run
  or the ~2-min window is still open; check the staging SC-5b OK lines).
- FAIL shapes are structural evidence problems, not launch failures: unparseable lines, lines
  ≥ 1 KiB (envelope-law breach), or a foreign `build_id` in CC (single-writer discipline
  violated or stale sink). FAIL halts the flow (stop-on-first-failure) — stop and reconcile
  the sink before any rating; never replay.
- Scope limit (cc1 STAGE-PLAN §9): a VALIDATED census evidences WHICH of the 53 fixed names
  are interned in the LIVE FName pool for that launch only. It never certifies FUNC_Native,
  invocation rate, game-thread identity, or any UFunction-object walk.

### Manual fetch fallback (the exact curl commands)

```
curl --connect-timeout 10 --max-time 60 'ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/runtime-status.txt'
curl --connect-timeout 10 --max-time 60 'ftp://ps5-host-not-configured.invalid:2121/data/plugins/UE4SS.log'
curl --connect-timeout 10 --max-time 60 'ftp://ps5-host-not-configured.invalid:2121/data/townfall_runtime_aligned_entry.log'
curl --connect-timeout 10 --max-time 60 'ftp://ps5-host-not-configured.invalid:2121/data/modbridge-ingame-facts.jsonl'
curl --connect-timeout 10 --max-time 60 'ftp://ps5-host-not-configured.invalid:2121/data/modbridge-exec-write-probe.jsonl'
curl --connect-timeout 10 --max-time 60 'ftp://ps5-host-not-configured.invalid:2121/data/modbridge-fc1pool-facts.jsonl'
curl --connect-timeout 10 --max-time 60 'ftp://ps5-host-not-configured.invalid:2121/data/modbridge-carrier-facts.jsonl'
```

---

## 3. The ordered procedure: launch → item spawned

Triage order on any anomaly (playbook): **read S first** (per-session truth) → **D1 events**
(count launches, locate stage) → **L** (attribution vs baseline, append order).

### CP0 — the composed PRX actually ran (module_start entered)

- File/shape: D1 gains exactly one new `module_start_entered value=0x00000000` line per launch,
  followed by `constructor_begin`/`constructor_return` pairs, `probe_lifecycle_result
  value=0x00000000`, `runtime_wiring_begin value=0x00000000` (playbook rows 1-5).
- Pass: one new `module_start_entered`…`module_start_return value=0x00000000` pair for this
  launch, with `probe_lifecycle_result 0x0`.
- On failure:
  - No new `module_start_entered`, S/L unchanged → **F0**: PRX never ran. Check loadlist/staging
    and ploader; the game running unmodded is by-design safe.
  - `probe_lifecycle_result value=0x<nonzero>` + `runtime_start_skipped` → **F1**: probe
    lifecycle refused wiring; `constructor_return` pairs identify the failing ctor index.

### CP1 — activation consumed; runtime constructed; pad provider opened

- File/shape: S header + `state 1 activated 0` (row 16). L banner sequence (row 13):
  `Native console, GUI, crash dumping and filesystem watching are unavailable` → `UE4SS - v… -
  Git SHA #…` → `root directory: /data/plugins` → `mods directories:` `[0] ue4ss/Mods` →
  `Setting up mods...` (row 14). D1: `mfa_activated_low/high`, then `runtime_start_ok
  value=0x00000000` + `module_start_return value=0x00000000` (row 17).
- Pass: fresh S session reaches `activated`; banner + `Setting up mods...` in L; no
  `modbridge module_start failed:` line.
- Proves (among others): **the pad provider activated** — `ConfigureRetainedPS5PadInput` runs in
  the backend ctor BEFORE `activated` is written and throws on any failure (ini hard-refusals
  with line numbers, `provider factory returned no source`, `PS5Pad input unavailable`); a
  missing ini means no input source, and the program ctor then throws
  `Native supplied input source is unavailable or failed activation` → `failed` status. So
  `activated` ⇒ ini parsed AND `scePadOpen` succeeded (pad-chord-audit hops 1-3).
- On failure (exact signatures in playbook F2-F5):
  - D1 `mfa_activate_failed value=0x<mfa>` + L `…mfa_activate_failed code=<mfa> detail=…` → F2;
    decode mfa code; `error.path` names the offending staged file.
  - D1 `runtime_start_failed value=0x0000000{1,2,3}` → F3 (capture/conversion), F4 (context —
    verify `/data/plugins/modbridge_supplier_composition.prx` exists), F5 (prepare/start threw).
  - D1 `runtime_start_ok 0x0` but S says `failed` and no engine-ready → **F6 silent trap**: trust
    S, treat as thread-spawn failure (errno in detail).

### CP2 — engine-ready; Tick detour installed at the predicted address

- File/shape: S `state 2 engine-ready 0` (row 21). L gains the module scan lines (row 19) and,
  from the init thread, the tick resolution line (LAUNCH2-PREDICTIONS, staged
  `UnrealInitializer.cpp:1318`):
  `GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x33ac8c0)`.
- Pass: `engine-ready` present AND that line shows **both** addresses equal to `0x33ac8c0`
  (runtime value; linked `0x2fac8c0` + base `0x400000` — the once-circulated `0x37ac8c0`
  "= base + linked `0x33ac8c0`" was a base-convention error, retracted by the SEED-TABLE
  byte decode; linked `0x33ac8c0` decodes mid-instruction. Scan preferred by
  `EngineTickResolveMethod=Scan`; fidelity note: under the armed seed path the scan arm is
  skipped — the seed constant IS the pinned value).
  The pair (address equality + CP3 tick liveness) separates outcome #1 from #6/#7.
- On failure (LAUNCH2 outcome table, rows 2-9):
  - L `Scan failed, falling back to vtable address for GameEngine::Tick` → outcome #2: acceptable
    (vtable rescued), but record it — scan uniqueness was not demonstrated live.
  - S terminal `failed` code 1 with L/throw detail `Native game-thread initializer callback
    registration failed` and NO engine-ready/mods-started → **outcome #3/#5 (THE real EngineTick
    failure)**: resolve or PLH patch refused; loud by design (tick-fallback §4). Never expect a
    "dead menu" from EngineTick — that world is unreachable (see §4a).
  - S `failed` detail `PS scan timed out` → outcome #8: required scan slots never satisfied.
  - `mods-started` fires then the game crashes on next frames → **outcome #6 (RISK)**:
    wrong-but-plausible hot address; nothing in the runtime systematically refuses it
    (TICK-VALIDATION-PATH §6). Console crash report is the evidence.
  - `mods-started` fires, game runs, but no tick observations ever → **outcome #7** (cold wrong
    address); the ONLY "stuck at mods-started" shape — distinguished from #1 by a flat tick
    counter (CP3).
- Do NOT re-derive or patch addresses from here; report and reconcile with the director.

### CP3 — mods-started, then the loop is alive (rung-1 liveness)

- File/shape: S `state 3 mods-started 0` immediately after engine-ready (row 22). Ordering
  nuance: `activated` → `engine-ready` → `mods-started` are all written BEFORE main.lua runs
  (bootstrap-audit §2) — the mod banner arrives later on the event thread.
- Pass: fresh `mods-started`; and for genuine tick liveness, `rung_check.py --rung RUNG1` (§2
  step B baselines). If C2 has no writer staged, the honest verdict is `UNVERIFIED_DRIVER` —
  do not treat the absence of rung-1 evidence as launch failure (gap G2).
- On failure:
  - S frozen at `activated`, L static after module lines → **F8** init hang: bounded wait
    watching mtimes (the only deadline is the 5-minute StartupWait, which converts to `failed`,
    not a hang); then unload and read `runtime_stop_result`.
  - S `activated` then `state 2 failed 1 <what>` → **F7**: init thread threw before engine-ready;
    `what` is the typed init error.
  - `mods-started` + flat/absent ticks + game alive → outcome #7 above (wrong cold Tick address).

### CP4 — the Item Spawner mod actually loaded (rung 4 gate)

- File/shape: exactly one new line in L (main.lua:746; playbook row 26):
  `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.`
- Pass: that line present once, fresh; S still ends `mods-started`. (A second load after a map
  change is not expected; the mod re-opens cleanly per smoke phase 3.)
- On failure (**F9 — script layer, NON-fatal; S still reaches mods-started**; fix mod/activation
  content, do NOT re-stage the PRX):
  - `Error loading script: <msg>` / `Error executing script: <msg>` → syntax/runtime error in
    main.lua (host smoke says none exists for the audited bytes — verify staged sha256
    `f58604…9658` main.lua, `5a8f66…15a7` UEHelpers.lua).
  - `…module 'UEHelpers' not found:…` → shared-mod layout missing from the activation.
  - `Failed to execute main script: <path>` / `Main script 'main.lua' not found in scripts
    directory: …` (casing!) → script missing from the mod snapshot.
  - `Exception during mod startup: …` / `Mod is absent from native Lua source: …` → inventory
    mismatch; check the staged `ue4ss/Mods/TownfallItemSpawner` tree and enabled.txt.
  - Whole-inventory failure → S `failed` with `Native UE4SS inventory rejected: …` (fatal,
    unlike F9).
- Absence of the banner with NO error line: mod not in the startup list — re-check the
  activation inventory before anything else.

### CP4b — ninth-slot verdict sinks X/F/CC (evidence-only; §2 Step C in full)

- File/shape: X `modbridge-exec-write-probe.jsonl` + F `modbridge-fc1pool-facts.jsonl`
  (ge.3b) and CC `modbridge-carrier-facts.jsonl` (ge.3c) gain THIS launch's fresh records —
  envelope law + success sequences per §1; cc1's census window is 30 polls × 4 s (~2 min).
- Pass: ge.3b `fc1pool_result validated` 11/11 + one `FC1POOL:` line; ge.3c
  `census_result CENSUS:VALIDATED` (controls 1/1, negative hits 0, `write_failures:0`,
  exactly one `CC1:` line in L).
- WARN is the honest terminal for: `CENSUS:REFUSED` (any reason_code — the grace-class
  remedy is a NEW staged window with NEW ids, never re-rating unchanged evidence, never
  replaying the identical rejected layout), zero fresh records (window still open / probe
  not registered), no `census_result` yet (incomplete sequence), `write_failures` ≠ 0, or
  ≠ 1 `CC1:` line. WARN never blocks CP5.
- FAIL = structural sink breach (unparseable / ≥ 1 KiB / foreign `build_id` lines): halt,
  reconcile the sink; an evidence-path problem is not a launch failure and is never replayed.
- This checkpoint NEVER gates CP5 (the tool gates the OPTIONS handoff on CP0-CP4 green only)
  and never advances/retards the rung ladder; a census_truncated VALIDATED verdict is a
  BOUNDED census — budget/scanned carry the real coverage.

### CP5 — OPTIONS pressed: the menu opens (full protocol in §4)

Gate: CP1-CP4 pass; user has LOADED A SAVE and is in-game. Pass = menu visibly on screen +
`Menu created.` evidence in L. Failures route through §4's four shapes.

### CP6 — CROSS pressed: item spawns (full path in §5)

Pass = `[Lua] [TownfallItemSpawner] Spawned <Item> x<N> ID=<id>` in L **and** the user can see
the new pickup in the world. Failures route through §5's four stage signatures.

### CP7 — optional deeper rungs (do not block the goal)

- RUNG2 object-count record via `rung_check.py --rung RUNG2` (§2 baselines) — only if the
  rung-1 driver is staged (gap G2).
- RUNG3 harmless reflected access: separate `Rung3Probe` mod per `RUNG3-RECIPE.md` §4-6 —
  PASS needs one `[Rung3Probe] find GameEngine valid=true stable=true` line after mods-started
  PLUS same-launch FC-1 C2 records (else signature F1, not evidence). Its four guard layers
  (width table, publication, missing-key, pusher-null) make a bad-offset crash unreachable
  (UNREACHABLE = signature F11; if it happens, stop and reconcile).

---

## 4. The OPTIONS test protocol (CP5 in full)

### 4.0 Preconditions — each with what proves it

| # | Precondition | Proof required before pressing |
|---|---|---|
| P1 | Runtime at mods-started | Fresh S session: `state 3 mods-started 0`, session-ms fresh (O2). |
| P2 | Pad provider active + chord map parsed | Structural: `activated` was reached at all (CP1 proof — provider activation is a hard precondition of construction) AND the CP4 load banner exists (main.lua's six `RegisterKeyBind` calls would have thrown `Input registration unavailable: no active input source` — Handler.cpp:109,123 — if no active source existed; that throw is caught and logged, so the banner could not be the only artifact… the banner appearing proves registration succeeded). Staging pin: `ue4ss/PS5Pad.ini` in the activated root, final package sha256 `1d919fbf…` 2960 B, `[Bindings]` `OPTIONS = F7` + the 5 nav chords (worker-real-input-pad). |
| P3 | A save is LOADED, user is in-game | User fact. Guarantees `FindFirstOf("ItemDatabaseSubsystem")` resolves (main.lua:62-67). Without it the protocol takes the "Load a save first" branch (§4.3c) — which is itself a valid test (T3). |
| P4 | Pad is the first logged-in user's controller | `[PS5Pad] User = -1` → first logged-in user (PS5PadSource.cpp:37-42); only one controller paired. |
| P5 | Event loop alive | CP3 liveness (rung-1 ticks or, absent driver, the game visibly running with mods-started). |

### 4.1 What the user presses

**OPTIONS alone — a brief press and release, ONE press.**

- The chord is single-button: match is `(buttons & OPTIONS) == OPTIONS` — extra unrelated buttons
  do NOT suppress the event; nothing else is required (PAD-DISPATCH-PATH §2).
- **Edge-triggered**: exactly one F7 per physical press; holding longer still yields exactly one;
  no autorepeat; no keyup synthesized. Poll cadence ~200 Hz on the UE4SS-UpdateThread with a
  duplicate-sample guard.
- F7 is a TOGGLE: same key opens and closes. Second press closes the menu (visibility restore +
  input unlock; closing is silent — no log line by design).
- The five nav binds (UP/DOWN/LEFT/RIGHT/CROSS) are silent no-ops while the menu is closed
  (main.lua:699-723) — pressing them menu-closed produces no log and no effect BY DESIGN; do not
  misread that as "pad dead".

### 4.2 Expected visible result (success)

- The menu overlay appears on screen: canvas panel with two borders + vertical box, title,
  12 row texts, controls line (built via WidgetBlueprintLibrary:Create → AddToViewport(9999) →
  SetVisibility Visible), game look/move input locked while open (mod compensates per
  RUNTIME-REQUIREMENTS C4).
- UP / DOWN move the selection; LEFT / RIGHT change page; CROSS activates the selection.
- Menu UI status line updates (item count on open, spawn result on CROSS).
- Known accepted limitation: UE4SS keybinds are non-exclusive — while open, the same buttons
  still reach the game (PS5Pad.ini header note).

### 4.3 Expected log lines — success, then each failure shape

All mod lines appear in `/data/plugins/UE4SS.log` with the `[Lua] [TownfallItemSpawner] `
prefix (prefix provenance: PATH-MAP §RUNG4).

**SUCCESS (first open with a save):** two new lines, in order —

```
[Lua] [TownfallItemSpawner] <N> spawnable pickups found. Pages=<P>
[Lua] [TownfallItemSpawner] Menu created.
```

Re-opening later (items cached) logs only the F7 toggle with no new line; that is success too.

**(a) EngineTick unavailable** — exact throw text (LuaMod.cpp:4719, caught by the keybind
dispatch lambda LuaMod.cpp:1944-1947), menu never opens:

```
[Lua] ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available (AOB scan failed)
```

plus a Lua traceback or the literal `No traceback` suffix. **Decisive action:** per
TICK-FALLBACK-DECISION this line is UNREACHABLE after `mods-started` (availability is fixed
during init, before engine-ready; reaching mods-started proves the detour resolved, installed,
and fired). Seeing it is an anomaly: check whether `UE4SS-settings.ini` line 12 was restaged —
the variant `ExecuteInGameThread: ProcessEvent method requested but ProcessEvent hook is not
available (AOB scan failed)` PROVES a restage (its ini sha `d8faa4ec (v3-authorized; pre-v3 was 9f3fa127)…30f7f` would diverge; any
size/hash pin must be re-verified). The real EngineTick failure shape is CP2's fatal
(`Native game-thread initializer callback registration failed` + `failed` status), not this line.
Keep the KEEP-EngineTick decision; no ini change.

**(b) UI build error** — menu stays closed, retry-safe (guards verified in smoke phase 3):

```
[Lua] [TownfallItemSpawner] WidgetBlueprintLibrary not found.
```
or
```
[Lua] [TownfallItemSpawner] UI creation failed: <lua error text>
```

**Decisive action:** `WidgetBlueprintLibrary not found.` = `/Script/UMG.Default__WidgetBlueprintLibrary`
unresolved → reflection/layout question (rung-3 territory; StaticFindObject refused — check
`Menu created` absence vs scan success). `UI creation failed:` carries the Lua error — read it:
UMG class/construct failures → layout gate; property-write errors → extended-pair gap (RUNG3-RECIPE
F4 shape: `Tried getting member variable '<Class>::<Key>' …`). Session stays healthy; iterate on
the named call, distinct revision IDs, no re-rating unchanged evidence.

**(c) Load a save first** — the mod's own guard (main.lua:66), menu never opens:

```
[Lua] [TownfallItemSpawner] Scan failed: Mods/TownfallItemSpawner/Scripts/main.lua:66: Item database not ready. Load a save first.
```

**Decisive action:** expected when P3 was skipped (host smoke reproduces it exactly). Load a
save, then press OPTIONS again. No fix needed; this is the T3 negative test passing.

**(d) No reaction at all** — OPTIONS pressed, ZERO new lines in L:

**Decisive action, in order:**
1. Confirm CP4: is the load banner present? If not, this is a CP4 problem (mod never loaded) —
   go to §3 CP4 failure list. (Banner present ⇒ the 6 binds registered ⇒ input source was active
   at load.)
2. Confirm staging: `ue4ss/PS5Pad.ini` present in the activated root (absent ini → no pad bridge;
   but see CP1 — construction would have died before `activated`, so a healthy session implies it
   was read; verify the staged hash `1d919fbf…` if in doubt).
3. Confirm the press was a clean edge (brief press-release; the duplicate-sample guard drops a
   second keydown for the SAME held sample; a stuck button held from before registration cannot
   phantom-fire — edge state tracked since poll start).
4. Confirm liveness: rung-1 ticks advancing (or outcome #7 check if driver staged). A dead
   UpdateThread = no polling = no dispatch.
5. Known non-causes (do not chase): nav binds silent menu-closed (§4.1); game also receiving the
   inputs (non-exclusive by design); ESC unused (no ESCAPE bind exists).
6. If all above hold and there is still zero reaction: the OPTIONS→F7 edge never fired on
   console — that is the first genuine live gap in the dispatch chain (pad read vs subscription).
   Escalate to the director with D1/S/L tails; do NOT re-press repeatedly hoping for logs.

### 4.4 Protocol as tests (pending-tests ledger; see §7)

- T2: with save → §4.3 SUCCESS shapes + visible menu; then arrows/CROSS reachable (§5).
- T3: without save → exactly shape (c); then load save → T2.
- T4: second OPTIONS → menu closes, input unlocked, no new log lines.
- T5: UP/DOWN selection moves (visual), LEFT/RIGHT page (visual; no per-move log lines exist —
  do not expect any).

---

## 5. The spawn path (CP6 in full): menu selection → spawned item

### 5.1 Controller sequence

Menu open (§4) → UP/DOWN to highlight an item row → LEFT/RIGHT to page if needed → **CROSS**
(→ `RETURN`, `activateSelection()` → `row.activate()` → `spawnSelected` inside an
`ExecuteInGameThread` closure = game thread by construction; `LoadAsset` throws off-thread and
cannot here — smoke phase 4).

### 5.2 The four stages (main.lua, smoke-traced)

| stage string | What runs | Host-verified shape |
|---|---|---|
| `ground trace` | `system:LineTraceSingle(...)` from the player into the scene; reads out-param table `hit` (`.ImpactPoint`, `.ImpactNormal.Z`) | contract verified Lua-side; **port-side marshalling = modreqs B5, never engine-executed — risk rank 3** |
| `load pickup class` | `StaticFindObject(<pickup class path>)` with fallback; invalid → error | resolves real objects only on console |
| `spawn actor` | `gameplay:BeginDeferredActorSpawnFromClass(class, transform)` → `actor:FinishSpawningActor(transform, 0)` | call shapes verified; never engine-executed |
| `configure pickup` | requires `PickupInteractable` component + valid inventory ID; property writes `PickupAmount`, `bAutoDestroyOnPickUp`; `SetEnabled` / `SetActorHiddenInGame` / `SetActorEnableCollision` | full flow passed end-to-end on host stubs (941-call trace) |

### 5.3 What proves a REAL spawn

1. **Log line (decisive console evidence):**
   ```
   [Lua] [TownfallItemSpawner] Spawned <ItemName> x<quantity> ID=<inventoryID>
   ```
   Host-observed shape: `Spawned Antibiotics x1 ID=DeprecatedItem`. The `ID=` value is read back
   from the spawned actor's `PickupInteractable` component AFTER `FinishSpawningActor` — a live
   readback from a real actor object, not a constant (main.lua:308-335).
2. **World-side observable (user confirmation):** a physical pickup actor appears at the aim
   point in front of the player — spawned deferred-then-finished, collision enabled, not hidden,
   interaction enabled, positioned at the ground-trace impact point. The user sees it on screen
   and can walk to it. The in-menu status line also switches to `Spawned <Item> x<N>`.
   A log line WITHOUT a visible actor = investigate (spawned into unloaded/hidden space); a
   visible actor without the log line should not be possible (the log is written from the
   post-readback path).
3. Rung-5 accounting: per playbook §5, rung 5 = exactly this (pad-only input → visible spawn);
   C4 video capture is out of scope for file assertions.

### 5.4 Failure shapes (each names its stage)

```
[Lua] [TownfallItemSpawner] Failed at <stage>. See UE4SS.log. <err>
```

| stage in message | Root cause → decisive action |
|---|---|
| `ground trace` | LineTraceSingle out-param marshalling failed (rank-3 B5 risk) → capture `<err>`, reconcile marshalling against LUA-API-MATRIX B5; distinct-revision fix, no blind retry. Also check the user is aiming at geometry (no hit → no spawn point). |
| `load pickup class` | Pickup class path absent in this build (`StaticFindObject` returned invalid; smoke could only verify stubs) → the logged `<err>` contains the exact `Could not find class: <path>`; re-derive the class path from the real build, new revision. |
| `spawn actor` | BeginDeferred/FinishSpawning refused or returned invalid → `<err>` carries `Could not construct`-family text; game-thread identity already guaranteed — treat as engine-side refusal, reconcile with rung-2/rung-3 evidence. |
| `configure pickup` | `Spawned actor has no PickupInteractable.` / `Spawned pickup has no valid inventory ID.` → an actor spawned but is not a live configured pickup: THIS IS NOT A SUCCESSFUL SPAWN even if something appeared; fix property/component assumptions. |

Failure is retry-safe (guards + pcall; second CROSS is safe). Every observed failure gets a
distinct revision; never re-rate unchanged evidence.

---

## 6. Consolidated failure playbook (triage index)

Triage order: **S → D1 → L** (§3). Full signatures: playbook F0-F10, LAUNCH2 #1-9,
RUNG3-RECIPE F1-F11, mfa codes in post_launch REPORT §"What it classifies".

| Symptom (what you saw) | Signature | Go to |
|---|---|---|
| Game boots unmodded, nothing anywhere | F0 (PRX never ran) | CP0 |
| `runtime_start_failed value=0x1/2/3` | F3/F4/F5 | CP1 |
| S `failed` with no matching L line | F6/F7 silent traps | CP1/CP3 |
| S frozen at `activated` | F8 hang (or #8 scan timeout with detail) | CP3/CP2 |
| `Native game-thread initializer callback registration failed` | Tick resolve/patch fatal (#3/#5) | CP2 |
| Tick line ≠ `0x33ac8c0`, or crash right after mods-started | #6/#7 wrong address (RISK) | CP2/CP3 |
| mods-started but no mod banner | F9 script layer | CP4 |
| Banner but OPTIONS does nothing on screen | §4.3d no-reaction ladder | CP5 |
| `ExecuteInGameThread: … hook is not available` | §4.3a anomaly (restage check) | CP5 |
| `UI creation failed:` / `WidgetBlueprintLibrary not found.` | §4.3b | CP5 |
| `Item database not ready. Load a save first.` | §4.3c expected guard | CP5 |
| `Failed at ground trace/load pickup class/spawn actor/configure pickup` | §5.4 stage table | CP6 |
| `Spawned` logged but nothing visible | §5.3 item 2 caveat | CP6 |
| S `stopped` + `runtime_stop_result` nonzero | F10 stop failure | §6, playbook F10 |
| Rung3Probe `valid=false` | F1 (FName unresolved; NOT evidence of absence) / F2 (genuine negative) | CP7, RUNG3-RECIPE §7 |

---

## 7. Honest gaps — what CANNOT be verified without console runs

**G1 — worker-tick-callback-audit is empty.** Its directory exists but holds no files; the
string `RUNG1-EVIDENCE` occurs nowhere in the tree (same finding as pad-chord-audit REPORT §3
and tick-address-validation REPORT "Honest limits"). The detour-mechanism facts it was to
contribute are reconstructed here from worker-tick-address-validation (DetourInstance
InstallHook :149-171 / ActivateHook :174-186 / EnsureHooked :232-238 via the run-3
`unreal-unrealinitializer.d` header set), and rung-1 evidence shapes from worker-query-harness
REPORT §5. If that worker's output lands later, reconcile before relying on §3 CP2/CP3 details.

**G2 — rung-1/rung-2 console evidence has NO writer staged.** `FormatRecordJsonLine` has no
target-side caller in the composed PRX (rung-evidence-reader REPORT §3; WIRING.md:120-124);
C2 (`/data/modbridge-ingame-facts.jsonl`) is written only by probe add-on PRXs. Consequence:
`rung_check.py --rung RUNG1/RUNG2` will honestly report `UNVERIFIED_DRIVER`/`CANNOT_ASSERT`
this launch. The controller must either stage a driver add-on (per that report's option (a)) or
accept rung-1 as unevidenced this launch — engine-ready/mods-started remain init-thread facts,
NOT game-thread proof (playbook rung-1 honesty note).

**G3 — the tick address prediction is now byte-evidenced; the LIVE line is not.** Runtime
Tick = `0x33ac8c0` (linked `0x2fac8c0` + base `0x400000`). The original structural-only
prediction (`0x37ac8c0`) is RETRACTED; the byte-compare gap is CLOSED linked-side by the
SEED-TABLE byte decode (canonical prologue at linked `0x2fac8c0`; linked `0x33ac8c0` decodes
mid-instruction). What remains console-only: the live `GameEngine::Tick address …` line has
never been observed (G7), and outcomes #6 (crash) and #7 (silent dead dispatch) are still
only distinguishable AFTER a console run, via CP2's address line + CP3 tick liveness.

**G4 — the OPTIONS→menu hop has never fired on any engine.** Pad chords were parsed and
unit-audited; the Lua side ran 90/90 on host stubs. The live `scePadReadState` edge →
keybind callback → `toggleMenu` chain, and UMG widget creation on the real renderer (host used
stubs; menu layout/legibility on screen is unverified), are console-only facts. §4's four
failure shapes + the no-reaction ladder exist precisely because the live behavior is
unproven. "Visible" is ultimately the user's eyes; no log substitutes.

**G5 — the spawn path is engine-unexecuted.** LineTraceSingle out-param marshalling (B5) is the
highest-risk unevidenced row; `BeginDeferredActorSpawnFromClass`/`FinishSpawningActor` and the
property writes have host-shape evidence only. §5.4's per-stage signatures are predictions.

**G6 — baseline line L2 is reconstructed**, not a verbatim console readback
(post_launch REPORT honesty note). Classification tolerates drift via prefix rules; a
`--baseline-file` override exists if the console shows different bytes.

**G7 — EngineTick "success" is inferable, not observed.** No console has ever logged
`GameEngine::Tick address …`; its success is implied by any engine-ready/mods-started ack
(init ordering). The optional direct confirmation is grepping L for the CP2 address line.

### Pending tests (explicit ledger — none may be marked done from host evidence)

- T1: first composition-5 launch through CP0→CP4 (rung 0 + rung 4 gate).
- T2: OPTIONS with save loaded → §4.2/§4.3 SUCCESS.
- T3: OPTIONS without save → §4.3(c) guard shape.
- T4: second OPTIONS → menu closes, input unlocked, silent.
- T5: UP/DOWN/LEFT/RIGHT navigation (visual; no log lines expected).
- T6: CROSS spawn → §5.3 (log + visible actor).
- T7: rung-1 driver decision: stage driver add-on, or record rung-1 as UNVERIFIED this launch.
- T8 (stretch): RUNG3 Rung3Probe mod per RUNG3-RECIPE sequencing (rungs 1-2 + extended pair +
  FC-1 prerequisites first).
- T9 (ninth slot): CP4b from a real launch — ge.3b `fc1pool_result validated` 11/11 and the
  cc1 census window closing with `census_result CENSUS:VALIDATED` + exactly one `CC1:` line
  (or an honest terminal `CENSUS:REFUSED` WARN recorded with its reason_code; never re-rated,
  a revision needs a NEW staged window). None of T9 is satisfiable from host evidence.

**Do-not-claim guard:** rung advancement requires the observations above from a real console
launch; host tests, drafted logs, and the 3 baseline lines never advance a rung
(playbook §5). This runbook classifies and routes; it never certifies.
