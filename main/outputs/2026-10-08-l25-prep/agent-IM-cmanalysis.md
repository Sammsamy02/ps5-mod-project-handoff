# agent-IM-cmanalysis — compact summary (2026-10-11)

Full analysis with file:line citations: `outputs/2026-10-08-l25-prep/IM-CHEATMANAGER-ANALYSIS.md`.

## The mechanism, in one paragraph

The F7 chain is: update thread polls the pad (`UE4SSProgram::update` →
`Handler::process_event` → `PS5PadSource::poll`), invokes the keybind lambda
which logs `[l22] keybind callback entry`, then BLOCKS acquiring
`LuaMod::m_thread_actions_mutex` (LuaMod.cpp:2209), runs the spawner Lua which
calls `ExecuteInGameThread(toggleMenu)` (queued under the same mutex,
LuaMod.cpp:6077-6108), and the game-thread R3 carrier thunk
(`funcptr_marshal_hook`, LuaMod.cpp:5264) drains it while holding that same
mutex for the entire drain via `NativeLuaActionDispatchScope` (LuaMod.cpp:4451,
5331). RegisterHook on ClientRestart (CheatManagerEnablerMod) swaps the
UFunction's Func to a registry thunk (ScriptHookThunks.cpp:124-163, swap at
155) and its Lua callback executes on the game thread mid-RPC with NO mutex
(LuaMod.cpp:364-465) — constructing a CheatManager and writing engine state
inside the thunk. The launch-2 log proves the failure point: F7 entry logged at
16:27:34, then ZERO `ExecuteInGameThread queued` lines ever, while game-thread
hook/drain Lua kept working for 5+ more minutes (a second CheatManager was
constructed at 16:32:37). The proven control log shows `queued` following the
entry within milliseconds. The update thread therefore died/blocked between
the entry log and the enqueue — the only waits there are the mutex acquire and
the Lua call on the spawner state. allow_input is exonerated: it defaults true
(Handler.hpp:50) and its only setter call site is inside the commented-out
console block (candidate UE4SSProgram.cpp:1825-1845).

## Key facts

1. Frozen build uses the ThunkDetail registry (r5 UFunction.cpp:84-93 routes
   RegisterPreHook/PostHook; verified in pinned SOURCE-SELECTION.json and PRX
   strings). Registry mutex is released before callback fire — no direct
   carrier deadlock from CM's hook.
2. Setting PlayerController.CheatManager does NOT affect our input routing —
   the pad path reads scePadReadState directly and never touches the engine's
   console/cheat pipeline. Upstream "CheatManager intercepts input" lore does
   not apply (BISECT-ANALYSIS theory 1b is wrong for this port).
3. The real intersections on ONE recursive mutex (`m_thread_actions_mutex`):
   keybind lambda (2209), drain scope (4451/5331), ExecuteInGameThread enqueue
   (6077), RegisterHook (5808), UnregisterHook (2324) — and ConsoleEnablerMod
   calls UnregisterHook from INSIDE its ClientRestart hook (its main.lua:86),
   putting the game thread on the keybind's mutex mid-RPC. The hook-callback
   Lua itself (and Stamina's async LoopAsync Lua, 9585-9595) run entirely
   unserialized.
4. CE's startup `ExecuteInGameThread(CreateConsole)` can trigger the lazy
   40K-object ForEachUObject carrier rescan from the update thread before the
   first carrier fire (LuaMod.cpp:5734-5755 → 5646) — the same scan class the
   [FL-MAP] fix removed from the drain after it produced this exact failure
   signature (comment at LuaMod.cpp:5339-5348).
5. ConsoleKeys TArray growth = `TArray::prepare_to_handle` → `AddZeroed`
   (LuaTArray.cpp:244-248), engine-heap realloc of the InputSettings CDO from
   whichever thread the Lua runs on; observed executing both from the hook
   (16:27:03) and the drain (16:27:17) in launch-2.

## Recommended fix (ready regardless of bisect outcome)

- Primary, Lua-only, no rebuild: move ALL work out of ClientRestart hook
  bodies — CheatManagerEnabler's construction and ConsoleEnabler's
  CreateConsole/RemapConsoleKeys/UnregisterHook should queue via
  `ExecuteInGameThread(...)` (CE's entry point already does this; its hook and
  CM's hook should too). This removes mid-RPC engine mutation AND the
  game-thread mutex acquisition from the thunk path.
- Engine hardening (small diff): hold `m_thread_actions_mutex` around the
  hook-callback Lua dispatch in `lua_unreal_script_function_hook_pre/post`
  (LuaMod.cpp:364/468) and the async LoopAsync call (9585) — one lock for all
  Lua, matching the design comment at 4444-4446. Recursive mutex makes
  re-entrant same-thread cases safe.
- If the bisect names CE specifically: also drop the in-callback
  UnregisterHook (idempotent guard already exists) and keep RemapConsoleKeys
  inside the game-thread closure only.
- If nothing is individually guilty: apply the mutex-unification hardening and
  restrict the lazy carrier rescan to the published game thread.

Prediction (falsifiable at bisect step 1): CM alone is less likely to reproduce
than CE — CM adds only unserialized hook Lua, while CE contributes the
mutex-taking in-hook UnregisterHook + the startup-triggered rescan + the TArray
growth write. If step 1 passes and step 2 fails, the Lua-only CE edit fixes it
without an engine rebuild.
