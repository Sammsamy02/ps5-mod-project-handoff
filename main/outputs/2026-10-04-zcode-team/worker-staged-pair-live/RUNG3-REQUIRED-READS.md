# RUNG3-REQUIRED-READS — exact live reads still needed beyond the validated GUA header

Worker: worker-staged-pair-live, 2026-10-04. The emitted pair
(`emission-live/`) contains ONLY what the validated GUA header establishes
(rung 1->2 subset: FUObjectArray + TUObjectArray). Everything rung-3
(UObjectBase/UStruct/UClass/FField/FProperty) is REFUSED by the assembler until
the reads below land as Validated predicates. Enumerated from
`vendored/worker-layout-batch3/bridge/width_contract.py` PREDICATE_REQUIREMENTS
+ the candidates profile offsets + the validated GUA field values.

## Anchors we already hold (validated, no further reads needed)

- GUA header at `0x97ba110` (`0x400000 + 0x93ba110`, single-load-bias,
  checkpoint `zcode_v3_MAPPING_BREAKTHROUGH`), layout A:
  ObjFirstGCIndex=35477, ObjLastNonGCIndex=35476,
  MaxObjectsNotConsideredByGC=35477, OpenForDisregardForGC=0,
  `Objects** = 0x13800220a0`, PreAllocated=NULL, MaxElements=2162688,
  NumElements=71959, MaxChunks=33, NumChunks=2.
- Derived: chunk capacity = MaxElements / MaxChunks = 2162688 / 33 = 65536
  (0x10000) items/chunk — exact, so the chunked form is internally consistent;
  1.5 MiB per chunk at FUObjectItem size 0x18 (65536 * 0x18 = 0x180000).

## R1 — TUObjectArray body (feeds nothing alone, prerequisite for R2)

1. `Objects* = [0x13800220a0]` (chunk-table base pointer). Validate: non-null,
   canonical, covered by a current trusted VMMAP span (same map-trust gating
   as the anchor probe; refusal recorded, never forced).
2. Chunk table entries `[chunk_table + 8*i]`, i in 0..NumChunks-1 (2 entries).
   Validate: non-null, pairwise distinct, ordered.

## R2 — FUObjectItem samples (predicate `uobject_item_chain` -> unlocks FUObjectItem, completes rung 2)

Sampled indices: 0, 1, 71958 (= NumElements-1), one mid index (e.g. 35477,
the GC boundary), and one index >= NumElements (free-list tail behavior).
Per sample read FUObjectItem (0x18): `Object*` @+0x0, Flags @+0x8,
SerialNumber @+0xC, ClusterRootIndex @+0x10 (exact field set = candidates
profile FUObjectItem entries / SET-5_06).
Predicate accounting: serial numbers index-linked, flags within vocabulary,
Object pointers canonical + inside mapped spans, re-read round-trip equality.

## R3 — UObjectBase through a live item (part of `ustruct_chain`)

From an R2 `Object*` U: vtable `[U]` (resolve against base `0x400000` —
corroborates, but does NOT substitute for, the Tick/SCO byte anchors),
ObjectFlags, InternalIndex, ClassPrivate (UClass*), NamePrivate, OuterPrivate.
Validate: ClassPrivate is itself findable in the same GUA population.

## R4 — UStruct chain from ClassPrivate (completes `ustruct_chain` -> unlocks UObjectBase, UStruct, UClass)

Read SuperStruct / Children / ChildProperties / PropertiesSize at the UStruct
candidate offsets; walk the SuperStruct chain to null termination recording
hop count (batch2 round-trip accounting discipline, N round-trips); walk the
Children UField::Next chain.

## R5 — FField variant settlement (`ffield_variant` -> unlocks FField, FProperty ONLY if variant == UE5_FField)

From the R4 ChildProperties head: determine FField-layout (NamePrivate/Next at
FField offsets, no UProperty header) vs legacy UProperty. The candidate table
is SET-5_06; a settled UE4_UProperty REFUSES the FField/FProperty section.
UE 5.6 expects UE5_FField, but that is an expectation, not a read.

## R6 — strengthening walks (record-only, never gates)

children_walk / childproperties_walk / uclass_links over the R4/R5 results
(UClass FuncMap/Interfaces/NetFields links). Manifested as
`strengthening_verdicts`; they widen nothing.

## Outcome after R1..R5 all Validated

Full 63-member / 8-section emission per EMISSION-RUNBOOK section 4
(FUObjectArray 10, TUObjectArray 7, FUObjectItem 6, UObjectBase 6, UStruct 8,
UClass 13, FField 3, FProperty 10; UFunction remains permanently refused —
classifier-only predicate). Members without compiled widths stay skipped
(e.g. UStruct::SuperStruct, UClass::ClassFlags/ClassCastFlags/
ReferenceSchema) regardless of predicate status.

## Read discipline (binding, from the anchor-validation corrections)

Every foreign read is gated by a CURRENT per-invocation sysctl KERN_PROC_VMMAP
collection, trust-gated; an unverifiable address yields a refusal record
(`map_untrusted` | `not_covered_current_map` | `covered_not_readable`) and
never a dereference. One invocation, per-record flush, no byte-replay of
retained-image expectations for these data reads (structural predicate
consumes the live bytes; the executable is never compared for data).
