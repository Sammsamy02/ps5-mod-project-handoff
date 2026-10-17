# IM-CHEATMANAGER-ANALYSIS — why F7 dies when CheatManagerEnablerMod is loaded

Agent IM, 2026-10-11. ANALYSIS ONLY (no builds, no console contact, read-only).
Build under analysis: the frozen l26-A lineage (PRX efbfc656, snapshot 24025929,
runtime object dbf6e6ae…; sources = `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/*`
over `outputs/2026-10-08-r5-source-integration/source/*`; selection verified in
`outputs/2026-10-08-l25-build-variants/l25-A/SOURCE-SELECTION.json` — the pinned
`UFunction.cpp` is the r5 ThunkDetail routing, NOT the port's direct-swap file).
Log evidence: `outputs/2026-10-08-l25-prep/evidence/launch2-final-session-20261011.decoded.log`
(failing, all mods) and `round4-spawn-session-20261011.decoded.log` (proven control).

All paths below are relative to `/WORKSPACE/.codex/worktrees/translation-core/what/`
unless absolute. "LuaMod.cpp" = `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp`.
"ScriptHookThunks.cpp" = same candidate dir. "UFunction.cpp" =
`outputs/2026-10-08-r5-source-integration/source/UFunction.cpp`.
"UFunctionStructs.cpp" = `outputs/2026-10-08-r5-source-integration/source/UFunctionStructs.cpp`.
"Handler.cpp/hpp", "UFunction.cpp(direct)", "UFunctionStructs.cpp(legacy)",
"UE4SSProgram.cpp(port)" = `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/…`.

---

## 0. THE HEADLINE (evidence first)

The launch-2 log localizes the failure to a single thread and a single lock:

```
16:27:34  [l22] keybind callback entry key=118 ref=4        <- F7 lambda STARTED (update thread)
          …and then NOTHING from the update thread, ever:
          no "[l22] ExecuteInGameThread queued", no error line, no further keybind entries
16:27:35…16:30:28  Lua StaticConstructObject widget builds continue   <- OTHER Lua paths alive
16:32:37  [CheatManagerEnabler] Constructed CheatManager [0x12000903F0] <- game-thread hook Lua alive 5+ min later
```
(launch2-final-session-20261011.decoded.log lines 2372 ff., 2360-2366, 2495)

The proven control (round-4, spawner+ueselftest+zzdiag, same binary family, same
[l22] instrumentation) shows what MUST follow the entry line within milliseconds:

```
08:48:20  [l22] keybind callback entry key=118 ref=4
08:48:20  [l22] ExecuteInGameThread queued method=2 state=0x88009bfa8 ref=12   <- immediate
08:48:20  [l22] ExecuteInGameThread drained … ref=12 (callback entry)          <- next carrier fire
08:48:20  [TownfallItemSpawner] 24 spawnable pickups found. Pages=3            <- menu builds
```
(round4-spawn-session-20261011.decoded.log lines 931-938 ff.)

Between the entry log and the missing queued log there are exactly two code
points in the keybind lambda (LuaMod.cpp:2205-2219):

1. `std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};` (LuaMod.cpp:2209) — an UNBOUNDED blocking wait.
2. `lua.registry().get_function_ref(...); lua.call_function(0,0);` (LuaMod.cpp:2212-2213) — runs the spawner's F7 Lua, whose body is literally `ExecuteInGameThread(toggleMenu)` (source-cache main.lua:694-695).

So the update thread either (a) blocked forever acquiring `m_thread_actions_mutex`,
or (b) entered the Lua call and never returned/completed the enqueue. Everything
below explains which threads can hold/entangle that mutex or that Lua state in
the three-mod config but not in the proven config.

---

## 1. THE REGISTERHOOK MECHANISM (what CheatManagerEnablerMod actually installs)

Mod source: `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods/CheatManagerEnablerMod/Scripts/main.lua`
— `RegisterHook("/Script/Engine.PlayerController:ClientRestart", function(self, NewPawn) … StaticConstructObject(CheatManagerClass, PlayerController) … PlayerController.CheatManager = CreatedCheatManager … end)` (main.lua:3-32).

Step by step, in the frozen build:

| # | Step | file:line |
|---|---|---|
| 1 | Lua `RegisterHook` binding entry; **takes `m_thread_actions_mutex`** | LuaMod.cpp:5807-5808 |
| 2 | Creates a dedicated Lua thread-state for the hook (`make_hook_state`, `lua_newthread` of the mod's main state) and refs the callback | LuaMod.cpp:5831-5838 |
| 3 | `StaticFindObject<UFunction*>("/Script/Engine.PlayerController:ClientRestart")` | LuaMod.cpp:5852 |
| 4 | Native branch selection: `GetFunc()` non-null, != ProcessInternal, FUNC_Native set | LuaMod.cpp:5863-5865 |
| 5 | Emplaces `LuaUnrealScriptFunctionData` (pre/post ids, Lua refs) into `g_hooked_script_function_data` | LuaMod.cpp:5867-5868; struct at 333-362 |
| 6 | `RegisterPreHook(&lua_unreal_script_function_hook_pre, …)` + `RegisterPostHook(&lua_unreal_script_function_hook_post, …)` | LuaMod.cpp:5869-5870 |
| 7 | → `UFunction::RegisterPreHook/RegisterPostHook` route to `ThunkDetail::Register` | UFunction.cpp:84-93 |
| 8 | `ThunkDetail::Register`: locks the registry recursive `mutex`; allocates a free `Slot` (capacity 256, ScriptHookThunks.hpp:12); stages an `UnrealScriptFunctionData` holding the ORIGINAL FuncPtr; commits it into the shared `HookedUFunctionMap`; marks slot Active | ScriptHookThunks.cpp:124-154 |
| 9 | **THE FUNC SWAP**: `function->SetFuncPtr(thunks[index])` — one 8-byte store replacing ClientRestart's `Func` with the thunk | ScriptHookThunks.cpp:155; `SetFuncPtr` at UFunction.cpp(direct):33-36 |

At invocation time (engine calls ClientRestart on the game thread):

| # | Step | file:line |
|---|---|---|
| 10 | `Thunk<Index>` → `Dispatch` | ScriptHookThunks.cpp:82-86, 48 |
| 11 | `Dispatch` takes the registry `mutex`, validates the slot, `++active_calls`, `bIsMidExecution=true`, then RELEASES the mutex | ScriptHookThunks.cpp:53-66 |
| 12 | `FirePreCallbacks` → `lua_unreal_script_function_hook_pre` — marshals Context + NewPawn into RemoteUnrealParams and calls the Lua callback. **No mutex is held during this Lua execution.** | ScriptHookThunks.cpp:75; UFunctionStructs.cpp:83-110; LuaMod.cpp:364-465 (Lua call at 456) |
| 13 | The CheatManagerEnabler Lua body runs synchronously: property reads (`CheatManager`, `CheatClass`), `StaticFindObject`, `StaticConstructObject` (LuaMod.cpp:2599 → UObjectGlobals.cpp:142-166 direct engine call), and the `PlayerController.CheatManager = …` property write | main.lua:4-31 |
| 14 | Original ClientRestart thunk executes | ScriptHookThunks.cpp:77 |
| 15 | `FirePostCallbacks` → `lua_unreal_script_function_hook_post` (return-value handling; hook removal if scheduled) | ScriptHookThunks.cpp:78; LuaMod.cpp:468-508 |
| 16 | `~DispatchLifetime`: re-takes registry mutex, `--active_calls`, clears `bIsMidExecution` | ScriptHookThunks.cpp:36-45 |

Live confirmation in the failing log: hook fired at 16:27:03 ("Constructed
CheatManager [0x124025D020]", "Enabled CheatManager") and again at 16:32:37
(lines 859-867, 2495) — a fresh PlayerController at each restart gets a fresh
CheatManager; the hook survives the whole session.

Note the borrowed-frame design: `GNatives_Internal` must be null for callbacks
to fire (ScriptHookThunks.cpp:63,128) and registration throws otherwise. Both
mod hooks and the R3 carriers ride this same thunk registry — the launch-2 log
line 781 additionally proves the StaticConstructObject/LoadMap/ProcessConsoleExec
DETOURS are not installed in this world, so the LuaMod.cpp:8833 StaticConstructObject
post-callback (and its `m_thread_actions_mutex` take at 8838) never fires on
console. That path is dead — see §3b.

---

## 2. THE KEYBIND DISPATCH MECHANISM (pad press → Lua)

Full chain with file:line:

| # | Step | file:line |
|---|---|---|
| 1 | UE4SS event loop thread (`m_event_loop` jthread → `UE4SSProgram::update`, named "UE4SS-UpdateThread") runs a 5 ms cadence loop; `on_program_start` (which started all mods) ran earlier ON THIS SAME THREAD | candidate UE4SSProgram.cpp:732, 1668-1689, 1869 |
| 2 | `m_input_handler.process_event()` each iteration | candidate UE4SSProgram.cpp:1847 |
| 3 | `Handler::process_event` → `m_platform_handler->process_event(this)` → `PS5PadSource::poll(subscribed, allow)` | Handler.cpp:66-75; PS5PadSource.cpp:95-98 |
| 4 | `scePadReadState` via backend fn-pointer; rising-edge detection; **`allow` gate** (`if (!baseline && down && !m_down[i] && allow && subscribed[b.key])`) | PS5PadSource.cpp:130-158 (gate at 155) |
| 5 | `if (!m_allow_input) return;` then callback snapshot under `m_event_mutex`, callbacks invoked outside the lock | Handler.cpp:76, 78-102 |
| 6 | The keybind callback closure registered by `RegisterKeyBind` → `lua_keybind_callback_lambda`: logs `[l22] keybind callback entry`, then **acquires `m_thread_actions_mutex`** and calls the mod's Lua directly ON THE UPDATE THREAD | LuaMod.cpp:2205-2219 (lock at 2209), registered at 2235-2240 |
| 7 | Spawner F7 Lua calls `ExecuteInGameThread(toggleMenu)` | source-cache main.lua:694-695 |
| 8 | `ExecuteInGameThread`: under PROSPERO marshal-first (`funcptr_marshal_usable()` at 5968), then **takes `m_thread_actions_mutex`** (re-entrant, same thread), `make_hook_state`, refs closure, pushes onto `m_game_thread_actions` (or `_pending_` if mid-drain), logs `[l22] ExecuteInGameThread queued` | LuaMod.cpp:5938-5971, 6075-6114 |
| 9 | GAME THREAD: engine ticks a carrier UFunction (a Blueprint override of Actor:ReceiveTick / AnimInstance:BlueprintUpdateAnimation / UserWidget:Tick — 66 armed in launch-2, log lines 899-906) → carrier's thunk `Dispatch` → `FirePostCallbacks` | carrier family: LuaMod.cpp:5622-5683; arm at 5587-5620 |
| 10 | `funcptr_marshal_hook`: NativeNameThread ticket (5282), one-shot identity publication (5293-5308), `IsInGameThreadRaw` gate (5311), then **`NativeLuaActionDispatchScope`** — whose member `std::unique_lock Lock{m_thread_actions_mutex}` (4451) holds the mutex for the ENTIRE drain — then `try_deferred_loadmap_arm` (5338), `process_simple_actions` (5355), `process_delayed_actions<kFuncPtrMarshalExecutor>` (5356), pending-Notify (5358), requeue (5360-5379) | LuaMod.cpp:5264-5380; scope class 4446-4473 |
| 11 | `process_simple_actions` moves the queue out under the mutex, then calls the closure Lua (mutex still held by the scope in the caller frame); logs `[l22] ExecuteInGameThread drained … (callback entry)` | LuaMod.cpp:4557-4604 (move 4560-4568, Lua call 4580-4591) |
| 12 | `toggleMenu` runs on the game thread → FindFirstOf/PlayerController → buildUI (StaticConstructObject per widget, visible in logs) | source-cache main.lua:488-643 |

Thread model summary: update thread = steps 1-8; game thread = steps 9-12 plus
the ClientRestart thunk of §1; per-mod async thread = LoopAsync processing
(`LuaMod::update_async` → `process_delayed_actions`, LuaMod.cpp:9535-9614 —
queue move under the mutex at 9562-9567 but the async Lua call itself 9585-9595
runs WITHOUT the mutex); RegisterHook/UnregisterHook callbacks (§1 step 12-15)
run game-thread with NO mutex.

---

## 3. INTERSECTION POINTS (the four questions)

### 3a. Does the UFunction hook's callback execution hold a mutex that the keybind dispatch needs?

Not the registry mutex: `Dispatch` releases it before firing callbacks
(ScriptHookThunks.cpp:54-66), so ClientRestart's Lua never blocks the carriers.

BUT two Lua-callable functions inside hook callbacks DO take exactly the mutex
the keybind lambda needs (`m_thread_actions_mutex`):

- `UnregisterHook` — LuaMod.cpp:2323-2324 — and **ConsoleEnablerMod calls it from
  INSIDE its ClientRestart pre-callback** (ConsoleEnablerMod main.lua:86), i.e.
  the GAME THREAD acquires the keybind mutex mid-RPC, then runs a full
  `StaticFindObject` while holding it (LuaMod.cpp:2341).
- `RegisterHook` — LuaMod.cpp:5807-5808 (startup-time only, but same lock).

Conversely the marshal drain (`NativeLuaActionDispatchScope`, LuaMod.cpp:4451 +
5331) holds `m_thread_actions_mutex` for the whole drain INCLUDING all drain
Lua. So the three-way contention set on that one recursive mutex is:
{update-thread keybind lambda (2209), game-thread drain scope (4451/5331),
game-thread UnregisterHook-in-hook (2324), startup RegisterHook (5808),
ExecuteInGameThread enqueue (6077), async queue move (9562)}. The proven config
exercises only {keybind, drain, enqueue}; the three-mod config adds the
UnregisterHook-in-hook entry on the game thread and continuous async/queue churn.

Additionally, the hook-callback Lua itself (§1 step 12; `lua_unreal_script_function_hook_pre`
LuaMod.cpp:364-465) executes with NO serialization at all — neither the
thread-actions mutex nor the dispatch scope. It is the only Lua in the system
that can run on the game thread concurrent with update-thread keybind Lua and
async-thread LoopAsync Lua. Per-mod lua_States differ, so direct Lua-state
corruption across mods is not the issue; the shared C++ seams (property pusher
tables, FName bridge, output device) are the exposure surface.

### 3b. Does the CheatManager's StaticConstructObject (inside the hook callback) block the game thread?

It runs synchronously inside the ClientRestart thunk (game thread): LuaMod.cpp:2599
→ `Unreal::UObjectGlobals::StaticConstructObject` → direct engine
`StaticConstructObjectInternal` call (UObjectGlobals.cpp:142-166 in the
candidate). It blocks ClientRestart for its duration and runs the engine's
GUObjectArray create-listener chain — including OUR registered listeners
(UnrealInitializer.cpp:603-609), the same engine-side listener family that
hard-stalled init in l24 (mission context). Launch-2 evidence shows it
completing quickly (16:27:03 and 16:32:37 both return and log), so a hard hang
is NOT the observed failure mode — but it is the wrong place to do heavy
construction, and the port's LuaMod.cpp:8833 StaticConstructObject post-callback
(whose `m_thread_actions_mutex` take at 8838 WOULD be a direct keybind blocker)
is provably inert here because its detour is refused in the marshal world
(launch-2 log line 781 lists StaticConstructObject hooks as unregistered).

### 3c. Does setting PlayerController.CheatManager change the engine's input routing?

Not for THIS keybind chain. The PS5 pad path reads the controller directly via
`scePadReadState` (PS5PadSource.cpp:131 through the backend fn-pointer); it
never traverses the engine's UInputComponent/UConsole pipeline where a
UCheatManager lives (cheat-command interception in the ENGINE console path).
The BISECT-ANALYSIS.md line "a CheatManager takes over console command
processing and can intercept/redirect input handling" is upstream-PC lore that
does not apply to our direct-pad design. The only engine-input coupling in the
tree — the console-open → `set_allow_input(false)` block — is commented out
(see §4). **Mechanism 3c is exonerated.**

### 3d. Does the hook's pre/post mechanism interfere with the R3 carrier dispatch?

Not on the same UFunction: the carriers are Blueprint overrides of
ReceiveTick/BlueprintUpdateAnimation/UserWidget:Tick (LuaMod.cpp:5624-5627),
ClientRestart is a different UFunction on a different thunk slot; installation
vs dispatch is serialized by the registry mutex (ScriptHookThunks.cpp:54,125),
capacity is 256 (ScriptHookThunks.hpp:12; launch-2 armed 66 carriers + 2 mod
hooks — no exhaustion). The interference is INDIRECT, through the shared
`m_thread_actions_mutex` (§3a) and through unserialized hook-Lua (§3a last
paragraph). One historical precedent is carved into the source: the [FL-MAP]
removal note (LuaMod.cpp:5339-5348) documents that running a 40K-object engine
scan INSIDE `funcptr_marshal_hook` produced exactly this failure signature
("F7 keybind entries with ZERO Lua output and ZERO ExecuteInGameThread queues")
— the drain path remains sensitive to heavy/reentrant work, and
`r3_marshal_lazy_arm` (LuaMod.cpp:5734-5755, called from `ExecuteInGameThread`
via `funcptr_marshal_usable` at 4530-4544/5968) can still run a full
ForEachUObject carrier rescan (5646) from the UPDATE thread whenever armed
carriers have not fired within 1 s (e.g. any ExecuteInGameThread issued between
program start and the first carrier fire — ConsoleEnablerMod's startup
`ExecuteInGameThread(CreateConsole)` main.lua:74-76 is exactly such a call).

---

## 4. THE ALLOW_INPUT QUESTION — resolved

- Default: `bool m_allow_input{true};` — Handler.hpp:50.
- Read gates: Handler.cpp:76 (`if (!m_allow_input) return;`) and the `allow`
  parameter threaded into PS5PadSource::poll (Handler.cpp:75 → PS5PadSource.cpp:97,
  edge gate at 155).
- The ONLY `set_allow_input` call site in the tree is inside the commented-out
  console-state block — candidate UE4SSProgram.cpp:1825-1845 (call at 1840);
  identical commented block in port UE4SSProgram.cpp:1638-1658 (call at 1653),
  with the upstream note "doesn't work properly".
- No other writer exists (grep over the whole port tree: Handler.cpp:197-200 is
  the trivial setter; nothing else calls it).

**Verdict: allow_input is true for the entire session in every config.** The
keybind ENTRY lines in both failing logs (launch-2 line 2372; the round-4
all-mods session's reported entries) independently prove pad + handler +
allow were live. The round-4 "ZERO entries" observation is therefore an
update-thread-liveness question (loop dead/blocked or harvest mid-session),
not an allow_input question. This closes BISECT-ANALYSIS.md's "NEXT DIAGNOSTIC"
item — no code change needed for allow_input.

---

## 5. THE FIX (ready regardless of which mod the bisect names)

### 5.1 If CheatManagerEnablerMod is identified (bisect step 1)

Root cause class: heavy engine construction + property writes + (via CE sharing
the same hook) mutex-taking calls executed inside the ClientRestart thunk with
no serialization, colliding with the keybind lambda's `m_thread_actions_mutex`
acquire and/or corrupting the update thread's in-flight Lua.

Minimal fix, in order of preference:

- **(a) Mod-level (no engine change, deployable as a Lua-only edit like the
  earlier main.lua instrumentation): make the hook body queue instead of
  construct.** Replace the direct `StaticConstructObject` + assignment inside
  the RegisterHook callback with `ExecuteInGameThread(function() … end)` —
  the construction then runs inside the marshal drain under the dispatch
  scope (serialized, on the proven pump) instead of mid-RPC. This is the same
  delivery pattern CE's entry point already uses (ConsoleEnablerMod
  main.lua:74-76) and it also stops the per-restart reconstruction churn seen
  at 16:32:37. 1-line-diff class change to CheatManagerEnablerMod/Scripts/main.lua.
- **(b) Engine-side hardening (small, local): take `m_thread_actions_mutex`
  around the hook-callback Lua dispatch** in `lua_unreal_script_function_hook_pre`
  (LuaMod.cpp:364, wrap the `TRY` body ~366-465) and `…_hook_post` (468+),
  mirroring the keybind lambda's discipline (2209). The mutex is recursive, so
  a hook that fires while the same thread already holds it (re-entrant carrier
  during drain) is safe; this closes the "unserialized game-thread Lua" class
  for every future RegisterHook mod, not just CM.
- Do NOT pursue "don't set PlayerController.CheatManager directly" — §3c shows
  the assignment itself is input-neutral in this port, and the property write
  worked (logged) in both failing sessions.
- "Delay the hook registration to after the first keybind test" (option a of
  the task) is NOT recommended: registration itself is startup-thread-only
  and harmless (§1 steps 1-9); the damage is in the callback body, which (a)
  fixes at the right place.

### 5.2 If ConsoleEnablerMod is identified (bisect step 2)

Two live mechanisms, both fixable at mod level:

- **The in-callback `UnregisterHook`** (ConsoleEnablerMod main.lua:86) takes
  `m_thread_actions_mutex` ON THE GAME THREAD inside the ClientRestart thunk
  (LuaMod.cpp:2324) and then StaticFindObjects under it (2341). Fix: drop the
  self-unregister (the hook is idempotent — `WasConsoleCreated` already
  guards), or defer it through `ExecuteInGameThread` like everything else.
- **The startup `ExecuteInGameThread(CreateConsole)`** (main.lua:74-76) triggers
  `r3_marshal_lazy_arm`'s ForEachUObject rescan from the update thread in the
  pre-first-carrier-fire window (LuaMod.cpp:5734-5755 → 5646, entered via
  4542/5968) — the same 40K-scan-during-load class the [FL-MAP] fix removed
  from the drain (5339-5348). Engine-side fix if needed: restrict the rescan
  to the published game thread (queue a rescan request that the drain itself
  executes), or extend the `last_fire==0` early-out to skip rescans before the
  first carrier fire.

### 5.3 If the TArray write is identified (task item 6 — the ConsoleKeys growth analysis)

The growth path, precisely: `ConsoleKeys[#ConsoleKeys + 1].KeyName = KeyName`
(ConsoleEnablerMod main.lua:36) executes as:
1. `__Length` → `FScriptArray::Num()` (LuaTArray.cpp:69-73),
2. `__Index` at N+1 → `TArray::prepare_to_handle` (LuaTArray.cpp:232-252):
   out-of-bounds detected at 244, growth = **`AddZeroed(count, inner->GetElementSize(), Unreal::DEFAULT_ALIGNMENT)`**
   at 248 — a direct engine-heap realloc of the CDO's `TArray<FKey>`
   (Default__InputSettings), from whatever thread the Lua runs on (game thread
   inside the ClientRestart hook at 16:27:03, drain thread at 16:27:17 — both
   observed in launch-2, lines 873-886 and 928-930),
3. element pusher (Get) wraps element N (214-222), then the `.KeyName` FName
   write through the property pusher Set path.

Failure modes specific to this port: (i) the realloc runs engine GMalloc from a
non-owner context if invoked off-thread; (ii) `AddZeroed`'s slack/`ArrayMax`
arithmetic uses the port's FScriptArray layout — a mismatched `ArrayNum`/
`ArrayMax` offset writes past the array header into adjacent UInputSettings
members (the "corrupts adjacent memory" theory in BISECT-ANALYSIS.md); (iii)
the zeroed FKey then takes an FName store through the wide-string seam. The
host tests pass because the FU harness stubs the growth metamethods
(BISECT-ANALYSIS.md "Why host tests pass but console fails").
Fix options: pre-size once via a single `AddZeroed` on the game-thread drain
(move RemapConsoleKeys wholly into the ExecuteInGameThread closure — CE is
already halfway there), verify the pinned FScriptArray member offsets for the
target engine before shipping the write, or bypass growth by assigning into an
existing entry.

### 5.4 If InfiniteStamina is identified

Its LoopAsync runs the mod Lua on the ASYNC thread with NO mutex around the
Lua call (LuaMod.cpp:9585-9595) and does FindFirstOf-class engine scans
(UEHelpers.GetPlayer). Fix: same discipline as 5.1(b) — serialize async-callback
Lua with `m_thread_actions_mutex`, or route its engine reads through
ExecuteInGameThread closures.

### 5.5 Universal hardening (recommended regardless of bisect outcome)

1. Serialize ALL Lua entry points with `m_thread_actions_mutex` (hook pre/post
   at LuaMod.cpp:364/468; async delayed-action call at 9585). This makes the
   mutex the single Lua lock — matching the design comment at 4444-4446.
2. Bound or thread-restrict the lazy carrier rescan (5734-5755).
3. Keep heavy engine mutation out of UFunction hook callbacks (docs rule for
   future mods; CM/CE both violate it today).

---

## 6. WHAT THE STAGED BISECT WILL DISCRIMINATE

- Step 1 (+CM): if F7 fails → §5.1 (fix (a) first, (b) as hardening).
- Step 2 (+CE): if F7 fails → §5.2/§5.3 (unregister-in-hook + TArray growth).
- Step 3 (+Stamina): if F7 fails → §5.4.
- Step 4 (CM+CE pair): shared-ClientRestart-slot interaction — both mods'
  callbacks share ONE thunk slot/UnrealScriptFunctionData (ThunkDetail::Register
  add-path at ScriptHookThunks.cpp:134-141); CE's UnregisterHook marks only its
  own ids, so CM's hook survives (benign), but the pair doubles the mid-RPC
  engine work per restart.
- If ALL individually clean: the interaction is the mutex triangle of §3a plus
  queue churn — apply §5.5.

Prediction from this analysis (falsifiable by step 1): CM alone reproduces the
launch-2 signature (entry logged, no queued line) only if some concurrent
no-mutex Lua or mutex-holding hook path is live at the press; since CM alone
adds only the no-mutex hook Lua and no queue churn, **CE is the more likely
sole reproducer** (it contributes BOTH the mutex-taking UnregisterHook-in-hook
AND the startup-ExecuteInGameThread-triggered rescan AND the TArray growth).
If step 1 passes and step 2 fails, §5.2(a)+§5.3 apply with no engine rebuild
(Lua-only edits to ConsoleEnablerMod/main.lua).

## 7. SOURCES CONSULTED

- `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp` (9650 lines — hooks, keybinds, marshal, drains, ExecuteInGameThread, StaticConstructObject/UnregisterHook/LoopAsync bindings, post-callback at 8833)
- `outputs/2026-10-08-r5-source-integration/source/UFunction.cpp`, `ScriptHookThunks.cpp/.hpp`, `UFunctionStructs.cpp`
- `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/src/Input/Handler.cpp`, `include/Input/Handler.hpp`, `outputs/2026-10-08-menu-path-audit/source-cache/PS5PadSource.cpp`
- `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UE4SSProgram.cpp` (event loop, commented console gate at 1825-1845), `UnrealInitializer.cpp` (listeners 603-629, GNatives 1972-1975), `UObjectGlobals.cpp`
- `.superpowers/…/assets/Mods/CheatManagerEnablerMod/Scripts/main.lua`, `ConsoleEnablerMod/Scripts/main.lua`, `outputs/2026-10-08-menu-path-audit/source-cache/main.lua` (spawner)
- `outputs/2026-10-08-l25-prep/evidence/launch2-final-session-20261011.decoded.log`, `round4-spawn-session-20261011.decoded.log`
- `outputs/2026-10-08-l25-prep/BISECT-ANALYSIS.md`, `CLAUDE-BISECT-STATUS.md`, `CLAUDE-MENU-UP-SPAWN-BLOCKED.md`, `FROZEN-KNOWN-GOOD-20261010.md`, `outputs/2026-10-08-menu-path-audit/REPORT.md`
- Frozen-build provenance: `outputs/2026-10-08-l26-package-A/FINAL-RECEIPT-VARIANT-A.json`, `outputs/2026-10-08-l25-build-variants/l25-A/SOURCE-SELECTION.json`, PRX strings (thunk markers verified in the frozen PRX).
