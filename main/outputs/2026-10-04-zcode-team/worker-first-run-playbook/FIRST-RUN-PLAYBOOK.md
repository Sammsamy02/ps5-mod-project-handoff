# FIRST-RUN-PLAYBOOK — composition-5 first launch (Silent Hill: Townfall, PS5)

Status: PROPOSAL / documentation only. Traced from OUR source with the delivered
module_start wiring in place. No console contact was made; nothing here is a
claim that any line beyond the 3 known baseline lines has ever appeared on the
console. Worker: worker-first-run-playbook (restart), 2026-10-04.

## 0. Sources traced (all paths relative to workspace root)

| Unit | Path |
|---|---|
| Entry (delivered wiring) | `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_module_entry.c` ("entry") |
| Bridge | `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge.cpp` / `.h` ("bridge") |
| Ack supplier / start chain | `outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp` ("ack-supplier") |
| Backend | `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/ue4ss_backend.cpp` ("backend") |
| Owner + RuntimeAckSink | `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/runtime_owner.cpp` ("owner") |
| UE4SS program | `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/UE4SSProgram.cpp` ("program") |
| Lua mod runtime | same tree `src/Mod/LuaMod.cpp` ("luamod") |
| print | same tree `src/LuaLibrary.cpp` |
| Vendored mfa | `outputs/2026-10-04-zcode-team/worker-f-r2-fix/composition/vendored/modforge_activation.c` / `.h` ("mfa") |
| Diagnostic channel | `outputs/2026-10-04-zcode-team/worker-aligned-entry-observer/source/diagnostic.c` + `source/generated/probe_config.h` |
| Conversion refusals | `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp` ("convert") |
| Pad profile | `outputs/2026-10-04-zcode-team/worker-real-input-pad/PS5Pad.ini` |
| Item Spawner mod | `outputs/2026-10-04-zcode-team/worker-itemspawner-bundle/bundle/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` |
| Known baseline lines | `outputs/2026-10-04-zcode-team/worker-minimal-path-test/STAGE-PLAN.md`, `worker-composition-5-rebuild/transport-ledger.composition5-mirror.json` |

Honesty flag: the task context cites entry sha `1c041f8b…`; the on-disk
`runtime_module_entry.c` hashes `f3654703bf08d9e00d5035c118a0b6f2c5194e41`.
All cites below are against the on-disk file; reconcile the sha in staging
Gate 1 (entry attestation) before launch.

## 1. The three sinks (everything observable lives here)

### 1.1 Aligned-entry observer log — `/data/townfall_runtime_aligned_observer.log`
- Opened `O_WRONLY|O_CREAT|O_APPEND` at `module_start` (diagnostic.c
  `diagnostic_open_retained`; `PROBE_LOG_PATH` in probe_config.h:6). Append-only
  across launches — never truncated.
- Event line format (diagnostic.c `diagnostic_event`, lines 146–157):
  `[PPSA33286] modbridge-runtime-aligned-observer/v1 build=e9a58a9a8740438ebcb59136252a0556 <tag> value=0x<8 hex>`
  (`PROBE_TITLE`/`PROBE_MARKER`, probe_config.h:2,7). No wall timestamp.

### 1.2 UE4SS log — `/data/plugins/UE4SS.log`
- Same file for two writers:
  1. Entry failure lines (entry:166–192), opened append per failure:
     `modbridge module_start failed: <stage> code=<n> detail=<sanitized>`
     Printable-ASCII sanitized; NO timestamp, NO pid (see §5).
  2. The runtime's own `Output::send` lines once `UE4SSProgram` constructs:
     `setup_paths` on `__PROSPERO__` with a mod source sets
     log dir = working dir = root dir = module parent = `/data/plugins`
     (program:752–784, branch at 776–784), so the runtime writes its own
     `UE4SS.log` here. Append-only across launches.

### 1.3 Runtime status — `/data/modbridge/roots/PPSA33286/runtime-status.txt`
- Format (mfa .h:137–148; mfa .c:599–646). `mfa_status_open` uses `O_TRUNC`
  (mfa .c:609): EVERY open REPLACES the file — one file per session, last
  session wins.
  ```
  modforge-runtime-status<TAB>1
  session<TAB><pid><TAB><monotonic ms at open>
  state<TAB><seq><TAB><state><TAB><code><TAB><detail>
  ```
- States (mfa .h:143): `activated`, `engine-ready`, `mods-started`, `failed`,
  `stopped`. Detail sanitized (`?` for control bytes). "The last state line is
  current; a new session replaces the file."
- Two independent openers exist at different times (never concurrently):
  the ack sink inside prepare (owner:130–153), and the entry's terminal
  `write_status_failed` (entry:198–219). Because open truncates, a terminal
  entry write REPLACES any earlier ack lines of the same launch (see F6/F7).

## 2. Actual startup trace with the wiring in place (execution order)

Threads: **L** = loader thread inside `module_start`; **I** = serialized init
thread (`RuntimeOwner::start` spawns it, owner:31); **E** = UE4SS event-loop
thread (program:673).

| # | Thread | Sink | Observable (exact) | Source | Proves |
|---|---|---|---|---|---|
| 1 | L | observer | `…modbridge-runtime-aligned-observer/v1 build=… module_start_entered value=0x00000000` | entry:305 | New entry really runs (one event per launch; count them) |
| 2 | L | observer | `entry_incoming_sp_mod16 value=0x…`, `entry_call_sp_mod16 value=0x…`, `constructor_count value=0x<N>` | entry:311–314 | Preserved probe prologue intact |
| 3 | L | observer | per ctor: `constructor_begin value=0x<idx>` then `constructor_return value=0x<result>` | entry:101–106 | All UE4SS init_array ctors ran exception-caught |
| 4 | L | observer | `probe_lifecycle_result value=0x00000000` | entry:318 | Probe lifecycle clean; wiring is attempted only on 0 |
| 5 | L | observer | `runtime_wiring_begin value=0x00000000` | entry:234 | Wiring started (exactly once; entry:232) |
| 6 | L | — | `mfa_activate` reads+validates root `/data/modbridge/roots/PPSA33286` all-or-nothing | entry:238–250 | Last moment /data is read from the game process |
| 7 | L | observer | `mfa_activated_low value=0x…` + `mfa_activated_high value=0x…` | entry:270–273 | Activation loaded; low/high = preloaded byte count |
| 8 | L | — | bridge stage 1: sce os-ops + `RetainedLiveModuleFactsSupplier({352,4,fw=0,false,"eboot.bin"})`, `capture()` VMMAP-first, opt-in `allow_vm_scan_base_as_module_image_base=true`, `TryMakeNativeLoaderFactsFromSupplier` | bridge:153–169 | Live loader facts captured; identity route is vm-scan + VMMAP span |
| 9 | L | — | bridge stage 2: `MakeNativeProgramContext` validates facts and BOTH files exist NOW (`/data/plugins/modbridge_supplier_composition.prx`, game exe) | bridge:176–182 | Context valid while /data reachable |
| 10 | L | — | `StartActivatedRuntimeWithAck`: `MakeRuntimeAckConfig(mod_root)` (status_root=exact root, pid, CLOCK_MONOTONIC ms) → `InstallDefaultEngineBindings()` → `PrepareActivatedUE4SSRuntime` → `owner->start()` | ack-supplier:68–95 | INTEGRATION-CHECKLIST STEP 6 order |
| 11 | L | — | `ValidateNativeProgramContext` again; `MakeActivatedUE4SSModSource`; **ack sink opens: status file created+truncated, header written** | backend:89–102; owner:130–153; mfa .c:609–618 | RUNTIME-ACK point 1; session identity (pid+ms) |
| 12 | L | — | backend ctor: pad profile read via mod source (`ue4ss/PS5Pad.ini`), `mod_startup_policy=Deferred`, engine-callback chain installed | backend:24–46; PS5Pad.ini header | Pad chord mapping armed before any mod runs |
| 13 | L | UE4SS.log | program ctor banner, in order: `Native console, GUI, crash dumping and filesystem watching are unavailable` → `UE4SS - v<maj>.<min>.<patch><pre> - Git SHA #<sha>` → `Timezone: UTC (local disabled …)` → `UE4SS Build Configuration: <cfg> (<compiler>)` → `Found configuration for game: …` or `No specific game configuration found…` → `Config: …`, `root directory: /data/plugins`, `working directory: /data/plugins`, `game executable directory: …`, `game executable: … (… bytes)`, `mods directories:`, `[0] ue4ss/Mods`, `log directory: /data/plugins`, `object dumper directory: …` | program:316,318,347,361,416–435; logical mods dir program:834–839 | UE4SS construction reached; paths = the native-context branch |
| 14 | L | UE4SS.log | `Setting up mods...` (native branch: LuaMod objects created from the activation's lua_mods; NO "Loading mods from:" line, no mods.txt lines) | program:1739,1741–1751 | Buffered mod inventory from the activation |
| 15 | L | — | `start_native_mods`: `setup_mods(); install_cpp_mods(); m_native_mods_started=true; start_cpp_mods(Yes)` — Lua main.lua NOT yet run (Deferred) | program:549–592,580–583 | Inventory verified ≠ scripts executed |
| 16 | L | status | `state<TAB>1<TAB>activated<TAB>0<TAB>` | backend:107; owner:171 | Activation consumed, title matched, inventory OK |
| 17 | L | observer | `runtime_start_ok value=0x00000000` then `module_start_return value=0x00000000`; module_start returns 0 | entry:285–286,330 | Owner started; loader thread done |
| 18 | I | — | `program_->init()`: `setup_unreal()` (signature scan) | backend:65; program:600–619 | Unreal modules located against the live game |
| 19 | I | UE4SS.log | `Unreal Engine modules (modular|non-modular)` + per-module `Main @ 0x<size…>` lines | program:621,644 | eboot.bin scanned; addresses are real |
| 20 | I | UE4SS.log | property/offset setup lines (e.g. `MemberVariableLayout.ini loaded` when applicable) + `share_lua_functions` | program:648–652,940 | Reflectivity prepared |
| 21 | I | status | `state<TAB>2<TAB>engine-ready<TAB>0<TAB>` — `on_engine_initialized` fired at the real site | program:654–661; backend:33–46; owner:172 | Unreal setup done; owner state EngineInitialized |
| 22 | I | status | `state<TAB>3<TAB>mods-started<TAB>0<TAB>` immediately after (buffered inventory verified) | backend:36–45; owner:173 | RUNTIME-ACK points 2+3 complete |
| 23 | I | — | event loop thread spawns; `init()` blocks joining it | program:673–678 | Tick loop exists |
| 24 | E | — | `update()` first calls `on_program_start()`: `install_lua_mods(); LuaMod::on_program_start(); fire_program_start_for_cpp_mods(); start_lua_mods()` | program:1499–1514,1481–1490 | Deferred Lua startup happens HERE, after the acks |
| 25 | E | UE4SS.log | per startup mod (native branch): `start_mods<LuaMod>` → `mod->start_mod()` | program:1946–1956; luamod:6440 | VM prepared, async thread, custom loader |
| 26 | E | UE4SS.log | mod prints: `[Lua] [TownfallItemSpawner] <msg>…` (`print` → `global_print` prefixes `[Lua] `; main.lua `log()` prefixes `[TownfallItemSpawner] `) | luamod:1521; LuaLibrary.cpp:35–44; main.lua:5–6 | **"mod loaded": our mod's own first prints** |

Successful end state: UE4SS.log ends the launch with the mod's own prints;
status last line = `mods-started`; observer shows exactly one
`module_start_entered`…`module_start_return` pair per launch (plus stop events
only on unload).

Key ordering nuance (matters for triage): `activated` → `engine-ready` →
`mods-started` are all written **before main.lua runs**. The Lua VM executes
main.lua later on the event thread. A broken mod script therefore does NOT
remove `mods-started` (see F9).

## 3. FAILURE-SIGNATURES (last-observable → next move)

Stage codes (bridge.h:36–41): `0`=OK, `1`=CONVERSION, `2`=CONTEXT, `3`=START,
`4`=INTERNAL. Entry failure line: `modbridge module_start failed: <stage>
code=<n> detail=<…>` (entry:172–179); status `failed` line written by the
entry uses the MRS code; status `failed` written by the ack/owner uses code 1.

| Failure | Last observable (files, in order) | Next diagnostic move |
|---|---|---|
| F0 module_start refused early (guard/diag channel) | observer ABSENT or no new `module_start_entered`; NO UE4SS.log line; NO status change; game unmodded | entry returns -20/-21/-22 silently by design (entry:297–304). Only the observer can prove arrival — no new event means the PRX never ran: check loadlist/staging, loader logs |
| F1 probe lifecycle failed (ctor threw / worker refused) | observer: `probe_lifecycle_result value=0x<nonzero>` + `runtime_start_skipped value=0x<result>`; NO UE4SS.log entry line, NO status write (wiring never attempted) | `constructor_return` pairs identify the failing ctor index; result = probe-lifecycle code; fix that unit; wiring discipline preserved (entry:319–328) |
| F2 `mfa_activate` refused | observer: `mfa_activate_failed value=0x<mfa code>` (+ `mfa_activate_os_error value=0x<errno>`); UE4SS.log: `…failed: mfa_activate_failed code=<mfa> detail=<MFA_NAME> <message> <path>`; status: NEW single-line session `state 1 failed <mfa> <stage detail>` | Decode `mfa_result_name`; verify root staged exactly at `/data/modbridge/roots/PPSA33286` (loadlist.txt, title match, ue4ss/ files); `error.path` names the offending file (entry:251–268) |
| F3 capture/conversion refused (incl. vm-scan) | observer: `runtime_start_failed value=0x00000001`; UE4SS.log: `… runtime_start_failed code=1 detail=<convert message>`; status: single `failed code=1` line | Messages are the convert strings (convert:115–299), e.g. `supplier has not captured facts (status NotCaptured); refusing conversion`, `vm-scan image span is empty or unrepresentable`, `no observed mapping interval intersects the module image extent with valid geometry`. Check VMMAP-first capture status and eboot.bin suffix match; opt-in is already set (bridge:165), so a base-refusal means the snapshot route differed |
| F4 context validation threw | observer: `runtime_start_failed value=0x00000002`; UE4SS.log: `… code=2 detail=<what>`; status: single `failed code=2` line | `MakeNativeProgramContext` throws on refused facts or missing regular files (bridge:176–182): verify the PRX path string `/data/plugins/modbridge_supplier_composition.prx` exists at that exact path and the observed game exe path is real |
| F5 prepare/start threw | observer: `runtime_start_failed value=0x00000003`; UE4SS.log: `… code=3 detail=<exception what>`; status: header + `state 1 failed 3 <detail>` (the entry's terminal open TRUNCATES the ack sink's earlier `failed code=1` line — expect exactly one state line) | detail is `StartActivatedRuntimeWithAck`'s exception text or `StartActivatedRuntimeWithAck returned null owner` (bridge:193–214); backend construction errors surface as `Native UE4SS construction rejected: <msg>` (backend:49–50) or the ack's `failed` reason (backend:110–121). UE4SS.log banner shows how far construction got |
| F6 start() refused (thread spawn) — silent OK trap | observer: `runtime_start_ok value=0x00000000`; status: header + `state 1 failed 1 <thread error>` (written by owner:31–40); NO engine-ready; game unmodded | Status file is the truth, not `runtime_start_ok`: bridge checks slot non-null but not start()'s bool (ack-supplier:93–95). Treat "observer ok + status failed + no engine-ready" as spawn failure; errno is in the owner's detail |
| F7 init thread threw before engine-ready | status: `activated` then `state 2 failed 1 <what>` (owner:111–118); UE4SS.log ends after the banner or mid module lines; no engine-ready | `what` = `native_startup_error_message` (typed init error) or `Native runtime init returned without a stop request` (owner:106–108). Banner/module lines show whether setup_unreal started; bounded wait then treat as hang if no growth |
| F8 init hang (tick never fires) | status frozen at `activated` (no engine-ready); UE4SS.log static after module lines; observer static after `runtime_start_ok` | Engine callback never reached (program:654–661). Wait a bounded window watching file mtimes; unload → `module_stop` events + `runtime_stop_result` show whether join succeeds or hangs (entry:340–348, owner:43–77) |
| F9 Lua init / script error (mod-level, NON-fatal) | status still completes to `mods-started`; UE4SS.log carries one of: `Error loading script: <msg>` (luamod:6413), `Error executing script: <msg>` (6424), `Exception in load_and_execute_script: …` (6435), `Failed to execute main script: <path>` (6465), `Main script 'main.lua' not found in scripts directory: …` (6471–6473), `Exception during mod startup: …` (6478), `Mod is absent from native Lua source: …` (911), `Mod path doesn't exist …` (935) | Session is alive; only the mod is dead. Fix main.lua/activation content; do NOT re-stage the PRX. Absence of `[Lua] [TownfallItemSpawner]` prints + one of these lines = script layer |
| F10 post-ready crash/stop | status last line `stopped` (single, owner:178–192) or `failed`; observer gains `runtime_stop_begin`/`runtime_stop_result value=0x…` (+ `runtime_stop_failed` entry line + UE4SS.log `… runtime_stop_failed code=<n>` on nonzero) | `runtime_stop_result` nonzero → owner stop reported failure; check init-thread join vs `MRS_ERR_STOP_EXN` (bridge:100–120) |

Triage order: read status file FIRST (per-session, pid+timestamped, truncates
per launch) → observer events (count launches, locate stage) → UE4SS.log
(collision of entry lines + runtime lines; use append order relative to the
3 baseline lines).

## 4. FRESHNESS-ORACLE — attributing new UE4SS.log lines per launch

Current console `/data/plugins/UE4SS.log` baseline (exactly 3 known lines,
all earlier probes):
1. `PATH_TEST: /data/plugins/UE4SS.log writable at <monotonic-ns> monotonic-ns`
   (worker-minimal-path-test STAGE-PLAN.md:47)
2. `ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: <32 hex>` (:48)
3. `DIAG: module_start reached at 1791155755.879 pid=298 build=sdg2-6f404cee6b94 variant=imp`
   (transport-ledger.composition5-mirror.json:10956)

Rules (proposal):
- O1 (launch count): every `module_start_entered` event in the observer log =
  exactly one attempted launch of the new PRX (append-only, never truncated).
  This is the primary per-launch counter.
- O2 (session identity): each launch that reaches ack-open or any terminal
  failure REPLACES `runtime-status.txt` (O_TRUNC) with a fresh header
  `session <pid> <monotonic-ms>`; file mtime + header = unambiguous launch
  identity and recency. Monotonic bases are comparable across status and the
  PATH_TEST/DIAG lines (all CLOCK_MONOTONIC/DIAG epoch on the same console
  boot), so ordering vs the baseline is reconstructible.
- O3 (UE4SS.log attribution): any line NOT matching the 3 baseline formats and
  not matching `PATH_TEST:`/`ACTIVATION_TEST:`/`DIAG: module_start reached`
  prefixes belongs to a composition-5+ launch. Within a launch, the
  timestamp-less entry failure lines are ordered by position: they appear
  strictly BEFORE the program banner (the entry writes them before any runtime
  thread exists, entry:161–166) and after the baseline lines.
- O4 (recommendation): YES — the next entry revision should add a monotonic
  ms + pid to the failure line (e.g. `… code=<n> at <ms> pid=<pid>`). Both
  primitives are already measured in the frozen import contract and the entry
  already computes monotonic ms for `write_status_failed` (entry:202–204) and
  links `getpid`; the change is bounded. It is NOT a launch blocker: O1+O2
  give unambiguous attribution today, and O3 suffices for the first run.
- O5 (deduction rule while lines are timestamp-less): number of
  `modbridge module_start failed:` lines = number of failed wiring attempts;
  a launch whose status file exists but has no matching new UE4SS.log failure
  line did not fail on a terminal entry path (it either succeeded to
  `module_start_return` or died inside the runtime — check status last state).

## 5. SUCCESS CRITERIA per acceptance rung (what counts, what it proves)

Shared precondition for every rung: game boots and runs UNMODDED-behavior-safe
on any failure (entry returns 0 on wiring failure, entry:35–38); status file's
last state is the session truth.

- Rung 0 — composed runtime alive (implicit gate): status reaches
  `activated`→`engine-ready`→`mods-started` (§2 #16,21,22) and UE4SS.log shows
  the banner + `Setting up mods...` + no `modbridge module_start failed:` line.
  Proves: entry→bridge→prepare→start chain works on-console.
- Rung 1 — game-thread callback proven: a real-process query record per
  worker-query-harness REPORT §5 (worker-query-harness/REPORT.md:176–200):
  `status:"completed"` with `count_source:"engine_uobject_array"`,
  `counts.recorded":2`, both counts `> 10000`, `monotonic` Equal/NonDecreasing,
  `game_thread_id_key` non-zero and equal across reads, ≥1
  `refused_wrong_thread`/`refused_identity_unknown` counter, zero
  `refused_layout_gate_closed`. Proves: the four engine seams installed by
  `InstallDefaultEngineBindings` (ack-supplier:82) resolve real game-thread
  identity from a genuine dispatch. Honest note: `engine-ready` alone proves
  only the init thread finished Unreal setup — it is NOT game-thread evidence.
- Rung 2 — valid object query: the same completed record's counts are
  plausible for live Townfall (stable/monotonic, not
  `inconclusive_below_threshold`) with layout-gate provenance naming
  `PublishStagedNativeLayoutMaps`. Proves `UObjectArray::GetNumElements`
  through published maps on the real process.
- Rung 3 — harmless reflected access: read-only finds succeed or fail cleanly
  without crashing: e.g. Item Spawner's
  `StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary")`
  (main.lua:499) returns a valid object or clean nil (pcall-wrapped
  `obj:IsValid()`, main.lua:9–13); no new `Error`-level lines from reflected
  reads; game continues. Proves: reflectivity reads through the layout are
  safe pre-spawn.
- Rung 4 — Item Spawner activated unchanged: UE4SS.log shows
  `[Lua] [TownfallItemSpawner] …` startup prints (§2 #26); status ends
  `mods-started`; the menu opens via the pad mapping (`OPTIONS` → `F7`,
  PS5Pad.ini `INPUT-MAP` section) with the mod's own binds wrapped in
  `ExecuteInGameThread` (main.lua:694–729 per PS5Pad.ini header). Proves:
  deferred Lua startup ran the real mod, unmodified, and pad-to-key dispatch
  reaches it.
- Rung 5 — visible spawn via real input: with the menu open, selecting an item
  and pressing `CROSS` (→ `RETURN`, main.lua:708) produces a visibly spawned
  item in-game. Proves: end-to-end real input (pad chord only — NO keyboard
  provider exists on this target) + game-thread callback execution +
  reflected construct/spawn — the full ladder.

Do-not-claim guard: rung advancement requires the observations above from the
real console launch; host tests, drafted logs, or the 3 baseline lines never
advance a rung.
