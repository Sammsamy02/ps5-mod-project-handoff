# Review of the lua-as-cpp track, round 2 (adversarial, 2026-10-12)

Every claim in this review is **Mac-only**. I made no console contact, read no console logs and built
no PRX. I wrote only this file in the worktree. My re-runs went to my session scratch folder, which is
temporary, so this file is the record. The round-1 review is kept word for word at the end (it was
the earlier content of this file, sha256 `e313382c…`).

## Plain verdict

- **Round 2 holds up. I could not refute it.** Every check I re-ran gave the same result as the
  worker's, and the round-1 review findings are fixed.
- **"Partial" is the right status.** The open crash risk (H4: a Lua error inside a hook that the game
  called) is real, and the worker says so clearly. Nothing should be built with Lua-as-C++ until H4
  has its own fix and review.
- I found **no blocker and nothing major**. There are 6 small problems (below). One is a planning
  rule: the proposed build "B5b" puts two fixes into one build.

## What I re-checked (Mac-only)

| Check | My result |
|---|---|
| Host test `run_v2.sh` (copy in scratch, one compile at a time, memory check OK 4.3 GB free) | rc=0, 90 checks, `PASS run_v2 (all three builds)`. C build: 0/1000 destructors, 2000 blocks leaked. C++ builds: 1000/1000, 0 leaked. Replica A/B bugs reproduce on cpp/cppcabi; the fixed versions match the C build. The C caller fails to link on `cpp` with the same 8 names and links on `cppcabi`. Same 159 public names. |
| Old B2 shape (luaL_error inside the catch) | Covered by the host test check "B2 OLD wrapper": it leaks 1000/1000 with Lua as C and leaks nothing with Lua as C++. |
| PS5 compile of `ldo.c` with the probe's exact argv (one compile) | rc=0, 0 stderr bytes, **byte-identical** to the worker's object (sha `4c5c5864…`). `LuaLock`/`LuaUnlock` keep C names. New refs are only the C++ EH set. |
| Lua source identity | All 33 probe units have the same source path and sha as the B4 receipt (0 differences). `.d` files: Lua headers resolve to the same paths as the C build. The only new rows are the prefix and libc++ wrapper headers. |
| Defined globals C vs C++ (33 objects, llvm-nm) | Internal `luaD_*`/`luaH_*` names become mangled (188 names). The exact-name check covers that nothing outside Lua uses them. A grep of the candidate tree and harvest tools finds no C-name use of Lua internals. |
| Packaging stays 207 | `import_references.S` lists `longjmp`/`setjmp`, and `import_references.o` references both (`llvm-nm -u`). `memory_lua.o` references exactly the 8 Lua names claimed. |
| Unwinder scope (H4 premise) | `runtime-l26-A.o` has `U __eh_frame_hdr_start` and `U __eh_frame_start`, and no `dl_iterate_phdr`. So the unwinder only knows the PRX's own frames, as the worker says. |
| Patches | Both `git apply --check` clean, not applied (`l26-runtime/lua-c-abi/` does not exist). `build_variants.py` sha `60408cef…` and the 4 runtime base shas are unchanged. The prefix header extracted from patch 0001 has sha `96b3f261…`, the same as the host-test copy. |
| Selection harness (scratch copy, read-only) | OFF: 205 units, 0 luaraw, no failed gates, PASS. ON: G-LUACPP-scope True (33 / 4,035 / sha ok), ON = OFF (205) + 33, PASS. It wrote nothing in the repo. |
| Other catch(...) sites | My own scan of all 319 B4 C++ unit sources plus the candidate tree found no non-rethrowing catch(...) that calls Lua and is missing from the v2 audit. |
| Rule scan of the worker folder | No console address or ports, no `/tmp` evidence, no Epic source, no `__pycache__`. No file outside the folder was written by this track as far as I can see. The worker says it did not write `harvest/launch_report.py`, and I cannot attribute that file to it. |

## Problems found

| # | Severity | Problem | Evidence |
|---|---|---|---|
| 1 | minor | The proposed **B5b = B5a + H4 fix + 0001** puts two fixes into one build. CLAUDE.md §4 says "One labelled, gated build per fix". The H4 fix (a pcall around every engine-to-Lua entry point) also changes the C build: hook errors would be caught instead of longjmp/panic. So it is its own fix and needs its own bisect point. Suggested order: B5a = 0002 only; B5b = B5a + H4 fix (Lua still C); B5c = B5b + 0001. | `FEASIBILITY.md` "Next steps" 3; report `decisions_needed[2]`. |
| 2 | minor | The include-graph exemption accepts **any** of the 4,035 BASE-pinned dependency rows for the 33 Lua units, if its path and sha match. It is not limited to the 403 rows the probe actually saw. This widens a gate the rules call "refuses new unapproved includes". It is recorded only in `risks` and is not one of the decisions for the user. A tighter version would pin the exact (path, sha) list from probe v2. | `0001` patch hunk `@@ -927,6 +983,11 @@` (`base_dep_pins` built from `universe_res` ∩ `oldsel['source_pins']`); harness output `base_pinned_dependencies: 4035`. |
| 3 | minor | The H1 text says all six patched handlers are "reached from Lua, with no game code between them and their Lua handler". That is not true for `LuaMadeSimple.cpp:1459` (inside `Lua::call_function`, which engine hooks call directly) or for `:1587` when `throw_error_impl` is reached from a hook (for example `LuaMod.cpp:8696` `lua.throw_error` inside `script_hook`). The outcome does not change: the code after the handler raises a Lua error along the same path anyway, and :1459 cannot see a Lua error (`query_lua_memory` only calls `lua_gc(LUA_GCCOUNT)`, `LuaMadeSimple.cpp:446-461`). The sentence is still too broad. | `FEASIBILITY.md` §5 H1 "Fixed" bullet; `LuaMadeSimple.cpp:1439-1462`, `:1563-1597`. |
| 4 | minor | The plain verdict says "Today it jumps over the game code instead". A non-programmer could read that as "today is safe". In fact today's longjmp also skips the game frames' cleanup, and the no-handler and cross-thread cases are fatal in both builds (H5). | `FEASIBILITY.md` Verdict, last bullet. |
| 5 | minor | Task item (2) asked for the list of callers and how each one includes `lua.h`. FEASIBILITY gives one line ("callers keep `<lua.hpp>`"), and the 39 consumer objects appear only inside `PROBE-RESULTS.json`. It does not say that `LuaMadeSimple.cpp:12` includes the Lua **internal** header `<lstate.h>` and reads `lua_State::errorJmp` / `l_G->mainthread` (`:1567`), so it depends on Lua's internal struct layout. Under the staged design that layout does not change (nothing in `lstate.h` depends on `__cplusplus`), but the feasibility answer should say so. | `grep` of the candidate: `LuaMadeSimple.hpp:8`, `LuaUnrealString.hpp:5`, `LuaPathShim.cpp:63` (`<lua.hpp>`); `LuaMadeSimple.cpp:12` (`<lstate.h>`). |
| 6 | minor | The 33 PS5-toolchain probe compiles ran without first running `fs_throw_gate.py` / `no_detour_gate.py` (CLAUDE.md §4: "Gates run before compile"). The objects are analysis-only, the Lua sources are unchanged, and no PRX was built, so the risk is small. Still, the report does not mention the rule. The gates were also not run on the tree patched by 0002 (the worker says so). | `ps5-xcompile-probe-v2/probe2-run.log` (33 compiles); report `tests` "Not run: … fs_throw_gate.py, no_detour_gate.py". |

Not counted as problems (the worker already disclosed them): H3 and H4 are open; patch 0002 has not
been compiled for the PS5; it is unknown whether C++ throws are caught on the console (this now
covers every coroutine yield); the evidence is not yet copied into `outputs/2026-10-08-l25-prep/evidence/`
(the worker could not write there); the worker ran as one agent, not several.

## Next steps I agree with

1. Mac: design the H4 engine-boundary pcall and audit every engine-to-Lua entry point (hooks,
   tick, delayed and async actions, NotifyOnNewObject). Decide H3 (TRY on the 26 direct registrations).
2. Mac: compile the 4 files changed by 0002 for the PS5, one at a time. Run fs_throw_gate.py and
   no_detour_gate.py on that tree. Pin the exact include-exemption rows (#2).
3. Then one labelled, gated build per fix, each with the user's own OK (#1).

---

## Archived: round-1 review (verbatim, previous content of this file)

# Review of the lua-as-cpp track (adversarial, 2026-10-12)

Every claim here is **Mac-only**. I made no console contact, read no console logs and built no PRX.
The only file I wrote in the worktree is this one. My re-runs were done in my session scratch folder,
which is temporary. That is why the replica sources and their outputs are copied into this file below:
this file is the durable record.

## Plain verdict

- **The main finding holds (Mac-only).** When Lua is built as C++, C++ cleanup runs when a Lua error
  passes through. I re-ran the worker's host test and got 17 + 17 PASS, with the same leak numbers.
- **The safety claim is wrong.** The worker says no code mishandles a Lua error in normal play. I
  found two places that do:
  1. **Every Lua error during a UFunction call** (for example, a mod passing the wrong number of
     arguments) loses its message and returns a status Lua does not use (-1). This is
     `LuaUObject.cpp`, PS5-only code that was written for the current C build.
  2. **When memory runs out while an error message is being built**, Lua's global lock stays held.
     Every other thread that uses Lua then waits forever (a hang). This is `LuaMadeSimple.cpp`, and
     it happens under memory pressure, which is exactly the situation this work is about.
- So "B5 = B4 + this build change only" is **not ready**. B5 also needs runtime code changes in
  those two files. The packager change is also still missing, as the worker said.

## Problems found

| # | Severity | Problem | Evidence |
|---|---|---|---|
| 1 | blocker | Missed H1 site, reached in normal play: `protected_ufunction_call` swallows every Lua error raised during a UFunction call | `typescan-r4/candidate/src/LuaUObject.cpp:135-148` (`catch (...)` at :142 stores `std::current_exception()` and returns 0). Its try block calls `call_ufunction_from_lua_impl`, which raises with `lua.throw_error` at 11 or more places (e.g. :158, :267 "UFunction expected {} parameters, received {}"). :475-477 rethrows the swallowed exception outside the inner pcall. The audit lists this site with `lua_calls_in_try: []` (`audit/CATCH-ALL-AUDIT.json`, LuaUObject.cpp line 142), because its regex does not match `call_ufunction_from_lua_impl`. Replica A below: C build 1000/1000 `LUA_ERRRUN` with the message kept; C++ build 0/1000, status **-1**, message `<non-string error object>`, Lua-side `pcall` err = `function: 0x...`. The unit is compiled in B4 (`lua-luauobject` -> this file). This contradicts FEASIBILITY §5 ("none can see a Lua error in normal play") and the verdict line ("The audit found no place that does this in normal play"). The fix is not a simple `throw;`: rethrowing out of this C function sends the exception into Lua's `catch(...)`, which is H3 (abort). Outside ldo.c, code cannot catch `lua_longjmp*` by type either: it is an incomplete type (lstate.h:122). The fix has to be designed. |
| 2 | major | Missed H1 site that causes a hang under memory pressure: the B2 `catch (...)` in `throw_error_impl` swallows a Lua out-of-memory error and leaks Lua's process-wide lock | `typescan-r4/candidate/src/LuaMadeSimple/LuaMadeSimple.cpp:1576-1592`: `try { handle_error(...) } catch (...) { raw text }`, then `luaL_error`. `handle_error` calls `luaL_traceback` (:1536), which raises `LUA_ERRMEM` when Lua's allocator fails. All bindings raise errors through this path. Replica B below: C build status 4, lock depth 0; C++ build swallowed=1, status 2, **lock depth 1, other-thread-can-lock=0**. The audit regex missed `handle_error`. |
| 3 | major | The staged patch is not yet a complete build change | Disclosed by the worker: the packager contract still expects `setjmp`/`longjmp` (`2026-10-08-l26-package-A/package_prx_l19.py:843-862` would report `missing=[longjmp, setjmp]` and gap; `check_import_identity.py:52` requires set equality; pinned `.superpowers/.../native-full-runtime-aligned-entry-1/import-contract.json` lists both, at lines 5412 and 10029). Even so, `decisions_needed` asks for the B5 OK now. Combined with #1 and #2, B5 is a build change plus runtime code changes, not a build change alone. |
| 4 | minor | Receipts would misreport the import change under the opt-in (not covered by the patch) | `build_variants.py:1039` hard-codes `'actual_undefined_added':[],'actual_undefined_removed':[]` in COMPOSITION-RECEIPT, and the plan-mode G7 record at :1013 still says "llvm-nm -u equality with BASE enforced at execute". With `L26_LUA_AS_CPP=1` both statements would be false. The new include-graph exemption (patch hunk at :978) also records nothing about which rows it approved, or how many. |
| 5 | minor | H3 is described too narrowly | Any C++ exception, std or not, that escapes a `lua_CFunction` without TRY reaches Lua's `catch(...)` and causes the unbalanced unlock. My scan of the B4 sources found 26 direct `lua_pushcfunction`/`lua_pushcclosure` registrations (23 distinct functions) that bypass `process_lua_function`'s TRY. Examples: `pcall_error_handler`, which builds a `std::string` and can throw `bad_alloc`, `memfile_*`, `custom_module_searcher` and `protected_ufunction_call`. The proposed mitigation ("TRY also converts non-std exceptions") does not cover these. The C build is also broken here (the exception flies past `lua_pcall`), so this is minor. |
| 6 | minor | Coroutine yields are not considered | In Lua 5.4, `coroutine.yield` is `luaD_throw(L, LUA_YIELD)` (`LuaRaw/src/ldo.c:898`, reached from `luaB_yield`, lcorolib.c:112-113). In the C++ build every yield is therefore a C++ throw. Replica C: 0.545 vs 4.682 µs per yield. So the open question "are throws caught on the PS5?" applies to every mod that uses coroutines in normal play, not only to errors. The cost note ("mods that use errors heavily") understates this. |
| 7 | minor | The review of LuaMod.cpp:8641-8752 is inaccurate | The try block contains `lua.throw_error` at :8696 (unsupported hooked-param type), outside `call_function`, so it is not limited to out-of-memory pushes. If the hook fires again while the same state is running (errorJmp set), the `catch (...)` at :8748 swallows the error in the C++ build and the lock leaks. This should be rare. |
| 8 | minor | False "fails on zero" claim | README: "Every script prints how many items it checked and exits non-zero on a zero count." `staged-patch/selection-check/selection_harness.py:9-17` never exits non-zero: it prints `failed gates` and returns 0, and the OFF run shows 0/0/0 counts. |
| 9 | minor | Cited numbers are not in the evidence file | FEASIBILITY §5 gives 0.71 / 8.22 µs per error. `host-test/RESULT.txt` (sha `53b5db6d…`) says 0.73 / 6.87 µs, and my re-run gave 0.72 / 7.70 µs. Timings vary from run to run, so the report should cite the file. |
| 10 | minor | The "Yes" is wider than the evidence | The task asked whether this fixes the **PS5 memory growth**. The evidence shows that one leak mechanism is removed on the Mac. Nothing here links the PS5 growth rate to the Lua error rate (for example, B2 `[MEM] raised_errors` against growth). The plain summary should say "removes one leak mechanism; whether it causes the growth is unknown". |
| 11 | minor | "78/78 resolve" is a name-only match | `ps5-xcompile-probe/probe.py:96-98` and the LINK section compare demangled base names. The 51 caller units were not compiled with `-DLUA_COMPILED_AS_CPP`, so the real mangled references were not checked. A mismatch would still fail closed at G7, so this is minor. |

No rule violations found: no console contact, no writes outside the allowed folder, no edits to
protected files, no game- or mod-specific runtime code, no evidence in /tmp, no console IP, no Unreal
or Epic source copied.

## What held up (re-checked)

- All 20 listed files exist (20 checked, 0 missing). 225 files in total, 3.5 MB.
- Protected files are unchanged. `PROTECTED-FINGERPRINT-before.json`: 646 checked, 0 changed,
  0 missing. `build_variants.py` is `60408cef…` and `l26_packaging_driver.py` is `2bf49e0d…`, the
  base shas the patch states. Their mtimes (Oct 11 22:20) and `__pycache__` are from before the
  worker ran. Overnight `host-tests/b2-error-wrapper/` is untouched (Oct 11 22:31).
- `git apply --check` on the staged patch is clean, and the patch is not applied.
- Upstream option, checked from the source: `LuaRaw/CMakeLists.txt:7` (OFF by default), `:49-51`,
  `:68`, `:78`, `:87`; `xmake.lua:1-5,11-12,30-36`; `ldo.c:55-61`; `lua.hpp:7-15`. Lua is 5.4.7
  (`lua.h:18-20`).
- In B4, the 33 `luaraw-*` units are `retained_exact_accepted_or_admitted_TU`, built with
  `prospero-clang -std=c11 -Os … -resource-dir …`. They are the only units with `-resource-dir`.
  All 319 C++ units use `-std=c++23`. `prospero-clang++:83` adds `-frtti -fexceptions`.
  `runtime-l26-A.o` has sha `732aef21…`, the same as pinned in the B4 FINAL receipt.
- I scanned all 395 B4 link inputs with llvm-nm: only `luaraw-ldo.o` references `setjmp`/`longjmp`.
- Probe: 33 objects, 0 bytes of stderr. I re-derived from the probe objects: 95 external references
  and 0 new imports. All 7 EH references (`__cxa_throw`, `__gxx_personality_v0`,
  `_ZTVN10__cxxabiv119__pointer_type_infoE`, …) are defined in `runtime-l26-A.o`. The 33 probe
  objects have no `.init_array`/`.fini_array`, so packaging gate P8 is unaffected. The include gate
  would refuse 108 rows, all under `include/c++/v1` (`__config`/`__config_site` ×33, …).
- The B4 PRX has `.eh_frame` 0x6d8c30, `.eh_frame_hdr` 0x1ce8c4 and `.gcc_except_table`. The four
  `__eh_frame*` symbols are in `l26-A/pass1/undefined.stdout`.
- The patch's compile path: `rewrite_compile` (`build_runtime.py:13-23`) redirects `-o`/`-MF` into
  the variant dir, so the prebuilt `.superpowers/.../coherent-full-1/luaraw-*.o` files are not
  overwritten.
- noexcept: only `LuaMod.cpp:179` and `:261` take `lua_State`. I agree that both are safe (all of
  their Lua work runs inside pcall).
- Destructors that call the Lua API directly: only `LuaMadeSimple.cpp:642` (`lua_close`) and
  `LuaUObject.cpp:110` (`luaL_unref`, which leaves the stack unchanged). This is low risk.

## Replicas (Mac-only; linked against the worker's own `host-test/build-c` / `build-cpp` objects, read-only; one compile at a time with memcheck before each)

### A: `LuaUObject.cpp:135-148` + `:463-485` shape

```cpp
struct Invocation { std::exception_ptr Pending{}; ~Invocation() { ++g_inv_dtor; } };
static int impl(lua_State* L) { return luaL_error(L, "%s", "UFunction expected 2 parameters, received 1"); }
static int protected_ufunction_call(lua_State* L) {
    auto* inv = static_cast<Invocation*>(lua_touserdata(L, lua_upvalueindex(1)));
    try { return impl(L); }
    catch (...) { ++g_swallowed; inv->Pending = std::current_exception(); return 0; } }
static int call_ufunction_from_lua(lua_State* L) {
    const int n = lua_gettop(L); int status;
    { Invocation inv; lua_pushlightuserdata(L, &inv); lua_pushcclosure(L, protected_ufunction_call, 1);
      lua_insert(L, 1); status = lua_pcall(L, n, LUA_MULTRET, 0);
      if (inv.Pending) std::rethrow_exception(inv.Pending); }
    if (status != LUA_OK) return lua_error(L);
    return lua_gettop(L); }
static int binding(lua_State* L) {                 // process_lua_function TRY shape
    char msg[1024]; msg[0] = 0;
    try { return call_ufunction_from_lua(L); }
    catch (std::exception& e) { std::snprintf(msg, sizeof msg, "%s", e.what()); }
    return luaL_error(L, "%s", msg); }
// main: 1000 x lua_pcall(binding, 1 arg); then luaL_dostring("local ok, err = pcall(CallFunction, 7) ...")
```

```
mode=C errors=1000 status==LUA_ERRRUN:1000/1000 message-kept:1000/1000 last_status=2 last_msg='UFunction expected 2 parameters, received 1'
mode=C catch(...) in protected_ufunction_call swallowed:0  Invocation dtors:1001  lock depth after:0  other-thread-can-lock:1
mode=C Lua-side pcall(CallFunction): dostring rc=0 err='UFunction expected 2 parameters, received 1'
mode=C++ errors=1000 status==LUA_ERRRUN:0/1000 message-kept:0/1000 last_status=-1 last_msg='<non-string error object>'
mode=C++ catch(...) in protected_ufunction_call swallowed:1001  Invocation dtors:1001  lock depth after:0  other-thread-can-lock:1
mode=C++ Lua-side pcall(CallFunction): dostring rc=0 err='function: 0x1026d8b28'
```

### B: `LuaMadeSimple.cpp:1576-1592` shape, with Lua's allocator failing inside `luaL_traceback`

```cpp
static bool g_fail = false;
static void* alloc(void*, void* p, size_t, size_t n) { if (n == 0) { std::free(p); return nullptr; }
                                                        if (g_fail) return nullptr; return std::realloc(p, n); }
static int throw_error_impl_shape(lua_State* L) {
    char final_text[256]; std::snprintf(final_text, sizeof final_text, "%s", "raw error text");
    try { g_fail = true; luaL_traceback(L, L, "UFunction expected 2 parameters, received 1", 0); g_fail = false; }
    catch (...) { g_fail = false; ++g_swallowed; }
    g_fail = false;
    return luaL_error(L, "%s", final_text); }
// main: L = lua_newstate(alloc, nullptr); lua_pcall(throw_error_impl_shape); print status, lock depth; _Exit(0)
```

```
mode=C pcall status=4 (2=ERRRUN 4=ERRMEM) msg='not enough memory' catch(...) swallowed=0 lock depth after=0 other-thread-can-lock=1
mode=C++ pcall status=2 (2=ERRRUN 4=ERRMEM) msg='raw error text' catch(...) swallowed=1 lock depth after=1 other-thread-can-lock=0
```

### C: coroutine yield cost (`coroutine.wrap(function() while true do coroutine.yield() end end)`, 100,000 resumes)

```
mode=C rc=0 100000 yields: 0.545 us per yield
mode=C++ rc=0 100000 yields: 4.682 us per yield
```

### Worker host test re-run (copy of `host-test/run.sh` with `WHAT` pinned, run from scratch)

`PASS both builds (17 + 17 checks)`. C: OLD wrapper 1000 never released; frames 0/1000 destructors
run, 2000 blocks leaked, 0.72 µs; caller-frame 1000 leaked. C++: OLD wrapper 0 never released;
frames 1000/1000 destructors run, 0 leaked, 7.70 µs; caller-frame 0 leaked; all three hazard
checks behave as the worker described.

## What the next step needs (Mac-only, before any B5 OK)

1. Design the C++-mode handling for `protected_ufunction_call`. When Lua is C++, the inner pcall
   wrapper may simply be unneeded. Then fix the `catch (...)` in `throw_error_impl` so that it never
   swallows a Lua error. Prove both with replicas A and B: they must give the C-build results.
2. Do a call-graph pass over all 131 non-rethrowing `catch (...)` sites, matching callees that take
   the Lua state as an argument. My quick pass of that kind is what found #1 and #2: only 5 sites
   matched, and the other 3 are safe (`native_lua_chunk`, `native_lua_module` and
   `query_lua_memory` do not raise).
3. Stage the packager contract change and the receipt fields (#3, #4). Then do one labelled,
   gated build.
