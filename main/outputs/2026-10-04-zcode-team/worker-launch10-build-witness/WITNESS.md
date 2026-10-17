# WITNESS — launch-10 build inputs (independent second pair of eyes)

Worker: worker-launch10-build-witness · 2026-10-05 (UTC 12:56) · LOCAL ONLY, read-only
outside this directory. No staged file was modified. Workspace:
`/WORKSPACE/.codex/worktrees/translation-core/what`.

## Per-item verdict table

| # | Item | Claimed sha / fact | Actual (witness-computed) | Verdict |
|---|------|--------------------|---------------------------|---------|
| 1a | Initializer `worker-launch10-readpath-guard/staging/UnrealInitializer.cpp` | `1fc82e6b…` | `1fc82e6bff3fd1febfd0b62cdce241ad5c1749b12511902418f10cc3957fa2fc` | **PASS** |
| 1a | Composes gmalloc V1/V2 validator | claimed present | `check_gmalloc_instance` (line 804), `check_gmalloc_vptr` (line 811), Phase 2b comment (769); validators in SeedValidation.hpp:225/237 | **PASS** |
| 1a | Composes SeedAddressClass read-path guards | 5 guards | `classify_seed_address` guards at 757 (slot 0), 777 (slot 3), 828 (slot 1), 879 (slot 2 CodeCallOnly), 905 (slot 4 CodeCallOnly); delta vs gmalloc staging base = exactly those 5 guards, 33 added / 0 removed lines (receipts/initializer-gmalloc-vs-readpath-guard.diff) | **PASS** |
| 1a | Armed SeedTable; `SeedTable.h` byte-identical to `SeedTable.launch10.h` | byte-identical | both `d183e0c03e4ec7568e2e1416787599f8c9f363f5a940cfc47a2a31ff04ca3cda`; armed constants 0x93089b8 / 0x7ec26e0 present (SeedTable.h:21-22); identical to the gmalloc staging copy pinned in its hash-manifest | **PASS (re-verified)** |
| 1b | Compile gate re-run by witness, same 190-arg argv + LLVM_CONFIG | exit 0, object 1933568 B `8b9e66b8…` | exit **0**, 190 args (3 swaps: input/-o/-MF into this dir only), object **1933568 B**, sha `8b9e66b8839b5c5b463f555f0407b08260a440e52dbdb3f9fd485274df8416ec` | **PASS** |
| 1b | Determinism: witness object byte-identical to controller object | implied by claim | `cmp` clean — **BYTE-IDENTICAL** | **PASS** |
| 1b | Warnings | 6, all pre-existing classes, zero new | 6; normalized stderr identical to gmalloc baseline (same classes/headers); 4 classes: asmjit deprecation, deprecated-enum-enum-conversion x2, -Wswitch x2, undefined-inline | **PASS** |
| 1b | Guard strings in object | 7/7 present once, 0 in baseline | baseline object: "refusing reads" 0, "refusing bind" 0, "seed class" 0; witness object: 3 / 2 / 5 (5 guard messages = 3 data + 2 code) | **PASS** |
| 2 | LMF core `worker-facts-geometry-fix/staging/live_module_facts.geometryfix.cpp` | `2527b309…`; only delta = anchor_mappings intersect-extent block | sha matches; diff vs pinned canonical (`5eff2ad9…` verified) = **exactly 1 hunk** `@@ -444,10 +444,42 @@`: VmScan-route intersect `[base, checked_add(base,extent))`, cap `kMaxAnchorMappings` kept, non-VmScan routes keep the containing rule (receipts/lmf-canonical-vs-geometryfix.diff) | **PASS** |
| 3 | Bridge `worker-facts-geometry-fix/staging/runtime_start_bridge_r13.geometrydiag.cpp` | `c3d6c4de…`; conversion behavior byte-identical + diag events | sha matches; diff vs canonical TEAM_WIRING r13 = **3 hunks**: (1) header comment, (2) refusal path 6 `::diagnostic_event` (lmf_extent_base_low/high, lmf_extent_end_low/high, lmf_anchor_count, lmf_intervals_found=0), (3) success path same 6 with real values; all through the existing `diagnostic_event` channel; inserted strictly after the conversion decision on both paths — no conversion-behavior change | **PASS** (1 observation below) |
| 4 | Diagnostic sink `worker-diagnostic-sink/staging/diagnostic.c` | `f5e96c41…`; entry channel bytes unchanged, additive-only | sha matches; lineage baseline `931d7969…` verified; diff = 3 hunks, **only additive** (the single "removed" line is `write_line(buffer, used);` re-emitted with a trailing comment, plus the added `supplier_write_event(tag, value);` call after it; +2 includes) — entry-channel buffer construction and write call unchanged | **PASS** |
| 4 | Symbol delta `{__error, clock_gettime, getpid}` | claimed exact | rehearsal object (sha `d14d0ac2…` verified) undefined: `__error, clock_gettime, getpid, sceKernelOpen, sceKernelWrite, sceKernelClose`; shipped composition6-1/-2 `build/diagnostic.o` undefined: `sceKernelOpen, sceKernelWrite, sceKernelClose` only → delta **exactly** the claimed 3; all 3 already undefined in launch-9 `composition6-2/build/combined-supplier.o` (verified) → packaging unresolved-set expectation unchanged | **PASS** |
| 4 | Supplier marker/path strings | claimed | rehearsal object carries `{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":` and `/data/modbridge-supplier-diag.jsonl` | **PASS** |
| 5 | Every CLAIM.json sha vs actual files | all | see table + CLAIM.json `shas_verified` — 12/12 match; fixture `4d80e70d…` verified too | **PASS (0 mismatches)** |
| 6 | One-pass AFTER-check (`worker-comp6-driver/prx-candidate-composition6-1` + receipts + gate-receipt) | — | **BUILD HAS NOT RUN.** Newest comp6-driver file = `gate-receipt-composition6-FINAL.json` 2026-10-04 23:55 local; every staged launch-10 input is 2026-10-05 07:26+ local. Current `composition6-1/build/diagnostic.o` has no sink symbols and the existing FINAL gate receipt is the composition-5-era pre-staging gate (verdict "GO (offline gate)", expected_imports 196) over the OLD launch-9 PRX. Per instruction: stating this and stopping after input verification. | **NOT RUN YET** |

## Blockers

None.

## Observations (non-blocking)

1. **Bridge success-path `.at(MainExe)`** — the new success-path diag block reads
   `converted.facts.modules.images.at(RC::ScanTarget::MainExe)`. A `std::out_of_range`
   is only possible if conversion succeeded with no MainExe image record; the adapter
   plants `images[MainExe].base` during conversion (lmf_to_native_loader_facts.cpp:130)
   and the launch-9 refusal class shows extent facts exist exactly when conversion
   succeeds. Guaranteed-present in practice.
2. **gmalloc hash-manifest self-entry staleness** —
   `worker-launch10-gmalloc-integration/receipts/hash-manifest.json` lists CLAIM.json as
   `8b7bbdfc…` but the current file hashes `b175a706…` (manifest 12:02:37Z predates a
   later CLAIM edit). Every artifact sha in that manifest verifies. Not one of the
   task-listed CLAIMs; provenance note only.
3. **"7/7 guard refusal strings"** — reconciled as 3x "refusing reads" (slots 0/1/3) +
   2x "refusing bind" (slots 2/4) plus shared fragments; 0 occurrences in the gmalloc
   baseline object, all present in the witness object.

## Pre-registered post-build checks (when the controller's one-pass runs)

1. Composed object carries the 5 seed-guard refusal strings (baseline has none).
2. Composed object carries `lmf_intervals_found` + the 5 sibling lmf geometry events
   (already proven present in the staged bridge object receipt).
3. Composed object carries the supplier-diag marker `modbridge.supplier-diag/v1` and
   path `/data/modbridge-supplier-diag.jsonl` (already proven in the rehearsal object).
4. `build/diagnostic.o` undefined symbols = {sceKernelOpen, sceKernelWrite,
   sceKernelClose, __error, clock_gettime, getpid} exactly.
5. PRX import count vs the frozen contract (the 3 added symbols are already undefined
   in combined-supplier.o — verified on the launch-9 build).
6. Gate receipt GO 15/15.

## Witness receipts index (this directory only)

- `receipts/run_witness_compile_gate.py` — bounded runner (asserts exactly 3 path swaps, argv length 190)
- `receipts/compile-gate.stdout` / `receipts/compile-gate.stderr` — witness gate run (exit 0, 6 warnings)
- `receipts/unreal-unrealinitializer-witness.o` (1933568 B, `8b9e66b8…`, byte-identical) / `.d`
- `receipts/initializer-gmalloc-vs-readpath-guard.diff` — 5 guards, 33+/0-
- `receipts/seedvalidation-gmalloc-vs-readpath-guard.diff` — classifier, additive only
- `receipts/lmf-canonical-vs-geometryfix.diff` — 1 hunk
- `receipts/bridge-r13-vs-geometrydiag.diff` — 3 hunks
- `receipts/diagnostic-lineage-vs-sink.diff` — 3 hunks, additive-only
