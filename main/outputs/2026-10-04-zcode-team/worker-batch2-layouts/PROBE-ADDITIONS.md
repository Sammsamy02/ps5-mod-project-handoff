# PROBE-ADDITIONS.md — minimal additions that close the batch-2 blocks

Worker: worker-batch2-layouts, team 2026-10-04-zcode-team, 2026-10-04.
Read-this-with: BATCH2-STATUS.md. Everything here is a SPEC for a future
console probe run; nothing ran on target in this work. Existing prepared
assets are referenced, not rebuilt.

## 0. What already exists (no new work needed)

| Asset | Location | State |
|---|---|---|
| `probe_guobject_array` + corrected invariant | worker-5-layout-validator / worker-predicate-gc-fix | VALIDATED live at GUA 0x97ba110, layout A |
| `igu_probe_rung3` (header → Objects** → chunks → FUObjectItem → UObjectBase, gated `igu_read`) | worker-batch2-live-prep/src/live_gua_reads.{h,c} | host-proven 111/111, ASan/UBSan clean |
| `rc3_probe_chunks` + in-process vmmap re-walk (`rc3_vmmap_walk_min`, v3 variable-kve walk, trust gate) + `rc3b::run_ustruct_chain` bridge | worker-rung3-chunk-probe/src/ | host-proven 20/20 (156 asserts); live vmmap walk capability already demonstrated by igf5 mapscan (10,026 records, trusted=1) |
| `probe_ustruct_chain` / `probe_ffield_variant` / `probe_ustruct_child_variant` / `probe_ufunction` | worker-layout-batch2/src/reflection_layout_predicates.* | host-proven 43/43 |
| `probe_uobject_item_chain` reference semantics | worker-layout-minimum/validation/min_predicates.py:266 | host reference (serials index-linked, flags vocabulary, round-trip) |
| Assembler + width contract + ParsePair | worker-emit-final-pair/vendored/worker-layout-batch3/bridge + dryrun | ran live in this work (exit 0, byte-identical reproduction) |

The ONLY missing ingredient is target evidence: not one byte has been read at
or beyond `Objects** = 0x13800220a0` (grep over all controller console
records: zero hits), and that address's readability is undecided until an
in-process vmmap re-walk (`not_covered_current_map`, worker-batch2-live-prep §2).

## 1. One probe run (R1–R5) closes GATE A for all six batch-2 families

Single in-process PRX invocation (igf7-lineage, same 7-verbatim-import
packaging, no new imports), per-record flush, one invocation, no byte-replay
of retained-image expectations for data reads:

0. **Fresh map first.** `rc3_vmmap_walk_min` naming every target window below
   (GUA, Objects**, both chunk item windows, object/class/CDO/vtable pages).
   Trust gate `retained>=8 && insane*4<=retained`; on failure record
   `RC3_ST_NO_MAP` and stop — reads are never forced. KVME prot encoding;
   translate any POSIX-mode records via `igu_posix_mode_to_kvme()`.
1. **R1 — TUObjectArray body.** Read u64 @ `0x13800220a0` (Objects*) and
   chunk[0..1] = u64 @ Objects* + 8·i, i in 0..NumChunks-1 (=2). Predicate:
   non-null, canonical (`(p>>47)==0`), pairwise distinct, ordered; each covered
   by a READABLE span of the fresh map. (Feeds R2; alone unlocks nothing.)
2. **R2 — FUObjectItem samples → verdict `uobject_item_chain`.** Per sample
   index i in {0, 1, 35477 (GC boundary), 71958 (NumElements-1), 72000
   (≥NumElements free-list tail)} read the 0x18 item at
   `chunk[i>>16] + (i & 0xFFFF)·0x18`: Object* @+0x0 (w8), Flags @+0x8 (w4),
   ClusterRootIndex @+0xC (w4), SerialNumber @+0x10 (w4). Accounting
   (min_predicates.py:266 semantics + rc3 step-d): serial numbers index-linked,
   flags within vocabulary, Object pointers canonical + inside mapped spans,
   re-read round-trip equality. Null items are legal GC slots (recorded raw).
   Emit verdict record `{"probe":"uobject_item_chain","verdict":…,"codes":[…]}`.
   RECORDED DIVERGENCE: RUNG3-REQUIRED-READS.md R2 lists "SerialNumber @+0xC,
   ClusterRootIndex @+0x10", but the SET-5_06 candidates profile (what the
   assembler would emit: ClusterRootIndex=0xC, SerialNumber=0x10) and
   worker-batch2-live-prep's implemented reader both place ClusterRootIndex
   FIRST (0xC). Read per the profile/implementation; record both raw words so
   the conflict stays visible (same discipline as the NamePrivate/OuterPrivate
   prompt-vs-setter discrepancy).
3. **R3 — UObjectBase through a live item → part of `ustruct_chain`.** From an
   R2 Object\* U read the 0x28 window: vtable @0x0 (non-null, canonical,
   8-byte-readable), ObjectFlags u32 @0x8 (`& ~0x0FFFFFFF == 0`, RF_AllFlags),
   InternalIndex i32 @0xC (bounds + round-trip `item[InternalIndex].Object==U`
   through the 65536-chunk math, incl. cross-chunk), ClassPrivate @0x10
   (non-null readable AND itself findable in the GUA population), NamePrivate
   raw 8 B @0x18 (copied, never interpreted — P2), OuterPrivate @0x20
   (null-or-readable).
4. **R4 — UStruct chain from ClassPrivate → completes `ustruct_chain`.**
   `rc3b::run_ustruct_chain` (unchanged) publishes only READABLE spans to
   `rfl::probe_ustruct_chain`: SuperStruct-chain walk to null termination with
   visited-list acyclicity (≤64 hops), Children/ChildProperties
   readable-or-null, UClass extension (CDO@0x110 null-or-readable) →
   `first_ustruct_like` / `first_uclass_like` anchors + `STR_I_UCLASS_EXT_OK`.
5. **R5 — FField variant settlement → verdict `ffield_variant`.** THE ONE
   GENUINELY NEW BRIDGE STEP (small, host-testable): the rc3 bridge currently
   stops after `probe_ustruct_chain`. Add: feed `first_uclass_like`'s
   ChildProperties head (UStruct+0x50) into `rfl::probe_ffield_variant` (and
   `probe_ustruct_child_variant` for the head cells) in the SAME snapshot.
   Validated requires exactly one surviving hypothesis; record
   `{"probe":"ffield_variant","verdict":…,"variant":"UE5_FField"|"UE4_UProperty"}`.
   A settled UE4_UProperty REFUSES the SET-5_06 FField/FProperty sections at
   the assembler (by design).
6. **Emission.** Successor verdicts file = the real
   `verdicts-live-gua.json` + the new records (same schema, same digest
   discipline). Re-run the UNMODIFIED assembler full-scope (command identical
   to `emission-batch2-attempt`, new verdicts + out-dir). Expected result:
   exactly 8 sections / 63 keys (55 widthed members + 8 UEP_TotalSize keys —
   the 63 figure already excludes the 8 BLOCKED-NO-WIDTH members, which stay
   in `skipped_no_width` per BATCH2-STATUS §3), UFunction refused. Verify
   with the frozen `parser_verify` + the same negative controls.

**Policy decision owed before the run (controller):** batch2 maps an
unreadable ClassPrivate to refute-class `OBJ_R_CLASSPTR_BAD`; igu/rc3 classify
coverage failures UNAVAILABLE/Inconclusive. Pick one policy explicitly
(worker-batch2-live-prep §6 divergence note); recommended: batch3 parity
(refute only read-and-contradicted), since `not_covered_current_map` is
evidence-unavailable, not contradiction.

**Image-prot hazard:** under strict KVME semantics the image span
0x400000–0x787c000 is recorded X-only (prot 4) → vtable reads classify
`covered_not_readable` (Inconclusive, degrading R3). igf4 hit exactly this;
igf5's mprotect-first lineage is the demonstrated workaround; alternatively
re-arbitrate image prot semantics BEFORE the run. Do not silently special-case.

## 2. GATE B closures (widths experiments, NOT probes)

Probes can never create contract widths. Per WIDTH-GATES.md, the widths-2
precedent (one focused actual-SDK control, constants read from an owned
control object, exactly one canonical key added) is the closing mechanism:

1. **FProperty.PropertyFlags @0x38** — CLOSEABLE, highest value: getters exist
   and are fully typed (`GetPropertyFlagsBase()` → uint64&); expected
   constant 8 (widths-3 candidate).
2. **FField.Owner @0x10 / Next @0x18 / NamePrivate @0x20** — CLOSEABLE the
   same way by reproducing the private type expressions locally (widths-2 did
   exactly this for the private `UFunction::Func` getters, 303→304 keys).
   `Next` is the rung-3 property-read blocker (`UStruct::FindProperty` walks
   it); `NamePrivate` matters only as raw copy (FName stays P2).
3. **UStruct.SuperStruct @0x40** — contract explicitly null (delta re-asserts
   it); same experiment shape. Without it the UStruct section stays 7/8.
4. **UClass.ClassFlags @0xD4 / ClassCastFlags @0xD8 / ReferenceSchema @0x1E8**
   — same experiment shape; lowest priority (no rung-3 dependency).

Secondary, evidence-only (never creates a width): bit-plausibility narrowing
across many predicate-validated anchors per WIDTH-GATES §1.

## 3. Order of operations

1. Controller settles the one-policy call + image-prot approach.
2. Implement the R5 bridge step + verdict serialization (small C++ delta on
   worker-rung3-chunk-probe; host-test parity 20/20 → extend suite).
3. One console run executes steps 0–5, flushing per-record.
4. Assembler re-run + ParsePair + negative controls → successor pair.
5. Widths-3 experiments run OFF TARGET in parallel; each landed key widens
   the successor pair's next regeneration (assembler re-run only — no probe
   dependency between GATE A and GATE B closures).
