// townfall_layout_probes.cpp
//
// Implementation notes
// --------------------
// * All integer decoding is explicit little-endian byte composition (the
//   profile pins the image as ELF64 x86-64 LE; nothing depends on host
//   endianness).
// * Both GUObjectArray layout hypotheses are evaluated independently; the
//   combined verdict only claims Validated when exactly one hypothesis
//   survives the whole predicate chain.
// * Cited offsets live in the header provenance block; they are repeated
//   below as named constants only.

#include "townfall_layout_probes.hpp"

namespace tlp {
namespace {

// --- cited TUObjectArray member offsets (5_00..5_06 setters, identical) ---
constexpr size_t kOffObjects = 0x0;              // FUObjectItem***
constexpr size_t kOffPreAllocatedObjects = 0x8;  // FUObjectItem**
constexpr size_t kOffMaxElements = 0x10;         // int32
constexpr size_t kOffNumElements = 0x14;         // int32
constexpr size_t kOffMaxChunks = 0x18;           // int32
constexpr size_t kOffNumChunks = 0x1C;           // int32
// (TUObjectArray UEP_TotalSize 0x20 is implicit in the 0x20/0x30 header reads)

// --- cited FUObjectArray outer offsets (5_00..5_06 setters) ---
constexpr size_t kOffObjFirstGCIndex = 0x0;      // int32
constexpr size_t kOffObjLastNonGCIndex = 0x4;    // int32
constexpr size_t kOffObjObjects = 0x10;          // embedded TUObjectArray

inline uint32_t rd_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
inline uint64_t rd_u64(const uint8_t* p) {
    return (uint64_t)rd_u32(p) | ((uint64_t)rd_u32(p + 4) << 32);
}

struct ChainOut {
    Verdict verdict;
    RCode code;
    GuaObserved observed{};
};

void finish_validated(ChainOut& out, const uint8_t* hdr, size_t tua_off,
                      GuaLayout layout, const uint64_t* scanned,
                      uint32_t scanned_count, uint32_t needed_chunks,
                      bool scan_complete) {
    out.verdict = (scan_complete && scanned_count >= needed_chunks)
                      ? Verdict::Validated
                      : Verdict::Inconclusive;
    out.code = (out.verdict == Verdict::Validated)
                   ? RCode::OK
                   : RCode::GOA_E_CHUNK_EXTENT_UNVERIFIABLE;
    out.observed.layout_matched = layout;
    out.observed.obj_first_gc_index =
        (layout == GuaLayout::FUObjectArray_Outer)
            ? (int32_t)rd_u32(hdr + kOffObjFirstGCIndex) : -1;
    out.observed.obj_last_non_gc_index =
        (layout == GuaLayout::FUObjectArray_Outer)
            ? (int32_t)rd_u32(hdr + kOffObjLastNonGCIndex) : -1;
    const uint8_t* tua = hdr + tua_off;
    out.observed.max_elements = (int32_t)rd_u32(tua + kOffMaxElements);
    out.observed.num_elements = (int32_t)rd_u32(tua + kOffNumElements);
    out.observed.max_chunks = (int32_t)rd_u32(tua + kOffMaxChunks);
    out.observed.num_chunks = (int32_t)rd_u32(tua + kOffNumChunks);
    out.observed.objects_ptr = rd_u64(tua + kOffObjects);
    out.observed.preallocated_objects_ptr = rd_u64(tua + kOffPreAllocatedObjects);
    out.observed.chunk0_ptr = scanned_count > 0 ? scanned[0] : 0;
    out.observed.chunk1_ptr = scanned_count > 1 ? scanned[1] : 0;
    out.observed.chunks_scanned = scanned_count;
}

// One layout hypothesis' full predicate chain over the header bytes.
// tua_off: 0x10 for layout A (embedded ObjObjects), 0x0 for layout B.
ChainOut run_chain(const MemSpan* spans, size_t span_count, const uint8_t* hdr,
                   size_t tua_off, bool is_layout_a, GuaLayout layout) {
    ChainOut out{};
    out.verdict = Verdict::NotProbed;
    out.code = RCode::OK;

    const uint8_t* tua = hdr + tua_off;

    // P1: pointer fields canonical and inside readable ranges.
    const uint64_t objects = rd_u64(tua + kOffObjects);
    const uint64_t prealloc = rd_u64(tua + kOffPreAllocatedObjects);

    if (!pointer_canonical_or_null(objects) ||
        !pointer_canonical_or_null(prealloc)) {
        out.verdict = Verdict::Refuted;
        out.code = RCode::GOA_R_PTR_NONCANONICAL;
        return out;
    }
    if (objects == 0 && prealloc == 0) {
        out.verdict = Verdict::Refuted;
        out.code = RCode::GOA_R_PTRS_BOTH_NULL;
        return out;
    }
    if (objects != 0 && !range_readable(spans, span_count, objects, 8)) {
        out.verdict = Verdict::Refuted;
        out.code = RCode::GOA_R_PTR_OUTSIDE_READABLE;
        return out;
    }
    if (prealloc != 0 && !range_readable(spans, span_count, prealloc, 8)) {
        out.verdict = Verdict::Refuted;
        out.code = RCode::GOA_R_PTR_OUTSIDE_READABLE;
        return out;
    }

    // P2: Num/Max/chunk-count sanity bounds (cited constants).
    const int32_t max_elements = (int32_t)rd_u32(tua + kOffMaxElements);
    const int32_t num_elements = (int32_t)rd_u32(tua + kOffNumElements);
    const int32_t max_chunks = (int32_t)rd_u32(tua + kOffMaxChunks);
    const int32_t num_chunks = (int32_t)rd_u32(tua + kOffNumChunks);

    const bool counts_ok =
        num_elements > 0 && num_elements <= max_elements &&
        max_chunks >= 1 && max_chunks <= kMaxChunksBound &&
        num_chunks >= 1 && num_chunks <= max_chunks &&
        (int64_t)max_elements <= (int64_t)max_chunks * kNumElementsPerChunk;
    if (!counts_ok) {
        out.verdict = Verdict::Refuted;
        out.code = RCode::GOA_R_COUNTS_INSANE;
        return out;
    }

    // P3: GC index ordering (layout A only; layout B has no such fields).
    // UE invariant (FUObjectArray): permanent objects occupy indices
    // [0, ObjLastNonGCIndex]; GC-visible objects start at ObjFirstGCIndex ==
    // ObjLastNonGCIndex + 1 in the settled state. A gap
    // (ObjFirstGCIndex > ObjLastNonGCIndex + 1) is legal transiently while
    // GC mutates the pool. The previous `first_gc <= last_non_gc` form
    // inverted the invariant and refuted healthy arrays (live data:
    // first=35477, last=35476 at 0x97ba110).
    if (is_layout_a) {
        const int32_t first_gc = (int32_t)rd_u32(hdr + kOffObjFirstGCIndex);
        const int32_t last_non_gc =
            (int32_t)rd_u32(hdr + kOffObjLastNonGCIndex);
        if (!(first_gc >= 0 && first_gc > last_non_gc &&
              last_non_gc >= -1 && last_non_gc <= max_elements)) {
            out.verdict = Verdict::Refuted;
            out.code = RCode::GOA_R_GC_ORDER;
            return out;
        }
    }

    // P4: cross-check NumElements against the chunk array.
    const uint32_t needed_chunks =
        (uint32_t)((num_elements + kNumElementsPerChunk - 1) / kNumElementsPerChunk);
    if ((uint32_t)num_chunks < needed_chunks) {
        out.verdict = Verdict::Refuted;
        out.code = RCode::GOA_R_CAPACITY_MISMATCH;
        return out;
    }

    const uint32_t to_scan =
        (uint32_t)num_chunks < kMaxChunkScan ? (uint32_t)num_chunks : kMaxChunkScan;
    const bool scan_complete = ((uint32_t)num_chunks <= kMaxChunkScan);

    // P5: needed chunk pointers readable, distinct, non-null, non-overlapping.
    uint64_t scanned[kMaxChunkScan];
    uint64_t scanned_extent[kMaxChunkScan];  // needed bytes per scanned chunk
    uint32_t scanned_needed = 0;
    for (uint32_t i = 0; i < to_scan; ++i) {
        uint64_t chunk_ptr = 0;
        if (!read_bytes(spans, span_count, objects + (uint64_t)i * 8, 8,
                        &chunk_ptr)) {
            // The chunk-pointer array's readable extent does not cover all
            // allocated chunks: evidence unavailable, not a contradiction.
            out.verdict = Verdict::Inconclusive;
            out.code = RCode::GOA_E_CHUNK_EXTENT_UNVERIFIABLE;
            return out;
        }
        const bool needed = i < needed_chunks;
        if (chunk_ptr == 0) {
            if (needed) {
                out.verdict = Verdict::Refuted;
                out.code = RCode::GOA_R_CHUNK_NULL_WHILE_NEEDED;
                return out;
            }
            scanned[i] = 0;
            scanned_extent[i] = 0;
            continue;
        }
        if (!pointer_canonical_or_null(chunk_ptr)) {
            out.verdict = Verdict::Refuted;
            out.code = RCode::GOA_R_PTR_NONCANONICAL;
            return out;
        }
        const int64_t rem = (int64_t)num_elements - (int64_t)i * kNumElementsPerChunk;
        const int64_t items = rem < kNumElementsPerChunk ? rem : kNumElementsPerChunk;
        const uint64_t need_bytes = (uint64_t)items * kFuObjectItemSize;
        if (!range_readable(spans, span_count, chunk_ptr, need_bytes)) {
            if (!range_readable(spans, span_count, chunk_ptr, 8)) {
                // The chunk pointer itself points at no readable byte at all.
                out.verdict = Verdict::Refuted;
                out.code = RCode::GOA_R_CHUNK_UNREADABLE;
                return out;
            }
            out.verdict = Verdict::Inconclusive;
            out.code = RCode::GOA_E_CHUNK_EXTENT_UNVERIFIABLE;
            return out;
        }
        scanned[i] = chunk_ptr;
        scanned_extent[i] = need_bytes;
        if (needed) {
            // pairwise against earlier NEEDED chunks only
            for (uint32_t j = 0; j < scanned_needed; ++j) {
                if (scanned[j] == chunk_ptr) {
                    out.verdict = Verdict::Refuted;
                    out.code = RCode::GOA_R_CHUNK_DUPLICATE;
                    return out;
                }
                const bool overlap =
                    (scanned[j] < chunk_ptr + need_bytes) &&
                    (chunk_ptr < scanned[j] + scanned_extent[j]);
                if (overlap) {
                    out.verdict = Verdict::Refuted;
                    out.code = RCode::GOA_R_CHUNK_OVERLAP;
                    return out;
                }
            }
            ++scanned_needed;
        }
    }

    finish_validated(out, hdr, tua_off, layout, scanned, to_scan,
                     needed_chunks, scan_complete);
    return out;
}

void push_code(RCode* codes, uint8_t& count, RCode c) {
    if (count >= kMaxRationaleCodes) return;
    for (uint8_t i = 0; i < count; ++i) {
        if (codes[i] == c) return;  // dedup
    }
    codes[count++] = c;
}

}  // namespace

// ---------------------------------------------------------------------------

const char* verdict_string(Verdict v) {
    switch (v) {
        case Verdict::Validated: return "Validated";
        case Verdict::Inconclusive: return "Inconclusive";
        case Verdict::Refuted: return "Refuted";
        default: return "NotProbed";
    }
}

const char* rcode_string(RCode c) {
    switch (c) {
        case RCode::OK: return "OK";
        case RCode::GOA_E_ADDR_UNREADABLE: return "GOA_E_ADDR_UNREADABLE";
        case RCode::GOA_E_CHUNK_EXTENT_UNVERIFIABLE:
            return "GOA_E_CHUNK_EXTENT_UNVERIFIABLE";
        case RCode::GOA_E_LAYOUT_AMBIGUOUS: return "GOA_E_LAYOUT_AMBIGUOUS";
        case RCode::GOA_R_PTR_NONCANONICAL: return "GOA_R_PTR_NONCANONICAL";
        case RCode::GOA_R_PTR_OUTSIDE_READABLE: return "GOA_R_PTR_OUTSIDE_READABLE";
        case RCode::GOA_R_PTRS_BOTH_NULL: return "GOA_R_PTRS_BOTH_NULL";
        case RCode::GOA_R_COUNTS_INSANE: return "GOA_R_COUNTS_INSANE";
        case RCode::GOA_R_GC_ORDER: return "GOA_R_GC_ORDER";
        case RCode::GOA_R_CHUNK_NULL_WHILE_NEEDED:
            return "GOA_R_CHUNK_NULL_WHILE_NEEDED";
        case RCode::GOA_R_CHUNK_UNREADABLE: return "GOA_R_CHUNK_UNREADABLE";
        case RCode::GOA_R_CHUNK_DUPLICATE: return "GOA_R_CHUNK_DUPLICATE";
        case RCode::GOA_R_CHUNK_OVERLAP: return "GOA_R_CHUNK_OVERLAP";
        case RCode::GOA_R_CAPACITY_MISMATCH: return "GOA_R_CAPACITY_MISMATCH";
        case RCode::CODE_E_UNREADABLE: return "CODE_E_UNREADABLE";
        case RCode::CODE_I_PROLOGUE_LIKE: return "CODE_I_PROLOGUE_LIKE";
        case RCode::CODE_I_NOT_PROLOGUE_LIKE: return "CODE_I_NOT_PROLOGUE_LIKE";
        case RCode::CODE_I_PRE_PADDING: return "CODE_I_PRE_PADDING";
        default: return "UNKNOWN";
    }
}

bool read_bytes(const MemSpan* spans, size_t span_count, uint64_t addr,
                size_t len, void* out) noexcept {
    if (spans == nullptr || out == nullptr) return false;
    for (size_t i = 0; i < span_count; ++i) {
        const MemSpan& s = spans[i];
        if (s.data == nullptr) continue;
        if (addr >= s.start && len <= s.size && (addr - s.start) <= s.size - len) {
            const uint8_t* src = s.data + (addr - s.start);
            uint8_t* dst = static_cast<uint8_t*>(out);
            for (size_t k = 0; k < len; ++k) dst[k] = src[k];
            return true;
        }
    }
    return false;
}

bool range_readable(const MemSpan* spans, size_t span_count, uint64_t addr,
                    size_t len) noexcept {
    if (spans == nullptr) return false;
    for (size_t i = 0; i < span_count; ++i) {
        const MemSpan& s = spans[i];
        if (addr >= s.start && len <= s.size && (addr - s.start) <= s.size - len) {
            return true;
        }
    }
    return false;
}

bool pointer_canonical_or_null(uint64_t p) noexcept {
    if (p == 0) return true;
    if (p & 0x7) return false;  // 8-byte alignment for pointer-array storage
    const uint64_t upper = p >> 47;
    return upper == 0 || upper == 0x1FFFF;  // canonical 48-bit form
}

GuaProbeResult probe_guobject_array(const MemSpan* spans, size_t span_count,
                                    uint64_t image_base,
                                    uint64_t goa_vaddr) noexcept {
    GuaProbeResult res{};
    res.verdict = Verdict::NotProbed;
    res.layout_a.verdict = Verdict::NotProbed;
    res.layout_b.verdict = Verdict::NotProbed;

    // Resolve label address with overflow check (base + file-image vaddr).
    if (image_base > UINT64_MAX - goa_vaddr) {
        res.verdict = Verdict::Inconclusive;
        res.code_count = 1;
        res.codes[0] = RCode::GOA_E_ADDR_UNREADABLE;
        res.layout_a.verdict = Verdict::Inconclusive;
        res.layout_a.code_count = 1;
        res.layout_a.codes[0] = RCode::GOA_E_ADDR_UNREADABLE;
        res.layout_b = res.layout_a;
        return res;
    }
    const uint64_t addr = image_base + goa_vaddr;

    // Header bytes for each hypothesis (A reads GC fields + embedded array =
    // 0x30 bytes; B reads a bare TUObjectArray = 0x20 bytes).
    uint8_t hdr_a[0x30];
    uint8_t hdr_b[0x20];

    ChainOut a{};
    a.verdict = Verdict::Inconclusive;
    a.code = RCode::GOA_E_ADDR_UNREADABLE;
    if (read_bytes(spans, span_count, addr, sizeof(hdr_a), hdr_a)) {
        a = run_chain(spans, span_count, hdr_a, kOffObjObjects,
                      /*is_layout_a=*/true, GuaLayout::FUObjectArray_Outer);
    }

    ChainOut b{};
    b.verdict = Verdict::Inconclusive;
    b.code = RCode::GOA_E_ADDR_UNREADABLE;
    if (read_bytes(spans, span_count, addr, sizeof(hdr_b), hdr_b)) {
        b = run_chain(spans, span_count, hdr_b, 0x0,
                      /*is_layout_a=*/false, GuaLayout::TUObjectArray_Direct);
    }

    // Record per-layout detail.
    res.layout_a.verdict = a.verdict;
    res.layout_a.code_count = 1;
    res.layout_a.codes[0] = a.code;
    res.layout_b.verdict = b.verdict;
    res.layout_b.code_count = 1;
    res.layout_b.codes[0] = b.code;

    // Combine honestly:
    //  - exactly one Validated -> Validated under that layout;
    //  - both Validated -> Inconclusive (ambiguous);
    //  - both Refuted -> Refuted (label bytes contradict both hypotheses);
    //  - otherwise -> Inconclusive (evidence missing for the live one).
    const bool a_v = a.verdict == Verdict::Validated;
    const bool b_v = b.verdict == Verdict::Validated;
    const bool a_r = a.verdict == Verdict::Refuted;
    const bool b_r = b.verdict == Verdict::Refuted;

    if (a_v && !b_v) {
        res.verdict = Verdict::Validated;
        res.codes[0] = RCode::OK;
        res.code_count = 1;
        res.observed = a.observed;
    } else if (b_v && !a_v) {
        res.verdict = Verdict::Validated;
        res.codes[0] = RCode::OK;
        res.code_count = 1;
        res.observed = b.observed;
    } else if (a_v && b_v) {
        res.verdict = Verdict::Inconclusive;
        res.codes[0] = RCode::GOA_E_LAYOUT_AMBIGUOUS;
        res.code_count = 1;
        // Scalars from hypothesis A are recorded for the controller, but no
        // layout is claimed.
        GuaObserved mixed = a.observed;
        mixed.layout_matched = GuaLayout::None;
        res.observed = mixed;
    } else if (a_r && b_r) {
        res.verdict = Verdict::Refuted;
        push_code(res.codes, res.code_count, a.code);
        push_code(res.codes, res.code_count, b.code);
    } else {
        res.verdict = Verdict::Inconclusive;
        if (a.verdict == Verdict::Inconclusive)
            push_code(res.codes, res.code_count, a.code);
        if (b.verdict == Verdict::Inconclusive)
            push_code(res.codes, res.code_count, b.code);
        if (a_r) push_code(res.codes, res.code_count, a.code);
        if (b_r) push_code(res.codes, res.code_count, b.code);
    }
    return res;
}

CodeProbeResult probe_code_shape(const MemSpan* spans, size_t span_count,
                                 uint64_t image_base, uint64_t vaddr,
                                 const char* /*family_name*/) noexcept {
    CodeProbeResult res{};
    res.verdict = Verdict::Inconclusive;  // by design, always
    res.code_count = 0;

    if (image_base > UINT64_MAX - vaddr) {
        res.codes[res.code_count++] = RCode::CODE_E_UNREADABLE;
        return res;
    }
    const uint64_t addr = image_base + vaddr;

    // Read up to 16 entry bytes (prefix readable within one span).
    uint32_t got = 0;
    for (size_t i = 0; i < span_count && got == 0; ++i) {
        const MemSpan& s = spans[i];
        if (addr >= s.start && addr < s.start + s.size) {
            const size_t avail = (size_t)(s.start + s.size - addr);
            got = avail < 16 ? (uint32_t)avail : 16;
            for (uint32_t k = 0; k < got; ++k)
                res.first_bytes[k] = s.data[(addr - s.start) + k];
        }
    }
    if (got == 0) {
        res.codes[res.code_count++] = RCode::CODE_E_UNREADABLE;
        return res;
    }
    res.bytes_read = (uint8_t)got;

    // Supporting signal only: function-boundary filler immediately before the
    // entry (0xCC int3 / 0x90 nop, standard MSVC/Clang padding shapes).
    if (addr >= 8 && range_readable(spans, span_count, addr - 8, 8)) {
        uint8_t pre[8];
        if (read_bytes(spans, span_count, addr - 8, 8, pre)) {
            bool all_filler = true;
            for (int i = 0; i < 8; ++i) {
                if (pre[i] != 0xCC && pre[i] != 0x90) { all_filler = false; break; }
            }
            if (all_filler) res.codes[res.code_count++] = RCode::CODE_I_PRE_PADDING;
        }
    }

    // Conservative x86-64 frame-setup grammar. Greedy element decode:
    //   endbr64            F3 0F 1E FA
    //   push rbp/rbx/rsi/rdi   53 55 56 57
    //   push r12..r15         41 54..41 57
    //   mov rbp,rsp           48 89 E5 | 48 8B EC
    //   sub rsp,imm8          48 83 EC xx
    //   sub rsp,imm32         48 81 EC xx xx xx xx
    //   mov [rsp+d8],r        48/4C 89 44/4C 24 xx
    const uint8_t* b = res.first_bytes;
    uint32_t i = 0;
    uint32_t elements = 0;
    while (i < got) {
        if (i + 4 <= got && b[i] == 0xF3 && b[i + 1] == 0x0F && b[i + 2] == 0x1E &&
            b[i + 3] == 0xFA) { i += 4; ++elements; continue; }
        if (b[i] == 0x53 || b[i] == 0x55 || b[i] == 0x56 || b[i] == 0x57) {
            i += 1; ++elements; continue;
        }
        if (i + 1 < got && b[i] == 0x41 && b[i + 1] >= 0x54 && b[i + 1] <= 0x57) {
            i += 2; ++elements; continue;
        }
        if (i + 3 <= got && b[i] == 0x48 && b[i + 1] == 0x89 &&
            b[i + 2] == 0xE5) { i += 3; ++elements; continue; }
        if (i + 3 <= got && b[i] == 0x48 && b[i + 1] == 0x8B &&
            b[i + 2] == 0xEC) { i += 3; ++elements; continue; }
        if (i + 3 < got && b[i] == 0x48 && b[i + 1] == 0x83 &&
            b[i + 2] == 0xEC) { i += 4; ++elements; continue; }
        if (i + 3 < got && b[i] == 0x48 && b[i + 1] == 0x81 &&
            b[i + 2] == 0xEC) {
            i += (i + 7 <= got) ? 7 : (got - i); ++elements; continue;
        }
        if (i + 4 < got && (b[i] == 0x48 || b[i] == 0x4C) && b[i + 1] == 0x89 &&
            b[i + 2] == 0x44 && b[i + 3] == 0x24) { i += 5; ++elements; continue; }
        break;  // first unrecognized byte
    }

    // Negative-shape knowledge: alignment nop padding, int3, ret, jmp, or a
    // plain ALU/mov-first byte are body shapes, not frame setup.
    if (elements > 0) {
        res.codes[res.code_count++] = RCode::CODE_I_PROLOGUE_LIKE;
    } else {
        res.codes[res.code_count++] = RCode::CODE_I_NOT_PROLOGUE_LIKE;
    }
    return res;
}

}  // namespace tlp
