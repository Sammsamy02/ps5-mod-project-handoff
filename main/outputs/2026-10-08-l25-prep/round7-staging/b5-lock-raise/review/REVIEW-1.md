# REVIEW-1 (lens: RULES + RISK) - B5 lock-raise (sites, host-test, gate) and B6 diagnostics

Reviewer: adversarial, read-only. Everything below is **Mac-only** (no console contact, no builds, no PRX).
I wrote only this file and scratch copies under the session scratchpad. Nothing was edited in the candidate, original, mods, harvest or evidence trees.

## Verdict

- **B5 fix (final-b5.diff): stands.** I could not refute it. It is generic, adds no `#include`, no detour, and the gates stay clean.
- **B6 as staged: do not build yet.** It has 3 pre-build problems (below): stacked on the wrong B5, the B5 gates silently under-scan the B6 tree, and one shipped test fails.
- `refuted=false`. Nothing here breaks the B5 fix. The B6 problems are fixable before any build.

## What I re-ran (all Mac-only)

| Check | Result |
|---|---|
| `host-test/build.sh` (copy in scratchpad, full rebuild of Lua 5.4.7 as C, memory 4.3 GB free) | `checked 63, failed 0`, PASS |
| `sites/hosttest/build_and_run.sh` (scratchpad) | `checks=32 failures=0`, PASS |
| `sites/tools/lock_raise_gate_v2.py --file` on base / final-b5 / empty file | base FAIL (scopes=47, FAIL=17); final-b5 PASS (FAIL=0); empty FAIL "nothing scanned" |
| `gate/lock_raise_gate.py --selftest` | 82/82 PASS |
| `gate/lock_raise_gate.py --helpers off --files-from compiled-files-patched-b5.txt` | 3558 files, 168 scopes, 0 findings, exit 0 |
| `harvest/no_detour_gate.py` on base, final-b5, B6 work, final-b5+B6 | PASS in all 4 (0 sites, 4 notes) |
| `harvest/fs_throw_gate.py` on same trees | same 30 call sites, same 12 violations in all 4 (no new one) |
| `b6_source_gate.py`, `lock_raise_gate_b6.py --require-tracked` | PASS (105 includes compared, 46 scopes, 12 markers) |
| B6 host + pump test (copy in scratchpad, rebuilt from current `work/`) | host 157/0, pump 23/0, PASS. This re-run covers the sources edited after the 09:03 results |
| B6 shim test (never run before) | **2 of 15 checks FAIL** (see B6-3) |
| `prospero-clang++ -fsyntax-only` (archived l24 argv, single TU, no object), **final-b5 + b6.diff** applied in a scratch copy: LuaMod.cpp, LuaMadeSimple.cpp, LuaPathShim.cpp | exit 0, 0 errors, same 23 warnings as base. Control: a deliberate error is caught, and LuaMod.cpp against the old header gives 6 errors, so the check bites |

The last row retires B6's open risk 1 ("not compiled on the console toolchain") for syntax and types. It is syntax-only, not a link or PRX build.

## Findings

### B6-1 (major) B6 is built and verified on the wrong B5

- `b6.diff:1-2` says it applies on top of `proposed-B5-raise-after-unlock.diff` (the 15-site diff). `base/src/LuaMod.cpp` sha is 8e514710..., and `grep -c get_mod_ref_cxx work/src/LuaMod.cpp` is 0.
- So B6's tests, source gate and patch-chain check (`tests/verify_patch_chain.sh:11-17`) never saw final-b5 (sha afaf5caa..., the version with `get_mod_ref_cxx`).
- In `work/src/LuaMod.cpp:3646` (RegisterCustomEvent) and `:6614` (RegisterHook), `get_mod_ref(lua)` still runs under `b6diag::ActionsLock`. That is the leak the sites track found. `lock_raise_gate_b6.py --require-tracked` passes on this tree with 0 findings. It has no `get_mod_ref` ban.
- I checked the combination: `patch -p1 < final-b5.diff && patch -p1 < b6.diff` applies with no fuzz or offset and is syntax-clean. So it is fixable, but the B6 package must be rebased and re-verified on final-b5. The ledger must say "final-b5 + b6".

### B6-2 (major, gate hole) B5 gates under-scan the B6 tree and still pass

- B6 swaps `std::lock_guard` for `b6diag::ActionsLock` at 46 sites. On the final-b5+B6 tree:
  - `sites/tools/lock_raise_gate_v2.py` checks **3** lock scopes (47 before), PASS.
  - `gate/lock_raise_gate.py --helpers off` checks **11** scopes (56 before), PASS.
- Counts are non-zero, so the "fails on zero" rule is met, but 36 to 44 scopes are no longer looked at. Only `lock_raise_gate_b6.py` covers them, and it lacks the `get_mod_ref` ban (B6-1).
- Fix: have one gate know both lock types, or fail when a known guard type name is declared that it does not parse. Wire the B6 build to run it plus the `get_mod_ref` ban.

### B6-3 (major for a shipped test, minor for the product) shim test fails

- `tests/b6_shim_test.cpp` (written, never run per B6.md) fails lines 62 and 88 when run. Cause: `thread_local char tl_backend_text[160]` (`work/src/LuaPathShim.cpp:800`) cuts the liolib message `"<path>: No such file or directory"` before the strerror text when the path is long. It is read back as `'/TMP/.../nodir/file.txt: No su'`.
- Product impact is small: `errno=` and `strerror=` in the `[SHIM] write-fail` line come from the errno value, so only `backend=` is cut. Real PS5 paths of 100+ characters will hit it.
- The test's temp path template is also hard-coded to the session scratchpad. Fix the test or enlarge the buffer to 256, re-run, and put the result in `RESULT-shim.txt`.

### B6-4 (minor, claim wrong) "uncontended path reads no clock"

- `B6.md:54` and `:99` say an uncontended lock reads no clock.
- The code: `ActionsLock` ctor (`work/src/LuaMod.cpp:484-488`) calls `holder_set(line)` (`:468`) on every outermost acquisition. That reads `now_ms()` and calls `on_game_thread()` (`IsInGameThreadRaw`). `B6.md:161` itself admits "one clock read and one game-thread-identity call" per outermost lock.
- This runs on every `script_hook`, pump and keybind lock. Cost on the console is unmeasured. For the "no stutter" acceptance it is worth a line in the report. Cheap fix: take the timestamp lazily (only in the pump stuck scan), or drop `since_ms` for non-slow paths.
- I could not find how `in_current_raw()` is implemented, so I cannot say whether it takes a lock. Unverified.

### B6-5 (minor) heuristics and limits worth stating in the report

- `kind=pump` is "same depth > 0 in two pumps, no dispatch". It can fire if the carrier or tick pump runs nested inside a game-thread `script_hook` Lua callback (holds `ActionsLock`, not a dispatch scope). It is rate limited to once per mod then every 30 s. B6.md open risk 4 already says heuristic.
- `[LOCKWAIT] still waiting` is only printed by the game-thread pump. If the **game thread** is the blocked waiter, nothing is logged. `B6.md:66-69` ("a lock that never comes back is still reported") holds only when the game thread is not the waiter. That is the opposite of the B4 shape, so B6 would not have helped if a worker had leaked the lock.
- `b6diag::on_game_thread()` has a non-PROSPERO branch that calls `Unreal::GetGameThreadId()`, which throws before init. Thrown from the `ActionsLock` ctor after the lock is taken, it would leak the lock. Host-only code (`__PROSPERO__` is the PS5 path), so not live here.
- `__LINE__` site ids in `[LOCKWAIT]` match `SITES.txt` only for this exact staged file. Any offset when patching breaks the map.

### G-1 (verified, pre-existing, affects "gates clean" coverage) shared lexer bug

- Confirmed: `harvest/no_detour_gate.py` `lex_cxx` (`:155-216`) sets `in_block=True` before it checks that the `/* ... */` closes on the same line, so every following line is blanked until the next `*/`.
- I measured it with a fixed copy of the lexer (scratchpad, harvest untouched):
  - patched `LuaMod.cpp`: 558 of 9840 lines hidden;
  - B6 `LuaMadeSimple.cpp`: 1434 of 1846 lines hidden. The B6 additions in that file are in the hidden part.
- With the fixed lexer, verdicts for base, B5 and B6 do **not** change (`no_detour` PASS; `fs_throw` same 12 violations; PS5-live lines go from 8658 to 10365). So the "gates clean" claims are true, but only after this check. The gate track found the bug correctly. Someone with write access to harvest should fix it (reset `in_block` when `*/` closes on the same line).

## Checked and OK (tried to break, could not)

- **Generic.** No mod, game or engine name added by `final-b5.diff` or `b6.diff` (the only hits are old comments already in the candidate). Mod name in `[LOCKLEAK]` and `[PERF]` comes from `get_name()` at run time.
- **No new `#include`.** `final-b5.diff` has no include line. `b6.diff` only adds `#if defined(__PROSPERO__)` lines; the source gate compared 105 include lines. `LuaMod.cpp` already includes `<atomic>`; `std::chrono`, `std::mutex` were already in use there (44 and 2 uses in base), and the syntax check compiles.
- **No detours.** `no_detour_gate` PASS on all four trees.
- **B5 semantics.** All 17 sites are in `register_function` lambdas, which go through `process_lua_function` -> `TRY` (`LuaMadeSimple.cpp:1726`), catching `std::exception&`. `TRY` copies the text into a 1024-byte buffer and raises after the handler exits. Longest real site message is 771 bytes of overload text at most for other functions, and the 17 sites are under 300 bytes plus a function name, so the 1023-byte cut does not bite (host test E reproduces the cut at 1500 bytes). `get_userdata` in `get_mod_ref_cxx` throws a C++ exception, not a longjmp. The script_hook site (`final-b5` line ~8712) has its own `catch (std::exception&)`, so it becomes a logged error like PC.
- **Lock behaviour of B6.** `ActionsLock` unlocks in the same order as before, clears the holder before `unlock()`, recursion works (4 threads x 20000 stress passes). No lock is held while a rate limiter's own mutex is used for logging. The pump uses a plain `try_lock` and no Lua call.
- **Rate limiting.** Widget line: 5 per 1 s window, then one summary on the next event or pump flush. `[LOCKLEAK]`: first per mod immediate, then every 30 s with a repeat count. `[LOCKWAIT]`: 5 lines per 10 s. The one gap: the `[LOCKWAIT]` summary is only emitted on the next over-threshold wait.
- **Markers.** All 7 narrow rows are the start of one contiguous `snprintf` format literal (`work/src/LuaMod.cpp:357, 439, 445, 515, 560, 609, 5323`); the shim row is a wide `STR` (`LuaPathShim.cpp:923`). Same pattern as the accepted `b2-lua-mem-note` (utf-8) and `b4-shim-write-area-dir` (utf-16le). `strerror` is already in the B4 PRX import list (`EXACT-IMPORT-IDENTITIES.json`: 2 hits). TLS: `thread_local` variables are trivial types (no `__cxa_thread_atexit`), same as the existing `NativeLuaActionDispatching`. The marker collision with the lock-fix design's `[LOCKLEAK] repaired` is documented.
- **Writes outside the track dir.** I found none attributable to these four tracks. `round8-prep/*` files changed during the same window belong to another workflow. No `.pyc` was written in harvest. Console IP appears nowhere in the staged tree.
- **Claims labelling.** Nothing is called console-proven; all are Mac-only. The `RESULT-all.txt` line "steps=9 failed=0 skipped=1" correctly makes `run_all_b6_checks.sh` exit 1. Not a pass, as the B6 summary says ("partial").

## Out of scope but worth a line in the launch report

B5 only stops the stall. With the minimap's three `RegisterHook` calls still refused (`HookProcessInternal=0`), the mod will still get a Lua error from each (same text as before). Whether the mod survives that is the refused-hooks track's job, not B5's. The first launch after B5 is the first console proof of C++ unwinding through `TRY` for these frames. Expected signs: more than one keybind entry, `[MEM] cxx=` up by 3, self-test starts.

## Suggested order before any build

1. Rebase B6 on final-b5 (regenerate `base/`, `work/`, `b6.diff`, patch-chain check).
2. Add the `get_mod_ref` ban and the two-lock-type coverage to one gate; make it fail when a guard type is declared but not parsed.
3. Fix the shim buffer or test, run `b6_shim_test`, keep `RESULT-shim.txt`.
4. Decide on `holder_set` clock cost (B6-4) and fix the doc claim either way.
5. Optionally fix `lex_cxx` in harvest (G-1) and re-run the two harvest gates.
