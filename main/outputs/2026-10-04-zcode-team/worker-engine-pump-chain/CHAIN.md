# CHAIN — from "seeds all bound" to "the F7 menu can open"

Worker: worker-engine-pump-chain, 2026-10-05. Local-only read-and-derive. Companion to
`CLAIM.json` (every hop with file:line) and `REPORT.md` (narrative + verdicts). Citation
aliases: `UI_FC` = launch10-build/final-units/UnrealInitializer.cpp; `PROG` = the composed
UE4SSProgram.cpp; `LUAMOD` = the composed LuaMod.cpp; `ASM` = the walkguard ASMHelper;
`MAIN` = canonical TownfallItemSpawner main.lua; full alias table in CLAIM.json.

```
 module_start (bridge r14, fail-open)
   └─ PROG:619 setup_unreal
       └─ PROG:1265 UnrealInitializer::Initialize
           ├─ UI_FC:1576 PreInitialize
           ├─ UI_FC:1581 ScanGame
           │    ├─ UI_FC:1068-1109  arming decision (SHAPE A, all-4-slots-or-nothing)
           │    └─ UI_FC:1140-1058  consume_required_slot_seeds  ◄══ "SEEDS ALL BOUND"
           │         slot 0 GUA  → "[Seeds] slot 0 g_uobject_array bound …"        :812
           │         slot 3 GMalloc V1/V2 → "[Seeds] slot 3 gmalloc bound …"       :856
           │         FName pool v4 admission → "[Seeds] FName pool validated …"    :967
           │         slot 2 ctor+probe → "[Seeds] slot 2 FName ctor bound …"       :1008
           │         slot 4 SCO → "[Seeds] slot 4 static_construct_object …"       :1057
           │         (any refusal: UI_FC:687 seed_refused — terminal, nothing bound)
           │
           ├─ M1 REMAINING WORK (inside UI_FC:1592 try, gate disarm-on-throw :2160)
           │    ├─ VerifyFNameConstructor SKIPPED (seed-bound)                     :1601
           │    ├─ required-object wait + data-side CDO finds (READS OK)     :1766-1873
           │    ├─ ▼ FIRST EXEC-TEXT WALK: "Finding GameEngine::LoadMap"           :1877
           │    │     VCMACRO:127-140 → ASM get_first_instruction_at_address
           │    │     fold-0 → [WalkRxGuard] REFUSED (W1, ASM:233) + throw (W2, ASM:236)
           │    │     ═══ GATE-1: kills the whole serialized startup ═══
           │    │     (walk-permitted world continues:)
           │    ├─ Tick vtable fallback resolve (slot 10 unseeded)            :1886-1944
           │    ├─ RegisterEngineTickPreCallback(HookedEngineTick)                 :1955
           │    ├─ remaining virtual resolves + type system                  :1975-2051
           │    ├─ ProcessInternal derivation (skipped when settings v3 keys set) :2053
           │    └─ UI_FC:2170 PostInitialize (GMalloc null throw :394)
           │         ⇒ PROG:655-661 m_is_program_started + on_engine_initialized
           │         ⇒ runtime-status: engine-ready
           │
           ├─ M2 MOD LOADING (ONLY if setup_unreal returned without throwing)
           │    ├─ PROG:673 event loop jthread ── UpdateThread (5 ms cadence)
           │    ├─ PROG:1514 on_program_start → :1488 start_lua_mods → :1942 start_mods
           │    │     "Starting mods (from mods.txt …)"                            :2004
           │    │     "Mod 'TownfallItemSpawner' has enabled.txt, starting mod."   :2110
           │    ├─ LUAMOD:6450 start_mod → load_and_execute_script(main.lua)       :6483
           │    │     main.lua:694-729 six RegisterKeyBind (needs live source, wpd2
           │    │     Handler lets them register while deferred)
           │    │     ⇒ "[Lua] [TownfallItemSpawner] Loaded v1.0 …"   main.lua:746
           │    │     ⇒ runtime-status: mods-started (before the banner)
           │    │     ═══ GATE-5 (R3 world): LuaMod's own eager LoadMapPreCallback
           │    │         arms a detour → throws → start_lua_mods never runs ═══
           │
           ├─ M3 PUMP ARM ── two worlds
           │    ● EngineTick world (launch-10 shape): detour needs exec READ
           │      (GATE-1) + exec WRITE (GATE-2). Lazy arm at first
           │      ExecuteInGameThread: LUAMOD:4527→DETOUR AddCallback→InstallHook
           │      → "Tried to install hook but function is unavailable." (DETOUR:164)
           │      → "Failed to add hook, detour installation likely failed!" (:284)
           │      → throw "EngineTick callback registration failed" (LUAMOD:4536)
           │    ● R3 marshal world (launch-11, staged DIFF-1+DIFF-2+consumer):
           │      DIFF-2 compiles OUT initializer registrations (NOT the resolves —
           │      GATE-1 survives), logs "[R3] refused: …" + "deferring game-thread
           │      pump to R3 marshal"; consumer enqueues closures onto the marshal;
           │      carrier SetFuncPtr = heap-tier data write, ZERO exec writes
           │      ⇒ "[R3] FuncPtr-marshal armed on carrier …Actor:ReceiveTick …"
           │      ⇒ "[R3] marshal is the game-thread pump (initializer deferred …)"
           │      drain: engine_tick_hook→process_simple_actions (LUAMOD:4488→4113)
           │      ═══ GATE-4: DIFF-1 ladder still tries EngineTick FIRST and throws
           │          at the first default-method call (RUNBOOK §5.1) ═══
           │
           ├─ M4 PAD INPUT ── UpdateThread, independent of M3
           │    eager bytes: scePadOpen → 0x80920004 ALREADY_OPENED → fail-closed
           │    (captured run3/run4 "runtime_start_failed code=3 … error=-2137915388")
           │    wpd1 defer: attached-but-unavailable, startup proceeds
           │    wpd2 retry (STAGED): poll() unavailable-branch → maybe_retry(),
           │      first at poll 200 (~1 s), doubling → 3200-poll cap (~16 s)
           │      sdiag tags: pad_open_class / pad_retry / pad_retry_failed / pad_available
           │      ═══ GATE-3: succeeds only if the game's exclusive handle frees ═══
           │    delivery: Handler::process_event → poll → rising OPTIONS edge
           │      (edge :85) → Lua callback on the UpdateThread (LUAMOD:1937-1948)
           │    ═══ G-AP1: NO per-press tag exists in the composed runtime ═══
           │
           └─ M5 MENU (needs M2 ∧ M3-draining ∧ M4-edge at the SAME press)
                MAIN:695 ExecuteInGameThread(toggleMenu)
                  EngineTick world: throw at LUAMOD:4719 (availability trap, GATE-4)
                  R3 world (post ladder-fix): enqueue → carrier → drain → game thread
                toggleMenu → showMenu(true) → scanItems (first open)
                  ⇒ "[Lua] [TownfallItemSpawner] <N> spawnable pickups found. Pages=<M>" :189
                buildUI → UMG widget tree (live ProcessEvent proof) → AddToViewport :631
                  ⇒ "[Lua] [TownfallItemSpawner] Menu created."                :643
                press #2+ mute by design; console open ⇒ allow_input swallows all

 M6 SPAWN (one drained closure, MAIN:215-348; needs M5 first)
    stage "player"            :224  FindFirstOf PlayerController/pawn/location/yaw
    stage "ground trace"      :240  KismetSystemLibrary:LineTraceSingle :245
                                    ── GAP-L2a FHitResult OUT readback ──
    stage "load pickup class" :279  StaticFindObject → LoadAsset(100% PE) → re-query
    stage "spawn actor"       :293  BeginDeferredActorSpawnFromClass :295 + Finish :305
                                    ── GAP-SPAWN engine-internal SCO path ──
    stage "configure pickup"  :311  PickupInteractable.ItemId read, Amount, SetEnabled
    success ⇒ "[Lua] [TownfallItemSpawner] Spawned <name> x<N> ID=<id>"  :333-334
    failure ⇒ "[Lua] [TownfallItemSpawner] Failed at <stage>. … <err>"   :341-342
```

## The order, plainly

1. **M1 is the master gate.** Seeds binding is the EASY part — every seed line is host-proven
   machinery. The initializer's remaining work walks straight into the first exec-text read
   (`Finding GameEngine::LoadMap`, UI_FC:1877). Under launch-10's fold-0 walkguard that read
   refuses loudly and throws; the throw is caught fail-closed; **no event loop, no mods, no
   pump, no input, no menu — the entire rest of the chain dies at that one line.** The oracle
   (ORACLE-SUPPLEMENT S1.2) is explicit: F34/F35 (engine-ready/mods-started) are unreachable
   past W2. Honest flag: SPAWN-CAPABILITY.md's "the mod loads and logs" sentence is in tension
   with this; the source reading (walk site precedes the event loop, PROG:619→673) sides with
   the oracle.
2. **M2 starts only off a completed M1** — the event loop thread that runs `on_program_start →
   start_lua_mods` does not exist until `setup_unreal()` returns cleanly.
3. **M3 and M4 are side conditions, not chain links.** They proceed in parallel with M2 from
   M1's completion: the pad retry (M4) is UpdateThread-paced and engine-state-independent;
   the R3 marshal arm (M3) is engine-driven (carrier ReceiveTick) and needs no keypress. But
   M5 needs BOTH to be simultaneously true — a delivered edge AND a draining pump.
4. **M5 is the AND-gate milestone**: first milestone whose blocker list has no single owner.
5. **M6 is strictly after M5** (menu open → row confirm → drained spawn closure).

## Shared gates (one gate, many milestones blocked)

| gate | what it is | milestones blocked | lift |
|---|---|---|---|
| GATE-1 exec-read (walkguard fold) | vtable-resolve walks read exec-only text; fold-0 refuses+throws | M1 fatal → cascades to ALL (M2-M6) | IGW RX verdict / scoped read route. NOTE: R3's DIFF-2 does NOT lift it (registrations compiled out, resolves not) |
| GATE-2 exec-write (PLH detours) | every detour needs text write + PolyHook disassembly | M3 normal arms (EngineTick/PE/LoadMap/FName) | none today (E1); R3 SetFuncPtr marshal is the designed bypass |
| GATE-3 pad ownership | scePadOpen ALREADY_OPENED, game-owned handle | M4; (pre-wpd1 bytes also M1/M2 via fail-closed throw) | wpd2 retry + freed handle, or the open-class probe |
| GATE-4 availability predicate | `IsEngineTickAvailable()` = AOB-found, never true (scan dead) | M5/M6 at the first ExecuteInGameThread — even if a pump armed | DIFF-1 ladder re-order (marshal-first), distinct-ID revision |
| GATE-5 LuaMod eager LoadMap | on_program_start's own RegisterLoadMapPreCallback arms a detour → throws (R3 world) | M2 (no mods) → cascades | unowned today (RUNBOOK §5.2); needs scoped guard |

## Host-testable vs live, one line each

- **M1**: seed/admission machinery + line ORDER host-proven (69/0 fixtures, gate 15/15);
  console traversal LIVE-only.
- **M2**: mod tree/loadlist/manifests host-rehearsed; script execution LIVE-only.
- **M3**: consumer bytes + determinism host-proven; arm/pump/drain lines LIVE-only (F-1 open).
- **M4**: retry/backoff/threading/lease host-proven (54 checks); a real open + real edge LIVE-only.
- **M5**: menu logic host-readable; edge→drain→UMG chain LIVE-only.
- **M6**: LIVE-only, full stop — zero live PE/trace/spawn evidence exists; every failure is
  pcall-caught with a distinct stage string.

Every in-game line in this chain is a PREDICTION of evidence shape, not a captured result.
Nothing here closes a rung.
