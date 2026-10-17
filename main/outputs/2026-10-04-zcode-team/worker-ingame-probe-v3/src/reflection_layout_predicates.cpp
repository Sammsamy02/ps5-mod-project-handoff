// reflection_layout_predicates.cpp
//
// Implementation notes
// --------------------
// * Integer decoding is explicit little-endian byte composition (ELF64
//   x86-64 LE pinned by the profile), independent of host endianness.
// * Pointer checks reuse worker-5's tlp helpers so canonicality/readability
//   semantics are byte-identical across the two predicate sets.
// * FName-width fields (NamePrivate, RepNotifyFunc) are copied raw and never
//   interpreted: FName storage width is unresolved (WIDTHS FName 0/0) and the
//   P2 rule excludes name-pool reasoning.
// * Every refute-class check is a structural contradiction against a cited
//   constant; plausibility relations that rest on engine semantics rather
//   than a cited constant (ArrayDim>=1, NumParms/ParmsSize coherence,
//   ReturnValueOffset<=ParmsSize, MinAlignment power-of-two) are used ONLY in
//   classifier-class codes or documented refute codes (FLD_H5_R_ARRAYDIM_ZERO)
//   and are labeled as such in the header.

#include "reflection_layout_predicates.hpp"

namespace rfl {
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
inline uint16_t rd_u16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

void push_code(RCode* codes, uint8_t& count, RCode c) {
    if (count >= kMaxCodes) return;
    for (uint8_t i = 0; i < count; ++i) {
        if (codes[i] == c) return;  // dedup
    }
    codes[count++] = c;
}

// Resolve FUObjectItem address for absolute index i using the cited chunked
// layout (NumElementsPerChunk 64*1024, stride 0x18). False = chunk pointer
// unreadable or chunk index outside NumChunks (evidence unavailable).
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

// Null-or-(canonical AND readable-8) pointer discipline used for every
// optional link field. Returns: 0 ok, 1 non-canonical, 2 unreadable.
int link_ptr_check(const MemSpan* spans, size_t span_count, uint64_t p) {
    if (p == 0) return 0;
    if (!pointer_canonical_or_null(p)) return 1;
    if (!range_readable(spans, span_count, p, 8)) return 2;
    return 0;
}

// UObjectBase plausibility for one object address. Returns a code: OK when
// plausible, else the first matching OBJ_R_* contradiction. Fills
// index_roundtrip_ok when the InternalIndex round-trip was performed and hit.
RCode classify_uobjectbase(const MemSpan* spans, size_t span_count,
                           const ValidatedGuaView& gua, uint64_t obj,
                           bool* index_roundtrip_ok) {
    *index_roundtrip_ok = false;
    uint8_t base[kObjTotalSize];
    if (!read_bytes(spans, span_count, obj, sizeof(base), &base))
        return RCode::OBJ_E_ITEM_UNREADABLE;

    // P1: vtable pointer at +0x0 inside readable image ranges.
    const uint64_t vptr = rd_u64(base + 0);
    if (vptr == 0 || !pointer_canonical_or_null(vptr))
        return RCode::OBJ_R_VPTR_NONCANONICAL;
    if (!range_readable(spans, span_count, vptr, 8))
        return RCode::OBJ_R_VPTR_UNREADABLE;

    // P2: ObjectFlags within RF_AllFlags (cited mask).
    const uint32_t flags = rd_u32(base + kObjObjectFlags);
    if (flags & ~kRfAllFlags) return RCode::OBJ_R_FLAGS_MASK;

    // P3: InternalIndex bounds + round-trip (rung-2 cross-check pattern).
    const uint32_t idx = rd_u32(base + kObjInternalIndex);
    if ((int32_t)idx < 0 || (int32_t)idx >= gua.num_elements)
        return RCode::OBJ_R_INDEX_BOUNDS;
    uint64_t self_item = 0;
    if (item_addr(spans, span_count, gua, idx, &self_item)) {
        uint64_t back = 0;
        if (read_bytes(spans, span_count, self_item + kItemObject, 8, &back))
            *index_roundtrip_ok = (back == obj);
        if (!*index_roundtrip_ok) return RCode::OBJ_R_INDEX_ROUNDTRIP;
    }  // item unreadable: evidence unavailable, not a contradiction

    // P4: ClassPrivate non-null canonical readable (rung-2 cross-check).
    const uint64_t classp = rd_u64(base + kObjClassPrivate);
    if (classp == 0 || !pointer_canonical_or_null(classp))
        return RCode::OBJ_R_CLASSPTR_BAD;
    if (!range_readable(spans, span_count, classp, 8))
        return RCode::OBJ_R_CLASSPTR_BAD;

    // P5: OuterPrivate null-or-readable.
    const uint64_t outer = rd_u64(base + kObjOuterPrivate);
    if (link_ptr_check(spans, span_count, outer) != 0)
        return RCode::OBJ_R_OUTER_BAD;

    return RCode::OK;  // plausible UObjectBase (NamePrivate never read)
}

// UStruct-like extension of a plausible object. Returns OK / refute code /
// Inconclusive-class code. Cycle detection via bounded linear visited list.
RCode classify_ustruct_like(const MemSpan* spans, size_t span_count,
                            uint64_t obj) {
    uint8_t win[0x60];  // covers SuperStruct..ChildProperties + size/align
    if (!read_bytes(spans, span_count, obj, sizeof(win), &win))
        return RCode::STR_E_STRUCT_WINDOW_UNREADABLE;

    // SuperStruct null-or-readable, chain acyclic (hop cap 64).
    const int super_chk =
        link_ptr_check(spans, span_count, rd_u64(win + kUStructSuperStruct));
    if (super_chk == 1 || super_chk == 2) return RCode::STR_R_SUPER_BAD;
    uint64_t visited[kMaxSuperHops];
    uint32_t hops = 0;
    uint64_t cur = rd_u64(win + kUStructSuperStruct);
    while (cur != 0) {
        for (uint32_t k = 0; k < hops; ++k)
            if (visited[k] == cur) return RCode::STR_R_SUPER_CYCLE;
        if (hops >= kMaxSuperHops) break;  // bound hit: stop, chain so far acyclic
        visited[hops++] = cur;
        uint8_t sb[0x48];  // need up to SuperStruct field at +0x40
        if (!read_bytes(spans, span_count, cur, kUStructSuperStruct + 8, &sb))
            return RCode::STR_R_SUPER_BAD;  // mid-chain unreadable after a
                                            // non-null hop: bad super link
        const uint64_t next = rd_u64(sb + kUStructSuperStruct);
        if (next != 0 && !pointer_canonical_or_null(next))
            return RCode::STR_R_SUPER_BAD;
        cur = next;
    }

    // Children / ChildProperties readable-or-null.
    const int child_chk =
        link_ptr_check(spans, span_count, rd_u64(win + kUStructChildren));
    if (child_chk != 0) return RCode::STR_R_CHILDREN_BAD;
    const int cprop_chk =
        link_ptr_check(spans, span_count, rd_u64(win + kUStructChildProperties));
    if (cprop_chk != 0) return RCode::STR_R_CHILDPROPS_BAD;

    // PropertiesSize/MinAlignment: raw record only (no cited bound claimed).
    return RCode::OK;
}

// UClass extension of a struct-like object (pointer fields only; FuncMap and
// NativeFunctionLookupTable are containers = worker-6 scope, raw u32 only).
RCode classify_uclass_ext(const MemSpan* spans, size_t span_count,
                          uint64_t obj, uint32_t* func_map_raw,
                          uint32_t* native_lookup_raw) {
    uint8_t tail[8];
    uint64_t f = obj + kUClassClassDefaultObject;
    if (!read_bytes(spans, span_count, f, 8, &tail)) {
        // window past 0x110 may be legitimately unmapped for a smaller
        // object: evidence unavailable, not a contradiction
        return RCode::STR_E_UCLASS_WINDOW_UNREADABLE;
    }
    if (link_ptr_check(spans, span_count, rd_u64(tail)) != 0)
        return RCode::STR_E_UCLASS_WINDOW_UNREADABLE;  // CDO bad: not-classlike
    f = obj + kUClassSparseClassData;
    if (read_bytes(spans, span_count, f, 8, &tail)) {
        if (link_ptr_check(spans, span_count, rd_u64(tail)) != 0)
            return RCode::STR_E_UCLASS_WINDOW_UNREADABLE;
        f = obj + kUClassSparseClassDataStruct;
        if (read_bytes(spans, span_count, f, 8, &tail)) {
            if (link_ptr_check(spans, span_count, rd_u64(tail)) != 0)
                return RCode::STR_E_UCLASS_WINDOW_UNREADABLE;
        }
    }
    if (func_map_raw != nullptr &&
        !read_bytes(spans, span_count, obj + kUClassFuncMap, 4, func_map_raw))
        *func_map_raw = 0;  // raw record only
    if (native_lookup_raw != nullptr &&
        !read_bytes(spans, span_count, obj + kUClassNativeFuncLookup, 4,
                    native_lookup_raw))
        *native_lookup_raw = 0;
    return RCode::OK;
}

}  // namespace

// ---------------------------------------------------------------------------

const char* rcode_string(RCode c) {
    switch (c) {
        case RCode::OK: return "OK";
        case RCode::OBJ_E_GUA_VIEW_INVALID: return "OBJ_E_GUA_VIEW_INVALID";
        case RCode::OBJ_E_WALK_TRUNCATED: return "OBJ_E_WALK_TRUNCATED";
        case RCode::OBJ_E_NO_CANDIDATES: return "OBJ_E_NO_CANDIDATES";
        case RCode::OBJ_E_ITEM_UNREADABLE: return "OBJ_E_ITEM_UNREADABLE";
        case RCode::OBJ_R_VPTR_NONCANONICAL: return "OBJ_R_VPTR_NONCANONICAL";
        case RCode::OBJ_R_VPTR_UNREADABLE: return "OBJ_R_VPTR_UNREADABLE";
        case RCode::OBJ_R_FLAGS_MASK: return "OBJ_R_FLAGS_MASK";
        case RCode::OBJ_R_INDEX_BOUNDS: return "OBJ_R_INDEX_BOUNDS";
        case RCode::OBJ_R_INDEX_ROUNDTRIP: return "OBJ_R_INDEX_ROUNDTRIP";
        case RCode::OBJ_R_OUTER_BAD: return "OBJ_R_OUTER_BAD";
        case RCode::OBJ_R_CLASSPTR_BAD: return "OBJ_R_CLASSPTR_BAD";
        case RCode::STR_R_SUPER_CYCLE: return "STR_R_SUPER_CYCLE";
        case RCode::STR_R_SUPER_BAD: return "STR_R_SUPER_BAD";
        case RCode::STR_R_CHILDREN_BAD: return "STR_R_CHILDREN_BAD";
        case RCode::STR_R_CHILDPROPS_BAD: return "STR_R_CHILDPROPS_BAD";
        case RCode::STR_E_STRUCT_WINDOW_UNREADABLE:
            return "STR_E_STRUCT_WINDOW_UNREADABLE";
        case RCode::STR_E_NONE_FOUND: return "STR_E_NONE_FOUND";
        case RCode::STR_E_UCLASS_WINDOW_UNREADABLE:
            return "STR_E_UCLASS_WINDOW_UNREADABLE";
        case RCode::STR_I_UCLASS_EXT_OK: return "STR_I_UCLASS_EXT_OK";
        case RCode::FN_E_UNREADABLE: return "FN_E_UNREADABLE";
        case RCode::FN_I_FUNC_READABLE: return "FN_I_FUNC_READABLE";
        case RCode::FN_I_FUNC_NULL: return "FN_I_FUNC_NULL";
        case RCode::FN_N_FUNC_BAD: return "FN_N_FUNC_BAD";
        case RCode::FN_I_FLAGS_IN_MASK: return "FN_I_FLAGS_IN_MASK";
        case RCode::FN_N_FLAGS_UNDEFINED_ONLY: return "FN_N_FLAGS_UNDEFINED_ONLY";
        case RCode::FN_I_PARM_CONSISTENT: return "FN_I_PARM_CONSISTENT";
        case RCode::FN_N_PARM_IMPLAUSIBLE: return "FN_N_PARM_IMPLAUSIBLE";
        case RCode::FN_I_LINKPTRS_OK: return "FN_I_LINKPTRS_OK";
        case RCode::FN_N_LINKPTR_BAD: return "FN_N_LINKPTR_BAD";
        case RCode::FLD_E_ANCHOR_UNREADABLE: return "FLD_E_ANCHOR_UNREADABLE";
        case RCode::FLD_H5_R_CLASS_NULL: return "FLD_H5_R_CLASS_NULL";
        case RCode::FLD_H5_R_CLASS_BAD: return "FLD_H5_R_CLASS_BAD";
        case RCode::FLD_H5_R_OWNER_BAD: return "FLD_H5_R_OWNER_BAD";
        case RCode::FLD_H5_R_NEXT_BAD: return "FLD_H5_R_NEXT_BAD";
        case RCode::FLD_H5_R_ARRAYDIM_ZERO: return "FLD_H5_R_ARRAYDIM_ZERO";
        case RCode::FLD_H5_R_LINK_BAD: return "FLD_H5_R_LINK_BAD";
        case RCode::FLD_H4_R_VPTR_BAD: return "FLD_H4_R_VPTR_BAD";
        case RCode::FLD_H4_R_FLAGS_MASK: return "FLD_H4_R_FLAGS_MASK";
        case RCode::FLD_H4_R_INDEX: return "FLD_H4_R_INDEX";
        case RCode::FLD_H4_R_CLASS_BAD: return "FLD_H4_R_CLASS_BAD";
        case RCode::FLD_H4_R_OUTER_BAD: return "FLD_H4_R_OUTER_BAD";
        case RCode::FLD_H4_R_LINK_BAD: return "FLD_H4_R_LINK_BAD";
        case RCode::FLD_E_AMBIGUOUS: return "FLD_E_AMBIGUOUS";
        case RCode::FLD_E_NO_GUA_EVIDENCE: return "FLD_E_NO_GUA_EVIDENCE";
        case RCode::FLD_I_H4_ROUNDTRIP_MISS: return "FLD_I_H4_ROUNDTRIP_MISS";
        case RCode::FLD_I_H4_ROUNDTRIP_HIT: return "FLD_I_H4_ROUNDTRIP_HIT";
        default: return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// UStruct/UClass chain probe
// ---------------------------------------------------------------------------

StructChainResult probe_ustruct_chain(const MemSpan* spans, size_t span_count,
                                      const ValidatedGuaView& gua) noexcept {
    StructChainResult res{};
    res.verdict = Verdict::NotProbed;

    if (!gua.valid()) {
        res.verdict = Verdict::Inconclusive;
        res.codes[res.code_count++] = RCode::OBJ_E_GUA_VIEW_INVALID;
        return res;
    }

    const uint32_t to_walk =
        (uint32_t)gua.num_elements < kMaxObjectsWalked
            ? (uint32_t)gua.num_elements
            : kMaxObjectsWalked;
    res.walk_truncated = (uint32_t)gua.num_elements > kMaxObjectsWalked;

    bool saw_base_contradiction = false;  // UObjectBase/FUObjectItem layer
    bool saw_plausible = false;

    for (uint32_t i = 0; i < to_walk; ++i) {
        uint64_t it = 0;
        if (!item_addr(spans, span_count, gua, i, &it)) continue;
        uint64_t obj = 0;
        if (!read_bytes(spans, span_count, it + kItemObject, 8, &obj)) continue;
        if (obj == 0) continue;  // unpopulated slot (GC etc.) — skip, count not
        ++res.items_examined;

        bool rt_ok = false;
        const RCode base_code =
            classify_uobjectbase(spans, span_count, gua, obj, &rt_ok);
        if (base_code != RCode::OK) {
            // OBJ_E_ITEM_UNREADABLE is evidence-unavailable, not contradiction.
            ++res.implausible_uobject;
            if (base_code != RCode::OBJ_E_ITEM_UNREADABLE) {
                push_code(res.codes, res.code_count, base_code);
                saw_base_contradiction = true;
            }
            continue;
        }
        ++res.plausible_uobject;
        if (rt_ok) ++res.roundtrips_ok;
        saw_plausible = true;
        if (res.first_plausible == 0) res.first_plausible = obj;

        const RCode st = classify_ustruct_like(spans, span_count, obj);
        if (st == RCode::OK) {
            ++res.struct_like_count;
            if (res.first_ustruct_like == 0) res.first_ustruct_like = obj;
            uint32_t fm = 0, nl = 0;
            if (classify_uclass_ext(spans, span_count, obj, &fm, &nl) ==
                RCode::OK) {
                if (res.first_uclass_like == 0) {
                    res.first_uclass_like = obj;
                    push_code(res.codes, res.code_count, RCode::STR_I_UCLASS_EXT_OK);
                }
            }
        } else if (st != RCode::STR_E_STRUCT_WINDOW_UNREADABLE) {
            // Struct-region contradictions are recorded as per-object negative
            // evidence only: without FName/type identification (P2 gap) a
            // non-struct UObject's bytes in the UStruct region are foreign
            // layout, NOT a refutation of the struct family.
            push_code(res.codes, res.code_count, st);
        }
    }

    // Combine honestly:
    //  Validated   >= 1 object passed the FULL chain (base + struct-like);
    //                existence proof for the UStruct shape in this snapshot.
    //  Refuted     the rung-2-validated UObjectBase/FUObjectItem layer is
    //                contradicted (vptr/flags/index-round-trip/outer/class
    //                failed on a read item) and no struct-like was found.
    //  Inconclusive otherwise (no candidates / windows unreadable / walk
    //                truncated at the 64-object cap before any struct).
    if (res.struct_like_count > 0) {
        res.verdict = Verdict::Validated;
        push_code(res.codes, res.code_count, RCode::OK);
    } else if (saw_base_contradiction) {
        res.verdict = Verdict::Refuted;
    } else {
        res.verdict = Verdict::Inconclusive;
        if (res.items_examined == 0)
            push_code(res.codes, res.code_count, RCode::OBJ_E_NO_CANDIDATES);
        else
            push_code(res.codes, res.code_count, RCode::STR_E_NONE_FOUND);
        if (res.walk_truncated)
            push_code(res.codes, res.code_count, RCode::OBJ_E_WALK_TRUNCATED);
    }
    (void)saw_plausible;
    return res;
}

// ---------------------------------------------------------------------------
// UFunction classifier (Inconclusive-only by design)
// ---------------------------------------------------------------------------

UFunctionProbeResult probe_ufunction(const MemSpan* spans, size_t span_count,
                                     uint64_t function_addr) noexcept {
    UFunctionProbeResult res{};
    res.verdict = Verdict::Inconclusive;  // by design, always

    uint8_t win[kUFunctionTotalSize];
    if (!read_bytes(spans, span_count, function_addr, sizeof(win), &win)) {
        res.codes[res.code_count++] = RCode::FN_E_UNREADABLE;
        return res;
    }

    // Func pointer: low 8 bytes at +0xD8 under both width alternatives.
    res.func_ptr = rd_u64(win + kUFunctionFunc);
    if (res.func_ptr == 0) {
        res.codes[res.code_count++] = RCode::FN_I_FUNC_NULL;
    } else if (pointer_canonical_or_null(res.func_ptr) &&
               range_readable(spans, span_count, res.func_ptr, 8)) {
        res.codes[res.code_count++] = RCode::FN_I_FUNC_READABLE;
    } else {
        res.codes[res.code_count++] = RCode::FN_N_FUNC_BAD;
    }

    // FunctionFlags mask plausibility (u32; width structurally bounded to 4
    // by compiled-width NumParms@0xB4; mask cites the enum holes 0x10/0x20).
    res.function_flags = rd_u32(win + kUFunctionFlags);
    if (res.function_flags != 0 &&
        (res.function_flags & ~kFuncUndefinedBits) == 0) {
        res.codes[res.code_count++] = RCode::FN_N_FLAGS_UNDEFINED_ONLY;
    } else {
        res.codes[res.code_count++] = RCode::FN_I_FLAGS_IN_MASK;
    }

    // Parm coherence (classifier knowledge, labeled in header; never Refute).
    res.num_parms = win[kUFunctionNumParms];
    res.parms_size = rd_u16(win + kUFunctionParmsSize);
    res.retval_offset = rd_u16(win + kUFunctionRetValOffset);
    const bool parm_coherent =
        !(res.num_parms == 0 && res.parms_size != 0) &&
        res.retval_offset <= res.parms_size;
    res.codes[res.code_count++] =
        parm_coherent ? RCode::FN_I_PARM_CONSISTENT : RCode::FN_N_PARM_IMPLAUSIBLE;

    // Optional link pointers null-or-readable.
    const uint64_t fpti = rd_u64(win + kUFunctionFirstPropToInit);
    const uint64_t egf = rd_u64(win + kUFunctionEventGraphFn);
    const int c1 = link_ptr_check(spans, span_count, fpti);
    const int c2 = link_ptr_check(spans, span_count, egf);
    res.codes[res.code_count++] =
        (c1 == 0 && c2 == 0) ? RCode::FN_I_LINKPTRS_OK : RCode::FN_N_LINKPTR_BAD;

    return res;
}

// ---------------------------------------------------------------------------
// FField variant discriminator
// ---------------------------------------------------------------------------

namespace {

struct Hyp {
    Verdict verdict;
    RCode code;
};

// UE5 hypothesis: anchor is an FField (5_06 FField header + FProperty payload).
Hyp run_h5(const MemSpan* spans, size_t span_count, const uint8_t* win) {
    Hyp h{Verdict::Refuted, RCode::OK};
    const uint64_t classp = rd_u64(win + kFFieldClassPrivate);
    if (classp == 0) { h.code = RCode::FLD_H5_R_CLASS_NULL; return h; }
    if (!pointer_canonical_or_null(classp) ||
        !range_readable(spans, span_count, classp, 8)) {
        h.code = RCode::FLD_H5_R_CLASS_BAD; return h;
    }
    if (link_ptr_check(spans, span_count, rd_u64(win + kFFieldOwner)) != 0) {
        h.code = RCode::FLD_H5_R_OWNER_BAD; return h;
    }
    if (link_ptr_check(spans, span_count, rd_u64(win + kFFieldNext)) != 0) {
        h.code = RCode::FLD_H5_R_NEXT_BAD; return h;
    }
    const uint32_t array_dim = rd_u32(win + kFPropArrayDim);
    if (array_dim == 0) { h.code = RCode::FLD_H5_R_ARRAYDIM_ZERO; return h; }
    // payload link pointers 0x48/0x50/0x58/0x60 (SET-5_06)
    const size_t links5[4] = {kFPropLinkNextUe5, kFPropNextRefUe5,
                              kFPropDtorLinkUe5, kFPropPostCtorLinkUe5};
    for (size_t l : links5) {
        if (link_ptr_check(spans, span_count, rd_u64(win + l)) != 0) {
            h.code = RCode::FLD_H5_R_LINK_BAD; return h;
        }
    }
    // FlagsPrivate/NamePrivate/RepNotifyFunc: raw record only (P2 / no mask).
    h.verdict = Verdict::Validated;
    h.code = RCode::OK;
    return h;
}

// UE4 hypothesis: anchor is a UObject (4_07 UObjectBase + UProperty payload).
Hyp run_h4(const MemSpan* spans, size_t span_count, const uint8_t* win,
           const ValidatedGuaView* gua, bool* roundtrip_hit,
           bool* roundtrip_attempted) {
    Hyp h{Verdict::Refuted, RCode::OK};
    *roundtrip_hit = false;
    *roundtrip_attempted = false;
    const uint64_t vptr = rd_u64(win + 0);
    if (vptr == 0 || !pointer_canonical_or_null(vptr) ||
        !range_readable(spans, span_count, vptr, 8)) {
        h.code = RCode::FLD_H4_R_VPTR_BAD; return h;
    }
    if (rd_u32(win + kObjObjectFlags) & ~kRfAllFlags) {
        h.code = RCode::FLD_H4_R_FLAGS_MASK; return h;
    }
    const uint32_t idx = rd_u32(win + kObjInternalIndex);
    if (gua != nullptr && gua->valid()) {
        if ((int32_t)idx < 0 || (int32_t)idx >= gua->num_elements) {
            h.code = RCode::FLD_H4_R_INDEX; return h;
        }
        // Round-trip itself needs the anchor address; performed by the caller
        // (probe_ffield_variant) once this hypothesis survives structurally.
        *roundtrip_attempted = true;
    }
    const uint64_t classp = rd_u64(win + kObjClassPrivate);
    if (classp == 0 || !pointer_canonical_or_null(classp) ||
        !range_readable(spans, span_count, classp, 8)) {
        h.code = RCode::FLD_H4_R_CLASS_BAD; return h;
    }
    if (link_ptr_check(spans, span_count, rd_u64(win + kObjOuterPrivate)) != 0) {
        h.code = RCode::FLD_H4_R_OUTER_BAD; return h;
    }
    // payload link pointers 0x50/0x58/0x60/0x68 (SET-4_07)
    const size_t links4[4] = {kFPropLinkNextUe4, kFPropNextRefUe4,
                              kFPropDtorLinkUe4, kFPropPostCtorLinkUe4};
    for (size_t l : links4) {
        if (link_ptr_check(spans, span_count, rd_u64(win + l)) != 0) {
            h.code = RCode::FLD_H4_R_LINK_BAD; return h;
        }
    }
    h.verdict = Verdict::Validated;
    h.code = RCode::OK;
    return h;
}

}  // namespace

FieldVariantResult probe_ffield_variant(const MemSpan* spans, size_t span_count,
                                        uint64_t property_addr,
                                        const ValidatedGuaView* gua) noexcept {
    FieldVariantResult res{};
    res.verdict = Verdict::NotProbed;
    res.variant = FieldVariant::None;
    res.h5_verdict = Verdict::NotProbed;
    res.h4_verdict = Verdict::NotProbed;
    res.settling_evidence = nullptr;

    uint8_t win[kFPropTotalSize];
    if (!read_bytes(spans, span_count, property_addr, sizeof(win), &win)) {
        res.verdict = Verdict::Inconclusive;  // evidence unavailable
        res.codes[res.code_count++] = RCode::FLD_E_ANCHOR_UNREADABLE;
        res.h5_verdict = Verdict::Inconclusive;
        res.h4_verdict = Verdict::Inconclusive;
        return res;
    }

    // raw observations (FName fields never interpreted)
    res.h5_class_private = rd_u64(win + kFFieldClassPrivate);
    res.h5_owner = rd_u64(win + kFFieldOwner);
    res.h5_next = rd_u64(win + kFFieldNext);
    res.h5_flags_private = rd_u32(win + kFFieldFlagsPrivate);
    res.h5_array_dim = rd_u32(win + kFPropArrayDim);
    res.h5_element_size = rd_u32(win + kFPropElementSize);
    res.h4_vptr = rd_u64(win + 0);
    res.h4_object_flags = rd_u32(win + kObjObjectFlags);
    res.h4_internal_index = rd_u32(win + kObjInternalIndex);
    res.h4_class_private = rd_u64(win + kObjClassPrivate);
    res.h4_outer = rd_u64(win + kObjOuterPrivate);
    res.h4_offset_internal = rd_u32(win + kFPropOffsetInternalUe4);

    Hyp h5 = run_h5(spans, span_count, win);
    bool rt_hit = false, rt_attempted = false;
    Hyp h4 = run_h4(spans, span_count, win, gua, &rt_hit, &rt_attempted);

    // H4 round-trip resolved here (needs the anchor address). A SUCCESSFUL
    // read that maps elsewhere refutes H4 (a UE4 UProperty must be tracked);
    // an UNREADABLE item is evidence-unavailable and leaves H4 standing.
    bool rt_done = false;
    if (h4.verdict == Verdict::Validated && rt_attempted) {
        uint64_t it = 0;
        if (item_addr(spans, span_count, *gua, res.h4_internal_index, &it)) {
            uint64_t back = 0;
            if (read_bytes(spans, span_count, it + kItemObject, 8, &back)) {
                rt_done = true;
                rt_hit = (back == property_addr);
                if (!rt_hit) {
                    h4.verdict = Verdict::Refuted;
                    h4.code = RCode::FLD_H4_R_INDEX;
                }
            }
        }
    }

    res.h5_verdict = h5.verdict;
    res.h5_code = h5.code;
    res.h4_verdict = h4.verdict;
    res.h4_code = h4.code;

    const bool h5_v = h5.verdict == Verdict::Validated;
    const bool h4_v = h4.verdict == Verdict::Validated;

    if (h5_v && !h4_v) {
        res.verdict = Verdict::Validated;
        res.variant = FieldVariant::UE5_FField;
        res.codes[res.code_count++] = RCode::OK;
    } else if (h4_v && !h5_v) {
        res.verdict = Verdict::Validated;
        res.variant = FieldVariant::UE4_UProperty;
        res.codes[res.code_count++] = RCode::OK;
    } else if (h5_v && h4_v) {
        res.verdict = Verdict::Inconclusive;
        res.codes[res.code_count++] = RCode::FLD_E_AMBIGUOUS;
        res.settling_evidence =
            "Need ONE of: (a) FName storage width (compiled accessor entry; "
            "WIDTHS coverage.json FName 0/0 today) to test NamePrivate@+0x20 "
            "as UE5 FName vs UE4 OuterPrivate UObject* round-trip into "
            "GUObjectArray; (b) compiled width for FProperty::PropertyFlags@"
            "0x38 (none today) plus its flag-mask semantics; (c) a longer "
            "GUObjectArray containment scan than the 64-object cap proving "
            "the anchor is not a tracked UObject.";
    } else {
        res.verdict = Verdict::Refuted;
        push_code(res.codes, res.code_count, h5.code);
        push_code(res.codes, res.code_count, h4.code);
    }

    if (gua == nullptr || !gua->valid() || (rt_attempted && !rt_done)) {
        push_code(res.codes, res.code_count, RCode::FLD_E_NO_GUA_EVIDENCE);
    } else if (rt_done) {
        // containment support codes (classifier-class only)
        push_code(res.codes, res.code_count,
                  rt_hit ? RCode::FLD_I_H4_ROUNDTRIP_HIT
                         : RCode::FLD_I_H4_ROUNDTRIP_MISS);
    }
    return res;
}

FieldVariantResult probe_ustruct_child_variant(const MemSpan* spans,
                                               size_t span_count,
                                               uint64_t struct_addr,
                                               const ValidatedGuaView* gua) noexcept {
    uint8_t win[0x58];  // Children@0x48 / ChildProperties@0x50
    if (!read_bytes(spans, span_count, struct_addr, sizeof(win), &win)) {
        FieldVariantResult res{};
        res.verdict = Verdict::Inconclusive;
        res.h5_verdict = Verdict::Inconclusive;
        res.h4_verdict = Verdict::Inconclusive;
        res.codes[res.code_count++] = RCode::FLD_E_ANCHOR_UNREADABLE;
        return res;
    }
    // ChildProperties head is the UE5 FField list; Children head is the
    // UField/UObject list. Test the ChildProperties head first (engine is
    // validated-static 5.6, so UE5 is the expected winner); fall back to the
    // Children head when ChildProperties is null.
    const uint64_t cprops = rd_u64(win + kUStructChildProperties);
    const uint64_t children = rd_u64(win + kUStructChildren);
    if (cprops != 0)
        return probe_ffield_variant(spans, span_count, cprops, gua);
    if (children != 0)
        return probe_ffield_variant(spans, span_count, children, gua);
    FieldVariantResult res{};
    res.verdict = Verdict::Inconclusive;  // no child anchor to test
    res.h5_verdict = Verdict::Inconclusive;
    res.h4_verdict = Verdict::Inconclusive;
    res.codes[res.code_count++] = RCode::OBJ_E_NO_CANDIDATES;
    res.settling_evidence =
        "Struct has both Children and ChildProperties null: pick a struct "
        "with reflected members, or supply the property anchor directly.";
    return res;
}

}  // namespace rfl
