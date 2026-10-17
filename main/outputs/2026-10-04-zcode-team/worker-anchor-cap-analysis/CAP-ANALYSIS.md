# CAP-ANALYSIS — kMaxAnchorMappings=8 vs the observed image fragmentation

worker-anchor-cap-analysis · 2026-10-05 · local in-tree evidence only (no
console, no network, no canonical edits). Provenance for every number is in
`CLAIM.json` (same directory).

## 0. Answer up front

**The observed shape needs 7 intersecting mappings; the cap is 8; margin = 1.**
The best-known runtime fragmentation of the anchored image extent
`[0x400000, 0xc0602c8)` (igf3 clean `kve_structsize` walk, corroborated by the
launch-9 registration count) is **7 records: 1 exec-only + 6 readable + 0
others**. One extra fragment saturates the cap exactly (8, no loss); two extra
fragments silently drop the **highest** record — `[0x970c000,0x9a34000)` prot=3,
the fragment that hosts GUA `0x97ba110` — because the collection loop keeps the
lowest 8 in ascending-address order.

**Recommendation: fold `kMaxAnchorMappings` 8 → 16 into the staged fix before
the launch-10 build** — one line in
`.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/live_module_facts.hpp:88`:

```diff
-static constexpr size_t kMaxAnchorMappings = 8;
+static constexpr size_t kMaxAnchorMappings = 16;
```

## 1. Extent actually collected by the fix

- The fix collects mappings **intersecting** `[chosen_base, chosen_base +
  chosen_extent)` when `identity_source == VmScan && chosen && extent != 0`
  (`staging/live_module_facts.geometryfix.cpp:467-490`).
- The launch-9/launch-10 identity route is the r13 data-window anchor:
  `chosen_base = 0x400000`, `chosen_extent = 0xbc602c8`
  (`runtime_start_bridge_r13` overlay lines 157/165, planted via
  `VmScanResult` at lines 620-621) → extent **`[0x400000, 0xc0602c8)`**
  (worker-facts-geometry-fix/ANALYSIS.md §0 pins the same span).
- The task brief's `[0x400000, 0xfc602c8)` is the **r11-era clamp** preserved
  in an r13 comment ("the extent clamp 0xfc602c8 is NOT page-aligned"); the
  constants the fix actually consumes give `0xc0602c8`. On pinned evidence the
  distinction does not change the count (zero igf3 records in
  `[0xc0602c8, 0xfc602c8)`), but any future allocation in that extra ~600 MB
  would be collected under the r11 figure only.
- The extent end is **exactly the LOAD8 phdr boundary**
  (`p_vaddr+p_memsz` = `0xbc602c8`; page-up would be `0xc064000`) and is NOT
  16K-page-aligned.

## 2. Best-known runtime fragmentation of `[0x400000, 0xc0602c8)`

Source: igf3 clean walk (build `igf3-de0c0cb644d3`, `walk_mode=1`,
`ksz_variable_records=21721`, `ksz_144_records=21721`, `insane_records=0`,
`fallback_used=0` — parsed record at
`worker-parser-validation/run2/ingame-v3-full-results.parsed.jsonl:66`,
records from its 1024-part/16-entry page family). Cross-checks: launch-9
`bridge_lease_reg_spans=6` readable (entry log lines 17035-17040), the v3
MAPPING_BREAKTHROUGH ledger pins (`0x953c000` first-RW, GUA fragment), and the
phdr page arithmetic (`worker-geometry-reference/phdrs-derived.json`).

| #  | start      | end        | prot | size       | role (phdr correspondence)                                | readable |
|----|------------|------------|------|------------|------------------------------------------------------------|----------|
| 1  | 0x400000   | 0x787c000  | 4    | 0x747c000  | LOAD0 `--X`, contains anchor base (record #0 of the walk)  | no       |
| 2  | 0x787c000  | 0x82bc000  | 1    | 0xa40000   | LOAD1 `R--` (page-round(0x82ba918) + inter-LOAD gap merged)| yes      |
| 3  | 0x82bc000  | 0x953c000  | 1    | 0x1280000  | LOAD3 `RW-` observed **R--** — PT_GNU_RELRO demotion       | yes      |
| 4  | 0x953c000  | 0x9568000  | 3    | 0x2c000    | LOAD7 head RW (start byte-pinned by v3 breakthrough)       | yes      |
| 5  | 0x9568000  | 0x96dc000  | 1    | 0x174000   | LOAD7 mid **R--** demoted fragment                         | yes      |
| 6  | 0x96dc000  | 0x970c000  | 3    | 0x30000    | LOAD7 RW (hosts GMalloc candidate 0x970add8)               | yes      |
| 7  | 0x970c000  | 0x9a34000  | 3    | 0x328000   | LOAD7 tail RW, hosts **GUA 0x97ba110** (FUObjectArray)     | yes      |

- **Intersecting the extent: 7.** Readable (prot&1): **6** — exactly the
  launch-9 `bridge_lease_reg_spans=6` from an independent boot.
- LOAD8 `---` `[0x9a33420,0xc0602c8)` (40 MB) is **not materialized** as any
  vmmap record in this walk.
- Zero prot-0 records inside the extent; zero records spanning across
  `0x400000` from below; the exec span is snapshot record #0.
- LOAD7 appears as **4 fragments** (records 4-7) — the pinned worst-case
  per-LOAD fragmentation behavior of this kernel/loader.

## 3. Expected `anchor_mapping_count` under the fix, and margin

The fix counts **pre-D2-filter** (exec + readable + prot-0 + any record
touching the extent all enter `anchor_mappings`; the adapter filters later):

```
count = 1 (exec) + 6 (readable) + 0 (prot-0/other) = 7   →  margin vs cap 8 = 1
```

Saturation ladder:

| shape delta                                             | count | outcome                                        |
|---------------------------------------------------------|-------|------------------------------------------------|
| pinned observed shape                                    | 7     | fits, 1 slot spare                              |
| LOAD8 materializes as 1 prot-0 record                    | 8     | **at cap**, zero margin, no loss                |
| LOAD8 as 2 records, OR any extra split/guard page inside | ≥9    | record #7 (GUA host) **silently dropped**       |

Direction of loss: records are collected first-come in VMMAP (ascending
address) order with the cap test before insert
(`geometryfix.cpp:483`), so overflow drops the **highest** records — exactly
the GUA-hosting tail, the most load-bearing span for launch-10's armed reads.
Consequence if dropped: the adapter's D2-filtered `readable_ranges` lose
`[0x970c000,0x9a34000)`; conversion still succeeds (≥1 readable span survives;
refusal risk stays low because the 6 readable records rank inside the first 8
under any plausible shape), but stage-2 reads of GUA fall outside the declared
ranges → readpath-guard / required-slot refusal — deterministic degradation,
not memory unsafety, and a lost launch-10 verdict.

Plausible >8 shapes (none observed, all consistent with pinned behavior):
LOAD8 materializing (its 40 MB `---` range exists in the phdr); further RELRO
demotion splits inside LOAD1/LOAD3; LOAD7 splitting beyond 4 (guard/sub-range
demotions); a prot-0 reservation or guard page landing inside
`[0x400000,0xc0602c8)` (153 prot-0 records exist process-wide, just none in the
extent on the pinned boot).

## 4. Recommended cap

**`kMaxAnchorMappings = 16`** (header line 88), rationale:

- **Semantic cost:** `anchor_mappings` is a fixed array inside the in-process
  facts snapshot (`live_module_facts.hpp:178-179`; `VmMapping` = 20 B, 24 B
  padded). 8→16 adds ~192 B to a snapshot that already carries
  `kMaxVmMappings=1024` mappings (~20 KB). Negligible.
- **Loop bounds:** the collection bound
  (`facts_.anchor_mapping_count < kMaxAnchorMappings`) and the adapter's
  count-bounded iteration need no change; no arithmetic on the cap; overflow
  behavior of `checked_add_u64` unaffected.
- **ABI:** every consumer (bridge PRX, entry PRX, core TU, conversion adapter,
  host fixtures) compiles the single shared header, and the raw snapshot never
  crosses a module boundary (conversion happens in-process) — a constant-only
  change with a full rebuild is safe.
- **16 vs 12:** worst pinned fragmentation is 4 records for ONE LOAD (LOAD7);
  a stack of LOAD0(1) + LOAD1(≤2) + LOAD3 RELRO(≤2) + LOAD7(≤6) + LOAD8(≤2) +
  a guard/reservation(≤1) reaches 11-14. 16 covers the plausible worst case
  with visible slack; saturation remains detectable via `lmf_anchor_count=16`.

Minimum-defensible alternative if 16 is rejected: 12 (covers the ladder through
"LOAD8 as 2 + one extra split"). Do NOT ship 8 expecting the diagnostics to
catch saturation on launch-10 — the failure it causes is silent loss, and the
verdict it corrupts (GUA reads) is exactly what launch-10 is armed to test.

## 5. Sanity checks on the collection loop (task item 4)

1. **Extent exactly at a mapping boundary.** Real and pinned: the extent end
   `0xc0602c8` is exactly LOAD8's phdr end (not a live record boundary — the
   top record ends at `0x9a34000`). The open-interval test
   `start < scan_end && base < mappings[m].end` is boundary-correct: a mapping
   ending exactly at `scan_end` is collected (adapter clamp yields non-empty);
   a mapping starting exactly at `scan_end` is excluded — matching the bridge
   registration's `b_raw >= e_raw` skip. Consistent pair.
2. **Mappings fully inside the extent.** Collected (both comparisons strict);
   adapter clamp is identity. All 7 observed records qualify.
3. **prot values observed on PS5.** In-extent: 4 (exec-only), 1 (R), 3 (RW) —
   includes the RELRO-demoted LOAD3 and a mid-LOAD7 R-- fragment, so "RW-
   appears-as-R--" is real and the D2 read-bit filter is the correct
   predicate. Process-wide (igf3): 1×147, 3×16011, 4×71, 0×153, plus ONE 5
   (R-X) and ONE 7 (RWX) — exotic combos exist but never in the extent. The
   collector treats protection as an opaque int32 — correct; the D2 filter
   keeps 1/3/5/7 and drops 0/4, so exec-only never leaks into readable ranges.
4. **kve_structsize variable stride.** Does not affect the count. The shipped
   walk strides by each record's own `kve_structsize` (all 144 B in both clean
   observations; `insane_records=0`, `fallback_used=0`), refuses loudly on
   malformed sizes (`query_vmentry` −6), and the count is a record count,
   stride-independent. The v1/v2 fixed-1160-stride logs must NOT be used for
   counting: their artifact rows (e.g. start=0, end=0x100000000;
   structsize-typed starts) would pass the intersect test and inflate the
   count — one more reason for cap headroom if a fallback walk ever ran.
5. **Upstream 1024-record truncation** (`kMaxVmMappings`): the exec span is
   snapshot record #0 and all image records sit at the front (35 records
   below `0x100118000` on the pinned boot), so `vm_copied=1024` cannot drop
   them; `vm_mappings_truncated=1` (totals 9,966-22,321 across boots) is
   already an honest visible flag. Only a boot with >~1017 mappings below the
   image top could starve the set — no evidence, same observable
   (`lmf_anchor_count` below the readable count + 1).
6. **Overflow path.** `checked_add_u64(base, chosen_extent)` cannot overflow
   with the pinned constants; on overflow the containing rule applies
   unchanged — safe legacy behavior.

### Bonus finding for launch-10 log reading

The igf3 readable records 2-7 are **pairwise adjacent**
(`0x787c000→0x82bc000→0x953c000→0x9568000→0x96dc000→0x970c000→0x9a34000`), and
`derive_readable_ranges` coalesces overlap AND adjacency — so on an
igf3-shaped boot the success-path diagnostic should read
**`lmf_anchor_count=7`, `lmf_intervals_found=1`** (single coalesced run
`[0x787c000,0x9a34000)`) next to `bridge_lease_reg_spans=6` (raw spans). The
staged fixture assumed page-gapped, non-adjacent readable spans
(`test_geometry_fix_host.cpp:144-145`, expecting intervals == 6); if launch-10
prints `lmf_intervals_found=1`, that is the predicted coalescing divergence
(worker-facts-geometry-fix ANALYSIS.md §6.2), NOT a fix failure. Byte-wise the
lease coverage is identical either way.
