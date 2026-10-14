# REPORT — worker-menu-input-map (2026-10-05)

Mission: map the EXACT dependency chain from controller button (OPTIONS) to Item Spawner
menu-open so the combined candidate's evidence can prove each rung of
`runtime starts → mod loads → game-thread work runs → controller input ARRIVES → menu opens`.
Local only, read-and-derive; nothing ran on console; no canonical file touched.

Deliverables: `CLAIM.json` (every hop file:line), `RUNG-TABLE.md` (the rung table +
discrimination ladder + gaps), this REPORT (mod-side path, composition-side conditions,
outcome-B verdict).

## 1. Mod side: main.lua's F7 detection path

Canonical main.lua (package-v3, 21,415 B): `RegisterKeyBind(Key.F7, function()
ExecuteInGameThread(toggleMenu) end)` at **main.lua:694-696**, plus five nav binds
`:698-729`. Chain into the runtime:

1. `Key.F7` is the Lua global `Key` table entry built at LuaMod.cpp **:448-614**
   (`add_pair("F7", static_cast<uint32_t>(Input::Key::F7))` at **:553**; `make_global("Key")`
   **:614**) — the Lua integer is `Key::F7 = 0x76` (KeyDef.hpp **:138**).
2. `RegisterKeyBind` = LuaMod.cpp **:1915**; key range-checked to 0-255 (**:1930-1933**);
   callback stored as a registry ref (**:1960**); registered via
   `mod->m_program.register_keydown_event(key, cb, /*custom_data=*/1, mod)` (**:1964-1969**)
   → `UE4SSProgram::register_keydown_event` (UE4SSProgram.cpp **:2882-2885**) →
   `Handler::register_keydown_event` (Handler.cpp **:105-116**): appends `KeyData` (**:111**)
   and sets `m_subscribed_keys[key]=true` (**:115**). On PROSPERO the registration throws
   `"Input registration unavailable: no active input source"` (**Handler.cpp:109**) when
   injected-mode is on with no available source — and injected-mode defaults TRUE on
   PROSPERO (Handler.hpp **:56-57**). Consequence used by the rung table: **the load banner
   (main.lua:746) proves an active pad source existed at load time.**
3. **Which UE4SS input layer receives the keypress:** the `RC::Input::Handler` event pump.
   `Handler::process_event()` is called once per tick by the UE4SS event loop
   (UE4SSProgram.cpp **:1660**, loop at **:1499**, thread name `UE4SS-UpdateThread` **:1507**,
   5 ms sleep **:1682** ⇒ ~200 Hz worst case). It pulls events from the active
   `PlatformInputSource` (Handler.cpp **:75**; interface PlatformInputSource.hpp **:15-51**),
   applies the `m_allow_input` gate (**:76**, default true Handler.hpp **:50**, per-tick
   setter from game console state at UE4SSProgram.cpp **:1653**), matches
   key+modifier exactly (**:83-94**), and invokes callbacks **outside** the mutex
   (**:99-102**).
4. **Which thread fires the Lua callback:** the **UpdateThread** — NOT the game thread.
   `lua_keybind_callback_lambda` (LuaMod.cpp **:1937-1948**) takes
   `m_thread_actions_mutex` (**:1938**) and calls the Lua function (**:1941-1942**); errors
   are caught and logged to UE4SS.log (**:1943-1947**). The mod's callback body is only
   `ExecuteInGameThread(toggleMenu)`, so real menu work is re-deferred: LuaMod.cpp
   **:4690-4767** throws at the FIRST press if the EngineTick hook never became available
   (**:4719**) and otherwise enqueues a `SimpleLuaAction` into `m_engine_tick_actions`
   (**:4752**, pending queue **:4748**), drained on the real game thread by
   `engine_tick_hook → process_simple_actions` (**:4478**, implementation **:4113**).

## 2. Composition side: from the input layer to the PLATFORM INPUT SOURCE

`Handler::m_platform_handler` is a `RC::Input::PlatformInputSource`
(PlatformInputSource.hpp **:15-51**: `is_available/activate/deactivate/source_priority/
process_event`). The composed units' Input/Handler.cpp and the PS5 pad source are:

- `…/native-coherent-runtime-1/source/port/first/src/Input/Handler.cpp` (+ `include/Input/`)
- `…/ue4ss-native-pad-config-1/src/PS5PadConfig.cpp` + `NativePadInput.cpp` (byte-identical
  to the built/ack copies), backend `ue4ss_backend.cpp`
- `…/ue4ss-native-pad-provider-1/port/PS5PadSource.{hpp,cpp}` + `PS5PadNative.cpp`

What must be TRUE for a pad press to reach the Lua callback:

- **(a) A source instance attached.** Backend ctor calls
  `ConfigureRetainedPS5PadInput(*context.mod_source, context.input_source)`
  (ue4ss_backend.cpp **:22**) BEFORE `program_->init()` (**:47**). The bridge reads exactly
  `ue4ss/PS5Pad.ini` from the activated mod source (NativePadInput.cpp **:13**; absent file =
  NO pad bridge, **:14**; ini + supplied source = conflict-throw, **:15**), parses fully
  (PS5PadConfig.cpp **:52-124**; `OPTIONS`→button bit **:42**; right side via
  `string_to_key` **:104**; `f7`→`Key::F7` KeyDef.cpp **:117,180-196**), builds the source
  via `make_native_ps5_pad_source` (**:17**) and assigns it only after an availability check
  (**:19-25**). `UE4SSProgram` ctor then `Handler::init()`s it (UE4SSProgram.cpp
  **:250,:731-733**; Handler.cpp **:53-64**, availability check **:59**).
- **(b) The source `is_available()`.** `PS5PadSource::is_available() == (handle >= 0)`
  (PS5PadSource.cpp **:24**); the handle exists only after `activate()`: user_initialize
  (already-initialized `0x80960003` accepted **:29-31**) → `scePadInit` (**:35-36**) →
  `sceUserServiceGetLoginUserIdList` + first logged-in user for `User=-1` (**:37-42**) →
  `scePadOpen` (**:43-44**). THE ALREADY_OPENED PROBLEM: the eager open happens during the
  backend ctor (~+790-800 ms into `module_start`), and the REAL captured refusals are
  `runtime_start_failed code=3 detail=PS5Pad input unavailable: phase=6, error=-2137915388`
  — phase 6 = `PadPhase::Open` (PS5PadSource.hpp **:10-11**), error = `0x80920004`
  PS5_PAD_ERROR_ALREADY_OPENED (run3/run4 UE4SS.log **:54**, run3 runtime-status **:3**;
  TIMING.md §0/§4). Because the failure THROWS, the current bytes are all-or-nothing:
  no pad open ⇒ no runtime ⇒ not even outcome-A.
- **(c) A polling loop running.** The poll lives in the **UE4SS event loop's UpdateThread**
  — the same unit (`UE4SSProgram::update`, started as a jthread at UE4SSProgram.cpp **:673**)
  — at ~200 Hz worst case (5 ms sleep **:1682**). There is NO separate pad thread.
  `Handler::process_event()` (per tick, **:1660**) → `PS5PadSource::process_event` →
  `poll(subscribed, allow)` (PS5PadSource.cpp **:65-94**): `scePadReadState` (**:73**),
  duplicate-timestamp guard (**:81**; ReadState is a cached snapshot), baseline swallow
  (**:82**), chord match `(buttons & chord) == chord` (**:84** — OPTIONS alone; extra
  unrelated buttons do not suppress), emit on rising edge only
  `!baseline && down && !m_down[i] && allow && subscribed[b.key]` (**:85**; one keydown per
  press, no autorepeat, no keyup synthesized; edge state tracked while gated, **:90**).
  Cadence owner: the runtime loop; the pad-config unit drives nothing after construction.
- **(d) The key mapping applied.** Resolved ONCE at parse time into `PadBinding.key`
  (`OPTIONS = F7` and the five nav chords; shared profile worker-real-input-pad/PS5Pad.ini
  **:35-40**), so the poll compares live button bits against pre-resolved `Key` enum values;
  dispatch matches those against what main.lua registered (same `Key` enum), with exact
  modifier equality (Handler.cpp **:89**).

## 3. Rung table (summary — full table in RUNG-TABLE.md)

| Rung | Evidence (existing vocabulary) | Outcome | Status |
|---|---|---|---|
| runtime starts | D1 `runtime_start_ok`/`module_start_return` + S `activated`; pad proof inside R1: `activated` ⇒ ini parsed AND `scePadOpen` succeeded once (fail-closed throw precedes it) | A | Instrument EXISTS (sdiag + status). Real failure artifacts captured (ALREADY_OPENED ×2 boots, ini-parse, mfa-size). |
| mod loads | L `[Lua] [TownfallItemSpawner] Loaded v1.0…` + S `engine-ready`/`mods-started`; banner implicitly proves the 6 binds registered against an active source | A | Instrument EXISTS. |
| game-thread work runs | **GAP (G-AP2)**: only inferential (acks) or circular (first-press mod line). C2 driver unwired; walkguard W1/W2 proves the fold-0 pump NOT armed loudly | A | **Instrument gap.** |
| controller input ARRIVES | **GAP (G-AP1)**: no tag when a pad event reaches the handler/dispatch; the only per-press record anywhere is the standalone probe PRX's `key_fired` (`/data/pad-probe-events.jsonl`), which runs OUTSIDE the composed runtime. Downstream proxies: first-press scan line (needs R3), `ExecuteInGameThread…` throw line (discriminates R4-alive/R3-dead) | B | **Instrument gap — the mission's suspected gap is CONFIRMED.** |
| menu opens | L first-press `<N> spawnable pickups found. Pages=<M>` + `Menu created.` + visible panel; press #2+ mute by design | B | Instruments EXIST (POST-POPUP §2); requires R3+R4. |

Failure modes per rung, with exact lines, are in RUNG-TABLE.md column (iii); the
first-press discrimination ladder folds POST-POPUP §3 and the RUNBOOK §4.3d no-reaction
steps into rung order.

## 4. Honest verdict on outcome-B

**What the menu minimally needs:** ANY route that delivers pad events to the source's poll —
exactly three live facts, nothing more:

1. `is_available() == true` (an open pad handle in this process) **at poll time** — not
   necessarily at startup;
2. `poll()` actually called per tick (UpdateThread alive, `HAS_INPUT` compiled — it is, per
   the build recipes), `allow_input` true (game console closed), F7 subscribed (automatic
   once main.lua loaded);
3. a fresh rising edge on the OPTIONS bit (the duplicate-sample guard makes poll rate
   irrelevant to event count).

**The blocker and the realistic route.** The single failing fact today is (1): `scePadOpen`
returns `0x80920004 ALREADY_OPENED` at the earliest practical injection moment (~+790 ms),
i.e. the game's pad stack already owns the pad BEFORE injection (TIMING.md §4 inference,
flagged). No successful open exists anywhere in the captured evidence, and nothing shows the
game ever releases the handle — so a fixed boot-relative delay is unjustified. The minimal
outcome-B route is the pad-defer shape TIMING.md already recommends: keep the source
attached but unavailable, and retry `activate()` event-driven (per poll tick or on
user/disconnect transitions). Each attempt is 4 bounded SDK calls; a retry issued from
inside `poll()` on the UpdateThread is same-thread serialized and satisfies the provider
lifecycle contract (PlatformInputSource.hpp **:11-14**; Handler.hpp SAFETY comment
**:60-64** — "reconfiguration must never overlap process_event"). Re-injecting via
`Handler::init` from a different thread would violate that contract and must NOT be done.

**What the pad worker's SDK analysis must decide** (handle ownership, enumeration, peek vs
consuming reads, non-exclusive): whether ANY open class can yield a readable handle while
the game holds its own — per-user/per-port-type ownership semantics of `scePadOpen`,
whether `scePadReadState` (a cached snapshot, not a queue consumer) works from a
second/non-exclusive handle whose buttons still mirror the shared DualSense, and what the
user-list enumeration actually returns in-game (wrong user selection would also fail or
read the wrong pad). Until one of those opens succeeds, outcome-B is blocked at the FIRST
gate and no later rung can advance; the standalone probe PRX
(worker-pad-hardware-probe, `modbridge_pad_probe.prx`) remains the cleanest instrument to
answer the open question in isolation, without betting the whole composed-runtime start on
it.

**Risks if outcome-B is achieved:**

- **Input latency:** negligible for a menu. Poll at ~200 Hz worst case; ReadState cached +
  duplicate-timestamp guard = one event per physical press; menu-open additionally waits one
  EngineTick frame for the drained closure. Perceived latency is dominated by frame time.
- **Thread context of the poll:** the poll AND the Lua keybind callback run on the
  **UE4SS-UpdateThread**, not the game thread. Lua state access there is serialized by
  `m_thread_actions_mutex` (LuaMod.cpp **:1938**) — the same mutex the game-thread drain
  holds while running drained closures, so a long callback body would contend with the
  drain. The Item Spawner's callback is enqueue-only (main.lua **:695**), so contention is
  bounded; any future mod that does heavy work directly in the keybind body (not wrapped in
  `ExecuteInGameThread`) would stall the poll loop and with it every other keybind.
- **Silent gates to keep on the failure list:** `allow_input` (console open ⇒ everything
  swallowed, G-AP3), mid-session pad disconnect (edges cleared, silent), nav binds
  menu-closed (no-ops by design), press #2+ mute (menu already built).
- **Fail-closed startup coupling:** while the eager open + throw remains, a pad failure
  blocks the ENTIRE runtime (outcome-A impossible) — the ALREADY_OPENED refusals in
  launch10-evidence are exactly this shape. The pad-defer redesign is what makes outcome-A
  (startup-only) separable from outcome-B; it needs the G-AP4 retry/phase instrumentation to
  stay honest about WHY the pad is still unavailable.

## 5. Files and provenance

- `CLAIM.json` — every hop with absolute-alias file:line pins, outcome definitions, real
  evidence artifacts, instrument gaps G-AP1..4, and two documented line-number drifts vs
  older derived docs (PAD-DISPATCH-PATH.md cites a few lines off from this on-disk
  revision; direction/content agree; this claim re-read the sources directly).
- `RUNG-TABLE.md` — the five-rung table (evidence / outcome / failure modes), first-press
  discrimination ladder, gap summary.
- Key retained sources read: composed runtime (`native-coherent-runtime-1/source/port`),
  pad-config unit (`ue4ss-native-pad-config-1/src`), pad provider
  (`ue4ss-native-pad-provider-1/port`), canonical main.lua (package-v3), plus derived docs
  POST-POPUP.md, PAD-DISPATCH-PATH.md, PAD-API-AND-INTEGRATION.md, MASTER-VERIFICATION-RUNBOOK.md,
  ORACLE.md (+SUPPLEMENT), TIMING.md, SPAWN-CAPABILITY.md, pad-hardware-probe REPORT.md.
- Real evidence artifacts cited: `outputs/2026-10-04-zcode-controller/launch10-evidence/`
  run3/run4 UE4SS.log:54, run3-runtime-status.txt:3, run3-sdiag.jsonl (accumulated).
- Honest bounds: source-derived + already-captured refusals only; no console contact; rung
  advancement requires the real launch (ORACLE.md:293-294).
