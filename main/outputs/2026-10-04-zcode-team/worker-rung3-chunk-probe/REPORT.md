# REPORT — worker-rung3-chunk-probe (rung-3 chunk/object read ADD-ON for the next probe iteration)

CLAIM.json was written first (entry marker included); this REPORT is the last
artifact. Everything written lives under
`outputs/2026-10-04-zcode-team/worker-rung3-chunk-probe/`. No network, no
console on target, no nested subagents; all sibling files consumed read-only.
~37 tool calls used (bound: 80). Scope honored: PREPARATION for the NEXT
console test (igf7-lineage); nothing ran on target and no target fact is
claimed.

## 0. Deliverables

| File | Purpose |
|---|---|
| `CLAIM.json` | ownership + bounds, entry marker (written first) |
| `src/rung3_chunk_addon.h` | C99 add-on header: minimal VMMAP-walk API (`rc3_vmmap_walk_min` + `rc3_vmstats` + v3 trust gate), per-chunk views (`rc3_chunk_view`), `rc3_record`, refusal status `RC3_ST_NO_MAP`, full provenance block |
| `src/rung3_chunk_addon.c` | implementation; ZERO new imports; every read flows through the batch2-live-prep `igu_read*()` after a covering-READABLE-span proof; walk never dereferences (byte-assembled loads) |
| `src/rung3_batch2_bridge.hpp/.cpp` | C++ bridge (lineage-consistent): readable-only `tlp::MemSpan` gatekeeper + gated `rfl::probe_ustruct_chain` run (UStruct chain, UClass CDO), POD result for fact records |
| `tests/test_rung3_addon.cpp` | 20-test host suite (156 assertions) on synthetic chunk/object structures matching the live GUA values; `-Wall -Wextra -Werror` clean |
| `tests/test-run.txt`, `tests/test-run2.txt`, `tests/test-run-asan.txt` | committed logs: **20/20 PASS, 156 asserts**, two runs **byte-identical**, ASan+UBSan **0 findings** |

## 1. The add-on sequence (commissioned steps a–g, exactly)

`rc3_probe_chunks(&table, &vm, gua, scan_max, &rec)` — built ON TOP of the
111/111-tested `igu_probe_rung3` core (included read-only from
worker-batch2-live-prep; no duplication, same types, same status codes):

1. **a) Minimal VMMAP re-walk** (`rc3_vmmap_walk_min`): v3
   variable-`kve_structsize` stride (KVE offsets 0/8/16/56, 136-byte
   pre-path header, sanity `start<=end`, extent<=2^44, prot<=0xff,
   136<=ksz<=kve_sizeof, 4-aligned; insane structsize STOPS, never guesses).
   Minimal by contract: the walk stops EARLY (stop_reason 3) once every
   caller-named target address is covered by a retained READABLE span —
   NOT a full dump; total/retained/truncated/insane/ksz_144/aligned16k all
   recorded. The v3 trust gate (`retained>=8 && insane*4<=retained`) is
   enforced STRUCTURALLY: `rc3_probe_chunks` takes the walk's `rc3_vmstats`
   and reads NOTHING when the gate fails (`RC3_ST_NO_MAP`, `map_trusted=0`)
   — the honest needs-map mode after the proven-incomplete recovered map.
   Spans are KVME-encoded with identity data pointers (live discipline);
   `igu_posix_mode_to_kvme()` remains the translation for old probe-side
   prot-6 logs (t11).
2. **b) Objects\*\*** = u64 @ GUA+0x10 (live `0x13800220a0`): null →
   `IGU_R_NULL`, `(p>>47)!=0` → `IGU_R_NONCANONICAL` (refute-class).
3. **c) chunk[0] AND chunk[1]** (8 B each @ Objects\*\*+i·8), recorded in
   separate `rc3_chunk_view`s — a chunk[1] coverage failure never discards
   chunk[0] evidence. Needed-chunk rule (worker-5
   `GOA_R_CHUNK_NULL_WHILE_NEEDED`): null while `i < ceil(NumElements/65536)`
   contradicts (live: 71959 elements needs BOTH chunks). `num_chunks<2` →
   `RC3_ST_NOT_RUN` (neutral, never degrades).
4. **d) first FUObjectItem of EACH chunk** (0x18: Object/Flags/
   ClusterRootIndex/SerialNumber per SET-5_06): bounded null-scan
   (≤64 slots, core parity); null items are legal GC slots (raw fields
   recorded from the last examined slot only); item-window coverage
   failures carry the exact `IGU_E_*` code; non-canonical Object → refute.
   Flags/cluster/serial are RAW — no cited mask, never judged.
5. **e) UObjectBase (0x28) from the first non-null item across both
   chunks** (chunk 0 preferred; source recorded in `obj_source_chunk`):
   vptr@0x0 (identity-only), ObjectFlags@0x8, InternalIndex@0xC,
   ClassPrivate@0x10, NamePrivate@0x18 (RAW 8 B, never interpreted — P2
   rule; prompt-vs-setter discrepancy inherited, not silently resolved),
   OuterPrivate@0x20.
6. **f) checks** → `IGU_CSK_PASS/CONTRADICTED/UNAVAILABLE`: vtable
   non-null+canonical+8 B-readable (X-only image records → UNAVAILABLE,
   never Refuted); RF_AllFlags mask; index bounds + round-trip
   `item[InternalIndex].Object == obj_addr` via the 65536-element chunk
   math; ClassPrivate non-null readable; Outer null-or-readable.
   Overall `rc3_class()`: 2 Refuted / 1 Inconclusive / 0 Validated-class;
   refute wins over coverage-class wins over OK; info checks stay
   non-gating (core discipline).
7. **g) batch2 predicate handoff** (`rc3b::run_ustruct_chain`):
   `publish_memspans()` is the GATEKEEPER — only spans with KVME READ prot
   become `tlp::MemSpan` views (withheld counts recorded, never silent),
   because batch2's `read_bytes` trusts spans unconditionally. The
   `rfl::ValidatedGuaView` (objects_ptr/num_elements/num_chunks from the
   record) is assembled ONLY when map + header + Objects\*\* + chunk[0] +
   UObjectBase steps are all `IGU_OK` and no info check contradicted
   (machine-readable gate codes: MAP_UNTRUSTED / STEP_FAILED /
   INFO_CONTRADICTED). Then `rfl::probe_ustruct_chain` runs unchanged:
   SuperStruct-acyclic UStruct classification, Children/ChildProperties
   heads, UClass extension via CDO@0x110 — producing
   `first_ustruct_like` / `first_uclass_like` / `STR_I_UCLASS_EXT_OK`.

## 2. Honest handling of "not covered by current map" (task 2)

Three explicit layers, each host-tested:

- **Walk level**: a target the fresh map does not cover keeps its
  `targets_mask` bit clear and disables the early stop (t02/t03);
  `targets_covered < targets_total` is machine-readable "map incomplete".
- **Read level**: every commissioned read that lands outside the fresh map
  yields `IGU_E_NO_SPAN` (or `IGU_E_NOT_READABLE` / `IGU_E_STRADDLE`),
  stored verbatim in the step status (t02/t04/t05/t06/t09) — recorded as
  Inconclusive, never guessed, never dereferenced.
- **Probe level**: an untrusted walk refuses ALL reads up front
  (`RC3_ST_NO_MAP`, t12) and the bridge refuses the batch2 handoff on the
  same evidence (t16/t19).

## 3. Host-test evidence (task 3)

Live-shaped values throughout: GUA `0x97ba110` inside the captured span
`[0x970c000,0x9a34000)`, Objects\*\* = `0x13800220a0`, MaxElements 2162688 =
33×65536, NumElements 71959, MaxChunks 33, NumChunks 2, first_gc 35477 /
last_non_gc 35476 (worker-staged-pair-live / worker-anchor-validation).
The synthetic kernel buffer uses 144-byte path-stripped `kinfo_vmentry`
records (one 152-byte record), KVME prot words, dummies before/after the
needed records. Chunk/object storage addresses are SYNTHETIC and labeled as
such. Highlights: happy path proves chunk[0]/chunk[1] ptrs + per-chunk first
items + UObjectBase fields byte-exact incl. round-trip; early-stop consumes
24 of 44 records and the probe still validates on the minimal table; bridge
run finds `first_ustruct_like`=STRUCT_A and `first_uclass_like`=UCLASS_C
(CDO@0x110) with the negative-evidence `STR_R_SUPER_BAD` recorded for a
non-struct plausible object; refutes (non-canonical Objects\*\*, null-needed
chunk), coverage classes (gap / W-only / straddle / X-only vtable),
exhausted scan, POSIX-6 translation, trust-gate refusal, determinism,
gatekeeper withholding, arg guards.

Two harness bugs were found BY the suite and fixed (both fixture-side, no
module changes): the determinism test compared raw `span->data` host
pointers/identity vaddrs (now compares span values + fixture bytes), and
the straddle fixture wrote past a shrunken span's host storage (writers now
auto-extend host storage; the MAP extent stays small). ASan+UBSan build of
module + core + bridge + both sibling predicate sources: 0 findings.

## 4. PRX integration contract for the igf7-lineage probe

```c
/* in-process, once per snapshot (7-import lineage; ZERO new imports) */
rc3_vmstats st;
igu_span spans[RC3_SPAN_CAP];                    /* caller-owned            */
const uint64_t targets[] = { gua, objects_cand, chunk0, chunk1, ... };
uint32_t n = rc3_vmmap_walk_min(vmmap_buf, vmmap_len, 1160,
                                spans, CAP, targets, 9, &st);
for (i = 0; i < n; ++i) /* live only */;          /* data already identity   */
igu_span_table t = { spans, n };
rc3_record rec;
int rc  = rc3_probe_chunks(&t, &st, 0x97ba110, IGU_SCAN_MAX_ITEMS, &rec);
int cls = rc3_class(&rec);        /* 0 validated-class / 1 inconv / 2 refuted */
/* cls==0 → C++ side: rc3b::run_ustruct_chain(rec, st, spans, n)  */
```

The bridge needs the sibling predicate sources in the build exactly as the
host build did (`live_gua_reads.c`, `townfall_layout_probes.cpp`,
`reflection_layout_predicates.cpp` + this add-on + bridge). On success the
fact record carries: walk stats, per-step statuses, both chunk views, the
UObjectBase fields (NamePrivate raw), all check states, and the batch2
verdict/codes/anchors — every non-Validated outcome machine-readable.

## 5. What this unlocks for the next console test

- First live proof of the chunk-table pointer (0x13800220a0 today) and of
  both live-needed chunks, with per-chunk first-item evidence — the exact
  reads batch2's `uobject_item_chain` round-trip needs on target.
- The first live UObjectBase behind a non-null item, with the full check
  family — the per-object gate `probe_ustruct_chain` requires; on cls==0
  the UStruct/UClass chain + CDO evidence (the batch3 PROMOTABLE-SET gate
  toward the 55 stageable members) becomes runnable on real bytes.
- The fresh-map-first discipline closes the recovered-map gap hazard:
  0x13800220a0 coverage is decided by the SAME snapshot that backs the
  reads, never by the stale arbitrated list.

## 6. Honest limits

- Synthetic beyond the chunk-table pointer: no target fact about chunk[0],
  chunk[1], items, or objects is established by this work.
- The walk's early-stop is only as good as the target list; callers should
  name every window the sequence will touch (GUA, Objects\*\*, both chunk
  item windows, object/class/CDO/vtable pages) — the header documents the
  rule; `targets_covered` exposes shortfalls.
- One scoped test-process hang occurred during iteration (ASan catching a
  genuine fixture OOB write; one stale process briefly remained in
  uninterruptible-exit state and was killed by PID; no unrelated process
  touched).
- Leak detection is unsupported by ASan on this macOS host (documented in
  the ASan log run notes); no leak claim is made either way.
- Divergence intentionally preserved: this add-on classifies coverage
  failures as UNAVAILABLE/Inconclusive (batch3 parity) while batch2 maps
  some of them to Refute — the controller still owes the one-policy call
  flagged by worker-batch2-live-prep.
