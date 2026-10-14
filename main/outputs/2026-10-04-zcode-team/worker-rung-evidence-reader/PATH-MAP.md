# PATH-MAP — exact on-console file path per acceptance-rung observable

worker-rung-evidence-reader, 2026-10-04. Every path below was traced to a
writer constant in the composed runtime source (no console contact). FTP base:
`ftp://ps5-host-not-configured.invalid:2121` (anonymous, one fresh curl per file — the
worker-post-launch-tools pattern). All cites are absolute repo-relative.

## The five fetched files (rung_check.py FETCH_FILES)

| # | Console path | Writer (source of truth) | Cite (file:line) | Feeds |
|---|---|---|---|---|
| C2 | `/data/modbridge-ingame-facts.jsonl` | IgSink in the probe-PRX family (`modbridge_ingame_facts`, `ig_run_*` add-ons). Constant: `IG_LOG_PATH_DEFAULT` / generated `IG_LOG_PATH`. | `outputs/2026-10-04-zcode-team/worker-igf5-diagnosis-rebuild/src/record_sink.c:15`; `.../determinism/run1/generated/ingame_config.h:4` (run2 identical); channel table `worker-acceptance-evidence/EVIDENCE-SPEC.md:25` | RUNG1, RUNG2 (+FC1 records for RUNG3) |
| L | `/data/plugins/UE4SS.log` | Two writers: (1) composition-5 entry failure lines, constant `MODBRIDGE_UE4SS_LOG_PATH`; (2) the runtime's own Output file device: `set_file_name_and_path(m_log_directory / "UE4SS.log")`, with the `__PROSPERO__` native-context branch setting `m_log_directory = m_working_directory` = `/data/plugins`. | `worker-composition-5-rebuild/staging/lineage-overlay-1/lifecycle/runtime_module_entry.c:68`; `port/ue4ss/src/UE4SSProgram.cpp:256-258`; path branch `UE4SSProgram.cpp:778-784`; playbook `worker-first-run-playbook/FIRST-RUN-PLAYBOOK.md:42-51` | RUNG1 context, RUNG3, RUNG4, RUNG5 |
| S | `/data/modbridge/roots/PPSA33286/runtime-status.txt` | `mfa_status_open` joins literal `"runtime-status.txt"` under the mfa root; entry hardcodes `MODBRIDGE_MOD_ROOT "/data/modbridge/roots/PPSA33286"`. `O_TRUNC` per session (last session wins). | `native-coherent-runtime-1/source/activation/modforge_activation.c:599-618`; grammar `modforge_activation.h:137-148`; root constant `worker-composition-5-rebuild/staging/lineage-overlay-1/lifecycle/runtime_module_entry.c:65-66` | RUNG1 gate condition |
| D1 | `/data/townfall_runtime_aligned_entry.log` | Composition-5 entry diagnostic channel (`diagnostic_event`), `PROBE_LOG_PATH` in the staged lineage overlay. Append-only. | `worker-composition-5-rebuild/staging/lineage-overlay-1/generated/probe_config.h:3`; writer `staging/lineage-overlay-1/diagnostic.c:60` | context / RUNG3-5 cross-check (EVIDENCE-SPEC C1 lineage) |
| D2 | `/data/townfall_runtime_aligned_observer.log` | Standalone aligned-entry observer PRX only — its config comment: "distinct from the proven `/data/townfall_runtime_aligned_entry.log` so both modules can be staged alongside each other". | `worker-aligned-entry-observer/source/generated/probe_config.h:3-7` (identical copy `worker-observer-packaging/observer-src/generated/probe_config.h:3-7`) | optional cross-check (counted events) |

Note: `worker-post-launch-tools/post_launch_check.py:190-196` fetches D1 under
the key `observer_log` — correct for the composition-5 entry lineage; D2 is a
different module's file and is fetched separately by rung_check.py.

## RUNG -> observable -> exact format

### RUNG1 — genuine game-thread callback (C2-driven; EVIDENCE-SPEC §RUNG 1)

- Gate: status last state `engine-ready` or `mods-started` (engine-ready-or-later;
  ladder order activated -> engine-ready -> mods-started, mfa .h:143).
- C2 present; `ticks_accepted` strictly increases between two snapshots
  (~3 s apart; `--snapshot-gap`).
- `game_thread_id_key` stable and non-zero across snapshots (format:
  `"0x"` + 16 lowercase hex digits — `NativeQueryHarness.cpp:332-341`).
- >= 1 counted guard refusal (`refused_wrong_thread` /
  `refused_identity_unknown` / `refused_bindings_absent` /
  `refused_layout_gate_closed` >= 1 anywhere in C2).
- Context line (info, not required): `Using engine version: 5.6` in
  UE4SS.log — `port/unreal/src/UnrealInitializer.cpp:989`.

**LOUD CAVEAT (driver not wired):** no target-side caller of
`FormatRecordJsonLine` exists in the composed PRX.
`worker-query-s2-arm/integration-patch/WIRING.md:120-124`: "prerequisite 3
(controller driver: TrySnapshot / SnapshotDispatchCounters /
FormatRecordJsonLine) remains the controller's console-evidence step and needs
no target code." C2 is written only when a probe/add-on PRX that links
`record_sink.c` is staged. rung_check.py therefore reports
`UNVERIFIED_DRIVER` (no native_query_harness line ever seen) instead of a
false PASS, and `--discover` lists `/data` candidates over FTP when the
expected file is absent.

### RUNG2 — valid object query (C2 record; EVIDENCE-SPEC §2.1)

Terminal line written via `FormatRecordJsonLine`
(`worker-query-harness/src/NativeQueryHarness.cpp:402-445`):

```
{"record":"native_query_harness","magic":"NQH1","status":"<StatusName>","engine":"<maj>.<min>","axes":{"non_shipping":N,"stats":N},"storage":"<StorageName>","count_source":"<CountSourceName>","game_thread_id_key":"0x<16 hex>","counts":{"first":N,"second":N,"recorded":N},"delta":N,"monotonic":"<MonotonicName>","identity_checks":N,"threshold":10000,"ticks":{"dispatched":N,"accepted":N,"refusals_between":N},"gate_source":"<label>"}
```

PASS shape: `status=="completed"`, `count_source=="engine_uobject_array"`,
`counts.recorded==2`, both counts `> 10000`, `monotonic in {equal,
non_decreasing}`, key non-zero, `identity_checks==2`,
`ticks.refusals_between==0`, `threshold==10000`, non-empty `gate_source`.
Status strings: `NativeQueryHarness.cpp:357-375`
(`completed`, `inconclusive_below_threshold`; `equal`, `non_decreasing`,
`decreasing`).

### RUNG3 — harmless reflected access (placeholders; EVIDENCE-SPEC §3.1)

UE4SS.log mod-tag lines (rung 3/4 discriminator tails) + FC1 records in C2
(build_id carries the fc1 probe's build id):
- `[TownfallItemSpawner] Scan failed: Item database not ready. Load a save first.` — positive tail
- `[TownfallItemSpawner] Scan failed: attempt to call a nil value (global 'FindFirstOf')` — B2-unresolved fail tail
- FC1-A..D records under the probe build_id in C2 (IgSink envelope)

### RUNG4 — unchanged Item Spawner activation (placeholder; EVIDENCE-SPEC §4.1)

- `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` — expected exactly once, in `/data/plugins/UE4SS.log`.
- Lua print destination proof: `global_print` prepends `[Lua] ` and calls
  `Output::send` -> UE4SS.log file device (`port/ue4ss/src/LuaLibrary.cpp:36-102`,
  registration `src/Mod/LuaMod.cpp:1519-1521`; playbook row 26,
  FIRST-RUN-PLAYBOOK.md:103). No separate console device exists on
  `__PROSPERO__` (UE4SSProgram.cpp:317-318: console unavailable).
- Alternates if the tag is absent (older lineages,
  EVIDENCE-SPEC.md:350-354): `/data/townfall_runtime_buffered_lua.log`,
  `/data/townfall_mod.log` — rung_check.py reports them as NOT-FETCHED
  alternates, they are not writers in this composed source.

### RUNG5 — visible spawn through real input (placeholder; EVIDENCE-SPEC §5.3)

- `[TownfallItemSpawner] Spawned` / `[TownfallItemSpawner] Failed at` counts in UE4SS.log (mod IS:335/341-342).
- EngineTick delivery-gate refusal lines (runtime-side) as negative-control counters.
- C4 (video/frame capture) is OUT OF SCOPE for file assertions — never a file on FTP.

## runtime-status.txt grammar (S)

```
modforge-runtime-status<TAB>1
session<TAB><pid><TAB><monotonic ms at open>
state<TAB><seq><TAB><state><TAB><code><TAB><detail>
note<TAB><seq><TAB><key><TAB><text>          (optional diagnostics)
```
States exactly: `activated`, `engine-ready`, `mods-started`, then terminal
`stopped` or `failed` (`modforge_activation.h:140-148`). One open per session,
`O_TRUNC` (mfa .c:609) — last session wins.

## C2 line envelopes seen in /data/modbridge-ingame-facts.jsonl

1. IgSink probe envelope (`record_sink.c:148-163`):
   `{"marker":"modbridge.ingamefacts","schema":1,"build_id":"<id>","seq":<n>,"exp":"<experiment>","rc":<n>,"errno":<n>,"fields":{...}}`
2. Query-harness terminal record: the `native_query_harness` JSON line above.
rung_check.py classifies each line by its `record` / `marker` key.
