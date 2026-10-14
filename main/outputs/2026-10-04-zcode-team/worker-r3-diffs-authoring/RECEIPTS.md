# RECEIPTS INDEX — worker-r3-diffs-authoring (2026-10-04)

All hashes sha256. Deliverable order honored: CLAIM.json first, REPORT.md last.

## Deliverables

| file | sha256 | note |
|---|---|---|
| `CLAIM.json` | `800658a7b68744ad65cd0ab9605f3261e4a0ef7435ee1731345c17f075a0b2d1` | written first; amended once mid-session (RDA-1 prologue-order correction, RDA-3 honest -Werror method) exactly per the carrier-prep correction discipline |
| `diffs/r3-luamod-funcptr-marshal.diff` | `a8d1d6b4f277fe1af80aa947d5a1022af8b2c3c0cd8bf11e97a7674fc2b38d2a` | DIFF 1: 24907 bytes, 13 hunks (SettingsManager.hpp enum append + 12 LuaMod.cpp hunks) |
| `diffs/r3-initializer-r3-marshal-mode.diff` | `d556c4e7de66c4692829ede78ed1bea8859953130088a44fa56683a96bada80c` | DIFF 2: 10408 bytes, 6 hunks on the staged initializer |
| `ARM-DISARM.md` | `d4fc5dca689c5ffae295394be6650febb61293dd22cc1706ecb1d1ea234d13a7` | R3-mode arm/disarm procedure |
| `MATRIX-ROW-NOTE.md` | `43fcd1e7b64c4d5faffe49d4240063e1d04c292b4cf1eb30477ab5b1956d7267` | R8/R9 consumption note |
| `REPORT.md` | (written last) | session report |

## Verification receipts (this session)

| file | content |
|---|---|
| `receipts/apply-anchors.json` | 21 anchors, each verified occurrence-count == 1 before replacement, none aborted; per-file base + patched sha256 (patched copies: LuaMod `ce61f02a9557…`, SettingsManager `5b7c50f458bd…`, UnrealInitializer `5a755d96a3e5…`) |
| `receipts/diff-hashes.json` | diff file hashes at generation time |
| `receipts/compile-gate.json` | full gate record: toolchain paths, recipe sha, per-variant exit codes/diagnostics/object hashes, warning_parity (0 new kinds both units), werror_parity (0 new / 0 missing kinds incl. the ARMED initializer variant), gate verdict PASS |
| `receipts/compile-gate-run.txt` | raw gate stdout (all 10 variants) |
| `receipts/patch-dryrun.txt` | patch -p1 --dry-run clean for both diffs; applied output hash-identical to the anchored patched copies (the compiled units are the deliverable-diff bytes) |
| `host-tests/test_r3_marshal_mock.cpp` + `host-tests/build.stderr` + `host-tests/run-output.txt` | standalone logic mock, clang++ -std=c++20 -Wall -Wextra -Werror, `PASS: 24 checks` |
| `work/gate/*/command.json` + `compile.stderr` | verbatim argv and full compiler output per variant (reproducible) |

## Base pins verified before authoring

- `SRC:port/ue4ss/src/Mod/LuaMod.cpp` = `a2d07f7c…cc4ae` (== RECIPE luamod source_sha256)
- `SRC:port/ue4ss/include/SettingsManager.hpp` = `a9b4abbe…5aa2e`
- `TEAM:worker-query-s2-arm/staging/UnrealInitializer.cpp` = `933d08a2…2542` (composition-4-attested staged bytes)

## Toolchain provenance

`prospero-clang++` = `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++`
wrapper over `outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64` (Target: x86_64-sie-ps5),
env `LLVM_CONFIG` per unit as build_runtime.py passes it; argvs from
`.superpowers/native-dependency-spike/native-coherent-runtime-1/compile-recipes.json` (recipe sha recorded in
compile-gate.json), `@ROOT@` = the `what` workspace root (build_runtime.py's R), initializer staging includes
per the composition-5 `W_INCLUDE` flow. No spike-tree or staging-tree file was modified; all outputs stayed in
this worker's directory.
