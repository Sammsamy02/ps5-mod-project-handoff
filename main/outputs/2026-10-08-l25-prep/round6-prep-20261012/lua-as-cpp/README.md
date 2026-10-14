# lua-as-cpp (round 6 prep, 2026-10-12) -- Mac-only

Question: does compiling Lua as C++ fix the PS5 Lua memory growth? Start with
**FEASIBILITY.md** (plain verdict first; round 2 replaces round 1). No console
contact, no console logs read, no PRX built, nothing outside this folder written.

## Round 2 (current)

| Path | What it is |
|---|---|
| `FEASIBILITY.md` | Verdict, the 5 answers, hazards H1-H5, next steps (file:line cited) |
| `REVIEW.md` | The adversarial review of round 1 (input to round 2; not written by this worker) |
| `host-test-v2/run_v2.sh` | Builds the port's real Lua 3 ways (c / cpp / cppcabi) and runs the 3 programs below against each. One compile at a time, strict free-memory check before each. Exits non-zero on any failure or zero checks |
| `host-test-v2/lua_c_abi_prefix.h` | **Staged** forced-include header: Lua compiled as C++ but keeps C names (sha `96b3f261…`) |
| `host-test-v2/replicas_test.cpp` | Review replicas A (`LuaUObject.cpp`) and B (`LuaMadeSimple.cpp`), current vs fixed, plus coroutine yields |
| `host-test-v2/c_caller.c` | Plain-C caller using the same 8 Lua calls as frozen `lineage/probe/memory_lua.c` |
| `host-test-v2/lua_as_cpp_host_test.cpp`, `luauser_counting_stub.c` | Round-1 test and lock stub, copied (one `#if` widened so the C-name build counts as C++) |
| `host-test-v2/RESULT-V2.txt`, `run_v2.log`, `build-*/` | Results: 90 checks, all as expected (sha `77c375ef…`); per-build outputs and objects (Mac only) |
| `ps5-xcompile-probe-v2/probe2.py` | Compiles the 33 Lua units once each with the PS5 toolchain and the staged flags (no link, no PRX). Then replays the include gate, checks symbols by exact name (runtime + 13 packaging objects), predicts imports |
| `ps5-xcompile-probe-v2/PROBE-RESULTS.json`, `probe2-run.log`, `probe2-analyze.log` | 33/33, 0 warnings; 79/79 symbols keep their names; PRX imports stay 207 |
| `ps5-xcompile-probe-v2/objects/` | PS5 analysis objects. **Never link or upload these** |
| `audit/catch_all_audit_v2.py`, `CATCH-ALL-AUDIT-V2.json`, `catch-all-audit-v2.log` | Call-graph audit: 152 `catch(...)`, 21 flagged (incl. both review sites), 12 noexcept/destructor sites, 0 pointer throws |
| `staged-patch/v2/0001-v2-l26-lua-as-cpp-c-abi-opt-in.patch` | **Staged, NOT applied.** `build_variants.py` opt-in `L26_LUA_AS_CPP=1` + new prefix header (sha `3545c48e…`) |
| `staged-patch/v2/0002-runtime-catch-all-passes-lua-errors.patch` | **Staged, NOT applied.** 6 `catch (void*) { throw; }` handlers in 4 runtime files (sha `3e52a46f…`) |
| `staged-patch/v2/make_build_patch.py`, `make_runtime_patch.py` | Regenerate both patches from the real files (base-sha checked, exact-match edits, fail on zero) |
| `staged-patch/v2/build_variants.patched.py` | The patched driver text (for review/harness only; not used by any build) |
| `staged-patch/v2/selection-check/` | `selection_harness_v2.py` (read-only selection run; fails on failed gates/zero counts): OFF = B4's 205, ON = 205 + 33; `argv_check.py`: staged argv = probe argv for 33/33 |
| `tools/memcheck_strict.sh` | Refuses a compile when vm_stat "Pages free" < 2 GB (round 2 uses this strict rule) |

Re-run: `sh host-test-v2/run_v2.sh`; `python3 -I ps5-xcompile-probe-v2/probe2.py [--analyze-only]`;
`python3 -I audit/catch_all_audit_v2.py`; from the worktree root:
`python3 -I staged-patch/v2/selection-check/selection_harness_v2.py staged-patch/v2/build_variants.patched.py off`
(then `L26_LUA_AS_CPP=1 … on`). Every round-2 script prints its counts and exits non-zero on a
zero count.

## Round 1 (kept as history, SUPERSEDED -- do not use)

`host-test/`, `ps5-xcompile-probe/`, `audit/catch_all_audit.py` (+ JSON/log),
`staged-patch/0001-l26-lua-as-cpp-opt-in.patch`, `staged-patch/selection-check/`, `tools/memcheck.sh`.
Superseded because: the round-1 patch used upstream C++ names (breaks the plain-C packaging unit)
and changed packaging to 205 imports (wrong: stays 207). The round-1 audit missed the 2 review
sites, and the round-1 `selection_harness.py` never exits non-zero (review #8; the round-1 README
claim to the contrary was false).

These results belong in the evidence tree (`outputs/2026-10-08-l25-prep/evidence/`). This worker
may only write to this folder, so the driver session should copy them there.
