# REPORT — worker-d2-textbase-design

Date: 2026-10-04. Worker of the ZCode team. PROPOSALS ONLY: nothing outside
`outputs/2026-10-04-zcode-team/worker-d2-textbase-design/` was written; no
frozen/wiring file touched; no console contact; no nested subagents. Every
claim cites `file:line` in D2-DESIGN.md (all paths are relative to the
workspace root; `W:` abbreviates it). Deliverable order honored: CLAIM.json
first, D2-DESIGN.md, this report last. Tool budget: ~40 calls, within the
40-70 bound.

## What was read (full chain)

- HAZARD rows 2.6-2.11: `worker-post-anchor-hazards/HAZARD-TABLE.md:99-104`
  (+ channel shapes :17-33, bottom line :130-140).
- Adapter: `ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp`
  — `derive_readable_ranges` :74-103 (no protection filter),
  facts.mapped assembly :222-235, text_base=0 "never supplied, unresolved by
  design" :226-227; header note `staging/lineage-overlay-1/
  lmf_to_native_loader_facts.hpp:46-50`. The design note pointed to by the
  comment is the S1 deviation D2: `worker-s1-adapter/REPORT.md:102-108`.
- Gates: `PsMappedImageCaller.hpp` :23-34 (contains; {0,0}=true/{0,1}=false),
  :44-53 (R-mandatory spans), :55-70 (exact coverage + containment throw),
  :71-73; `OwnedMappedImageCaller.hpp` provider variant :55-57 (lease-before-
  caller order, permits_code throw; read-lease variant :48-50), permits :32-43;
  `PsScanAbi.h` :85-93 (text_base doc; metadata-{0,0} rule; readable-borrow
  promise). Call site: `OwnedMappedScanSource.cpp:10,15` (text_base flows
  verbatim from facts).
- Lease/registration: `ue4ss-native-read-lease-1/src/OwnedMemory.cpp`
  :106-138 (R-required lease), :207-211/:41-44 (ScopedRegion, RWX subsets
  accepted), `include/polyhook2/OwnedMemory.hpp:25-29` ("not an API for
  registering discovered process/game mappings").
- r8 bridge: `worker-module-start-wiring/runtime_start_bridge_r8.cpp` —
  exec-only-text premise :113-119, window table :133-143, R-coverage anchor
  :311-315, path ladder :339-383, PLH pre-registration :385-421 (R-only,
  text excluded by design :392-393), register-before-conversion :476-479.
- Composed supplier (in-game version):
  `worker-composition-prep/staging/.superpowers/native-dependency-spike/
  ue4ss-native-tick-file-owner-1/src/live_module_facts.cpp:443-459` (anchor
  mappings recorded with protection, no filter; the `worker-supplier-ingame-update`
  copy is byte-identical).
- Integration mechanics: `worker-composition-4-fix/build_runtime_composition4_fixed.py:41,57-64,79`
  (exact affected-6 set; replace targets must be freshly compiled affected
  units); `worker-composition-5-rebuild/build_runtime_composition5.py:155,165-174`
  (adapter as extra unit; replace-runtime-unit already in use).
- Downstream: `NativeProgramContext.hpp:64-66,83-101` (scanner_source optional,
  unvalidated); `worker-query-s2-arm/staging/UnrealInitializer.cpp:576-633`
  (ScanGame retry/timeout-throw); consumer audit for `text_base` (zero readers
  in collectors, serialized-ELF source, and the vendored patternsleuth Rust
  crate).
- Precedents: `worker-s1-adapter/REPORT.md:139-152` probe ladder +
  `host_tests/test_lmf_adapter_host.cpp:430-437,458-478,501-519` (probes A/B/C).
- igf6 record: `worker-checkpoint-reconcile/CURRENT-STATE.md:9`.

## Key findings

1. **Gate order and mechanics** (D2-DESIGN.md §1): the ReadLease (2.6) runs
   before the PsMappedImageCaller checks; `contains({0,1})` is structurally
   false for the adapter's text_base=0 (hpp:24-25,68); permits_code (2.11)
   requires an R|X segment (hpp:57,69,39) which requires R registration
   (OwnedMemory.cpp:125-126) of bytes that must already be in
   `readable_ranges`.
2. **r8 premise correction** (§2): with the staged units, r8 stops at 2.6, not
   2.9 — the adapter still derives the exec-only text span as a readable range
   while the bridge deliberately registers only R spans. The task's "ranges
   start at 0x787c000" is the post-satellite state. Supplier-side filtering is
   impossible (anchor_mapping_count would become 0 → conversion refusal, lmf
   cpp:155-158); the satellite filter belongs in `derive_readable_ranges`.
3. **Probe precedents answered** (§3): probes A/C host-prove the two failure
   strings exactly as the hazard table predicts. A registered-pages
   configuration where text_base=0x400000 passes BOTH gates exists service-side
   (register the text pages R|X — `ScopedRegion` accepts RWX subsets,
   OwnedMemory.cpp:41-44) — and is exactly the configuration that must NOT be
   built on this target: it is a false record over prot=4 pages whose reads
   igf6 found blocked, and the first scan read would dereference them.
4. **Consumer audit**: nothing in the C++ tree or the vendored Rust crate reads
   `view().text_base`; a `text_base=0` view is inert downstream.

## Recommendation (proposals in D2-DESIGN.md §3-§4)

Option (c): honor PsScanAbi.h:92's own metadata-absence rule for text_base —
diff B (`PsMappedImageCaller.hpp:68`: `(text_base!=0 && !contains({text_base,1}))`)
+ diff C (`OwnedMappedImageCaller.hpp:57` provider and `:50` read-lease variant:
`(text_base!=0 && !permits_code(text_base))`, message unchanged) — plus the
required satellite diff A (adapter `derive_readable_ranges`: skip
`(protection & 1)==0` mappings). Ranked: (c) > (b, defer — sound only as ABI-V2
work; `native-owned-memory` is not even replaceable under the current affected
inventory) > (a, reject — hardware lie + crash path). Integration fits the
existing r8 rebuild mechanics: adapter is an `--extra-unit` (fresh every run),
headers are under the trigger-header chain so all instantiating TUs are in the
freshly-compiled affected 6-set; receipts (`--extra-receipt`,
`--replace-receipt`) and the lineage-overlay copies need hash refreshes.

## Honest limits

- Static source reading only; no target evidence produced or claimed.
- Option (c) makes Stage 2 passable; it does not make the mapped scan succeed
  for code slots (text is unreadable) — ScanGame will loop to "PS scan timed
  out" (UnrealInitializer.cpp:610-631) unless slot-policy/witness work lands
  next. That is the next design deviation, documented in D2-DESIGN.md §4.
- HAZARD row 2.7's remedy hint is stale (contradicted by r8:113-114 and the
  igf6 record); flagged for its owner, not edited (frozen file).
