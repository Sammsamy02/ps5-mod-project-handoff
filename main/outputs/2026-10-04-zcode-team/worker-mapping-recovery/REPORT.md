# REPORT — worker-mapping-recovery (2026-10-04)

Offline recovery of the corrupted v2 in-game VMMAP span records. No network, no console,
read-only inputs, no nested subagents. CLAIM.json written first; this REPORT.md last.
All writes confined to `outputs/2026-10-04-zcode-team/worker-mapping-recovery/`.
Tool-call budget: 17 of 30 used.

## Deliverables (this directory)

- `recovered-spans.json` — every entry classified {trusted, plausible, rejected} with the
  constraint that admitted it or the reason it was rejected; plus 100 one-sided boundary
  anchors, 43 leaked prot pairs, 38 low-region boundary pairs, gap analysis, GUA table.
- `vmmap-v3-records.jsonl` — all 55 recovered spans in the exact v1/v2/v3 per-entry format
  (`{"start":"0x%016x","end":"0x%016x","prot":N}`) as a `v1_vmmap_page`-shaped wrapper
  line + one provenance record per span; drops into v3 cross-validation.
- `ANALYSIS.md` — full map picture, gap answer, GUA candidate-base implications.
- `recover_spans.py` — deterministic recovery script (inputs never modified).

## Headline numbers

- 960 listed entries -> 2 trusted + 53 plausible (49 flagged high-family) + 905 rejected.
- Trusted: 0x400000-0x787c000 (prot 4) and 0x8018c000-0x100118000 (prot 1) — identical in
  the v1 g5 partial view and v2 part 0.
- Independent validation: recovery's r/w/x = 50/48/2 fits the clean v1_vmmap_full
  counters 57/52/5 exactly, with the 3 missing exec spans matched 1:1 to surviving exec
  anchors (0x7ed7f8000, 0x8811b0000, 0x800ecc000).
- The 49-span high family (0x10001f0000-0x306ae60000) is strictly monotonic and
  non-overlapping with zero breaks across parts 4-47, but lies above the stated 36-bit
  VA bound — flagged (real >36-bit region vs systematic displacement), v3 arbitrates.
- Gap answer: NO recovered span lies inside 0x787c000-0x8018c000, but 47 boundary anchors
  and 18 leaked (start,end) pairs sit inside it — the gap is dense, corrupted, not empty.
- GUA (vaddr 0x93ba110): base 0x400000 -> anchor 0x97ba110 lands in that gap (falsifiable
  v3 check: a W span should cover it); base 0x8018c000 -> lands in a read-only span
  (argues against); base 0x800ecc000 (live scan's sole RX candidate) -> 0x80a286110, the
  exact blind spot v3's tier-a/tier-b probes target.

## Caveats

- The v3 worker's directory was being actively rewritten mid-task; the drop-in format was
  fixed against the stable v1/v2 entry shape (which v3's claim commits to keep), not
  against v3's still-moving sources.
- The 36-bit-vs-high-family conflict is reported, not resolved; both readings and the
  arbitration rules are in ANALYSIS.md section 5.
- 64 kept spans were never listed by v2 (48x20 truncation); nothing can be recovered
  from them beyond the r/w/x budget arithmetic.
