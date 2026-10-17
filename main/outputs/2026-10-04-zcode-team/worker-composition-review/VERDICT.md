# VERDICT — worker-composition-review (2026-10-04)

## ALL_CLEAR (no blocking mismatches)

Independent review of the controller's composition-script extension
(`.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/build_runtime.py`)
and the supplier composition receipt
(`ue4ss-native-supplier-composition-1/run-1/receipt.json` + `undefined-delta.json`).

## Summary per review target

1. **Extended build_runtime.py mechanism — SOUND.** Extras require
   `--extend-undefined-baseline` (L66); the flag without extras still raises on any
   undefined-delta (L93-94-102); with no extras every added/removed/changed symbol is
   refused exactly as the original did (reference: `native-1` receipt from the
   pre-extension script tolerated zero delta). All original invariants — base-receipt
   sha pin, host/rust receipt hash checks, originals input-hash checks, prior-composite
   refusal, exactly-one-old-Rust check, unit source/object pins, fresh-dep
   must-be-in-pins, affected-inventory set equality (L42) — execute before any extra
   handling. `-D` defines are inserted only inside the extras loop and were verified
   absent from all baseline fresh compiles. Extras append at the end of the link input
   order.

2. **Provider gate — REAL.** It runs `llvm-nm -D --defined-only` over every
   `sdk-v0.43/.../target/lib/*.so` (34 `sdknm-*` commands, exit 0, stdout saved) and
   raises when an added symbol has no provider. Re-verified independently with the
   pinned LLVM-21.1.8 `llvm-nm`: `libkernel.so` defines `sceKernelGetModuleInfo`
   (`T 1218c`), and the exporting set is exactly the four libkernel variants named in
   the receipt. `linker_provided={'_DYNAMIC'}` is minimal and justified: no SDK library
   exports `_DYNAMIC` (checked all), and it is the standard ELF dynamic-section symbol
   defined by the final PRX link.

3. **Composition receipt — CONSISTENT.** added = exactly
   `[_DYNAMIC, sceKernelGetModuleInfo]`, removed/changed empty (independently
   recomputed from the 117 MB object; my undefined set equals the receipt's 199-symbol
   set, and equals native-1's 197 + exactly those 2). Runtime object sha256
   `fb2f4dd1...75c` and byte size 117,624,920 match the file. 10 fresh units =
   6 affected + `native-ownedserializedelfsource` + 3 lmf; 353 retained (363 total =
   native-1's 360 + 3 extras). All 66 `extra_pinned` entries are
   `ps5-payload-sdk/target/include` kernel headers from the SCE TU — no project file
   unpinned (bucketed all three lmf `.d` files: zero deps outside ROOT, zero unpinned
   spike deps). Extra attestation receipt (worker-s1-adapter) hash matches.

4. **Vendored SCE TU — FAITHFUL.** Diff vs
   `outputs/2026-10-04-zcode-team/worker-1-live-supplier/src/live_module_os_ops_sce.cpp`
   shows only the `#ifdef`/`#else`/`#endif` around `#include <dlfcn.h>` (+ comment) and
   around the two dl-retention functions; both `#else` branches are byte-identical to
   the worker-1 original, so with `LMF_DISABLE_DL_RETENTION` absent the TU is
   functionally the original. Degraded returns are documented inline and match the
   real path's failure conventions (retain → `nullptr`, release → `-1`).

5. **dl cross-check — CLEAN.** Pinned `llvm-nm` over the composed object: zero
   `__dl*`/`dlfcn`/`dlopen`/`dlsym`/`dlclose` undefined symbols. The broad
   case-insensitive sweep finds only `sceKernelDlsym` (PS5 SDK module API, already in
   the pre-extension baseline, exported by `libkernel.so` — not a libdl import).

## Minor non-blocking observations

- O1: provider-oracle `.so` files are nm-verified but not hash-pinned in the receipt.
- O2: extra-unit deps resolving outside the worktree ROOT would escape pinning
  (none did in run-1; all 1003 dep files were ROOT-local).
- O3: `--extra-receipt` is optional; run-1 supplied one and its hash verifies.
- O4: no committed pre-extension script exists for a byte-level diff (file staged,
  never committed); original behavior established via the native-1 receipt
  (pre-extension script pin `aaf8f7d2...`) plus code-path analysis.

Details and citations: `findings.json` and `REPORT.md` in this directory.
