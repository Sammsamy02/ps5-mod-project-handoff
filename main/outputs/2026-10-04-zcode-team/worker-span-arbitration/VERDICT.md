# VERDICT — arbitration of the >36-bit high-family spans (0x10001f0000–0x306ae60000)

Worker: `worker-span-arbitration` (offline, read-only, no console/network, no nested agents).
Arbitrating: `worker-mapping-recovery`'s 49 flagged "high-family" spans
(`outputs/2026-10-04-zcode-team/worker-mapping-recovery/{ANALYSIS.md,vmmap-v3-records.jsonl}`),
recovered from `outputs/2026-10-04-zcode-controller/console/ingame-v2-facts-2026-10-04.jsonl`
(v2 build `igf2-764f957dc0a6`) and cross-checked against
`ingame-v1-facts-2026-10-04.jsonl` (v1 build `igf-f4a91d8123b0`).

## VERDICT: REAL — the 49 spans are genuine kernel-reported mappings, not displaced values

Confidence: high. The corruption mechanism is identified exactly, reproduces every observed
artifact class including the exact periodicity and exactly which entries stayed sane, and
under that mechanism the family slots are provably field-aligned reads. Displacement has no
viable mechanism and is refuted by the data. The "36-bit VA bound" that made the family look
impossible was an assumption, not a fact (§4).

---

## 1. The exact kinfo_vmentry layout the SDK declares

Source: `.superpowers/native-sdk-build-check/ps5-payload-sdk/target/include/sys/user.h`
($FreeBSD: releng/11.4/sys/sys/user.h 341778; duplicate copy at
`outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sys/user.h`).
`struct kinfo_vmentry` (user.h:466–488), amd64 natural alignment, KINFO_VMENTRY_SIZE=1160
(user.h:463):

| offset | field | type |
|---|---|---|
| 0 | kve_structsize | int |
| 4 | kve_type | int |
| 8 | kve_start | uint64 |
| 16 | kve_end | uint64 |
| 24 | kve_offset | uint64 |
| 32 | kve_vn_fileid | uint64 |
| 40 | kve_vn_fsid | uint32 |
| 44 | kve_flags | int |
| 48 | kve_resident | int |
| 52 | kve_private_resident | int |
| 56 | kve_protection | int |
| 60 | kve_ref_count | int |
| 64 | kve_shadow_count | int |
| 68 | kve_vn_type | int |
| (72, 4-byte pad) | kve_vn_size | uint64 |
| 80 | kve_vn_rdev | uint32 |
| 84 | kve_vn_mode | uint16 |
| 86 | kve_status | uint16 |
| 88..135 | _kve_ispare[12] | int |
| 136..1159 | kve_path[PATH_MAX] | char — "Truncated before copyout in sysctl" (user.h:486) |

KVME_PROT_READ/WRITE/EXEC = 1/2/4 (user.h:425–427). KVME_TYPE_NONE..MGTDEVICE = 0..8,
UNKNOWN=255 (user.h:414–423).

**How v2 used it**: `worker-ingame-probe-v2/src/experiments.c:107–150` — sysctl
`{CTL_KERN,KERN_PROC,KERN_PROC_VMMAP,getpid()}` with ENOMEM-doubling buffer to 4 MiB
(attempts=6), then `const size_t ks = sizeof(struct kinfo_vmentry)` (experiments.c:134) and a
**fixed-stride walk at ks=1160**, copying `kve_start/kve_end/kve_protection` into
`IgSpan{u64,u64,u32}` (factstore.h:17–20, IG_SPAN_MAX=1024). Critically,
`g_facts.vmmap_kve_size = sizeof(struct kinfo_vmentry)` (experiments.c:127) is the **header
constant, not the kernel's per-record kve_structsize** — so the logged `kve_size:1160` says
nothing about the kernel's actual record size. v2 also ran **no filter**: every reader slot
was copied verbatim; "spans_kept:1024" is the IG_SPAN_MAX cap (span_truncated=1).

## 2. The corruption mechanism — fixed stride 1160 over kernel records of 144 bytes

### 2.1 Arithmetic (exact)

- v2 `v1_vmmap_full` fields: `final_buf:1442448`, `entries:1243` (= floor(1442448/1160)),
  `spans_kept:1024`, `attempts:6`.
- **1442448 = 10017 × 144 exactly** (1442448 mod 144 = 0). Through the 1160-stride lens the
  same buffer is 1243 slots (1243×1160 = 1441880) plus a 568-byte tail — and 568 is not a
  record multiple under 1160, but is exactly 3×144 + 136 under 144: record-structure bytes
  all the way down. The kernel wrote a whole number of 144-byte records; only the 144 model
  divides the buffer length exactly, for both runs.
- v1 `g5_vmmap`: `final_buf:1452672` = **10088 × 144 exactly**. Two independent runs, two
  different lengths, both exact multiples of 144.
- Reader phase advance per slot: 1160 mod 144 = 8 bytes. Phase cycle length:
  8P ≡ 0 (mod 144) ⇒ **P = 18 slots**; equivalently 144×145 = 20880 = 18×1160.
- Observed: a `{start = 0x0N00000090}` artifact at global listed indices 17, 35, 53, 71, …,
  953 — **step exactly 18, 53 artifacts** ((953−17)/18 = 52). Zero drift over 936 slots.

### 2.2 The artifact classes and what each phase reads

At reader phase φ = (slot offset mod 144), with record-relative offsets start@8, end@16,
prot@56:

| phase class | reads | observed in log | count (960 listed) |
|---|---|---|---|
| φ=0 (aligned) | real (start, end, prot) | 2 trusted low spans + 49 family spans + no-path anon records reading real values | 55 sane total |
| φ=136 (= −8: slot start@8 lands on next record's bytes 0..7) | **kve_structsize ‖ kve_type** = 0x0N0000_0090 (structsize=144=0x90, type=N) — and "end" = that record's real kve_start, "prot" = a following small field | indices 17 (type 1), 35 (5), 53 (9), 71 (4), then 4s | 53, period exactly 18 |
| φ over bytes 88..143 (ispare + stripped path region = zeros) | {0,0,0} | the zero flood | 326 |
| 4-mod-8 phases at boundaries of mixed-size stretches | end‖next-low pairs, odd values (0x100000001 = end 0x100000000 ‖ prot 1; 0xa00000001 = prot 1 ‖ refcount 0xa) | e.g. listed idx 4,5,22,23 | 103 odd-valued |

The four "one-sided exec anchors" the recovery worker flagged (0x7ed7f8000 prot 4,
0x7eeffc000 prot 1, 0x800e6c000 prot 1, 0x8811b0000 prot 4 — its ANALYSIS.md §2 item 4) are
the **φ=136 class: each is the real kve_start of the NEXT kernel record**, not a lost exec
span end. Their companion garbage starts 0x100000090/0x500000090/0x900000090/0x100000090 are
structsize=144 plus KVME_TYPE 1/5/9/4 (DEFAULT/PHYS/unknown/DEVICE).

### 2.3 Does the stride bug reproduce the EXACT observed pattern, including which entries stayed sane?

**Yes.** A constant record size of 144 forces: (a) exact 18-slot artifact periodicity —
observed exactly, 53 hits, no drift, across all 48 parts; (b) a zero flood from the
path/ispare region — observed 326/960 (worker counted 534/960 with a looser
zero-inclusive rule; both majority-class); (c) structsize-type starts exactly at φ=136 —
observed 0x0N00000090 family; (d) alias odd values at boundary phases — observed 103;
(e) sane slots only at φ=0 — observed exactly 55: the 2 trusted spans plus the 49 family
spans. The v1 g5 record (different binary, different boot, different buffer length) shows
the **identical artifact sequence at the same indices** (ingame-v1-facts line, g5 fields:
0x400000–0x787c000/4, 0x8018c000–0x100118000/1, then {0,0x100000000}, {0,0x100000001},
{0x100000001,0xa00000001}…), which is what a deterministic stride-vs-record-structure
misread predicts and what random corruption does not. Note worker-ingame-probe-v3
(`src/vmmap_walk.c:22–39`) had already inferred "real kernel records carry their own
kve_structsize (variable; the path is truncated before copyout), so the fixed stride
periodically re-aligns with real records" and "record boundaries are 4-aligned, not
8-aligned"; the buffer arithmetic here pins the dominant record size to exactly 144 with
exact period 18.

**Consequence for the two hypotheses**: under this mechanism, reader slots that look sane
are φ=0 reads of the verbatim buffer. The walk copies bytes; it adds nothing. There is no
stage at which a constant k·2^32 or k·2^36 could be ORed/added into aligned reads. So the
displacement hypothesis has **no mechanism**, and the stride bug itself, far from
manufacturing the family, is what PROVES the family is verbatim buffer content.

## 3. Testing both hypotheses against the corrupted-entry statistics

The family (worker rule: sane, start ≥ 2^36, prot ∈ {0,3}, 0x10000-granular): 49 spans,
0x10001f0000-0x1000200000 … 0x306ae30000-0x306ae60000, 46× prot 3 + 3× prot 0, strictly
monotone non-overlapping, sizes 64 KiB … ~1 GiB (the largest is the prot-0 reservation
0x1f01760000-0x1f40000000), total ~1009 MiB. Re-derived independently from the raw log for
this arbitration (matches `vmmap-v3-records.jsonl`).

**H1 (real as-is) supports:**
1. Aligned-phase provenance (§2): the only mechanism that yields coherent 16 KiB-granular
   (start,end) pairs with independent prots is φ=0 verbatim reads.
2. **Prot/end-independence discriminant (refutes the alias/displacement family of errors):**
   if the family starts/ends were 4-byte-phase aliases, the prot slot would read end's byte
   4, i.e. prot == (end>>32)&0xff would hold for every member. Observed: 0/46 prot-3 spans
   match; (end>>32)&0xff for prot-3 spans ranges over {16,17,18,19,20,21,22,23,24,25,26,27,
   28,29,30,32,33,34,48} — wildly inconsistent with prot=3, perfectly consistent with a real
   independent prot field. Likewise if the family were low spans shifted up, their prot bits
   would still be coupled through any alias; they are not.
3. Monotonicity/non-overlap across 44 parts interleaved with corrupt slots — corruption
   replaces/remaps slots; it does not weave an ordered 176-GiB-wide monotone series through
   880 positions. The family's listed indices run 90..954 with the artifact/zeros pattern
   continuing AROUND them, exactly as phase classes predict.
4. 64 KiB granularity of large anonymous/UMA allocations is the natural granularity for a
   shipping UE5-style title's allocator, and 3 PROT_NONE reservations incl. a 1 GiB one are
   normal allocator behavior.
5. No in-tree source caps PS5 user VA at 36 bits (§4); the family itself then just measures
   the VA width: ≥ 0x306ae60000 (~38.4 bits) — plausible on a FreeBSD-11-amd64-derived
   kernel (stock VM_MAXUSER_ADDRESS = 0x800000000000) even though the exact PS5 cap is not
   documented in-tree.

**H2 (systematic displacement +k·2^32 / +k·2^36) refutations:**
1. No mechanism (§2): the payload path is sysctl → fixed-stride cast → field copy → emit.
   Displacement requires an adder that does not exist. The recovery worker's own ANALYSIS.md
   §2(5b) conceded this ("weakened by the fact that the raw sequence is perfectly monotonic
   while subtracting k*2^32 is not [corroborated]").
2. The only testable shifts land the family inside already-trusted spans with wrong
   protections: −1×2^36 puts family members 0x1000…→0x1f0000… and 0x306ae60000→0x2ae60000,
   i.e. overlapping the TRUSTED exec-image span 0x400000–0x787c000 (prot 4) with claimed
   prot 3 — contradiction. No k·2^32 places the (start,end) pairs jointly inside the
   recovered low spans (any single-span containment hits are coincidental subsets).
3. The worker's motivating anomaly — prot fields containing full addresses
   (0xff88c000, 0xee6e4000, hist prefix) — is fully explained by φ-shifts (§2.2) and needs
   no global displacement.

## 4. In-tree FreeBSD/PS5 VM layout knowledge (36-bit? 40-bit? GPU regions?)

Searched: `.superpowers/**`, `outputs/2026-10-04-zcode-team/**`, `docs/**` for 36-bit/40-bit,
VA space, vm_map_max, VM_MAX_USER, GPUVA/linear/gpu_va. Findings:

- **No in-tree source documents the PS5 user VA width.** The "36-bit address space" appears
  only as a *worker assumption*: worker-mapping-recovery CLAIM.json
  (`structural_constraints_planned: "…36-bit address space"`) and worker-ingame-probe-v3
  vmmap_walk.c:39 ("variable kve_structsize and 36-bit addresses" — a sanity-bound input,
  not a measured fact). Neither cites a source. The 36-bit figure that flagged the family is
  therefore unsourced and should be treated as void.
- The SDK user.h declares no VA-width constant; it is FreeBSD releng/11.4 userland ABI
  (amd64), where the stock kernel user VA ceiling is 0x800000000000 — no in-tree PS5
  override exists.
- GNM/GPU knowledge in-tree is limited to driver-import receipts
  (`worker-e2e-rehearsal/composition/receipts/console-provider-libSceGnmDriver*.stdout`,
  same under `worker-ps5io-rdwr/receipts/native-link/`): libSceGnmDriver imports exist, but
  no receipt maps GPU/linear VA regions. PS5 GPU 40-bit VA is external knowledge, NOT
  in-tree; nothing here lets us attribute the family to GPU space, and no KVME_TYPE in the
  artifacts suggests device regions at those addresses (the family prots are 3/0 = plain
  anon RW / NONE).
- Bottom line: in-tree knowledge neither predicts nor forbids >36-bit CPU VAs. The family,
  if real (as found), is itself the first in-tree evidence that this process's user VA
  space extends beyond 2^36 (≥ ~0x306ae60000).

## 5. Corrections to worker-mapping-recovery (amendments, raw reports preserved)

1. "kinfo entries returned by the kernel: 1243" → **1243 reader slots**; the kernel actually
   returned 10017 records of 144 B (v2) / 10088 (v1); only ~1243×1160/144 ≈ 10008 records
   were ever visible to the walk, 960 listed.
2. "1024 spans kept … 64 lost to truncation" → IG_SPAN_MAX reader-slot cap; **219 slots
   (219×1160 bytes ≈ 275 real records) never read**; there was no filter.
3. The "budget check" (r=50/w=48/x=2 vs spans_r/w/x=57/52/5 "strong validation") is **void**:
   spans_r/w/x were computed over all 1024 reader slots *including corrupted ones*
   (experiments.c:216–222), so they count artifact prots, not real spans. The "3 missing
   exec anchors" prediction inherits this and is withdrawn (those anchors are next-record
   starts at φ=136, §2.2).
4. Trusted spans 0x400000–0x787c000/4 and 0x8018c000–0x100118000/1 are CONFIRMED real:
   their start slots are not structsize-typed (refuting φ≠0), and v1+v2 agree.
5. The 6 unflagged plausible low spans (0x7ed8ac000–0x7ed90c000, 0x800250000–0x800258000,
   0x800e8c000–0x800e90000, 0x88131c000–0x88149c000 etc.): consistent with φ=0 reads; keep
   plausible status until v3 confirms (their neighborhoods contain φ≠0 slots, so per-slot
   alignment must be re-derived by v3's structsize walk, not assumed).
6. The 49 high-family spans: upgrade **flagged-plausible → real (high confidence)**, with
   v3 confirmation still required by the acceptance rules (candidate ≠ validated).
7. The 0x787c000–0x8018c000 gap analysis stands (anchors/pairs are real record fields seen
   through phases), and the GUA prediction (a W span covering 0x97ba110 = 0x400000+0x93ba110
   inside the gap) is unchanged — the family being real does NOT relocate GUA: GUA is in
   eboot LOAD7 BSS by the byte-cited vaddr-semantics work, and no family span is a
   plausible L7 BSS host (the only ≥2×0x27e110-sized family span is the 1 GiB PROT_NONE
   reservation 0x1f01760000–0x1f40000000).

## 6. Falsifiable predictions for v3's clean dump (both hypotheses)

If H1 (REAL) is true, v3's structsize-respecting walk (mode 1) should show:
- **P1 (record structure):** every record reports kve_structsize = 144 (0x90); total record
  bytes equal the sysctl length exactly (v2 run: 1442448 = 10017×144); the walk ends exactly
  at buffer end with no short/insane record. If instead sizes are variable, the *majority*
  must still be 144 and the v2 fixed-stride replica (mode 2) must reproduce the exact 18-slot
  artifact period.
- **P2 (family):** spans at [0x10001f0000..0x306ae60000] appear verbatim (same order, sizes,
  prots 46×3 + 3×0, modulo real churn between boots; same-boot replay must be byte-identical).
- **P3 (artifact classes vanish):** zero {0,0,0} slots; no 0x0N00000090 starts; the 53
  period-18 artifacts and 103 odd values are gone; every listed entry has start<end,
  16 KiB-granular, prot ≤ 7.
- **P4 (mechanism replay):** v3's mode-2 fixed-stride replica of the v1/v2 walk over the
  same buffer reproduces the v2 artifact pattern (period-18 structsize-type starts, zero
  flood) — a direct causal proof, cheap to run.
- **P5 (counters):** r/w/x over REAL records will NOT be 57/52/5 (those counted corrupt
  slots); expect r,w ≥ 50+ from the family alone plus the low map.
- **P6 (gap, independent of the family question):** the gap [0x787c000,0x8018c000) contains
  real spans (the 47 anchors/18 leaked pairs materialize as boundaries), including a
  writable span covering 0x97ba110 (GUA = 0x400000+0x93ba110 under mapped_start(LOAD0)
  = 0x400000).

If H2 (DISPLACEMENT) is true instead:
- **D1:** the clean dump shows NO spans ≥ 2^36; exactly 49 spans materialize at a constant
  offset −k·2^32 or −k·2^36 from the family, preserving order/sizes/prots, with k uniform
  across all 49; and no writable span exists covering any [2^36, 2^37) address.
- **D2:** record sizes are not 144 (P1 fails), so the period-18 mechanism is void and the
  φ=0 alignment argument collapses — then the family's 46 independent prot-3 values would
  need an alternative explanation (none currently known).

Decision rule: P1+P2 observed → family REAL, VA width > 36 bits established, close the
flag. D1 observed → displacement, adopt k, re-map the family. Neither (family absent, no
shift) → v3 bug or extreme churn; re-arbitrate with the raw v3 buffer snapshot (v3 already
snapshots the 136 pre-path header bytes per record — sufficient to re-derive everything
offline).

## 7. Evidence index

- SDK layout: `.superpowers/native-sdk-build-check/ps5-payload-sdk/target/include/sys/user.h:414-488`
  (KVME_*, KINFO_VMENTRY_SIZE=1160, struct kinfo_vmentry, path-truncation comment :486).
- v2 walk/filter/writer: `outputs/2026-10-04-zcode-team/worker-ingame-probe-v2/src/experiments.c:103-259`
  (stride :134, kve_size :127, IG_SPAN_MAX cap, no filter, per-part writer, r/w/x and
  capped histogram code), `src/factstore.h:11-20` (IgSpan, IG_SPAN_MAX=1024).
- v3 prior insight: `outputs/2026-10-04-zcode-team/worker-ingame-probe-v3/src/vmmap_walk.c:9-40`,
  `CLAIM.json` (writer/stride fixes; 36-bit as assumption).
- Raw records: `outputs/2026-10-04-zcode-controller/console/ingame-v2-facts-2026-10-04.jsonl`
  (v1_vmmap_full seq3: final_buf 1442448, kve_size 1160, entries 1243, attempts 6, spans_kept
  1024, spans_r/w/x 57/52/5; 48 v1_vmmap_page parts, 960 entries; v2_scan_page seq53),
  `ingame-v1-facts-2026-10-04.jsonl` (g5_vmmap: final_buf 1452672, identical early artifacts).
- Recovery under arbitration: `outputs/2026-10-04-zcode-team/worker-mapping-recovery/`
  (ANALYSIS.md §2(5) flag, §5 cross-check rules; CLAIM.json 36-bit assumption; REPORT.md).
- All numbers in §2–§3 re-derived from the raw log during this arbitration (repair-parser:
  the known missing-comma writer bug is deterministic and was repaired mechanically; raw
  bytes preserved, corrections recorded here as amendments).
