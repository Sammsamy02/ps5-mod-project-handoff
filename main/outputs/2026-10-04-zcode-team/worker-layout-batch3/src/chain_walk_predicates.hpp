// chain_walk_predicates.hpp
//
// BATCH-3 CHAIN-WALK LAYOUT PREDICATES (rung 3 derivable closures).
// Team: 2026-10-04-zcode-team / worker-layout-batch3 (ZCode).
//
// This file EXTENDS worker-layout-batch2's `rfl` predicate set (which extends
// worker-5's `tlp` set) with the chains that become derivable ONCE a validated
// UStruct/UClass anchor exists:
//
//   1. probe_ustruct_children_walk      — UStruct.Children@0x48 chain,
//         linked by UField::Next@0x28 (each node is a UObject).
//   2. probe_ustruct_childproperties_walk — UStruct.ChildProperties@0x50
//         chain, linked by FField::Next@0x18 (each node is an FField, UE5
//         variant settled by batch2's probe_ffield_variant).
//   3. probe_uclass_property_links      — UClass CDO@0x110 back-pointer +
//         UStruct PropertyLink@0x70 / RefLink@0x78 / DestructorLink@0x80
//         chains linked by FProperty::PropertyLinkNext@0x48.
//
// Same conventions as both siblings: pure functions, no OS calls, no
// allocation, no exceptions, no console/network; explicit LE decoding; all
// memory access through caller-supplied tlp::MemSpan views. Sibling files are
// included read-only and NEVER modified. The duplicated UObjectBase
// classification lives in the .cpp (batch2's helper is in an anonymous
// namespace) and is a check-for-check mirror of
// worker-layout-batch2/src/reflection_layout_predicates.cpp
// classify_uobjectbase(); the host tests pin both verdicts to identical
// inputs.
//
// PROVENANCE OF EVERY CONSTANT (nothing guessed):
//   SET-5_06 = .superpowers/native-dependency-spike/native-coherent-runtime-1/
//     source/port/unreal/generated_include/FunctionBodies/
//     5_06_MemberVariableLayout_DefaultSetter_<Class>.cpp (verified this
//     session by direct read of the setter bodies):
//       UField:   Next=0x28, UEP_TotalSize=0x30.
//       UStruct:  SuperStruct=0x40, Children=0x48, ChildProperties=0x50,
//                 PropertiesSize=0x58 (uint32), MinAlignment=0x5C,
//                 Script=0x60, PropertyLink=0x70, RefLink=0x78,
//                 DestructorLink=0x80, PostConstructLink=0x88,
//                 UEP_TotalSize=0xB0.
//       UClass:   ClassDefaultObject=0x110 (SET-5_06, cited by batch2).
//       FField:   ClassPrivate=0x8, Owner=0x10, Next=0x18
//                 (SET-5_06, cited by batch2).
//       FProperty(UE5): ArrayDim=0x30, ElementSize=0x34,
//                 Offset_Internal=0x44, PropertyLinkNext=0x48
//                 (SET-5_06, cited by batch2).
//       UObjectBase: ObjectFlags=0x8, InternalIndex_Private=0xC,
//                 ClassPrivate=0x10, NamePrivate=0x18, OuterPrivate=0x20,
//                 UEP_TotalSize=0x28 (SET-5_06, cited by batch2/worker-5).
//   Chain-owner semantics:
//       FField.hpp:188 `friend FieldType* GetChildFieldsFromStruct(UStruct*
//         Owner);` and :190 `GetNextField(FieldType*)` (port
//         ue4ss-unreal-native-build-1/port/unreal/include/Unreal/FField.hpp)
//         — ChildProperties chain nodes are owned by the UStruct and chained
//         by Next; hence Owner@0x10 == struct anchor for every node of a
//         ChildProperties chain.
//       Class.hpp (port Unreal/CoreUObject/UObject/Class.hpp, UField
//         consolidated there per UField.hpp forwarding note): a UField's
//         OuterPrivate is its owning UStruct/UClass and Children chain nodes
//         are linked by UField::Next; hence OuterPrivate@0x20 == struct
//         anchor for every node of a Children chain.
//       PROPERTY-LINK chains deliberately do NOT use an equality check on
//         Owner: UStruct::PropertyLink/RefLink/DestructorLink include
//         inherited properties whose FField::Owner is the declaring
//         super-UStruct, so only null-or-(canonical AND readable) is
//         asserted there.
//   Verdict masks/bounds reused from batch2 (cited there): kRfAllFlags
//     0x0FFFFFFF (UnrealFlags.hpp:95), kNumElementsPerChunk 64*1024
//     (UObjectArray.hpp:88), item stride 0x18 (SET-5_06 FUObjectItem).
//
// VERDICT DISCIPLINE (identical to worker-5/batch2):
//   Validated    = the whole chain walked under its cap with every node
//                  passing all hard checks (evidence for the mapped snapshot
//                  only). An empty chain (head==0) is NOT Validated: nothing
//                  was confirmed — it is Inconclusive with an info code.
//   Refuted      = a cited layout forbids the bytes (non-canonical/dangling
//                  pointer, ObjectFlags outside RF_AllFlags, index
//                  bounds/round-trip failure, owner back-pointer mismatch,
//                  chain cycle, CDO ClassPrivate mismatch).
//   Inconclusive = evidence unavailable (unreadable windows, walk truncated
//                  at the node cap, empty chain, invalid GUA view). NEVER
//                  success or failure.
//   Negative codes (CW_N_*) record classifier-only implausibilities
//   (element-size zero, property offset beyond PropertiesSize) WITHOUT
//   flipping the verdict — same negative-vs-refute split as batch2's
//   FN_N_* codes. No predicate here manufactures any width: staging remains
//   gated by the compiled accessor width contract (widths-1 + widths-2).
//
// WIDTH CONSEQUENCES ENFORCED (see PROMOTABLE-SET.md for the full table):
//   Nothing staged here. These walks strengthen the EVIDENCE for offsets
//   (ChildProperties, UField/FField Next, CDO, PropertyLink family) whose
//   staging is separately width-gated; FField::Owner/Next stay
//   cannot-stage (null compiled width) even when the walk is Validated.

#ifndef CHAIN_WALK_PREDICATES_HPP
#define CHAIN_WALK_PREDICATES_HPP

#include <cstdint>

#include "../../worker-layout-batch2/src/reflection_layout_predicates.hpp"

namespace rfl3 {  // batch-3 chain-walk predicates

using tlp::MemSpan;
using tlp::Verdict;
using rfl::ValidatedGuaView;

// ---------------------------------------------------------------------------
// Rationale codes (stable, machine-readable; distinct from rfl::RCode)
// ---------------------------------------------------------------------------

enum class RCode : uint16_t {
    OK = 0,

    // UStruct.Children walk (0x55xx)
    CH_E_NO_GUA_VIEW        = 0x5501, // GUA view invalid — round-trips impossible
    CH_E_WINDOW_UNREADABLE  = 0x5502, // [struct, struct+0x60) unavailable
    CH_I_CHAIN_EMPTY        = 0x5503, // Children head null — nothing to confirm
    CH_R_HEAD_BAD           = 0x5504, // Children head non-canonical/unreadable
    CH_R_NODE_VPTR_BAD      = 0x5505, // node vtable null/non-canonical/unreadable
    CH_R_NODE_FLAGS_MASK    = 0x5506, // node ObjectFlags outside RF_AllFlags
    CH_R_NODE_INDEX         = 0x5507, // node InternalIndex bounds/round-trip fail
    CH_R_NODE_CLASS_BAD     = 0x5508, // node ClassPrivate null/bad/unreadable
    CH_R_NODE_OUTER_BAD     = 0x5509, // node OuterPrivate non-null bad pointer
    CH_R_OWNER_MISMATCH     = 0x550A, // node OuterPrivate != walk anchor
    CH_R_NEXT_BAD           = 0x550B, // UField::Next non-null non-canonical/unreadable
    CH_R_CYCLE              = 0x550C, // chain revisits a node (Brent detection)
    CH_I_WALK_TRUNCATED     = 0x550D, // node cap reached — evidence incomplete
    CH_I_CHAIN_OK           = 0x550E, // info: all walked nodes passed

    // UStruct.ChildProperties walk (0x56xx)
    CP_E_WINDOW_UNREADABLE  = 0x5601, // [struct, struct+0x60) unavailable
    CP_I_CHAIN_EMPTY        = 0x5602, // ChildProperties head null
    CP_R_HEAD_BAD           = 0x5603, // head non-canonical/unreadable
    CP_R_CLASS_BAD          = 0x5604, // FField ClassPrivate null/bad/unreadable
    CP_R_OWNER_MISMATCH     = 0x5605, // FField Owner != struct anchor
    CP_R_NEXT_BAD           = 0x5606, // FField::Next non-null bad
    CP_R_ARRAYDIM_ZERO      = 0x5607, // FProperty ArrayDim==0 (batch2 precedent)
    CP_N_ELEMENT_SIZE_ZERO  = 0x5608, // negative-only: ElementSize==0
    CP_N_OFFSET_BEYOND_SIZE = 0x5609, // negative-only: Offset_Internal>PropertiesSize
    CP_R_CYCLE              = 0x560A, // chain cycle
    CP_I_WALK_TRUNCATED     = 0x560B, // node cap reached
    CP_I_CHAIN_OK           = 0x560C, // info: all walked nodes passed

    // UClass CDO + property-link chains (0x57xx)
    UC_E_NO_GUA_VIEW        = 0x5701, // GUA view invalid
    UC_E_WINDOW_UNREADABLE  = 0x5702, // [class, class+0x118) unavailable
    UC_I_NO_CDO             = 0x5703, // CDO null (info, common for abstract classes)
    UC_R_CDO_BAD            = 0x5704, // CDO non-canonical/unreadable/fails UObjectBase
    UC_R_CDO_CLASS_MISMATCH = 0x5705, // CDO.ClassPrivate != class anchor
    UC_I_LINK_HEAD_NULL     = 0x5706, // a link head is null (recorded per walk)
    UC_R_LINK_HEAD_BAD      = 0x5707, // link head non-canonical/unreadable
    UC_R_NODE_BAD           = 0x5708, // link node failed an FField/FField-class check
    UC_R_NEXT_BAD           = 0x5709, // PropertyLinkNext non-null bad
    UC_R_CYCLE              = 0x570A, // link chain cycle
    UC_I_WALK_TRUNCATED     = 0x570B, // node cap reached on some chain
    UC_N_OFFSET_BEYOND_SIZE = 0x570C, // negative-only: offset beyond PropertiesSize
    UC_I_LINKS_OK           = 0x570D, // info: CDO (when present) + all chains passed
    UC_E_ALL_EVIDENCE_NULL  = 0x570E, // CDO null AND all three link heads null
};

const char* rcode_string(RCode c);

// ---------------------------------------------------------------------------
// Cited structure constants (see provenance block above)
// ---------------------------------------------------------------------------

// SET-5_06 UField / UStruct / UClass / FField / FProperty
inline constexpr size_t kUFieldNext = 0x28;
inline constexpr size_t kUStructChildren = 0x48;
inline constexpr size_t kUStructChildProperties = 0x50;
inline constexpr size_t kUStructPropertiesSize = 0x58;  // uint32
inline constexpr size_t kUStructPropertyLink = 0x70;
inline constexpr size_t kUStructRefLink = 0x78;
inline constexpr size_t kUStructDestructorLink = 0x80;
inline constexpr size_t kUClassCDO = 0x110;
inline constexpr size_t kFFieldClassPrivate = 0x8;
inline constexpr size_t kFFieldOwner = 0x10;
inline constexpr size_t kFFieldNext = 0x18;
inline constexpr size_t kFPropArrayDim = 0x30;
inline constexpr size_t kFPropElementSize = 0x34;
inline constexpr size_t kFPropOffsetInternalUe5 = 0x44;
inline constexpr size_t kFPropPropertyLinkNextUe5 = 0x48;
// SET-5_06 UObjectBase (mirror of batch2 constants, kept local for clarity)
inline constexpr size_t kObjObjectFlags = 0x8;
inline constexpr size_t kObjInternalIndex = 0xC;
inline constexpr size_t kObjClassPrivate = 0x10;
inline constexpr size_t kObjOuterPrivate = 0x20;
inline constexpr size_t kObjTotalSize = 0x28;
// Masks/bounds reused from batch2 (cited there)
inline constexpr uint32_t kRfAllFlags = 0x0FFFFFFFu;  // UnrealFlags.hpp:95
inline constexpr int32_t kNumElementsPerChunk = 64 * 1024;  // UOA-HPP:88
inline constexpr size_t kItemStride = 0x18;  // SET-5_06 FUObjectItem
inline constexpr size_t kItemObject = 0x0;

// Operational bound: a large UClass can own many properties; the cap keeps the
// walk bounded (deterministic, no allocation). Exceeding it is Inconclusive
// (truncated), never a failure.
inline constexpr uint32_t kMaxChainNodes = 1024;

constexpr uint8_t kMaxCodes = 8;

// ---------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------

struct ChainWalkResult {
    Verdict verdict;
    uint8_t code_count;
    RCode codes[kMaxCodes];
    uint32_t nodes_visited;
    uint32_t nodes_validated;
    uint64_t head;      // resolved chain head (0 when empty)
    uint32_t properties_size; // u32@0x58 of the anchor, recorded raw
    bool truncated;
};

// Walk UStruct.Children@0x48 via UField::Next@0x28. Every node must pass the
// full UObjectBase classification (batch2-mirrored) AND its OuterPrivate must
// equal `struct_addr` (chain-owner semantics). `gua` must be a VALIDATED view;
// without it the predicate is Inconclusive by construction.
ChainWalkResult probe_ustruct_children_walk(const MemSpan* spans,
                                            size_t span_count,
                                            uint64_t struct_addr,
                                            const ValidatedGuaView& gua) noexcept;

// Walk UStruct.ChildProperties@0x50 via FField::Next@0x18 under the UE5 FField
// hypothesis settled by batch2's probe_ffield_variant (running it before that
// discriminator returned Validated/UE5_FField is a caller error; the hard
// checks below are exactly batch2's H5 rules plus owner back-pointer).
ChainWalkResult probe_ustruct_childproperties_walk(
    const MemSpan* spans, size_t span_count, uint64_t struct_addr) noexcept;

// Check UClass CDO@0x110 (back-pointer CDO.ClassPrivate == class anchor) and
// walk PropertyLink@0x70 / RefLink@0x78 / DestructorLink@0x80 via
// FProperty::PropertyLinkNext@0x48. Link nodes are FFields: H5 checks apply;
// owner equality is NOT asserted (chains include inherited properties owned
// by super-UStructs).
ChainWalkResult probe_uclass_property_links(const MemSpan* spans,
                                            size_t span_count,
                                            uint64_t class_addr,
                                            const ValidatedGuaView& gua) noexcept;

}  // namespace rfl3

#endif  // CHAIN_WALK_PREDICATES_HPP
