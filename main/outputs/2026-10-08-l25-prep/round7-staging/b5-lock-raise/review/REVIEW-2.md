# REVIEW-2 (lens: RULES + RISK) - round 8: one lock gate, B6 rebased on final-b5, shim fix

Reviewer: adversarial, read-only. All results are **Mac-only** (no console contact, no PRX build, no packaging).
I wrote only this file inside `round7-staging/b5-lock-raise/`. Everything else I built or ran lives in the session scratchpad
(`.../scratchpad/r2/`). One exception: `gate/run_gates.sh` writes into `gate/results-r8/` and `gate/compiled-files-*.txt`; I backed them
up first, the lists came out byte-identical and I restored `results-r8/` (only the random temp-dir names inside the mutant logs differed).

## Verdict

- **B5 (final-b5.diff): stands.** Not refuted. Patch applies on the pristine base (sha 4d6bc63b) and gives afaf5caa; no new `#include`, no detour; gates clean.
- **Round-8 fixes B6-1 (rebase), B6-2 (one gate), B6-3 (shim): confirmed.** The compile tests that the author could not run now pass (below).
- **B6 claim "no new locking hazards" is refuted** by one reproduced defect (R2-1): the log call inside the lock-wait slow path can leak the mutex.
  Fix before building B6. It is a 3-line change and does not touch B5.
- `refuted=true` is set for that single B6 claim only.

## What I re-ran (all Mac-only; memory was 2.1-3.4 GB free, one compile at a time)

| Check | Result |
|---|---|
| Patch chain: pristine base -> `final-b5.diff` -> `b6.diff`, my own scratch copy, `patch -F0` | applies, 4 files, no fuzz; shas equal `SHA256SUMS-work.txt` (LuaMod.cpp 0c15fd2d, shim 7d1ed2e3, LMS.cpp dfe06c28, LMS.hpp 936a827e) |
| `gate/run_gates.sh` (real tree, 5 mutants, 7 breakers) | `checks=18 failed=0`; hard mode b5+b6: 3561 files, 186 scopes, 46 ActionsLock, 0 std guards left, 0 findings |
| `lock_raise_gate.py --selftest` | 115/115 |
| My own new mutants on the b5+b6 tree: raise after `NativeLuaActionDispatchScope action_dispatch;` and raise under the pump's raw `try_lock()` | both caught (exit 1, "under holder action_dispatch ... ActionsLock@5174" and "under manual ... b6_pump_1hz") |
| B5 host test (`host-test/build.sh`, full Lua rebuild as C, copy in scratchpad) | checked 63, failed 0 |
| B5 sites host test (`sites/hosttest/build_and_run.sh`) | checks=32 failures=0 |
| **B6 host + pump + shim tests (the ones marked NOT RUN), rebuilt from current `work/`** | **host 160/0, pump 23/0, shim 19/0**; BENCH: `std::lock_guard` 25 ns vs `ActionsLock` 52 ns (Mac) |
| **B6 mutation check (marked NOT RUN)** | 11 run, 0 survived |
| `prospero-clang++ -fsyntax-only`, round-8 `work/` LuaMod.cpp / LuaPathShim.cpp / LuaMadeSimple.cpp with `--overlay-include work/include` | all exit 0, 0 errors (LuaMod.cpp 23 warnings, the known set). Control with the OLD header: 6 errors, so the check bites. `syntax_check.py` works. |
| `no_detour_gate.py --root work` | PASS (0 sites, 4 LoadMap soft-refused notes, pre-existing) |
| `fs_throw_gate.py` base vs work | both exit 1 with the same 12 pre-existing violations, normalised diff empty (see R2-4) |
| `b6_source_gate.py` | PASS: 105 includes compared, 54 removed lines intended, 12 markers, 3 CORE slices, 46 tracked, pump body 68 lines with no Lua call |
| grep of `b6.diff` / `final-b5.diff` for mod/game names, offsets, detours, console IPs | none |

Net: the author's "do not build B6 until skipped=0" condition is now met on my copy (host, pump, shim, mutation all pass).
`RESULT-all.txt` in the staging dir still says `skipped=2`; whoever finalises should re-run `run_all_b6_checks.sh` once memory allows.

## Findings

### R2-1 (major, reproduced) a throw from the log glue inside `lock_slow` leaks `m_thread_actions_mutex` forever

- `b6diag::lock_slow` (`work/src/LuaMod.cpp` lines 451-469) calls `mutex.lock()` and then, if the wait was over 250 ms, `log_lockwait(...)`.
  That is inside the `ActionsLock` constructor, **after the mutex is taken and before the constructor returns**.
- `log_lockwait` -> `emit_line` (`work/src/LuaMod.cpp:686`). `emit_line` is not `noexcept`. It builds `StringType wide` with `push_back` **outside**
  `L22Diagnostics::observe` (which is the only part that swallows exceptions). A `bad_alloc` there leaves the constructor by exception.
  A constructor that throws never runs its destructor, so the recursive mutex stays owned by that thread for good. Every other thread then blocks on it.
  That is exactly the B4 failure class that B5 removes.
- Repro (scratchpad `r2/repro/emit_throw.cpp`, compiled against the shipped CORE slices): holder thread keeps the mutex 400 ms, main thread constructs
  `ActionsLock` while `emit_line` throws `std::bad_alloc`. Output: `ctor threw=1, mutex free for other thread=0`. Mac-only.
- Likelihood is low (needs a lock wait over 250 ms AND an allocation failure) but both happen together under the exact heap pressure the `[MEM] fails=` counters watch for.
  B6.md and the `on_game_thread()` comment claim the constructor "cannot throw" / adds no hazard; this path contradicts that.
- Fix (generic, no new include): wrap the `log_lockwait` call in `lock_slow` in `try { ... } catch (...) {}`, and/or make `emit_line` catch everything
  (move the widening loop inside a `try`). Add one host-test case where `emit_line` throws during a contended acquisition and assert the mutex is free afterwards.
  The test stub `emit_line` never throws today, which is why 160/160 did not see it.

### R2-2 (minor) stale code comment contradicts the corrected claim B6-4

- `work/src/LuaMod.cpp:449-450`: "Slow path of the acquisition: only reached when try_lock failed, so the uncontended path never reads a clock."
  This text is inside `b6.diff` and will ship. The code (`holder_set`, `LuaMod.cpp` ~line 466) reads `now_ms()` and calls `on_game_thread()` on every outermost acquisition,
  which B6.md:16 now says correctly. Change the comment.

### R2-3 (minor) MARKERS.md / SITES.md text is stale or wrong in places

- `MARKERS.md` (mtime 09:40, older than the 10:30 regeneration of `work/`) cites `LuaPathShim.cpp:921`; the `write-fail` literal is now at `:935`.
  It cites `emit_line` at `LuaMod.cpp:680-716` (now starts at 686) and the widget edit at `3181-3190` (now `3203`). The round-8 summary says MARKERS.md was updated; the line numbers were not.
  The marker texts and encodings themselves are fine: all 8 rows are contiguous literal prefixes (`"[LOCKLEAK] mod=%s ..."` etc. are single `snprintf` formats), `verify_build.sh` supports both `utf-8` and `utf-16le` per row.
- `sites/SITES.md:115` "Correction (round 8)": says the `actions_lock()/actions_unlock()` pairs are "manual lock calls on `m_thread_actions_mutex`". They are not.
  `actions_lock()` locks the **per-mod** `std::mutex m_actions_lock` (`native-coherent-runtime-1/source/port/ue4ss/include/Mod/LuaMod.hpp:192,233-239`).
  The conclusion (none is lock-then-raise) holds; the stated mutex is wrong, and so is the claim that the gate "models these pairs" as pairs on the actions mutex.

### R2-4 (minor, wording) "fs_throw_gate still clean" is not what the evidence shows

- `fs_throw_gate.py` exits 1 on the untouched B5 base too (12 pre-existing violations, e.g. `std::filesystem::exists` without `error_code` at the `script_path` checks). B6 adds none and removes none (identical normalised sets).
  The accurate claim is "no new violation", not "clean". Ledger text should say so, or the build driver must already allow the 12.

### R2-5 (notes, not defects)

- Helper-tier "0 NEW" is measured against `baseline-b5-patched-UNREVIEWED.txt`, which was written from the same tree. It proves "B6 adds no new helper site", not that the 38 are safe.
  They include `get_function_ref` under the lock in keybind, notify and `script_hook` paths (ambient-pcall residual, `sites/optional-ambient-getfuncref.diff`, not applied). B6's `[LOCKLEAK]` is the detector for those on the console.
- Pump `kind=pump` reports re-count: `g_leak_total` (the `leaks=` field of `[PERF]`) rises by one every second while the same depth stays stuck, so `leaks=` is "reports", not "distinct leaks". Log output itself is correctly limited to one line per mod per 30 s.
- `[LOCKWAIT] summary:` is only printed on the next qualifying acquisition after a window ends (the pump flushes the widget limiter but not the lock-wait limiter). A burst followed by silence never prints its summary. Harmless.
- `std::strerror` is new in `LuaPathShim.cpp`; the author's import argument (patched `lauxlib.c` already calls `strerror`) is a source inference and is correctly marked as such. Confirm with the import-contract check at build time.
- Thread-local use is not new to the PS5 build (`NativeLuaActionDispatching` is already `thread_local` in LuaMod.cpp), so the new `thread_local` ints in `LuaMadeSimple.cpp` and the 256-byte buffer in the shim add no new platform feature. Console behaviour is still unproven (Mac-only).

## Rule checklist (lens)

| Rule | Result |
|---|---|
| Generic, no mod/game names, no offsets | OK (grep clean) |
| No new `#include` | OK (`b6_source_gate` 105 compared; `<atomic>` already in base LuaMod.cpp line 9; `<cstring>` already in the shim) |
| No detours | OK (`no_detour_gate` PASS) |
| Gates print checked counts and fail on zero | OK (3561 files / 186 scopes; unknown wrapper exits 2; empty input exits 2) |
| B6 behaviour change | Only logging and counters: `[l22]` per-widget line limited to 5/s with a summary, `[MEM]` from the ExecuteInGameThread path skipped when the pump printed one in the last 2.5 s (the `return` is inside the `observe` lambda, nothing after it is skipped except the same diagnostic events the pump emits). OK |
| B6 new locking hazards | **R2-1** (log under lock after acquire can leak the mutex). Everything else: lock order is actions mutex -> rate-limiter mutex, limiter mutex is never held while logging; pump takes the actions mutex only with `try_lock` and unlocks without a throwing call between |
| Rate limiting | Correct per the host test (5 lines/10 s lock-wait, 5/s widget, 1 per mod per 30 s leak, 1 per slot per 5 s stuck-waiter, PERF at most once per 900 ms) |
| Markers realistic for `verify_build` | Yes (R2-3 is only line numbers) |
| Nothing written outside the track dir | OK as far as I can see; the author's scratch use is under the scratchpad |
| No console contact, no builds | OK |
| Claims labelled | Mostly. "fs_throw clean" and "no new locking hazards" are the two that read stronger than the evidence (R2-4, R2-1) |

## Recommended next step

1. Apply the R2-1 fix to `parts/apply_luamod.py` (try/catch around `log_lockwait` in `lock_slow`; `emit_line` fully guarded) and add the throwing-glue host-test case.
2. Correct the comment (R2-2), MARKERS.md line numbers (R2-3), the SITES.md sentence (R2-3), and the fs_throw wording (R2-4).
3. Regenerate `b6.diff` and the shas, re-run `run_all_b6_checks.sh` once (expect `skipped=0`), then B6 can be built as its own labelled build on top of B5.
