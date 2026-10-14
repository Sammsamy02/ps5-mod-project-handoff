VERDICT: FIX FIRST

# B6 review (final-b6.diff against candidate B5). Mac-only: source reading plus the B5 launch-2 decoded log. No console contact, no builds.

Bottom line: the main fix, [R9-MEM], does nothing for the mods that are loaded when the game starts. Every mod's Lua state is created before the GMalloc backend is installed, so all of them stay on the libc heap that ran out in launch 2. The other parts (EIGT guard, REFUSED, cxx get_function_ref, rate limit) are safe to ship, apart from the smaller issues below.

## Findings

### 1. BLOCKER: [R9-MEM] is installed after every mod Lua state already exists, so mod states stay on libc
- Each mod's Lua state is created in the LuaMod constructor: `src/LuaMod.cpp:1278` `, m_lua(LuaMadeSimple::new_state())`. `m_lua` is a reference member (`LuaMod.hpp:38 LuaMadeSimple::Lua& m_lua;`), so it cannot be swapped later.
- LuaMod objects are built in `setup_mods()` (`src/UE4SSProgram.cpp:2105-2107`, native-source path). On PS5, `setup_mods()` is called from `start_native_mods()` (`src/UE4SSProgram.cpp:765`). That is the one-shot step that runs "before UE4SS initialization".
- `r9_install_lua_backing_allocator()` runs much later, at `src/UE4SSProgram.cpp:1751`, just before `install_lua_mods()`.
- Order in the B5 launch-2 log (`evidence/launch-l26-B5-launch2-20261012/decoded/ue4ss.decoded.log`): line 21 `Setting up mods...` (all mod states created here) -> line 518 `[Seeds] slot 3 gmalloc bound` -> line 659 `Post-initialization: GMalloc` -> line 772 `install_lua_mods result`. The code comment at UE4SSProgram.cpp:1748-1750 cites 518/659/772 but misses line 21.
- `new_state()` picks its backend once, at creation (`src/LuaMadeSimple/LuaMadeSimple.cpp:1719-1720`). `lua_newthread` children (main_lua, async_lua, hook threads, LuaMod.cpp:1367/1414/1440) inherit the parent's allocator. So every mod's memory stays on libc.
- Only states created after install use GMalloc: the console executor (LuaMod.cpp:8828) and hot-reloaded mods (UE4SSProgram.cpp:1896/2716/2923).
- Result: the next launch would log `[MEM] lua-backend=gmalloc ... probe=ok` and `be=gmalloc`, but `be_states` stays at about 0-1, `libc_states` equals the mod count, and `be_kb` is about 0. The launch-2 crash path (libc heap exhausted) is unchanged.
- Suggested fix (generic; nothing game-specific):
  1. Add a function to LuaMadeSimple.cpp, plus a declaration in LuaMadeSimple.hpp, that moves a still-empty libc state onto the installed backend:
     ```cpp
     // returns true when the state now lives on the backing store
     RC_LMS_API auto rebase_pristine_state_on_backing(Lua& lua) noexcept -> bool;
     // impl: old = lua.m_lua_state; lua_getallocf(old,&ud); want = backing_for_new_state();
     //   if (!want || ud == want || lua_gettop(old) != 0) return ud == want;
     //   fresh = lua_newstate(lms_counting_alloc, want); if (!fresh) return false;  // keep libc state
     //   lua_atpanic(fresh,&lms_lua_panic); lua_setwarnf(fresh,lms_warnfoff,fresh);
     //   auto node = lua_instances.extract(old); node.key() = fresh; lua.m_lua_state = fresh;
     //   lua_instances.insert(std::move(node)); lua_close(old);   // old blocks freed by libc (its own ud)
     //   g_libc_states.fetch_sub(1); g_backing_states.fetch_add(1); return true;
     ```
     `Lua::m_lua_state` is private (hpp:100), so this function needs friend access.
  2. Call it at the top of `LuaMod::start_mod()` (LuaMod.cpp:8537), before `prepare_mod(lua())`, under `#ifdef __PROSPERO__`, wrapped in try/catch. The state is empty until then: `install_mods<LuaMod>` (UE4SSProgram.cpp:2157-2193) only sets flags and never touches Lua.
  3. Log one line per mod, e.g. `[MEM] mod=<name> backend=gmalloc|libc`. After `start_lua_mods()`, add a check that warns if `be=gmalloc` while `libc_states > 0` (console executor excluded), so a regression shows up in the first log.
  - Alternative: create `m_lua` lazily in `start_mod` (header change to LuaMod.hpp; larger).

### 2. MAJOR: [R9-EIGT] clears the flag, but an escaping exception still drops queued work and leaves running delayed/loop actions stuck forever
- The guard itself is correct. It is only built after `if (m_is_currently_executing_game_action) defer/continue` (LuaMod.cpp:4928, 5149/5161), so the flag is always false at construction. Resetting it to false on exit restores the previous value, nesting is fine, and the normal path behaves as before.
- But the escaping exception (std::bad_alloc thrown by TRY's own "Error:" line) still leaves `process_simple_actions` / the delayed loop:
  - process_simple_actions: the remaining `to_run` items (already moved out of `actions`, 4920-4921) and everything in `deferred` are destroyed without running. Callbacks are lost silently and their registry refs leak.
  - Delayed loop (5165-5171): the post-loop block that resets `action.status` from `Executing` (from 5173 on) is skipped. Every action in `ready` keeps `status == Executing`, and line 5128 then skips it forever. A `LoopInGameThread*` action (minimap/UI tickers) stops for good, with no log line.
- Fix: catch inside each iteration, so one bad action cannot take the queue with it:
  ```cpp
  { const GameActionExecutingGuard g{};
    try { TRY([&]{ ... }); } catch (...) { L22Diagnostics::observe([]{ Output::send<LogLevel::Error>(STR("[R9-EIGT] action threw past TRY; continuing\n")); }); }
    luaL_unref(...);   // simple-actions site: also runs on the exception path
  }
  ```
  Do the same at 5165-5171, so the status-reset block always runs.

### 3. MINOR: the two guarded sites still use the longjmp-raising get_function_ref
- LuaMod.cpp:4937 and :5169 still call `registry().get_function_ref(...)`. Item 4's own reasoning applies here: if the pump runs inside a Lua pcall on the same thread (a hook fired from a Lua-started engine call, with the flag still false), a non-function ref causes `throw_error` to longjmp over `GameActionExecutingGuard`. Its destructor never runs, and the flag stays true, which is the exact failure EIGT is meant to remove. Rare (needs a stale ref), but the fix costs nothing.
- Fix: use `get_function_ref_cxx(*lua_data.lua, ...)` and `get_function_ref_cxx(*exec.lua, ...)`. TRY already catches std::exception.

### 4. MINOR: REFUSED mod_name_of() can hit undefined behaviour or a Lua error, and is not "never throws"
- LuaMod.cpp:422-425: if `ModRef` is not nil it calls `get_userdata<LuaModRef>(-1)` without checking the type. If the value is not a full userdata, `lua_getiuservalue` (inside get_userdata, hpp:774) reads garbage (api_check is compiled out). If the user value is unexpected, get_userdata throws `std::runtime_error` and leaves a traceback on the stack (hpp:798-803). `lua_getglobal` can also raise a Lua error, either through an `_G.__index` "strict" metamethod when ModRef is missing (non-mod states such as the console executor) or on out-of-memory. `emit()` builds `StringType wide` (line 408) outside `observe`, so bad_alloc can escape.
- Effect: a Lua error raised in `accepted(...)` (normal-return path) after the registration already happened. No lock is held at any call site (checked), so this does not deadlock.
- Fix: `if (lua_getglobal(L,"ModRef") == LUA_TUSERDATA) { ... }`. Wrap the body of `mod_name_of` and all of `emit` in try/catch(...) and fall back to "?". Use `lua_rawget` on the globals table (`lua_pushglobaltable` + `lua_getfield` raw) so no metamethod runs.

### 5. MINOR: REFUSED counts are off in both directions (GetRefusedHookCount feeds the self-test GAPS line)
- Under-count: `Family::CustomEvent` (LuaMod.cpp:445-450) only checks `ProcessLocalScriptFunctionInternal.is_ready()`. In the R3 world, though, the script hook is registered only inside the skipped eager block (LuaMod.cpp:9931-9941, "end of the eager detour-arm block"). So RegisterCustomEvent is accepted, never delivered, and never logged. The same applies to RegisterHook's BP-script path (`m_script_hook_callbacks`, LuaMod.cpp:6239-6255), which is delivered by that same unregistered script_hook. Fix: CustomEvent returns "detour-block-skipped-r3-marshal" when `eager_detour_block_skipped` is set. Add `refused_log::accepted(lua,"RegisterHook",function_name_no_prefix,Family::CustomEvent)` on the script-hook branch.
- Over-count: LoadMap registrations made while `loadmap_state==1` are counted as refusals ("queued-pending", line 466) even though they are delivered once the deferred arm succeeds (state 2). Fix: do not add `queued-pending` to `total` (log it only), or subtract once the state reaches 2.

### 6. MINOR (note): GMalloc Realloc does not return null on out-of-memory
- In UE allocators (Binned2/3), `Realloc` calls `OnOutOfMemory` -> fatal rather than returning nullptr. So `be_fails` will almost always read 0, and an engine out-of-memory now ends the game with an engine fatal instead of a Lua LUA_ERRMEM. Acceptable (launch 2 crashed anyway), but do not read `be_fails=0` as "no pressure". TryRealloc (slot 72) has weaker provenance ("DEFAULT_SOURCE_ASSIGNED_EXECUTABLE_IMAGE_ENTRY", one source), so do not switch to it under the two-source rule.
- Related: the `Error: std::ba` lines in launch 2 (decoded:3046+) are C++ `operator new` failures, and C++ new/delete still use the libc heap after B6. Moving Lua off libc only helps if Lua was the main consumer. Watch live_kb against the bad_alloc timing on the next launch.

## Checked and OK
- lua_Alloc contract on the backing path (LuaMadeSimple.cpp:383-422): free when nsize==0 (ptr may be NULL); realloc(NULL,n) acts as malloc via UE Realloc; osize is a type tag when ptr==NULL and is not used for accounting; on failure the old block is kept (UE Realloc does not free on failure); signed delta in modular size_t arithmetic is correct; every block is freed by the allocator selected by its own state's ud, with no mixing (lua_close of a libc state still frees through libc). lauxlib's buffer box uses lua_getallocf with the same ud, so it stays consistent. No lua_setallocf/getallocf calls anywhere in the port.
- Vtable slots: Realloc=64 and Free=80 match the profile (decoded:392/400, two sources agree) and the eboot malloc-replace thunks (+0x40/+0x50). The wrapper's `static const offset` is first initialized by the probe on one thread, before any state exists, so it is thread-safe after that. Alignment 0 gives DEFAULT_ALIGNMENT, and Lua needs at most 8.
- noexcept: both callbacks catch(...), and install catches everything, including the log. Install-once compare-and-swap (CAS) 0->2->1 with acquire load in backing_for_new_state is correct.
- GMalloc is called from the async and keybind threads: UE GMalloc is thread-safe (a proxy wraps it if not). Same as the game's own libc thunks.
- [B6-RATE]: correct, with an atomic counter, the first 20 lines, then one line per 1000.
- get_function_ref_cxx: stack effect matches the original, and each replaced site sits inside a try/TRY that catches std::exception (2395 catch runtime_error; 5040 TRY; 8879 try; 9281 inside TRY).
- The REFUSED helper takes no lock and is never called with m_thread_actions_mutex held. The line cap is correct (256 lines, one cap line, then silent while counting continues).

---

# B6b RE-CHECK (delta-b6b.diff only; Mac-only, source reading)

VERDICT: SHIP (findings 1-4 above are fixed; the two follow-ups below are minor and do not block)

What was checked and is correct:
- Rebase, `LuaMadeSimple.cpp:1731-1784`:
  - `lua_getallocf(old,&ud)` compared with `backing_for_new_state()` is the right ownership test. A state created after install returns 0, so hot-reloaded mods are untouched.
  - Pristine test: empty stack plus `lua_next` on the globals table. `lua_pushglobaltable`/`lua_pushnil`/`lua_next(nil key)` cannot raise. A fresh 5.4 state has an empty `_G`. The LuaMod constructor never touches the state: `Lua::Lua` only stores the pointer and `Registry(*this)`, and `install_mods<LuaMod>` only sets flags.
  - Map re-key: `extract` -> `node.key() = new` -> `insert` is legal for unordered_map node handles. The `Lua` object (and its `m_registry`, which holds `const Lua&`) keeps its address, so every `Lua&` stays valid.
  - Nothing else caches the old `lua_State*` before start_mod. The `Lua` members other than `m_lua_state` hold no state pointer, and `LuaMod.cpp:3374` compares `get_lua_state()` at call time.
  - On allocation failure (-3) the node goes back in and the libc state is kept, so it fails closed.
  - `lua_close(old)` frees the old blocks through libc (ud nullptr), so the right store frees them.
  - `atpanic` and `warnf` (ud = new state) are re-applied. The friend declaration matches the namespace declaration, noexcept included.
- Call site, `LuaMod.cpp` start_mod: it runs before `prepare_mod`, before the main/async/hook threads are created and before any script runs. `const_cast` is safe because the underlying `Lua` object is not const. The log line maps 1/0 to gmalloc and -1/-2/-3 to libc, and is wrapped in try/catch.
- mod_name_of: the raw lookup with a `LUA_TUSERDATA` check (full userdata only) closes the undefined-behaviour case. try/catch plus `lua_settop(top)` restores the stack even if get_userdata throws after pushing a traceback.
- Catches:
  - Simple actions: `luaL_unref` runs exactly once, either as the last statement of the try or in the catch. The remaining `to_run` items and `deferred` are kept.
  - Delayed actions: the status-reset block after the loop is now always reached, so loop actions no longer freeze at `Executing`.
  - The guard destructor runs before each catch, so the flag is cleared.
  - Both sites now use `get_function_ref_cxx`, so no longjmp crosses the guard.

Follow-ups (minor, not blocking):
1. **MINOR (diagnostic only):** the rebase does `g_backing_states.fetch_add(1)` but never decrements `g_libc_states`. After a full rebase, `[MEM]` shows `be_states=N libc_states=N+...`, which looks like half the mods are still on libc. Fix: add `g_libc_states.fetch_sub(1);` next to the `fetch_add` (LuaMadeSimple.cpp, the success path just before `lua_close(old_state)`). The reviewer should judge success from the per-mod `[MEM] mod=X lua-state-backend=gmalloc rebase=1` lines.
2. **MINOR (pre-existing race, slightly wider):** `lua_instances` is an unsynchronized global map, and `process_lua_function` calls `find` on every Lua->C call from any thread. The rebase extracts and inserts while earlier-started mods may already be running Lua on their async thread, the keybind thread or game-thread hooks. This is the same race class as the existing unsynchronized `new_thread()` emplace in make_main_state/make_async_state/make_hook_state, and emplace can rehash, which is worse. So it is not a new hazard. Cheap way to close the rebase's share of it: rebase all LuaMods in one pass at the top of `start_lua_mods()`, before the first `start_mod`, when no mod Lua runs yet. Keep the start_mod call as a no-op fallback (it returns 0).
- Edge (accepted): `lua_pushstring` inside mod_name_of can still raise LUA_ERRMEM (longjmp) on out-of-memory. Only on out-of-memory, and no lock is held there.

---

# B6c CHECK (delta-b6c.diff, read with whitespace ignored; Mac-only, source reading plus the launch-3 klog/decoded log)

VERDICT: FIX FIRST (one small fix needed: finding 1. Everything else is correct.)

## Findings

### 1. MAJOR: the new async-loop catch can turn a crash into a frozen game thread, because a hand-locked mutex leaks
- `LuaMod::process_delayed_actions()` (candidate `LuaMod.cpp:10070-10075`) calls `actions_lock()` (`LuaMod.hpp:235`, a plain `std::mutex::lock()`), then `m_delayed_actions.insert(...)`, which can throw bad_alloc when the vector grows, and only after that calls `actions_unlock()`.
- Before B6c, that exception ended the thread and terminated the game. Now `update_async` catches it (10050-10057) and loops again with `m_actions_lock` still held:
  - the next `actions_lock()` on the same thread either deadlocks, or (error-checking mutex) throws system_error every 5 ms, flooding the log;
  - the next mod call to ExecuteAsync / ExecuteWithDelay / LoopAsync on the game thread (`LuaMod.cpp:4033/4060/4091`, the same lock) blocks forever, so the game freezes.
- Fix: use RAII at every site. This is a member function, so the private mutex is accessible:
  ```cpp
  { std::lock_guard<std::mutex> g{m_actions_lock};
    m_delayed_actions.insert(...); m_pending_actions.clear(); }
  ```
  Do the same at `clear_delayed_actions` (10141) and the three Lua registration sites 4033/4060/4091: `std::lock_guard<std::mutex> g{mod->m_actions_lock};` around the emplace_back, or a small RAII wrapper over `actions_lock`/`actions_unlock` if the member is not reachable there. An emplace_back that throws there is caught by the Lua function wrapper, but in B5/B6 it also leaves the lock held, so it is the same bug.

## Checked and OK
- **LuaObjectSetAllocator** (`LuaUObject.cpp:48-120`):
  - The rebind (template converting constructor, `value_type`, `==`/`!=` all true) is correct for libc++ `__hash_table`, both for nodes and for the bucket array.
  - libc++'s default-constructed unordered_set allocates nothing, so the first allocation is the first `emplace`. All allocate/deallocate calls (node, bucket rehash on emplace, erase) happen inside the three functions, and each holds `s_lua_unreal_objects_map_mutex`. `find`/`contains` never allocate. So the sticky mode read and write always happen under the mutex.
  - No other reference to the old global exists (grep over the candidate tree and port headers).
  - Alignment: GMalloc default is at least 8, enough for nodes and pointers.
  - A `bad_alloc` from allocate unwinds the lock_guard, and emplace keeps its strong guarantee.
  - The set is never destroyed, so nothing is freed into GMalloc at exit.
  - If the first wrap ever happens before install, the mode sticks to libc. That is B6b behaviour, safe but with no gain. Suggest logging the chosen mode once (`[MEM] uobject-set-backend=...`) so the next launch proves it.
- **Hook catches** (process_event_hook, funcptr_marshal_hook, VmtEngineTickDrain, engine_tick_hook):
  - `NativeLuaActionDispatchScope` (4802-4826) is declared inside each try. On unwind it restores `m_is_processing_actions = PreviousProcessing` (false) and `NativeLuaActionDispatching = false`, and releases its `unique_lock` on `m_thread_actions_mutex` before the catch runs. The later explicit `m_is_processing_actions = true` is also undone by that destructor. So no flag or lock stays stuck.
  - The NativeName ticket in the marshal hook is RAII inside the try.
  - Pending queues not merged on the failing tick are merged on the next good tick. VmtEngineTickDrain's overflow moved to pending is returned the same way.
  - catch(...) does not intercept Lua longjmp, which is unchanged. A nested Lua->engine->hook path does not drain because OwnsDispatch is false, so no longjmp can cross these scopes from below.
- **Listener catches**: correct and harmless. Note that `FClassCreateListener::NotifyUObjectCreated` has an empty body, and `FLuaObjectDeleteListener::OnUObjectArrayShutdown` (inline, `LuaUObject.hpp:39-42`) is not wrapped. Minor, shutdown only; wrap it for completeness.
- **update_async / remove_if lambda**: inner catches are now guarded, and a Loop action that throws is removed as before (result=true), which keeps PC behaviour. Only the hand-locked insert (finding 1) can escape.
- (4) The `[MEM] FAIL` Error line and (5) the `g_libc_states` decrement are correct.

## Crash reading (launch 3): plausible, with one correction
- Module mapping of the klog backtrace (`klog-20261012-162139.log` from line 6178; launch-3 dynamic-library table):
  - Launch 3: bottom frames are all eboot.bin (main thread, from `0x4000bf` up to `0x726b2c`). Exactly ONE PRX frame (`+0xe17367`) sits directly above game code, then an 8-frame PRX chain `+0xb2df0 .. +0x24bcd`, then PRX `+0x11dd5d6 .. +0x11bc04c`, then libSceLibcInternal `abort`.
  - Launch 2 (`klog-20261012-152716.log`): the SAME 8-frame chain (`+0x24bcd .. +0xb5409/+0xb2df0`), but under it are PRX frames `+0x3005a1 .. +0x2f97d0` and libkernel `+0x763d` (thread start). So that crash was on a PRX-created thread, probably the async Lua thread.
- So "same frames" means the shared throw -> no handler -> `std::terminate` -> abort chain (libunwind/libc++abi), NOT the same throw site. The two crashes start differently:
  - launch 2: an exception escaped a PRX thread function. B6c item 3 targets this; add finding 1's fix as well.
  - launch 3: an exception escaped an engine-called PRX entry on the game thread, with NO Lua VM frames under it. The stack is too shallow for a throw from inside the ForEachUObject Lua walk; that path would show luaV_execute / process_lua_function / ForEachUObject frames. It points at an engine->PRX callback that throws directly. B6b had no catch in `FClassDeleteListener::NotifyUObjectDeleted` (allocating Output::send marker plus pool removes), and the engine calls it for every GC'd object. Another candidate is a pump hook's own bookkeeping outside process_simple_actions. B6c guards both.
- The ForEachUObject trigger fits the libc-exhaustion story: about 160k set nodes (around 32 B each through libc) plus a bucket array of about 1.3 MB, about 6 MB in total, with a temporary double size during rehash. That is about the size at which launch 2 ran out. The exhausted heap then makes the next unguarded allocation in an engine callback throw. Moving the set (item 1) is the real fix; the guards keep the game alive if something else runs out.
- Limits: the symbol sidecar is `MAP-RECONSTRUCTED`, with an assumed order and not sidecar-exact (names land past function sizes), and the bare PRX has no symtab. So frame names cannot be trusted, and bad_alloc cannot be told apart from a libc++ verbose-abort, which also calls abort. Cheap follow-up (no new import): a `std::set_terminate` handler that emits a `diagnostic_event("terminate", code)`, with code 1 = bad_alloc, 2 = system_error, 3 = other std::exception, 4 = non-std, using `std::current_exception()` plus a rethrow-and-catch ladder, written with fixed storage. The next crash would then identify itself.

---

# B6g CHECK (delta-b6g-uobjectglobals.diff + delta-b6g-classlistener.diff; Mac-only, source reading. UE behaviour is from my knowledge of the engine's FUObjectArray; I did not open Epic source.)

VERDICT: FIX FIRST (one crash risk, and results that differ from the walk a PC mod gets)

The premise holds in the common case. UE `FreeUObjectIndex` calls every `NotifyUObjectDeleted` first and only then clears the item and pushes the index to `ObjAvailableList`. `AllocateUObjectIndex` then either pops a freed index or appends with `AddSingle`. Locking is fine (see the end). The gaps are below.

## Findings

### 1. BLOCKER: Query dereferences UClass pointers that may already be freed
- `S.Classes` is never pruned. On every query, `Query` runs `TSuperStructRange(Class)` and `Super->GetNamePrivate()` for EVERY class ever seen, BEFORE any staleness check (the `ClassAt[i] != Class` test comes later, per index).
- Dynamic classes do get garbage-collected: Blueprint-generated classes from streamed or unloaded levels, plugin content, and reinstanced classes. After that, the index reads freed engine memory (GMalloc may have reused it), and a garbage super-chain can crash the game or match the wrong name.
- Fix: when a class is first inserted in `ClassSlot` (it is alive then), store its own `InternalIndex` (`ClassObjIdx`). In Query, check that it is alive before touching the chain:
  ```cpp
  const int32_t ci = S.ClassObjIdx[c];
  if (ReadSlot(ci) != Class || SlotUnreachable(ci)) { S.ClassIndices[c].clear(); continue; } // dead class: drop
  ```
  If a class is alive, its supers are alive too, so the chain is safe after this check. Optional: keep a tombstone, so a later class allocated at the same address re-inserts cleanly.

### 2. MAJOR: FindAllOf returns duplicates when an index is reused by an object of the same class, and the per-class lists grow without limit
- The Dirty path sets `ObjAt[i]=ClassAt[i]=nullptr`, then `Record(i, new)` pushes `i` onto class C's list again. The OLD entry `i` is still in C's list, and it is valid again because `ClassAt[i]==C`. Query then pushes `i` twice and FindAllOf returns that object twice.
- Freeing and reusing an index for the same class is the common churn pattern (widgets, markers, projectiles; exactly the minimap's case). Those duplicates are never compacted, so lists grow for as long as the game runs.
- Fix: after `std::sort(Hits...)` add `Hits.erase(std::unique(Hits.begin(), Hits.end()), Hits.end());`. Also drop duplicates during the compaction loop, using a per-query stamp array (`Vec<uint32_t> Seen` sized like ObjAt; skip an entry and do not keep it if `Seen[i]==stamp`).

### 3. MAJOR: FindFirstOf no longer returns what the walk (and PC UE4SS) returns
- The walk (`UObjectGlobals.cpp` FindFirstOf, ForEachUObject arm) returns the FIRST object whose own class name equals the query (it breaks there). Super-chain matches only overwrite `ObjectFound` and do not break. So if no direct match exists, the walk returns the LAST super-chain match. Upstream UE4SS behaves the same way.
- The index returns the lowest-index hit of either kind. The answer differs whenever a subclass instance comes before the first direct instance, or whenever only subclass instances exist (for example `FindFirstOf("PlayerController")` when only `BP_PlayerController_C` instances are live).
- Fix (FirstOnly): first take the lowest-index hit with `ObjAt[i]->GetClassPrivate()->GetNamePrivate().Equals(ClassName)`. Otherwise take the HIGHEST-index super-chain hit. FindAllOf is fine as written: index order matches the walk's order.

### 4. MAJOR: slots read as null are never re-checked, so objects the walk finds can be missed
- **Async-loading race:** `AddSingle` makes the new `NumElements` visible before `ObjectItem->Object` is stored. A Sync running at that moment (the async loading thread creates objects while FindAllOf runs on the game thread or on a mod's async thread) records `nullptr` for `i` and moves `SyncedNum` past it. That object is never recorded until it is freed.
- **Disregard-for-GC pool:** `AllocateObjectPool` pre-adds `MaxObjectsNotConsideredByGC` items, so `NumElements` already covers the whole pool. While `OpenForDisregardForGC` is set, new objects take `++ObjLastNonGCIndex`, which is neither an append nor a reuse. If the first FindXOf runs before `CloseDisregardForGC` (mods start early on PS5), every later disregard object is missed.
- Fix (generic, no new offset): treat every slot seen as null as "watch". In the Build loop and in the append loop, `if (!O) S.FreeWatch.push_back(i);`. FreeWatch already re-checks each sync until the slot fills. Cost is a few reads per watched slot, far below the full walk.

### 5. MAJOR: the index trusts a delete listener that might not be registered
- The index is exact only if `FShutdownDeleteListener` really receives every delete. If registration was refused, the index goes stale silently: reused slots hold new objects, `ReadSlot(i) != ObjAt[i]` drops them at query time, and they are never recorded. Refusal can happen through the L25 `REFUSED (5)` path (`UObjectArray.cpp:862-866`, "growth required but GMalloc unbound; skipping registration") or in a variant-B build.
- Fix: enable the index only when the delete-listener registration is confirmed (a flag set where the Add succeeds; for example the existing L25 add-success path, or `L25ShutdownDeleteNotifyMarker.calls() > 0` at build time). Otherwise `Query` returns false and the walk is used.

### 6. MINOR: no unreachable check before following object pointers in Sync, a crash risk on non-recycling builds
- When `bShouldRecycleObjectIndices` is false, or the index is `<= ObjLastNonGCIndex`, `FreeUObjectIndex` leaves the item's `Object` pointer and its Unreachable flag in place while the memory is freed. The Dirty and FreeWatch loops then call `Record(i, ReadSlot(i))`, which calls `GetClassPrivate()` on freed memory.
- Fix: in all four record paths (build, dirty, freewatch, append), treat `SlotUnreachable(i)` like null and watch the slot. The walk skips unreachable objects too. During a GC mark phase this only parks slots in FreeWatch for one sync.

### 7. MINOR: failure partway through leaves partial results that the walk then duplicates
- If `OutStorage.push_back` throws inside Query, the catch disables the index and returns false. FindAllOf then runs the walk, which appends to an `OutStorage` that is already partly filled.
- Fix: record `const size_t base = OutStorage.size();` and `OutStorage.resize(base)` when Query returns false.

### 8. MINOR (performance)
- `ObjAt.resize(Num + 4096)` copies about 2.6 MB every 4096 new objects during loading. Use geometric growth: `std::max(Num + 4096, size * 3 / 2)`.
- Query walks the super-chains of ALL classes (thousands) on every call. Cache the matching-class list per `ClassName` and rebuild it only when `Classes.size()` changes (and after finding 1 drops a class).
- `Dirty` can reach 4M x 4 B = 16 MB on GMalloc when no query runs for a long time. Acceptable.

## Checked and OK
- Locking: Query holds `S.Mutex` only around reads of the object array and object fields, and the `Out` lambdas (assign or push_back; no Lua, no engine call). The delete listener takes the same mutex and never calls into Query. Neither path triggers GC or object deletion, so the non-recursive mutex cannot deadlock on one thread. A GC (game thread or GC worker) waiting on the mutex while an async-thread Query runs just delays one notification; it even shields that object from being freed during the read.
- FindAllOf returns hits in index order, the same order as the walk. The validity checks match the walk (ForEachUObject skips unreachable/null; same `IsValidObjectForFindXOf`; same super-chain-by-name match).
- EngineAlloc (rebind, equality, throws bad_alloc when GMalloc is missing) and the fail-closed exception path are fine. Memory: about 2.6 MB for ObjAt/ClassAt, plus about 0.7 MB of index lists and the hash, all on GMalloc.
- `r9_index_note_deleted` runs inside the R9-GUARD try in `FShutdownDeleteListener::NotifyUObjectDeleted`, so a bad_alloc on `Dirty.push_back` cannot reach the engine. It does leave the index stale for that one index, though: on bad_alloc set `S.Disabled = true` there (catch inside `r9_index_note_deleted`).

---

# B6g RE-CHECK (delta-b6g-uobjectglobals-v2.diff; Mac-only, source reading)

VERDICT: FIX FIRST (one small fix: finding 1. Fixes 1-8 from B6g CHECK are otherwise applied correctly.)

## Findings

### 1. MAJOR (new): a class marked "dead" by mistake loses its objects from the index for good
- In Query, when `LiveAt(ClassSelfIndex[c]) != Class`, the code sets `ClassDead[c]=1` and calls `ClassIndices[c].clear()`.
- The objects of that class stay recorded in `ObjAt`/`ClassAt` with the same values, and `Observe` returns early when `ObjAt[i]==Object && ClassAt[i]==Class`. So nothing ever pushes them back. `ClassSlot` (which resets `ClassDead`) runs only for an object observed NEW.
- Result: if the class check ever fails while the class is actually alive, FindAllOf/FindFirstOf miss every existing instance of that class for the rest of the session, while the walk would still find them.
- `LiveAt` returns null for a slot flagged Unreachable. Live classes carry that flag for a moment during a GC mark phase, and a Query can run then from a mod's async thread (LoopAsync/ExecuteAsync calling FindAllOf). It also fails if `ClassSelfIndex` were ever wrong.
- Fix: clear only when the class is really gone, and otherwise just skip it for this query:
  ```cpp
  FUObjectItem* CI = ItemAt(S.ClassSelfIndex[c]);
  UObject* CO = CI ? CI->GetUObject() : nullptr;
  if (CO != static_cast<UObject*>(static_cast<void*>(Class))) { S.ClassDead[c] = 1; S.ClassIndices[c].clear(); continue; } // slot freed/reused: really gone
  if (CI->IsUnreachable()) { continue; }  // transient (GC mark) or pending purge: skip this query only, keep the list
  ```
  This never walks an unreachable class's chain. A freed class's slot is cleared (recycling) or still flagged unreachable (no recycling), so the chain is still never read from freed memory.

### 2. MINOR: lists of classes that are never queried keep growing
- Duplicates and stale entries are compacted only for classes a query matches. Every free and reuse of an index by the same class pushes that index again (Observe after `ObjAt[i]` was cleared to null). Over a long session, churny classes no mod asks for (particles, components) grow at 4 B per reuse, on GMalloc.
- Fix: keep `Vec<UClass*> ListedUnder` per index and push only when `ListedUnder[i] != Class` (then set it). In the common same-class reuse case the old entry already serves.

### 3. MINOR (safety net): check `GetInternalIndex_Private` once at build
- The offset is profile-checked (InternalIndex_Private = 12, decoded:156/683), but a wrong value would silently make every class look dead. That is fail-open (empty results), not fail-closed.
- Fix: at the end of the first build, require `LiveAt(Class->GetInternalIndex_Private()) == Class` for the first few recorded classes; otherwise set `Disabled = true`, so the walk is used.

## Checked and OK
- **FirstOnly:** lowest-index exact class match, else the highest-index subclass match. This matches the walk exactly (the walk breaks on a valid exact match; subclass matches only overwrite `ObjectFound`).
- **De-duplication:** `SeenStamp` per index per query, plus `sort`, gives correct and unique FindAllOf results in index order. The stamp skips 0 when it wraps.
- **Observe:** never calls `GetClassPrivate` on an empty or unreachable slot. Build, dirty, watch and appended slots all go through Observe, so the async-loading race and the start-up pool cases are both covered by FreeWatch re-checks. `ClassSlot` dereferences a class only while one of its objects is live.
- **Class pointer reused at the same address by a new class:** the refresh in `ClassSlot` updates `ClassSelfIndex` and clears `ClassDead`. Old entries are filtered out by `LiveAt(i) != O`.
- **`DeletesSeen > 0` gate:** correct (it proves the listener is live). The rebuild after more than 4M entries in Dirty is fine. A bad_alloc in note_deleted disables the index.
- **FindAllOf:** truncates back to the start size on failure. Growth is geometric from 65536.
- **Locking:** unchanged from the B6g CHECK, and still no deadlock path.

---

# B6h CHECK (delta-b6h.diff; Mac-only, source reading)

VERDICT: FIX FIRST (one correctness fix in the name cache: finding 1. Everything else is diagnostic-level.)

## Findings

### 1. MAJOR: the name cache goes stale when a class address is reused before any query saw the old class die
- The cache (`NameEntry::Upto` / `Matches`) is keyed by class POSITION `c`. A class position before `Upto` is examined once and never again. The only invalidation is `if (S.ClassDead[c]) S.Names.clear();` in `ClassSlot`.
- `ClassDead[c]` is set ONLY by `ClassState`, and `ClassState` runs again later only for classes in some entry's `Matches` (or `Pending`). So:
  - **Missed objects:** a class that did NOT match a cached name is never re-checked. When it dies and a NEW class is allocated at the same address (GMalloc's binned allocator reuses freed blocks of the same size; a map change unloads and loads level/widget Blueprint classes), `ClassSlot` finds the key with `ClassDead[c]==0`, so Names is not cleared. The new class keeps the old "no match", and if it does match (for example the minimap's widget class after a map change), FindAllOf/FindFirstOf miss all of its objects for the rest of the session. The walk would find them.
  - **Wrong objects:** a class that DID match dies, and a new class lands at the same address. UE recycles the most recently freed index first, so the new class can also get the same object index. `ClassState` then sees "live" (the slot holds that pointer), and the index returns objects of an unrelated class for that name.
- Before B6h the super-chain was evaluated on every query, so neither case could happen. This is new.
- Fix: identify the class by more than its address. Store `ClassNameAt[c]` (the class's own FName) when it is inserted. In the `ClassSlot` existing-key path:
  ```cpp
  const int32_t self = Class->GetInternalIndex_Private();
  if (S.ClassDead[c] || self != S.ClassSelfIndex[c] || !Class->GetNamePrivate().Equals(S.ClassNameAt[c])) { S.Names.clear(); S.ClassNameAt[c] = Class->GetNamePrivate(); }
  ```
  For a matched class, also check `Class->GetNamePrivate().Equals(S.ClassNameAt[c])` in the per-query `ClassState` (2 reads) and treat a mismatch as dead plus `Names.clear()` (after the loop, because `E` points into `Names`). Same address + same index + same name is effectively the same class reloaded. Cheaper and stricter alternative: also record `GetSuperStruct()` per position and compare it.

### 2. MINOR: per-function call counts are wrong across windows (diagnostic only)
- `take_function_stats` exchanges `g_fn_calls[i]` to 0 and then writes `calls` BACK for every active entry, but resets only the top-N entries at the end. Every non-top function carries its count into the next window while its ticks start from 0, so its calls/tick ratio is inflated. The plain store and the reset also race with other threads' `fetch_add` (lost increments).
- Fix: keep calls in a local `uint32_t best_calls[16]` next to `best_ticks`; never write `g_fn_calls` back.

### 3. MINOR: notes on reading the numbers, plus a cheap speed-up
- Times are inclusive: Lua -> engine -> Lua calls nested inside a measured function are counted in both, so `api_ms` can exceed the real time spent. Note this in the report text so it is not read as "more than 100% of a frame".
- `take_function_stats` does 2 x 16384 locked exchanges per second on the game thread (roughly a few hundred microseconds). Scan only up to the highest id ever registered: keep a relaxed `g_fn_max_id` updated in `note_fn_name`.

### 4. MINOR (existing bug, not new): off-by-one in `process_lua_function`
- `if (func_id > lua_functions.size())` should be `>=`. `func_id == size()` would index past the end. Not reachable with valid upvalues; fix it while you are in this function.

## Checked and OK
- **E pointer stability:** `S.Names` can change only in `Sync` -> `Observe` -> `ClassSlot` (clear), which runs BEFORE `E` is taken, and in the `>512` clear plus `emplace_back`, which also runs before `E` is taken. Nothing in `Examine`, `ClassState` or the Matches loop touches `Names` or `Classes`. Class positions never move (`Classes` only grows). `Pending` is swapped into a local before it is re-examined, so retries cannot run twice. No pointer invalidation.
- **Dead classes:** they stay in `Matches` but are skipped by `ClassState` (1). Classes still being garbage-collected are skipped for that query only (2). Pending covers classes that were unreachable when first examined.
- **Using comparison index plus number as the key** is at least as strict as the walk's `Equals`, so it is never wrong; at worst there are two cache entries.
- **Timer and longjmp:** the RAII timer is skipped when `TRY` raises its Lua error by longjmp. That only means no add; it holds no resource and the counters stay consistent. Relaxed `fetch_add` from many threads with `exchange` on the game thread is fine for counters.
- **`__builtin_readcyclecounter`** is rdtsc on PS5 (invariant TSC). Converting with each window's ticks per ns is fine; the first window prints -1.
- **The [PERF] formatter:** guarded by the existing `catch(...)`; `snprintf` truncation stops the loop (`fused + 48` guard); output is always NUL-terminated.
- **One `lua_instances.find` instead of `contains` + `find`:** same behaviour.

---

# B6i CHECK (delta-b6i.diff; Mac-only, source reading)

VERDICT: FIX FIRST (two small fixes in [R9-NOTIFY]: findings 1 and 2. The B6h fixes are applied correctly.)

## Findings

### 1. MAJOR: objects created BEFORE a registration get notified, up to about 1M of them in one burst
- `NotifyWanted` becomes true at the first `r9_index_collect_new` and is never reset. After that, `Observe` pushes every new object into `NewIdx`, up to `1<<20`.
- `r9_feed_notify_from_index` returns early, WITHOUT collecting, while `m_static_construct_object_lua_callbacks` is empty. That happens when every callback cancelled itself (returned true) or the mod was reloaded. During that time any Sync started by FindFirstOf/FindAllOf keeps filling `NewIdx` with objects created while nobody was registered.
- When a mod later registers NotifyOnNewObject, the next pump delivers all of them: callbacks fire for objects built minutes earlier, possibly thousands in one frame (a hitch), where PC would fire only for objects constructed after the registration.
- Same effect on a smaller scale at the very first registration: `collect` sets `NotifyWanted` before its `Sync`, so everything appended since the last Sync (which may be long before the registration) is delivered.
- Fix (generic, small):
  - Add `r9_index_notify_arm()`, called at the end of the NotifyOnNewObject registration (it takes only the r9 mutex; no other lock is held there). It sets `NotifyWanted=false`, runs `Sync(S)` (absorbs everything that exists now, building the index if needed), runs `NewIdx.clear()`, then sets `NotifyWanted=true`.
  - In `r9_feed_notify_from_index`, when the callback list is empty, call `r9_index_notify_disarm()`: `NotifyWanted=false; NewIdx.clear();`. Do not just return.
  - Then only objects that appear after the registration (to within one Sync) are delivered, matching PC.

### 2. MAJOR: the same object can be notified twice
- `NewIdx` holds bare indices. A Sync started by a Query between pumps can push index `i` for object A. If A dies and B reuses `i` before the next pump, the pump's Sync pushes `i` again for B. The feed reads the CURRENT object at each entry, so B gets two callbacks (and A gets none, which is correct).
- Fix: store pairs `{index, object pointer}` in `NewIdx`. In the feed, skip an entry unless `IndexToObject(i)->GetUObject() == ptr` (and the object is reachable). Optionally also skip a repeated `(i, ptr)` in the same batch.

### 3. MINOR (note, deliberate or acceptable differences from PC; put them in the report)
- An object created AND destroyed between two pumps is never delivered. PC fires it inside StaticConstructObject.
- The index also reports objects that were not made through StaticConstructObject (for example classes and functions created in place by the engine). The class plus outer-name filter makes this practically invisible to mods.
- Delivery happens after the object is fully initialized (next pump) rather than inside construction. You already noted this.
- `GetSerialNumber()` is usually 0 for new objects (the serial is assigned lazily), so the executor's serial check is skipped. The executor still re-checks `GetUObject() == constructed_object` before every callback, which covers slot reuse within the same pump (pointer ABA is acceptable).

## Checked and OK
- **Lock order:** the pump holds `m_thread_actions_mutex` (NativeLuaActionDispatchScope keeps a unique_lock for the whole dispatch), and `collect` then takes the r9 mutex, so the order is thread_actions -> r9. No path takes them the other way: Query and note_deleted take only r9 and call nothing that locks thread_actions; GC (delete listener) can start inside a Lua callback, but that is again thread_actions -> r9. The feed's own re-lock of the recursive mutex is fine. No deadlock.
- **Feed matching:** the same rule as the seam (class or any super with the registered name AND outer name). Matching runs on live, reachable objects only (`IsUnreachable` and null are checked at feed time). Feed and executor run in the same call on the game thread, and the executor re-checks the slot before each callback, so an object that dies before its callback runs is skipped.
- **Coverage:** reused and appended slots both reach `Observe` (via Dirty/FreeWatch and the appended range). Objects that existed at the first build are not reported.
- **B6h fixes:** `ClassNameKey` with invalidation on dead / index change / name change in `ClassSlot`, a name-key mismatch in `ClassState` causing dead plus `NeedNamesClear` (cleared after the loop; `E` is not used afterwards), local `best_calls`, the scan limited to `lua_functions.size()`, and `func_id >=`. All correct.

---

# B6i RE-CHECK (candidate UObjectGlobals.cpp:1385-1652, LuaMod.cpp:170-176/3272/5160-5172; Mac-only, source reading)

VERDICT: FIX FIRST (one-line fix: finding 1.) Findings 1 and 2 from B6i CHECK are otherwise fixed correctly.

## Findings

### 1. MAJOR: a second registration throws away notifications already owed to the first
- `r9_index_notify_arm()` (UObjectGlobals.cpp:1625-1641) always runs `NotifyWanted=false; Sync; NewIdx.clear(); NewObj.clear();`.
- When a registration already exists (NotifyWanted is already true), that clear drops every object that appeared since the last pump: up to one frame of notifications owed to the callbacks already registered, by this mod or another. It happens whenever a NotifyOnNewObject registration runs after the first one in a different frame: a second mod starting later, a hot reload, a registration made inside a callback, or the self-test.
- Fix:
  ```cpp
  if (S.NotifyWanted) { return; } // already armed: keep what existing listeners are owed
  ```
  at the top of the try. Only the first arm (from the disarmed state) absorbs and clears. The cost is that a later registrant may also receive objects created up to one pump before it registered. That is within the same frame, and is the same tolerance as the one-pump delivery delay.

### 2. MINOR: arm and disarm can race when a registration happens off the game thread
- If NotifyOnNewObject is called from a non-game thread (inside ExecuteAsync/LoopAsync), the feed can see an empty list under `m_thread_actions_mutex` while the registration's `emplace_back` (which takes no lock; an existing race) and `arm()` run, and then call `disarm()` AFTER `arm()`. The new registration then never fires.
- Fix (cheap): in the feed, when the callback list is non-empty but `collect` returned false only because it is not armed, call `r9_index_notify_arm()` once. For that, `collect` must report `!NotifyWanted` separately (for example return an enum or an out flag).

### 3. MINOR: an index failure silently brings back the old refusal
- If the index is `Disabled` (or `DeletesSeen==0` forever), `arm` and `collect` do nothing and NotifyOnNewObject is accepted but never delivered. The `[REFUSED]` line was removed, so this is a silent refusal again.
- Fix: in the registration lambda, emit `refused_log::accepted(..., "index-disabled")` when the index reports Disabled (add a `r9_index_is_usable()` query). Or emit one Error line the first time `collect` returns false while callbacks exist.

### 4. NOTE: delivery order
- `collect` sorts by index, so callbacks within one pump arrive in slot order rather than construction order. Harmless, but different from PC. Mention it in the report next to the one-pump delay.

## Checked and OK
- **`collect`:** returns false unless armed. It keeps only `{index, object}` entries whose slot still holds exactly that object, live and equal to `ObjAt`. A slot that changed hands between collects delivers only the later object, once. `sort` + `unique` removes leftovers. Everything runs under the r9 mutex and the exception path disables the index.
- **`arm`:** Sync with NotifyWanted off absorbs everything that exists now (building the index if needed) before it switches on. It is called from the registration lambda after the `emplace_back`. The lock order stays thread_actions -> r9 (or r9 alone off the game thread).
- **`disarm`:** runs each pump while nobody listens, takes the r9 mutex briefly, and clears the queue, so nothing builds up for a later registrant.
- **Feed:** unchanged (the matching and executor re-check were already OK in B6i CHECK). The declarations at the top of `namespace RC` match the definitions.

---

# B6k CHECK (delta-b6k.diff; Mac-only, source reading)

VERDICT: SHIP (two minor follow-ups; neither blocks)

## Checked and OK
- The per-callback try/catch in `process_pending_notify_on_new_object_callbacks` now contains a failing mod callback, so the remaining callbacks and the rest of the pump run. That is the intended fix for launch 7's 58 guard lines.
- **Cancel-return:** when the callback errors there is no return value, so no cancel is recorded and the registration stays. That matches PC (an erroring callback is not unregistered). `thread_refs_to_unref` / `callbacks_to_remove` are filled only on the success path, and the unref/erase blocks after the loop now always run, because the exception no longer leaves the loop.
- **Lua stack on a Lua error inside the callback:** `Lua::call_function` (`LuaMadeSimple.cpp:1591-1597`) pops the error message and removes its error handler before throwing, and the function and argument are already consumed by `lua_pcall`. So the stack is balanced. A failing `get_function_ref_cxx` discards its own push.
- **Hook guards:** the added `catch (const std::exception&)` before `catch (...)` only adds `what()` to the log, and the log call is itself guarded. RAII state (dispatch scope, lock) unwinds as before.
- **PERF-ASYNC 5 ms threshold:** diagnostic only.

## Follow-ups (minor)
1. **Lua stack slot leak when building the argument fails.** If `LuaType::auto_construct_object` throws after `get_function_ref_cxx` pushed the function, the function stays on `exec.lua`'s stack (one slot per failure, on the mod's hook thread state). With a 100k-object batch and a class that fails to build every time, this grows towards the stack limit. Fix: `const int r9_top = lua_gettop(exec.lua->get_lua_state());` before the try, and `lua_settop(exec.lua->get_lua_state(), r9_top);` in both catch blocks.
2. **Unlimited log on non-standard exceptions.** The `catch (...)` log line has no rate limit, unlike the 20-line cap on the std::exception branch. Share the same counter.
- Report note: the 20 logged `what()` texts will show which mod's callback fails and why. They are the mod's own Lua errors, not runtime faults, unless the text points at a binding (for example an object that could not be wrapped).

---

# B6l CHECK (delta-b6l.diff; Mac-only, source reading)

VERDICT: FIX FIRST (one blocker in the method-lookup cache: finding 1. The rest is sound.)

## Findings

### 1. BLOCKER: the method cache is keyed by the receiver's CLASS, but the lookup searches the receiver ITSELF when the receiver is a struct
- `handle_unreal_property_value` (`LuaUObject.cpp:2697+`) keys the cached `GetFunctionByNameInChain` result on `base->GetClassPrivate()`.
- `GetFunctionByNameInChain` searches `ReceiverAsStruct(this)` (`UObject.cpp:596-604`). When the receiver is itself a UStruct (a UClass, UScriptStruct or UFunction object reached through Lua, e.g. a class from StaticFindObject or `obj:GetClass()`), it searches THAT struct, not its class.
- Those receivers' classes are `Class` / `BlueprintGeneratedClass` / `ScriptStruct` / `Function`, which are all CLASS_Native, so they pass the cache gate. Two consequences:
  - The first lookup of name N on class object X is cached under (`UClass`, N). A later lookup of N on a DIFFERENT class object Y returns X's UFunction, and the call runs X's function against Y. That is a wrong call, a parameter-layout mismatch and a possible crash.
  - If X is a Blueprint class, the cached UFunction belongs to a class that CAN be unloaded, so the pointer can dangle. That breaks the "native classes are never freed" premise.
- Fix: cache only when the searched struct is the receiver's own native class:
  ```cpp
  const bool r9_receiver_is_struct = r9_class && r9_class->HasAllCastFlags(static_cast<Unreal::EClassCastFlags>(Unreal::UStruct::StaticClassCastFlags()));
  const bool r9_cacheable = r9_class && !r9_receiver_is_struct && r9_class->HasAnyClassFlags(Unreal::EClassFlags::CLASS_Native);
  ```
  (Or key by the struct that is actually searched and require it to be a native UClass.)
- The property path is NOT affected: `r9_cacheable` is set only in the `!obj_as_struct` branch (receiver's class). When `DerivedType` is a UStruct type, the struct itself is searched and nothing is cached.

### 2. MINOR: the busy-wait lock has no pause or back-off
- `r9prop::lock()` spins on `__atomic_exchange_n` with no pause. The critical section is one slot copy, so contention is tiny. But if an async thread is preempted while holding the lock, the game thread spins for a whole OS time slice.
- Fix: inside the loop, spin on a relaxed load with `__builtin_ia32_pause()`, and `sched_yield()`/`std::this_thread::yield()` after about 64 tries, if yield is already available without a new import. Otherwise pause only.

### 3. MINOR: the notify executor's setup work is not budgeted
- Each pump still moves the WHOLE pending list into `to_execute` (one `find_if` over callbacks per entry), runs about 2 ms of callbacks, and then re-inserts the rest at the front (O(n)).
- With a mod registered on a common class during a level load, pending can hold thousands of entries, so the setup plus re-queue costs scale with the backlog every frame.
- Fix later: take only the first K pending entries (for example 256) into `to_execute`; leave the rest where they are.

### 4. MINOR: the de-duplication marker can collide with a query stamp
- `collect` uses `SeenStamp[i] == 0xFFFFFFFF` as an "already taken" marker and resets those entries to 0 afterwards (all under the r9 mutex). That is correct, except that Query stamps can also reach 0xFFFFFFFF. An index whose last query stamp was 0xFFFFFFFF would be skipped once by `collect`.
- Once in about 4 billion queries, so negligible. Fix: skip 0xFFFFFFFF in the stamp increment, the same way 0 is skipped.

## Checked and OK
- **Native property cache:** the receiver's class with CLASS_Native. Compiled classes are never unloaded in a shipping PS5 build (no hot reload or live coding), so a cached FField*/null cannot dangle or go stale. Custom properties are still checked first. Caching null results is correct for native classes (fixed layout) and is what makes method calls cheap (a property miss comes before every function hit). The key includes the name number, which is stricter than the lookups' index-only `Equals`, so there are no false hits. The table is a C++17 inline variable, so one instance across TUs.
- **Front re-queue:** the remaining `to_execute` entries go back in order at the FRONT. The feed appends newer matches at the back, so order is kept. Entries for callbacks cancelled in this pump are dropped next pump (the callback id lookup fails after the erase). There are no duplicates: entries skipped before the budget point are not re-queued.
- **Dying-object skip:** checked right after the pointer re-check (the object is alive), using the Unreachable flag and BeginDestroyed/FinishDestroyed.
- **Lua stack:** `lua_settop` restore in both catches, with the shared 20-line error cap.
- **Feed backlog:** `{index, object}` entries are skipped unless the slot still holds that object and it is reachable. Disarm clears the backlog. The re-arm when the backlog drains is a no-op when already armed, and it fixes the disarm race noted in B6i RE-CHECK item 2. Matching runs under a 1 ms budget, holding the recursive thread_actions mutex as before.
- **Function name from the key at -1:** guarded by the type check, with no conversion side effect; it falls back to the metatable name.
- **Sync and delete-listener timers:** RAII timers with relaxed atomics; diagnostic only.
