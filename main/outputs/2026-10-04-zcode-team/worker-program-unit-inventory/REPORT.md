# REPORT — worker-program-unit-inventory

**Verdict: COMPLETE. The composition-6 program-unit input inventory is delivered and every claim is
rehearsal-verified. The comp6-driver can consume `PROGRAM-UNIT-INVENTORY.md` + `dependency-manifest.json`
directly. Zero unexpected drift found; exactly two expected content diffs (the two staged files).**

## What was done

1. **Argv extraction.** Unit `program` found in composition-4 run-3 `receipt.json`
   (`.superpowers/.../ue4ss-native-supplier-composition-4/run-3/receipt.json`, `selected_runtime_units.program`,
   retained unit, 353 retained / 15 fresh). Its full 176-element compile argv extracted verbatim
   (`program-argv.json`): prospero-clang++, 40 `-I` occurrences (34 distinct dirs; the ue4ss include dir repeats
   at argv indexes 2/46/54/64), 33 `-D` defines, flags `-std=c++23 -fms-extensions -O0 -fno-lto -fPIC
   -ferror-limit=8 -Wno-unknown-pragmas -Wno-unused-parameter -Wno-unused-variable`, tail
   `<src> -c -o program.o -MD -MF program.d`. Receipt pins `source_sha256=dfdd5014…` and
   `object_sha256=cae3279b…` both VERIFIED on disk before any compile. `LLVM_CONFIG` env taken from the
   receipt field, applied exactly as `build_runtime_composition4_fixed.py` does (`dict(os.environ, LLVM_CONFIG=…)`).

2. **Control compile (unpatched original, exact argv, only `-o`/`-MF` redirected into this dir).**
   - `control.o` sha256 `cae3279b7b3ef63d50f4e01e931b3b24ad6b5ab77fee078c7851f33392df064a` — **bit-identical**
     to the historical run-3 object (same 3,881,064 bytes). The argv replay is exact; determinism proven.
   - `control.d` dependency set == historical `program.d`: **1328 == 1328, zero difference**.

3. **Patched compile (controller `outputs/2026-10-04-zcode-controller/comp6-integration/UE4SSProgram.cpp`,
   sha256 `75abf2ab…`).** Two rehearsals:
   - `patched-literal` (frozen includes untouched): **exit 1, 3 errors** — all
     `no member named 'configured_game_directory' in 'RC::Platform::NativeProgramContext'` at patched lines
     433/775/776. Hard evidence that the driver MUST stage the new `Platform/NativeProgramContext.hpp`
     (controller `Platform-NativeProgramContext.hpp`, sha256 `0a068bb9…`) before the fresh compile; the frozen
     header (`66499615…`) alone cannot compile the patched source.
   - `patched-staged`: first `-I` dir substituted with an in-dir symlink shadow of the frozen
     `source/port/ue4ss/include` (15 files; sole real file = the new header). **Exit 0**, 6 pre-existing-style
     warnings, 0 errors; `patched-staged.o` 3,883,600 B, sha256 `135392038b21a9d8af6221480ba4a0099a325227ca2dae3d23a711bf715a4a24`.

4. **Delta analysis (control vs patched).**
   - `.d` file sets: **identical** (1328 each). Raw diff is 11 ↔ 11 paths — exactly the 11 headers resolving
     from the shadowed first `-I` dir (10 realpath-identical symlinks + the copied header). No dependency added
     or removed by the patch; in the driver's real run (staging at canonical paths) the fresh `.d` set equals
     control's.
   - Object delta (llvm-nm/llvm-objdump, raw evidence in `*.nm`/`*.dis`/`*.strings`): all differences are the
     renumbered local-label cascade plus — 2 functions only in control (`initializer_list::begin/end` of the
     removed two-path loop), 1 only in patched (the new `lexical_path_valid` lambda), and 6 size-delta
     functions fully attributed to the hunks: `ValidateNativeProgramContext` −452 (header hunk),
     `setup_paths` +164 (hunk 2), program ctor +156 (hunk 1), `NativeProgramContext` move-ctor/dtor +54/+29
     (two new path members), `init()` +3 (knock-on). Two new string literals (the header's
     invalid_argument messages). No unexpected symbol appears or disappears.
   - Frozen comparison over all 1328 deps (`dependency-manifest.json`): 309 frozen-covered entries —
     **unexpected diffs: 0**; the only frozen-hash overrides are the two staged contents above. 1018 deps
     (934 PS5-SDK under `outputs/ps5-mod-bridge/…`, 84 vendor-spike headers) are not in
     `frozen-source-hashes.json` by design; all hashed in the manifest.

5. **Deliverables** (this dir, writes only here):
   - `CLAIM.json` (first), `REPORT.md` (last)
   - `PROGRAM-UNIT-INVENTORY.md` — argv verbatim (all 176 elements), rehearsal results, delta summary, §3.2
     replace-receipt pin list, complete 1328-row per-file sha256 table
   - `dependency-manifest.json` — machine-readable pins (`path/category/sha256/effective_sha256/frozen_sha256/status`)
   - `program-argv.json`, per-variant `*.commands.json`/`*.receipt.json`/`*.stderr`/`*.d`/`*.o`,
     `rehearse_compile.py`, `staged/ue4ss-include/` shadow

## Driver-facing facts that matter

- The fresh program-unit compile needs EXACTLY two staged contents beyond the frozen tree:
  `UE4SSProgram.cpp` → `75abf2ab77791f4e0aa2c068164d95d7737aefdcf0536a791f40599cd4ffb574` and
  `Platform/NativeProgramContext.hpp` → `0a068bb9d9b25f9b7213d584dabf26cadc793c42c93a0bb125f37d83c0a58f48`.
  Anything else hashing differently during the real run is genuine drift (control replay is bit-exact).
- The fresh `.d` file set will be identical to the historical 1328-file set; `pins ∪ rr` coverage per §3.2.2
  can be checked against `dependency-manifest.json` verbatim.

## Bounds and honesty

No writes outside this dir; the frozen tree, spike trees, and controller outputs were only read. No engine or
source edits. Rehearsal compiles only — no link, no PRX packaging, no console claims. The staged-shadow
path-substitution artifact (11 ↔ 11) is documented rather than hidden. Tool calls used: ~25 (within 30–60
budget including this report).
