# REPORT — worker-launch10-build-witness

2026-10-05 (UTC 12:56) · read-only witness pass over the launch-10 build inputs, plus an
independent compile-gate re-run. LOCAL ONLY; nothing outside
`outputs/2026-10-04-zcode-team/worker-launch10-build-witness/` was written. No staged
file was modified.

## Bottom line

**All launch-10 input verifications PASS. Zero blockers. The controller's one-pass
composition has NOT run yet** — every file under `worker-comp6-driver/` predates
(2026-10-04 23:55 local max) the staged launch-10 inputs (2026-10-05 07:26+ local), and
the existing `prx-candidate-composition6-1` PRX is the launch-9 chain build (its
`build/diagnostic.o` lacks the sink symbols; the FINAL gate receipt there is the
composition-5-era pre-staging gate, not the launch-10 15/15 gate). Per instruction, I
state this and stop after the input verification; post-build checks are pre-registered
in WITNESS.md.

## What was independently verified

1. **Initializer** (`worker-launch10-readpath-guard/staging/UnrealInitializer.cpp`,
   `1fc82e6b…` — sha verified). Composes all three required pieces: the gmalloc V1/V2
   validator (`check_gmalloc_instance`/`check_gmalloc_vptr`, Phase 2b before the bind),
   the five SeedAddressClass read-path guards (slots 0/1/3 DataReadable, slots 2/4
   CodeCallOnly; delta vs the gmalloc staging base is exactly those 5 guards, 33 added
   / 0 removed lines), and the armed SeedTable (`SeedTable.h` byte-identical to
   `SeedTable.launch10.h`, both `d183e0c0…`, constants 0x93089b8/0x7ec26e0 present;
   `SeedValidation.hpp` delta vs the gmalloc copy is purely the additive classifier
   matching the CLAIM's phdr geometry).
2. **Compile gate re-run (determinism proof)** — I re-ran the exact 190-arg unit argv
   (loaded from `worker-launch10-gmalloc-integration/receipts/compile-gate.argv.json`,
   sha `1e2f7822…` verified) with `LLVM_CONFIG` pinned to the specified llvm-config,
   swapping only the `-c` input, `-o` and `-MF` into my own receipts directory
   (`receipts/run_witness_compile_gate.py` asserts exactly those 3 swaps). Result:
   **exit 0**, 6 warnings whose normalized stderr is **identical** to the gmalloc
   baseline run (zero new classes), object **1933568 B** — and **byte-identical**
   (`cmp` clean) to the controller's
   `worker-launch10-readpath-guard/receipts/unreal-unrealinitializer-readpath-guard.o`
   (`8b9e66b8839b5c5b463f555f0407b08260a440e52dbdb3f9fd485274df8416ec`). Guard refusal
   strings: 0 occurrences in the gmalloc baseline object; present in my object
   ("refusing reads" x3, "refusing bind" x2, "seed class" x5).
3. **LMF core** (`live_module_facts.geometryfix.cpp`, `2527b309…` verified): diff vs
   the pinned canonical (`5eff2ad9…` verified) is **exactly 1 hunk**
   (`@@ -444,10 +444,42 @@`) — precisely the claimed anchor_mappings intersect-extent
   block: VmScan route only, overflow-checked extent end, `kMaxAnchorMappings` cap
   kept, every other route byte-identical containing rule. Nothing beyond the claimed
   block.
4. **Bridge** (`runtime_start_bridge_r13.geometrydiag.cpp`, `c3d6c4de…` verified):
   diff vs canonical TEAM_WIRING r13 is **3 hunks** — a header comment, 6 new
   `diagnostic_event` calls on the refusal path, and the same 6 on the success path,
   all through the existing diagnostic channel and strictly after the conversion
   decision. Conversion behavior unchanged. One non-blocking observation: the
   success-path block uses `images.at(MainExe)`, guaranteed-present after a successful
   conversion (the adapter plants it; see WITNESS.md).
5. **Diagnostic sink** (`diagnostic.c`, `f5e96c41…` verified vs lineage baseline
   `931d7969…`): diff is **additive-only** — the single "removed" line is the entry
   `write_line(buffer, used);` re-emitted with a trailing comment, plus the added
   `supplier_write_event(tag, value);` after it; entry-channel bytes unchanged. Symbol
   delta proven by object inspection with llvm-nm: rehearsal object
   (`d14d0ac2…` verified) adds exactly **{__error, clock_gettime, getpid}** over the
   shipped `diagnostic.o` {sceKernelOpen, sceKernelWrite, sceKernelClose}; all three
   are already undefined in the launch-9 `composition6-2/build/combined-supplier.o`,
   so the packaging unresolved-set expectation is unchanged. Supplier marker
   `modbridge.supplier-diag/v1` and `/data/modbridge-supplier-diag.jsonl` present in
   the rehearsal object.
6. **Sha cross-check**: 12/12 CLAIM-referenced shas match the actual files (initializer,
   SeedValidation.hpp, readpath object + size, SeedTable pair, geometryfix core,
   geometrydiag bridge, canonical LMF, fixture, sink, lineage sink, rehearsal object).
   **0 mismatches.** One benign provenance note: the gmalloc worker's
   `hash-manifest.json` self-entry for its CLAIM.json is stale (manifest predates a
   later CLAIM edit); every artifact sha in that manifest verifies and no task-listed
   CLAIM sha is affected.

## Deliverables

- `CLAIM.json` — structured witness claims, verdicts, shas, one-pass status
- `WITNESS.md` — per-item PASS/FAIL/BLOCKER table with shas + pre-registered post-build checks
- `REPORT.md` — this file
- `receipts/` — compile-gate re-run (runner script, stdout/stderr, witness object
  `8b9e66b8…` byte-identical, .d) and all five bounded diffs
  (initializer, SeedValidation, LMF, bridge, diagnostic)

## Post-build status (item 6)

NOT RUN YET. When the controller's one-pass completes under
`worker-comp6-driver/prx-candidate-composition6-1` (+ receipts + gate-receipt), the
pre-registered checks in WITNESS.md apply: seed-guard strings, `lmf_intervals_found` +
sibling events, supplier-diag marker in the composed object; `build/diagnostic.o`
symbol set; import count vs the frozen contract; gate receipt GO 15/15. Nothing in
this pass certifies any in-game/console behavior — inputs and host-side compile
evidence only.
