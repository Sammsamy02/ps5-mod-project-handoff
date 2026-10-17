# b7-lua-cpp (round 8 prep, 2026-10-12) -- Mac-only

Groundwork for **B7 = Lua built as C++** (errors unwind as C++ exceptions instead of longjmp). No console contact, no
console log read, no PRX built, nothing outside this folder written, no tree edited (all patches are staged, not applied).
Start with **B7-PLAN.md** (plain verdict first), then **CATCH-AUDIT.md**.

| Path | What |
|---|---|
| `B7-PLAN.md` | Verdict, upstream parity, toolchain proof (proven vs not), the two-build plan, gates, size, rollback, decisions |
| `CATCH-AUDIT.md` | Every `catch (...)` / `catch (std::exception&)` in the compiled runtime: what it wraps, can a Lua error cross it, swallow = wrong/harmless; noexcept, destructors, raw Lua callbacks, engine-entry points; proposed edits |
| `staged-patch/0001-build-variants-lua-as-cpp-opt-in.patch` | `build_variants.py` opt-in `L26_LUA_AS_CPP=1` + prefix header (copy of round-6 v2 patch, sha `3545c48e...`; `git apply --check` clean on the current file, not applied) |
| `staged-patch/b7-on-{candidate,b5final,b6}/` | `b7-runtime.patch` (E1-E10 + startup probe, 13 sites in 5 files), `EDITS.json` (shas before/after), `tree/` (the patched files). Base trees: B4 candidate; candidate + final-b5 `LuaMod.cpp`; the round-7 B6 work copies as of generation time |
| `staged-patch/b7-h4/` | **new after review P1:** `ScriptHookThunks.h4.patch` (H1-H3: `catch (...)` at the three places where the game's hook thunk hands control back; one patch for all bases) + `EDITS.json` |
| `staged-patch/b7-h4-on-{candidate,b5final,b6}/tree/` | the B7 tree plus H1-H3: the overlay all gates and `ps5_compile_check.py` now use |
| `staged-patch/add_build_markers.B7-set.txt` | 5 `verify_build` markers for the driver session (harvest/ is not mine to edit) |
| `tools/make_b7_runtime_patch.py` | Regenerates the runtime patch from the real files (anchor based, base-sha checked, fails on zero) |
| `tools/catch_inventory.py`, `tools/cfunction_audit.py` | The catch-site and raw-Lua-callback inventories (extend round-6 `catch_all_audit_v2.py`; file set = B4 composition receipt + `.d` files) |
| `tools/make_b7_h4_patch.py` | Regenerates the H1-H3 patch and the `b7-h4-on-*` overlays (anchor based, sha pinned, fails on zero) |
| `tools/b7_source_gate.py` | **Pre-compile gate**: A catch pass-through, B lock-raise (kept after B7), C raw callbacks, D noexcept, E game boundary (new). `--overlay DIR`, `--strict-h4`, `--selftest` |
| `tools/lock_raise_gate_b7.py` | The round-7 "error raised while a lock is held" gate kept and extended (same CLI as the original) |
| `tools/gate_mutation_test.py` | 8 cases: clean passes, 6 injected defects fail (incl. removing H1-H3), `--strict-h4` fails |
| `tools/b7_postlink_check.py` | **Post-link check** on the runtime object: Lua really is C++ (or C), names, imports |
| `tools/ps5_compile_check.py` | Compiles the 5 staged files once each with the PS5 toolchain and the units' B4 argv; include-graph and marker check |
| `tools/elf_sections.py`, `tools/memcheck_strict.sh` | stdlib ELF reader (sections, program headers) and the 2 GB free-memory refusal |
| `gate/` | Allow-lists/baselines with a reason per entry (`catch-allowlist.txt`, `cfunction-baseline.txt`, `noexcept-baseline.txt`) |
| `host-test/` | `run.sh`, `b7_host_test.cpp`, `extract_real.py` (real runtime code, byte for byte), `RESULT-before-void-catch-probe.txt` (last complete run), `RESULT-NOTE.txt` (status) |
| `audit/` | `CATCH-INVENTORY*.json/.log`, `CFUNCTION-AUDIT.*`, `section-sizes-c-vs-cpp.txt`, `postlink-check-*.txt`, `gates/` (fs_throw, no_detour), `gate-run-*.txt`, `ps5-compile-check-*-SKIPPED-lowmem.*` (the PS5 compile did not run: memory) |

Re-run (all from this folder; every script prints its counts and exits non-zero on a zero count; the compiles refuse
below 2 GB free memory):

```
python3 -I tools/make_b7_runtime_patch.py --base candidate     # also b5final, b6
python3 -I tools/catch_inventory.py [--overlay staged-patch/b7-on-b5final/tree]
python3 -I tools/make_b7_h4_patch.py                            # after make_b7_runtime_patch.py for all three bases
python3 -I tools/b7_source_gate.py --overlay staged-patch/b7-h4-on-b5final/tree
python3 -I tools/gate_mutation_test.py <scratch dir>           # about 3 minutes
sh host-test/run.sh                                            # about 10 seconds when memory is free
python3 -I tools/ps5_compile_check.py b5final <scratch dir>    # PS5 toolchain, one unit at a time
python3 -I tools/b7_postlink_check.py --expect cpp --runtime <runtime.o> --baseline-undefined <undefined.stdout>
```

These results belong in the evidence tree (`outputs/2026-10-08-l25-prep/evidence/`); this worker may only write here, so
the driver session should copy them there.

## Changes after the review (2026-10-12; `REVIEW.md` is the reviewer's file)

- **P1 (major, fixed in text, gate and staging):** native-UFunction hooks ARE live on the PS5 (B4 launch-1 `D:807, D:818,
  D:866-D:894, D:918`); only Blueprint-function hooks are refused. `B7-PLAN.md` (verdict 6, sections 6, 7, 8.3) and
  `CATCH-AUDIT.md` (verdict, 3f, 5) rewritten; new patch H1-H3 and gate check E; allow-list reason of `script_hook`
  corrected. The first version's `lua_pcall`-trampoline remedy is withdrawn (it breaks the pre/post stack protocol).
- **P2 (minor):** the `UE4SSProgram.cpp:1899` allow-list reason was wrong; indirect callees read by hand (CATCH-AUDIT 3e-bis).
- **P3 (minor):** `lua_close` is called; 11 pass-through blocks, not 12; section-3 PRX numbers re-measured on the file that exists now.
- Still not done: PS5-toolchain compile of the 6 staged files, the extended host test, and a host test for H1-H3 (the Mac stayed under the 2 GB free-memory rule).
