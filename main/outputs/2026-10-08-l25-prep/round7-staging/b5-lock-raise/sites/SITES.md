# B5 lock-then-raise: SITES track report

Base: `candidate/src/LuaMod.cpp` sha256 `4d6bc63b7a90a8026a9c918ee6bee36c2d4eaa450220be82e8071f5427bfde9a` (B4 snapshot b63ba460).
No console contact, no PRX build, nothing written outside this directory (build products of the host test and compiler logs went to the session scratchpad). Every claim is **Mac-only** unless it says
"console-proven" with a log line. Line numbers are the candidate (base) file unless stated.

> Round-8 note: the live candidate `LuaMod.cpp` was overwritten with the final-b5 result (sha `afaf5caa...`) by another workflow at 10:04. Everything here that says "candidate/base" refers to the pristine B4 file (sha `4d6bc63b...`), kept exactly in `base-candidate/` (reverse-applied from `final-b5.diff`, sha verified). The candidate output appended at the end of `CHECK-RESULTS.txt` was taken before that and cannot be reproduced against the live file.

## 1. Plain summary

- The proposed 15-site diff applies cleanly and every one of its 15 raises is inside a lock scope that the
  TRY wrapper catches (14) or a local catch (1). Message and error type seen by the mod do not change.
- It is **not complete**: 2 more raises sit inside the same lock scopes, hidden in the helper `get_mod_ref`
  (RegisterCustomEvent line 3084, RegisterHook line 5969). The diff leaves them as a longjmp over the guard.
  RegisterHook's `get_mod_ref` sits in the same guarded region that the minimap's refused calls go through, so it belongs in the same build.
  Fix: a C++-throwing twin `get_mod_ref_cxx` used at those two lines. Included in `final-b5.diff`.
- Four more native lock scopes (seven call lines) can only raise when the code runs nested inside a Lua call and a Lua
  reference or property is invalid ("ambient"), and nine scopes can only fail on out-of-memory. Both are listed, not fixed in the final
  diff (a small optional diff for the stale-reference part is provided).
- `L22Diagnostics::throw_lua_error` is not used inside any lock scope, so no log line is lost.
- One real behaviour difference: TRY keeps at most 1023 bytes of the message (old path 4095). No site message comes near that.

## 2. Files in this directory

| file | what |
|---|---|
| `final-b5.diff` | **the final diff** against the candidate: the 15 proposed replacements (unchanged text) plus `get_mod_ref_cxx` (helper + 2 call sites); 12 hunks after `diff -u` merges neighbours. `patch -p1` or `git apply` from the worktree root. |
| `optional-ambient-getfuncref.diff` | optional, apply after `final-b5.diff`: stale-reference raise in 4 native lock scopes. Not part of the final diff. |
| `patched/final-b5/src/LuaMod.cpp`, `patched/final-b5-plus-optional/src/LuaMod.cpp` | the patched files (sha in section 9) |
| `inventory-base.md` | all 97 lock scopes in the candidate tree, one row each (section 5) |
| `inventory-final-b5-LuaMod.md`, `inventory-final-b5-plus-optional-LuaMod.md` | same for the patched LuaMod.cpp |
| `tools/lock_raise_gate_v2.py` | **superseded (round 8)**: now a 30-line wrapper that runs the ONE gate `../gate/lock_raise_gate.py` in hard mode (same command line as before). The round-7 regex gate is kept in `tools/archive/` for the record |
| `tools/scan_all_locks.py`, `tools/inventory.py`, `tools/syntax_check_luamod.py` | survey / inventory / single-TU syntax check |
| `hosttest/b5_sites_host_test.cpp`, `hosttest/RESULT.txt`, `hosttest/build_and_run.sh` | host test on real Lua 5.4.7 compiled as C (32 checks) |
| `CHECK-RESULTS.txt` | output of every check below |

## 3. Does the proposed diff apply? (task part 1)

- Header base sha equals the candidate file sha: yes (`4d6bc63b...`, CHECK-RESULTS.txt top).
- `patch -p1 --dry-run` and `git apply --check` from the worktree root: clean, no offset, no fuzz (11 hunks, 15 replacements).
- Each hunk replaces one `lua.throw_error(x)` with `throw std::runtime_error(x)` (the 8695 hunk keeps `fmt::format`, which also
  returns `std::string`). No new `#include`: `<stdexcept>` is already line 16, `<format>` line 13. Include lines base vs final are identical.
- Header comment line refs are slightly off: `process_lua_function` is LuaMadeSimple.cpp:1699-1734 and its TRY call is at 1726 (the
  header says 1699-1722). Cosmetic.

## 4. How a C++ throw becomes the same Lua error, after unwinding (task part 3)

Wrapper path for sites 1-16 (all four lambdas are registered through `Lua::register_function`):

1. `Lua::register_function` (LuaMadeSimple.cpp:1148-1157) stores the lambda in `lua_functions` and pushes
   `process_lua_function` as the C closure (`lua_pushcclosure(..., &process_lua_function, 2)`). The lambdas are
   `LuaMod.cpp:2345` (UnregisterHook), `3062` (RegisterCustomEvent), `3105` (UnregisterCustomEvent), `5946` (RegisterHook).
   `lua_functions` is only ever called from one place, cpp:1727, so nothing can reach these lambdas without TRY.
2. The Lua VM calls `process_lua_function` (cpp:1699). It runs `return TRY(lua_state, [&]{ lua_functions[id].value()(data_owner); ... })` (cpp:1726-1732).
3. `TRY` (LuaMadeSimple.hpp:843-857): `try { return code(); } catch (std::exception& e) { capture_cxx_exception_text(message, 1024, e.what()); }`
   then, **after the handler has exited**, `throw_error_from_buffer(lua_state, message)` (hpp:856).
   `std::runtime_error` is a `std::exception`, so it is caught.
4. Order: C++ unwinding destroys the `lock_guard` (and the local `std::string`) on the way from the `throw` to the handler,
   before the handler body runs. `capture_cxx_exception_text` (cpp:79) is `noexcept` and allocation free. The Lua raise comes only
   after the handler: `throw_error_from_buffer` (cpp:1604) -> `throw_error_impl` (cpp:1564): `handle_error` (traceback, error callbacks,
   `lua_settop(0)`) -> `luaL_error("%s")` (cpp:1596). Same `handle_error`, same single `luaL_error`, same Lua frame as before.
5. Proof on real Lua (host test, Mac-only): the guard is released (A/B), and OLD vs NEW give the same pcall status, error type,
   full text including traceback and stack balance for 4 message shapes x 4 call shapes plus the mod-side `pcall` (C). Error value is a string in both.
   luaL_error's position prefix (`chunk:line:`) is added in both, because both raise from the same C function frame.

Sites that are **not** TRY-wrapped:

- Site 17, `script_hook` line 8695 (`static auto script_hook`, 8610). It is a native hook callback, not a registered Lua function.
  Its wrapper is the per-callback `try { ... } catch (std::exception& e)` at 8744-8747, which logs
  `[script_hook] A callback threw an exception: <text>` and restores the Lua stack top. The throw is caught locally, so no Lua error is raised at all.
  Old behaviour was: C++ throw (with traceback text) when no Lua call is active (cpp:1569-1574, by reading), a longjmp over guard and catch when nested in one
  (host test F reproduces the nested case). Difference: the logged text no longer carries a traceback, and error callbacks are no longer notified. Acceptable.
  Reachability: registered only inside the eager detour-arm block (LuaMod.cpp:9692-9700). Console-proven absent on the B4 build:
  `decoded/ue4ss.decoded.log:780` says the custom-event script hooks "stay unregistered for this process lifetime". So site 17 is dead code on PS5 today.
- No registered-lambda site needs "unlock then raise". No raw `lua_CFunction` in the tree holds a lock and raises: the raw functions are
  `LuaPathShim.cpp` wrappers (they take `context.mutex` only in 7 short data-only helpers, none calls a Lua API; the file states "never call lua_error"),
  `memfile_*` (`luaL_checkudata`, no lock), `LuaFName/LuaFText/LuaUnrealString` lambdas, `protected_ufunction_call`, `custom_module_searcher`, `native_lua_*`
  (no lock). If a future raw function must raise under a lock, the only correct form is: do the work in an inner block that ends the guard, then raise
  after it (host test G shows the leaky and the correct form).

## 5. Per-site table: every raise or helper-raise inside a lock scope that the candidate has

All 17 sit in `std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex}`. "Covered" = fixed by the proposed diff.

| # | line | function (Lua name) | lock line | what raises | wrapper path | covered by proposed diff | fix in final-b5.diff |
|---|---|---|---|---|---|---|---|
| 1 | 2355 | UnregisterHook | 2346 | first arg not a string | TRY (registered 2345) | Y | `throw std::runtime_error` |
| 2 | 2366 | UnregisterHook | 2346 | no UFunction with that name | TRY | Y | same |
| 3 | 2373 | UnregisterHook | 2346 | PreCallbackId not integer | TRY | Y | same |
| 4 | 2379 | UnregisterHook | 2346 | PostCallbackId not integer | TRY | Y | same |
| 5 | 2385 | UnregisterHook | 2346 | PreCallbackId > int32 | TRY | Y | same |
| 6 | 2392 | UnregisterHook | 2346 | PostCallbackId > int32 | TRY | Y | same |
| 7 | 2435 | UnregisterHook | 2346 | hook kind not recognised | TRY | Y | same |
| 8 | 3071 | RegisterCustomEvent | 3063 | first arg not a string | TRY (registered 3062) | Y | same |
| 9 | 3081 | RegisterCustomEvent | 3063 | second arg not a function | TRY | Y | same |
| **10** | **3084** | RegisterCustomEvent | 3063 | `get_mod_ref` -> `lua.throw_error` at line 312 ("ModRef" global nil) | TRY | **N** | `get_mod_ref_cxx(lua)` (new static helper after line 319, same text) |
| 11 | 3114 | UnregisterCustomEvent | 3106 | first arg not a string | TRY (registered 3105) | Y | same as 1 |
| 12 | 5956 | RegisterHook | 5947 | first arg not a string | TRY (registered 5946) | Y | same |
| 13 | 5966 | RegisterHook | 5947 | second arg not a function | TRY | Y | same |
| **14** | **5969** | RegisterHook | 5947 | `get_mod_ref` -> `lua.throw_error` at line 312 | TRY | **N** | `get_mod_ref_cxx(lua)` |
| 15 | 5994 | RegisterHook | 5947 | no UFunction with that name | TRY | Y | same |
| 16 | 6043 | RegisterHook | 5947 | hook refused (neither native nor script: the B4 failure, HookProcessInternal=0) | TRY | Y | same |
| 17 | 8695 | script_hook (native) | 8612 | property type without a registered handler | local `catch (std::exception&)` 8744 (section 4) | Y | same |

Counts: 15 direct raises (all covered) + 2 helper raises (not covered, now fixed) = 17.

`get_mod_ref_cxx` is a new `static` function placed right after `get_mod_ref` (it is used at 3084 and 5969 only). The original `get_mod_ref`
(about 45 other callers, several outside registered functions, e.g. lines 490 and 8375) is left untouched: changing it to throw C++ would be unsafe
for callers that are not TRY-wrapped, which would then escape as an uncaught C++ exception.

## 6. Every lock scope in the whole candidate tree (task part 2)

`tools/scan_all_locks.py` and `tools/inventory.py` scanned `candidate/src` and `candidate/include` (41 files; `.orig`/`.rej` skipped) plus
`typescan-r4/include` (5 files, zero locks): **97 lock scopes, 0 scopes skipped**. Locks searched: `lock_guard`, `unique_lock`, `scoped_lock`, `shared_lock` on any mutex.
Manual `lock()/unlock()` on a mutex exists only in `FileDevice.hpp:110-115` (output device, no Lua). **Correction (round 8, review finding):** the earlier text also needs the `actions_lock()`/`actions_unlock()` pairs, which are manual lock calls on `m_thread_actions_mutex` in `LuaMod.cpp` (candidate lines 3827/3829 `ExecuteAsync`, 3854/3861 `ExecuteWithDelay`, 3885/3892 `LoopAsync`, 9731/9736 `process_delayed_actions`, 9790/9793 `clear_delayed_actions`). Each holds the lock only across `emplace_back` / `insert` / `clear` (allocation failure aside), and `get_mod_ref` / `make_ref` are called BEFORE `actions_lock()`, so none is a lock-then-raise site. The ONE gate models these pairs (they are the five `manual` rows in `gate/results-r8/b5-hard-list-locks.txt`). `generated_include` has no locks.
Full table: `inventory-base.md` (one row per scope). Its context column names the outermost registered Lua function, so the row for 2231-2241 says
"RegisterKeyBind / TRY-wrapped" although that scope is the inner keybind callback lambda, which runs natively on the UE4SS-UpdateThread. Summary of the base tree:

| class | scopes | what it means | action |
|---|---|---|---|
| SITE: raise in a registered Lua function | 4 scopes (UnregisterHook, RegisterCustomEvent, UnregisterCustomEvent, RegisterHook) = sites 1-16 | longjmp over guard | fixed in `final-b5.diff` |
| SITE: raise in native hook | 1 scope (script_hook, site 17) | longjmp only if nested in a Lua call | fixed in `final-b5.diff` |
| own pcall | 1 scope (`L23GameThreadProbe::Queue`, LuaMod.cpp:265-278) | the raise (`request`, `luaL_error` at line 256) happens inside the `lua_pcall` made in the same frame, so the longjmp lands inside the guard's scope; `catch(...)` around it | none |
| ambient, native | 3 more scopes (RegisterKeyBind callback 2231-2241, process_pending_notify 4761-4860, StaticConstructObject post callback 9007-9101) plus script_hook | stale `get_function_ref` (and `auto_construct_object`, `RemoteUnrealParam::construct`, property pushers) raise only on an invalid state, and are a longjmp only if the native code runs nested inside a Lua `pcall` | listed, optional diff for `get_function_ref` (section 7) |
| allocation only | 7 more scopes in the base tree (9 after the patch) | `luaL_ref`, `lua_newthread`, `lua_pushlstring` can raise LUA_ERRMEM only | listed, not fixable lexically |
| none | 81 | containers, flags, logging, `std::mutex` data helpers (LuaPathShim 7, UObjectArray 2, ClassListener 4, UnrealInitializer 6, ScriptHookThunks 9, UE4SSProgram 11, LuaUObject 3, LuaMadeSimple 5, String.hpp 2, FileDevice 1, plus 31 LuaMod.cpp delayed-action and drain scopes) | none |

Helpers examined for raising under a lock (the task asked for these by name):

- `get_mod_ref` (LuaMod.cpp:308): raises via `lua.throw_error` at 312. Called under a lock at 3084 and 5969 only. Its other failure, `get_userdata<LuaModRef>`, throws a C++ `std::runtime_error` (after `luaL_traceback`), which unwinds correctly. -> fixed.
- `LuaMadeSimple::Lua` accessors (`get_string/get_integer/get_number/get_bool/get_float`, `is_*`, `set_*`, `discard_value`, `get_table`, LuaMadeSimple.cpp:1170-1295): they use `lua_to*`/`lua_push*`/`lua_remove` and never call `throw_error` or `luaL_check*`. Not raisers (push of strings can only hit LUA_ERRMEM).
  The raising members are `Registry::get_*_ref` (cpp:659-715, only `get_function_ref` is used under locks), `Table::get_*_field` (hpp:407, cpp:832-901; only used in RegisterCustomProperty, no lock) and `for_each_in_table` (no lock). `Lua::call_function` throws a C++ `std::runtime_error` (cpp:1465), never a longjmp.
- `LuaFNameBridge::Bridge` (used under the lock in RegisterHook/UnregisterHook/RegisterCustomEvent): refuses without raising (header says "non-throwing"; no `lua_error`/`luaL_*` call in LuaFNameBridge.cpp). `get_hook_lua`/`make_hook_state`/`ensure_hook_thread_exists` (1215-1240): `lua_newthread` + `luaL_ref` only (alloc class).
- `L22Diagnostics::throw_lua_error` (see section 8).
- `lua_getglobal(L, "ModRef")` can run a `__index` metamethod if a mod installed a strict-mode `_G` metatable AND removed `ModRef`. Residual, extremely unlikely, same in `get_mod_ref_cxx`.

## 7. Uncovered by the proposed diff, and the minimal edit

| item | where | proposed edit | in final diff? |
|---|---|---|---|
| `get_mod_ref` raise under lock | 3084, 5969 | `get_mod_ref_cxx` | **yes** |
| stale `get_function_ref` under lock | 2234 (keybind callback, UE4SS-UpdateThread), 4824 (process_pending_notify), 8643 (script_hook), 9040 (StaticConstructObject callback) | `get_function_ref_cxx` helper (C++ throw, same stack effect) used at those 4 lines: `optional-ambient-getfuncref.diff`, 5 hunks, syntax-checked | **no, optional** |
| `auto_construct_object`, `RemoteUnrealParam::construct`, property pushers (`PusherParams::throw_error`) under the lock in 4825, 8646, 9041 | those lines | no lexical minimal edit (hundreds of raise sites in LuaUObject.cpp). Only the structural option below. | no |
| LUA_ERRMEM in `luaL_ref`/`lua_newthread` etc. under lock | 9 scopes | none | no |
| `NativeLuaActionDispatchScope` (LuaMod.cpp:4588-4612) holds the mutex as an object whose lifetime is the whole drain (constructed at 5032, 5470, 5554, 5637) | `process_event_hook`, `funcptr_marshal_hook`, `VmtEngineTickDrain`, `engine_tick_hook` | none needed for registered functions: the drain calls Lua through `call_function` (pcall) so a Lua error lands inside the scope. Only a raise made by the drain's OWN C++ frames (stale ref in `process_simple_actions` 4720, etc.) matters, and only when nested in a Lua call. Same ambient class. | no |

Why the ambient class is low priority (Mac-only reasoning):
- 8612 (script_hook) and 9007 (StaticConstructObject callback) are unreachable on the B4 build: console-proven by `decoded/ue4ss.decoded.log:780` (hooks "stay unregistered for this process lifetime").
- 2231 runs on the UpdateThread. With no Lua call active on that state, `throw_error_impl` throws C++ (cpp:1569-1574) and the local `catch (std::runtime_error&)` handles it.
- 4761 runs on the game thread from the tick drain, not from inside Lua.
- They need an invalid state (stale registry ref or unsupported property type) AND a nested Lua call. Nothing in the B4 log shows either.

Structural alternative (VERIFY.md item 1: thread_local depth, `throw_error_impl` throws C++ while depth > 0). Not recommended for B5:
the C++ exception would then also be thrown when a deeper registered function (called from Lua called from the locked scope) raises, so it would escape
`process_lua_function` into Lua's C frames. It needs the depth saved/zeroed/restored around every `lua_pcall` (`Lua::call_function`, TRY) and cleared on any
Lua-internal LUA_ERRMEM longjmp, which touches the B2 wrapper that other tracks own. Higher risk than 17 explicit lines.

## 8. L22Diagnostics::throw_lua_error (task part 4)

Definition: `outputs/2026-10-08-l22-runtime/diagnostics/include/L22Diagnostics/Logging.hpp:26-30` (same sha a16f6583... in both l22-package-r2 copies; not inside the candidate tree):
`error(context, message); lua.throw_error(message);`, where `error()` is `noexcept` and does `Output::send<LogLevel::Error>("[l22] {}: {}\n")`.

- It is used in `LuaMod.cpp` only in RegisterKeyBind (2216, 2222, 2280, 2286, 2327) and in `LuaFText.cpp` (128, 155, 180). **None is inside a lock scope** (RegisterKeyBind's lock is in the inner callback lambda, 2231, which contains no raise except the ambient `get_function_ref`).
- The 17 changed lines were all plain `lua.throw_error(...)` with no logging, so the diff removes no log line and adds none. `final-b5.diff` does not touch any `L22Diagnostics::` line (checked: no `L22Diagnostics` in any `+`/`-` line).
- Trap to remember: lambdas shaped `try { ... } catch (const std::exception& e) { L22Diagnostics::error(name, e.what()); throw; }` (RegisterKeyBind 2203, ExecuteInGameThread 6077 ...) would now log one extra `[l22] <Name>: <msg>` line for a C++ throw that used to longjmp past the catch. None of the 17 sites is inside such a lambda. If a `throw_lua_error` site is ever converted, the equivalent is `L22Diagnostics::error(ctx, msg); throw std::runtime_error(msg);` placed before the throw.
- The optional refusal-log diff (`proposed-B5-optional-refusal-log.diff`, a bounded Warning before the `throw` at the two RegisterHook refusal sites) applies on top of `final-b5.diff` (patch rc 0, line offset only, no reject) and the combination passes the same `-fsyntax-only` check (0 errors, 23 warnings). It is not part of `final-b5.diff`.

## 9. Differences the mod or the log can see

| item | OLD | NEW | impact |
|---|---|---|---|
| Lua error type, status, position prefix, traceback | string, LUA_ERRRUN | identical (host test C, 20 combinations) | none |
| message length | up to 4095 | up to **1023** (TRY buffer, hpp:846); host test E | longest real message here is about 400 bytes (RegisterHook refusal text plus the function name, which would have to be 600+ characters to hit the cap). A one-line change to a larger TRY buffer would remove it (B2 file, not mine). |
| `[MEM] ... cxx=` counter | unchanged | +1 per refused call (TRY counts C++ exceptions, cpp:79-82); `errs=` +1 in both | next launch proof signal, see section 11 |
| stack | cleared by `handle_error` | same | none |
| leaked heap | `std::string` locals and the vector `hook_lua` stack leak on longjmp | unwound | better |
| site 17 log text | traceback appended | no traceback | cosmetic |

Patched file shas: `final-b5` `afaf5caa3f093f996e5c5d5f9682ce5415821f6ccc99b0b2d658b4c8e1983550`,
`final-b5-plus-optional` `50528f24f97a27e3c4c7b818d8881619ff56c2ca9645cdbe4878bdf187a9280a` (re-derived by applying the diffs to a fresh copy of the candidate file).

## 10. Gates and tests (all counts printed; each exits non-zero on zero checked)

| check | result |
|---|---|
| **round 8: the ONE gate** (`../gate/lock_raise_gate.py`, hard mode `--helpers off --unprotected-calls --metamethod-calls`) on candidate `LuaMod.cpp` | FAIL: 56 lock scopes, **17 findings** (14 direct + 2 banned `get_mod_ref` + 1 lambda); on `patched/final-b5` and `final-b5-plus-optional`: PASS, 0 findings (output at the end of `CHECK-RESULTS.txt`). Full compiled-set runs, the 18-check `run_gates.sh` and the mutants are in `../gate/GATE.md` |
| round 7 (superseded): `tools/lock_raise_gate_v2.py` (old regex gate) on the candidate tree | FAIL: files 41, scopes 97, **FAIL=17** (15 raises + 2 `get_mod_ref`), WARN=7 |
| same on `patched/final-b5` LuaMod.cpp | PASS: files 1, scopes 47, FAIL=0, WARN=7 |
| same on `patched/final-b5-plus-optional` | PASS: scopes 47, FAIL=0, WARN=3 (`auto_construct_object` x2, `construct` x1) |
| same with zero files | FAIL (nothing scanned), exit 1 |
| their `lock_raise_gate.py` on base / final | FAIL findings=15 / PASS findings=0, scopes 46 (it ignores non-`m_thread_actions_mutex` locks and helpers, so it cannot see sites 10 and 14) |
| `no_detour_gate.py` base vs final | PASS both, 0 detour sites, 4 notes |
| `fs_throw_gate.py` base vs final | identical: 1 file, 26 call sites, 12 pre-existing violations (baseline debt), nothing new |
| include lines | identical |
| prospero-clang++ `-fsyntax-only` of LuaMod.cpp (archived l24 argv, single TU, no object, not a build) | base, final-b5, final-b5+optional: exit 0, 0 errors, same 23 warnings (text identical) |
| host test (real Lua 5.4.7 as C, `hosttest/RESULT.txt`) | 32 checks, 0 failures: OLD leaks the mutex, NEW frees it over 1000 calls, error text identical, get_mod_ref helper leak reproduced and fixed, nested native hook, raw CFunction forms, truncation difference |

The host test uses line-for-line copies of the B2 TRY / `throw_error_impl` shapes, not the PS5 binary. It omits `throw_error_impl`'s
"no Lua call active -> throw C++" branch (needs Lua internals).

Not checked: the PRX build and its gates (include-graph gate, verify_build) are the build owner's. C++ exception unwinding through these
exact frames on the PS5 PRX is **not console-proven**: the B4 log's last `[MEM]` rows show `errs=0 cxx=0` (decoded:811 and :917), i.e. no C++-to-Lua
conversion happened on console before the stall. The code base already relies on C++ throw/catch across the same frames (for example the `throw` in
`ensure_process_event_hooked`, caught by the TRY inside ExecuteInGameThread), but the first console proof will be the next launch.
Circumstantial hint only (earlier session, **not** evidence for this verdict): `evidence/acceptance-pass2-20261012.decoded.log:1705` shows `[l22] ExecuteInG: std::ba` (a caught `std::bad_alloc` text, truncated by the log), i.e. some C++ exception was caught on the console in an earlier build. B5 unwinding through the TRY frames is still unproven until the next launch shows `cxx=` rising.

## 11. Next-launch proof (what to look for, console-proven only if present in that launch's log)

1. More than one `keybind callback entry` line (B4 had exactly 1, decoded:2410). Several F7/F12 presses should each log one.
2. After the minimap's three refused RegisterHook calls: `[MEM] ... cxx=` increases by 3 (or the mod's pcall prints the refusal text) and
   the next `ExecuteInGameThread queued` lines keep appearing from the async thread. Pre-fix signature to check is absent: `[zz] [Q] try` with no `ok` after the pawn-class GetFullName line.
3. Self-test starts (`[ST2] AUTO start` followed by queued lines).
4. If a single keybind entry still appears after B5, the next suspects are the ambient list in section 7 and the LUA_ERRMEM scopes.

## 12. Decision for the build

Build `final-b5.diff` only (one labelled gated build per fix). Leave `optional-ambient-getfuncref.diff` out of B5: it covers a state that needs a nested
Lua call plus a stale reference, it is unreachable on PS5 for 2 of its 4 sites, and it would not close the whole ambient class. Gate v2 should
replace `lock_raise_gate.py` in the pre-compile gate set, with `get_mod_ref` kept in its banned list.
**Round 8:** that is now done inside the single hardened gate (`../gate/lock_raise_gate.py`): `get_mod_ref` is in its built-in ban list and fails in every mode, including `--helpers off`, so a revert of `get_mod_ref_cxx` back to `get_mod_ref` (the review's mutant) is caught; the B6 lock wrapper is a built-in guard type, so B5's checks no longer lose scopes when B6 is applied.
