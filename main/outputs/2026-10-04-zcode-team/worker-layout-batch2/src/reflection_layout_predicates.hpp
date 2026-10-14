// reflection_layout_predicates.hpp
//
// REFLECTION-FAMILY LAYOUT VALIDATION PREDICATES (rung 3 of the acceptance
// ladder; rung 5 UFunction classifier included as Inconclusive-only).
// Team: 2026-10-04-zcode-team / worker-layout-batch2 (ZCode).
//
// This file EXTENDS worker-5-layout-validator's `tlp` predicate set with the
// families worker-layout-minimum/MINIMUM-SET.md line 166-169 lists as lacking
// predicates: UStruct/UClass (chain), FField/FProperty (variant
// discriminator), UFunction (classifier). Same conventions: pure functions,
// no OS calls, no allocation, no exceptions, no console/network; every memory
// access flows through caller-supplied tlp::MemSpan views so the same code
// runs in host tests (synthetic images) and later inside the game module.
// The sibling's files are included read-only and NEVER modified.
//
// PROVENANCE OF EVERY CONSTANT (nothing guessed; offsets are the
// worker-layout-minimum candidates' cited origins, engine 5.6 per
// validated-static identity in worker-2's facts profile):
//   SET-5_06 = .superpowers/native-dependency-spike/native-coherent-runtime-
//     1/source/port/unreal/generated_include/FunctionBodies/
//     5_06_MemberVariableLayout_DefaultSetter_<Class>.cpp
//       UObjectBase: ObjectFlags=0x8, InternalIndex_Private=0xC,
//         ClassPrivate=0x10, NamePrivate=0x18, OuterPrivate=0x20,
//         UEP_TotalSize=0x28.
//       UField: Next=0x28, UEP_TotalSize=0x30 (UE4 4_07 identical).
//       UStruct: SuperStruct=0x40, Children=0x48, ChildProperties=0x50,
//         PropertiesSize=0x58, MinAlignment=0x5C, UEP_TotalSize=0xB0.
//       UClass: ClassDefaultObject=0x110, SparseClassData=0x118,
//         SparseClassDataStruct=0x120, FuncMap=0x128,
//         NativeFunctionLookupTable=0x1F0, UEP_TotalSize=0x200.
//       UFunction: FunctionFlags=0xB0, NumParms=0xB4, ParmsSize=0xB6,
//         ReturnValueOffset=0xB8, RPCId=0xBA, RPCResponseId=0xBC,
//         FirstPropertyToInit=0xC0, EventGraphFunction=0xC8,
//         EventGraphCallOffset=0xD0, Func=0xD8, UEP_TotalSize=0xE0.
//       FField: ClassPrivate=0x8, Owner=0x10, Next=0x18, NamePrivate=0x20,
//         FlagsPrivate=0x28, UEP_TotalSize=0x30. (No 4_xx FField setter
//       exists in the port tree: FField does not exist in UE4 — itself
//       discriminator evidence.)
//       FProperty (5_06): ArrayDim=0x30, ElementSize=0x34, PropertyFlags=0x38,
//         RepIndex=0x40, Offset_Internal=0x44, PropertyLinkNext=0x48,
//         NextRef=0x50, DestructorLinkNext=0x58, PostConstructLinkNext=0x60,
//         RepNotifyFunc=0x68, UEP_TotalSize=0x70.
//   SET-4_07 = same directory, 4_07_MemberVariableLayout_DefaultSetter_*.cpp
//     (adversarial UE4 alternative shapes used by the variant discriminator):
//       UStruct(4_07): SuperStruct=0x30, Children=0x38, PropertiesSize=0x40,
//         MinAlignment=0x58, UEP_TotalSize=0x90 — NO ChildProperties member.
//       FProperty(4_07, UProperty-shaped, UObjectBase base):
//         RepNotifyFunc=0x44, Offset_Internal=0x4C, PropertyLinkNext=0x50,
//         NextRef=0x58, DestructorLinkNext=0x60, PostConstructLinkNext=0x68,
//         UEP_TotalSize=0x70. ArrayDim/ElementSize/PropertyFlags/RepIndex
//         coincide with 5_06 at 0x30/0x34/0x38/0x40.
//       UObjectBase(4_07): same five members at 0x8..0x20 (verified this
//         session by grep of the 4_07 setter).
//   FUObjectItem (MINIMUM-SET.md rung 2, SET-5_06 FUObjectItem):
//     Object=0x0, Flags=0x8, ClusterRootIndex=0xC, SerialNumber=0x10,
//     stride UEP_TotalSize=0x18.
//   NumElementsPerChunk = 64*1024 and MaxChunks bound 512:
//     UOA-HPP .../include/Unreal/UObjectArray.hpp:88 + generated member
//     wrapper Chunks_Type = UObjectBase**[512] (same citations as worker-5).
//   RF_AllFlags = 0x0FFFFFFF: UnrealFlags.hpp:95 (EObjectFlags mask;
//     ObjectFlags "sane" = no bits outside RF_AllFlags).
//   EFuncFlags: FUNC_AllFlags = 0xFFFFFFFF at UnrealFlags.hpp:381, and bits
//     0x10/0x20 are explicitly commented-out holes in the enum body
//     (UnrealFlags.hpp ~lines 333-381) — a FunctionFlags value whose set
//     bits are a subset of {0x10,0x20} is mask-implausible.
//   WIDTHS = .superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/
//     coverage.json: FName has ZERO compiled accessor widths (0/0) and
//     FProperty::PropertyFlags / UFunction::FunctionFlags have NO compiled
//     width. Consequences enforced below:
//       * NamePrivate (UObjectBase+0x18, FField+0x20) and RepNotifyFunc
//         (FProperty+0x68) are copied as raw bytes and NEVER interpreted
//         (P2 rule; kFNamePoolPredicateExcluded in worker-5's header).
//       * FunctionFlags is read as u32 only because the compiled-width
//         neighbor NumParms@0xB4 (width 1) structurally bounds it to 4 bytes;
//         the read stays an Inconclusive-classifier input, never a staged
//         member.
//       * Func pointer width alternatives [16,8] (MINIMUM-SET rung 5): the
//         pointer value is the low 8 bytes under BOTH widths, so the
//         readability/canonicality check below is width-agnostic.
//
// VERDICT DISCIPLINE (identical to worker-5's):
//   Validated    = the family's whole structural predicate chain passed
//                  (for the FField variant discriminator: exactly one of the
//                  two variant hypotheses passed and the other failed a hard
//                  structural check). Elevates candidate -> validated FOR THE
//                  MAPPED SNAPSHOT ONLY.
//   Refuted      = a cited layout forbids the bytes (non-canonical/dangling
//                  pointer, flag bits outside the cited mask, index out of
//                  bounds or failing round-trip, SuperStruct cycle).
//   Inconclusive = evidence unavailable (unreadable window, walk truncated at
//                  the 64-object cap, discriminator ambiguity). NEVER treated
//                  as failure or success.
//   UFunction probe is INCONCLUSIVE-CLASSIFIER ONLY by design (mirrors
//   worker-5's code-shape probes): it can never return Validated (no
//   callable-ABI claim; FUNC_Native etc. is not asserted) and never Refuted
//   (shape mismatch is recorded as negative evidence for the controller).
//
// INPUTS the predicates consume (never invented): worker-1 live spans, a
// ValidatedGuaView assembled from a Validated worker-5 probe_guobject_array
// result's `observed` fields, and caller-supplied object addresses. No
// hardcoded vaddr, no guessed offset anywhere.

#ifndef REFLECTION_LAYOUT_PREDICATES_HPP
#define REFLECTION_LAYOUT_PREDICATES_HPP

#include "../../worker-5-layout-validator/src/townfall_layout_probes.hpp"

namespace rfl {  // reflection-family layout predicates (batch 2)

using tlp::MemSpan;
using tlp::Verdict;

// ---------------------------------------------------------------------------
// Rationale codes (stable, machine-readable; distinct from tlp::RCode)
// ---------------------------------------------------------------------------

enum class RCode : uint16_t {
    OK = 0,

    // Object-walk family — Inconclusive-class (0x50xx)
    OBJ_E_GUA_VIEW_INVALID        = 0x5001, // null Objects / Num<=0 / chunks<=0
    OBJ_E_WALK_TRUNCATED          = 0x5002, // 64-object cap hit, no struct found
    OBJ_E_NO_CANDIDATES           = 0x5003, // zero readable non-null items
    OBJ_E_ITEM_UNREADABLE         = 0x5004, // an item's window was unreadable

    // Object-walk family — Refute-class (0x51xx), per-object contradictions
    OBJ_R_VPTR_NONCANONICAL       = 0x5101, // vtable null or non-canonical
    OBJ_R_VPTR_UNREADABLE         = 0x5102, // non-null vptr outside all spans
    OBJ_R_FLAGS_MASK              = 0x5103, // ObjectFlags & ~RF_AllFlags != 0
    OBJ_R_INDEX_BOUNDS            = 0x5104, // InternalIndex <0 or >= NumElements
    OBJ_R_INDEX_ROUNDTRIP         = 0x5105, // index maps to a different Object
    OBJ_R_OUTER_BAD               = 0x5106, // OuterPrivate non-null bad pointer
    OBJ_R_CLASSPTR_BAD            = 0x5107, // ClassPrivate null/bad/unreadable

    // UStruct/UClass chain — Refute + Inconclusive (0x52xx)
    STR_R_SUPER_CYCLE             = 0x5201, // SuperStruct chain revisits a node
    STR_R_SUPER_BAD               = 0x5202, // non-null super non-canonical/unreadable
    STR_R_CHILDREN_BAD            = 0x5203, // Children non-null bad pointer
    STR_R_CHILDPROPS_BAD          = 0x5204, // ChildProperties non-null bad pointer
    STR_E_STRUCT_WINDOW_UNREADABLE= 0x5205, // could not read [obj, obj+0x60)
    STR_E_NONE_FOUND              = 0x5206, // examined candidates, none struct-like
    STR_E_UCLASS_WINDOW_UNREADABLE= 0x5207, // UClass tail window unreadable
    STR_I_UCLASS_EXT_OK           = 0x5208, // info: UClass extension passed too

    // UFunction classifier — Inconclusive-class ONLY (0x53xx)
    FN_E_UNREADABLE               = 0x5301, // [f, f+0xE0) window not readable
    FN_I_FUNC_READABLE            = 0x5302, // support: Func ptr canonical+readable
    FN_I_FUNC_NULL                = 0x5303, // support-less: Func ptr null
    FN_N_FUNC_BAD                 = 0x5304, // negative: Func ptr non-canonical/unreadable
    FN_I_FLAGS_IN_MASK            = 0x5305, // support: flags use only defined bits
    FN_N_FLAGS_UNDEFINED_ONLY     = 0x5306, // negative: set bits subset of {0x10,0x20}
    FN_I_PARM_CONSISTENT          = 0x5307, // support: NumParms/ParmsSize/RVO coherent
    FN_N_PARM_IMPLAUSIBLE         = 0x5308, // negative: NumParms==0&&ParmsSize!=0 or RVO>ParmsSize
    FN_I_LINKPTRS_OK              = 0x5309, // support: FPTI/EventGraphFunction sane
    FN_N_LINKPTR_BAD              = 0x530A, // negative: link ptrs non-canonical/unreadable

    // FField variant discriminator (0x54xx)
    FLD_E_ANCHOR_UNREADABLE       = 0x5401, // anchor window bytes unavailable
    FLD_H5_R_CLASS_NULL           = 0x5402, // UE5: ClassPrivate null at +0x8
    FLD_H5_R_CLASS_BAD            = 0x5403, // UE5: ClassPrivate non-canonical/unreadable
    FLD_H5_R_OWNER_BAD            = 0x5404, // UE5: Owner non-null bad pointer
    FLD_H5_R_NEXT_BAD             = 0x5405, // UE5: Next non-null bad pointer
    FLD_H5_R_ARRAYDIM_ZERO        = 0x5406, // UE5: ArrayDim==0 (classifier refute)
    FLD_H5_R_LINK_BAD             = 0x5407, // UE5: payload link ptr bad at 0x48..0x60
    FLD_H4_R_VPTR_BAD             = 0x5408, // UE4: vtable null/bad at +0x0
    FLD_H4_R_FLAGS_MASK           = 0x5409, // UE4: ObjectFlags outside RF_AllFlags
    FLD_H4_R_INDEX                = 0x540A, // UE4: index bounds or round-trip fail
    FLD_H4_R_CLASS_BAD            = 0x540B, // UE4: ClassPrivate@0x10 bad
    FLD_H4_R_OUTER_BAD            = 0x540C, // UE4: OuterPrivate@0x20 bad
    FLD_H4_R_LINK_BAD             = 0x540D, // UE4: payload link ptr bad at 0x50..0x68
    FLD_E_AMBIGUOUS               = 0x540E, // both hypotheses pass all hard checks
    FLD_E_NO_GUA_EVIDENCE         = 0x540F, // H4 round-trip unavailable (no view)
    FLD_I_H4_ROUNDTRIP_MISS       = 0x5410, // support UE5: bounded scan w/o anchor
    FLD_I_H4_ROUNDTRIP_HIT        = 0x5411, // support UE4: anchor IS a tracked object
};

const char* rcode_string(RCode c);

// ---------------------------------------------------------------------------
// Cited structure constants (see header provenance block; hex form matches
// the port setter templates exactly)
// ---------------------------------------------------------------------------

// SET-5_06 UObjectBase
inline constexpr size_t kObjObjectFlags = 0x8;    // uint32 (RF_AllFlags mask)
inline constexpr size_t kObjInternalIndex = 0xC;  // uint32
inline constexpr size_t kObjClassPrivate = 0x10;  // pointer
inline constexpr size_t kObjNamePrivate = 0x18;   // FName — RAW ONLY (P2)
inline constexpr size_t kObjOuterPrivate = 0x20;  // pointer
inline constexpr size_t kObjTotalSize = 0x28;

// FUObjectItem (MINIMUM-SET rung 2, SET-5_06)
inline constexpr size_t kItemObject = 0x0;
inline constexpr size_t kItemStride = 0x18;

// SET-5_06 UStruct / UClass
inline constexpr size_t kUStructSuperStruct = 0x40;
inline constexpr size_t kUStructChildren = 0x48;
inline constexpr size_t kUStructChildProperties = 0x50;
inline constexpr size_t kUStructPropertiesSize = 0x58;  // uint32
inline constexpr size_t kUStructMinAlignment = 0x5C;    // uint32
inline constexpr size_t kUStructTotalSize = 0xB0;
inline constexpr size_t kUClassClassDefaultObject = 0x110;      // pointer
inline constexpr size_t kUClassSparseClassData = 0x118;         // pointer
inline constexpr size_t kUClassSparseClassDataStruct = 0x120;   // pointer
inline constexpr size_t kUClassFuncMap = 0x128;         // container (raw u32)
inline constexpr size_t kUClassNativeFuncLookup = 0x1F0; // container (raw u32)
inline constexpr size_t kUClassTotalSize = 0x200;

// SET-5_06 UFunction
inline constexpr size_t kUFunctionFlags = 0xB0;   // u32-bounded (see header)
inline constexpr size_t kUFunctionNumParms = 0xB4; // uint8
inline constexpr size_t kUFunctionParmsSize = 0xB6;// uint16
inline constexpr size_t kUFunctionRetValOffset = 0xB8; // uint16
inline constexpr size_t kUFunctionFirstPropToInit = 0xC0;   // pointer
inline constexpr size_t kUFunctionEventGraphFn = 0xC8;      // pointer
inline constexpr size_t kUFunctionFunc = 0xD8;      // pointer, width [16,8]
inline constexpr size_t kUFunctionTotalSize = 0xE0;

// SET-5_06 FField / FProperty, and SET-4_07 UE4 alternatives
inline constexpr size_t kFFieldClassPrivate = 0x8;
inline constexpr size_t kFFieldOwner = 0x10;
inline constexpr size_t kFFieldNext = 0x18;
inline constexpr size_t kFFieldNamePrivate = 0x20;  // FName — RAW ONLY (P2)
inline constexpr size_t kFFieldFlagsPrivate = 0x28; // uint32 (no cited mask)
inline constexpr size_t kFFieldTotalSize = 0x30;
inline constexpr size_t kFPropArrayDim = 0x30;      // uint32
inline constexpr size_t kFPropElementSize = 0x34;   // uint32
inline constexpr size_t kFPropRepIndex = 0x40;      // uint16
inline constexpr size_t kFPropRepNotifyUe5 = 0x68;  // FName — RAW ONLY (P2)
// UE5 link pointers
inline constexpr size_t kFPropLinkNextUe5 = 0x48;
inline constexpr size_t kFPropNextRefUe5 = 0x50;
inline constexpr size_t kFPropDtorLinkUe5 = 0x58;
inline constexpr size_t kFPropPostCtorLinkUe5 = 0x60;
inline constexpr size_t kFPropOffsetInternalUe5 = 0x44; // uint32
// UE4 (4_07) link pointers + RepNotifyFunc FName (RAW ONLY, P2)
inline constexpr size_t kFPropLinkNextUe4 = 0x50;
inline constexpr size_t kFPropNextRefUe4 = 0x58;
inline constexpr size_t kFPropDtorLinkUe4 = 0x60;
inline constexpr size_t kFPropPostCtorLinkUe4 = 0x68;
inline constexpr size_t kFPropOffsetInternalUe4 = 0x4C; // uint32
inline constexpr size_t kFPropRepNotifyUe4 = 0x44;
inline constexpr size_t kFPropTotalSize = 0x70;

// UnrealFlags.hpp:95 (RF_AllFlags) and :381 + enum holes (EFuncFlags).
inline constexpr uint32_t kRfAllFlags = 0x0FFFFFFFu;
inline constexpr uint32_t kFuncUndefinedBits = 0x30u;  // 0x10|0x20 holes

// Operational bounds (not offsets): task cap for the bounded object walk,
// SuperStruct chain hop bound, and chunk-pointer reuse from worker-5 facts.
inline constexpr uint32_t kMaxObjectsWalked = 64;
inline constexpr uint32_t kMaxSuperHops = 64;
inline constexpr int32_t kNumElementsPerChunk = 64 * 1024;  // UOA-HPP:88

// ---------------------------------------------------------------------------
// Inputs
// ---------------------------------------------------------------------------

// Assembled from a VALIDATED worker-5 probe_guobject_array result's
// `observed` fields (layout already resolved — this worker never re-derives
// it): objects_ptr / num_elements / num_chunks. Nothing else is consumed.
struct ValidatedGuaView {
    uint64_t objects_ptr;    // TUObjectArray.Objects under matched layout
    int32_t num_elements;
    int32_t num_chunks;
    bool valid() const noexcept {
        return objects_ptr != 0 && num_elements > 0 && num_chunks > 0;
    }
};

constexpr uint8_t kMaxCodes = 8;

// ---------------------------------------------------------------------------
// UStruct/UClass chain probe (rung 3)
// ---------------------------------------------------------------------------

struct StructChainResult {
    Verdict verdict;
    uint8_t code_count;
    RCode codes[kMaxCodes];
    // walk accounting
    uint32_t items_examined;    // non-null readable items classified
    uint32_t plausible_uobject; // items passing the UObjectBase chain
    uint32_t implausible_uobject;
    uint32_t roundtrips_ok;     // InternalIndex round-trip confirmations
    bool walk_truncated;        // cap hit before exhausting NumElements
    // first hits (absolute addresses; 0 when absent)
    uint64_t first_plausible;
    uint64_t first_ustruct_like;
    uint64_t first_uclass_like; // struct-like AND UClass extension passed
    uint32_t struct_like_count;
};

// Walk up to kMaxObjectsWalked objects of the pre-validated array; classify
// each as plausible/unplausible UObjectBase (vtable inside readable spans,
// ObjectFlags inside RF_AllFlags, index in bounds + round-trip, Outer
// null-or-readable, ClassPrivate non-null readable); identify the first
// UStruct-like object via SuperStruct-acyclic+readable and Children/
// ChildProperties readable-or-null; extend to UClass via CDO/Sparse pointers.
StructChainResult probe_ustruct_chain(const MemSpan* spans, size_t span_count,
                                      const ValidatedGuaView& gua) noexcept;

// ---------------------------------------------------------------------------
// UFunction classifier (rung 5; Inconclusive-classifier ONLY)
// ---------------------------------------------------------------------------

struct UFunctionProbeResult {
    Verdict verdict;            // always Inconclusive by design
    uint8_t code_count;
    RCode codes[kMaxCodes];
    // raw observations, never interpreted beyond cited classifier rules
    uint64_t func_ptr;          // low 8 bytes at +0xD8 (width [16,8] agnostic)
    uint32_t function_flags;    // u32 at +0xB0 (bounded by NumParms@0xB4)
    uint8_t num_parms;          // +0xB4
    uint16_t parms_size;        // +0xB6
    uint16_t retval_offset;     // +0xB8
};

UFunctionProbeResult probe_ufunction(const MemSpan* spans, size_t span_count,
                                     uint64_t function_addr) noexcept;

// ---------------------------------------------------------------------------
// FField/FProperty variant-layout discriminator (rung 3, hardest gap)
// ---------------------------------------------------------------------------

enum class FieldVariant : uint8_t {
    None = 0,
    UE5_FField = 1,    // anchor is an FField (FField header + 5_06 payload)
    UE4_UProperty = 2, // anchor is a UObject (UObjectBase + 4_07 payload)
};

struct FieldVariantResult {
    Verdict verdict;        // Validated iff exactly one hypothesis survived
    FieldVariant variant;   // None unless Validated
    uint8_t code_count;
    RCode codes[kMaxCodes];
    // per-hypothesis detail (each carries its refute code when it failed)
    Verdict h5_verdict; RCode h5_code;
    Verdict h4_verdict; RCode h4_code;
    // raw observations (FName-width fields copied raw, never interpreted)
    uint64_t h5_class_private, h5_owner, h5_next;
    uint32_t h5_flags_private;
    uint32_t h5_array_dim, h5_element_size;
    uint64_t h4_vptr, h4_class_private, h4_outer;
    uint32_t h4_object_flags, h4_internal_index;
    uint32_t h4_offset_internal;
    // exact additional evidence that would settle an ambiguous case
    // (only populated on FLD_E_AMBIGUOUS; stable strings, cited in REPORT.md)
    const char* settling_evidence;
};

// Classify `property_addr` under both variant hypotheses. `gua` (optional)
// enables the UE4 InternalIndex round-trip: a UE4 UProperty MUST be a
// GUObjectArray-tracked object; a round-trip hit is H4 support, a miss over
// the bounded walk is weak H5 support (never proof — emit Inconclusive codes,
// never Refute, on miss).
FieldVariantResult probe_ffield_variant(
    const MemSpan* spans, size_t span_count, uint64_t property_addr,
    const ValidatedGuaView* gua) noexcept;

// Convenience wrapper: read Children@+0x48 / ChildProperties@+0x50 (SET-5_06
// UStruct offsets) from a struct anchor and run the variant discriminator on
// the first non-null child head. If both heads are null the struct simply has
// no children: Inconclusive FLD_E_NO_CANDIDATES-style (no anchor to test).
FieldVariantResult probe_ustruct_child_variant(
    const MemSpan* spans, size_t span_count, uint64_t struct_addr,
    const ValidatedGuaView* gua) noexcept;

}  // namespace rfl

#endif  // REFLECTION_LAYOUT_PREDICATES_HPP
