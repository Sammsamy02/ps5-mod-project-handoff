# NEXT-BUILD.md — what the next l26 build adds beyond 5ea2c845 (the verified-query console build)

Written by agent EE, 2026-10-08, after integrating the two Claude-review-3 /
user-approved l26-production items into the tree. This change set lands AFTER
the current verified-query build's console verdict; NOTHING here was built,
packaged or uploaded yet. Mac-local only; no console contact; proposal-grade
runtime code now staged in the tree with host/prospero compile + fixture
proofs (below).

## Baseline identity

- Production PRX of the current verified-query build (variant A):
  `outputs/2026-10-08-l26-package-A/pass-1/modbridge_supplier_composition.prx`
  sha256 `5ea2c845ba4a01dde4599c08f1930262b394e76279d1bfbaa235a19499dbfc51`
  (FINAL-RECEIPT-VARIANT-A.json; pass-2 twin identical; 207 imports).
- That build's runtime source state: typescan-r4 CHANGES rows as of the DA
  verified-query fixes; UObjectArray.cpp `ea7221e8...`, UnrealInitializer.cpp
  `cac95e48...`, UObjectArray.hpp `69a2391a...`, bridge (outer_bridge)
  `f510c014...` (agent-Q candidate, ungated).

## What the NEXT build adds (this change set)

### 1. VARIANT C — engine-side delete-listener registration (production default)

- `typescan-r4/candidate/src/UObjectArray.cpp` now
  `cbe50d15ad024932b297848360119a50b9357aadcec6cdd8b4576586a3dfb119`:
  `variant-c-proposal/variant-c-patch-rebased.diff` applied byte-clean onto
  ea7221e8 (BC's recorded intermediate 72d17fc8 verified), then the EE
  fail-closed rework. Under `MODBRIDGE_L25_ENGINE_ADD_LISTENER` the
  Add/Remove callee delegates to the ENGINE'S OWN
  `FUObjectArray::Add/RemoveUObjectDeleteListener` — locked (recursive
  pthread mutex at this+0x78) and engine-allocator-backed (engine GMalloc
  global 0x9340950 -> vtable+0x40), never our port's bindings or our
  hand-rolled Append. All l25 instrumentation kept.
- FAIL-CLOSED admission chain (the review-3 requirement):
  - NEW `typescan-r4/include/EngineListenerOpsProfile.hpp`
    `39cbe0e83713c69295caed6353d0dc7f487d9ed7de23effa39ccc04a8ed0f673`:
    image-pinned labels Add 0x4841A0 / Remove 0x484240 (runtime
    0x8841A0/0x884240 at base 0x400000) + the two 16-byte prologue
    signatures, pinned to image sha 3b6cfad4... (the seed table's image).
    Bytes re-verified by EE against the pinned eboot at file offsets
    label+0x4000 BEFORE embedding (exec PT_LOAD off 0x4000/vaddr 0).
  - `UnrealInitializer.cpp` now
    `4b831a7a060dc7883ab51a52e78741d9d2536480c871f642ccd902539deebec0`:
    Phase 6 of `consume_required_slot_seeds` (C switch only): CodeCallOnly
    class checks for BOTH labels, shared-budget check, then
    `code_identity` + `cached_code_signature` (the exact slot-4 SCO pair)
    against the cached 16-byte disk prefixes — ANY mismatch =>
    `seed_refused` (terminal, loud, nothing bound): registration can never
    run, engine functions never called. On success the two VERIFIED runtime
    pointers are published via the new
    `UObjectArray::SetupEngineDeleteListenerOps` (declared unguarded in
    `UObjectArray.hpp`, now
    `51e44f9a0c8d4bac0840ca5c528b47a5f18386377e31f8b562003b563fd0e4c9`;
    no data members, no ABI change).
  - Callee-side: the engine-call resolver returns ONLY the
    admission-published pointers; while unset it refuses with NEW code 8
    (`EngineListenerOpsUnverifiedByAdmission`) and never calls; a
    published-vs-label-derived disagreement refuses with NEW code 9
    (`EngineListenerOpsAddressMismatch`) and never calls; null GUObjectArray
    keeps code 1; misaligned derived base keeps code 7.

### 2. Q/Z15 copyin-gate early release (LABELLED bridge change)

- The CONSUMED bridge candidate
  `copyin-probe/candidate/lifecycle/runtime_start_bridge.cpp` is now
  `94f81a17d6904b9edfe8b06e8e4284268fbd4c738f96ac4475931225e719274c` =
  f510c014 (Q's adopted candidate, the 5ea2c845 outer_bridge) +
  `gating-proposal/gating-patch.diff` (Z15; applied with /usr/bin/patch
  --fuzz=0, clean; result byte-identical to Z15's proof candidate 50caba3b)
  + a 3-line `[Seeds][L26]` marker comment (the ONLY delta vs 50caba3b).
  Effect: `bridge_copyin_initializer_complete()` disarms the copyin owner
  gate at admission completion (`copyin_probe_gate_l26_early_release` event
  + `g_copyin_initializer_owner.leave()`); probe_until and gate siblings
  fail closed for the rest of the session; the shutdown leave() becomes an
  idempotent no-op. Pin updated in `copyin-probe/MANIFEST.json`
  (`l26_gating_note`); own CHANGES entry in
  `gating-proposal/CHANGES.json` (composition chain 89e4b643 -> f510c014 ->
  50caba3b -> 94f81a17 documented).

### 3. Build drivers repointed (production default = C-on, B removed from production)

- `build_variants.py` now `549b54dd1b8d14981d9f0244d8757792acadb99a41d5498134fdc1d95debd407`:
  variant A = production, defines `-DMODBRIDGE_L25_ENGINE_ADD_LISTENER`
  (NO skip token); variant B = diagnostic only, keeps the
  `MODBRIDGE_L25_SKIP_DELETE_LISTENER` skip; per-token define-unit
  computation (B: luamod+unreal-unrealinitializer; C:
  unreal-unrealinitializer+unreal-uobjectarray; conservative superset
  applied because the new profile header is a shared added header);
  pair-receipt G10 allowed-set = union of both tokens' define units.
- `l26_packaging_driver.py` now
  `68146c3f88a91f9eee10b90f9f80744014ff552536ba3dd8fb342a8e8a8bfd51`:
  variant A note updated to the variant-C production text; the gated bridge
  (94f81a17) flows through the existing outer_bridge pin path.

## Switch matrix (documented; also in typescan-r4 CHANGES.json)

| build | SKIP token | ENGINE_ADD token | registration path |
|---|---|---|---|
| production (l26-A) | undefined | DEFINED | engine-call via admission-verified pointers (variant C) |
| diagnostic (l26-B) | DEFINED | any (B wins) | call sites compiled out — no registration (UNSAFE: stale pointers not cleared) |
| hand-rolled (NOT built) | undefined | undefined | #else Append path — KNOWN-BROKEN vs current profile offsets; do not build until the AA profile fix lands |

B+C compose: B defined => registration never called => C never reached.

## Proofs already run (Mac-local; no builds, no console)

- HOST `/usr/bin/clang++ -fsyntax-only` (BC recipe + l26 vfs overlay):
  UObjectArray.cpp control(ea7221e8)/C-off/C-on/B+C and ClassListener.cpp
  C-on -> rc=0, 0 errors, 2 known inherited warnings; `-H` proves the l26
  candidate headers + the new profile header are the ones consumed.
- PROSPERO `prospero-clang++` (real toolchain via
  LLVM_CONFIG=native-probe LLVM-21.1.8): UObjectArray.cpp C-off/C-on/B+C
  and UnrealInitializer.cpp C-on/C-off -> rc=0, 0 errors; every warning
  inherited (Atomic.hpp / UnrealType.hpp / asmjit), none from this change.
- Fixtures (/tmp/ee-variantc-fixture; byte-verbatim extract of the landed
  registration region, sha 21561665...): engine-path mock fixture 22/22
  checks (plain + ASan/UBSan) — mock engine thunks placed at the REAL
  production labels inside an mmap'd module image with GUObjectArray at
  base+0x93BA110; proves only admission-published pointers are called, with
  args (this=&GUObjectArray, listener), zero hand-rolled writes, and codes
  8/9/1 fail closed with the mock NEVER called. Hand-rolled regression
  16/16 checks (plain + ASan/UBSan): fresh-alloc, duplicate 4, insane 3,
  GMalloc-unbound 5, implausible-data 6, exception containment 2, remove
  was_present 0/1, concurrent-writer ANOMALY, file-local mutex
  serialization. Z15's gating compile-proof re-run: ALL CHECKS PASSED.
- `build_variants.py --plan` (from the repo ROOT cwd): 64 gates; the only
  false gates are (a) a PARALLEL agent's stale UObjectGlobals.cpp row
  (declared 4abf743b vs actual 83e20b38 — that file's owner must refresh
  its CHANGES row before --execute; not touched by EE) and (b) the
  expected Q `.base` expectation drift, for which the driver takes the
  patch-rebased composition path (Q's overlay rebased cleanly onto the
  revised UnrealInitializer; composed sha 23864f58..., both tokens
  present). `l26_packaging_driver.py --plan`: 9/9 gates ok.

## Next-build procedure (after the console verdict)

1. The UObjectGlobals.cpp row owner refreshes its CHANGES hash (G2 pin).
2. Archive/move the existing `outputs/2026-10-08-l26-build-variants/l26-A`,
   `l26-B`, `staging-copyin-probe` and the l26-package-A/B dirs (drivers
   refuse to overwrite non-fresh dirs), then:
   `build_variants.py --execute` (from the repo ROOT cwd) ->
   `l26_packaging_driver.py --prepare` -> `--execute`.
3. Expect variant A to differ from 5ea2c845 by exactly: the four refreshed
   tree files + new profile header + gated bridge + A's C define; variant B
   keeps the skip token only. All acceptance gates unchanged (20/20 static,
   207 imports, 238/2 constructor arrays, twins identical, A/B differ).
4. On-console expectations for the A build: `[Seeds][L26] engine
   delete-listener ops admitted: add=0x8841A0 remove=0x884240...` then
   `[L25][DeleteListener] engine-call resolved ... admission-published ==
   module base` and `engine-call Add returned in <ms>ms` (replaces the
   hand-rolled header dumps on the registration path); the bridge logs
   `copyin_probe_gate_l26_early_release 1` at admission. Any `l25_dl_refused`
   8/9 on-console is a seed/pin problem — capture the logged addresses, do
   not retry.
