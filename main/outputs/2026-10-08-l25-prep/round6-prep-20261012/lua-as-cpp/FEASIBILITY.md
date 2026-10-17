# Compile Lua as C++: feasibility (lua-as-cpp track, round 2, 2026-10-12)

All of this is **Mac-only**. I made no console contact, read no console logs and built no PRX.
"Unknown" means I did not check it. Round 2 fixes what the review found (`REVIEW.md`). It also
fixes two errors I found myself in round 1. The round-1 text is replaced, and the round-1 files are
kept as history (see README).

## Verdict (plain)

- **It removes one cause of the memory growth, on the Mac.** When Lua is built as C++, our cleanup
  code runs every time a Lua error passes through it: 1,000 of 1,000 times, and 0 of 2,000 memory
  pieces are left behind. **Unknown:** how much of the PS5 growth comes from this. Nobody has yet
  compared the console's growth with its error count (the B2 `[MEM]` lines).
- **It is not ready for a build yet. Status: partial.** Round 2 found and fixed these:
  1. **Two places in our own code broke in the C++ build** (from the review). The fix is staged and
     proven on the Mac: all three Lua builds now give the same correct result.
  2. **Upstream's way of doing this would break the link.** A frozen plain-C packaging file calls 8
     Lua functions by their C names. So the staged build keeps the C names: only the 33 Lua files
     change, and no other file is touched or recompiled.
  3. **The packaging import count stays 207** (round 1 wrongly said 205, which would have failed
     packaging). That packaging change is dropped. The packager needs no change.
- **Still open (why "partial"):** a Lua error raised inside a hook callback that the game called
  while a Lua mod was running would now have to unwind through game code. On the PS5 that most
  likely crashes. Today it jumps over the game code instead. This needs one more design step
  (H4 below) before any build.

## 1. How the l26 pipeline compiles LuaRaw today (unchanged from round 1, re-checked)

- 33 `luaraw-*` units, all **retained** prebuilt objects (B4 receipt
  `outputs/2026-10-08-l26-build-variants/l26-A/pass1/COMPOSITION-RECEIPT.json`). They are built with
  `prospero-clang -std=c11 -Os -fPIC -ffunction-sections -fdata-sections -D_XOPEN_SOURCE=700 -pthread
  -resource-dir …`, with no `LUA_USE_*` define, so `ldo.c:63-75` uses ISO `setjmp`/`longjmp`.
- Of the 395 runtime link inputs, only `luaraw-ldo.o` references `setjmp`/`longjmp` (llvm-nm, round 1,
  re-confirmed by the reviewer).

## 2. The exact change (round 2 design: C++ code, C names)

**Round-1 design refuted by me (new finding, Mac-only).** Upstream's `LUA_COMPILED_AS_CPP`
(`LuaRaw/CMakeLists.txt:7,49-51,78`; `xmake.lua:1-5,30-36`) gives every Lua function a C++ name. But
the frozen packaging bootstrap unit `lineage/probe/memory_lua.c` is **plain C**. It includes `<lua.h>`
(`memory_lua.c:3-4`) and calls 8 Lua functions by C name. `llvm-nm -u
2026-10-08-l26-package-A/pass-1/build/memory_lua.o` lists `luaL_loadbufferx luaL_newstate lua_close
lua_isinteger lua_pcallk lua_tointegerx lua_tolstring lua_type`. Round 1 scanned only the 395
runtime inputs and missed this object. The host test reproduces the effect: with C++ names, a C
caller does not link ("8 undefined", `host-test-v2/RESULT-V2.txt`, `[cpp] c_caller`).

**Staged design.** The 33 Lua units are compiled as C++ (so `ldo.c:55-61` makes Lua errors and
yields C++ `throw`s), and every public Lua symbol keeps its C name:
- `prospero-clang` -> `prospero-clang++` (adds `-frtti -fexceptions`, `prospero-clang++:83`);
  `-std=c11` -> `-std=c++23 -Wno-deprecated-enum-enum-conversion -include
  lua-c-abi/lua_c_abi_prefix.h`; `-x c++` before `-c`; drop `-resource-dir` (round 1, run-1 evidence).
- `lua_c_abi_prefix.h` (sha `96b3f261…`, copy in `host-test-v2/`) sets `LUA_API` to `extern "C"`.
  That is Lua's own documented customisation point (`luaconf.h:282-286`; `LUALIB_API`/`LUAMOD_API`
  = `LUA_API`, `:305-306`). `luaconf.h:297` has no guard, so a `-D` would be overridden. The header
  also declares the 4 lock hooks `extern "C"` (`lua.h:522-525`). It also defines `LUA_LIB` first:
  headers only test `(LUA_CORE || LUA_LIB)`, at `luaconf.h:289` and `:694`, and no Lua `.c` file
  tests either name.
- **No other unit changes.** Callers keep `<lua.hpp>` in its `extern "C"` form (`lua.hpp:7-15`),
  so they are not recompiled. The 51-unit define list from round 1 is gone.
- Two sources for "C++ throw while keeping C names works": Lua's design (`luaconf.h:282-306` and
  `lua.hpp:3`, "Lua also compiles as C++"), plus the Mac proofs below. The SDK wrapper does not pass
  `-fexternc-nounwind` (grep of `prospero-clang++`/`prospero-clang`), so callers keep their cleanup
  paths around `extern "C"` Lua calls. The host test shows 1000/1000 destructors with C names.

**PS5 cross-compile probe v2** (`ps5-xcompile-probe-v2/`, real PS5 toolchain, one compile at a time,
strict free-memory check, no link, no PRX):
- 33/33 compile, 0 warnings, 0 bytes of stderr.
- **Exact** symbol check, no demangling (fixes review #11): 79 Lua-defined symbols are referenced
  outside Lua by 39 objects, counting the 13 packaging bootstrap objects. All 79 are still defined
  under the same name. The C build has 159 public Lua names: 0 are missing and 0 are C++-mangled.
- Runtime raw-import delta: exactly `-longjmp -setjmp`, nothing new. The 7 new C++ exception
  references are all defined in `runtime-l26-A.o`.
- **Final PRX imports stay 207.** The frozen lineage import table `lineage/import_references.S:214-216,391-393`
  lists `longjmp`/`setjmp`, and `pass-1/build/import_references.o` references both. So packaging
  P7/P11 (`l26_packaging_driver.py:336,392`), the packager precheck (`package_prx_l19.py:843-862`)
  and `check_import_identity.py:52` all stay valid with **no change** (fixes review #3).

## 3. Effect on the gates

| Gate | With the staged change |
|---|---|
| include-graph (`build_variants.py:920-930`) | Would refuse all 33 units: 436 new rows. 403 of them are byte-identical BASE-pinned dependencies (libc++ `__config` etc.) and 33 are the staged prefix. The patch approves a new row only when it matches a BASE pin by path and sha. **Every approved row is recorded** per unit in the receipt, and gate `G-LUACPP-include-exemption` fails on zero (fixes review #4). The prefix sits under `l26-runtime/`, so the existing rule covers it, and its sha is pinned in code. |
| G7 import delta (`:993-997`) | Declared delta `-longjmp -setjmp`, nothing added; anything else fails. Plan mode and receipts now report the real delta: `IMPORT-DELTA.json`, `COMPOSITION-RECEIPT.actual_undefined_added/removed`, the plan-mode note (fixes review #4). |
| packaging P7/P11, precheck, import identity | **Unchanged (207).** |
| new G-LUACPP-scope | Fails unless there are 33 luaraw units, more than 0 BASE pins, and the prefix sha matches. Read-only run: 33 / 4,035 / ok. |
| fs-throw, no-detour | Not run. The build patch adds no `std::filesystem` call and no code patching. The runtime patch adds only `catch (void*) { throw; }` handlers. |

Selection (read-only harness, now fails on zero counts or failed gates, fixes review #8): opt-in
off = 205 units, exactly B4; opt-in on = the same 205 + exactly the 33 luaraw units
(`staged-patch/v2/selection-check/SELECTION-*.txt`). `ARGV-CHECK.txt`: the staged `lua_cpp_argv()`
gives exactly the argv the probe compiled (33/33). Both patches pass `git apply --check` cleanly;
neither is applied.

## 4. Does the PS5 runtime already rely on C++ exceptions? (round 1, unchanged)

Statically, yes: `LuaMadeSimple.cpp:1569-1573` throws; the TRY wrappers `LuaMadeSimple.hpp:497-512`
and `:843-857` catch `std::exception&`. `libunwind.a`, `libc++abi.a` and `libc++.a` are fixed link
inputs. `runtime-l26-A.o` defines `__cxa_throw`, `__gxx_personality_v0` and `_Unwind_RaiseException`.
The B4 PRX has `.eh_frame`, `.eh_frame_hdr` and `.gcc_except_table`. **Unknown:** whether a throw is
caught on the console. Also, with this change **every coroutine yield is a throw**
(`ldo.c:898` in `lua_yieldk`; review #6), so this open question covers every mod that uses
coroutines, not only errors.

## 5. Host tests (Mac, the port's real Lua 5.4.7 sources)

`host-test-v2/run_v2.sh` builds Lua three ways: **c** (today), **cpp** (upstream, C++ names) and
**cppcabi** (staged). It links three programs against each build. Result: **90 checks, all as
expected** (`host-test-v2/RESULT-V2.txt`, sha `77c375ef…`). The numbers below are quoted from
that file (review #9):

| Check | c | cpp | cppcabi |
|---|---|---|---|
| destructors in frames a Lua error crosses | 0/1000 | 1000/1000 | 1000/1000 |
| heap blocks left per 1000 errors | 2000 | 0 | 0 |
| **A current** (`LuaUObject.cpp:135-148` shape) | status 2, message kept 1000/1000 | **status -1, message lost 1000/1000** | **same bug** |
| **A fixed** (`catch (void*) { throw; }`) | status 2, kept, owner dtor 1001/1001 | same | same |
| A fixed, a `std::exception` while marshalling | still stored, re-thrown after the inner pcall, turned into a Lua error | same | same |
| **B current** (`LuaMadeSimple.cpp:1576-1592`, allocator failing in `luaL_traceback`) | status 4, lock free | **status 2, lock held (depth 1): other threads would hang** | **same bug** |
| **B fixed** | status 4, lock free | same | same |
| plain-C caller (`memory_lua.c` shape) | links, 2/2 | **does not link (8 undefined)** | links, 2/2 |
| public Lua names | 159 C names | 81 mangled in lapi.o | identical 159 C names |
| coroutine yields (100,000) | correct, 0.547 µs | correct, 4.715 µs | correct, 4.593 µs |
| yield from a C function: its destructor | skipped | runs | runs |
| cost per Lua error (host -O1) | 0.72 µs | 7.00 µs | 7.99 µs |

The round-1 test (`host-test/`, 17 + 17 checks) is unchanged and still PASSES. The v2 copy runs the
same 17 checks against all three builds.

**Hazards that exist only in the C++ build**

- **H1. A `catch(...)` that does not rethrow swallows a Lua error.** The call-graph audit
  `audit/catch_all_audit_v2.py` covers 1,145 files, 14,304 functions and 152 `catch(...)`; 21 of them
  rethrow. It flags 21 sites, including **both review sites** (`LuaUObject.cpp:142`,
  `LuaMadeSimple.cpp:1587`) that round 1 missed. It finds 0 pointer throws.
  - **Fixed (staged patch 0002, 6 sites):** `LuaUObject.cpp:142`, `LuaMadeSimple.cpp:1459` and
    `:1587`, `LuaMod.cpp:1018` and `:1446`, `LuaPathShim.cpp:1790`. All six are reached from Lua,
    with no game code between them and their Lua handler, so a rethrow behaves exactly like today's
    longjmp. In the C build the new handler is dead code.
  - **Reviewed, no change:** `LuaMod.cpp:241`, `:249` and `:278` sit inside `noexcept` functions
    (`:179`, `:261`), where a rethrow would terminate. All of their Lua work runs inside `lua_pcall`
    (round 1; the reviewer agreed). The audit's other matches are not Lua at all: `:282`/`:286`
    (logging lambdas), `ToolchainSelfTest.cpp:127` (`L"..."` wide string), `UE4SSProgram.cpp:1899`,
    `JMapGenerator.cpp`, `Logging.hpp:14`, staging `UnrealInitializer.cpp`. The audit matches names,
    so it can over-report. I read every candidate-tree site by hand. For the others I checked the
    audit's witness chain: each one is a name collision (`add_value`, the `callback` template
    parameter in `Logging.hpp:13`), an `L"…"` wide string read as a Lua state (`NameTypes.cpp:215`),
    or a call to the `noexcept` `Poll`, which no exception can leave.
  - **Open, H4 below:** `LuaMod.cpp:8748` (script hook; `lua.throw_error` at `:8696` is in the try,
    review #7 is correct) and `LuaMod.cpp:9772` (async loop), and `:5616` inside `noexcept
    VmtEngineTickDrain` (`:5543`).
- **H2. A Lua error crossing a `noexcept` function or a destructor calls `std::terminate`.** The v2
  audit lists 12 such functions, 7 of them in the candidate tree. `LuaMod.cpp:179`, `:261`, `:280`,
  `:284`, `:5543` and `:9814` are engine or self-test callbacks: when they run at top level, Lua has
  no handler, so an error panics in both builds before anything is thrown. `~FNativeUFunctionCall`
  (`LuaUObject.cpp:111`, `luaL_unref`) runs after the inner pcall has returned, not while an error
  unwinds.
- **H3, widened per review #5.** **Any** C++ exception that escapes a Lua-registered C function with
  no TRY reaches Lua's `catch(...)` and ends in an unbalanced unlock and `abort()`. The reviewer
  counted 26 such registrations (23 functions), for example `pcall_error_handler`, which can throw
  `bad_alloc`. The C build is also broken here (the exception flies past `lua_pcall`). Not fixed;
  the next step below handles it together with H4.
- **H4 (new, open; this is why the status is partial).** A Lua error raised **outside a fresh
  pcall inside a callback that game code called** (Lua mod -> UFunction call -> game code -> our hook
  -> raise) must now unwind through **game frames** to reach the mod's pcall. Today a longjmp
  jumps over them. As far as I can tell, the PRX's unwinder only knows our own module's
  `.eh_frame` (via `__eh_frame_hdr_start`), so this would most likely end in `std::terminate`
  (**unknown**, not console-checked). Known sources: `LuaMod.cpp:8696`, and an out-of-memory error
  from any Lua call outside `call_function` in `script_hook` (`:8610-8752`). **Proposed fix (not
  staged):** a generic rule that every engine-to-Lua entry point runs its Lua work inside one
  `lua_pcall` of a small protected C function, the same pattern as `protected_ufunction_call`. Then
  no Lua error can leave an engine callback. That needs its own audit of entry points (hooks,
  tick, delayed and async actions, NotifyOnNewObject), its own replica and its own review.
- **H5 (both builds, not a regression).** An error on the async Lua thread with no handler of its
  own goes to the **main thread's** handler (`ldo.c` `luaD_throw` mainthread branch). The C build
  longjmps onto another thread's stack. The C++ build throws on the wrong thread. Both are fatal.

## 6. Staged patches (none applied)

- `staged-patch/v2/0001-v2-l26-lua-as-cpp-c-abi-opt-in.patch` (sha `3545c48e…`): `build_variants.py`
  (base `60408cef…`, unchanged) plus the new file `l26-runtime/lua-c-abi/lua_c_abi_prefix.h`. Opt-in
  `L26_LUA_AS_CPP=1`. Generated by `make_build_patch.py` (11 exact-match edits).
- `staged-patch/v2/0002-runtime-catch-all-passes-lua-errors.patch` (sha `3e52a46f…`): 6 handlers in
  4 candidate files (base shas checked by `make_runtime_patch.py`). It is behaviour-neutral in the
  C build, so it can go into a labelled build on its own first. **Not compiled for the PS5**: the
  host replicas prove the pattern, not these exact files.
- **No `l26_packaging_driver.py` change.** The round-1 207 -> 205 hunk was wrong (section 2).

## Next steps (in order)

1. Mac: design and replicate the H4 engine-boundary pcall rule; audit every engine-to-Lua entry point;
   decide on H3 (TRY for direct registrations). Have it reviewed.
2. Mac: compile the 4 files patched by 0002 once each for the PS5 (single compiles, memory check),
   then run fs_throw_gate.py and no_detour_gate.py on the patched tree.
3. User OK for labelled build **B5a = B4 + 0002 only** (Lua still C; the new handlers are dead code,
   so behaviour should be identical: a safe bisect point). Then **B5b = B5a + H4 fix + 0001
   (`L26_LUA_AS_CPP=1`)**. Each one is its own gated build, `verify_build --manifest` and ledger entry.
4. Console (each step needs its own OK): one launch of B5b, then compare its `[MEM]` growth and error
   counts with B4. Only that comparison can answer the first verdict line.
