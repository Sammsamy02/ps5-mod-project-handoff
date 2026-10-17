# WIDTH-GATES.md — the width-missing entries, what the port tree says, and what could be validated in-process

Team 2026-10-04-zcode-team / worker-layout-batch3. Everything below is an
EXPERIMENT PLAN, not a default: no width is assumed, and nothing here changes
the staging contract — a member becomes stageable only when a compiled
accessor width lands in the frozen contract
(`ue4ss-native-layout-widths-1/coverage.json` + `ue4ss-native-layout-widths-2/
coverage-delta.json`), produced the way widths-1/widths-2 produced theirs
(actual SDK compile of focused controls, constants read from the owned control
object, no game/native execution).

## How to read the gate

The widths-1 helper computes a per-key width from the port's generated getter
bodies: `ValueWidth<T>()` returns `sizeof(remove_cvref_t<T>)` for COMPLETE
types and `nullopt` otherwise, and the conservative policy takes the max over
alternatives but yields null when ANY alternative is null
(`ue4ss-native-layout-widths-1/include/NativeLayoutWidths.hpp:11-20,23-27`).
Therefore every "no compiled width" row in PROMOTABLE-SET.md is one of:
(a) the getter's value type was incomplete in the helper's TU, (b) the getter
was not recognized/callable there, or (c) the policy refused because not all
engine-version alternatives were complete. The closing mechanism is the
widths-2 precedent: reproduce the private type expressions locally, compile a
focused control with the actual SDK, read the constants, add exactly one
source-proved canonical key (widths-2 did this for `UFunction.Func`, 303→304).

## Gate-by-gate analysis (from the port tree, cited)

### 1. FProperty.PropertyFlags @0x38 — CLOSEABLE (highest value)
- Getters EXIST, fully typed: `uint64& FProperty::GetPropertyFlagsBase()` and
  `EPropertyFlags& FProperty::GetPropertyFlags420()`
  (`ue4ss-unreal-native-build-1/port/unreal/generated_include/
  MemberVariableLayout_SrcWrapper_FProperty.hpp:42-68`).
- `enum EPropertyFlags : uint64_t` — complete underlying type
  (`port/unreal/include/Unreal/UnrealFlags.hpp:106`).
- Experiment (widths-3 candidate): focused control reproducing
  `GetPropertyFlagsBase()` over the wrapper exactly as widths-2 reproduced the
  Func getters; expected constant 8 (uint64&). Why widths-1 refused is not
  visible from coverage.json alone (its per-key accessor mapping is generated);
  the focused control bypasses that question by compiling the genuine type
  expression.
- Bit-plausibility narrowing (secondary, in-process, evidence-only): under the
  settled UE5 variant, read candidate widths {8,4} at offset 0x38 across many
  predicate-validated FProperty anchors; if bytes 4..7 are uniformly zero AND
  the low 32 bits never exceed the CPF mask, the u64-vs-u32 question is
  value-irrelevant for staging decisions. This NARROWS candidates; it never
  creates the contract width.

### 2. UFunction.FunctionFlags @0xB0 — CLOSEABLE (rung-5 value, currently also predicate-blocked)
- Getters exist: `uint32& UFunction::GetFunctionFlagsBase()` /
  `EFunctionFlags& GetFunctionFlags417()`
  (`MemberVariableLayout_SrcWrapper_UFunction.hpp:4-30`);
  `enum EFunctionFlags : uint32_t` (`UnrealFlags.hpp:343`).
- Independent structural bound already in evidence: batch2's classifier reads
  it as u32 because compiled-width NumParms@0xB4 (w=1) bounds it; the
  batch3-promoted staging gate ignores it either way until UFunction has a
  Validated-class predicate (it never will under the classifier-only
  discipline). So this width experiment is only worth doing AFTER (or
  together with) a rung-5 decision that changes the UFunction predicate
  policy — documented here so the ordering is explicit.

### 3. FField::Owner @0x10 / FField::Next @0x18 — CLOSEABLE
- Getters exist: `FFieldVariant& FField::GetOwner()`, `FField*& GetNext()`
  (`MemberVariableLayout_SrcWrapper_FField.hpp:23-55`).
- `FFieldVariant` is FULLY DEFINED in the port
  (`port/unreal/include/Unreal/FField.hpp:37`, with constructors and an
  explicit `sizeof(FFieldVariant::Container)` at :159); `FField*&` is a
  pointer. Both value types are complete in the port include graph, so a
  widths-3 control should yield 8/8. Today both members stay
  BLOCKED_NO_WIDTH (PROMOTABLE-SET.md), and batch3's childproperties walk
  validates their OFFSETS without staging them.

### 4. FField::NamePrivate @0x20 and FName storage width ("FName 0/0")
- "FName 0/0" in MINIMUM-SET means: class FName has ZERO canonical entries in
  the binding contract — it is not a staged layout class at all. It does NOT
  mean FName's size is unknowable:
  - `FName` is fully defined in the port
    (`port/unreal/include/Unreal/NameTypes.hpp:215-263`: FNameEntryId
    ComparisonIndex + uint32 Number; the WITH_CASE_PRESERVING_NAME variant
    pads differently — exactly why a compiled per-member width, not a
    header reading, is the contract).
  - Compiled STORAGE-width evidence already exists elsewhere in the frozen
    contract: `UObjectBase.NamePrivate` = 8 and `FProperty.RepNotifyFunc` = 8
    (widths-1 coverage), both `FName&` getters.
  - The refused key `FField.NamePrivate` has an existing getter
    (`FName& FField::GetNamePrivate()`,
    SrcWrapper_FField.hpp:61-78) — its refusal is a helper/TU artifact of the
    same class as (1)/(3).
- Experiment: widths-3 control reproducing `GetNamePrivate()` with the port's
  NameTypes.hpp in the TU; the produced constant must agree with the already
  compiled 8 for UObjectBase.NamePrivate. If it disagreed, that disagreement
  itself would be a finding (engine-version axis), not something to paper
  over.
- P2 rule unchanged: even with a width, NamePrivate content is never
  interpreted (FNamePool layout unresolved); width only enables raw staging.

### 5. UStruct.SuperStruct @0x40 — the critical transcription correction
- The frozen contract REFUSES it: widths-2 `coverage-delta.json` records
  `"UStruct.SuperStruct": null` and `source-facts.json` explains:
  `GetSuperStruct507` returns a complete `TObjectPtr<UStruct>` (compiled 8)
  but `GetSuperStructBase` uses `ObjectPtr_Private::TNonAccessTrackedObjectPtr`
  which is forward-only in the retained Class.hpp; the conservative policy
  requires ALL alternatives, so the canonical entry is refused.
- The worker-layout-minimum candidates profile transcribed width 8 for it —
  an error this worker's bridge corrects: `SuperStruct` is recorded
  skipped_no_width in the dry-run emission even though batch2's chain
  predicate validates the offset (readable-or-null + acyclicity) and batch3
  depends on it. PROMOTABLE-SET.md marks it BLOCKED_NO_WIDTH.
- Experiment (policy-level, NOT a default): a widths-3 delta that documents a
  modern-only alternative set (as widths-2 did for Func's two private-type
  fallbacks) would close it; alternatively an authorized policy change to
  per-alternative reporting. Both are source-proved-delta work in the widths
  campaign's style; neither may be simulated by editing the contract by hand.

### 6. UClass::ClassFlags / ClassCastFlags / ReferenceSchema
- `uint32& UClass::GetClassFlagsBase()` / `EClassFlags& GetClassFlags417()`
  exist (`MemberVariableLayout_SrcWrapper_UClass.hpp:42-68`) — same closable
  pattern as (1). ClassCastFlags/ReferenceSchema getters follow the same
  wrapper shape (same file); the widths-3 control approach applies
  identically. Until then: skipped_no_width in every emission (proven in the
  dry-run manifest).

## What is NOT closeable from the port tree today
- Nothing observed suggests a permanently-unclosable width among the staged
  families: every blocked member above has an existing, type-complete or
  type-completable getter in the port tree. The genuinely open objects remain
  the FName POOL (FNameEntry/FNamePool layout — unrelated to member staging)
  and container internals (FuncMap/TMap etc. — worker-6 scope), which are
  predicated/interpreted gaps, not width gaps.

## Sequencing recommendation (evidence-driven, no schedule)
1. widths-3a: PropertyFlags + FField Owner/Next/NamePrivate + UClass
   ClassFlags/ClassCastFlags/ReferenceSchema (one focused-control campaign,
   widths-2 style) — directly converts 8 BLOCKED_NO_WIDTH rows into
   STAGEABLE_TODAY without any new predicates.
2. Only then consider the SuperStruct policy delta (5) and rung-5 UFunction
   work (2), each requiring its own authorized source-proved change.
