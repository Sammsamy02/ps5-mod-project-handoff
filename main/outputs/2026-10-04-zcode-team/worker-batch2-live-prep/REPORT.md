# REPORT — worker-batch2-live-prep (rung-3 live-read sequence: GUA → chunks → objects)

CLAIM.json was written first; this REPORT is the last artifact. Everything
written lives under `outputs/2026-10-04-zcode-team/worker-batch2-live-prep/`.
No network, no console, no nested subagents; all sibling files consumed
read-only. 26 tool calls used (bound: 80). Scope honored: design + host-test
prep; the console run itself is NOT part of this assignment.

## 0. Deliverables

| File | Purpose |
|---|---|
| `CLAIM.json` | ownership + bounds (written first) |
| `src/live_gua_reads.h` | C99 header: KVME span table, three-way-classified reads, rung-3 record + check states, full provenance block |
| `src/live_gua_reads.c` | implementation; the ONLY memory access is `igu_read()` through `span->data` after a covering-READABLE-span proof; no OS calls, no allocation |
| `tests/test_live_reads.cpp` | 20-test host suite on live-shaped synthetic fixtures; `-Wall -Wextra -Werror` clean |
| `tests/test-run.txt`, `tests/test-run-asan.txt` | committed logs: **111/111 PASS**, ASan+UBSan clean (binaries `test_live_reads`, `test_live_reads_asan`) |

## 1. The live-read sequence (commissioned steps a–e, exactly)

Implemented as `igu_probe_rung3(&span_table, gua_addr, scan_max, &rec)` in
`src/live_gua_reads.c`; every step carries its own status so partial evidence
is never lost:

1. **Header** (prereq): read the 96-byte window GUA+0x00..0x2F at
   `gua_addr` (layout A). Records `first_gc/last_non_gc/max_not_gc/
   open_dfgc` (0x00–0x0C) and `max_elements/num_elements/max_chunks/
   num_chunks` (0x20/0x24/0x28/0x2C) — offsets cited from
   worker-predicate-gc-fix `vendored-fixed/src/townfall_layout_probes.cpp`
   lines 23–25,31. Non-gating info checks recorded:
   `max_elements == max_chunks*65536` (live truth: 2162688 == 33×65536),
   `num_elements <= max_elements`, `1 <= num_chunks <= max_chunks`.
2. **a) Objects\*\*** = u64 @ GUA+0x10 (live value `0x13800220a0`). Null →
   `IGU_R_NULL`; non-canonical ((p>>47)!=0) → `IGU_R_NONCANONICAL`.
3. **b) chunk[i]** = u64 @ Objects\*\* + i·8, i from 0 while i < num_chunks
   (NULL chunk pointers skipped; non-canonical refutes). Records
   `chunk_ptr`, `chunk_index`.
4. **c) FUObjectItem** (0x18: Object@0x0, Flags@0x8, ClusterRootIndex@0xC,
   SerialNumber@0x10 — SET-5_06, stride 0x18) at
   `chunk_ptr + slot·0x18`, scanning up to `scan_max` (≤64, batch2 walk-cap
   parity) for the first non-null, canonical Object. Null items are legal
   GC slots (recorded raw, never refuted). Chunk math per batch2/UOA-HPP:
   65536 ELEMENTS per chunk (identity re-proven by the live 33×65536
   equality, not assumed).
5. **d) UObjectBase** (0x28) at `item_object`: vtable@0x0 (identity-only,
   never called), ObjectFlags u32@0x8, InternalIndex i32@0xC,
   ClassPrivate@0x10, NamePrivate raw 8B@0x18 (copied, NEVER interpreted —
   P2 rule), OuterPrivate@0x20.
6. **e) checks** → `IGU_CSK_PASS / CONTRADICTED / UNAVAILABLE` per check:
   vtable non-null+canonical+8-byte-readable; `obj_flags & ~0x0FFFFFFF == 0`
   (RF_AllFlags, UnrealFlags.hpp:95); index bounds + **round-trip**
   `item[InternalIndex].Object == item_object` via the chunk math (incl.
   cross-chunk idx≥65536 walking Objects\*\*[1]); ClassPrivate non-null +
   readable; OuterPrivate null-or-readable. Overall
   `igu_rung3_class()`: 0 = all pass, 1 = Inconclusive (any coverage
   failure / exhausted scan), 2 = Refuted (CONTRADICTED or non-canonical).

**Prompt-vs-evidence discrepancy (must be recorded):** the commissioning
prompt listed `OuterPrivate@0x18 / NamePrivate@0x20`. The retained
SET-5_06 setter and batch2's validated provenance both place NamePrivate
BEFORE OuterPrivate (0x18 < 0x20). The code follows the cited setter and
records the raw 0x18 window untouched, so the conflict is visible, never
silently resolved.

## 2. Readable-span coverage of 0x13800220a0 (task 2) — VERDICT: not_covered_current_map

From `worker-mapping-recovery/vmmap-v3-records.jsonl` (recovered, arbitrated
REAL by `worker-span-arbitration/VERDICT.md`):

- Nearest recovered span below: `[0x1340c40000, 0x1340c50000)` prot 3.
  Nearest above: `[0x13803b0000, 0x13803c0000)` prot 3.
  **0x13800220a0 falls in the gap** (≈0x3F6B0000 ≈ 1014 MiB wide; the chunk
  table sits ≈0x38DF60 ≈ 3.6 MiB below the upper neighbor's start — same
  0x138 high-prefix family, noted for arbitration, no conclusion drawn).
- High-span context: 49 family spans 0x10001f0000–0x306ae60000, 46 prot 3
  (KVME R|W) + 3 prot 0 reservations (largest 0x1f01760000–0x1f40000000).
  The top of the map (0x306ae60000) is far above the chunk-table address;
  "up to 0x306ae60000" does NOT cover 0x13800220a0 — coverage is per-span,
  not per-extent.
- **Absence here is NOT unreadability.** The recovered map is provably
  incomplete: (a) the validated GUA span `[0x970c000, 0x9a34000)` prot 6
  (worker-anchor-validation RECEIPT:127) is itself missing from the
  recovered list; (b) the v2 walk read only ~1243 of 10017 records
  (fixed 1160 stride over 144-byte kernel records; 219 slots ≈ 275 records
  never read). Correct classification: **not_covered_current_map = evidence
  unavailable** — exactly the taxonomy worker-anchor-validation's stage plan
  uses. Consequence: rung 3 on target must FIRST re-walk the vmmap
  in-process (v3 variable-`kve_structsize` walk,
  worker-ingame-probe-v3/src/vmmap_walk.c pattern) and treat every pointer
  read as refuse-to-dereference until its window is covered by a READABLE
  span of that fresh map. `IGU_E_NO_SPAN` / `IGU_E_NOT_READABLE` /
  `IGU_E_STRADDLE` carry this classification per read.

**Protection-encoding hazard found and handled:** the captured GUA span was
logged "prot 6" — POSIX mode bits (RW) — while kernel `kinfo_vmentry` uses
KVME_PROT (R=1,W=2,X=4), where 6 = W|X (unreadable!). This exact bug
surfaced in my own first host run (reads refused as not-readable). The
module requires KVME-encoded spans and provides
`igu_posix_mode_to_kvme()` for suppliers converting older probe-side logs
(test t20). Related open item for arbitration: the image span
0x400000–0x787c000 is recorded prot 4 (X-only); under the strict
`prot & KVME_PROT_READ` policy a vtable there classifies as
covered_not_readable (Inconclusive, never Refuted) until image prot
semantics are re-arbitrated. Conservative by design.

## 3. PRX integration contract

```c
/* in-process, once per snapshot: */
build span table from sysctl {CTL_KERN,KERN_PROC_VMMAP,getpid()} using the
  v3 variable-kve_structsize walk; translate any POSIX-mode records via
  igu_posix_mode_to_kvme(); igu_span_set_data_identity() on every span;
igu_rung3_record rec;
int rc = igu_probe_rung3(&table, 0x97ba110 /*gua*/, IGU_SCAN_MAX_ITEMS, &rec);
int cls = igu_rung3_class(&rec);   /* 0 ok / 1 inconclusive / 2 refuted */
```

Safety invariant: the module has NO raw dereference path — the only memory
read is inside `igu_read()`, through `span->data`, after `igu_find_covering()`
proved the whole window inside ONE span with READ. An uncovered address can
never fault the game. C99, `-Wall -Wextra -Werror` clean as a separate C
translation unit, linkable from the PRX's C sources.

Integration note for batch2: `tlp::MemSpan` has no prot field and batch2's
`read_bytes` trusts spans unconditionally — batch2 may only be handed spans
this module (or an equivalent gatekeeper) has proven covered-and-readable.
`igu_probe_rung3` is that gatekeeper for the chunk/object chain; only on
`cls == 0` should the newly confirmed regions be published as MemSpans to
the predicate layer.

## 4. Host-test evidence (task 4)

20 tests / 111 assertions, live-shaped values throughout (GUA 0x97ba110 in
the captured span `[0x970c000,0x9a34000)` prot R|W, Objects\*\*=
0x13800220a0, 71959 elements, 33/2 chunks, first_gc 35477 / last_non_gc
35476 — worker-staged-pair-live CLAIM + predicate-gc-fix). Chunk/object
storage addresses beyond the chunk table are synthetic and labeled as such.

Highlights: happy path with live header round-trip + 65536-chunk-math
round-trips at index 5, 65536 and 70000 (cross-chunk via Objects\*\*[1]);
chunk table not-covered → Inconclusive; covered PROT_NONE → not-readable;
null-scan exhaustion at bound 3 → Inconclusive; non-canonical Objects\*\*
and item object → Refuted; flags outside RF_AllFlags, negative index,
round-trip mismatch, null ClassPrivate, non-canonical Outer → Refuted;
uncovered/covered-not-readable vtable and ClassPrivate → Inconclusive
(UNAVAILABLE, never Refuted — batch3 parity); 0x28-window straddle →
Inconclusive; header-window uncovered / PROT_NONE → Inconclusive;
chunk[0]=NULL fall-through to chunk[1]; POSIX→KVME translation (t20).
Reuse note: same conventions as batch2/batch3 suites (`-Wall -Wextra
-Werror`, ASan+UBSan runs, committed logs), fixtures shaped on the same
live-validated GUA constants their T14/dryrun images use.

## 5. What these reads would establish for the layout chain (task 5)

Today's validated facts stop at the GUA header bytes (rung 2). Steps a–e
are precisely the missing bridge to rung 3:

1. **a (Objects\*\* dereference):** first live proof that the header's
   `Objects` value is a real mapped chunk array — converts layout A from
   "header bytes consistent" to "pointer structure resolves". If
   not-covered/not-readable: evidence unavailable → re-arbitrate the map;
   if read and non-canonical/NULL: the first genuine refute-class fact
   against layout A's TUObjectArray interpretation.
2. **b (chunk[0]) + c (FUObjectItem):** first live view of TUObjectArray
   chunk storage — unlocks batch2's `uobject_item_chain` round-trip
   accounting on target (today satisfied only synthetically), and exercises
   the 65536-element chunk identity (33×65536 = 2162688) against real
   chunk pointers.
3. **d+e (UObjectBase fields):** the exact gate batch2's
   `probe_ustruct_chain` requires per object (vtable-in-readable-span,
   RF mask, index round-trip, ClassPrivate/OuterPrivate) — running it live
   turns batch2's host-proven predicate into target evidence and produces
   `first_ustruct_like` / `first_uclass_like` anchors.
4. **Downstream unlocks (batch2/batch3):** with an anchor + confirmed
   spans: `probe_ustruct_chain` UStruct/UClass layers (SuperStruct
   acyclicity, Children/ChildProperties heads, CDO@0x110 visibility),
   `probe_ffield_variant` (UE5 FField vs UE4 UProperty discriminator),
   batch3 `probe_ustruct_children_walk` / `probe_ustruct_childproperties_walk`
   / `probe_uclass_property_links` (CDO back-pointer, Property/Ref/
   Destructor link walks). Per batch3 PROMOTABLE-SET this is the gate for
   the 55 STAGEABLE_TODAY members; everything else stays honestly blocked
   (8 no-width, 9+1 UFunction classifier-only, FName P2, identity
   controller-owned).
5. **Failure taxonomy is evidence, not noise:** each step's status names
   the exact missing fact (`st_b == IGU_E_NO_SPAN` ⇒ "chunk table not in
   current map" → extend map, not "layout refuted"; `chk_roundtrip ==
   CONTRADICTED` ⇒ index structure contradicted → refute-class). This keeps
   the batch2/batch3 Validated/Inconclusive/Refuted discipline intact for
   the live run and gives the controller machine-readable
   settling-evidence for every non-Validated outcome.

Also note: `worker-staged-pair-live/CLAIM.json` planned a
`RUNG3-REQUIRED-READS.md` that never materialized (that directory holds
only the CLAIM). This REPORT §1–§5 supplies that missing deliverable.

## 6. Honest limits

- Host fixtures are synthetic beyond the chunk-table address: nothing in
  this work establishes any target fact about chunk[0], items, or objects.
  The only live-target numbers reused are the already-validated header set.
- 0x13800220a0 readability is UNDECIDED until an in-process vmmap re-walk;
  no claim is made either way (not_covered_current_map).
- The vtable-readability policy question (image prot 4 records) is flagged,
  not resolved.
- Divergence recorded: batch2 maps an unreadable ClassPrivate to
  refute-class `OBJ_R_CLASSPTR_BAD`; this module (and batch3's parity rule)
  classifies coverage failures as UNAVAILABLE/Inconclusive and refutes only
  read-and-contradicted facts. The controller should apply one policy
  explicitly when the live run lands.
