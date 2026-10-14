# IS-LUA-ALLOCATOR — Where Lua memory comes from, what "not enough memory" actually is, and the staged fix

Agent IS, 2026-10-12. Analysis + patch prep only (battery-safe: no builds, no
console, no live-tree edits). Deliverables staged in
`outputs/2026-10-08-l25-prep/round5-staging/lua-alloc/` (see README-STAGING.md).

Evidence base: `outputs/2026-10-08-l25-prep/evidence/acceptance-pass2-20261012.decoded.log`
(frozen l26-A PRX efbfc656 session, 2026-10-11 20:46:48–20:49:49).

---

## 1. Executive summary

Claude round-13 item 1 said: "GROUP A is a real port problem, not 'registry
full'. Lua has no registry cap; 'not enough memory' means our Lua allocator
failed." Verification result, point by point:

1. **The Lua allocator is the stock default `l_alloc` (realloc/free)** — no
   custom allocator, no arena, no cap anywhere in the port. Every Lua
   allocation bottoms out in the GAME PROCESS's libc heap.
2. **There is no Lua-side cap to raise** — no LUAI_MAXALLOC, no lua_setallocf,
   no FMalloc binding for Lua (FMalloc appears in the port only as a vtable
   layout name map in UE4SSProgram.cpp). The failing "cap" is the shared
   game-process `libSceLibcInternal` heap, which the ENGINE squeezes during
   save/map load.
3. **The failure is real but transient**: LUA_ERRMEM storms hit exactly while
   the game is mid-load (20:47:54–20:48:21); Lua mods run fine again after
   the map is in (20:49:39). Independently, the log shows the C++ side
   throwing `std::bad_alloc` at the same moments (`[l22] ExecuteInG: std::ba`
   — "std::bad_alloc" truncated by the known narrow→wide print seam).
4. **A second, masking mechanism exists**: any C++ exception escaping a
   registered function is converted to a Lua error by
   `TRY -> throw_error -> luaL_error` — and `luaL_error` ALLOCATES to format
   the message. Under heap exhaustion that allocation fails, and the error
   surfaces as `LUA_ERRMEM => not enough memory`, destroying the real
   message. So SOME "not enough memory" lines are actually std::bad_alloc
   (or any other exception) wearing LUA_ERRMEM's clothes. This is why the
   group-A failures look like a Lua memory problem even when the first
   failure is a C++ vector push.
5. **A small real leak exists** (per-run registry refs grow monotonically on
   the self-test state: 7→28→33→51→56) but its magnitude (tens of closures)
   cannot explain the storm; the self-test re-run path that caused it was
   already fenced by IU's one-run-per-session guard — the console ran an
   older mod build.

Fix staged: (a) observability so the next launch has the numbers Claude
asked for; (b) an opt-in dedicated arena so small Lua allocations stop
competing with the engine for the libc heap; (c) self-test hygiene so
crash-resume cannot stack duplicate permanent registrations.

---

## 2. FIND THE ALLOCATOR — the exact chain (all receipts in-tree)

```
LuaMod.cpp:902        m_lua(LuaMadeSimple::new_state())
LuaMadeSimple.cpp:1090  auto new_state() -> Lua& { luaL_newstate(); ... }
lauxlib.c:1109        LUALIB_API lua_State *luaL_newstate (void)
                        { return lua_newstate(l_alloc, NULL); }
lauxlib.c:1027        static void *l_alloc(...) { ... return realloc(ptr, nsize); }
```

File identities (frozen l26-A build):

- `LuaMod.cpp` pin: `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp`
  (SOURCE-SELECTION pin; sha256 bfc392c5…16a2)
- `LuaMadeSimple.cpp` pin:
  `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/src/LuaMadeSimple/LuaMadeSimple.cpp`
- `lauxlib.c` pin:
  `.superpowers/native-dependency-spike/ue4ss-stdio-consumer-adaptation-1/patched/lauxlib.c`
  (stock `l_alloc`; the patch series touches stdio consumers, not allocation)
- The `luaL_newstate` call site is the ONLY state factory: every mod Lua
  state and the console-executor state go through it
  (LuaMod.cpp:902 and LuaMod.cpp:6723 `LuaStatics::console_executor`).

Where realloc/malloc resolve:
`outputs/2026-10-08-l26-package-A/lineage/import-contract.json` — `malloc`,
`calloc`, `realloc`, `free`, `aligned_alloc` are strong imports whose
evidence module is **libSceLibcInternal** (the game process's libc).
So Lua memory = the game process's libc heap. Not a PRX pool, not
sceKernelMemoryPoolFit, not FMalloc, not a fixed buffer.

Upstream comparison: upstream UE4SS also uses plain `luaL_newstate()` — the
port did not diverge here; what diverges is the platform (a game process
whose libc heap is contended by the engine).

## 3. THE CAP — answering the task's a/b/c/d

- (a) PRX module heap (sceKernelMemoryPoolFit budget): **does not exist**.
  The PRX imports allocator functions from libSceLibcInternal; it does not
  create its own kernel memory pool for libc.
- (b) FMalloc arena limit: **not involved for Lua**. `Unreal::FMalloc`
  appears only in vtable-layout bookkeeping (UE4SSProgram.cpp:1156-1162).
  Nothing routes Lua (or any port allocation) through GMalloc.
- (c) custom Lua allocator with fixed buffer: **none**. No lua_setallocf
  call exists anywhere in the port or the vendored Lua tree.
- (d) system malloc running out of address space: **effectively yes, at the
  libc-heap level**: the game process's libSceLibcInternal heap is the
  single shared backing store, and during save/map load the engine's own
  libc usage squeezes it. Lua 5.4's core already does an emergency full GC
  + one retry before l_alloc's NULL becomes LUA_ERRMEM (lmem.c), so every
  reported failure survived a full collection — it is genuine backing-store
  refusal at that moment, not garbage accumulation.

Supporting timeline (pass-2 log):

| Time | Event |
|---|---|
| 20:47:17–19 | ST2 runs 1+2: 147/168 PASS each, no ERRMEM |
| 20:47:54 | first `std::bad_alloc` (ExecuteInGameThread C++ body) + first LUA_ERRMEM, zzdiag state 0x8801ae208 |
| 20:47:54–20:48:05 | repeated bad_alloc/ERRMEM on keybind callbacks (keys 37–40, 118) |
| 20:48:07 | ST2 run 3: 133/168 — 16 tests FAIL "not enough memory" (129–146, 168: the registration/exec families) |
| 20:48:21 | last recorded bad_alloc |
| 20:48:54+ | engine map transition churn (DeleteListener NotifyUObjectDeleted, calls=35869→137299) |
| 20:49:39 | CheatManagerEnabler Lua callback runs CLEAN (map loaded; pressure gone) |

Also decisive: during the storm, the OTHER Lua states' queue refs stayed at
their steady reuse values (0x8800be888 always ref=10, 0x8801ae208 always
ref=12 when it worked) — luaL_ref slot reuse proves those closures were
freed properly; failure was the allocator, not the registry.

## 4. THE LEAK — and the exception-masking trap

Real leak (small): the ueselftest state (0x8802d4068) shows monotonic
registry-ref growth 7→28→33→51→56 across the session. Cause: part9's
no-unregister registration families (RegisterProcessConsoleExecPostHook,
RegisterCallFunctionByNameWithArguments Pre/Post, RegisterULocalPlayerExec
Pre/Post, RegisterConsoleCommandHandler/GlobalHandler, RegisterKeyBindAsync)
each pin a Lua closure + C++ callback entry forever; the deployed mod build
re-ran them per RUN. Current ueselftest main.lua already has IU's
"one run per session" guard (the console ran an older build), and 0003 adds
once-only arming so even crash-resume can't stack duplicates. Magnitude:
tens of small closures per run — real, observable with the new counters,
but not the storm.

Exception-masking trap (fix NOT staged — behavior change needs its own
review, documented here): `LuaMadeSimple.hpp:797 TRY(lua_State*, ...)`:

```cpp
try { return code_to_try(); }
catch (std::exception& e) { throw_error(lua_state, e.what()); return 0; }
```

`throw_error` → `luaL_error(lua_state, final_message.c_str())` — and
`luaL_error` allocates (lua_pushvfstring). Under heap exhaustion THAT
allocation fails, the bad_alloc metamorphoses into LUA_ERRMEM "not enough
memory", and the original message ("std::bad_alloc") is destroyed (it only
survives in the parallel [l22] line, truncated by the print seam). Note the
direction of causality the log proves: the C++ bad_alloc comes FIRST
(logged by the lambda's own catch before `throw;`), then the pcall reports
LUA_ERRMEM. (Lua itself is compiled as C11 with setjmp/longjmp —
luaraw-ldo.o has setjmp/longjmp imports and no C++ personality — so a C++
exception cannot be converted by Lua's LUAI_TRY; the conversion happens in
OUR TRY wrapper.) Consequence for triage: on this port, "not enough memory"
means "allocation failed OR an exception's error-formatting allocation
failed". The 0001 diagnostics disambiguate: allocator failures increment
`fails=`; masked exceptions show `[l22] …: std::ba…` style lines with
`fails=` flat.

## 5. THE DIAGNOSTIC (staged, 0001) — what the next launch will log

- Every `lua_pcall … LUA_ERRMEM` line now ends with
  `[lua-mem: gc_kb=N live_kb=N blocks=N allocs=N fails=N fail_o=N fail_n=N]`
  (per-state gc_kb; process-wide allocator counters; last failing sizes).
- A 1/s `[MEM] state=… gc_kb=… gc_step=… live_kb=… blocks=… allocs=… fails=…
  fail_o=… fail_n=… arena_cap_kb=… arena_used_kb=… arena_ext_kb=…` line rides
  the existing ExecuteInGameThread queue-depth throttle (proves GC liveness
  via one real LUA_GCSTEP per line, per Claude's ask).
- New read-only Lua binding `GetLuaMemory()` returning the same snapshot.
- ueselftest main.lua (0003) prints `MEM before` / `MEM after` around every
  run — Claude's exact before/after-run numbers with zero engine coupling.
- Interpretation key: `fails` rising + `gc_kb` flat = backing-store refusal
  (engine pressure) → enable the arena (0002). `gc_kb` climbing without
  bound = a true Lua-side leak → hunt refs with the once-armed self-test.

## 6. THE FIX (staged)

- 0002 arena (compiled-in, env-gated OFF): one 64 MiB (8..512 tunable)
  reservation taken at startup while the heap is healthy; all Lua blocks
  ≤2048 B served from 32 size classes with intrusive free lists; larger
  blocks malloc-backed with a 16-byte header (ownership checks always
  in-bounds); resize-in-place inside a class; arena exhaustion degrades
  per-block to malloc instead of failing. No new link-time imports
  (frozen 207-import contract intact). Verified on host under ASan+UBSan
  (200k-op churn: alignment, no duplicate pointers, in-place shrink, zero
  missed frees) and prospero-clang++ -fsyntax-only with the frozen l26-A
  flag sets (rc=0 for both TUs).
- Why not FMalloc for Lua: routing Lua through the engine allocator would
  give it the game's whole budget, but couples Lua state lifetime to engine
  allocator availability/ABI (GMalloc pointer is verified for reads in this
  project; allocating through it pre-setup_unreal or from the async thread
  is unproven risk) — and the frozen import contract forbids nothing but
  the mission prefers no new coupling. The arena achieves decoupling with
  zero engine ABI dependence. Revisit only if the diagnostics show the
  reservation itself cannot be taken.
- 0003 self-test hygiene: once-only arming for the 8 no-unregister
  registration families (crash-resume safety) + MEM before/after bracketing.

## 7. What was deliberately NOT done

- No change to the TRY/throw_error masking path (behavior change; separate
  review; documented in section 4).
- No live-tree edits, no builds, no console contact, no import-contract
  changes, no UE4SS_LUA_ARENA default-on.
- The `[l22] ExecuteInG: std::ba` print-seam truncation (narrow→wide
  ensure_str) is a known separate seam (agents EV/EM territory); the new
  logs avoid it by using only wide literals and numbers.
