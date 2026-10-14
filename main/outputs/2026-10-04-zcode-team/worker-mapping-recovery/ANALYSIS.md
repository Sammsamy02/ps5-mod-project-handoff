# ANALYSIS — offline recovery of the corrupted v2 in-game VMMAP spans

Worker: `worker-mapping-recovery` (offline, read-only inputs, no console/network).
Inputs: `outputs/2026-10-04-zcode-controller/console/ingame-v2-facts-2026-10-04.jsonl`
(v2 build `igf2-764f957dc0a6`, 48 `v1_vmmap_page` records + `v1_vmmap_full` histogram),
plus the earlier partial view in `ingame-v1-facts-2026-10-04.jsonl` (g5_vmmap, build
`igf-f4a91d8123b0`). Companion artifacts: `recovered-spans.json` (full evidence),
`vmmap-v3-records.jsonl` (drop-in for v3 cross-validation).

No span was invented: every recovered entry carries the constraint that admitted it, and
every rejected entry carries its reason.

## 1. Totals

| quantity | count |
|---|---|
| kinfo entries returned by the kernel (v2 full) | 1243 |
| spans kept by the probe (both v1 and v2 agree) | 1024 |
| entries actually listed (48 parts x 20) | 960 |
| kept spans never listed (lost to truncation) | 64 |
| **recovered — trusted** (v1+v2 exact corroboration) | **2** |
| **recovered — plausible** | **53** (4 unflagged + 49 high-family, flagged) |
| rejected (reason recorded) | 905 |

Rejected-entry reasons: 534 `{0,0,0}` empty slots (writer padding / unread kve, 55.6% of
the stream), 162 `start>=end`, 110 `start=0` (null page, unfilled start), 54 unaligned
start (the packed-garbage family `0x0N00000090`), 45 unaligned end. The stream-level bug
that produced most of this is the record-writer JSONL comma omission plus the vmmap
stride/format corruption already identified by the v3 worker's claim; per-entry structure
survived only where both kve_start and kve_end landed intact.

Independent budget check (strong validation of the recovery): the clean `v1_vmmap_full`
counters say the 1024 kept spans contain exactly 57 readable, 52 writable, 5 executable
spans. My 55 recovered spans account for r=50, w=48, x=2 — every budget respected, with
the x deficit of exactly 3 accounted for by three exec *anchors* that survived with a
lost endpoint (0x7ed7f8000 end-anchor prot-4 entry, 0x8811b0000 end-anchor prot-4 entry,
and the scan's sole RX candidate 0x800ecc000). 2+3 = 5 = spans_x exactly. The r/w
deficits (7/4) sit plausibly inside the 64 unlisted kept spans and the gap.

## 2. Address-space layout story

Five regions emerge (full hex values in `recovered-spans.json`):

1. **Main executable image — 0x0000000000400000-0x000000000787c000, prot 4 (X), ~116.7 MiB.**
   TRUSTED (identical in v1 g5 and v2 part 0). Not homogeneous: leaked prot-field
   addresses 0xc8c000, 0xd42000, 0x1120000, 0x1224000, 0x1dc000, ... fall strictly inside
   it — internal segment boundaries of the eboot that the corruption displaced into kve_protection.
2. **The 0x787c000-0x8018c000 gap — see section 3.** Nothing recovered; heavily structured.
3. **Giant read region — 0x000000008018c000-0x0000000100118000, prot 1 (R), ~2.0 GiB.**
   TRUSTED. Internal boundaries 0xee6e4000->0xee878000 and 0xff88c000->0xff8b0000 leaked as
   paired prot values and corroborated independently by BOTH the v1 and the v2 histograms.
4. **The ~34.4 GiB module/staging neighborhood (0x7ed7f0000-0x881798000).** Four clean
   spans recovered: 0x7ed8ac000-0x7ed90c000 (RW, 384 KiB), 0x800250000-0x800258000 (R,
   32 KiB), 0x800e8c000-0x800e90000 (X, exactly one 16 KiB page), 0x88131c000-0x88149c000
   (RW, 1.5 MiB); plus five one-sided anchors (0x7ed7f8000 X-end, 0x7ed9b0000 start,
   0x7eeffc000 R-end, 0x8002a8000 start, 0x800e6c000 R-end, 0x8811b0000 X-end,
   0x881798000 start). The v2 base-scan picked its ONLY RX candidate here: base
   0x800ecc000 ("rx_span", prot read as 15 — itself corrupt; span_end lost; ELF magic
   read returned 0). This is the densest small-span region in the map — the place where a
   real module base hunt (v3 tier-a magic probes at every span start) has the best odds.
5. **High monotone family — 0x10001f0000-0x306ae60000.** 49 spans, 0x10000-granular,
   46 prot 3 (RW) + 3 prot 0 (reservations 0x1f01760000-0x1f40000000 ~1 GiB,
   0x3056e40000-0x3056e60000, 0x3067aa0000-0x3067ab0000), strictly monotonic and
   non-overlapping across parts 4-47 with ZERO breaks. Flagged, not silently trusted:
   every member lies in [2^36, 3*2^36), above the stated 36-bit VA bound. Two readings:
   (a) the process VA space is actually wider than 36 bits and this is the real RW working
   set just past the 64 GiB boundary; (b) a systematic high-bit displacement (+k*2^32 /
   +k*2^36) corrupted the whole family. Reading (b) is weakened by the fact that the raw
   sequence is perfectly monotonic while subtracting k*2^32 is not; but only v3's clean
   dump can arbitrate (rules in section 5).

100 one-sided boundary anchors and 43 leaked (start,end)-shaped prot pairs were salvaged
as evidence (not spans); 38 consecutive-entry boundary pairs (`prev_end`/`next_start`,
always ascending) reconstruct the ordered low-region walk the corruption otherwise ate.

## 3. The 0x787c000-0x8018c000 gap question

The gap spans 0x787c000-0x8018c000 = 0x78910000 bytes (~1.88 GiB) between the top of the
exec image and the giant read region.

- **Recovered spans inside: ZERO.** No defensible span, trusted or plausible, lies in the gap.
- **Is it empty VA? Almost certainly NOT.** 47 salvaged boundary-anchor addresses fall
  strictly inside it (0x13880000 ... 0x80040000-0x80130000 band, full list in
  `recovered-spans.json`), and 18 leaked (a,b) prot pairs form plausible span intervals
  inside it — e.g. 0x40060000-0x400f0000, 0x404a0000-0x40590000, 0x53da0000-0x53eb0000,
  0x65fb0000-0x660e0000, 0x6a260000-0x6a4a0000, and a cluster 0x80040000-0x80130000
  ending just ~360 KiB below the trusted read span's start 0x8018c000.
- Reading: the gap is the writable/bss/anon + mid-address module band of the process,
  and it is precisely where the corruption did its worst (the E3/E4 boundary-pair
  corruption pattern and the packed-garbage starts cluster around it). "Nothing
  recovered" is a property of the log, not of the memory.

## 4. Candidate-base implications for the GUA anchor (vaddr 0x93ba110, GUObjectArray)

Anchor = B + 0x93ba110 under each candidate base; "base+vaddr" and "span_start+vaddr"
rows coincide until a parsed ELF base exists (they differ only by an unmeasured
slide), so the table below covers both hypotheses.

| candidate base B | why considered | anchor B+0x93ba110 | lands where |
|---|---|---|---|
| 0x400000 | trusted exec image span start | **0x97ba110** | **inside the 1.88 GiB gap** |
| 0x8018c000 | trusted read-region span start | 0x89546110 | inside read span 0x8018c000-0x100118000 (prot 1) |
| 0x800ecc000 | v2 scan's sole RX candidate | 0x80a286110 | unrecovered VA above 4.3 GiB (34.6 GiB neighborhood) |
| 0x800e8c000 | recovered 16 KiB exec page | 0x80a246110 | unrecovered VA (same neighborhood) |
| 0x800250000 | recovered R span start | 0x80960a110 | unrecovered VA (same neighborhood) |

Implications:

- **If the image base is 0x400000**, GUA lands at 0x97ba110 in the gap — consistent with
  GUObjectArray living in writable .data/.bss, and it *predicts the gap contains at least
  one writable span covering 0x97ba110*. That is a sharp, falsifiable check for v3: if
  v3's clean dump shows a W span covering 0x97ba110, hypothesis base=0x400000 gains hard
  support and the tier-b oracle at B+0x93ba110 should be run with B=0x400000.
- **If the base were 0x8018c000**, GUA would sit in a read-only span — GUObjectArray is
  written at runtime, which argues against the read region being the image base (or
  against its prot-1 aggregate being accurate).
- **If the base is in the 34.4 GiB neighborhood** (0x800ecc000 or nearby, the only place
  the live scan found an RX span), GUA lands at ~0x80a2xxxxx, outside every recovered
  span — exactly the blind spot v3's tier-a magic probes + tier-b GUA oracle at
  B+0x93ba110 are designed to resolve. The recovered 0x800e8c000 exec page and the
  0x800e6c000/0x800ecc000 anchors delimit the 64 KiB window where a real module header
  can sit.

## 5. Drop-in for v3 cross-validation (`vmmap-v3-records.jsonl`)

Line 1 is a `v1_vmmap_page`-shaped wrapper record containing all 55 recovered spans in
the exact per-entry format the v1/v2 records use
(`{"start":"0x%016x","end":"0x%016x","prot":N}`, start/end 16-digit hex strings, prot a
plain integer); lines 2+ carry one `rr_recovered_span` record per span with
classification, admitting constraint, caveat and source index. Rules for the cross-check
when v3's clean dump lands:

1. The 6 unflagged spans (2 trusted + 4 plausible) should match v3 verbatim; any
   mismatch is a v3 bug or real map churn, not recovery error.
2. The 49 flagged high-family spans: if v3 shows spans at 0x10001f0000+ verbatim, the
   VA space exceeds 36 bits and the family is real; if v3 is strictly 36-bit, test each
   member for a k*2^32 / k*2^36 displacement before discarding — sizes and order are
   trustworthy even under displacement.
3. v3's r/w/x span counts must read exactly 57/52/5 (clean counters); the three exec
   anchors (0x7ed7f8000, 0x8811b0000, 0x800ecc000) should materialize as the 3 missing
   exec spans.
4. The 47 in-gap anchors and 18 in-gap leaked pairs should each land on real span
   boundaries inside 0x787c000-0x8018c000; check specifically for a W span covering
   0x97ba110 (GUA prediction).
