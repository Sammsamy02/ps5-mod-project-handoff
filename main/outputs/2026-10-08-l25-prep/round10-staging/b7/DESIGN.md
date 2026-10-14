# B7: build Lua as C++ (round 10 staging, 2026-10-12). All Mac-only.

Nothing here touched the console, the real candidate tree, or a build. Every result below is **Mac-only**.

## What it does (plain)

When a Lua mod hits an error today, the PS5 build jumps straight back to the safe point. On the way it skips
our cleanup code, so locks can stay held and memory can stay used. Building Lua as C++ turns that jump into a
normal C++ error: cleanup runs on the way, like it does on PC. To make that safe, our own code needs:

- **11 small "pass it on" handlers** (E1-E11). They let a Lua error reach Lua's own safe point instead of
  being swallowed halfway. E11 (`mod_name_of`, the hook-report helper used by RegisterHook) is new since round 8.
- **3 "stop here" handlers** (H1-H3) where the game calls our hook code. Nothing may travel into game code
  (the PS5 cannot unwind through it), so anything that arrives there is logged and stopped.
- **One startup proof line** that shows which mode the binary really runs in.

## How it differs from PC

Same behaviour as PC (cleanup runs). One deliberate difference from upstream's C++ option: Lua's public
functions keep their C names (a prefix header), because a frozen plain-C packaging unit calls them by C name.

## Files

| Path | What |
|---|---|
| `apply_b7.py` | Applies the 17 runtime edits to a tree: `python3 -I apply_b7.py [ROOT]` (default ROOT = real candidate). Each anchor must match exactly once, else exit 1 and nothing is written. Refuses a second apply. |
| `tree/` | base-tree + apply_b7.py (6 files changed). `delta.diff` = `diff -ru base-tree b7/tree`. |
| `build-tool/0001-build-variants-lua-as-cpp-opt-in.patch` | Build-tool change. **NOT applied.** |
| `build-tool/add_build_markers.B7-set.txt` | 5 verify_build markers for `harvest/add_build_markers.py` (driver adds them). |
| `gate/`, `tools/` | B7 source gate + allow-lists, rebased onto this base (see "Gate" below). |
| `host-test/` | Host test, repointed at base-tree (today) and `tree/` (staged). `RESULT.txt` = last run. |
| `gate-runs/`, `syntax/` | Raw outputs of every check below. |

## The build-tool patch (staged, not applied)

- File it changes: `outputs/2026-10-08-l26-runtime/build_variants.py` (still sha `60408cef...`, unchanged since
  round 8) and it adds `outputs/2026-10-08-l26-runtime/lua-c-abi/lua_c_abi_prefix.h` (sha `96b3f261...`).
- Apply from the worktree root: `git apply outputs/2026-10-08-l25-prep/round10-staging/b7/build-tool/0001-build-variants-lua-as-cpp-opt-in.patch`
  (`git apply --check`: rc=0 today). Build with `L26_LUA_AS_CPP=1` set in front of `harvest/run_labelled_build.sh`
  (the script passes the environment through to `build_variants.py`). Without the variable the build is the same as today.
- How the build proves Lua was compiled as C++:
  1. build_variants gates (in `build_variants.log` and the receipt): `G-LUACPP-scope` (33 Lua units; BASE l25-A has
     33), `G-LUACPP-include-exemption` (rows checked per unit, fails on zero), `G7-import-delta-declared-lua-as-cpp`
     (raw imports = BASE minus exactly `setjmp`, `longjmp`; both are in BASE `undefined.stdout` today).
  2. Post-link: `python3 -I tools/b7_postlink_check.py --expect cpp --runtime <runtime .o> --baseline-undefined <BASE pass1/undefined.stdout>`
     (`luaD_throw` / `luaD_rawrunprotected` exist only under C++ names, public API keeps C names, no setjmp/longjmp).
     Re-checked today on the real BASE object: `--expect c` 13/13 PASS, `--expect cpp` 7 of 14 FAIL, as it must.
  3. verify_build markers: run `add_build_markers.py B7` with the set in `build-tool/` first.
  4. On the console: the `[LUA-CPP] mode=cxx-throw` line (below).

## How the launch proves it ran

Log markers (wide literals unless noted):

- `[LUA-CPP] mode=cxx-throw err_status=` : healthy B7. Expected full line:
  `[LUA-CPP] mode=cxx-throw err_status=2 err_dtor=1 yield_status=1 yield_dtor=1 void_catch=1 swallowed=0`
- `[LUA-CPP] mode=longjmp err_status=` : Lua is still C (the build flag did not take). Stop.
- `[LUA-CPP] mode=UNKNOWN ran=` : mixed or broken. Stop, roll back.
- `b7-probe-void` (narrow) : probe's own text; proves the probe code is in the PRX.
- `[B7-H4] contained a non-std exception (a Lua error?) at the game boundary: ` + `thunk pre callback` /
  `thunk post callback` / `script thunk` : should **never** appear. If it does, report it with the lines around it.
- Existing lines that now also catch a Lua error if one ever reaches them: `[R9-GUARD] game-thread hook contained an exception`,
  `[R9-EIGT] ... threw; continuing`, `[R9-NOTIFY] ... threw a non-standard exception`, `[R9-GUARD] async action loop contained an exception`.

No line printed, or the game dying in its first seconds = C++ errors do not unwind on the PS5. Roll back.

## Checks run (counts)

| Check | Result |
|---|---|
| apply_b7.py on a fresh base-tree copy vs `tree/` | identical (`diff -r` empty), 17 edits, 6 files; 2nd apply refused (rc=1) |
| `syntax_check_tree.py` (PS5 prospero-clang++, l24 argv) | 5 units checked, 0 failed: lua-luauobject, luamod, lms-luamadesimple, lua-luaftext, native-script-hook-thunks |
| same recipe, l26-A receipt argv (LuaPathShim is not in the l24 receipt) | `lua-pathshim` 1 unit, 0 failed (`tools/syntax_check_tree_l26A.py`) |
| `field_gate.py --root tree` | PASS (41 files, 460 member-reading calls, 0 new unproven reads) |
| `fs_throw_gate.py` | same as base: 41 files, 54 sites, the same 35 known violations (line shifts only); nothing new |
| `no_detour_gate.py` | PASS, 0 detour sites (same as base) |
| `b7_source_gate.py` on `tree/` | PASS: 2913 files, 328 catch handlers, 53 lock scopes, 46 raw callbacks, 14 noexcept, 3 game-facing try blocks (0 without catch-all) |
| same on the unpatched base | FAIL as it must: the 7 pass-through sites + 3 game-boundary sites |
| `gate_mutation_test.py` | 9 cases, 0 failed (incl. new M7: remove E11) |
| host test `run.sh` | PASS, 140 checks, 0 failed (C 66, C++ 68, cross-build 6). Probe: C = `mode=0 ... void_catch=0`, C++ = `mode=1 err_dtor=1 yield_dtor=1 void_catch=1 swallowed=0`. First run of the extended probe test. |
| new imports from `catch (void*)` | none: `_ZTIPv`, `__cxa_*`, `__gxx_personality_v0` are already defined in BASE `runtime-l25-A.o` |

Syntax-check note: run with `--root tree` directly, the checker fails even on the **unmodified** base-tree
(NameTypes.hpp is reached through the copied include/ and the real include/, so it is read twice). I checked through
`syntax-root/` (src = this tree, include = symlink to the real, identical candidate include/). B7 changes no header.

## Gate changes (my staging copies only)

- Allow-list rebased: 12 new or re-keyed sites read by hand. Class `BOUNDARY-CONTAIN` = R9 guards where the game or
  engine calls us, or the top of our own thread. Passing the error on there would crash, so they stay as "stop here".
  Three entries are gone because their code is not in the base any more (B6 diagnostics, old delayed-action key).
- `rebase_pristine_state_on_backing` (new noexcept function): its Lua calls cannot raise. Allow-listed.
- Check E now accepts a catch-all whose handler wraps its own log call in try/catch (H1-H3 do this now, so a
  failing log line cannot escape into game code). Self-test case added.

## Risks / not provable on the Mac

1. **That C++ errors unwind through Lua on the PS5 at all.** The Mac uses Apple's unwinder. Only the startup line proves it.
2. **A contained error at H1-H3 / R9-GUARD can leave Lua's global lock held** (the raising Lua call took it). That only
   happens in the cross-thread case (H5), which crashes today. The result would be async mods stopping, not a crash.
   The log line names it.
3. **Raw callbacks without TRY** (6 in `gate/cfunction-baseline.txt`; the gate counts 7): with Lua as C++, a
   `std::bad_alloc` there means abort at the next unlock instead of escaping. This only happens when memory runs out.
   Unchanged from round 8.
4. Every Lua error and coroutine yield costs more (Mac: about 3x-10x, microseconds). Measured on the console only.
5. Merge: E11, P2 and E4/E5/E8-E10 sit in LuaMod.cpp, which HOOKS also edits. Anchors are small catch blocks plus
   the `on_program_start` opening, so apply_b7.py should still apply after HOOKS. If an anchor moved, it stops and names it.
6. `--strict-h4` still fails on 2 latent sites (`script_hook` for Blueprint hooks, which are refused today, and
   VmtEngineTickDrain's inner drain catch). These are warnings, as in round 8.
