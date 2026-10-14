# B7 plan: build Lua as C++ so errors unwind properly

Status: **Mac-only groundwork done and tested; nothing built, nothing applied, no console contact.**
**Revised after the reviewer's findings (P1 major, P2/P3 minor).** The first version said no Lua hook callback is live on the
PS5. That was wrong (item 6 below and section 6 are rewritten, the gate has a new check E, one new staged patch).
Reviewer answer 2 (build Lua as C++ as its own build B7, after B6 is proven; keep the "error raised while a lock is held"
scan; B7's launch counts as the second clean launch) is the brief. Label rule used throughout:
**console-proven** (cites a log line of an existing session, supporting evidence only) / **Mac-only** (source reading,
host tests on the port's real Lua 5.4.7, a PS5-toolchain analysis compile). Companion files: `CATCH-AUDIT.md`
(every catch site), `README.md` (index), earlier work this extends: `round6-prep-20261012/lua-as-cpp/FEASIBILITY.md`.

## Plain verdict

1. **Do it, as two small labelled builds, after B6 is proven.** The change is safe to stage now; it is not yet safe
   to *launch* because two things cannot be proven on the Mac (items 5 and 6).
2. **It is how the PC build already behaves.** Upstream UE4SS builds Lua as plain C by default. On Windows the C
   "jump" that Lua uses for errors runs our cleanup code on the way (the B4 report says so; I did not re-verify that
   against Microsoft's documentation). On the PS5 the same jump skips cleanup, which is exactly why a held lock leaked in
   the B4 launch (spawner menu dead) and why memory pieces are left behind on every Lua error. B7 gives the PS5 the PC
   behaviour. (Mac-only for the PS5 half; the Windows half is the B4 report's statement, labelled unverified here.)
3. **What the Mac tests prove (real runtime code, real Lua):** with Lua as C++, 1000 of 1000 errors run every cleanup
   (3000 of 3000 destructors), 0 memory pieces left behind (today: 3000 left behind), the lock-then-raise leak cannot
   happen even in the old code shape, and the error text the mod sees is byte-identical in both builds.
4. **It exposes two real bugs in our own code**, found by those tests: every failed UFunction call would lose its
   error message (status -1), and an out-of-memory error while the error text is built would leave Lua's lock held.
   Both are fixed by a one-line edit each (staged, tested: 0 failures). Nine more places get the same one-line edit
   or a log-parity edit; none changes behaviour today.
5. **The one thing the Mac cannot prove:** that C++ exceptions travel through the Lua code on the PS5 itself. The PRX
   carries unwind tables (Mac-only, section 3) and plain C++ throw/catch already works in the runtime (`D:904` of the B4
   launch, console-proven for a plain throw only). B7 therefore prints **one proof line in the first seconds of the
   launch**: `[LUA-CPP] mode=cxx-throw ... void_catch=1 swallowed=0`. Any other value = stop, roll back to B6.
6. **One exposure is LIVE and needs a small extra edit (corrected).** The game itself calls our Lua hook callbacks:
   hooks on native game functions are registered and fire on the PS5 today (B4 launch-1 log `D:807`, `D:818`, `D:866-D:894`,
   `D:918`; only Blueprint-function hooks are refused). So game code sits below those callbacks, and a Lua error that
   escaped one of them would have to travel through game code, which the PS5 unwinder cannot do: with Lua as C++ the
   process would end. It can escape only in narrow cases (an unsupported parameter type, a value-pusher failing,
   out-of-memory; an ordinary error inside a mod's own callback is caught and logged), and in the most likely case the
   game already aborts today, in both builds (Mac-only code reading; the B4 log shows neither case happening). The fix
   is small and staged: three `catch (...)` lines in `ScriptHookThunks.cpp` so nothing can leave into game code
   (`staged-patch/b7-h4/`, gate check E). The idea in the first version (a protected call inside the hook callbacks)
   is withdrawn: it would break the way the pre and post callbacks pass a return value on the Lua stack
   (`CATCH-AUDIT.md` section 5). **B7b should not be built without these three lines** (decision 8.3).
7. **Cost:** +0.13 KB of Lua code in the PRX (Mac-only, measured), plus a few KB of runtime edits; each Lua error is
   3-10 times slower on the Mac (3.5 to 11 microseconds with three frames to unwind); every coroutine yield is also a
   throw. Measured on the console only after B7 runs.
8. **What I need from you:** the decisions in section 8 (two builds vs one; whether to launch B7a; whether the game-boundary
   lines ride in B7a or get their own label). Console work, as always, only with your OK for each step.

## 1. Parity: how upstream builds Lua

Source: `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/` (the copy this
port builds from; the dependency set is pinned by `source-dependencies/dependency-receipt.json`).

| Question | Answer (Mac-only, file:line) |
|---|---|
| Default language of Lua | **C.** CMake `deps/first/LuaRaw/CMakeLists.txt:7` `option(LUA_COMPILE_AS_CPP ... OFF)`; xmake `LuaRaw/xmake.lua:1-5` option default `false`. No other file in the tree sets it (grep: 0 hits outside LuaRaw), and the local CI workflows do not set it. Which setting the published release binaries use cannot be determined from this tree. |
| What the option does | CMake `:49-51` `set_source_files_properties(... LANGUAGE CXX)`; xmake `:30-36` `add_files(lua_sources, {sourcekind="cxx"})` + `add_defines("LUA_COMPILED_AS_CPP", {public=true})`; CMake `:78` the same define, public. |
| What the define changes | Only `include/lua.hpp:7-15`: C++ callers stop wrapping the Lua headers in `extern "C"` (so all Lua names become C++-mangled). Nothing in Lua's own sources reads it. |
| What actually switches errors to throw | `ldo.c:53-61`: when `__cplusplus` is defined and `LUA_USE_LONGJMP` is not, `LUAI_THROW(L,c)` is `throw(c)` and `LUAI_TRY` is `try {...} catch(...) { if (status==0) status=-1; }`. No `LUAI_THROW`, `LUAI_TRY`, `LUA_USE_LONGJMP` or `LUA_USE_POSIX` is defined anywhere in the upstream build files (grep: 0 hits). So "compile as C++" is the only switch, exactly as `lua.hpp:3` says ("Lua also compiles as C++"). |
| Flags per platform | Upstream builds only Win64 (`tools/xmakescripts/rules/build_rules.lua` platform table has one entry). C++ standard: CMake `target_compile_features(... cxx_std_23)`, xmake `set_languages("cxx23")`, `set_exceptions("cxx")` (so `/EHsc` or `-fexceptions`). Clang: `-Wno-deprecated-enum-enum-conversion` (CMake `:87`); MSVC: `/W0` (`:85`). CMake sets the C++ standard at `:68`, xmake at `:11-12`. Mode flags: `-g`, `-fms-extensions`, optimize `fastest` for Shipping. |
| Lock | Windows `CRITICAL_SECTION` (`src/luauser.c`), recursive, process-wide. This port replaced it with a pthread recursive mutex (`ue4ss-native-lua-lock-final-1/src/platform_locks.c`); same semantics. |
| Exception handling in the upstream wrapper | Same TRY / `process_lua_function` shape as ours; the B2 wrapper is a hardened version of it. |

What this port does today (B4 receipt `l26-A/pass1/COMPOSITION-RECEIPT.json`, unit `luaraw-ldo`):
`prospero-clang -std=c11 -Os -fPIC -ffunction-sections -fdata-sections -D_XOPEN_SOURCE=700 -pthread -resource-dir ...`,
33 retained objects, no `LUA_USE_*` define, so `ldo.c:73-74` uses ISO `setjmp`/`longjmp` (FreeBSD libc: no unwinding).
Three of the 33 sources are port-specific: `lauxlib.c` and `liolib.c` (stdio adaptation) and `luauser.c`
(= `platform_locks.c`). Only `luaraw-ldo.o` references `setjmp`/`longjmp`.

What B7b changes (staged v2 design, still valid; `staged-patch/0001-build-variants-lua-as-cpp-opt-in.patch`,
sha `3545c48e...`): the same 33 units compile with `prospero-clang++ -std=c++23 -Wno-deprecated-enum-enum-conversion
-include lua-c-abi/lua_c_abi_prefix.h -x c++ -Os -fPIC ...` (prospero-clang++ adds `-frtti -fexceptions`, wrapper line 83;
no `-resource-dir`). **One deliberate difference from upstream's option:** the public Lua names keep their **C**
names (the prefix header sets `LUA_API` to `extern "C"`, Lua's own customisation point `luaconf.h:282-306`) instead of
defining `LUA_COMPILED_AS_CPP`. Reason (round 6, Mac-only): the frozen packaging bootstrap `lineage/probe/memory_lua.c`
is plain C and calls 8 Lua functions by C name; with C++ names it would not link. Behaviour is identical to upstream's
C++ option; only the symbol names differ. Mac proof: 90 checks in the round-6 host test, 33 of 33 units compile with the
real PS5 toolchain with 0 warnings, 79 of 79 outside-referenced names unchanged.

## 2. The staged change set

All under `staged-patch/`; none applied; regenerate with the scripts in `tools/` (they stop on any anchor or sha drift).

| Part | What | File |
|---|---|---|
| Build opt-in | `L26_LUA_AS_CPP=1` in `build_variants.py` (+ new `lua-c-abi/lua_c_abi_prefix.h`): compiles the 33 Lua units as C++ with C names; declared import delta `-longjmp -setjmp`; include-graph exemption limited to rows identical to BASE pins and recorded per unit; gates `G-LUACPP-scope`, `G-LUACPP-include-exemption`, `G7-import-delta-declared-lua-as-cpp` all fail on zero. `git apply --check` clean on the current `build_variants.py` (sha `60408cef...`). Packaging unchanged: the final PRX still imports 207 symbols (the frozen import table lists `setjmp`/`longjmp`; nothing calls them). | `0001-build-variants-lua-as-cpp-opt-in.patch` |
| Runtime edits | E1-E10 (`catch (void*) { throw; }` in front of **11** catch-alls: E7 sits in two places; see CATCH-AUDIT section 6) + startup probe P1/P2. 13 sites in 5 files; no new `#include`. | `b7-on-candidate/`, `b7-on-b5final/`, `b7-on-b6/` (`b7-runtime.patch`, `EDITS.json`, patched `tree/`) |
| Game boundary (new) | H1-H3: `catch (...)` after the existing `catch (std::exception&)` in the thunk's pre-callback block, post-callback block and `Thunk<Index>` (`ScriptHookThunks.cpp`), logging `[B7-H4] contained a non-std exception ...`. 3 sites in 1 file, identical for all three bases; no new `#include`. Applies cleanly (`patch -p1 --dry-run`). | `b7-h4/ScriptHookThunks.h4.patch`, `b7-h4/EDITS.json`; overlays `b7-h4-on-{candidate,b5final,b6}/tree/` |
| Markers | 5 `verify_build` markers (3 wide `[LUA-CPP] mode=...` literals, 1 narrow, 1 wide `[B7-H4] contained ...`). | `add_build_markers.B7-set.txt` |

**The probe** (`lua_cpp_probe`, `LuaMadeSimple.cpp`; one startup line from `LuaMod::on_program_start`). On a throw-away
Lua state it runs a pcall that raises from a C function with a destructor-bearing frame, a coroutine yield from such a
frame, and a `catch (void*) { throw; }` / `catch (...)` pair around a raise. It reports whether the destructors ran and
which handler took the throw. Output, one line, in the log's first seconds:

```text
[Console log excerpt omitted from the private source handoff.]
```

It is generic (no mod or game value), protected (every raise is inside `lua_pcall`/`lua_resume`), and adds no include.
Host test: both builds print the expected line (section 5). If exceptions do not unwind through Lua on the PS5 the
process dies here, in the first second, with the klog showing an uncaught exception: a cheap and clear failure.

## 3. What the PS5 toolchain supports (proven vs not)

| Claim | Evidence | Label |
|---|---|---|
| The runtime already throws and catches C++ exceptions | `LuaMadeSimple.cpp:1569-1573` (throw), TRY `LuaMadeSimple.hpp:497-512`, `:843-857`; **`D:904` of the B4 launch-1 decoded log**: `[LoopA...] [Lua::call_function] lua_pcall returned LUA_ERRRUN => ...` is the text of the `std::runtime_error` thrown at `LuaMadeSimple.cpp:~1468`, printed by the `catch (std::exception&)` at `LuaMod.cpp:9766` | console-proven for a plain throw/catch across runtime frames (earlier session; supporting only) |
| The PRX has unwind tables for everything, Lua included | **Re-checked 2026-10-12 on the file that exists now**: `l26-package-A/pass-1/bare-sdk/modbridge_supplier_composition.prx` (bare, 31,569,336 bytes, sha `54f70bf4...`, a newer build than B4): `PT_GNU_EH_FRAME` program header present (`filesz 0x1ce8cc`), `.eh_frame` 0x6d8c58, `.eh_frame_hdr` 0x1ce8cc, `.gcc_except_table` 0xed1c0 (`tools/elf_sections.py info`). The numbers first recorded for the B4 file (sha `d044cf0b...`: `.eh_frame` 0x6d8c30, 236,823 FDEs, `.gcc_except_table` 0xed074) can no longer be re-checked: that pass-1 directory was overwritten by a newer build and no archived copy of the B4 PRX exists in the evidence tree. The qualitative claim (unwind tables for everything) holds for both. The 33 Lua objects already carry `.eh_frame` today (28,664 bytes; `audit/section-sizes-c-vs-cpp.txt`) because x86-64 clang emits it for C too | Mac-only |
| The toolchain compiles Lua as C++ and emits a personality-based catch in `luaD_rawrunprotected` | round-6 probe: 33 of 33 compile, 0 warnings; `ldo.o` (C++) has `.gcc_except_table` 20 bytes, references `__cxa_throw`, `__gxx_personality_v0`, `__cxa_begin_catch`, `__cxa_end_catch`, the pointer typeinfo; all defined inside `runtime-l26-A.o` | Mac-only |
| `libunwind.a`, `libc++abi.a`, `libc++.a` are fixed link inputs | `l26-A/pass1/LINK-COMMAND.json` `fixed_inputs` | Mac-only |
| An exception unwinds through Lua frames, through `luaD_rawrunprotected`'s catch, and `catch (void*)` matches a `lua_longjmp*` **on the PS5** | **not provable on the Mac** (the host uses Apple's libc++abi/libunwind). The startup probe proves exactly these three things on the console | open until the B7b launch |
| Unwinding across *game* frames | the unwinder only knows the PRX's own `.eh_frame` (`runtime-l26-A.o` references `__eh_frame_hdr_start`/`__eh_frame_start`, no `dl_iterate_phdr`); game frames have no entry, so this ends in terminate. Reproduced on the Mac with a `noexcept` frame standing in for game code. **Relevant today** because the game calls our native-hook callbacks (section 6) | Mac-only (the PS5 half is inference) |

## 4. Build plan (two labelled builds, each gated)

Preconditions (Mac): B5 and B6 built, gated, B6 launched clean (your order). The B6 work copies are still moving (they
were rebased onto `final-b5` at 09:39), so re-run `tools/make_b7_runtime_patch.py --base b6` against the **final** B6
tree first; every anchor must match once or it stops.

**B7a "catch-passthrough + probe"** (Lua stays C; `L26_LUA_AS_CPP` unset)
1. Apply `b7-runtime.patch` **and `b7-h4/ScriptHookThunks.h4.patch`** (Mac-only edit of the candidate tree copy used for the build).
2. Gates before compile, all must print counts and pass: `fs_throw_gate.py` (same violation set as the base: checked,
   `audit/gates/`), `no_detour_gate.py` (checked, same), **`tools/b7_source_gate.py`** (A catch pass-through, B lock-raise,
   C raw Lua callbacks, D noexcept, E game boundary; section 7), `tools/lock_raise_gate_b7.py` on the touched files.
3. Single PS5-toolchain compile of the 6 touched units (`tools/ps5_compile_check.py` now lists all six, including `ScriptHookThunks.cpp`, and reads the `b7-h4-on-BASE` overlay); it also checks that the include
   graph is unchanged (`deps_added` empty) and that the markers are in the object.
4. Add the B7 marker set, run `build_variants.py` as for B6, package P0-P13, `verify_build.sh --manifest`, then
   `tools/b7_postlink_check.py --expect c` on the runtime object. Ledger row.
   Expected: behaviour identical to B6 (the new handlers are dead code); the log gains the one `mode=longjmp` line.
   `[B7-H4] contained ...` must not appear in a healthy session; if it does, it is a finding to report (a Lua error left a hook
   callback), not a failure of the build.
   This build is the bisect point between B6 and B7b. **Launch optional** (decision 8.2).

**B7b "lua-as-cpp"** (B7a tree + `L26_LUA_AS_CPP=1`, patch `0001`)
5. `git apply 0001` to `build_variants.py` (+ new header), build with `L26_LUA_AS_CPP=1`. New gates fire:
   `G-LUACPP-scope` (33 units, more than 0 BASE pins, prefix sha), `G-LUACPP-include-exemption` (rows per unit
   recorded; fails on zero), `G7-import-delta-declared-lua-as-cpp` (exactly `-longjmp -setjmp`, nothing added).
6. `tools/b7_postlink_check.py --expect cpp --runtime runtime-l26-B7b.o --baseline-undefined <B7a undefined.stdout>`:
   `luaD_throw`/`luaD_rawrunprotected` exist only under their C++ names, public API (`lua_pcallk`, `lua_error`,
   `luaL_error`, `lua_yieldk`, `LuaLock`, `LuaUnlock`) keeps C names, no `setjmp`/`longjmp`, `__cxa_throw` and
   `__gxx_personality_v0` present, undefined set = baseline minus exactly {setjmp, longjmp}. Verified the checker on the
   real objects: C object + `--expect c` 7/7, C++ object + `--expect cpp` 7/7, C object + `--expect cpp` 7/7 failures,
   B4 runtime (120 MB) + `--expect c` 14/14, + `--expect cpp` 7 failures (`audit/postlink-check-*.txt`).
7. Package P0-P13 (**207 imports**, unchanged), `verify_build.sh --manifest` (24 + 5 markers), size check: the new PRX
   must be within 64 KB of B7a and `.eh_frame` must not shrink. Ledger row `B7b-lua-as-cpp` with sha and snapshot.
8. Archive every intermediate (CLAUDE.md section 4); variant A only.

**Expected size** (Mac-only): the 33 Lua objects change by **+132 bytes** in total (text -5, rodata +29, data +8,
data.rel.ro +48, eh_frame +32, gcc_except_table +20; `audit/section-sizes-c-vs-cpp.txt`). The runtime edits add a probe
(about 1 KB at -O0) and 13 small handlers. PRX file size (B4: 31,594,535 bytes signed, 31,569,336 bare) should change by
well under one page group (16 KB); anything above 64 KB means something else came in. PS5 object sizes of the 5 touched
units: `audit/ps5-compile-check-*.json` (see section 9 for status).

**Rollback.** B7a differs from B7b only by the env flag, so a bad B7b is rebuilt as B7a in minutes. On the console:
re-upload the archived B6 PRX (decrypt-off readback, loadlist row-check; each step with your OK). The probe line is the
first-seconds tripwire: `mode=UNKNOWN`, a missing line, or a startup abort = roll back; do not debug on the console.

**Console launch of B7b (after your OK for each action; one launch; game offline):** acceptance = everything B6's report
lists (spawner menu on every L3+R3, self-test result with timings vs 10 ms, perf and lag lines, lock-wait stats,
minimap settings write result, refused-hook list) **plus**: the `[LUA-CPP] mode=cxx-throw ... void_catch=1 swallowed=0`
line within the first seconds; no `[LOCKLEAK]` (expected 0 by construction: destructors run, so B6's leak detector
cannot fire; the lock-wait stats are the live health signal); `[MEM]` growth and the `errs=`/`cxx=` counters against
B6 for the same session length (this is the first launch that can answer whether the Lua memory growth came from the
skipped cleanup); no new "non-standard C++ exception" lines; report every `[B7-H4] contained ...` line with its neighbours (none expected). If clean, it counts as Townfall's second clean launch
(reviewer answer 2).

## 5. Host test (Mac-only; `host-test/`, last complete result `host-test/RESULT-before-void-catch-probe.txt`)

`run.sh` builds the port's real Lua 5.4.7 two ways (C as today; C++ with C names as staged), one compile at a time with a
2 GB free-memory check before each, and links `b7_host_test.cpp` against each. The program uses the **real runtime code**
extracted byte for byte from the candidate and the staged tree (`extract_real.py`; one documented adaptation: `fmt::format`
-> string concatenation): the B2 wrapper (`handle_error`, `throw_error_impl`, `throw_error`), the free `TRY`,
`protected_ufunction_call` + `call_ufunction_from_lua`, and the staged probe. Each build must produce its own
expected result, so a pass means "C behaves like today and C++ behaves as designed". Lines the two builds must agree on
(Lua-visible error text, statuses) are compared across builds.

| What | Lua as C (today) | Lua as C++ (B7b) |
|---|---|---|
| Probe line | `mode=longjmp` (0 destructors) | `mode=cxx-throw`, `void_catch=1 swallowed=0` |
| Old shape: lock_guard alive, `throw_error` (the B4 bug), real wrapper | app mutex **leaked**, destructors skipped | mutex **released**, destructors ran |
| B5 shape (C++ throw, TRY raises after unlock), raise-after-unlock | fine | fine |
| Error text mod sees, OLD vs NEW vs CONTROL | identical in all three | identical in all three, and identical to the C build |
| 1000 errors across 3 frames (string, vector, lock_guard) | 0/3000 destructors, 3000 blocks leaked, mutex leaked | 3000/3000, 0 leaked, mutex free |
| Out-of-memory at 40 sweep points, real `throw_error_impl` **today's code** | lock balanced at all 40 | **lock left held at 5 of 40** (bug E2) |
| same, **staged code** (E2) | lock balanced at all 40 | lock balanced at 40; `LUA_ERRMEM` reaches the pcall 6 times |
| UFunction call error, real code, today's code | status and message kept 200/200 | **status -1, message lost 200/200** (bug E1) |
| same, staged code (E1) | 200/200 | 200/200; owner destroyed 200/200 |
| Lua error crossing a `noexcept` "game frame" (H4) | survives: `LUA_ERRMEM`, frame skipped | **terminate**: "uncaught exception of type lua_longjmp*" |
| same with all pushes inside one protected call | inner status 4, outer 0 | inner status 4, outer 0 (no terminate). **Shows the mechanism only; withdrawn for the hook callbacks** (it changes the Lua stack the pre and post callbacks share, CATCH-AUDIT section 5). The staged remedy is the boundary `catch (...)` (H1-H3), whose host test is **not written yet** (it needs a run, and the Mac is under the 2 GB rule) |
| `std::exception` out of a raw Lua C function without TRY (H3) | escapes `lua_pcall` to the C++ caller | Lua's own catch turns it into -1; the next `lua_unlock` aborts |
| same wrapped in the real `TRY` | status 2 | status 2 |
| cost per error (3 frames, host -O1) | about 3.5 microseconds | about 11 microseconds (round 6, one frame: 0.72 vs 7.99) |

Counts: see the tail of `host-test/RESULT-before-void-catch-probe.txt` (`TOTAL checks=138 failed=0`, `PASS run.sh`) and the status note above. The scenarios above are in `b7_host_test.cpp`; the sweep, children and probe are the new parts compared
with round 6 (which used replicas, not the real extracted code).

## 6. The game boundary (H4): live, small fix staged

**What changed from the first version (reviewer P1).** I wrote that hooks are refused on the PS5 and that the
data-only thunk route was not live. Only *Blueprint*-function hooks are refused (`HookProcessInternal=0`,
`HookProcessLocalScriptFunction=0`; B4 report lines 99 and 197). Hooks on native UFunctions work and are used by two of
the test mods: `D:807`/`D:818` registered, `D:866-D:894` callbacks ran and one mod removed its own hook, `D:918` the thunk
fired from game code (B4 launch-1 decoded log; console-proven for that session, supporting evidence only). `D:780` is the
built-in detour-style hooks and says nothing about them.

**Facts (Mac-only, source reading; details and file:line in `CATCH-AUDIT.md` 3f and 5).**
- Path: game -> `Thunk<Index>` -> `Dispatch` -> `FirePreCallbacks` -> `lua_unreal_script_function_hook_pre` -> Lua. Game
  frames are below it. `ScriptHookThunks.cpp:75-86` catches `std::exception&` only.
- Ordinary mod errors inside the callback are contained already (`call_function` -> `std::runtime_error` -> `TRY` logs).
- A Lua error raised outside a protected call in the callback (unsupported parameter type via `throw_error`, a value
  pusher, out-of-memory) lands on the hook coroutine, which has no handler: Lua re-throws it in the main state when
  that state is inside a protected call (Lua as C: longjmp over the game frames; Lua as C++: a throw into game frames =
  terminate), otherwise it aborts the process (**the same in both builds**, so also today).
- No `PANIC` line appears in the B4 launch-1 log, and no `lua_error` crossed a hook there; this is an exposure found by
  reading code, not a crash seen on the console.

**Fix (staged, not applied): `b7-h4/ScriptHookThunks.h4.patch`, edits H1-H3.** One `catch (...)` per place where our code
hands control back to the game. Generic, three lines, no Lua stack involved, and in Lua-as-C builds it only catches what
would otherwise be `std::terminate`. Price: a contained error is logged, not delivered (if the main state was inside a
protected call, that call is not unwound by it). Not covered: the panic/abort case (same as today); an optional follow-up
H4-2 (Lua-as-C++ only: let `lms_lua_panic` throw so `TRY` logs and swallows it) is described in `CATCH-AUDIT.md` section 5
and is not staged. Gate: check E fails if any game-facing try block in `ScriptHookThunks.cpp` lacks the catch-all.
Still latent (and listed as such): Blueprint-function hook callbacks (`script_hook`), the top-of-thread drains.

**Disagreement with the round-6 review's "nothing should be built until H4 has its own fix", restated:** with this fix the
statement is met in a minimal form: B7b is not built without H1-H3. They are a prerequisite edit in B7a (decision 8.3).
Reviewer item 4 (refused-hook list) must say that native-UFunction hooks are NOT refused.

## 7. The pre-build gates, kept and extended

`tools/lock_raise_gate_b7.py FILE...` is the round-7 gate "no Lua raise while a lock is held", **kept on purpose after
B7** (with Lua as C++ the guard is released by unwinding, but the rollback build, `noexcept` frames and swallowed errors
still leak it; the gate does not relax in C++ mode). Same command line as the original at
`evidence/launch-l26-B4-launch1-20261012/analysis/spawner-menu/lock_raise_gate.py`; extended with the B6 guard
`b6diag::ActionsLock`, `NativeLuaActionDispatchScope`, `throw_lua_error`/`lua_yield*` raises and one-hop helper raises
(the `get_mod_ref` case the B5 sites track found by hand). It prints files and lock scopes checked and fails on zero.
Checked: B4 `LuaMod.cpp` = 17 findings (15 direct, 2 helper; the original finds 15); final-b5 `LuaMod.cpp` = 0; current
B6 work copy = 0 (the original gate prints "0 lock scopes ... FAIL" there because B6 replaced the guard type, so it
fails closed rather than passing silently).

`tools/b7_source_gate.py` adds, before compile (every check prints its counts and fails on zero; five checks, E is new): **A** every catch-all
that can see a Lua error either passes it on or sits in `gate/catch-allowlist.txt` with a reason; **B** the lock-raise
scan above over all compiled files; **C** raw Lua callbacks that allocate need `TRY` or a baseline entry; **D**
`noexcept` functions and destructors that reach Lua need a baseline entry; **E** (game boundary) every try block in `ScriptHookThunks.cpp` that hands control back to the game ends in a `catch (...)`, and the live `RegisterPreHook`/`RegisterPostHook` callbacks in `LuaMod.cpp` are listed (3 found: the two Lua hook callbacks and the R3 carrier hook). Results (`audit/gate-run-*.txt`):

| Tree | Result |
|---|---|
| B4 candidate, unpatched (first run, 09:xx) | FAIL: 6 catch findings (the E1-E6 sites), 17 lock-raise findings |
| current candidate tree (now final-b5 `LuaMod.cpp`), unpatched, re-run for this revision (`gate-run-unpatched-current-candidate-20261012.txt`) | FAIL: 6 catch findings + 3 game-boundary findings (0 lock-raise: B5 is in) |
| staged B7 (E1-E10 + probe) **without** H1-H3, on final-b5 (`gate-run-staged-b7only-NO-H4-b5final-MUST-FAIL.txt`) | FAIL: 3 game-boundary findings (this is the check that would have caught the review's finding) |
| staged B7 + H1-H3 on final-b5 (`gate-run-staged-h4-b5final.txt`) | PASS (3 latent warnings) |
| staged B7 + H1-H3 on the B6 work copy (`gate-run-staged-h4-b6.txt`) | PASS (3 latent warnings); needed 7 allow-list entries for B6's own noexcept diagnostics (`diag_report`, `diag_check_raise`, `diag_fn_return`, `after_pcall`, `on_leak`, `b6_pump_1hz`), each reviewed (Lua calls are `lua_getstack`/`lua_getinfo "n"`, which cannot raise) |
| same with `--strict-h4` (`gate-run-staged-h4-b5final-strict-h4.txt`) | FAIL (3 latent sites: `script_hook`, `VmtEngineTickDrain`, `process_delayed_actions`) |
| `tools/gate_mutation_test.py` (now 8 cases, on the B7+H4 tree; result `audit/gate-mutation-test-h4.txt`; the older 7-case run is `audit/gate-mutation-test-run1.txt`) | see the file: clean passes; removed E1, new swallowing catch, raise under the lock, new allocating raw callback, new noexcept Lua caller, removed H1-H3 each fail; `--strict-h4` fails |

`tools/b7_postlink_check.py` (section 4, step 6) is the post-link counterpart. `fs_throw_gate.py` / `no_detour_gate.py`:
the 5 staged files give the same violation set as the base (12 known `std::filesystem` sites in both, 0 detour sites;
raw outputs in `audit/gates/`), as CLAUDE.md section 4 requires before compile; the sixth file (`ScriptHookThunks.cpp`, H1-H3)
was run separately: fs_throw 1 file, 0 violations before and after; no_detour 1 source, 0 sites before and after.

## 8. Decisions needed

1. **Two builds (B7a then B7b) or one.** Recommendation: two. The runtime edits are (almost) dead code in B7a and prove
   nothing on their own; they exist so the only difference between B7a and B7b is the Lua flag, giving a one-flag bisect.
   (CLAUDE.md "one labelled gated build per fix" read as: the fix is "Lua as C++", the edits are its prerequisites.)
2. **Launch B7a?** Recommendation: not required; B7b's launch is the one that counts. Launch B7a only if you want the
   probe's `mode=longjmp` line and zero-change proof on the console first (costs one launch).
3. **H4 game-boundary lines (replaces the first version's "proceed without the guard", which rested on a wrong premise).**
   Recommendation: H1-H3 ride in B7a (they change nothing in a healthy run and are the prerequisite for B7b). Strict
   reading of "one labelled gated build per fix": H1-H3 are a different fix from the catch edits, so they could get their
   own label (B7a1) before B7a2/B7b; costs one more build and ledger row, no extra launch. Your call. Either way B7b is not
   built without them, and gate check E enforces it.
4. **Log-parity edits E7-E10** (stop misleading "non-standard C++ exception" lines): recommendation: keep (4 one-line
   edits, no behaviour change).
5. **C names vs upstream's C++ names:** recommendation: C names (staged), because of the frozen plain-C packaging unit.

## 9. Open items and honesty notes

- **Not compiled, not run (still true after this revision):** H1-H3 and the E1-E10 edits have not been compiled with the
  PS5 toolchain, and the extended host test has not run; the Mac stayed under the 2 GB free-memory rule in this session too
  (`vm_stat` showed about 0.6 GB free at 10:5x). The H1-H3 syntax is trivial (a `catch (...)` with one `Output::send(STR(...))`
  like the line above it) but it is unverified.
- **PS5-toolchain compile of the 5 staged files:** `tools/ps5_compile_check.py` (exact B4 argv, virtual-file-system
  overlay, include-graph and marker check). It refuses to run below 2 GB free memory; at the time of writing the Mac
  stayed under that for the whole session (other agents were compiling), so this check had not run yet when this file
  was written. Its result would land in `audit/ps5-compile-check-b5final.json`; the only files present are `...-SKIPPED-lowmem.*` (every unit skipped), i.e. it did not run. It must
  pass before B7a is built.
- **Host test:** the last complete run (138 checks, 0 failed) is `host-test/RESULT-before-void-catch-probe.txt`. It ran
  before the probe gained the `catch (void*)` check; the extended test was written afterwards and **has not run**
  (memory refusals; see `host-test/RESULT-NOTE.txt`). So the `void_catch=1 swallowed=0` expectation in sections 2 and 5
  is a Mac-side expectation not yet confirmed by a run; the earlier run did prove, with the real E1/E2 code, that
  `catch (void*)` passes the Lua error on with Apple's libc++abi (the "fixed" cases). Run `sh host-test/run.sh` once
  memory is free.
- The B6/B5 staging in `round7-staging` is read-only for me and still changing; `b7-on-b6/` was generated from the work
  copy with `LuaMod.cpp` sha `0c15fd2d...`. Regenerate before use (`EDITS.json` records the shas).
- The Windows `longjmp` behaviour is taken from the B4 report ("On PC (MSVC) a longjmp does run C++ destructors");
  I did not verify it locally. If it were wrong, B7 would still be right on the PS5 merits (destructors run, no leaks), only
  the parity argument would weaken.
- Not part of B7 and unchanged: H3 (7 raw callbacks that allocate without `TRY`), H5 (cross-thread errors), the 12
  `std::filesystem` sites. (`lua_close` *is* called: `LuaMod.cpp:8576`, `:8624`; only `Lua::~Lua` has it commented out,
  `LuaMadeSimple.cpp:644-645`. The earlier text was wrong.)
- Coroutine-heavy mods: every yield is a throw (host: about 4.6 microseconds). The four test mods use no coroutines
  (grep of the pinned `main.lua` files); the self-test and zzdiag were not checked.

## 10. Files

See `README.md` for the full index. Key ones: `CATCH-AUDIT.md`, `staged-patch/`, `tools/` (gates, generators, ELF and
nm helpers), `gate/` (allow-lists with reasons), `host-test/` (test, extractor, results), `audit/` (inventories, logs).
