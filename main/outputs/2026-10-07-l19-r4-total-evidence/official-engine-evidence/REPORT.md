The new addendum proposes exact native totals **FField `0x30` and FFieldClass `0x40`**, with confidence **high, validated-static source-contract-correlated inference**. These are pending independent semantic review before profile promotion. Neither is a fresh console/live sizeof measurement. FField's result is a measured native return constant interpreted by a newly available, independently corroborated source contract. FFieldClass's result joins a complete original declaration, every observed native constructor field, and an actual PS5 compiler layout control.

This addendum preserves the earlier unknown/prefix receipt byte-for-byte. The new inputs are licensed official Epic source files supplied locally by the parent, pinned to `5.6.1-release` commit `6978b63c8951e57d97048d8424a0bebd637dde1d`. All ten file SHA256 and Git blob SHA1 values were independently verified. No network/account/console contact occurred in this agent.

FField's exact-image constructor installs root vptr `7EDC650`. Its unique actual DT_RELA record at file `9380058` assigns slot zero to `3701A0`, whose instruction `3701A4` (`b830000000`) returns zero-extended `0x30`. The original Field.h now supplies the missing semantic link: GetFieldSize is the first declared virtual method, with body `return sizeof(FField)`; DECLARE_FIELD_API overrides it with `return sizeof(TClass)`.

The method identification uses independent topology anchors. Native root slot `+8` selects empty complete destructor `B290`, `+10` selects deleting thunk `26030`, `+18` selects routine `364710` serializing Name at `+20` and Flags at `+28` with width four, and `+20` selects `364700`, which calls receiver vtable `+48`. Official FField::PostLoad calls Bind; official virtual order places Bind at `+48`, where native root selects an empty body. All 13 root entries through `+60` match the official declaration order, including the two destructor entries. Five named property subtype slot-zero constants also independently match their proved constructor allocation totals. This provides a specific method-family correlation, beyond an untyped constant or the API documentation signature alone.

FFieldClass is a complete non-polymorphic class in the original source, with no base class or explicit layout attribute and no conditional data members. Its destructor is non-virtual. The original UE_NONCOPYABLE macro expands to four deleted special-member methods and adds no storage, virtuals, or packing. The complete-class audit identifies exactly these eight data members and no others:

| Original member | Native offset / width | Native store |
|---|---|---|
| Name | `0 / 8` | `3639EF`, `488903` |
| Id | `8 / 8` | `3639B7` |
| CastFlags | `10 / 8` | `3639BB` |
| ClassFlags | `18 / 4` | `3639BF` |
| SuperClass | `20 / 8` | `3639C6` |
| DefaultObject | `28 / 8` | `3639CA` |
| ConstructFn | `30 / 8` | `3639D2` |
| UnqiueNameIndexCounter | `38 / 4` | `3639D6`, `c7473800000000` |

Offsets and widths above are hexadecimal. The validator derives all eight from exact native store operands rather than assuming PC offsets. It also checks four FField constructor member stores and its owner read at `+10`. The complete original FName, FNameEntryId, and FThreadSafeCounter declarations were audited: their only data are the conditional name indices/number, uint32 id, and volatile int32 counter, respectively; none has a base, virtual, or explicit layout attribute.

The compiler controls use **literal byte-extracted declarations and dependencies** from those originals, including the real enum bodies, generic platform type aliases, pointer-selector template, tagged FFieldVariant union, noncopyable macro, first virtual size-query body, virtual destructor, and conditional metadata pointer. Non-layout methods/friends/static members are omitted; private data are publicized only for offsetof access. The input spans and transformations are recorded in EXTRACTION.json. This is an original-source layout projection, not a full Unreal translation unit or a hand-authored mirror of desired native offsets. The saved compiler reports target `x86_64-sie-ps5`; objects are x86_64 ELF, and their 25-value arrays and actual direct size-query machine bodies were independently decoded.

| Compile control | FField total | FFieldClass total | Native compatibility |
|---|---:|---:|---|
| Natural, noncase inline Name | `30` | `40` | all checked physical constraints match |
| Pack four | `2C` | `38` | Super `1C`, Default `24`, Construct `2C`, Counter `34`; four native offsets contradict |
| Metadata enabled | `38` | `40` | native root size-query `30` contradicts |
| Case-preserving inline Name | `30` | `48` | Name becomes `C` bytes; Id `10` and FField Flags `2C` contradict |
| Noncase outlined Name | `28` | `40` | Name becomes four bytes; Flags `24` and root query contradict |
| Case-preserving outlined Name | `30` | `40` | all checked physical constraints match |

Totals and offsets in the table are hexadecimal. The two configurations matching every checked physical constraint both produce `FField=30` and `FFieldClass=40`. No uniqueness claim about name flags follows from this table; the earlier independent FName configuration evidence remains separate. Pack-four rejection is based on four **constructor offset contradictions**, not an assumed alignment from qword loads/stores. The FFieldClass conclusion uses the complete source declaration contract and native geometry matched by the actual target compiler, rather than rounding its last member extent. The descriptor/guard `40` gap remains corroboration only and is not the decisive upper bound.

The physical FField extra pointer is controlled by **WITH_METADATA**, not WITH_EDITORONLY_DATA. Native complete destructor is empty and native Serialize has only Name/Flags behavior, matching metadata-disabled original bodies. The independently measured native size query also rejects the metadata-enabled declaration's `38`. WITH_EDITORONLY_DATA itself is not proved by this receipt; in Field.h it adds methods/delegate declarations and does not change the projected physical data. The saved shipping label is not substituted for a direct editor-flag measurement.

The important qualification is source identity: the exact game engine commit is not proved identical to Epic 5.6.1. The correlation is supported by named registration, constructor parameter and field geometry, every class data field, root virtual order and behaviors, five independent subtype allocations, and scalar/tagged-owner geometry. The full proprietary platform preset headers/config were not reproduced. A native per-class packing/alignment override or added fields outside the correlated original declaration would invalidate this inference. No such effect appears in the pinned class declarations or audited macros, and deliberate packing/name/metadata controls expose the relevant observable contradictions. The facts therefore retain the stated source-contract-correlated confidence; they are not labeled byte-direct native allocation proofs.

The receipt uses a `facts` array with keys `FField.UEP_TotalSize` and `FFieldClass.UEP_TotalSize`, hexadecimal string values, `width_bytes: null`, `status: validated-static`, `live_validation: false`, confidence and explicit inference chains. Top-level `status: review_pending` preserves independent-review gating. Downstream promotion must pin the immutable fact/audit identities rather than changing this status in place.

Final validation passed 134 single-instruction exact-byte/PT_LOAD/decode windows, 13 unique actual root relocation records, six size-query family records with five independently allocated subtypes (18 distinct relocation targets), ten official file SHA256/Git blobs, fourteen literal source segments, four complete class declarations, twelve independently derived constructor offset/width pairs, and six saved PS5 object arrays/direct method bodies. Two physical configurations match; four contradictory controls are rejected. Native byte/address corruption and packed counter-offset controls also reject. Zero failures. All earlier bounded R4 frozen files remain unchanged. No engine linking/execution, runtime integration, map publication, checkpoint edit, or menu/spawn readiness claim occurred.

To reproduce, run the new extract/audit scripts, the six exact compiler commands in COMPILE-COMMANDS-FINAL.json with its LLVM_CONFIG value, then:

```sh
python3 outputs/2026-10-07-l19-r4-total-evidence/official-engine-evidence/collect_compiled_controls.py
python3 outputs/2026-10-07-l19-r4-total-evidence/official-engine-evidence/build_totals_receipt.py
python3 outputs/2026-10-07-l19-r4-total-evidence/official-engine-evidence/validate_totals.py
```

Initial projection failures and the successful intermediate projection are preserved separately. The final objects and final commands use the sourced noncopyable macro. All generated work belongs to this new addendum; original engine files and frozen earlier inputs are read-only.
