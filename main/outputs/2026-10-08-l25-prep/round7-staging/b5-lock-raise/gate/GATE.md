# Lock-raise gate (ONE gate for B5 and B6) -- round 8 revision

All claims below are **Mac-only** (static source analysis, no console contact, no build, no PRX).
Nothing was installed into `harvest/`; no candidate, original, mod, harvest or evidence file was touched.
Previous version of this file: `archive/GATE.md.before-round8`; previous gate: `archive/lock_raise_gate.py.before-round8`.

## What changed in round 8 (answers to REVIEW-1: B6-1, B6-2, gate-breakers)

| Review finding | What was wrong | Fix |
|---|---|---|
| "`--helpers off` must pass" did not catch a revert of `get_mod_ref_cxx` -> `get_mod_ref` (major) | `get_mod_ref` was only a *helper-tier* finding (name found by scanning), so `--helpers off` ignored it | New **banned-call tier**: a call to a name in the ban list (default `get_mod_ref`) while ANY lock is held is a finding in **every** mode, including `--helpers off`. `get_mod_ref_cxx` is a different identifier and stays allowed. `--ban NAME` / `--unban NAME`. Proven by the review's exact mutant (`run_gates.sh`, checks 7-8: 2 banned-call findings, exit 1) |
| After B6 swaps `std::lock_guard` for `b6diag::ActionsLock`, the B5 gates scanned 3 of 47 scopes and still passed (major) | there were THREE gates (`gate/lock_raise_gate.py`, `sites/tools/lock_raise_gate_v2.py`, `b6-diagnostics/lock_raise_gate_b6.py`), each knowing different lock types and bans | **One gate.** `ActionsLock` is a built-in custom guard type (mutex = FIRST argument, so `ActionsLock{LuaMod::m_thread_actions_mutex, __LINE__}` is parsed correctly); `--guard-type NAME` adds another RAII wrapper; `using`/`typedef` aliases of any guard type are discovered automatically. The other two files are now ~30-line wrappers that call this one (`archive/` keeps the old ones) |
| "or fail when a known guard type is declared that it does not parse" | a renamed or new wrapper silently reduced the scope count | **Unknown-guard check (exit 2):** a declaration `T name{...mutex...}` / `T name(...mutex...)` where T is not a known guard type stops the gate with `UNKNOWN-GUARD file:line`. Allow a non-guard with `--allow-unknown-guard NAME`. Proven by the mutant that renames the wrapper (check 10: exit 2, not a silent smaller scan). On the real compiled set it fires 0 times |
| B6 must keep tracking every site | `lock_raise_gate_b6.py --require-tracked` counted raw std scopes only on LuaMod.cpp | `--forbid-raw-guard MUTEX_REGEX`: any std guard (or alias) on a matching mutex fails (exit 1); it also fails (exit 2) when the regex matches no guard at all (nothing checked). Prints `N std guard(s) ... (must be 0), M tracked wrapper guard(s)` |
| Gate-breaker shapes (7 cases, all missed) | alias/typedef, CTAD `auto g = std::lock_guard(m)`, holder classes (ctor-init-list; ctor `lock()` + dtor `unlock()`), metamethod APIs, `std::function` callback | Caught now: alias, typedef (and alias of alias), CTAD declaration, class with an uninitialised guard member (assumed built by the ctor, fail closed), class whose inline ctor locks and inline dtor unlocks, and with `--metamethod-calls` `lua_gettable/settable/getfield/setfield/geti/seti/getglobal/setglobal/arith/concat/compare/len/next, luaL_len, luaL_tolstring`. **Still not caught (documented limit): a callback (`std::function`/functor) called under a lock inside a callee whose lambda argument raises** (see "Known limits") |
| Both "B5 gates" ran on a file list from the 1e49188b PRX | that list has the older port copies of `LuaMadeSimple`, not the candidate's | `make_lists.py` builds three lists from the **B4 build's** compiled-files (`evidence/builds-20261012/B4-shim-final-stack/compiled-files.txt`, 3561 files): `compiled-files-b5.txt` (candidate LuaMod.cpp -> final-b5 copy) and `compiled-files-b5-b6.txt` (the four B6-touched files -> the staged B6 work copies). The old `compiled-files-patched-b5.txt` is kept for history |

## Files here

| File | Purpose |
|---|---|
| `lock_raise_gate.py` | **the gate** (stdlib only; ~2700 lines incl. self-test) |
| `run_gates.sh` | one command: self-test, real trees, 5 mutants of the real trees, 7 gate-breaker cases; prints the number of checks, exits 1 on any failure |
| `make_lists.py` | builds `compiled-files-b5.txt` and `compiled-files-b5-b6.txt` (prints entries read/replaced/written, fails if a replacement matched nothing) |
| `patched/` | copy of candidate `LuaMod.cpp` with `../sites/final-b5.diff` applied (patched sha256 `afaf5caa...3550`) |
| `baseline-b5-patched-UNREVIEWED.txt` | the 38 helper-tier sites that remain after B5 (the same 38 remain after B6; checked). **Not an approval**: every line says `## UNREVIEWED` |
| `compiled-files-b4.txt` | the B4 list with the PRISTINE base LuaMod.cpp (`../sites/base-candidate/`, sha `4d6bc63b`). The live candidate `LuaMod.cpp` was patched with final-b5 by another workflow at 10:04 (sha now `afaf5caa`), so "B4 candidate" rows are run on this copy |
| `breakers/` | copies of the reviewer's 7 gate-breaker sources (`run_gates.sh` runs them) |
| `staged-harvest-fix/` | staged fix for the `lex_cxx` defect (diff + copy + `check_fix.sh`); harvest itself is untouched |
| `results-r8/` | all outputs of this revision (`run_table.sh` regenerates the table below); `results/` is the round-7 output (old gate) |
| `archive/` | the round-7 gate and doc |

## Verdict (Mac-only)

* **B4 candidate, hard mode**: 17 findings = the 15 known lock-then-raise sites (14 direct + the local-lambda site at `LuaMod.cpp:8695`) + the 2 `get_mod_ref` sites (`:3084`, `:5969`), at the same lines as before. Exit 1.
* **final-b5**: hard mode 0 findings (exit 0). **final-b5 + b6**: hard mode 0 findings, 46 `ActionsLock` guards tracked, 0 std guards left on `m_thread_actions_mutex` (exit 0). The scope count goes from 179 (B5) to 186 (B5+B6): the 46 guards are the same acquisitions (now of type `ActionsLock`); the +7 are B6's own new code (LuaMod.cpp +5: two rate-limiter mutex guards, the wrapper's own `lock()`/`try_lock()`/`unlock()`; LuaPathShim.cpp +2: the new write-fail guards on `context->mutex`). None of them has a raise or a banned call inside.
* **Helper tier**: 38 sites on final-b5 and the SAME 38 on final-b5+b6 (`38 known, 38 still present, 0 resolved, 0 NEW` with the existing baseline). B6 adds no new helper-call under a lock.
* **Recommended hard rule for every build that goes to the console** (exactly what `run_gates.sh` and `b6-diagnostics/run_all_b6_checks.sh` run):

```
python3 lock_raise_gate.py --files-from compiled-files-b5-b6.txt \
        --path-map outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/b6-diagnostics/work/=outputs/2026-10-08-l26-runtime/typescan-r4/candidate/ \
        --helpers off --unprotected-calls --metamethod-calls --forbid-raw-guard m_thread_actions_mutex
```
  (for a B5-only build use `compiled-files-b5.txt`, `--path-map outputs/.../gate/patched/=`, and drop `--forbid-raw-guard`). When the real build tree is gated, give it the real compiled-files list and no path map.
* `--metamethod-calls` finds 0 sites on the candidate under any lock, so it is part of the hard rule at no cost.

## Results table (all Mac-only; `results-r8/run_table.sh` regenerates every row)

| Run | files checked | files parsed | lock scopes | raise sites seen | findings | exit |
|---|---|---|---|---|---|---|
| B4 candidate, hard mode (`--helpers off --unprotected-calls --metamethod-calls`) | 3561 | 56 | 179 | 389 | 17 (14 direct, 2 banned-call, 1 lambda) | 1 |
| B4 candidate, full (helper tier on) | 3561 | 165 | 183 | 310 | 55 (14 direct, 2 banned, 36 helper, 3 lambda) | 1 |
| final-b5, hard mode | 3561 | 56 | 179 | 375 | 0 | **0** |
| final-b5, full | 3561 | 165 | 183 | 295 | 38 (36 helper, 2 lambda) | 1 |
| final-b5, full + baseline | 3561 | 165 | 183 | 295 | 38 known, 0 new | **0** |
| final-b5 + b6, hard mode + `--forbid-raw-guard m_thread_actions_mutex` | 3561 | 56 | 186 | 375 | 0 (0 raw, 46 tracked) | **0** |
| final-b5 + b6, full | 3561 | 165 | 190 | 295 | 38 (36 helper, 2 lambda) | 1 |
| final-b5 + b6, full + baseline | 3561 | 165 | 190 | 295 | 38 known, 0 new | **0** |

"raise sites seen" in hard mode is larger than in full mode (375 vs 295 on final-b5) only because `--metamethod-calls` adds the metamethod APIs to the primitive set; it adds no finding.
"lock scopes" = lock acquisitions (guard declarations, manual lock()/actions_lock(), lock-holder instances, lock macros), counted once per source line even for `#if/#else` files scanned in two alternatives.

## `run_gates.sh` (18 checks, all pass)

selftest (115/115) - B4 candidate finds 17 - final-b5 hard 0 - final-b5+b6 hard 0 (46 tracked) - baseline x2 (0 NEW) - mutant: final-b5 with `get_mod_ref_cxx` reverted -> exit 1, 2 banned-call - mutant: same revert in the B6 tree (the reviewer's exact mutant) -> exit 1 - mutant: every `ActionsLock` swapped back to `std::lock_guard` -> `--forbid-raw-guard` exit 1 - mutant: wrapper renamed -> `UNKNOWN-GUARD`, exit 2 - identity-mutant control stays PASS - breakers alias/autoctad/h1/h2/typedef_ caught (exit 1) - gettable caught with `--metamethod-calls` - stdfunc is the documented known limit (exit 0).

## Self-test (`--selftest`: 115/115 PASS, `results-r8/selftest.txt`)

* 95 synthetic source cases (positives P01-P56, negatives N01-N39) plus 20 gate-behaviour cases (exit codes 0/1/2: empty root, zero lock scopes, zero raise sites, finding, baseline write/known/new/empty, `--files-from`, unbalanced braces, **unknown guard type, `--allow-unknown-guard`, `--guard-type`, parameter named `mutex`, `--forbid-raw-guard` (only wrappers / std guard left / matches nothing), banned `get_mod_ref` fails with `--helpers off`, `get_mod_ref_cxx` passes, wrapper-only file is scanned**).
* New cases this round: P43-P56 / N31-N39 (custom wrapper holds / scope ended; banned under std and wrapper guard with `--helpers off`; `get_mod_ref_cxx`, outside-lock and member-call negatives; `--unban`, `--ban`; using/typedef/alias-of-alias; CTAD; guard member + ctor-init-list; ctor-lock/dtor-unlock holder and its negative; metamethod on/off; `--guard-type`; `--mutex` filter with the first-argument rule).
* Mutation check (scratchpad script, not shipped; each mutant made at least one case fail): no ban list (3), no `ActionsLock` (6), no alias discovery (3), no CTAD (1), no member-held (1), no ctor/dtor holder (1), no unknown-guard check (1), no raw-guard check (1), no metamethod tier (1), wrapper mutex = whole argument list (1). Round-7 mutants (lock-held test 44, unlock 6, branch merge 2, lambda policy 3, helpers 6, blanking 1) were not re-run; the 82 round-7 cases still pass unchanged.

## What the gate finds (inside a region where a lock is HELD)

* `direct`: a raise primitive (`lua.throw_error`, `throw_error_impl`, `luaL_error`, `lua_error`, `luaL_arg*`, `luaL_check*/opt*`, `lua_yield*`; with `--unprotected-calls` `lua_call/lua_callk`; with `--metamethod-calls` the metamethod APIs listed above). C++ `throw` is deliberately not a raise (destructors run).
* `banned`: a call to a banned helper (default `get_mod_ref`), always a finding.
* `helper`: a call to a function that raises (names built by scanning the file set, up to `--helper-depth`, default 2), including `#define` macros and `L22Diagnostics::throw_lua_error`. Off with `--helpers off`, WARN-only with `--helpers warn`.
* `lambda`: a call (or use as an argument) of a LOCAL named lambda whose body raises.

Lock forms understood: `std::lock_guard/unique_lock/scoped_lock/shared_lock` (+ aliases, with or without template args, brace or paren init, CTAD `auto g = std::lock_guard(m)`, `defer_lock/adopt_lock/try_to_lock`, if/for/switch init statements, `unlock()/lock()` on the guard variable), the wrapper types (`ActionsLock`, `--guard-type`), `mutex.lock()/unlock()/try_lock()` statements, `std::lock(a,b)`, `actions_lock()/actions_unlock()`, classes that own a guard member (also an uninitialised member built in a ctor) or whose inline ctor locks and inline dtor unlocks (instances hold the lock), and `#define` macros declaring a guard.

Scope model, flow handling, lambda policy, comment/string pre-pass, baseline ratchet, inline `// [LOCK-RAISE-APPROVED]` marker, and exit codes (0 PASS, 1 findings, 2 usage / nothing checked / unknown guard / unbalanced braces) are unchanged from round 7 (see the module docstring; `lock_raise_gate.py -h`).

New switches: `--guard-type NAME`, `--allow-unknown-guard NAME`, `--ban NAME`, `--unban NAME`, `--forbid-raw-guard MUTEX_REGEX`, `--metamethod-calls`. Existing: `--helpers fail|warn|off`, `--helper-depth N`, `--unprotected-calls`, `--mutex REGEX`, `--lambda-policy`, `--show-helpers`, `--list-locks`, `--json OUT`, `--baseline F`, `--write-baseline F`, `--path-map FROM=TO`, `--root`, `--files-from`, `-v`, `--selftest`.

## The 38 helper-tier sites (unchanged; still need a human decision)

| Group | Sites | What it is |
|---|---|---|
| `erase_from_container(...)` in `LuaMod::uninstall` under the guard | 17 | calls `get_mod_ref` (banned when called DIRECTLY under a lock; here it is one hop away, inside a helper). Not a registered Lua function; whether a pcall is active there is a runtime question |
| `process_simple_actions` / `process_delayed_actions` / `process_pending_notify_on_new_object_callbacks` under `NativeLuaActionDispatchScope` | 12 | each reaches `TRY -> throw_error`; the game-thread pump |
| `get_function_ref` (ambient) | 4 | matches the sites track's `optional-ambient-getfuncref.diff` |
| `TRY` under the guard | 3 | TRY raises after catching, so calling it while an outer lock is held re-creates the shape one level up |
| `fire_on_lua_stop_for_self`, `construct` (via lambda) | 2 | `construct` is a generic name (name matching) |

The baseline file keys have no line numbers, so it is valid for both the B5 and the B5+B6 tree. Replace `UNREVIEWED` by a reason or fix a site; do not treat the file as an approval.

## Known limits (fail-closed where noted)

* **Callbacks called under a lock** (`void f(std::function<void()> cb){ lock_guard g{m}; cb(); }` with `f(lua, [&]{ lua.throw_error("x"); })` at the call site) are NOT followed: a lambda passed as an argument is judged by the lock held at the CALL site, not by what the callee does with it. None exists in the candidate (the review confirmed). Covered only when the callee is a named function whose own body raises.
* Out-of-class constructor/destructor definitions (`Holder::Holder(){ m.lock(); }`) are not read for the ctor-lock/dtor-unlock holder rule (inline ones are).
* Helper resolution is by **name** (not types/overloads); generic names can collide (`construct`); virtual calls and function pointers are not followed.
* Static analysis cannot see whether a Lua pcall is active at run time; that decides if `throw_error_impl` longjmps or throws C++. The helper tier is therefore triage + baseline.
* The unknown-guard check looks for `T name{x}` / `T name(x)` where the argument's last identifier contains "mutex" (case-insensitive) and T is not a known guard. A wrapper that takes something else (an object holding a mutex, a lambda) is invisible to it; a wrapper named in a `using` of a non-guard type is not an alias target either.
* `#if` groups: only uniform alternatives are tried; a file that balances in none exits 2.
* Guard moves/returns, `lk.release()`, and locks taken through wrapper functions other than `actions_lock()` are not modelled.
* A scan is only as good as its file list. `make_lists.py` builds the lists from the B4 build's compiled-files; a real B5/B6 build must gate its own compiled-files list.

## Shared-lexer defect (harvest, pre-existing; not fixed here because harvest is read-only for this task)

`harvest/no_detour_gate.py: lex_cxx` leaves `in_block = True` after a block comment that opens and closes on one line, so the following lines are blanked until the next `*/` (verified: `lex_cxx(['int a; /* x */ int b;','lua.throw_error("x");'])` -> empty code view for line 2). This gate has its own pre-pass and is immune (self-test case P22). `fs_throw_gate.py` and `no_detour_gate.py` are exposed. Staged fix: `staged-harvest-fix/lex_cxx-same-line-block-comment.diff` (one line: `in_block = False` once the `*/` is found on the same line). `staged-harvest-fix/check_fix.sh` shows the defect on the sample and runs both harvest gates with the ORIGINAL and the fixed lexer on final-b5, the B6 base and the B6 work tree: identical verdicts and identical violation sets (the fix only exposes more live lines, e.g. 6633 -> 7104 in LuaMod.cpp); 7 of 7 checks pass.

## How to wire it

Pre-compile, same slot as `fs_throw_gate.py` and `no_detour_gate.py`: run the hard-rule command above on the build's real compiled-files list; it must exit 0. Run the helper tier with `--baseline` as a ratchet (it must report 0 NEW). When installing into `harvest/`, it finds `no_detour_gate.py` next to itself (or via `GATE_HARVEST_DIR`); it never writes bytecode there (`sys.dont_write_bytecode`). Installing it is a separate approved step.

## Evidence index (`results-r8/`)

`selftest.txt`; `run_gates.txt`, `check-NN.txt` (one file per `run_gates.sh` check); `B4-hard.txt`, `B4-full.txt`, `b5-hard.txt`, `b5-full.txt`, `b5-baseline.txt`, `b5b6-hard.txt`, `b5b6-full.txt`, `b5b6-baseline.txt`, `b5-hard-list-locks.txt`; `make_lists.txt`; `rc.txt`.
Round-7 outputs of the old gate remain in `results/`.
