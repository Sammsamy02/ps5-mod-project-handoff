# CATCH-AUDIT: what can swallow a Lua error once Lua errors are C++ exceptions (B7)

Everything here is **Mac-only** (source reading, host tests on the port's real Lua 5.4.7, a PS5-toolchain
analysis compile where noted). No console contact, no console log read, no PRX built, nothing applied to a
tree. Line numbers are the **B4 candidate** (`typescan-r4/candidate`, snapshot b63ba460) unless a line says
"staged". Claims about the console are labelled; the only console line used is `D:904` of the B4 launch-1
decoded log (earlier session, supporting evidence only, never a verdict for B7).

**Revised after the reviewer's finding P1 (2026-10-12).** The first version said "no engine-to-Lua entry is live on the
PS5 because hooks are refused". That was wrong: only *Blueprint*-function hooks are refused. Hooks on native
UFunctions are live (B4 launch-1 decoded log `D:807`, `D:818` registered; `D:866-D:894` both mods' callbacks ran;
`D:918` thunk fired from game code). Sections 3f and 5 are rewritten; the other sections only changed where marked.

## Plain verdict

- **Out of 254 catch handlers in the code the build compiles, only 2 do real harm once Lua is built as C++**, and
  both have a tested one-line fix:
  1. `LuaUObject.cpp:142` swallows the Lua error of **every failed UFunction call** (wrong argument count and
     similar). Result with the real code: the mod gets a broken status and loses the message, 200 of 200 times.
  2. `LuaMadeSimple.cpp:1587` (the B2 error wrapper) swallows an **out-of-memory error** and leaves Lua's global
     lock held. Other threads would then hang. Result with the real code: lock left held at 5 of 40 allocator
     failure points; with the fix, 0 of 40.
- **Four more handlers are harmless today but get the same one-line fix** so every catch-all in the Lua code treats
  Lua errors the same way. **Five handlers would print a misleading "non-standard C++ exception" log line for every
  Lua error** (today the error jumps over them); the fix keeps the logs identical to today.
- **`catch (std::exception&)` can never catch a Lua error** (a Lua error is a thrown *pointer*). The 96 handlers of that
  kind are not a risk.
- **One exposure is live, not parked (corrected):** the game itself calls our Lua hook callbacks. Native-UFunction hooks
  are registered and fire on the PS5 today (`D:807/D:818/D:866-D:894/D:918`, console-proven for B4 launch 1), through
  the data-only Func swap in `ScriptHookThunks.cpp`, so **game frames sit below every hook callback**. Whenever a Lua
  error escapes a hook callback, the PS5 unwinder cannot carry it through game code, and with Lua as C++ that is
  `std::terminate`. The error can only escape in narrow cases (an unsupported parameter type, a pusher failing,
  out-of-memory; ordinary mod-script errors are caught by `lua_pcall` and logged), and in the most common sub-case
  the process aborts **today** too (Lua's panic path, same in both builds). Fix, small and staged: one `catch (...)`
  at the three places where our code hands control back to the game (`ScriptHookThunks.cpp`, edits H1-H3), so nothing
  can leave into game frames. The earlier idea of a `lua_pcall` trampoline inside the hook callbacks is **rejected**
  (it changes the Lua stack the pre and post callbacks share, section 5). Only Blueprint-function hooks
  (`script_hook`) and top-of-thread drains stay latent.
- **The pre-build gate now enforces all of this** (`tools/b7_source_gate.py`): it fails on the unpatched tree (6
  catch findings + 3 game-boundary findings), passes on the staged tree, and all 6 injected defects were caught
  (plus the `--strict-h4` switch).

## 1. The fact everything rests on

`ldo.c:55-61` (chosen automatically when Lua is compiled as C++): a Lua error is `throw(c)` where `c` is a
`struct lua_longjmp *`: **a pointer**. A coroutine yield from a C function is the same throw (`ldo.c`
`lua_yieldk`). `luaD_rawrunprotected` catches with `catch(...)` and turns anything it did not throw itself into
status `-1` (`if (status == 0) status = -1`).

| Handler | Catches a Lua error? | Notes |
|---|---|---|
| `catch (std::exception&)` and every other class type | **No** (pointer is not a class) | all 96 `std::` handlers; also the 4 typed ones (`StartupCancelled`, `StartupDeadlineExceeded`, `runtime_error`) |
| `catch (void*)` | **Yes** (pointer converts to `void*`; `lua_longjmp` is incomplete outside `ldo.c`, so a named pointer type is impossible) | what the staged edits use. Host test proves it matches with Apple's libc++abi; **the B7 startup probe proves it with the PS5's libc++abi** (`void_catch=1 swallowed=0` in the `[LUA-CPP]` line) |
| `catch (...)` | **Yes** | swallowing = the Lua error never reaches `lua_pcall`; `lua_error` took Lua's lock before it threw, so the lock stays held |
| `noexcept` function or destructor | `std::terminate` | |

Why a swallowed error is wrong and not merely lost: Lua's API takes the global lock before throwing
(`lapi.c:1248` "will unlock when control actually leaves the kernel"; `ldo.c:516-533` `precallC` unlocks before and re-locks after every C function). `lua_pcall` pairs that unlock. If a
`catch (...)` eats the error first, the unlock never happens (host test: lock depth stays +1, other threads cannot
lock).

## 2. What was scanned

`tools/catch_inventory.py` (extends round-6 `catch_all_audit_v2.py`; new: all handler kinds, plus the 1,777 generated
`FunctionBodies/*.cpp` that were `#include`d and skipped before).
File set = the 378 unit sources of the B4 composition receipt + every repo path in their `.d` files, minus
libc++/SDK/LuaRaw. Result (`audit/catch-inventory.log`, `audit/CATCH-INVENTORY.json`):

| | Count |
|---|---|
| files / functions indexed | 2,913 / 14,256 |
| catch handlers (all kinds) | 254 |
| `catch (...)` | 154 (round 6 counted 152 on a smaller file set) |
| `catch (std::exception&)` family | 96 (33 have Lua code in the try block; 9 of those have a `pcall` in it) |
| other typed | 4 (startup-cancel types, no Lua) |
| handlers that can match a Lua throw (`...`/`void*`) | 154 |
| of those, rethrow | 21 |
| of those, Lua reachable in the try block and no rethrow | **21** (same set as round 6 v2; 6 fixed by the staged patch, 15 remain, all classified below) |
| noexcept functions / destructors that reach Lua | 12 (13 with the staged probe) |
| raw Lua callbacks (`lua_pushcfunction` / `pushcclosure` / `luaL_Reg` / `wrap_into`) | 45 registrations, 7 allocate without `TRY` |
| destructors indexed | 127; the only one that calls Lua is `~FNativeUFunctionCall` (`luaL_unref`); none contains a `throw` |

**Known limit of the call-graph rule** (name based, so it over- and under-reports): it does not see calls through
`std::function`, function pointers or virtual dispatch. I read those sites by hand: the hook dispatcher
(`DetourInstance.hpp:545,599`), `ScriptHookThunks.cpp` (`Thunk<Index>`), `VmtTickPumpCore.inc` (drive_tick), and the
other 130 `catch (...)` sites outside the Lua files (startup, scanning, logging, ports: none has a Lua call in its
try block; table in section 4).

## 3. Per-site verdicts: handlers that can see a Lua error

"Harmful" means: a Lua error crossing it is swallowed or mishandled. "Staged edit" is in
`staged-patch/b7-on-*/b7-runtime.patch` (one-line `catch (void*) { throw; }` before the `catch (...)`).

### 3a. Real defects (edit REQUIRED)

| # | Site (B4) | Wraps | Lua error can cross? | If swallowed | Edit |
|---|---|---|---|---|---|
| E1 | `LuaUObject.cpp:142` `protected_ufunction_call` | `call_ufunction_from_lua_impl` (UFunction marshalling; 11+ `lua.throw_error` calls, e.g. `:158`, `:267` "UFunction expected N parameters") | **Yes, in normal play** (any bad UFunction call) | **WRONG.** The catch-all stores the Lua throw and returns normally; the code then rethrows it *outside* the inner `lua_pcall`, where the outer `luaD_rawrunprotected` sees a throw it did not make (its status is still 0, `ldo.c:60`) and sets **-1**; the mod gets a non-string error. Real-code host test: status -1 and message lost, 200/200 | `catch (void*) { throw; }` |
| E2 | `LuaMadeSimple.cpp:1587` `throw_error_impl` (the B2 wrapper) | `handle_error` -> `luaL_traceback` (allocates) and the error callbacks | **Yes, on out-of-memory** (`LUA_ERRMEM` from the traceback buffer) | **WRONG.** The text is replaced by the raw message and a *second* error is raised; the first throw took Lua's lock and nobody releases it. Real-code sweep (allocator fails at point k=1..40): lock left held at 5 points; fixed: 0 | same |

### 3b. Harmless today, same edit for uniformity (DEFENSIVE)

| # | Site | Wraps | Why no Lua error crosses today |
|---|---|---|---|
| E3 | `LuaMadeSimple.cpp:1459` `call_function` mem-note | `query_lua_memory(L)` (`lua_gc(LUA_GCCOUNT)`, cannot raise) + `fmt::format` | no raising Lua call in the try |
| E4 | `LuaMod.cpp:1018` `native_lua_file` | `source->read`, `native_lua_chunk` -> `lua_load` | `lua_load` protects the parser; the reader is POD |
| E5 | `LuaMod.cpp:1446` `custom_module_searcher` (native path) | `native_lua_module` | every push goes through the protected `native_lua_push_text`; load is protected. (The non-native half of the function is unguarded but is not used on PS5 when a native source exists.) |
| E6 | `LuaPathShim.cpp:1790` `install` | ~40 Lua API calls (`lua_newtable`, `lua_settable`, `lua_pushfstring`, ...) | can raise only on out-of-memory, and `install` runs at mod preparation at the top level: with no handler a Lua error is a panic/abort (today too), so it never reaches the handler. If it ever ran nested, the error must reach the pcall: hence the edit |

### 3c. Misleading log noise (already rethrow; LOG-PARITY edits)

Today a Lua error jumps over these handlers (longjmp). With Lua as C++ it would land in them, log a line and
rethrow: result correct, **log wrong** ("... non-standard C++ exception" for an ordinary Lua error such as a bad
`RegisterKeyBind` call).
`LuaFText.cpp:140` and `:167` (E7), `LuaMod.cpp:2338` `RegisterKeyBind` (E8), `:2644` `StaticConstructObject` lambda (E9),
`:6355` `ExecuteInGameThread` (E10). Edit: `catch (void*) { throw; }` in front, so the logs stay as they are today.
Other rethrowing handlers (`WriterDevice.cpp:37`, `FText.cpp:160,180`, `UObjectGlobals.cpp:99,136`,
`UnrealInitializer.cpp:1466,1535,1577,1755,2111,2174,3252`, `LuaMod.cpp:8181`, `UE4SSProgram.cpp:646`,
`ScriptHookThunks.cpp:156`, `ue4ss_backend.cpp:113`): no Lua call in their try block, or top-level startup; correct as
they are.

### 3d. Harmless: inside `noexcept`, all Lua work is under `lua_pcall` (allow-listed, class HARMLESS-PCALL)

`LuaMod.cpp:241` and `:249` (`L23GameThreadProbe::callback`), `:278` (`Queue`), `:282` (`Cancel`), `:286` (`Poll`).
These are noexcept; a rethrow from them would be `std::terminate`. Every raising Lua call in their try blocks runs
inside `lua_pcall` (`luaL_loadbuffer` and `lua_load` are protected; `request` is called through `lua_pcall`).
**Keep as they are.**

### 3e. False positives of the name-based rule (allow-listed, class FALSE-POSITIVE; checked by hand)

`JMapGenerator.cpp:2213,2245` (JSON `add_value`, not Lua), `Logging.hpp:14` (template parameter named `callback`),
`UnrealInitializer.cpp:2335` and `:2636`, `ToolchainSelfTest.cpp:127` (an `L"..."` wide string read as a Lua state),
(`UE4SSProgram.cpp:1899` was listed here in the first version with the reason "the only Lua-reaching callee is
`Poll`"; that was wrong, see 3e-bis.)

**3e-bis. `UE4SSProgram.cpp:1899`, event-loop outer handler (class HARMLESS-INDIRECT; reason corrected after review P2).**
The try block (`UE4SSProgram.cpp` ~1800-1895, update thread) runs: (a) queued `event()` callables; (b)
`m_input_handler.process_event()`, i.e. the keybind lambdas (`LuaMod.cpp` ~2235), which take the LuaMod lock and call
Lua through `call_function` (= `lua_pcall`, failures become `std::runtime_error`, handled inside the lambda; the
`get_function_ref` push before it is an unprotected push like every push); (c) `mod->fire_update()` (empty for Lua mods
in this tree: `Mod::fire_update` has no body, `LuaMod` does not override it); (d) the noexcept `L23GameThreadProbe::Poll`.
(a) to (c) are indirect calls the name-based rule cannot see; I read them by hand now. No game frame is below this loop,
and a Lua error raised outside a protected call on this thread has no handler (panic/abort today and in B7: the H5
cross-thread case), so this `catch (...)` is a last fail-safe (it asks for shutdown), not a place that loses an error that
had somewhere to go. Allow-list entry reworded; no edit.

### 3f. Engine-to-Lua boundary: LIVE (native hooks) and latent (class LATENT-H4)

**Live on the PS5 today (console-proven for B4 launch 1, supporting evidence only):**

| Site | What it is | Evidence it is live | Exposure |
|---|---|---|---|
| `lua_unreal_script_function_hook_pre/post` (`LuaMod.cpp` B4 `:384`, `:488`), registered by `RegisterHook` native branch (`LuaMod.cpp` B4 `~:6020-6032`, `RegisterPreHook`/`RegisterPostHook`) | the Lua `RegisterHook` callbacks for native UFunctions | `D:807`, `D:818` `Registered native hook (3, 4) / (5, 6) for Function /Script/Engine.PlayerController:ClientRestart`; `D:866-D:894` both mods' callbacks ran, ConsoleEnabler unregistered its own hook; `D:918` `marshal hook first invocation (carrier fired)` | **game frames below it.** Inside the callback `TRY` (`RC::TRY`, `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/ExceptionHandling.hpp:75-91`, listed in `compiled-files-1e49188b.txt:44`) catches `std::exception&` only, logs it and returns (no rethrow) (a mod-script error comes back from `call_function` as `std::runtime_error`: contained). A Lua error raised *outside* a protected call there is not contained: `lua.throw_error` for an unsupported parameter type (`LuaMod.cpp` B4 `:467`, `:664`), a property pusher raising, or out-of-memory in a push. |
| `ScriptHookThunks.cpp` `Dispatch` / `Thunk<Index>` (`:75-86`; `catch (std::exception&)` only) | the per-UFunction thunk the game calls after the Func swap | same lines (`D:918`) | the three try blocks pass anything that is not a `std::exception` into game frames |

What happens to a Lua error raised on the hook thread (Mac-only, `ldo.c:115-135`, Lua 5.4.7): the hook thread is a
coroutine (`LuaMod.cpp` B4 `:1235` `ensure_hook_thread_exists`, `lua_newthread`) with no handler of its own at that moment. `luaD_throw` therefore
(1) if the main Lua state is inside a protected call, re-throws in the main state: **Lua as C** = a longjmp over the
game frames to that call (works by luck, skips every destructor on the way, e.g. `DispatchLifetime`); **Lua as C++** = a
C++ throw into game frames = `std::terminate`; (2) otherwise calls the panic function (`lms_lua_panic`,
`LuaMadeSimple.cpp:97`, returns 0) and Lua `abort()`s: **the same in both builds, i.e. today**. Sub-case (2) is probably the
common one (inference, not proven: mod code runs on its own coroutines, so the main state is rarely inside a protected call when the game fires a hook). No log of B4 launch 1 shows either
case happening (grep of the decoded log: no `PANIC`), so this is a code-reading exposure, not an observed crash.
B7 therefore changes only sub-case (1), from "longjmp over the game" to "terminate"; the H1-H3 containment turns it into
"logged, game continues" and leaves (2) as it is.

**Latent (not reachable by a Lua error with a game frame below it today):**

| Site | What it is | Why not live | Why it matters later |
|---|---|---|---|
| `LuaMod.cpp:8748` `script_hook` | callback of the generic script-hook route for **Blueprint** functions (`m_script_hook_callbacks`); its try block pushes Lua values and calls `lua.throw_error` (`:8696`) | Blueprint-function `RegisterHook` is refused: `HookProcessInternal=0`, `HookProcessLocalScriptFunction=0` (`UE4SS-settings.ini`), B4 report lines 99 and 197. (`D:780` is the built-in detour-style hooks only; it is **not** evidence about native-UFunction hooks.) | if Blueprint hooks are enabled (data-only UFunction Func swap), this site needs the same game-boundary containment as the live ones |
| `LuaMod.cpp:5616` `VmtEngineTickDrain` (noexcept) | engine-tick drain on the game thread | runs at the top of the thread: a Lua raise with no handler is panic/abort (same as today); `call_function` converts pcall failures into `std::runtime_error` | none unless a Lua frame ever sits below it |
| `LuaMod.cpp:9772` `process_delayed_actions` | loop/delayed actions, async thread or drain | same | the cross-thread case (H5) is fatal in both builds |
| `DetourInstance.hpp:545,599` `InvokeCallbacks` (not flagged by the rule: indirect call) | hook dispatcher `catch (...)` around registered callbacks | detours are not installed on PS5 (`no_detour_gate`) | a Lua error from a Lua hook callback would be swallowed here (lock +1) |

## 4. The other `catch (...)` sites (no Lua in the try block)

154 total; 29 have Lua reachable by the rule (sections 3a-3f); the other **125** have none. By area: candidate runtime
files that are not Lua binding code 49 (`UE4SSProgram.cpp` startup/shutdown/mod-start, `UObjectGlobals.cpp` 9,
`NameTypes.cpp` 5, `ToolchainSelfTest.cpp`, `VmtTickPumpCore.inc` 3, `TypeChecker.cpp`, `UObjectArray.cpp`,
`LuaUEnum.cpp`, `ScriptHookThunks.cpp`, `PosixFile.cpp`, `UObject.cpp`, ...), `UnrealInitializer.cpp` 20 (startup scans
and self-test logging), native ports under `.superpowers/` 32 (owned memory, detour owner, thread-name policy, loader),
other staging units 17, `typescan-r4/include` 5 (FName bridge logging), diagnostics 2. I read the try-block start of
each one in `audit/CATCH-INVENTORY.json` (file, line, enclosing function, handler hash); none has a Lua call, a
callback invocation or a Lua-state argument in its try block. Full list: `audit/CATCH-INVENTORY.json`.

## 5. Frames between a Lua raise and its `lua_pcall`

**noexcept / destructors (H2).** 12 noexcept functions or destructors reach Lua (`audit/catch-inventory.log` "H2"):
11 are the L23 probe (4), logging wrappers `observe`/`error`, the tick-drain pair, or name-collision false positives;
`~FNativeUFunctionCall` calls `luaL_unref` but is destroyed *outside* the inner pcall (below it), so a Lua error
never unwinds through it. No destructor contains a `throw`. Baseline file: `gate/noexcept-baseline.txt` (each entry
has the reason).

**Raw Lua C callbacks (H3).** 45 registrations (`audit/cfunction-audit.log`). A `std::exception` leaving one without
`TRY`: today flies through Lua's C frames to the next C++ catch; with Lua as C++ it is caught by Lua's own `catch (...)`,
becomes status -1, the lock count goes wrong and the next `lua_unlock` aborts (host test, child process). Same
severity as today (a crash), only under memory exhaustion. 7 callbacks allocate without `TRY`
(`pcall_error_handler`, `memfile_write`, the four shim wrappers `io_open/io_lines/os_remove/os_rename`); the other
38 either only push (cannot throw a C++ exception), are `TRY`-wrapped (`process_lua_function`), or have their own
try/catch. **Mitigation tested for both builds:** wrap the body in the existing `TRY(L, ...)` (host test
"raw-wrapped": status 2, no abort). Not part of B7 (not a regression); baseline `gate/cfunction-baseline.txt`.

**Engine boundary (H4).** Entry points that call Lua (40 `call_function`/`lua_pcall` sites in `LuaMod.cpp`), by group:

| Group | Sites (B4 lines) | Lua frame / game frame below it? | Status |
|---|---|---|---|
| **Native-UFunction hook callbacks** | `lua_unreal_script_function_hook_pre/post` `:384,:488` via `RegisterPreHook/RegisterPostHook` `:6028-6029`, thunk `ScriptHookThunks.cpp:75-86` | **game frames below (the game calls them)** | **LIVE on PS5** (`D:807, D:818, D:866-D:894, D:918`). Containment staged (H1-H3), gate check E |
| Blueprint-function hook callbacks | `script_hook` `:8748`, built-in hooks in `on_program_start` `:8907-9674`, `lua_load_map_pre/post_callback` `:5292,:5347` | possible | not live: Blueprint `RegisterHook` refused (`HookProcessInternal=0`); built-in detour hooks not installed (`D:780`, `D:922`). Same containment needed if enabled |
| Drain / top level | `process_simple_actions` `:4725`, `process_pending_notify_on_new_object_callbacks` `:4826`, `process_delayed_actions` `:4952,:9757,:9763` | no (engine tick, game-thread top) | live; a Lua error with no handler is a panic/abort today as well |
| Input / update thread | `RegisterKeyBind` `:2235`, `RegisterKeyBindAsync` `:2111`, `fire_on_lua_stop_for_self` `:8397` | no | live; cross-thread H5 as before |
| Loader / console | `load_and_execute_script` `:8277`, console exec `:9404` | no (`lua_pcall` with handler) | live, protected |
| Lua-initiated callbacks | `ForEachUObject` `:2977` (C++ array walk, not game frames) | yes, but only port frames | protected by `call_function` |

Fact for the decision: with Lua as C++, a Lua error that has to cross a **game** frame ends in `std::terminate`
(host test child `nested-unprotected`: "uncaught exception of type lua_longjmp*"); with Lua as C it is a longjmp that
silently skips the game frame (child survives, status LUA_ERRMEM).

**Rejected mitigation (the first version of this file proposed it): one `lua_pcall` of a C trampoline around the pushes
inside the hook callback.** Reading the callbacks closely shows why it cannot be dropped in: the pre callback leaves its
return value **on the hook thread's stack** and the post callback reads it back (`LuaMod.cpp` B4 `:530-575`,
`process_return_value`: "Stack pos 1: return value from callback 1 ... Stack pos 2 ..."; `is_nil()`, `discard_value()`,
`get_stack_size() > 0`). A `lua_pcall` frame hides everything below it and adds the trampoline's own argument, so
`get_stack_size() > 0` would always be true and the pre value would be invisible to post (MULTRET carrying would
change the indices a second time). The host-test children `nested-protected` prove the mechanism in isolation only;
they do not cover this protocol, and it cannot be tested here without the compile the 2 GB rule refused. **Do not use
it for the hook callbacks.**

**Staged instead: containment at the boundary (H1-H3, `ScriptHookThunks.cpp`).** A `catch (...)` after the existing
`catch (std::exception&)` in the pre-callback block, the post-callback block and `Thunk<Index>`, logging
`[B7-H4] contained a non-std exception (a Lua error?) at the game boundary: ...`. Properties (Mac-only reasoning,
compile and run not done): generic (no Lua knowledge in the file), no stack protocol touched, the original game
function still runs after a contained pre callback, in Lua-as-C builds it only ever catches what would have been
`std::terminate` anyway. Cost of containing instead of delivering: if the main Lua state was inside a protected call (the
sub-case above), that call is not unwound by the error (logged here instead); Lua leaves the main state with its
`lua_longjmp` status already set and one extra value on its stack. That is a defined, non-crashing state; it is not the PC
behaviour (PC delivers the error to the mod's pcall) and is listed as the price. **Not covered:** the panic/abort
sub-case (2) above. An optional follow-up H4-2 (not staged, needs its own review and a compile): in the Lua-as-C++ build
let `lms_lua_panic` throw a `std::runtime_error` instead of returning, which `TRY` in the hook callbacks would then log and
swallow; lock accounting looks balanced (`luaD_throw` unlocks before calling panic) but I have not tested it.

**Noexcept / frames.** Nothing else changed in this section.

## 6. Proposed edits (all in `staged-patch/`, none applied)

| Edit | Files | Class | Effect with Lua as C | Effect with Lua as C++ |
|---|---|---|---|---|
| E1, E2 | `LuaUObject.cpp`, `LuaMadeSimple.cpp` | REQUIRED | dead code | fix the two defects |
| E3-E6 | `LuaMadeSimple.cpp`, `LuaMod.cpp` x2, `LuaPathShim.cpp` | DEFENSIVE | dead code | uniform pass-through |
| E7-E10 | `LuaFText.cpp` x2, `LuaMod.cpp` x3 | LOG-PARITY | dead code | logs stay as today |
| P1, P2 | `LuaMadeSimple.cpp`, `LuaMod.cpp` | PROBE | prints `[LUA-CPP] mode=longjmp ...` | prints `[LUA-CPP] mode=cxx-throw ... void_catch=1 swallowed=0` |
| H1-H3 | `ScriptHookThunks.cpp` | H4-BOUNDARY (new) | contains any non-std exception at the game boundary (today: terminate) | the same; this is what keeps a Lua error out of game frames |

13 edit sites in 5 files per base tree for E1-E10 + P1/P2 (`b7-runtime.patch`), plus 3 sites in 1 more file for H1-H3
(`b7-h4/ScriptHookThunks.h4.patch`, one patch for all three bases; `tools/make_b7_h4_patch.py`). Three base trees are generated by `tools/make_b7_runtime_patch.py --base
candidate|b5final|b6`; every anchor must match exactly once or the script stops (it also stops on drift of the
pinned candidate shas). `git apply --check` / `patch --dry-run` are clean on `candidate` and `b6`. The H1-H3 patch is
`patch -p1 --dry-run` clean on the current tree; it also is applied first in `b7-h4-on-<base>/tree` (the overlay the gates use).

**Not proposed (and why):** no edit at `script_hook` / `VmtEngineTickDrain` / `process_delayed_actions` (a rethrow
would terminate or leak differently, see 3f); no `lua_pcall` trampoline in the hook callbacks (section 5: it breaks the
pre-to-post stack protocol); no change to `lms_lua_panic` (optional follow-up H4-2, section 5); no `TRY` hardening for non-std
exceptions (not a regression; listed in the gate baseline); no change to the 96 `std::` handlers.

## 7. Evidence

- Host test (real runtime code extracted byte for byte, real Lua 5.4.7 built as C and as C++ with C names; 138 checks,
  0 failed; `host-test/RESULT-before-void-catch-probe.txt`, see `host-test/RESULT-NOTE.txt`): see B7-PLAN section 5.
- `tools/b7_source_gate.py` (now five checks A-E): unpatched candidate = 6 catch findings (E1-E6 sites) + 3 game-boundary
  findings (`audit/gate-run-unpatched-current-candidate-20261012.txt`); staged B7 without H1-H3 = FAIL, 3 findings
  (`audit/gate-run-staged-b7only-NO-H4-b5final-MUST-FAIL.txt`); staged B7 + H1-H3 on final-b5 and on the B6 work copy =
  PASS with 3 latent warnings (`audit/gate-run-staged-h4-b5final.txt`, `...-h4-b6.txt`); `--strict-h4` = FAIL while the 3
  latent sites remain; `tools/gate_mutation_test.py` (see `audit/gate-mutation-test-h4.txt` for the 8-case run).
- Staged files vs base through `fs_throw_gate.py` and `no_detour_gate.py`: identical violation sets (12 known
  `std::filesystem` sites in both; 0 detour sites; `audit/gates/`). The H1-H3 file (`ScriptHookThunks.cpp`) separately:
  fs_throw 1 file checked, 0 violations before and after; no_detour 1 source scanned, 0 sites before and after
  (`audit/gates/*-h4-ScriptHookThunks-*.raw.txt`).
- PS5-toolchain single compile of the 5 staged files with the units' B4 argv: did NOT run (the script refused every unit: the Mac stayed under 2 GB free;
  see `audit/ps5-compile-check-b5final-SKIPPED-lowmem.json`).

## 8. Corrections to earlier text

- Round 6 FEASIBILITY H1 said 21 flagged sites and fixed six; the six are the same sites as E1-E6 here (E1, E2, E3,
  E4, E5, E6). New in this audit: E7-E10 (log parity), the 125-site read of the non-Lua catch-alls, the indirect sites
  (`DetourInstance.hpp`, thunks), the std-family proof, the callback and engine-entry tables, and the gate.
- Round 6 said "none of the six has game code between it and its Lua handler". That is true for E1/E2/E4/E5; for E3 it
  is irrelevant (no raising call) and E6 is top-level install code.
- **Input from the other workflow (round 7 B6 work copies, read-only).** The first snapshot I read (09:03, base sha
  `8e514710...`) sat on the earlier 15-site B5 proposal, without the `get_mod_ref_cxx` helper of `final-b5.diff`; the
  lock-raise gate here reported the 2 remaining helper raises (RegisterCustomEvent and RegisterHook call `get_mod_ref`
  at the lock). By 09:39 that workflow had rebased onto `final-b5` (base sha `afaf5caa...`); on that copy
  (`LuaMod.cpp` `0c15fd2d...`) the gate reports 0. It is a free real-world check that the gate catches the helper
  case; nothing to do now.
- **Reviewer finding P1 (2026-10-12).** The first version claimed no Lua hook callback is live on the PS5 and that the
  ScriptHookThunks route was not live. Wrong for native UFunctions (evidence in 3f). Corrected here and in `B7-PLAN.md`;
  the gate now has check E; the pcall-trampoline shape in the old section 5 is withdrawn (stack protocol).
- **Reviewer finding P2.** The `UE4SSProgram.cpp:1899` allow-list reason was wrong (3e-bis).
- **Reviewer finding P3.** `lua_close` is called (`LuaMod.cpp:8576`, `:8624`; only `Lua::~Lua` has it commented out,
  `LuaMadeSimple.cpp:644-645`); the pass-through blocks are 11 (E1-E10, E7 twice) plus the probe's own handler, not 12.
- Line numbers in this file are B4 candidate lines; the live candidate `LuaMod.cpp` is now the final-b5 file, so they no
  longer match the tree. Use the allow-list keys (file|function|type|handler hash), not line numbers.
