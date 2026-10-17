# RUNG-TABLE — OPTIONS press → Item Spawner menu: the exact dependency ladder

Worker: worker-menu-input-map · 2026-10-05 · local only, read-and-derive, no canonical edits.
Every hop is file:line-pinned in `CLAIM.json` (path aliases defined there). Companion:
`REPORT.md` (verdict on outcome-B + instrument gaps), `CLAIM.json` (machine-checkable pins).

Ladder (mission): **runtime starts → mod loads → game-thread work runs → controller input
ARRIVES → menu opens.**

Path aliases (full paths in CLAIM.json): `MAIN` = canonical main.lua · `LUAMOD`/`PROG` =
composed runtime LuaMod.cpp / UE4SSProgram.cpp · `HANDLER(±HPP)` = Input Handler ·
`PIS` = PlatformInputSource.hpp · `PADCFG`/`PADBRIDGE` = pad-config unit ·
`BACKEND` = ue4ss_backend.cpp · `PADSRC(±HPP)`/`PADNATIVE` = pad provider ·
`KEYDEF` = KeyDef. Evidence sinks: **S** = runtime-status.txt · **L** = UE4SS.log ·
**D1** = supplier-diag jsonl (`modbridge.supplier-diag/v1`).

Outcome definitions:
- **A = startup-only**: pad source ATTACHED at startup; runtime + mods start. Under the
  current fail-closed bytes this ALREADY requires one successful `scePadOpen` at backend-ctor
  time (`PADBRIDGE:19-24` throws otherwise) — A is not separable from B's first gate today.
- **B = usable-input**: presses DELIVER to the menu. Needs every A fact plus live poll, fresh
  edge, subscription, `allow_input`.

Key existing-instrument vocabulary: sdiag tags (`module_start_entered`, `runtime_start_ok`,
`runtime_start_failed`, `module_start_return`…), status states (`activated`/`engine-ready`/
`mods-started`/`failed`), L lines (`[Lua] [TownfallItemSpawner] …`, entry failure line,
`[WalkRxGuard] …`), probe sink records (`key_fired` etc. in `/data/pad-probe-events.jsonl`,
standalone probe only). "MIRROR" lines = the one-line probe verdict mirrors in L
(`FC1POOL:`/`CC1:`) — vocabulary available if the combined candidate adds a pad mirror line.

---

## The rung table

| # | Rung | What must be TRUE (hops) | (i) Durable evidence that proves it in a live run | (ii) Pad outcome | (iii) Failure modes (exact lines/shapes) |
|---|---|---|---|---|---|
| R1 | **Runtime starts** (composition constructed, owner started) | S1–S9: ini read (`PADBRIDGE:13-14`) → parse (`PADCFG:52-124`, OPTIONS bit `:42`, `F7`→`Key::F7` via `KEYDEF:117,180-196`) → factory eager-activates (`PADNATIVE:20` → `PADSRC:25-48`: user_init `:29-31` accepts `0x80960003`, pad_init `:35-36`, user_list `:37-42`, `scePadOpen` `:43-44`) → availability throw-gate `PADBRIDGE:19-24` → assign `:25` → `BACKEND:22` → `PROG:250,731-733` → `HANDLER:53-64` (`:59` available check) | D1: `runtime_start_ok value=0x00000000` then `module_start_return value=0x00000000`; S: `state 1 activated 0` (fresh `session\t<pid>\t<ms>`); L: UE4SS banner + `root directory: /data/plugins`. **Pad-specific proof inside R1:** `activated` ⇒ ini parsed AND `scePadOpen` succeeded once (activation precedes the status write and throws on failure — CP1 proof, MASTER-VERIFICATION-RUNBOOK.md:223-228) | **A** (and today, because of the fail-closed throw, A's evidence already contains one successful open) | L entry line `[t=<ms>ms pid=<pid>] modbridge module_start failed: runtime_start_failed code=3 detail=…` + S `state 1 failed 1 <detail>`. SEEN (real): `detail=PS5Pad input unavailable: phase=6, error=-2137915388` = `PadPhase::Open` (`PADSRC_HPP:10-11`) + `0x80920004` ALREADY_OPENED (`run3/run4-ue4ss.log:54`, `run3-runtime-status.txt:3`). Also SEEN: `code=3 detail=PS5Pad.ini must be ASCII text…` (parse refusal, run1); `mfa_activate_failed code=-8 … ue4ss/PS5Pad.ini` (size drift, run2). Ini-content refusals carry `PS5Pad.ini line N: <why>` (`PADCFG:24,105-106`) |
| R2 | **Mod loads** (Lua mod inventory + main.lua executed, 6 binds registered) | R1 + S10–S13: event-loop jthread `PROG:673`; on_program_start `PROG:1516`; `install_lua_mods`/`start_lua_mods` `PROG:1485,1488` (gated `ShouldRunNativeModCallbacks :1479-1482`); main.lua `MAIN:694-729` → `LUAMOD:1915,1964-1969` → `PROG:2882-2885` → `HANDLER:105-116` (`m_subscribed_keys[F7]=true :115`; throw at `:109` if injected-mode && !available — PROSPERO defaults injected-mode TRUE, `HANDLER_HPP:56-57`) | L: exactly one fresh `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` (`MAIN:746`); S: `state 2 engine-ready 0` then `state 3 mods-started 0` (written BEFORE main.lua runs; banner arrives later on the event thread). **Hidden rung proof:** the banner itself proves all six `RegisterKeyBind` calls succeeded ⇒ an ACTIVE available input source existed at load (`HANDLER:109` would have thrown into the script-error path otherwise) | **A** | Script layer (NON-fatal to status): `Error loading script: …` / `Error executing script: …` — the bind-throw shape is `[Lua] Input registration unavailable: no active input source` (throw text `HANDLER:109`, surfaced via `LUAMOD:1943-1947`-style error path during load); banner present + S `mods-started` but script dead = F9 class. Whole-inventory failure: S `failed` with `Native UE4SS inventory rejected: …` (`BACKEND:37-39` path) |
| R3 | **Game-thread work runs** (EngineTick pump armed; drained closures execute) | D10: pump armed at init (`RegisterEngineTickPreCallback` `LUAMOD:4525/:4529`; real detour `HookedEngineTick` in UnrealInitializer); drain `engine_tick_hook → process_simple_actions` `LUAMOD:4478,4113` gated by `NativeLuaActionDispatchScope`; `DefaultExecuteInGameThreadMethod=EngineTick` (settings v3 `:12`, applied `PROG:1476`) | **INSTRUMENT GAP (G-AP2) — no direct tag exists.** Today provable only: (a) inferentially — S `engine-ready`+`mods-started` acks (init ordering; NOT game-thread proof per playbook honesty note); (b) L `GameEngine::Tick address (vtable: 0x33ac8c0; scan: …)` line = resolve/registration path ran (CP2; never yet observed live, runbook G7); (c) circularly — any `[TownfallItemSpawner]` line emitted from inside an ExecuteInGameThread closure (first-OPTIONS scan line) — but that already needs R4 | **A** (pump arming is a startup fact; drain liveness is runtime) | On the launch-10-fold-0 candidate: pump arm REFUSES loudly — L `[WalkRxGuard] REFUSED: NO read attempted; …` (W1) + S failed detail `[WalkRxGuard] refused: guard_off_no_rx_verdict …` (W2) → engine-ready unreachable, R2 never happens (ORACLE S1.1-S1.2). Post-mods shapes: L `Native game-thread initializer callback registration failed` = resolve/patch fatal; L `ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available (AOB scan failed)` (`LUAMOD:4719`, fires at FIRST PRESS `MAIN:695`) = pump was NOT armed when pressed (post-`mods-started` this line is an anomaly → restage check, RUNBOOK §4.3a) |
| R4 | **Controller input ARRIVES** (pad edge → poll event → dispatch → F7 callback fired on UpdateThread) | R2 + D1–D7: loop alive (`PROG:1660` process_event per tick, sleep 5ms `:1682`); `PS5PadSource::poll` `PADSRC:69-94` (available `:71`, read_state `:73`, duplicate-timestamp guard `:81`, baseline `:82`, chord match `:84`, emit gate `!baseline && down && !m_down[i] && allow && subscribed[b.key]` `:85`); allow gate `HANDLER:76` (per-tick setter `PROG:1653`); key match `HANDLER:83-94` (exact modifier equality); invoke `HANDLER:99-102` → Lua lambda `LUAMOD:1937-1948` ON THE UpdateThread | **INSTRUMENT GAP (G-AP1) — NO tag fires when a pad event reaches the handler.** The pad+dispatch path is silent (`PADSRC`/`HANDLER` contain zero logging; only the non-pad `QueueInputSource.cpp:12,25` logs). Today's only durable proxies: (a) the standalone probe PRX's `key_fired` record in `/data/pad-probe-events.jsonl` (proves pad DELIVERY, outside the composed runtime — worker-pad-hardware-probe REPORT:85-87); (b) downstream R5 mod lines (which additionally require R3). Discriminating "poll dead" vs "event dropped" vs "callback fired but drain blocked" is impossible from sinks alone | **B** (this is the first strictly-B rung) | (1) Poll never runs: loop dead → no `fire_update` either; (2) `scePadReadState` errors → phase Read fail (`PADSRC:74`) — silent; (3) `!data.connected` → phase Disconnected, edges cleared (`:75-78`) — silent (G-AP3); (4) `allow_input` false (game console open; `PROG:1653` gate) → ALL presses swallowed — silent (G-AP3); (5) wrong user/pad (User=-1 takes the FIRST logged-in user, `PADSRC:41`); (6) callback fired but `ExecuteInGameThread` threw → L DOES log `ExecuteInGameThread: EngineTick …` (`LUAMOD:4719`) — this DISCRIMINATES R4-alive from R3-dead; (7) press not a clean edge — duplicate-sample guard drops held-button re-fire by design (`PADSRC:81,90`) |
| R5 | **Menu opens** (drained toggleMenu builds UI on the game thread) | R4 + D8–D11: enqueue `LUAMOD:4748/:4752` → drained `:4478/:4113` → `toggleMenu` `MAIN:670-672` → `showMenu(true)`: scan `:651` → buildUI `:652` → `AddToViewport(9999)` `:631` → `Menu created.` `:643` | L (first OPTIONS press of a session/map): `[Lua] [TownfallItemSpawner] <N> spawnable pickups found. Pages=<M>` (`MAIN:189`) + `[Lua] [TownfallItemSpawner] Menu created.` (`MAIN:643`); SCREEN: dark-red `TOWNFALL : ITEM SPAWNER` panel top-right with red `>>` marker. Silence on press #2+ is SUCCESS (re-open mute by design, `MAIN:489-491`; scan only when `#items==0` `:651`) — after a map load the first press always logs (`RegisterLoadMapPreHook` reset `MAIN:731-744`) | **B** | `Scan failed: Item database not ready. Load a save first.` (`MAIN:66,194`) = drain+query fine, save not loaded (expected guard, T3); `Scan failed: <other>` = DB getters threw; `WidgetBlueprintLibrary not found.` (`MAIN:501`); `UI creation failed: <err>` (`MAIN:636`); PlayerController miss is SILENT (`MAIN:494-497`) — no panel, no log; ZERO new lines on first press = R4 or R3 failed (use POST-POPUP §3 discrimination table + RUNG-TABLE R3/R4 failure modes) |

---

## First-press discrimination ladder (who is guilty when NOTHING happens)

Ordered check for "OPTIONS pressed, zero new mod lines, no panel", each step named by its
rung and its instrument:

1. **R1/R2 sanity** — S ends `mods-started`; L has the load banner. Missing → R1/R2 failure
   list (S/D1/L entry lines).
2. **R3 pump** — any `[WalkRxGuard]` line (W1/W2) in L or S = fold-0 candidate, menu can
   never open (EXPECTED for that candidate; record honestly, README-RETURN §4). A first-press
   `ExecuteInGameThread: …` line = pump not armed at press time (R3) while R4 works.
3. **R4 poll** — no instrument in the composed runtime today (G-AP1). Options: (a) run the
   standalone probe PRX (`key_fired` records) in a SEPARATE launch to prove delivery
   hardware-side; (b) have the combined candidate add the pad-event tag (REPORT.md §gaps);
   (c) negative checks: clean brief press (edge semantics), only one controller paired,
   game console closed (allow gate), nav binds menu-closed are silent no-ops by design
   (`MAIN:699-723`) — not evidence of a dead pad.
4. **R5 UI** — if a scan line appeared but no `Menu created.` and no panel: PC miss is silent
   (`MAIN:494-497`); `UI creation failed:`/`WidgetBlueprintLibrary not found.` name the call.

## Instrument gaps the combined candidate must fill (summary; details in REPORT.md)

| Gap | Missing tag | Where it would live (suggested, bounded) |
|---|---|---|
| G-AP1 | pad event reached handler / key dispatched | one-shot or rate-limited emitter at `PADSRC:88` (edge emitted) and/or `HANDLER:91` (key matched), as a supplier-diag tag or one `PAD:` MIRROR line in L |
| G-AP2 | EngineTick drain liveness | one-shot `gt_drain_first` from `process_simple_actions` first iteration (`LUAMOD:4113`) |
| G-AP3 | allow_input value + source phase transitions | per-transition tag in `PADSRC:74-78/:92`; one-shot `allow_input=false` transition at `PROG:1653` |
| G-AP4 | pad-defer retry accounting | attempt counter + last phase/error per retry (needed only if the deferred-open route lands) |
