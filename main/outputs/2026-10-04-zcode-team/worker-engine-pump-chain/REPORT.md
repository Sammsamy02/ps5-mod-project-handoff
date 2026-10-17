# REPORT — worker-engine-pump-chain

Worker: worker-engine-pump-chain · 2026-10-05 · local only. No console contact, no engine or
provider execution, no canonical edits, no composition runs. Sources read: the launch-10
final-units (initializer v4 `4a2b9400…`, bridge r14 `d96cfe70…`), the composed runtime port
(UE4SSProgram.cpp, LuaMod.cpp, Hooks/DetourInstance, walkguard ASMHelper, vtable macro), the
canonical TownfallItemSpawner main.lua (package-v3), the wpd1/wpd2 pad lineages, the launch-11
R3-marshal staging (DIFF-1 + DIFF-2 + consumer), and the delivered spawn-chain
(worker-re-spawn-chain) and menu-input (worker-menu-input-map) reports plus their supporting
artifacts (SPAWN-CAPABILITY.md, ORACLE-SUPPLEMENT.md W1-W7, FAILURE-TAXONOMY.md,
POST-POPUP.md, pad-coexist REPORT/SDK-ANALYSIS, l11-marshal RUNBOOK).

Mission: the EXACT chain from "seeds all bound" to "the F7 menu can open" — each milestone's
(a) code path file:line, (b) evidence line it would produce, (c) known blockers, (d)
host-testable vs live. Plus order, parallelism, and shared gates.

Deliverables: `CLAIM.json` (every hop pinned), `CHAIN.md` (the annotated chain diagram +
gate table), this REPORT.

---

## 1. The one-paragraph answer

"Seeds all bound" is the last EASY milestone. After the four seed slots bind
(UnrealInitializer.cpp:787-1058, five `[Seeds] … bound/validated` lines), the initializer's
remaining work is a straight line — skip the FName detour verifier (seed-bound), data-side CDO
finds (these reads work) — that terminates at the FIRST exec-text read: `Finding
GameEngine::LoadMap` (final-units:1877) → vtable macro → `get_first_instruction_at_address`.
Under the launch-10 fold-0 walkguard that read refuses loudly (`[WalkRxGuard] REFUSED …` W1 +
thrown `guard_off_no_rx_verdict` W2) with zero syscalls, the throw is caught fail-closed, the
program shuts native UE4SS down, and the game survives. Consequence: the event loop never
starts, so mods never load, so the pump never arms, so OPTIONS/F7 can do nothing —
**GATE-1 (exec-read) alone blocks every remaining milestone simultaneously.** The launch-11
R3-marshal path (SetFuncPtr on a carrier UFunction, zero exec writes) is the designed bypass
for the WRITE gate (GATE-2), but NOT for the read gate — DIFF-2 compiles out the initializer's
detour registrations, not its resolve walks — and it currently sits behind three unlanded
revisions: the walkguard fold (IGW verdict), the DIFF-1 ladder re-order (GATE-4), and a guard
for LuaMod's own eager LoadMap registration (GATE-5). Pad input (GATE-3) is an independent
side condition with its own staged fix (wpd2 retry) and its own unproven window (the game's
exclusive handle must free).

## 2. Milestone summaries (full pins in CLAIM.json, diagram in CHAIN.md)

### M1 — Engine initialization after all seeds bind
- **Path**: PROG:619→1265 → UI_FC:1572 Initialize → :1581 ScanGame → :1068-1109 arming →
  :1140-1058 `consume_required_slot_seeds` ("seeds all bound") → :1592 try → :1601 verifier
  skip → :1766-1873 object waits + CDO finds → **:1876-1884 LoadMap walk** → (fold-0) ASM:126
  refusal → :231-239 W1+W2 → UI_FC:2160 catch/rethrow → PROG:682 catch → fail-closed. In a
  walk-permitted world the path continues: Tick vtable fallback :1886-1944, EngineTick
  callback registrations :1955-1970, remaining resolves, type system, PostInitialize :2170,
  PROG:655-661 engine-ready.
- **Evidence**: the five `[Seeds]` lines (:812/:856/:967/:1008/:1057) + the arming line
  (:1099); then W1 (UE4SS.log Error) and W2 (`state <seq> failed 1 [WalkRxGuard] refused:
  guard_off_no_rx_verdict …` in runtime-status) with `Finding GameEngine::LoadMap` as the
  last progress line; engine-ready world would show `GameEngine::Tick address (vtable: …)`.
- **Blockers**: GATE-1 (fold-0 refusal — fatal to everything downstream); terminal seed
  refusals by design (all-or-nothing, :1085-1092); spanpub=0 pool-admission refusal
  (:880-921). Documented tension: SPAWN-CAPABILITY.md's "the mod loads and logs" vs
  ORACLE-SUPPLEMENT S1.2's "engine-ready/mods-started unreachable past W2" — source reading
  sides with the oracle (the walk site runs before the event loop exists, PROG:619→673);
  settle by the real launch only.
- **Host/live**: seed machinery + line order host-proven (validator v4 69/0, gate 15/15);
  traversal LIVE-only.

### M2 — Mod loading
- **Path**: PROG:673 event-loop jthread (hard serial gate on M1) → update :1499-1514 →
  on_program_start :1365/:1476-1490 → start_mods<LuaMod> :1942 (mods.txt discovery,
  configured root) → :2004/:2110 start lines → LUAMOD:6450 start_mod → :6483
  load_and_execute_script(main.lua) → MAIN:694-746 (six binds + load banner).
- **Evidence**: `Starting mods (from mods.txt (<path>) load order)...`; `Mod
  'TownfallItemSpawner' has enabled.txt, starting mod.`; **`[Lua] [TownfallItemSpawner]
  Loaded v1.0. Clean pickup-only release. F7 opens the menu.`** (MAIN:746); status
  mods-started before the banner. Failure shapes: `Failed to execute main script: …` /
  `Exception during mod startup: …` (LUAMOD:6485/:6494).
- **Blockers**: unreachable while GATE-1 stands (oracle F34/F35); in the R3 world GATE-5 —
  LuaMod::on_program_start's own eager RegisterLoadMapPreCallback arms a PLH detour and
  throws under no-exec-write, so `start_lua_mods()` never runs (l11 RUNBOOK §5.2, unowned);
  pre-wpd1 pad coupling (binds threw without an active source) is already lifted by the
  wpd1 defer fold.
- **Host/live**: tree/loadlist host-rehearsed; execution LIVE-only.

### M3 — Game-thread pump arm (R3 marshal)
- **Why the PLH detour cannot arm**: the Tick address resolve needs the exec read (GATE-1);
  detour creation needs the exec write + PolyHook disassembly (GATE-2, igf5 class, E1 open);
  the fallback predicate `IsEngineTickAvailable()` is an AOB-found global that can never be
  true with scans dead (KB anti-pattern 13). Concrete EngineTick-world refusal tags:
  `[UE4SS.EngineTick.LuaModImpl] Tried to install hook but function is unavailable.`
  (DetourInstance.hpp:164) → `Failed to add hook, detour installation likely failed!`
  (:284) → LuaMod throws `EngineTick callback registration failed` (LuaMod.cpp:4536) at the
  FIRST default-method ExecuteInGameThread.
- **The armed pump (launch-11 R3)**: DIFF-2 initializer fold (`UE4SS_R3_MARSHAL_MODE=1`,
  initializer-l10-r3:179) compiles out the initializer's EngineTick+FName registrations and
  defers the pump; DIFF-1+consumer LuaMod (04c9d916) enqueues ExecuteInGameThread closures
  onto a SetFuncPtr marshal on a carrier UFunction — heap-tier data writes only, zero exec
  writes. Arming is engine-driven (no keypress needed).
- **Evidence (the oracle lines, l11 RUNBOOK §3)**: `[R3] refused: FName constructor
  verification detour (…)` → `[R3] refused: EngineTick pre-callback registrations …` →
  `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` →
  `Finding GameEngine::LoadMap` (walks still run) → `Starting Lua mod 'TownfallItemSpawner'`
  → **`[R3] FuncPtr-marshal armed on carrier Function /Script/Engine.Actor:ReceiveTick
  (callback id 0)`** (arm proof) → **`[R3] marshal is the game-thread pump (initializer
  deferred its EngineTick registrations)`** (pump-live, first carrier invocation) → the
  closure's own print (drain proof). Failure variants: `marshal carrier candidate not
  found` / `marshal arm refused: …` / `marshal arm failed: no ranked candidate passed …`.
- **Blockers**: GATE-1 again (RUNBOOK §4: if the walks die the consumer never runs); the
  DIFF-1 ladder trap (§5.1 — availability-first ordering makes the marshal sit armed and
  unused while the first press throws); GATE-5 (§5.2); F-1 carrier rate UNMEASURED (armed
  line without the pump-live line = carrier never invoked).
- **Host/live**: consumer object hashes + determinism host-proven (854a476b); all oracle
  lines LIVE-only.

### M4 — Controller input delivery (wpd2 coexist)
- **Path**: PS5Pad.ini parse → OPTIONS→Key::F7 resolved once → (eager bytes) scePadOpen →
  captured `0x80920004 ALREADY_OPENED` → fail-closed throw (`runtime_start_failed code=3
  detail=PS5Pad input unavailable: phase=6, error=-2137915388`, run3/run4); (wpd1 defer)
  attached-but-unavailable, startup proceeds; (wpd2 staged) `poll()`'s unavailable branch
  runs `maybe_retry()`: first attempt at poll 200 (~1 s at the 5 ms UpdateThread cadence),
  doubling to a 3200-poll cap (~16 s), ≤4 SDK calls per attempt, same-thread serialized.
  Delivery: Handler::process_event per tick (PROG:1660) → poll (edge :85) → exact key+mod
  match (Handler:83-94) → Lua callback on the UpdateThread (LuaMod:1937-1948).
- **Evidence**: wpd2 sdiag vocabulary `pad_open_class` (=0, once) / `pad_retry` (attempt #)
  / `pad_retry_failed` (raw error) / `pad_available` (handle) (PS5PadSource-wpd2:98-109) —
  fills G-AP4. **Input ARRIVAL itself has NO tag in the composed runtime (G-AP1, confirmed
  by the menu-input worker)**; the proxies are: the first-press `[TownfallItemSpawner]`
  line (needs the pump), the `ExecuteInGameThread: EngineTick method requested but
  EngineTick hook is not available (AOB scan failed)` throw line (discriminates
  R4-alive/R3-dead), or the standalone probe PRX's `key_fired` events (outside the composed
  runtime).
- **Blockers**: exclusive pad ownership — the retry succeeds only if the game's handle
  frees (real but unproven window); SDK analysis found NO header-decidable second-handle
  route (pad-coexist SDK-ANALYSIS §2); the open-class probe (V0-V5) is specced, not run;
  wpd2 is NOT in the launch-10 EDITION pins (launch-11 asset). Silent gates: allow_input
  (console open swallows all), duplicate-timestamp guard, menu-closed nav no-ops.
- **Host/live**: retry/backoff/thread/lease host-proven (54 checks, 4 compile gates); a
  real open + real edge LIVE-only.

### M5 — Menu opening
- **Path**: OPTIONS edge → MAIN:694-696 callback (UpdateThread) → `ExecuteInGameThread(
  toggleMenu)` MAIN:695 → (EngineTick world) LUAMOD:4719 availability throw or :4752
  enqueue into `m_engine_tick_actions` → (R3 world, post ladder-fix) enqueue onto the
  marshal → drained on the game thread by `engine_tick_hook → process_simple_actions`
  (LUAMOD:4488→4113) → toggleMenu → showMenu(true) → scanItems (first open) → buildUI (UMG
  widget tree — itself the live ProcessEvent proof) → AddToViewport(9999) MAIN:631 →
  **`Menu created.`** MAIN:643.
- **Evidence**: first-press `[Lua] [TownfallItemSpawner] <N> spawnable pickups found.
  Pages=<M>` (MAIN:189); then `Menu created.` — which executes INSIDE the drained closure,
  so its absence means the pump never drained, not that UI code broke. Expected for the
  launch-10 candidate: silence on first OPTIONS (recorded honestly, not a drain stall).
- **Blockers**: needs M3-draining AND M4-delivered AND M2-loaded at the same press
  (AND-gate); GATE-4 throws at the first press in both current worlds (EngineTick
  availability trap; R3 ladder trap until §5.1 lands); menu latency is carrier-cadence-bound
  in the R3 world.
- **Host/live**: logic host-readable; the PS5 chain LIVE-only; instruments exist
  (POST-POPUP §2; outcome B requires R3+R4 per menu-input rung table).

### M6 — Item spawning
- **Path (one drained closure, MAIN:215-348)**: stage `player` :224 (FindFirstOf
  PlayerController / pawn / location / yaw — GUObjectArray iteration, hash-table fast path
  off by design) → stage `ground trace` :240 (KismetSystemLibrary:LineTraceSingle :245,
  FHitResult OUT readback — GAP-L2a) → stage `load pickup class` :279 (StaticFindObject →
  LoadAsset fallback — 100% ProcessEvent — → re-query; LoadAsset's return is never consumed)
  → stage `spawn actor` :293 (GameplayStatics:BeginDeferredActorSpawnFromClass :295 +
  FinishSpawningActor :305 — engine-internal SCO path, GAP-SPAWN) → stage `configure pickup`
  :311 (PickupInteractable.ItemId read, PickupAmount, bAutoDestroyOnPickUp, SetEnabled,
  unhide, collision) → success MAIN:333-334.
- **Evidence**: **`[Lua] [TownfallItemSpawner] Spawned <name> x<N> ID=<itemid>`** (:333-334);
  every failure is pcall-caught: `Failed at <stage>. See UE4SS.log. <err>` (:341-342) with
  per-stage payloads (FAILURE-TAXONOMY §C1-C5); `Failed at <stage>` present ⇒ rung 4
  executed (mod alive, pump drained, menu delivered).
- **Blockers**: strictly after M5; GAP-L2a (FHitResult out-param readback — highest historic
  risk), GAP-L2b (PE vtable slot 0x260 Win64-PDB-derived — but rung 3's UMG calls check it
  first), GAP-SPAWN (engine→engine SCO never executed under the port), GAP-A7 (LoadAsset
  registry fallback, likely unused for the six cooked classes). None structural per the
  spawn-chain worker.
- **Host/live**: LIVE-only, entirely — zero live ProcessEvent/trace/spawn evidence exists
  for any candidate.

## 3. Order, parallelism, shared gates

**Order**: strictly serial M1 → M2 → (M3 ∧ M4) → M5 → M6. M3 and M4 are side conditions
that begin after M1 and proceed in parallel with M2 (pad retry is UpdateThread-paced;
R3 arming is engine-driven and keypress-independent), but M5 is the AND-gate that needs
both simultaneously. M6 is strictly after M5. There is no configuration in which a later
milestone produces its evidence while an earlier one's gate stands.

**Shared gates** (the load-bearing answer to "gates that block multiple milestones"):
- **GATE-1 exec-read (walkguard fold)** — blocks M1 fatally, cascading to M2-M6. THE
  simultaneous blocker. Lifting it requires the IGW RX verdict; the R3 marshal does NOT
  lift it (DIFF-2 compiles out registrations, not resolves).
- **GATE-2 exec-write (PLH detours)** — blocks all normal hook arms; the R3 marshal is the
  designed data-side bypass.
- **GATE-3 pad ownership** — blocks M4; pre-wpd1 bytes also blocked M1/M2.
- **GATE-4 availability predicate** — blocks M5/M6 at the first default-method
  ExecuteInGameThread even in an armed-pump world; needs the DIFF-1 ladder re-order.
- **GATE-5 LuaMod eager LoadMap registration** — blocks M2 in the R3 world; unowned.

**First success shape**: the first candidate that can open the menu needs ALL of:
walk-reads permitted + DIFF-2 fold + DIFF-1 ladder re-order + LuaMod LoadMap guard + wpd2
pad retry with an available handle + a carrier that actually ticks. That is the launch-11
R3-marshal candidate plus three distinct-ID revisions and one probe verdict — all currently
proposal/staged, none folded into any launch-10 pin.

## 4. Honest bounds

- Everything above is source + delivered-report derived; every in-game line is the shape of
  evidence to EXPECT, not a captured result. Nothing here closes a rung or claims
  engine-ready/mods-started/menu/spawn for any build.
- The SPAWN-CAPABILITY vs ORACLE-SUPPLEMENT tension on "mods load at launch-10" is flagged,
  not resolved (section M1); both documents are cited verbatim in CLAIM.json and the real
  launch is the only decider.
- wpd2 pad coexist and the R3 marshal are launch-11 assets (staged, FOLD.md admission
  instructions, nothing in the launch-10 EDITION pins).
- F-1 (carrier invocation rate) is OPEN; `armed` without the pump-live line proves nothing
  about drain latency; query-harness semantics change from ticks to carrier invocations
  (RUNBOOK §3.4) makes older tick-phrased evidence non-comparable.
- This worker wrote only under `outputs/2026-10-04-zcode-team/worker-engine-pump-chain/`.
