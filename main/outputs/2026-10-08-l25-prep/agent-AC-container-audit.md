# Agent AC — Claude review item 3: engine-owned container growth/write audit, :528 → "Lua mod started"

Date: 2026-10-08. READ-ONLY on all trees; this report is the only file written.
Scope: every engine-owned-container growth/write (and every GMalloc-virtual-call
mediated by our RC wrappers) on the runtime path from
`UnrealInitializer.cpp:528` to the "[l22] Lua mod started" marker, for variant
B (skip `:528` under `MODBRIDGE_L25_SKIP_DELETE_LISTENER`) planning.

## 0. Sources (all sha256-verified against l24-build-r2/SOURCE-SELECTION.json)

| Unit actually compiled into l24 (`2e712a8b…`) | sha256 (prefix) |
|---|---|
| `outputs/2026-10-08-l24-runtime/typescan-r3/candidate/src/UnrealInitializer.cpp` | `78c16fb7c06a` (the file whose `:528` crashes; `[R3]` strings match live log) |
| `outputs/2026-10-08-l23-runtime/thread/src/UE4SSProgram.cpp` | `030407d7351e` |
| `outputs/2026-10-08-l23-runtime/thread/src/LuaMod.cpp` | `fc9589dbb81e` |
| `outputs/2026-10-08-r5-source-integration/source/UFunction.cpp` + `ScriptHookThunks.cpp` + `ScriptFunctionStorage.hpp` | `9a2863176c0c` (UFunction.cpp) |
| port root `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`: `unreal/src/UObjectArray.cpp` (`171560a95ff0`), `unreal/src/Hooks/Hooks.cpp` (`2db5b92f5b20`), `unreal/include/Unreal/Hooks/Internal/{DetourInstance.hpp,detourSubclasses.hpp,Metadata.hpp,CallbackGarbageCollector.hpp}` (`8e593c0627b6`/`4b393174f758`/`a6e5bb72ed75`/`3b956ba96f4c`), `unreal/src/FMemory.cpp` (`39d5b3e232ae`), `unreal/src/NameTypes.cpp`, `unreal/src/UObject.cpp`, `unreal/src/UObjectGlobals.cpp`, `unreal/src/ObjectDumper/ObjectToString.cpp`, `unreal/src/UObjectHashTables.cpp` (`068b33ab82d5`) | verified in pin file |
| `ue4ss-native-hash-tables-1/source/UObjectHashTablesNativeUnavailable.cpp` + `NativeHashTablesUnavailablePolicy.hpp` | pinned |
| `ue4ss-polyhook-platform-port-1/patched/include/polyhook2/PolyHookOs{,Includes}.hpp` + owned-memory polyhook headers (`OwnedMemory.hpp`, `OwnedMemoryAllocatorBridge.hpp`, `OwnedTrampoline.hpp`, `ZydisDisassembler.hpp` from `ue4ss-native-*-1`) | pinned |
| Settings actually live: `evidence/l24-settings-backup.ini` (the byte-exact 1947 B original, sha `b980cea7…`) | read |
| Evidence: `evidence/l24-ab2-ue4ss.log`, `evidence/l24-ue4ss-4.log`, `evidence/l24-aligned.log`, `evidence/l24-diag.jsonl` | read |

Thread identities (from Z11 + source): everything from `:528` through
`init()` return runs on the **UE4SS-InitThread**; `update()` →
`on_program_start()` → `install_lua_mods` → `LuaMod::on_program_start` →
`start_lua_mods` → `LuaMod::start_mod` run on the **UE4SS-UpdateThread**
(`m_event_loop` jthread, UE4SSProgram.cpp:715→1584→1598). Both are non-game
threads; the rejected-Realloc mechanism is indifferent to which of them makes
the virtual call into GMalloc.

## 1. The mechanism under test (what ":528 family" means here)

`UObjectArray::AddUObjectDeleteListener` (port `unreal/src/UObjectArray.cpp:379-389`):

```cpp
auto& DeleteListeners = GUObjectArray->GetUObjectDeleteListeners(); // ENGINE object member
if (DeleteListeners.Contains(Listener)) { throw …; }                // engine-array read
DeleteListeners.Append(TArray{Listener});                           // (a) temp TArray ctor → GMalloc->Malloc(8)
                                                                    // (b) engine-array growth → GMalloc->Realloc
                                                                    // (c) temp dtor → GMalloc->Free
```

All three steps are virtual calls into the game GMalloc via
`TSizedDefaultAllocator<32>` → `TSizedHeapAllocator<32, RC::Unreal::FMemory>`
→ `FMemory::Realloc` → `RC::Unreal::FMalloc::Realloc` (`unreal/src/FMemory.cpp`,
`IMPLEMENT_UNREAL_VIRTUAL_WRAPPER`) — exactly the Z11 symbolized chain. The
engine array data pointer is game-owned; the l24 stack proves step (b) ran
(`ResizeTo → ResizeAllocation → FMemory::Realloc → game GMalloc`), i.e. the
engine array had **no slack** at init time, so Append had to grow it.
`AddUObjectCreateListener` (`UObjectArray.cpp:366-372`) is the identical
construction against `GUObjectArray->GetUObjectCreateListeners()`.

Second GMalloc exposure family found (NOT a container growth, same allocator
virtual-call risk): **any `UObject::GetFullName()` / `UObject::GetName()`** on
this build routes `FName::ToString` → `ToStringInternalWrapper`
(`unreal/src/NameTypes.cpp:106-117`) → the Scan method is UNAVAILABLE on PS5
(`[Seeds] Unbound by design: fname_tostring`, live log line 24) → falls to
`ToStringInternalWrapper_UsingConv_NameToString` →
`UKismetStringLibrary_Conv_NameToString` (`NameTypes.cpp:70-105`) →
**`FName::KismetStringLibraryCDO->ProcessEvent(FName::Conv_NameToStringInternal, ParamData)`**
— a full engine ProcessEvent call that allocates the return FString through
GMalloc inside engine code, plus port-side `FStringOut` (GMalloc-backed)
construction/destruction. **The entire pre-:528 init path was engineered to
avoid exactly this** (`StaticFindObject_InternalNoToStringFromStrings`,
`[n: <unstable-int>]` prints instead of names, `UObjectGlobals.cpp:176`
comment); no GetFullName/ToString call has ever been observed on console in
this lineage.

## 2. Ranked site table (execution order after B skips :528)

Rank = crash-relevance under the rejected-Realloc/first-GMalloc-call mechanism;
"Order" = when the site executes after the skip.

| # | Order | Site (file:line in pinned unit) | Container / engine state written | Grows via engine allocator? | Thread | Risk | Verdict for B |
|---|---|---|---|---|---|---|---|
| **1** | **1st** | **`LuaMod.cpp:7995`** `Unreal::UObjectArray::AddUObjectDeleteListener(&LuaType::FLuaObjectDeleteListener::s_lua_object_delete_listener)` (first stmt of `LuaMod::on_program_start`) | ENGINE `GUObjectArray` delete-listener TArray: GMalloc Malloc (temp) + Append/Realloc + Free | **YES — identical to :528 (all 3 GMalloc touches)** | UpdateThread | **CRITICAL — B dies here with the same stack shape one call later** (the :528 crash proves the engine array needed growth at init; slack state minutes-of-save-load later is unknown; the Malloc/Free alone are the same virtual call) | **GATE under the same switch (G1)** |
| **2** | 2nd | **`LuaMod.cpp:5007, :5012, :5017`** (r3_marshal_carrier_precheck refusal prints), **`:5050, :5056`** (arm_funcptr_marshal_drain prints), **`:5099, :5105`** (event-carrier prints) — all call `carrier->GetFullName()` | none (reads) but triggers ENGINE ProcessEvent(Conv_NameToString) + engine-side GMalloc FString alloc + `FStringOut` GMalloc free, (outers+1)× per call | **YES — GMalloc virtual calls inside engine code** | UpdateThread | **HIGH — expected to fire**: the two ranked candidates ("Actor:ReceiveTick", "AnimInstance:BlueprintUpdateAnimation") are BlueprintImplementableEvents lacking `FUNC_Native`, so precheck REFUSES at `:5017` and prints GetFullName for each; armed carriers print again at `:5105` (≤8). Unproven engine-call class (§1) | **GATE: print the candidate/carrier STRING LITERALS already in hand instead of GetFullName (G2)** — zero functionality loss |
| 3 | 3rd | `LuaMod.cpp:5047 / :5096` `carrier->RegisterPostHook(...)` → `ScriptHookThunks.cpp Register()` → `function->SetFuncPtr(thunks[index])` | ENGINE `UFunction::Func` field: ONE atomic 8-byte pointer store into an existing RW engine object slot (`ScriptFuncStorage.hpp Store`, `__atomic_store_n`, alignment-checked) | **NO allocation** (staging `unordered_map`, slots array, `HookedUFunctionMap` — all our CRT side; original Func saved in our map) | UpdateThread | LOW for the rejected-Realloc mechanism (no allocator involvement; pure slot store). Residual: unlocked-by-engine-standards data write to a live UFunction racing game-thread Invoke | **LEAVE (the pump; L3+R3 delivery needs it)** |
| 4 | before #1 | `UnrealInitializer.cpp:546-574` `Hook::RegisterStaticConstructObjectPostCallback(...)` → Hooks.cpp:76 `AddCallback` → DetourInstance.hpp:284 `EnsureHooked()` → InstallHook (target `StaticConstructObjectInternal426`, slot-4 seed bound `0x890aa0`, live log line 539) → `ActivateHook` → `PLH::x64Detour::hook()` | nominally engine text (detour prologue); **in this composition it soft-REFUSES first**: every PLH touch of live game text is mediated by the owned-memory registry, engine text is exec-only (live `[WalkRxGuard] jmp-follow skipped (exec-only text; no read)` lines), prologue read returns nothing → hook() false at the empty-insts check, BEFORE any protection or write (measured for the identical AddCallback→EnsureHooked path in the composed unit; documented in `LuaMod.cpp:8005-8050`; same owned polyhook composition pinned in l24) | NO GMalloc; no engine write occurs; cost = one PLH SEV log + one "[…] Failed to add hook" Error line + ERROR_ID (ignored by :547) | InitThread | LOW (log noise only). Caveat: this specific arm has never executed on console (nothing past :528 ever ran) | LEAVE as-is; optionally quiet under B (G4). Not needed for mod start (searcher-pool feeder; cache off) |
| 5 | pre-#1 | `UnrealInitializer.cpp:543` `Hook::StartCallbackGarbageCollector()` → CallbackGarbageCollector.hpp:82 `StartGCThread` | none — lazy singleton + `std::jthread` + empty `std::vector` links (no detour ever activated ⇒ `Collect` no-ops); pure CRT | NO | InitThread | none | LEAVE |
| 6 | pre-#1 | `UnrealInitializer.cpp:530-531, :576-585` (`IsInitialized`=true, `bIsInitialized`=true, StaticSize/Alignment prints) | our statics; compile-time sizes | NO | InitThread | none | LEAVE |
| 7 | pre-#1 | `UE4SSProgram.cpp:1355-1387` HashTables branch of `setup_unreal` | none — on PROSPERO `FUObjectHashTables::IsConfigured()` returns false (`UObjectHashTablesNativeUnavailable.cpp` + policy header), so only the "[HashTables] … unavailable" warning prints; no `RunSelfTest`, no EngineTick registration, no `RetireUObjectArrayCache` | NO | InitThread | none | LEAVE |
| 8 | pre-#1 | `UE4SSProgram.cpp:663-688` "Unreal Engine modules" listing | reads of our `SigScannerStaticData::m_modules_info` | NO | InitThread | none | LEAVE |
| 9 | pre-#1 | `UE4SSProgram.cpp:690` `fire_unreal_init_for_cpp_mods()` | none — the native mod source (`bootstrap/activated_mod_source.cpp`) supplies **Lua mods only**; zero `CppMod` instances ⇒ no-op (same for `fire_program_start_for_cpp_mods` :1571 and `fire_on_lua_start_for_cpp_mods` LuaMod.cpp:7540) | NO | InitThread/UpdateThread | none | LEAVE |
| 10 | pre-#1 | `UE4SSProgram.cpp:691` → `:1789-1820` `setup_unreal_properties`: 29× `FName(STR("…Property"), FNAME_Find).GetComparisonIndex()` + CRT map emplace | engine FName pool LOOKUPS only (FNAME_Find never inserts/allocates; missing name ⇒ NAME_None ⇒ key 0, harmless) | NO allocation (engine ctor read path) | InitThread | LOW — this exact call class is PROVEN pre-:528 (live log: "Already constructed: …" ×~30 from `AddRequiredObject`'s FNAME_Find, "Initializing global FName variables", Kismet lookups; Z10 r4:1142/959-967 fnameabi behaviorally verified id 0x3) | LEAVE |
| 11 | pre-#1 | `UE4SSProgram.cpp:692` `UAssetRegistry::SetMaxMemoryUsageDuringAssetLoading` | our `StaticStorage` int (`UAssetRegistry.hpp:77`) | NO | InitThread | none | LEAVE |
| 12 | pre-#1 | `UE4SSProgram.cpp:694-703` share_lua_functions; `m_is_program_started`; `on_engine_initialized` (RuntimeAckSink — ours); `:715` jthread | ours | NO | InitThread | none | LEAVE |
| 13 | pre-#1 | `UE4SSProgram.cpp:1454-1520` GUI / debug-console / keydown registrations | **compiled out** (`#ifndef __PROSPERO__`); `ConsoleEnabled=0` in ini anyway; `TIME_FUNCTION` block compiled out | NO | — | n/a | n/a |
| 14 | pre-#1 | `UE4SSProgram.cpp:1523` `ObjectDumper::init()` → ObjectToString.cpp:352+ | `StaticClass()` = pure pointer read (StaticClassStorage pre-populated by the pre-:528 `[TypeScan]` pass that logged "scan-complete"; fallback find is ToString-free), `HashObject()` math, inserts into our `std::unordered_map` | NO | UpdateThread | none | LEAVE |
| 15 | pre-#1 | `UE4SSProgram.cpp:1569` `install_lua_mods()` → `:1907-1943 install_mods<LuaMod>` | `m_mods` bookkeeping (`set_installed(true)`) — our vector | NO | UpdateThread | none | LEAVE |
| 16 | after #2 | `LuaMod.cpp:8001` calibration consumer internals — `:5218` StaticFindObject ×2, `:5128-5132` base lookup, `:5140-5164` ForEachUObject scan (`GetNamePrivate().Equals` — ToString-free by design, `UObjectGlobals.cpp` NoToString family), CRT vectors; `:8063-8064` deferred-arm store + warning print (literals) | engine GUObjectArray/FName reads only; our atomics | NO | UpdateThread | none | LEAVE |
| 17 | after #2 | `LuaMod.cpp:8052` `R3MarshalPumpPending()==true` ⇒ the ENTIRE eager detour-arm block `:8068-8788` (RegisterLoadMapPre/Post, InitGameState, BeginPlay/EndPlay, StaticConstructObject, ProcessConsoleExec ×3, ULocalPlayerExec, CallFunctionByNameWithArguments, custom-event script hooks) is SKIPPED — no AddCallback, no EnsureHooked, nothing | none | NO | UpdateThread | none (this is what removes ~19 detour arms from B's path for free) | LEAVE |
| 18 | last | `LuaMod.cpp:7527-7576 start_mod`: prepare_mod / make_main_state / setup_lua_global_functions_main_state_only / make_async_state / start_async_thread (Lua + CRT); `load_and_execute_script` file read + lua_pcall; `:7557` "[l22] Lua mod started" | ours | NO GMalloc from runtime code | per-mod thread (started from UpdateThread) | — | **LEAVE — this IS B's destination.** Boundary note: main.lua (TownfallItemSpawner) executes BEFORE the marker; any engine calls it makes (FindObject, ExecuteInGameThread queueing = ours, RegisterKeyBind = ours+PS5Pad) are mod-controlled and NOT gated by any runtime switch — flag to Claude, do not gate blindly |
| X1 | (skipped today) | `UnrealInitializer.cpp:523` `AddUObjectCreateListener(&FClassCreateListener…)` | ENGINE create-listener TArray — same 3-touch GMalloc family | **YES** | InitThread | would fire BEFORE :528 if `bUseUObjectArrayCache=true`; ini (1947 B original) says `false` ⇒ block `:481-524` skipped (searcher pools + both class-listener registrations) | **Force-off under the B switch (G3)** so a settings change can't reintroduce it during B testing |
| X2 | (skipped today) | `UnrealInitializer.cpp:524` `AddUObjectDeleteListener(&FClassDeleteListener…)` | ENGINE delete-listener TArray — same family | **YES** | InitThread | same | same (G3) |

Related but NOT on the path: `UnrealInitializer.cpp:125`
`RemoveUObjectDeleteListener` (cache-retire path, cache off ⇒ unreachable);
`FLuaObjectDeleteListener::OnUObjectArrayShutdown` self-removal (engine
shutdown only); `FClassDeleteListener::Notify` pool pruning (engine-thread
dispatch of the class listener — unregistered while cache off). There is **no
`AddUObjectArrayShutdownListener` anywhere in the port or the l23/l24 units**
(grep: zero hits) — that API is engine-side only and is not called by this
runtime; nothing to gate. No `GEngine`/`GWorld` member writes exist anywhere
on the path (GameEngine/GameMode/ViewportClient are only read for CDO
existence checks pre-:528). No engine FName-pool WRITE (FNAME_Add) occurs
post-:528 on this path: the only FNAME_Add in the runtime (`Hooks.cpp`
`AddRequiredObject` fallback) runs pre-:528 and was never taken ("Already
constructed" for every required object in the live log).

## 3. Answers to the specific task questions

- **Create-listener prime suspect:** `AddUObjectCreateListener` would be the
  same mechanism, but in the shipped configuration it never executes
  (`bUseUObjectArrayCache=false` skips `:523`; there is no other create-listener
  registration in the l24 units). It matters only as X1: force it off under B.
- **First site B hits after the skip:** `LuaMod.cpp:7995` — the Lua
  delete-listener registration — NOT "one line later" but ~10 statements +
  2 thread handoffs later, with the identical 3-touch GMalloc append into the
  same engine `GUObjectArray` delete-listener TArray. If B gates only `:528`,
  the expected outcome is the same CE-108255-1 signature with
  `LuaMod::on_program_start` as the innermost modbridge frame instead of
  `PostInitialize`.
- **install_lua_mods / start_lua_mods engine growth:** NONE — verified pure
  CRT/Lua bookkeeping (`:1907-1943`, `:2051-2068`, `:7527-7576`). Nothing mod
  start provably needs is being gated by the plan below. The only
  engine-coupled things between the gated sites and the marker are the
  carrier-arm `SetFuncPtr` slot store (rank 3, needed for the pump) and
  main.lua itself (mod boundary).

## 4. GATE/SKIP PLAN (for the implementing agent)

Extend `MODBRIDGE_L25_SKIP_DELETE_LISTENER`'s meaning to "skip
engine-listener registrations and engine-name-tostring diagnostics" (Claude
authorized gating/skipping under the same B switch). Variant A keeps all of
these live.

- **G1 (REQUIRED for B to work): `LuaMod.cpp:7995`** — wrap the
  `AddUObjectDeleteListener(&LuaType::FLuaObjectDeleteListener…)` call in the
  B switch, with a loud one-line log ("[L25-B] Lua delete-listener registration
  skipped (diagnostic variant)"). Loss vs A: Lua `onDelete` callbacks never
  fire, and Lua-held stale object pointers are never cleared — same
  "unsafe long-term" caveat the review already attaches to B.
- **G2 (STRONGLY RECOMMENDED for B): the 7 `GetFullName()` print sites in the
  R3 consumer** — `LuaMod.cpp:5007, :5012, :5017, :5050, :5056, :5099, :5105`.
  Under B, print the in-hand string literal (`candidate_string` /
  `base_string` / carrier id) instead of `carrier->GetFullName()`. This
  removes every engine ProcessEvent(Conv_NameToString)+GMalloc cluster from
  B's path at zero functional cost (the pump, precheck, arming, and scan all
  keep working; only the log text changes). Alternative if touching 7 sites
  is too invasive: add a B-gated `to_log_name(UFunction*)` helper that returns
  the literal/`GetFName` comparison index (`[n: %X]` style, matching the
  proven init-gate log convention) and route the 7 prints through it.
- **G3 (belt-and-braces, cheap): `UnrealInitializer.cpp:523/:524`** — under B,
  hard-force the `bUseUObjectArrayCache` block off regardless of ini, so a
  future settings flip cannot reintroduce the same-family growth ahead of
  `:528` during B testing. (Today's 1947 B ini already has it `false`.)
- **G4 (optional, log hygiene only): `UnrealInitializer.cpp:546-574`** — the
  StaticConstructObject arm soft-refuses by composition (no engine touch), so
  it is SAFE to leave; if the implementer wants a clean B log, gate the
  registration too (nothing on the mod-start path consumes it while the cache
  is off).
- **DO NOT GATE:** `:543` GC thread start; the `setup_unreal_properties`
  FNAME_Find lookups (proven call class); the StaticFindObject/ForEachUObject
  scans in the R3 consumer (ToString-free); `RegisterPostHook`/`SetFuncPtr`
  (pump); `install_lua_mods`/`start_lua_mods`/`start_mod`; main.lua execution.
- **FLAG TO CLAUDE (no action):** main.lua runs before the "[l22] Lua mod
  started" marker — any engine-container growth inside TownfallItemSpawner's
  bootstrap (e.g., its own FindObject/ToString usage through our Lua API
  surface) is outside any runtime switch. If B reaches mod start but crashes
  inside main.lua, that is a separate, mod-level bisect.

## 5. Cross-check against l24 evidence (what real runs constrain)

- **Both crashing UE4SS logs end at exactly "Adding shutdown listener"**
  (`l24-ab2-ue4ss.log` line 602 @14:07:39 pid 266 VALID arm;
  `l24-ue4ss-4.log` @13:47:29 pid 208 original arm; the latter's trailing
  "module_start failed: mfa_activate_failed … size file is longer" line is a
  SEPARATE pid-222 launch's modbridge stderr with a wrong-size settings file —
  unrelated to the window). Zero post-:528 lines exist in ANY run: every
  statement above about post-:528 behavior is source-derived, with two
  composition-level precedents: (a) the measured launch-11 polyhook
  soft-refuse for this exact AddCallback→EnsureHooked→hook() path (documented
  in the pinned LuaMod.cpp:8005-8050 comment), and (b) the live pre-:528
  `[WalkRxGuard] jmp-follow skipped (exec-only text; no read)` lines proving
  engine text is exec-only and guarded.
- **Pre-:528 evidence PROVES these classes safe (leave ungated):** engine
  FName ctor calls (FNAME_Find) — "Already constructed" ×~30, "Initializing
  global FName variables", Kismet/CDO lookups, `[Seeds] slot 2 FName ctor …
  behaviorally verified`; GUObjectArray reads — ≥10000-element wait passed;
  StaticFindObject NoToString scans — every "Finding 'X' … located" line.
- **Pre-:528 evidence proves the ToString class UNPROVEN:** the init gate
  prints `[n: <unstable-int>]` instead of names and uses
  `…NoToStringFromStrings` helpers by explicit design (Z10's r4 caution lives
  here too); `[Seeds] Unbound by design: fname_tostring` forces any
  GetFullName into engine ProcessEvent — no console run has ever shown one
  succeeding. Hence rank 2.
- **`l24-diag.jsonl`** (28,413 events, 162 tags): vocabulary is
  module_start/seeds/copyin/bridge-only (per agent F); **zero events exist
  between "Adding shutdown listener" and the crash** — the window is
  unobservable in l24, so no evidence contradicts (or further orders) the
  ranked sites; the l25 phase tags (agent P) are what will make ranks 1-2
  observable. The `buffered_lua_result=0` events (and `l24-aligned.log`)
  prove the Lua mod machinery itself runs clean when the engine-callback
  surface is bypassed — supporting the claim that gated B can reach mod start.
- **The 1947 B settings**: `bUseUObjectArrayCache=false`,
  `HookProcessInternal=0`, `HookProcessLocalScriptFunction=0`,
  `HookEngineTick=true` (moot under R3 — registrations refused pre-:528, live
  lines 586-587), `ConsoleEnabled=0` — consistent with the table's "skipped"
  entries.

## 6. Bottom line

- **Risky sites (allocator-mediated engine coupling) on the :528→mod-start
  path: 8 live + 2 config-off.** Live: the rank-1 engine-array registration
  (`LuaMod.cpp:7995`) and the 7 GetFullName/Conv_NameToString sites
  (`LuaMod.cpp:5007/5012/5017/5050/5056/5099/5105`, one cluster). Config-off:
  `UnrealInitializer.cpp:523/:524` (would precede :528 if the cache were on).
- **FIRST thing B hits after the skip: `LuaMod.cpp:7995`** — the same
  AddUObjectDeleteListener→TArray Append→FMemory::Realloc→GMalloc mechanism,
  on the UE4SS-UpdateThread instead of the InitThread.
- **Recommended switch extensions:** G1 (gate `:7995`, mandatory), G2
  (literal-name logging at the 7 R3-consumer prints, strongly recommended),
  G3 (force `:523/:524` block off under B), G4 (optionally quiet `:547`).
  Nothing mod start provably needs is gated; the pump and main.lua stay live.
