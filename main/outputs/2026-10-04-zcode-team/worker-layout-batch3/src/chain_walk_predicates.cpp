// chain_walk_predicates.cpp
//
// Implementation notes
// --------------------
// * Explicit little-endian byte composition (ELF64 x86-64 LE pinned by the
//   profile), independent of host endianness — identical to batch2.
// * The UObjectBase classification below is a check-for-check mirror of
//   worker-layout-batch2/src/reflection_layout_predicates.cpp
//   classify_uobjectbase() (that helper is in an anonymous namespace and
//   cannot be linked). Verdict classes match batch2 exactly: an unreadable
//   OBJECT window is EVIDENCE-UNAVAILABLE (Inconclusive), never a refutation;
//   only cited-layout contradictions refute.
// * Cycle detection is Brent's algorithm (O(1) memory) — the no-allocation
//   rule forbids a visited set. An unreadable NEXT window inside the chase
//   reads as chain end there; the per-node walker independently re-validates
//   every NEXT field and refutes on a bad one, so the chase can never mask a
//   contradiction.
// * Negative codes (CP_N_*, UC_N_*) never flip the verdict — same split as
//   batch2's FN_N_* codes.
// * No width is produced anywhere: these predicates validate offsets and
//   link semantics only.

#include "chain_walk_predicates.hpp"

#include <cstddef>

namespace rfl3 {
namespace {

using tlp::pointer_canonical_or_null;
using tlp::range_readable;
using tlp::read_bytes;

inline uint32_t rd_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
inline uint64_t rd_u64(const uint8_t* p) {
    return (uint64_t)rd_u32(p) | ((uint64_t)rd_u32(p + 4) << 32);
}

void push_code(RCode* codes, uint8_t& count, RCode c) {
    if (count >= kMaxCodes) return;
    for (uint8_t i = 0; i < count; ++i) {
        if (codes[i] == c) return;  // dedup
    }
    codes[count++] = c;
}

// Three-outcome classifier result, mirroring batch2's verdict classes.
enum class Cls : uint8_t { Ok = 0, Refute = 1, Unavailable = 2 };

// Mirror of batch2's item_addr(): FUObjectItem address for absolute index i
// under the cited chunked layout (NumElementsPerChunk 64*1024, stride 0x18).
bool item_addr(const MemSpan* spans, size_t span_count,
               const ValidatedGuaView& gua, uint32_t i, uint64_t* out) {
    const uint32_t chunk = i / (uint32_t)kNumElementsPerChunk;
    const uint32_t in_chunk = i % (uint32_t)kNumElementsPerChunk;
    if (chunk >= (uint32_t)gua.num_chunks) return false;
    uint64_t chunk_ptr = 0;
    if (!read_bytes(spans, span_count, gua.objects_ptr + (uint64_t)chunk * 8,
                    8, &chunk_ptr))
        return false;
    if (chunk_ptr == 0 || !pointer_canonical_or_null(chunk_ptr)) return false;
    *out = chunk_ptr + (uint64_t)in_chunk * kItemStride;
    return true;
}

// Null-or-(canonical AND readable-8) discipline for optional link fields.
bool link_ok(const MemSpan* spans, size_t span_count, uint64_t p) {
    if (p == 0) return true;
    if (!pointer_canonical_or_null(p)) return false;
    return range_readable(spans, span_count, p, 8);
}

// Mirror of batch2's classify_uobjectbase() over an ALREADY-READ [obj,
// obj+0x28) window. Returns Ok, or a refute code (base_rc out) with
// Cls::Refute, or Cls::Unavailable when a needed window was unreadable
// (batch2: OBJ_E_ITEM_UNREADABLE semantics).
Cls classify_uobjectbase_buf(const MemSpan* spans, size_t span_count,
                             const ValidatedGuaView& gua, uint64_t obj,
                             const uint8_t* base, RCode* base_rc) {
    const uint64_t vptr = rd_u64(base + 0);
    if (vptr == 0 || !pointer_canonical_or_null(vptr)) {
        *base_rc = RCode::CH_R_NODE_VPTR_BAD;
        return Cls::Refute;
    }
    if (!range_readable(spans, span_count, vptr, 8)) {
        *base_rc = RCode::CH_R_NODE_VPTR_BAD;
        return Cls::Refute;
    }

    const uint32_t flags = rd_u32(base + kObjObjectFlags);
    if (flags & ~kRfAllFlags) {
        *base_rc = RCode::CH_R_NODE_FLAGS_MASK;
        return Cls::Refute;
    }

    const uint32_t idx = rd_u32(base + kObjInternalIndex);
    if ((int32_t)idx < 0 || (int32_t)idx >= gua.num_elements) {
        *base_rc = RCode::CH_R_NODE_INDEX;
        return Cls::Refute;
    }
    uint64_t self_item = 0;
    if (item_addr(spans, span_count, gua, idx, &self_item)) {
        uint64_t back = 0;
        if (read_bytes(spans, span_count, self_item + kItemObject, 8, &back)) {
            if (back != obj) {
                *base_rc = RCode::CH_R_NODE_INDEX;
                return Cls::Refute;
            }
        }  // item unreadable: evidence unavailable, not a contradiction
    }

    const uint64_t classp = rd_u64(base + kObjClassPrivate);
    if (classp == 0 || !pointer_canonical_or_null(classp)) {
        *base_rc = RCode::CH_R_NODE_CLASS_BAD;
        return Cls::Refute;
    }
    if (!range_readable(spans, span_count, classp, 8)) {
        *base_rc = RCode::CH_R_NODE_CLASS_BAD;
        return Cls::Refute;
    }

    if (!link_ok(spans, span_count, rd_u64(base + kObjOuterPrivate))) {
        *base_rc = RCode::CH_R_NODE_OUTER_BAD;
        return Cls::Refute;
    }
    return Cls::Ok;  // NamePrivate never read (P2)
}

// FField node check under the settled UE5 hypothesis (batch2 H5 rules) over
// an ALREADY-READ [node, node+0x30) window. require_owner_equality=true is
// the ChildProperties-chain rule (Owner == walking anchor); false is the
// property-link-chain rule (Owner null-or-(canonical AND readable), because
// link chains include inherited properties owned by super-UStructs).
Cls classify_ffield_h5_buf(const MemSpan* spans, size_t span_count,
                           const uint8_t* head, uint64_t owner_anchor,
                           bool require_owner_equality, RCode* out_rc) {
    const uint64_t classp = rd_u64(head + kFFieldClassPrivate);
    if (classp == 0 || !pointer_canonical_or_null(classp)) {
        *out_rc = RCode::CP_R_CLASS_BAD;
        return Cls::Refute;
    }
    if (!range_readable(spans, span_count, classp, 8)) {
        *out_rc = RCode::CP_R_CLASS_BAD;
        return Cls::Refute;
    }
    const uint64_t owner = rd_u64(head + kFFieldOwner);
    if (require_owner_equality) {
        if (owner != owner_anchor) {
            *out_rc = RCode::CP_R_OWNER_MISMATCH;
            return Cls::Refute;
        }
    } else if (!link_ok(spans, span_count, owner)) {
        *out_rc = RCode::UC_R_NODE_BAD;
        return Cls::Refute;
    }
    return Cls::Ok;
}

// Brent's cycle detection over a pointer-chase function. Returns true when a
// cycle was found; `steps` receives the hops performed. The chase never
// dereferences beyond reading the stored pointer value (read_ptr below).
template <typename Fn>
bool brent_cycle(Fn next_fn, uint64_t start, uint64_t cap, uint64_t* steps) {
    uint64_t tortoise = start;
    uint64_t hare = next_fn(start);
    uint64_t power = 1;
    uint64_t count = 0;
    while (hare != 0 && tortoise != hare) {
        if (count == power) {
            tortoise = hare;
            power *= 2;
            if (power > cap + 1) break;  // cap: caller reports truncation
        }
        hare = next_fn(hare);
        ++count;
        if (count > cap) break;
    }
    *steps = count;
    return tortoise != 0 && tortoise == hare;
}

uint64_t read_ptr(const MemSpan* spans, size_t span_count, uint64_t addr) {
    uint64_t v = 0;
    if (!read_bytes(spans, span_count, addr, 8, &v)) return 0;
    return v;
}

ChainWalkResult make_result(Verdict v, uint32_t visited, uint32_t validated,
                            uint64_t head, uint32_t propsize, bool truncated) {
    ChainWalkResult r{};
    r.verdict = v;
    r.code_count = 0;
    r.nodes_visited = visited;
    r.nodes_validated = validated;
    r.head = head;
    r.properties_size = propsize;
    r.truncated = truncated;
    return r;
}

ChainWalkResult seal(ChainWalkResult r, const RCode* codes, uint8_t n) {
    const uint8_t m = n > kMaxCodes ? kMaxCodes : n;
    for (uint8_t i = 0; i < m; ++i) r.codes[i] = codes[i];
    r.code_count = m;
    return r;
}

}  // namespace

const char* rcode_string(RCode c) {
    switch (c) {
        case RCode::OK: return "OK";
        case RCode::CH_E_NO_GUA_VIEW: return "CH_E_NO_GUA_VIEW";
        case RCode::CH_E_WINDOW_UNREADABLE: return "CH_E_WINDOW_UNREADABLE";
        case RCode::CH_I_CHAIN_EMPTY: return "CH_I_CHAIN_EMPTY";
        case RCode::CH_R_HEAD_BAD: return "CH_R_HEAD_BAD";
        case RCode::CH_R_NODE_VPTR_BAD: return "CH_R_NODE_VPTR_BAD";
        case RCode::CH_R_NODE_FLAGS_MASK: return "CH_R_NODE_FLAGS_MASK";
        case RCode::CH_R_NODE_INDEX: return "CH_R_NODE_INDEX";
        case RCode::CH_R_NODE_CLASS_BAD: return "CH_R_NODE_CLASS_BAD";
        case RCode::CH_R_NODE_OUTER_BAD: return "CH_R_NODE_OUTER_BAD";
        case RCode::CH_R_OWNER_MISMATCH: return "CH_R_OWNER_MISMATCH";
        case RCode::CH_R_NEXT_BAD: return "CH_R_NEXT_BAD";
        case RCode::CH_R_CYCLE: return "CH_R_CYCLE";
        case RCode::CH_I_WALK_TRUNCATED: return "CH_I_WALK_TRUNCATED";
        case RCode::CH_I_CHAIN_OK: return "CH_I_CHAIN_OK";
        case RCode::CP_E_WINDOW_UNREADABLE: return "CP_E_WINDOW_UNREADABLE";
        case RCode::CP_I_CHAIN_EMPTY: return "CP_I_CHAIN_EMPTY";
        case RCode::CP_R_HEAD_BAD: return "CP_R_HEAD_BAD";
        case RCode::CP_R_CLASS_BAD: return "CP_R_CLASS_BAD";
        case RCode::CP_R_OWNER_MISMATCH: return "CP_R_OWNER_MISMATCH";
        case RCode::CP_R_NEXT_BAD: return "CP_R_NEXT_BAD";
        case RCode::CP_R_ARRAYDIM_ZERO: return "CP_R_ARRAYDIM_ZERO";
        case RCode::CP_N_ELEMENT_SIZE_ZERO: return "CP_N_ELEMENT_SIZE_ZERO";
        case RCode::CP_N_OFFSET_BEYOND_SIZE: return "CP_N_OFFSET_BEYOND_SIZE";
        case RCode::CP_R_CYCLE: return "CP_R_CYCLE";
        case RCode::CP_I_WALK_TRUNCATED: return "CP_I_WALK_TRUNCATED";
        case RCode::CP_I_CHAIN_OK: return "CP_I_CHAIN_OK";
        case RCode::UC_E_NO_GUA_VIEW: return "UC_E_NO_GUA_VIEW";
        case RCode::UC_E_WINDOW_UNREADABLE: return "UC_E_WINDOW_UNREADABLE";
        case RCode::UC_I_NO_CDO: return "UC_I_NO_CDO";
        case RCode::UC_R_CDO_BAD: return "UC_R_CDO_BAD";
        case RCode::UC_R_CDO_CLASS_MISMATCH: return "UC_R_CDO_CLASS_MISMATCH";
        case RCode::UC_I_LINK_HEAD_NULL: return "UC_I_LINK_HEAD_NULL";
        case RCode::UC_R_LINK_HEAD_BAD: return "UC_R_LINK_HEAD_BAD";
        case RCode::UC_R_NODE_BAD: return "UC_R_NODE_BAD";
        case RCode::UC_R_NEXT_BAD: return "UC_R_NEXT_BAD";
        case RCode::UC_R_CYCLE: return "UC_R_CYCLE";
        case RCode::UC_I_WALK_TRUNCATED: return "UC_I_WALK_TRUNCATED";
        case RCode::UC_N_OFFSET_BEYOND_SIZE: return "UC_N_OFFSET_BEYOND_SIZE";
        case RCode::UC_I_LINKS_OK: return "UC_I_LINKS_OK";
        case RCode::UC_E_ALL_EVIDENCE_NULL: return "UC_E_ALL_EVIDENCE_NULL";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------------------
// probe_ustruct_children_walk
// ---------------------------------------------------------------------------

ChainWalkResult probe_ustruct_children_walk(const MemSpan* spans,
                                            size_t span_count,
                                            uint64_t struct_addr,
                                            const ValidatedGuaView& gua) noexcept {
    RCode codes[kMaxCodes];
    uint8_t n = 0;
    auto emit = [&](RCode c) { push_code(codes, n, c); };

    if (!gua.valid()) {
        emit(RCode::CH_E_NO_GUA_VIEW);
        return seal(make_result(Verdict::Inconclusive, 0, 0, 0, 0, false),
                    codes, n);
    }

    uint8_t anchor[0x60];  // through the UStruct direct members
    if (!read_bytes(spans, span_count, struct_addr, sizeof(anchor), &anchor)) {
        emit(RCode::CH_E_WINDOW_UNREADABLE);
        return seal(make_result(Verdict::Inconclusive, 0, 0, 0, 0, false),
                    codes, n);
    }
    const uint32_t propsize = rd_u32(anchor + kUStructPropertiesSize);
    const uint64_t head = rd_u64(anchor + kUStructChildren);

    if (head == 0) {
        emit(RCode::CH_I_CHAIN_EMPTY);
        return seal(
            make_result(Verdict::Inconclusive, 0, 0, 0, propsize, false),
            codes, n);
    }
    if (!pointer_canonical_or_null(head) ||
        !range_readable(spans, span_count, head, 8)) {
        emit(RCode::CH_R_HEAD_BAD);
        return seal(
            make_result(Verdict::Refuted, 0, 0, head, propsize, false), codes,
            n);
    }

    uint64_t chase_steps = 0;
    auto next_of = [&](uint64_t cur) -> uint64_t {
        return read_ptr(spans, span_count, cur + kUFieldNext);
    };
    if (brent_cycle(next_of, head, kMaxChainNodes, &chase_steps)) {
        emit(RCode::CH_R_CYCLE);
        return seal(
            make_result(Verdict::Refuted, 0, 0, head, propsize, false), codes,
            n);
    }

    uint32_t visited = 0, validated = 0;
    bool truncated = false;
    bool unavailable = false;
    Verdict verdict = Verdict::Validated;
    uint64_t node = head;

    while (node != 0) {
        if (visited >= kMaxChainNodes) {
            truncated = true;
            break;
        }
        ++visited;
        uint8_t nodebuf[0x30];
        if (!read_bytes(spans, span_count, node, sizeof(nodebuf), &nodebuf)) {
            emit(RCode::CH_E_WINDOW_UNREADABLE);
            unavailable = true;
            break;
        }
        RCode base_rc = RCode::OK;
        const Cls cls = classify_uobjectbase_buf(spans, span_count, gua, node,
                                                 nodebuf, &base_rc);
        if (cls == Cls::Unavailable) {
            emit(RCode::CH_E_WINDOW_UNREADABLE);
            unavailable = true;
            break;
        }
        if (cls == Cls::Refute) {
            emit(base_rc);
            verdict = Verdict::Refuted;
            break;
        }
        // Chain-owner semantics: UField::OuterPrivate == walking anchor.
        if (rd_u64(nodebuf + kObjOuterPrivate) != struct_addr) {
            emit(RCode::CH_R_OWNER_MISMATCH);
            verdict = Verdict::Refuted;
            break;
        }
        const uint64_t next = rd_u64(nodebuf + kUFieldNext);
        if (!link_ok(spans, span_count, next)) {
            emit(RCode::CH_R_NEXT_BAD);
            verdict = Verdict::Refuted;
            break;
        }
        ++validated;
        node = next;
    }

    if (truncated) {
        emit(RCode::CH_I_WALK_TRUNCATED);
        verdict = Verdict::Inconclusive;
    } else if (unavailable) {
        verdict = Verdict::Inconclusive;
    } else if (verdict == Verdict::Validated) {
        emit(RCode::CH_I_CHAIN_OK);
    }

    return seal(make_result(verdict, visited, validated, head, propsize,
                            truncated),
                codes, n);
}

// ---------------------------------------------------------------------------
// probe_ustruct_childproperties_walk
// ---------------------------------------------------------------------------

ChainWalkResult probe_ustruct_childproperties_walk(
    const MemSpan* spans, size_t span_count, uint64_t struct_addr) noexcept {
    RCode codes[kMaxCodes];
    uint8_t n = 0;
    auto emit = [&](RCode c) { push_code(codes, n, c); };

    uint8_t anchor[0x60];
    if (!read_bytes(spans, span_count, struct_addr, sizeof(anchor), &anchor)) {
        emit(RCode::CP_E_WINDOW_UNREADABLE);
        return seal(make_result(Verdict::Inconclusive, 0, 0, 0, 0, false),
                    codes, n);
    }
    const uint32_t propsize = rd_u32(anchor + kUStructPropertiesSize);
    const uint64_t head = rd_u64(anchor + kUStructChildProperties);

    if (head == 0) {
        emit(RCode::CP_I_CHAIN_EMPTY);
        return seal(
            make_result(Verdict::Inconclusive, 0, 0, 0, propsize, false),
            codes, n);
    }
    if (!pointer_canonical_or_null(head) ||
        !range_readable(spans, span_count, head, 8)) {
        emit(RCode::CP_R_HEAD_BAD);
        return seal(
            make_result(Verdict::Refuted, 0, 0, head, propsize, false), codes,
            n);
    }

    uint64_t chase_steps = 0;
    auto next_of = [&](uint64_t cur) -> uint64_t {
        return read_ptr(spans, span_count, cur + kFFieldNext);
    };
    if (brent_cycle(next_of, head, kMaxChainNodes, &chase_steps)) {
        emit(RCode::CP_R_CYCLE);
        return seal(
            make_result(Verdict::Refuted, 0, 0, head, propsize, false), codes,
            n);
    }

    uint32_t visited = 0, validated = 0;
    bool truncated = false;
    bool unavailable = false;
    Verdict verdict = Verdict::Validated;
    uint64_t node = head;

    while (node != 0) {
        if (visited >= kMaxChainNodes) {
            truncated = true;
            break;
        }
        ++visited;
        uint8_t payload[0x50];  // through PostConstructLinkNext@0x60 (UE5)
        if (!read_bytes(spans, span_count, node, sizeof(payload), &payload)) {
            emit(RCode::CP_E_WINDOW_UNREADABLE);
            unavailable = true;
            break;
        }
        RCode frc = RCode::OK;
        const Cls cls = classify_ffield_h5_buf(spans, span_count, payload,
                                               struct_addr,
                                               /*require_owner_equality=*/true,
                                               &frc);
        if (cls == Cls::Refute) {
            emit(frc);
            verdict = Verdict::Refuted;
            break;
        }
        const uint32_t array_dim = rd_u32(payload + kFPropArrayDim);
        const uint32_t element_size = rd_u32(payload + kFPropElementSize);
        const uint32_t offset_internal =
            rd_u32(payload + kFPropOffsetInternalUe5);
        if (array_dim == 0) {  // batch2 FLD_H5_R_ARRAYDIM_ZERO precedent
            emit(RCode::CP_R_ARRAYDIM_ZERO);
            verdict = Verdict::Refuted;
            break;
        }
        if (element_size == 0) emit(RCode::CP_N_ELEMENT_SIZE_ZERO);
        if (offset_internal > propsize) emit(RCode::CP_N_OFFSET_BEYOND_SIZE);

        const uint64_t next = rd_u64(payload + kFFieldNext);
        if (!link_ok(spans, span_count, next)) {
            emit(RCode::CP_R_NEXT_BAD);
            verdict = Verdict::Refuted;
            break;
        }
        ++validated;
        node = next;
    }

    if (truncated) {
        emit(RCode::CP_I_WALK_TRUNCATED);
        verdict = Verdict::Inconclusive;
    } else if (unavailable) {
        verdict = Verdict::Inconclusive;
    } else if (verdict == Verdict::Validated) {
        emit(RCode::CP_I_CHAIN_OK);
    }

    return seal(make_result(verdict, visited, validated, head, propsize,
                            truncated),
                codes, n);
}

// ---------------------------------------------------------------------------
// probe_uclass_property_links
// ---------------------------------------------------------------------------

ChainWalkResult probe_uclass_property_links(const MemSpan* spans,
                                            size_t span_count,
                                            uint64_t class_addr,
                                            const ValidatedGuaView& gua) noexcept {
    RCode codes[kMaxCodes];
    uint8_t n = 0;
    auto emit = [&](RCode c) { push_code(codes, n, c); };

    if (!gua.valid()) {
        emit(RCode::UC_E_NO_GUA_VIEW);
        return seal(make_result(Verdict::Inconclusive, 0, 0, 0, 0, false),
                    codes, n);
    }

    uint8_t anchor[0x118];  // through CDO@0x110 (+8)
    if (!read_bytes(spans, span_count, class_addr, sizeof(anchor), &anchor)) {
        emit(RCode::UC_E_WINDOW_UNREADABLE);
        return seal(make_result(Verdict::Inconclusive, 0, 0, 0, 0, false),
                    codes, n);
    }
    const uint32_t propsize = rd_u32(anchor + kUStructPropertiesSize);
    const uint64_t cdo = rd_u64(anchor + kUClassCDO);

    bool cdo_evidence = false;
    if (cdo == 0) {
        emit(RCode::UC_I_NO_CDO);
    } else {
        if (!pointer_canonical_or_null(cdo) ||
            !range_readable(spans, span_count, cdo, kObjTotalSize)) {
            emit(RCode::UC_R_CDO_BAD);
            return seal(
                make_result(Verdict::Refuted, 0, 0, cdo, propsize, false),
                codes, n);
        }
        uint8_t cdobuf[kObjTotalSize];
        if (!read_bytes(spans, span_count, cdo, sizeof(cdobuf), &cdobuf)) {
            emit(RCode::UC_R_CDO_BAD);
            return seal(
                make_result(Verdict::Refuted, 0, 0, cdo, propsize, false),
                codes, n);
        }
        RCode base_rc = RCode::OK;
        const Cls cls = classify_uobjectbase_buf(spans, span_count, gua, cdo,
                                                 cdobuf, &base_rc);
        if (cls == Cls::Unavailable) {
            emit(RCode::UC_E_WINDOW_UNREADABLE);
            return seal(
                make_result(Verdict::Inconclusive, 0, 0, cdo, propsize, false),
                codes, n);
        }
        if (cls == Cls::Refute) {
            emit(RCode::UC_R_CDO_BAD);
            return seal(
                make_result(Verdict::Refuted, 0, 0, cdo, propsize, false),
                codes, n);
        }
        if (rd_u64(cdobuf + kObjClassPrivate) != class_addr) {
            emit(RCode::UC_R_CDO_CLASS_MISMATCH);
            return seal(
                make_result(Verdict::Refuted, 0, 0, cdo, propsize, false),
                codes, n);
        }
        cdo_evidence = true;
    }

    static const size_t kLinkOffsets[3] = {kUStructPropertyLink,
                                           kUStructRefLink,
                                           kUStructDestructorLink};
    uint32_t visited_total = 0, validated_total = 0;
    bool truncated = false;
    bool unavailable = false;
    Verdict verdict = Verdict::Validated;
    uint64_t first_head = 0;

    for (int li = 0; li < 3; ++li) {
        const uint64_t head = rd_u64(anchor + kLinkOffsets[li]);
        if (li == 0) first_head = head;
        if (head == 0) {
            emit(RCode::UC_I_LINK_HEAD_NULL);
            continue;
        }
        if (!pointer_canonical_or_null(head) ||
            !range_readable(spans, span_count, head, 8)) {
            emit(RCode::UC_R_LINK_HEAD_BAD);
            return seal(make_result(Verdict::Refuted, visited_total,
                                    validated_total, head, propsize, truncated),
                        codes, n);
        }

        uint64_t chase_steps = 0;
        auto next_of = [&](uint64_t cur) -> uint64_t {
            return read_ptr(spans, span_count, cur + kFPropPropertyLinkNextUe5);
        };
        if (brent_cycle(next_of, head, kMaxChainNodes, &chase_steps)) {
            emit(RCode::UC_R_CYCLE);
            return seal(make_result(Verdict::Refuted, visited_total,
                                    validated_total, head, propsize, truncated),
                        codes, n);
        }

        uint64_t node = head;
        while (node != 0) {
            if (visited_total >= kMaxChainNodes) {
                truncated = true;
                break;
            }
            ++visited_total;
            uint8_t payload[0x50];
            if (!read_bytes(spans, span_count, node, sizeof(payload),
                            &payload)) {
                emit(RCode::UC_E_WINDOW_UNREADABLE);
                unavailable = true;
                break;
            }
            RCode frc = RCode::OK;
            const Cls cls = classify_ffield_h5_buf(
                spans, span_count, payload, class_addr,
                /*require_owner_equality=*/false, &frc);
            if (cls == Cls::Refute) {
                emit(RCode::UC_R_NODE_BAD);
                return seal(make_result(Verdict::Refuted, visited_total,
                                        validated_total, head, propsize,
                                        truncated),
                            codes, n);
            }
            const uint32_t offset_internal =
                rd_u32(payload + kFPropOffsetInternalUe5);
            if (offset_internal > propsize) {
                emit(RCode::UC_N_OFFSET_BEYOND_SIZE);  // negative-only
            }
            const uint64_t next = rd_u64(payload + kFPropPropertyLinkNextUe5);
            if (!link_ok(spans, span_count, next)) {
                emit(RCode::UC_R_NEXT_BAD);
                return seal(make_result(Verdict::Refuted, visited_total,
                                        validated_total, head, propsize,
                                        truncated),
                            codes, n);
            }
            ++validated_total;
            node = next;
        }
        if (truncated || unavailable) break;
    }

    if (truncated) {
        emit(RCode::UC_I_WALK_TRUNCATED);
        verdict = Verdict::Inconclusive;
    } else if (unavailable) {
        verdict = Verdict::Inconclusive;
    } else if (verdict == Verdict::Validated) {
        if (!cdo_evidence && visited_total == 0) {
            emit(RCode::UC_E_ALL_EVIDENCE_NULL);
            verdict = Verdict::Inconclusive;
        } else {
            emit(RCode::UC_I_LINKS_OK);
        }
    }

    return seal(make_result(verdict, visited_total, validated_total,
                            first_head, propsize, truncated),
                codes, n);
}

}  // namespace rfl3
