# DRAIN-TRACE — ExecuteInGameThread end-to-end in the composed launch-10 units

Worker: worker-executedrain-trace · 2026-10-05 · LOCAL ONLY (source + retained-image byte evidence; no console contact, no target execution, no Ghidra full run).

Every hop below is pinned file:line in `CLAIM.json`. Units are the ACTUAL composed launch-10
units (launch10-final-a, `outputs/2026-10-04-zcode-controller/launch10-build/EDITION.json`),
identified from the composed run receipt — not the port-tree lookalikes:

| unit | composed source | sha256 (prefix) |
|---|---|---|
| unreal-unrealinitializer | `launch10-build/final-units/UnrealInitializer.cpp` (readpath-guard v2 + gmalloc V1/V2 + spanpub; EngineTick block identical to the readpath-guard staging, +69 line shift) | `c3a1c9eb…` |
| luamod | `.superpowers/native-dependency-spike/ue4ss-native-notify-cancel-1/src/LuaMod.cpp` (per `worker-launch10-prebuild/…/run-a/receipt.json`) | `0f0697a3…` |
| program | `worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp` | — |
| mod payload | `worker-itemspawner-bundle/bundle/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | — |

In the hop pins below: **UI** = final-units UnrealInitializer.cpp, **LM** = notify-cancel LuaMod.cpp,
**DI** = DetourInstance.hpp, **XD** = x64Detour.cpp (PS5 PolyHook port), **MLS** = LuaMadeSimple.cpp.

## 1. The end-to-end chain

```
 [user presses F7]
      |  keybind fires (input port; its own precondition, not traced here)
      v
 main.lua:694-696  RegisterKeyBind(Key.F7, …)          main.lua:695 ExecuteInGameThread(toggleMenu)
      |
      v  HOP A — Lua binding (runs on the CALLING thread, not the game thread)
 LM:4700  register_function("ExecuteInGameThread")
 LM:4709    method = m_default_game_thread_method      <- comp6-program UE4SSProgram.cpp:1494
            <- settings ini line 8: DefaultExecuteInGameThreadMethod=EngineTick (explicit in BOTH staged bundles)
 LM:4725-31 EngineTick branch: is_engine_tick_hook_available()?          (UE4SSRuntime.cpp:10-16
            -> ensure_engine_tick_hooked()  [LM:4527-4541; registers LM:4535 lazily]   = TickInternal.is_ready())
 LM:4736-40 make_hook_state + luaL_ref the callback
 LM:4762    m_engine_tick_actions.emplace_back(SimpleLuaAction)         <- THE QUEUE (host-side vector
            (or m_pending_engine_tick_actions, LM:4759, mid-drain)         under m_thread_actions_mutex)
      |                     ^ enqueue touches NOTHING engine-side
      |                     |
      |  HOP B — the PUMP (what makes the queue drain at all)
      |
      |  [game startup, long before any F7]
 UI:1053     "[Seeds] Required-slot seed table armed (slots 0/2/3/4)"      (arming gate 1040-51, SHAPE A)
 UI:1078     SeedArming::consume_required_slot_seeds  (read-path-guarded seed binds; slots 0/1/3
             DataReadable-only, slots 2/4 CodeCallOnly bind-only)
 UI:1518     Initialize(const Config&)
 UI:1819     LoadMap vtable walk  <-- FIRST exec-page READ (ASMHelper Zydis thunk check)  <- launch-9/igf6 crash class
 UI:1827-28  "Finding GameEngine::Tick"  -> vtable_address = OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL(UEngine, Tick, GameEngine)
             (macro: UnrealVirtualBaseVC.hpp:132 vptr read [RW data, OK] -> :133 vtable ENTRY read [R-- data, OK]
              -> ASMHelper.cpp:52 -> 9-22 ZydisDecodeFull AT THE TARGET = data-mode READ of the --X LOAD0 page)
 UI:1865     EngineTickResolveMethod == Scan  (default, SettingsManager.hpp:126; no [Hooks] in the staged ini)
 UI:1892-3   scan_address is NULL on the armed build (tick deliberately NOT seeded, slot 10;
             comment UI:1867-84) -> "Scan failed, falling back to vtable address for GameEngine::Tick"
             -> UEngine::TickInternal.assign_address(vtable_address)     <- TickInternal.is_ready() now TRUE
 UI:1913     RegisterEngineTickPreCallback(HookedEngineTick, {GameThreadInitializer})   <- arms the detour:
             Hooks.cpp:239-42 -> DI:284 AddCallback -> EnsureHooked (DI:232-39)
             -> DI:164-6  PLH::x64Detour(TickInternal.get_function_address(), &StaticHookFn, &Trampoline)
             -> DI:183    hook():  XD:325 disassemble(target, +100 bytes)   <-- exec READ #2
                                   XD:~399 MemoryProtector(target, RWX) + writeEncoding  <-- exec WRITE (FLAG-1)
             ERROR_ID -> UI:1916 throw "Native game-thread initializer callback registration failed"  (LOUD)
 UI:1924     RegisterEngineTickPreCallback(QueryHarnessEngineTickDispatch)  (persistent tick witness)
 UI:2116     PostInitialize (GMalloc null-throw at UI:394-97 already passed: slot 3 bound at UI:1078)
      |
      v  [the game RUNS its real GameEngine::Tick at linked vaddr 0x2fac8c0 (runtime 0x33ac8c0 = 0x400000+0x2fac8c0;
         valid x86-64 prologue byte-verified in the sha-pinned eboot 3b6cfad4… — see CLAIM.json hop 12)]
      |
      |  the DETOUR fires, in callback order:
      |   1. HookedEngineTick (UI:161-221): publishes NativeGameThreadIdentity (UI:172) on the FIRST genuine
      |      tick, then RemoveSelf (UI:219).            <- HOP C: publication is a DRAIN PREREQUISITE
      |   2. QueryHarnessEngineTickDispatch (UI:226-229): per-tick harness witness.
      |   3. engine_tick_hook (LM:4467, registered lazily at the first ExecuteInGameThread call):
      |          LM:4471-6  if (!IsInGameThreadRaw()) return;     <- needs publication from step 1
      |          LM:4478-80 NativeLuaActionDispatchScope owns_dispatch
      |          LM:4488   process_simple_actions(m_engine_tick_actions)      <- THE DRAIN
      |          LM:4489   process_delayed_actions<EngineTick>(…)             (WithDelay/AfterFrames variants)
      |
      v  HOP D — the drain executes the deferred closure
 LM:4113-52  process_simple_actions: swap the vector out under the mutex; per action:
 LM:4137-8     get_function_ref + lua_data.lua->call_function(0, 0)
 MLS:971       lua_pcall(L, num_params, num_return_values, err_handler_abs)      <- a PURE LUA VM CALL
      |
      v
 toggleMenu() runs on the game thread -> showMenu(true) -> buildUI():
      "Menu created."            main.lua:643   <- first-drain proof line (UI build; uses ':' UFunction calls)
      "UI creation failed: …"    main.lua:636   <- drain PROVEN, closure failed above it
 [ENTER] row.activate -> spawnItem -> gameplay:BeginDeferredActorSpawnFromClass (main.lua:297-299)
      + FinishSpawningActor (301)  <- the ACTUAL spawn is a UFunction call (ProcessEvent machinery)
      "Spawned <item> x<n> ID=<id>"  main.lua:335/337
```

## 2. Q2 — does the drain need ProcessEvent/CallFunction?

**NO. The drain requires ONLY the EngineTick closure.**

- The queued artifact is a `SimpleLuaAction` (registry ref to a Lua function + a hook Lua thread
  state). The drain is `process_simple_actions` (LM:4113) -> `Lua::call_function` (MLS:949) ->
  `lua_pcall` (MLS:971). `ProcessEvent`, `CallFunction`, `UFunction`, and every other engine
  adapter appear **nowhere** in the enqueue->drain path.
- The ONE engine dependency is that the engine's own `GameEngine::Tick` executes through the
  detour (that is what fires `engine_tick_hook`), plus the game-thread identity publication
  (hop C) that the PROSPERO gate at LM:4471-6 checks.
- `ProcessEvent` is needed only by (a) the OTHER `EGameThreadMethod.ProcessEvent` branch — which
  launch-10 never takes (`DefaultExecuteInGameThreadMethod=EngineTick` is explicit in the staged
  settings ini line 8; no `[Hooks]` section overrides hook defaults, so `HookEngineTick=true`,
  SettingsManager.hpp:125, survives — the mod-load-readiness worker's claim VERIFIED at
  UI:1913/1924 + ini), and (b) the mod's own `:` UFunction calls ABOVE the drain (menu UI at
  main.lua:529/633/634, spawn at main.lua:297-301). Note `ProcessEventInternal` also gets its
  address assigned by the same vtable route at UI:1981 if the walks survive — the address gap
  closes with the same F29 evidence; the FUNCTIONAL marshaling gap (r3 DIFF-1, not composable
  into launch-10) does not.

So the two workers were each right at a different layer:

| claim | verdict |
|---|---|
| mod-load-readiness: "pump registered and READY in the readpath-guard lineage; hook defaults keep HookEngineTick=true" | **TRUE in-source.** UI:1913/1924 unconditional under `bHookEngineTick` (default true, no ini override); the drain design needs no ProcessEvent. |
| pc-ps5-comparison: "ProcessEvent/CallFunction is the widest gap this path sits on top of" | **TRUE one layer up.** The DRAIN doesn't need them, but the pump ARM needs exec-page READ (resolve thunk-check + PolyHook disassembly) and WRITE (detour patch) to have survived in-game, and the menu/spawn EXECUTION above the drain needs the UFunction call machinery to work. |

## 3. Q3 — the resolve branch the armed build takes, and its exec-page needs

- `EngineTickResolveMethod` default = **Scan** (SettingsManager.hpp:126; the staged console ini
  has NO `[Hooks]` section). On the armed build the scan arm is structurally dead
  (UI:1867-84: slot 10 deliberately NOT seeded; `scan_address` NULL), so Initialize takes the
  **Scan branch's vtable fallback** at UI:1892-1893. The tick address enters ONLY from the live
  vtable — no constant is compiled in.
- **Exec-page-readable: YES, it is needed — FLAGGED as requested.** Two reads of exec memory sit
  on the registration path:
  1. **Resolve**: every `OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL` Zydis-decodes the target's first
     instruction bytes (`UnrealVirtualBaseVC.hpp:133` -> `ASMHelper.cpp:52` -> `:9-22`) — a
     data-mode read of the `--X` LOAD0 segment. The **LoadMap walk at UI:1819 fires FIRST**
     (HookLoadMap default true, SettingsManager.hpp:119), before tick. The initializer's own
     arming comment names this the "igf6 crash class" (UI:1016-23), and oracle row F29 calls
     these walks "the launch-9 crash site".
  2. **Detour**: `PLH::x64Detour::hook()` disassembles up to 100 bytes at the target
     (XD:325) before patching — exec read again — then mprotects RWX and writes the jmp
     (XD:~399-408) — the FLAG-1 exec-write class, which the initializer comment already pins as
     "fails LOUD (ERROR_ID throw) if refused" (UI:1880-84 -> UI:1916).
- **Launch-10 blocker status**: launch10-final-a does **NOT** include the walk-site RX guard
  (that is a separate candidate, worker-launch10-walkguard-prep; EDITION.json edits exclude the
  asmhelper unit). So if exec pages are data-read-faulting on this kernel, launch-10 dies at F29
  before the pump ever registers. The in-game verdict that decides it (IGW `W_WORKS` /
  `W_FAILS_R_WORKS`) is still MISSING — the exec-write probe ran with all rows SKIPPED
  (`launch10-prep/xwp-jsonl-baseline.txt`: `rows_attempted:0, rows_skipped:6`), and the igf6
  mechanism is explicitly NOT localized. Honest status: **flagged blocker-class risk, decided by
  the F29 log evidence at launch-10 itself.**

## 4. Q4 — target function existence (tier-1, no Ghidra run)

- Retained image `…/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
  (sha256 `3b6cfad4…`, matches the readpath-guard geometry pin).
- Layout profile pins `GameEngineTick` = `validated-static`, `elf_image_vaddr 0x2fac8c0`
  (`worker-2-layout-facts/profiles/townfall-ps5-ue56.profile.json`).
- **Capstone decode of the bytes at 0x2fac8c0** (LOAD0, file offset == linked vaddr):
  `push rbp; mov rbp,rsp; push r15/r14/r12/rbx; cmp byte ptr [rip+0x633b9ea],1; jne …; mov
  r14,rdi; mov rbx,rsi; mov r15,rdx; call 0x178b20; lea rsi,[rip+0x4a2d983]…` — a real x86-64
  function entry (PS5 is x86-64), not data, not `ud2`.
- Address-class reconciliation, confirmed three independent ways (SeedTable rip_arithmetic
  fields, CP2 pin, oracle F29): runtime = base `0x400000` + linked `0x2fac8c0` =
  **`0x33ac8c0`** — exactly the value oracle F29 expects in the launch-10 log
  (`GameEngine::Tick address (vtable: 0x33ac8c0; scan: <null>)`).
- **Ghidra 12.1.4 full analysis was NOT run** (budget; see REPORT.md). What it could add
  (semantic identity of the function body) is already covered by the profile's validated-static
  survey and would not change the verdict — the verdict turns on the in-game exec-page verdicts,
  not on static identity. The F29 log line printing the live vtable value is the runtime
  confirmation that supersedes any static analysis.

## 5. VERDICT: **CONDITIONAL**

The drain is source-complete and needs only the EngineTick closure — **there is no missing
PROCESS-EVENT-class hop in the drain itself**. What the verdict hangs on is the pump arm plus
the layer above the drain:

| # | condition | decides | if it fails |
|---|---|---|---|
| C1 | F29 vtable walks survive (exec-page data reads: LoadMap UI:1819, Tick UI:1828, then the detour disassembly XD:325) | pump resolve + hook install | crash at the launch-9 crash site; log ends after `Finding GameEngine::LoadMap` → SPAWN-PATH-BLOCKED at the resolve walks (exec-read class, igf6 mechanism not localized) |
| C2 | detour WRITE accepted (FLAG-1: mprotect RWX + jmp patch, XD:~399) | pump arm | LOUD named throw `Native game-thread initializer callback registration failed` (UI:1916) → status failed → SPAWN-PATH-BLOCKED at the detour write, cleanly |
| C3 | game-thread publication succeeds (UI:172 first-tick publish) | drain gate LM:4471-6 | closures queue forever; F7 silently dead (no log line at all) |
| C4 | UFunction-call machinery works (menu `:` calls main.lua:529/633/634; spawn `BeginDeferredActorSpawnFromClass` main.lua:297-301) | menu OPEN and actual SPAWN — outside the drain, the pc-ps5-comparison gap; address lands via UI:1981 under C1, functional marshaling unproven (DIFF-1 not composable) | drain runs, closure errors: `UI creation failed:` / `Failed at <stage>.` lines — menu may not open or spawn fails |

C1-C3 are the same facts the LAUNCH10-MATRIX already encodes as rows R1/R2 (`W_WORKS`) vs R3/R4
(`W_FAILS_R_WORKS`) vs R5 (`BOTH_FAIL` — "F7-2 dead (no closure drain)"): this trace derives the
identical structure from source.

### Log lines launch-10 should show when the first deferred closure drains successfully

Preconditions (pump armed, in order):

```
Finding GameEngine::LoadMap
GameEngine::LoadMap address 0x<hex>
Finding GameEngine::Tick
GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x0)
Scan failed, falling back to vtable address for GameEngine::Tick
Finding GameModeBase::InitGameState          <- proves the walks progressed past Tick
```
(and in runtime-status: `state <k> engine-ready 0`; NO `failed` line naming
`Native game-thread initializer callback registration failed`)

Mod loaded (precondition only — runs at script load, NOT through the drain):

```
[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.
```

**THE first-drain proof — after F7 is pressed:**

```
[Lua] [TownfallItemSpawner] Menu created.
```

(main.lua:643 — this print executes INSIDE the `ExecuteInGameThread(toggleMenu)` closure, i.e.
after queue -> EngineTick detour -> publication gate -> `process_simple_actions` -> `lua_pcall`.)
`[Lua] [TownfallItemSpawner] UI creation failed: <err>` (main.lua:636) equally proves the drain
ran and the closure failed above it. F7 with NEITHER line and no Lua error line = the drain never
ran (silent C3 class). The later `[Lua] [TownfallItemSpawner] Spawned <item> x<n> ID=<id>`
(main.lua:335) is the visible-spawn rung and additionally proves C4.
