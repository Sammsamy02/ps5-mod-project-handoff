# REPORT — worker-span-arbitration

Worker: `worker-span-arbitration` (offline, read-only inputs, no console/network, no nested
subagents, ≤30 tool calls; 27 used). Deliverable: `VERDICT.md` (with `CLAIM.json`).

## Task

Arbitrate worker-mapping-recovery's flagged >36-bit "high family" (49 spans,
0x10001f0000–0x306ae60000): real vs systematic displacement, via (1) the exact SDK
kinfo_vmentry layout tested against the corrupted-entry statistics, (2) in-tree
FreeBSD/PS5 VM layout knowledge, (3) falsifiable predictions for v3's clean dump.

## Verdict

**REAL.** The 49 high-family spans are genuine kernel-reported mappings. High confidence.

## Key findings (all cited in VERDICT.md §7)

1. **SDK layout** (`sys/user.h:466–488`, releng/11.4): kve_structsize@0, kve_type@4,
   kve_start@8, kve_end@16, kve_offset@24, …, kve_protection@56, …, _kve_ispare[12]@88,
   kve_path[1024]@136 → 1160 total; path "truncated before copyout".
2. **Corruption mechanism identified exactly**: v2 walked the sysctl buffer with the fixed
   header stride `sizeof(struct kinfo_vmentry)`=1160 (experiments.c:134; the logged
   kve_size=1160 is the header constant, not a kernel-read value), but the PS5 kernel packs
   records at **kve_structsize=144** (path stripped). Proof: both run buffer lengths are
   exact multiples of 144 (v2 1442448=10017×144; v1 1452672=10088×144), and 144×145=20880=
   18×1160 predicts an exact 18-slot artifact period — observed: 53 `{0x0N00000090}` starts
   (= structsize 144 ‖ kve_type) at indices 17,35,…,953, step exactly 18, zero drift.
   This reproduces every observed artifact class AND which entries stayed sane: φ=0 slots
   are verbatim reads → exactly the 2 trusted + 49 family sane entries; zeros = path/ispare
   region; odd values = boundary aliases (end‖prot, prot‖refcount).
3. **Displacement refuted**: no adder exists anywhere in the sysctl→cast→copy→emit path;
   the prot/end-independence discriminant kills all alias-style manufacturing (46/46 prot-3
   spans have prot=3 while (end>>32)&0xff ∈ {16..34,48} — a 4-phase alias would force
   equality); −2^36 shifts collide with the trusted exec-image span with wrong prots.
4. **The 36-bit VA bound was an unsourced worker assumption** (recovery CLAIM.json; v3
   vmmap_walk.c comment). No in-tree source documents PS5 user VA width or GPU/linear
   regions (GNM driver receipts show imports only). The family, if confirmed, establishes
   user VA ≥ ~0x306ae60000 (>36 bits) — plausible for a FreeBSD-11-amd64-derived kernel.
5. **Recovery-worker corrections** (amendments; raw reports untouched): 1243 was reader
   slots, not kernel entries (10017×144 real records); "1024 kept" was the IG_SPAN_MAX cap
   (219 slots never read, no filter); the r/w/x 57/52/5 "budget validation" is void (counted
   corrupt slots); the four "exec X-end anchors" are next-record STARTS at φ=136; trusted
   low spans confirmed; the 49 spans upgrade flagged-plausible → real pending v3.
6. **Falsifiable v3 predictions**: P1 every record kve_structsize=144, lengths exact
   multiples; P2 family verbatim; P3 all artifacts vanish; P4 v3's fixed-stride replica
   reproduces the 18-period artifacts (cheap causal proof); P5 real r/w/x ≠ 57/52/5;
   P6 gap contains a W span covering 0x97ba110 (GUA, unchanged by this verdict — no family
   span can host L7 BSS; the only large one is PROT_NONE). Under displacement: D1 no spans
   ≥2^36 and family reappears at constant −k; D2 record sizes ≠ 144.

## Constraints honored

Read-only inputs; writes only in `outputs/2026-10-04-zcode-team/worker-span-arbitration/`;
no network, no console, no nested agents; ≤30 calls (27). Raw provenance preserved; the
known missing-comma writer bug was repaired mechanically and deterministically for parsing
only; corrections are recorded as amendments in VERDICT.md §5, original worker reports
untouched.

## Recommended next steps for the controller

- Fold VERDICT.md §5 amendments + §6 predictions into the v3 dispatch brief (they
  strengthen the existing writer/stride-fix amendments: expect structsize=144, not merely
  "variable"; mode-2 replica replay is a cheap decisive add-on).
- v3 acceptance for the family: P1+P2 (+P3) closes the flag; absence → D-branch
  re-arbitration using v3's per-record header snapshots.
- Treat the recovery worker's cross-check rule "6 unflagged spans match verbatim" as
  necessary but insufficient: per-slot phase alignment should be re-derived from v3's
  structsize walk (VERDICT.md §5.5–5.6).
