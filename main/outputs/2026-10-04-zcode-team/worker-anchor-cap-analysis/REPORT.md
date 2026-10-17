# REPORT — worker-anchor-cap-analysis (honest limits)

2026-10-05 · local-only analysis · no console contact, no network, no canonical
file edits. Deliverables: `CLAIM.json` (provenance), `CAP-ANALYSIS.md`
(fragmentation table, margin math, recommendation), this report.

## What is pinned vs not

**Pinned (multi-source, in-tree):**

- The fix's collection loop and cap:
  `outputs/2026-10-04-zcode-team/worker-facts-geometry-fix/staging/live_module_facts.geometryfix.cpp:467-490`,
  `kMaxAnchorMappings=8` at
  `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/live_module_facts.hpp:88`.
- The extent the fix will use on the launch-10 route:
  `[0x400000, 0xc0602c8)` (`kCandidateBase=0x400000` +
  `kImageExtentVaddr=0xbc602c8`, r13 overlay lines 157/165, planted via
  VmScanResult lines 620-621). **The task brief's `0xfc602c8` end is the stale
  r11-era clamp preserved in an r13 comment, not the constant the fix
  consumes.** Both extents intersect the same 7 records on pinned evidence.
- Launch-9 observed **6 readable** spans intersecting the extent
  (`aligned-entry-log-snapshot.log:17035-17040`, `bridge_lease_reg_spans=6`,
  clean walk stop, 9,966 records) — a COUNT only; the individual spans were
  never logged.
- The full 7-record fragmentation table from the igf3 clean
  kve_structsize walk (16384/16384 sane records, `fallback_used=0`),
  cross-consistent with launch-9's readable count, the v3
  MAPPING_BREAKTHROUGH ledger pins, and phdr page arithmetic.

**NOT pinned by in-tree evidence:**

1. **The live launch-10 fragmentation.** The 7-record table is from the igf3
   boot (2026-10-04 19:15); launch-9 (2026-10-05) contributes only the
   readable count 6. Boots differ process-wide (9,966-22,964 records observed
   across boots); the IMAGE-region shape happened to agree on every pinned
   observable, but agreement across two boots is not proof for the third.
2. **Whether LOAD8 `---` can materialize as vmmap records** (the +1/+2
   fragmentation source that would saturate the cap). It did not on igf3; its
   behavior across boots/firmware is unobserved.
3. **The identity of launch-9's 6 readable spans.** The table assumes they are
   the igf3 six; a boot with e.g. an extra low allocation and one image span
   merged away would produce the same count with different geometry. The cap
   math is insensitive to which readable spans they are, but the GUA-fragment
   drop scenario assumes the igf3 correspondence.

## What launch-10's diagnostics will resolve

The staged geometrydiag bridge emits, on BOTH conversion paths:
`lmf_extent_base_low/high`, `lmf_extent_end_low/high`,
`lmf_anchor_count`, `lmf_intervals_found` (plus the existing
`bridge_lease_reg_spans`/`bridge_lease_reg`). One launch-10 run decides:

- `lmf_anchor_count` = the TRUE pre-filter intersecting count on the live
  boot:
  - **7** → igf3 shape reproduced; cap 8 held with margin 1.
  - **8** → one extra fragment (most likely LOAD8 materializing); cap held at
    exactly zero margin — treat as the LAST warning before loss.
  - **9+ with cap=8** → the log cannot show it (count pins at 8); the
    signature of silent truncation is `lmf_anchor_count=8` AND
    `lmf_intervals_found` missing the top-of-image coverage AND/OR a
    downstream GUA/required-slot refusal while `bridge_lease_reg_spans > 6`.
    With cap=16 the count reads honestly instead.
- `lmf_intervals_found` vs `bridge_lease_reg_spans`: expected **1 vs 6** on an
  igf3-shaped boot (the six readable records are pairwise adjacent and the
  adapter coalesces adjacency — CAP-ANALYSIS.md §5). The staged fixture's
  page-gap assumption (intervals == 6) does not match the igf3 records;
  `lmf_intervals_found=1` next to `bridge_lease_reg_spans=6` is the predicted
  divergence, not a fix failure.

## Safest fallback cap

- **Primary recommendation: fold `kMaxAnchorMappings` 8 → 16** before the
  launch-10 build (one line, header line 88; ~192 B snapshot cost; no loop or
  ABI changes; saturation stays visible at `lmf_anchor_count=16`).
- If the controller ships cap=8 unchanged: the launch-10 conversion-refusal
  risk stays LOW (any plausible shape keeps ≥1 readable span inside the first
  8, so `InvalidReadableMappings` should not return); the realistic failure is
  **silent loss of the highest (GUA-hosting) span** → deterministic
  readpath-guard/required-slot degradation of exactly the verdict launch-10 is
  armed to test. If shipping 8, pre-commit to the interpretation of
  `lmf_anchor_count=8` as saturation and treat any GUA-side refusal in that
  run as cap-suspect before re-arbitrating geometry.
- Do not raise the cap above 16 without new evidence: beyond the plausible
  fragmentation ladder (≤14), a saturated `lmf_anchor_count=16` is itself the
  honest signal that the shape changed qualitatively.

## Residual risks (beyond the cap)

- The core's 1024-record snapshot truncation is upstream of the anchor cap and
  is a non-issue on pinned boots (image records occupy snapshot indices 0-6),
  but it is the same silent-truncation class; `vm_mappings_truncated=1` is
  already visible if it ever matters.
- The load-bearing reads (GUA 0x97ba110, GMalloc candidates 0x930add8/0x970add8)
  sit in mid-image fragments (records 6-7); only the TOP fragment is
  first-dropped by the ascending-order cap. A future shape that pushes more
  fragments below 0x953c000 would demote record 6/7 together — covered by cap
  16, not by 8.
- All conclusions are prototype-verdict-relevant only; nothing here claims
  acceptance quality or completion.
