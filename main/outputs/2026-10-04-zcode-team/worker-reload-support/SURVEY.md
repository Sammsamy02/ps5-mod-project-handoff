# SURVEY — worker-reload-support (2026-10-05)

Question: once runtime + game-thread delivery work, can repeated diagnostics and
supported Lua reloads reuse ONE game session instead of relaunching Townfall?

Method: READ-ONLY survey of the existing port lineage. No code changed, no
console contact, no runtime claims beyond what the cited files state. All paths
relative to the workspace root. Line numbers are from the exact staged files
named below.

Files surveyed:
- `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/` (canonical bootstrap: `runtime_owner.{hpp,cpp}`, `ue4ss_backend.{hpp,cpp}`, `activated_mod_source.*`)
- `outputs/2026-10-04-zcode-team/worker-ack-integration-deep/` (vendored ack bootstrap, WIRING-SPEC, patch set)
- `outputs/2026-10-04-zcode-team/worker-diagnostic-sink/staging/diagnostic.c`
- `outputs/2026-10-04-zcode-team/worker-durable-events/{REPORT,PROCEDURE}.md`
- `outputs/2026-10-04-zcode-team/worker-console-batch-runner/console_batch.py`
- `outputs/2026-10-04-zcode-controller/LAUNCH-DAY-10.md`
- `outputs/2026-10-04-zcode-team/worker-game-session-ridealongs/STAGE-PLAN-COMBINED.md`
- `outputs/2026-10-04-zcode-team/worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp` (3278 lines)
- `outputs/2026-10-04-zcode-team/worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp` (8342 lines)
- `outputs/2026-10-04-zcode-controller/launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp`

---

## 1. Command / diagnostic channels today

### 1.1 The out-of-game console payload workflow (host side)

- Transport: ploader manager HTTP `:8084` (`/list_payloads`, `/loadpayload:<path>`,
  `/manage:upload`, `/manage:check`) and FTP `:2121` STOR/RETR/DELE under `/data`
  (`console_batch.py:142-143`, `:2088-2090`, `:2275-2277`, `:2317`, `:2387`).
- Staging is a game-CLOSED operation: the launch-10 gate requires the game closed,
  one sanctioned FSELF STOR to `/data/plugins/modbridge_supplier_composition.prx`
  with pre-image backup + byte-exact readback (`LAUNCH-DAY-10.md:7`, `:18-20`).
- Launch is manual (user starts Townfall, `LAUNCH-DAY-10.md:24`); post-launch
  evidence is PULL-ONLY: one curl-FTP fetch of five files (`LAUNCH-DAY-10.md:26-33`),
  read by `evidence.sh` / `epoch_view.py`. Every diagnostic cycle so far = a fresh
  game epoch (new pid); the durable sdiag jsonl accumulates across epochs and is
  read per-pid (`worker-durable-events/PROCEDURE.md:14-17, 30-35`; REPORT `:26-31`).
- The ONLY existing command-file + ack pattern is the e2e intake:
  `data/modbridge/intake/e2e-cmd.json` is STORed once, dispatched once via
  `/loadpayload:`, then must be GONE with a `.consumed` readback hash match
  (`console_batch.py:203-204`, `:2236-2240`, `:2380-2420`, ack law `:1056-1061`).
  Its consumer is the SEPARATE one-shot installer payload
  `MODBRIDGE_INST_E2E.elf` (`console_batch.py:179-188`) — NOT the in-game
  composition. No route consumes any intake file inside the game runtime
  (grep for `intake`/`e2e-cmd` across `outputs/ps5-mod-bridge/` and the
  composition staging trees: no runtime hit).

### 1.2 In-game output channels (composition) — output only, no input route

- `diagnostic_event(const char* tag, uint32_t value)` — bootstrap
  `diagnostic.c:349-359`. Writes two sinks: (a) the shared entry text channel
  (`PROBE_LOG_PATH`, opened append-only `diagnostic.c:79-92`) and (b) the r-s1
  additive durable JSONL `/data/modbridge-supplier-diag.jsonl`
  (`diagnostic.c:169-173`) with `{marker,schema,seq,pid,tag,value,value_hex,t_ms}`
  (`diagnostic.c:291-347`). Append-only, bounded retries, every failure tolerated,
  never fatal, uses only the three already-frozen kernel imports
  (`diagnostic.c:1-22`). Carries: one tag string + one u32 per event. Emit sites
  are all module_start-stage code in the bridge (e.g.
  `runtime_start_bridge_r14_diag_spanpub.cpp:341-342`, `:403-413`, `:443-467`,
  `:693-694`, `:736-740`, `:1042-1099`); nothing emits after startup today.
- `RuntimeAckSink` → `<mod_root>/runtime-status.txt` — one sink per process;
  points `activated()` / `failed(code,detail)` / `stopped()`
  (`bootstrap/runtime_owner.hpp:52-56`; born in the ack supplier per
  `worker-ack-integration-deep/REPORT.md:73-80`). LATEST-STATE-ONLY: the status
  file is last-writer-wins with a terminal O_TRUNC write — it is session truth,
  not history; the durable history is the sdiag jsonl
  (`worker-durable-events/PROCEDURE.md:4-8`; REPORT `:33-35, :74-79`). Not
  repeatable within a session (one session row, closed at the terminal write).
- `UE4SS.log` — native output devices on PROSPERO
  (`UE4SSProgram.cpp:253-256`); Lua prints and `Output::send` append in-session;
  fetched via FTP. Composition failure lines use a separate `write_ue4ss_log_line`
  path strictly before runtime threads start
  (`worker-game-session-ridealongs/STAGE-PLAN-COMBINED.md` §5 table row 1).
- Ridealong windows (observer 30×2 s; qdrv1 30×4 s + harness dispatch after 6
  admitted polls) run ONCE from each ridealong's own `module_start`; "a new
  window needs a NEW launch (never re-rate unchanged evidence)"
  (`STAGE-PLAN-COMBINED.md` §5, Threads/bounds bullets). Not in-session repeatable.
- In-game Lua "command-ish" surfaces: keybinds via the pad input source are
  PROVEN in-session (F7/OPTIONS menu probe, `LAUNCH-DAY-10.md:24`, §4b). The
  console-exec Lua registrations exist (`RegisterProcessConsoleExecPreHook`
  `LuaMod.cpp:3452`, `RegisterProcessConsoleExecPostHook` `:3473`,
  `RegisterConsoleCommandGlobalHandler` `:3578`) but in the R3 pump-pending
  composed world the exec/console/loadmap detour arms stay unregistered for the
  process lifetime with no re-arm (`LuaMod.cpp:7432-7447`) — not a dependable
  command route today.

### 1.3 Bridge read/poll interfaces (`runtime_start_bridge_r14_diag_spanpub.cpp`)

- Published span accessors (extern "C", read-only, populated ONCE at the
  module_start scan): `bridge_readable_span_count` `:748-751`,
  `bridge_readable_span_at` `:753-761`, `bridge_capture_envelope` `:767-774`;
  publish happens inside the scan wrapper via `bridge_spanpub_publish`
  (`:714-741`, called at `:982`) from the SAME frozen capture snapshot.
- Lease registry `g_image_regions`/`g_lease_spans` (`:567-576`) with
  `bridge_lease_span_containing` (`:592-601`) is TU-static — no external
  accessor; registration is frozen-snapshot-derived
  (`bridge_register_from_frozen_snapshot` `:675-695`).
- `diagnostic_event` is declared extern "C" at `:119` and is callable from any
  TU linked into the same PRX ("no new imports" precedent, `:117-119`, `:47-52`).
- Lifecycle entries: `modbridge_runtime_start_with_activation` (`:909`) refuses
  re-entry ("runtime already started", `:933-936`);
  `modbridge_runtime_stop` (`:887-907`) joins via `StopRuntimeWithAck`.
- There is NO poll/command interface in the bridge: everything is start-time
  work or diagnostics-out.

---

## 2. Lua reload support in the runtime lineage

### 2.1 The settings/watcher route is explicitly OFF on PROSPERO

- `UE4SSProgram::validate_native_services` (`UE4SSProgram.cpp:728`) is called
  under `#ifdef __PROSPERO__` at `:248-250` and THROWS at `:737-738`:
  `if (settings_manager.General.EnableHotReloadSystem || settings_manager.General.EnableAutoReloadingLuaMods) throw …("Native hot reload and filesystem watcher services are unavailable")`.
  Both settings must stay false or startup refuses.
- The Windows file-watcher auto-reload is not compiled on PS5:
  watcher construction + `EnableAutoReloadingLuaMods` block is
  `#ifndef __PROSPERO__` (`UE4SSProgram.cpp:1535-1568`), the event-loop
  `process_sync_request` is likewise guarded (`:1693-1698`). The per-mod
  auto-reload body (uninstall → reconstruct `LuaMod` → `start_mod`, "Auto-reloading
  Lua mod '{}'", `:1580-1595`) lives inside that guarded block.
- The HotReloadKey keybind → `queue_reinstall_mods()` (`:1458-1464`) compiles on
  PROSPERO but is unreachable: its gating setting can never pass validation
  (`:737-738`).

### 2.2 The reload machinery itself is compiled and PS5-aware

- Full-reinstall: `UE4SSProgram::queue_reinstall_mods` `UE4SSProgram.cpp:2280`
  (PROSPERO gate `native_mods_enabled()` `:2283`, def at `:553`; marshals to the
  event loop via `queue_event` `:2285-2288`, def `:2871`; then
  `uninstall_mods()` → clear custom properties + static ConstructObject
  callbacks → `setup_mods(); start_cpp_mods(); start_lua_mods()` `:2300-2311`;
  re-fires unreal-init/program-start `:2316-2324`).
- `uninstall_mods` `:2170`; on PROSPERO an exception during any `mod->uninstall()`
  is FATAL: `fprintf(stderr, "Native mod uninstall failed; unloading is unsafe…")`
  + `std::terminate()` (`:2192-2200`; also cpp-mod path `:2217`).
- Install path: `setup_mods` `:1753`; `install_mods` template `:1816`;
  `install_mods<CppMod>` `:1856`; `install_mods<LuaMod>` `:1861`.
- Per-mod family (all event-loop marshaled, all `native_mods_enabled()`-gated):
  `queue_reinstall_mod(LuaMod*)` `:2330` (saves name/path, `mod->uninstall()`,
  removes the mod's keybinds `:2349-2356`, rebuilds one `LuaMod`,
  `start_mod()` `:2364-2372`); `queue_uninstall_mod(LuaMod*)` `:2380`;
  ModId variants `:2417` / `:2444`; `queue_reinstall_mod_by_name(string)`
  `:2471` (+ string_view `:2495`); `queue_uninstall_mod_by_name` `:2500`
  (+ `:2524`); `queue_start_lua_mod_by_path` `:2529`.
- Lua-callable (registered unconditionally, no PROSPERO ifdef in the block):
  `RestartCurrentMod` `LuaMod.cpp:6500`; `UninstallCurrentMod` `:6512`;
  `RestartMod(string mod_name)` → `queue_reinstall_mod_by_name` `:6532-6541`;
  `UninstallMod(string mod_name)` → `queue_uninstall_mod_by_name` `:6549-6558`.
- PROSPERO safety: `LuaMod::uninstall` (`LuaMod.cpp:7039`) refuses while a Lua
  action is dispatching — `throw std::runtime_error("Cannot synchronously
  uninstall a mod during Lua action dispatch; queue uninstall instead")`
  (`:7041-7049`) — then stops the async worker before teardown (`:7051-7060`).
- Honest limits: `LuaMod::on_program_start` (`LuaMod.cpp:7397`) arms detour
  families ONCE per process; in the R3 pump-pending world those arms are
  deferred with "no re-arm" declared explicitly (`:7432-7447`), and the R3
  marshal calibration consumer is idempotent-once (`:4776-4780`). A reload
  restarts Lua mods; it does NOT restore lost native hook arms.

### 2.3 What triggers a reload today

Nothing in the composed PS5 runtime can trigger one. The two upstream triggers
are the hot-reload keybind (`:1458-1464`, settings-dead) and the file watcher
(`:1535+`, not compiled). The only live callers of the reload machinery are the
Lua functions `RestartMod`/`UninstallMod`/`RestartCurrentMod`
(`LuaMod.cpp:6500-6558`) — which themselves need an in-session Lua trigger
(keybind / hook / delayed action) to fire.

---

## 3. Verdict

### (a) Repeated diagnostics in ONE session

- **Lua-driven diagnostics: SUPPORTED TODAY.** Pad input + game-thread delivery
  are proven in-session (F7/OPTIONS probe workflow, `LAUNCH-DAY-10.md:24`, §4b);
  repeated Lua prints/probes append to `UE4SS.log` and are fetched by the
  existing per-run FTP read (`LAUNCH-DAY-10.md:26-35`). Re-running a
  Lua-side probe needs no relaunch — only an action in the same session.
- **Bridge/entry-stage diagnostics (diagnostic_event → sdiag jsonl;
  runtime-status): NOT REPEATABLE in-session today.** All emit sites run during
  `module_start` stages (§1.2), and `runtime-status.txt` is latest-state-only
  with a terminal write (`worker-durable-events/REPORT.md:33-35`). New bridge
  diagnostics currently require a new epoch.
- **Reachable with a bounded addition:** (i) for durable repeatable diagnostics,
  call the EXISTING `diagnostic_event` from an in-session trigger (event-loop
  context or a small Lua-bound native shim). The sink is already open,
  append-only, import-safe — same three frozen kernel imports, zero new sinks
  (precedent: the geometry-diag merge reused the existing channel,
  `runtime_start_bridge_r14_diag_spanpub.cpp:47-52`). (ii) for a host→game
  COMMAND path, none exists; the bounded shape is the already-proven e2e
  intake pattern (`/data/modbridge/intake/e2e-cmd.json`: STOR once → consumed
  rename → evidence; `console_batch.py:2380-2420`) consumed by a small bounded
  poll on the event loop. Read-side cost is zero new imports if implemented as
  open-try(O_RDONLY)/read/close on the existing `sceKernelOpen` binding; a
  `stat`-based poll would add an import (recomposition gate).

### (b) A supported Lua mod reload

- **NOT supported today as a configuration:** both hot-reload settings refuse
  PROSPERO startup (`UE4SSProgram.cpp:737-738`), and the watcher path is not
  compiled (`:1535`, `:1693`). The Windows file-watcher is not reachable and is
  not needed.
- **REACHABLE WITH A BOUNDED ADDITION — the machinery already exists.** A
  minimal supported reload command needs exactly three things:
  1. A trigger. Two candidates: (i) zero-runtime-change — a surviving Lua mod
     fires the existing Lua API `RestartMod("<name>")` (`LuaMod.cpp:6532`) from
     a keybind or deferred action; (ii) a small command-file poll on the event
     loop (per (a)(ii)) that calls `UE4SSProgram::queue_reinstall_mod_by_name`
     (`UE4SSProgram.cpp:2471`) directly.
  2. Event-loop deferral — already built into every `queue_*` entry
     (`:2285-2288`, `:2330+`), riding the same delivery the in-game menu uses.
  3. Dispatch safety — the trigger must NOT run the reinstall inline during Lua
     action dispatch (`LuaMod.cpp:7041-7049` refuses it); queue/defer instead.
  Scope guard: reload ONE named mod. The all-mods `queue_reinstall_mods` has no
  Lua binding and its `uninstall_mods` path terminates the process on any
  uninstall exception (`UE4SSProgram.cpp:2192-2200`), whereas the per-mod route
  rebuilds exactly one mod (`:2330-2376`).
- **Honest bounds:** no in-game reload has ever been run on console — the route
  is code-present, evidence-absent. `runtime-status`/sdiag would only show a
  reload if the trigger also emits a `diagnostic_event` (one-line addition).
  Native hook arms lost at startup do not come back on reload
  (`LuaMod.cpp:7432-7447`, `:4776-4780`); a reload re-runs `setup/start` of Lua
  mods only. Precondition: the game-thread/event-loop pump must be alive for
  `queue_*` to execute — the proven F7 menu delivery is the same pump.

### Bottom line

One session CAN serve repeated Lua-driven diagnostics today, and a per-mod Lua
reload is reachable with a small, well-guarded addition (a deferred trigger onto
the existing `queue_reinstall_mod_by_name` / `RestartMod` route) — no file
watcher, no settings change, no new transport. What does not exist anywhere is a
host→game command channel; the e2e intake file pattern plus a bounded event-loop
poll is the minimal reusable shape for one.
