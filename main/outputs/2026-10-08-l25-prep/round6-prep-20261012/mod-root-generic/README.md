# mod-root-generic (round-6 prep, 2026-10-12, revision 2) — Mac-only

Goal: remove Townfall-only values from the runtime entry and the in-game self-test so
the Stellar Blade blind test needs no game-specific code. Nothing here was applied,
uploaded, packaged or built into a PRX. No console contact.

Start with **DESIGN.md** (plain verdict first). `REVIEW.md` is the adversarial review
that revision 2 answers.

**Never apply these patches in place.** The files they target are frozen, sha-pinned
build inputs (`package_combined.py:53` restages them for every l24/l26 build). To ship,
copy the `staged/` file to a new labelled location and repoint the build input
(`outer_entry`) there, in its own labelled, gated build run by the driver session.

| File | What it is |
|---|---|
| `DESIGN.md` | Verdict, which file the build compiles, inventory (runtime + tools + self-test), title-source design incl. the process-app-info evaluation, console-only proofs, decisions |
| `REVIEW.md` | The adversarial review of revision 1 |
| `0001-runtime-entry-generic-mod-root.patch` | Staged diff (NOT applied) against the compiled entry `outputs/2026-10-08-combined-outer-lifecycle/candidate-v2/runtime_module_entry.c` (sha `bc490893…`). Patch sha `3ccf0364…d2283` |
| `staged/lifecycle/runtime_module_entry.c` | The file 0001 produces (sha `721c4037…0530c`); every test below ran on exactly this file |
| `0002-ueselftest-title-free-root.patch` | Staged diff (NOT applied) of `ueselftest/gen_selftest.py` + `ueselftest/main.lua`: title-free self-test `ROOT`. Patch sha `a804068e…3c4e` |
| `staged/ueselftest/gen_selftest.py`, `staged/ueselftest/main.lua` | The files 0002 produces |
| `staged/ueselftest/gen-out-original/`, `gen-out-patched/` | Generator output of the original and the patched generator (only `main.lua` differs, by the ROOT lines) |
| `scan_game_constants.py` | Runtime inventory scanner (compiled-file lists / `.d` files / manifests); exits 1 on zero files or any zero-path input |
| `scan_tool_constants.py` | Tool / in-game Lua / launch-data scanner for `outputs/2026-10-08-l25-prep`; classifies each hit file; exits 1 on zero files or any empty root |
| `target_check.sh` | PS5 toolchain syntax + object compile of the staged entry with the packaging flags, logs the full command, lists new undefined symbols and checks each is an existing PRX import |
| `host-test/make_fixtures.py` | Writes 35 fixture trees (fake console paths); exits 1 on zero cases |
| `host-test/test_title_derive.c` | Host test: `#include`s the staged entry, stubs the platform, runs `start_composed_runtime()` per case; `-DHOST_UNBOUND_PREAD` variant tests the unbound-binding refusal |
| `host-test/run_host_test.sh` | Memory guard + fixtures + compile/run of the main test, then compile/run of the unbound variant |
| `host-test/run_mutants.sh` | 6 broken copies of the staged entry; each must be caught |
| `host-test/test_selftest_root.lua` | Where the self-test progress file lands on another game (host mirror of the path shim); proves finding A9 |
| `host-test/fixtures/`, `host-test/out/` | Generated fixtures (`CASES.json`) and test outputs |
| `evidence/game-constant-scan-B4.txt/.json/.tsv`, `b4-compiled-set.json` | Runtime scan: 4,182 files checked, 176 hits (124 in B4-compiled files) |
| `evidence/tool-constant-scan.txt/.json/.tsv` | Tool scan: 211 files checked, 105 with hits, 21 tools + 6 in-game Lua files |
| `evidence/scanner-zero-input-negative.log` | Both scanners exit 1 on a zero-path input / empty root |
| `evidence/host-test-run-3.log` | Current host test run: 35/35 + 17/17 + unbound 1/1 PASS (runs 1-2 = revision 1, kept) |
| `evidence/host-test-mutants-2.log` | Current mutant run: 6/6 caught (`host-test-mutants.log` = revision 1, kept) |
| `evidence/selftest-root-test.log` | Self-test root check: 6/6 |
| `evidence/target-check.log` | Current PS5 toolchain check with full command (`target-syntax-check.log` = revision 1, kept) |
| `evidence/staged-runtime_module_entry.o`, `staged-entry-undefined.txt`, `b4-entry-undefined.txt` | Symbol check object (never packaged) and undefined-symbol lists |

Re-run (Mac only): `host-test/run_host_test.sh`, `host-test/run_mutants.sh`,
`sh target_check.sh`, `lua host-test/test_selftest_root.lua <l25-prep>/host-lua-harness/path-shim.lua`.
