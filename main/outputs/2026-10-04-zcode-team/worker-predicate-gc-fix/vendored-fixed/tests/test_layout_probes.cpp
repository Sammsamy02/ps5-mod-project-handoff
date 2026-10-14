// test_layout_probes.cpp — host tests for townfall_layout_probes.
//
// Every test cites the predicate chain step and rationale code it exercises
// (see townfall_layout_probes.hpp for the full chain). All images are
// synthetic little-endian mapped images; the three vaddrs below are the
// worker-2 profile facts and nothing else is assumed about the target.
//
// Build:  c++ -std=c++17 -Wall -Wextra -Werror -I.. test_layout_probes.cpp \
//             ../src/townfall_layout_probes.cpp -o test_layout_probes

#include "../src/townfall_layout_probes.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

using namespace tlp;

// --- worker-2 profile vaddrs (facts; cited in test names below) ----------
static const TownfallProfileVaddrs kProfile{
    0x93ba110,  // GUObjectArray  (profile GUObjectArray.guobject_original)
    0x2fac8c0,  // UGameEngine::Tick (profile GameEngineTick.gameengine_tick)
    0x490aa0,   // StaticConstructObject (profile UObject.static_construct_object)
};
static const uint64_t kImageBase = 0x10000000ULL;  // synthetic live base (worker-1 fact shape)

static int g_failures = 0;

#define CHECK(cond, name, detail)                                            \
    do {                                                                     \
        if (cond) {                                                          \
            std::printf("PASS %s\n", name);                                  \
        } else {                                                             \
            ++g_failures;                                                    \
            std::printf("FAIL %s — %s\n", name, detail);                     \
        }                                                                    \
    } while (0)

static bool has_code(const RCode* codes, uint8_t n, RCode c) {
    for (uint8_t i = 0; i < n; ++i)
        if (codes[i] == c) return true;
    return false;
}

// --- synthetic mapped-image arena ------------------------------------------
struct Arena {
    std::vector<std::unique_ptr<std::vector<uint8_t>>> owned;
    std::vector<MemSpan> spans;
    uint64_t next = 0x0000100000000000ULL;  // synthetic canonical heap region

    std::vector<uint8_t>* alloc_at(uint64_t addr, size_t n) {
        auto buf = std::make_unique<std::vector<uint8_t>>(n, 0);
        spans.push_back(MemSpan{addr, n, buf->data()});
        owned.push_back(std::move(buf));
        return owned.back().get();
    }
    std::vector<uint8_t>* alloc(size_t n) {
        uint64_t a = next;
        next += (uint64_t)n + 0x1000;
        return alloc_at(a, n);
    }
    static void put64(std::vector<uint8_t>* b, size_t off, uint64_t v) {
        for (int i = 0; i < 8; ++i) (*b)[off + (size_t)i] = (uint8_t)(v >> (8 * i));
    }
    static void put32(std::vector<uint8_t>* b, size_t off, uint32_t v) {
        for (int i = 0; i < 4; ++i) (*b)[off + (size_t)i] = (uint8_t)(v >> (8 * i));
    }
};

// Label/header allocations for each layout hypothesis (offsets cited in the
// header provenance block: TUObjectArray 0x0/0x8/0x10/0x14/0x18/0x1C;
// FUObjectArray outer GC 0x0/0x4 + embedded array at 0x10).
struct LabelBufs {
    std::vector<uint8_t>* hdr_a;  // 0x30 bytes at the label
    uint64_t objects;             // TUObjectArray.Objects target (chunk ptr array)
    std::vector<uint8_t>* objects_buf;
    uint64_t chunk0;
    std::vector<uint8_t>* chunk0_buf;
};

// Build a well-formed image of either layout; knobs let tests break one
// predicate deliberately.
struct ImgOpts {
    bool layout_a = true;         // false = TUObjectArray-direct image
    int32_t first_gc = 0x201;     // layout A GC fields (0x201 = last_non_gc+1, settled UE state)
    int32_t last_non_gc = 0x200;
    int32_t num_elements = 300;
    int32_t max_elements = 0x20000;
    int32_t max_chunks = 4;
    int32_t num_chunks = 1;
    uint64_t objects_override = 0;      // if nonzero, use instead of arena ptr
    uint64_t chunk_override0 = 0;       // if nonzero, chunk slot 0 value
    uint64_t chunk_override1 = 0;       // if nonzero, chunk slot 1 value
    size_t chunk0_tail_slack = 0;       // make chunk0 span smaller than needed
};

static LabelBufs build_image(Arena& ar, const ImgOpts& o) {
    LabelBufs lb{};
    const size_t per_chunk = (size_t)kNumElementsPerChunk * kFuObjectItemSize;
    const int32_t nchunks_store = o.num_chunks > 2 ? o.num_chunks : 2;
    lb.objects_buf = ar.alloc(8 * (size_t)nchunks_store);
    lb.objects = (uint64_t)(uintptr_t)0;
    {   // record actual address of the objects array
        for (auto& s : ar.spans)
            if (s.data == lb.objects_buf->data()) lb.objects = s.start;
    }
    const int64_t rem = o.num_elements;
    const size_t need0 = (size_t)(rem < kNumElementsPerChunk ? rem : kNumElementsPerChunk) * kFuObjectItemSize;
    const size_t cap = need0 + per_chunk + 0x1000;  // plenty for overlap tests
    lb.chunk0_buf = ar.alloc(cap);
    for (auto& s : ar.spans)
        if (s.data == lb.chunk0_buf->data()) lb.chunk0 = s.start;

    uint64_t c0 = o.chunk_override0 ? o.chunk_override0 : lb.chunk0;
    uint64_t c1 = o.chunk_override1 ? o.chunk_override1 : lb.chunk0;
    if (o.chunk0_tail_slack) {
        // chunk0 points near the END of its span so the needed extent is only
        // partially covered -> GOA_E_CHUNK_EXTENT_UNVERIFIABLE case.
        uint64_t span_end = lb.chunk0 + (uint64_t)lb.chunk0_buf->size();
        c0 = span_end - o.chunk0_tail_slack;
    }
    Arena::put64(lb.objects_buf, 0, c0);
    Arena::put64(lb.objects_buf, 8, c1);
    for (int32_t i = 2; i < nchunks_store; ++i)
        Arena::put64(lb.objects_buf, (size_t)i * 8, i < o.num_chunks ? c0 : 0);

    const uint64_t objects_val = o.objects_override ? o.objects_override : lb.objects;
    lb.hdr_a = ar.alloc_at(kImageBase + kProfile.guobject_array, 0x30);
    const size_t tua = o.layout_a ? 0x10 : 0x0;
    if (o.layout_a) {
        Arena::put32(lb.hdr_a, 0x0, (uint32_t)o.first_gc);
        Arena::put32(lb.hdr_a, 0x4, (uint32_t)o.last_non_gc);
        Arena::put32(lb.hdr_a, 0x8, 0);   // MaxObjectsNotConsideredByGC (unchecked)
        Arena::put32(lb.hdr_a, 0xC, 0);   // OpenForDisregardForGC
    } else {
        Arena::put64(lb.hdr_a, 0x0, objects_val);          // B.Objects at +0x0
        Arena::put64(lb.hdr_a, 0x8, 0);                    // B.PreAllocatedObjects
    }
    Arena::put64(lb.hdr_a, tua + 0x0, objects_val);
    Arena::put64(lb.hdr_a, tua + 0x8, 0);
    Arena::put32(lb.hdr_a, tua + 0x10, (uint32_t)o.max_elements);
    Arena::put32(lb.hdr_a, tua + 0x14, (uint32_t)o.num_elements);
    Arena::put32(lb.hdr_a, tua + 0x18, (uint32_t)o.max_chunks);
    Arena::put32(lb.hdr_a, tua + 0x1C, (uint32_t)o.num_chunks);
    return lb;
}

int main() {
    // ---- pure helpers ----
    CHECK(pointer_canonical_or_null(0), "helper_null_canonical", "null must pass");
    // Note: 0xFFFF8000... is the canonical sign-extended KERNEL form; the
    // readable-range predicate is what rejects kernel addresses. A truly
    // non-canonical value has bit 47 set without full sign extension:
    CHECK(!pointer_canonical_or_null(0x0000800000000010), "helper_bit47_noncanonical",
          "half-extended pointer must fail");
    CHECK(!pointer_canonical_or_null(0x1234), "helper_misaligned",
          "8-byte alignment required");

    // ================= T1: Layout A (FUObjectArray outer) validates ========
    {
        Arena ar;
        build_image(ar, ImgOpts{});  // canonical layout-A image
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                kImageBase, kProfile.guobject_array);
        char d[128];
        std::snprintf(d, sizeof d, "verdict=%s layout=%d code0=%s",
                      verdict_string(r.verdict), (int)r.observed.layout_matched,
                      rcode_string(r.codes[0]));
        CHECK(r.verdict == Verdict::Validated &&
              r.observed.layout_matched == GuaLayout::FUObjectArray_Outer &&
              r.codes[0] == RCode::OK,
              "T1_gua_layoutA_validated", d);
        CHECK(r.observed.num_elements == 300 && r.observed.max_elements == 0x20000 &&
              r.observed.obj_first_gc_index == 0x201 &&
              r.observed.obj_last_non_gc_index == 0x200,
              "T1b_gua_layoutA_observed_fields", "observed scalars mismatch");
    }

    // ================= T2: Layout B (TUObjectArray direct) validates =======
    {
        Arena ar;
        build_image(ar, ImgOpts{/*layout_a=*/false});
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                kImageBase, kProfile.guobject_array);
        char d[128];
        std::snprintf(d, sizeof d, "verdict=%s layout=%d code0=%s",
                      verdict_string(r.verdict), (int)r.observed.layout_matched,
                      rcode_string(r.codes[0]));
        CHECK(r.verdict == Verdict::Validated &&
              r.observed.layout_matched == GuaLayout::TUObjectArray_Direct &&
              r.codes[0] == RCode::OK,
              "T2_gua_layoutB_validated", d);
    }

    // ============ Refuted: adversarial images =============================
    {
        Arena ar;  // P1 canonical: half-extended (non-canonical) Objects pointer
        build_image(ar, ImgOpts{true, 0x201, 0x200, 300, 0x20000, 4, 1,
                                /*objects_override=*/0x0000800000000010});
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                kImageBase, kProfile.guobject_array);
        CHECK(r.verdict == Verdict::Refuted &&
              (has_code(r.codes, r.code_count, RCode::GOA_R_PTR_NONCANONICAL) ||
               r.layout_a.codes[0] == RCode::GOA_R_PTR_NONCANONICAL),
              "T3_gua_refuted_noncanonical_ptr", "expected GOA_R_PTR_NONCANONICAL");
    }
    {
        Arena ar;  // P1: Objects and PreAllocatedObjects both null
        LabelBufs lb = build_image(ar, ImgOpts{});
        Arena::put64(lb.hdr_a, 0x10, 0);  // A.Objects = null (PreAlloc already 0)
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                kImageBase, kProfile.guobject_array);
        CHECK(r.verdict == Verdict::Refuted &&
              r.layout_a.codes[0] == RCode::GOA_R_PTRS_BOTH_NULL,
              "T4_gua_refuted_both_null", "expected GOA_R_PTRS_BOTH_NULL");
    }
    {
        Arena ar;  // P2 counts: NumElements > MaxElements
        build_image(ar, ImgOpts{true, 0x201, 0x200, /*num=*/200, /*max=*/100, 4, 1});
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                kImageBase, kProfile.guobject_array);
        CHECK(r.verdict == Verdict::Refuted &&
              r.layout_a.codes[0] == RCode::GOA_R_COUNTS_INSANE,
              "T5_gua_refuted_num_gt_max", "expected GOA_R_COUNTS_INSANE (Num>Max)");
    }
    {
        Arena ar;  // P3 GC order: ObjLastNonGCIndex > MaxElements
        build_image(ar, ImgOpts{true, 0x10, /*last=*/0x50000, 300, /*max=*/0x30000, 4, 1});
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                kImageBase, kProfile.guobject_array);
        CHECK(r.verdict == Verdict::Refuted &&
              r.layout_a.codes[0] == RCode::GOA_R_GC_ORDER,
              "T6_gua_refuted_gc_order", "expected GOA_R_GC_ORDER");
    }
    {
        Arena ar;  // P5: chunk[0] == null while NumElements needs it
        LabelBufs lb = build_image(ar, ImgOpts{});
        Arena::put64(lb.objects_buf, 0, 0);
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                kImageBase, kProfile.guobject_array);
        CHECK(r.verdict == Verdict::Refuted &&
              r.layout_a.codes[0] == RCode::GOA_R_CHUNK_NULL_WHILE_NEEDED,
              "T7_gua_refuted_null_chunk", "expected GOA_R_CHUNK_NULL_WHILE_NEEDED");
    }
    {
        Arena ar;  // P5: chunk pointer readable-but-outside any span (unreadable chunk)
        build_image(ar, ImgOpts{true, 0x201, 0x200, 300, 0x20000, 4, 1, 0,
                                /*chunk_override0=*/0x0000700000001000});
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                kImageBase, kProfile.guobject_array);
        CHECK(r.verdict == Verdict::Refuted &&
              r.layout_a.codes[0] == RCode::GOA_R_CHUNK_UNREADABLE,
              "T8_gua_refuted_unreadable_chunk", "expected GOA_R_CHUNK_UNREADABLE");
    }
    {
        Arena ar;  // P4/P5: NumElements needs 2 chunks, NumChunks=1
        build_image(ar, ImgOpts{true, 0x201, 0x200, /*num=*/70000, /*max=*/0x40000, 4,
                                /*nchunks=*/1});
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                kImageBase, kProfile.guobject_array);
        CHECK(r.verdict == Verdict::Refuted &&
              r.layout_a.codes[0] == RCode::GOA_R_CAPACITY_MISMATCH,
              "T9_gua_refuted_capacity_mismatch", "expected GOA_R_CAPACITY_MISMATCH");
    }
    {
        Arena ar;  // P5: two needed chunks share one address
        build_image(ar, ImgOpts{true, 0x201, 0x200, /*num=*/70000, /*max=*/0x40000, 4,
                                /*nchunks=*/2, 0, 0 /*chunk0 default*/});
        // slot1 already defaults to the same chunk0 -> duplicate
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                kImageBase, kProfile.guobject_array);
        CHECK(r.verdict == Verdict::Refuted &&
              r.layout_a.codes[0] == RCode::GOA_R_CHUNK_DUPLICATE,
              "T10_gua_refuted_duplicate_chunks", "expected GOA_R_CHUNK_DUPLICATE");
    }
    {
        Arena ar;  // P5: two needed chunks with overlapping extents
        LabelBufs lb = build_image(ar, ImgOpts{true, 0x201, 0x200, /*num=*/70000,
                                               /*max=*/0x40000, 4, /*nchunks=*/2});
        Arena::put64(lb.objects_buf, 8, lb.chunk0 + 0x1000);  // inside chunk0 window
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                kImageBase, kProfile.guobject_array);
        CHECK(r.verdict == Verdict::Refuted &&
              r.layout_a.codes[0] == RCode::GOA_R_CHUNK_OVERLAP,
              "T11_gua_refuted_overlapping_chunks", "expected GOA_R_CHUNK_OVERLAP");
    }

    // ============ Inconclusive: boundary cases =============================
    {
        Arena ar;  // label bytes not covered by any span
        ar.alloc(0x1000);  // some unrelated readable memory
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                kImageBase, kProfile.guobject_array);
        CHECK(r.verdict == Verdict::Inconclusive &&
              has_code(r.codes, r.code_count, RCode::GOA_E_ADDR_UNREADABLE),
              "T12_gua_inconclusive_addr_unreadable", "expected GOA_E_ADDR_UNREADABLE");
    }
    {
        Arena ar;  // needed chunk extent only partially readable
        build_image(ar, ImgOpts{true, 0x201, 0x200, 300, 0x20000, 4, 1,
                                /*objects_override=*/0, /*chunk_override0=*/0,
                                /*chunk_override1=*/0, /*chunk0_tail_slack=*/0x800});
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                kImageBase, kProfile.guobject_array);
        char d[160];
        std::snprintf(d, sizeof d, "verdict=%s a=%s b=%s",
                      verdict_string(r.verdict), rcode_string(r.layout_a.codes[0]),
                      rcode_string(r.layout_b.codes[0]));
        CHECK(r.verdict == Verdict::Inconclusive &&
              r.layout_a.codes[0] == RCode::GOA_E_CHUNK_EXTENT_UNVERIFIABLE,
              "T13_gua_inconclusive_chunk_extent", d);
    }
    {
        // Carefully constructed dual image: BOTH hypotheses pass every hard
        // check -> honest ambiguity, never a Validated claim. (A.PreAlloc
        // lives at bytes+0x18 = MaxChunks|NumChunks<<32, so MaxChunks=8 keeps
        // that synthetic pointer 8-aligned and a span is placed for it.)
        Arena ar;
        const uint64_t P0 = 0x0000000F00000010ULL;      // B.Objects; halves = A GC first 0x10 / last 0x0F
        const uint64_t A_obj = 0x0000012000030000ULL;   // A.Objects = Max|Num<<32
        const uint64_t A_pre = 0x0000000100000008ULL;   // MaxChunks=8|NumChunks=1<<32
        std::vector<uint8_t>* b_chunk_arr = ar.alloc_at(P0, 8);
        std::vector<uint8_t>* a_chunk_arr = ar.alloc_at(A_obj, 8);
        ar.alloc_at(A_pre, 8);                            // readable; content unused
        std::vector<uint8_t>* c0 = ar.alloc(0x2000);
        std::vector<uint8_t>* c1 = ar.alloc(0x2000);
        uint64_t c0_addr = 0, c1_addr = 0;
        for (auto& s : ar.spans) {
            if (s.data == c0->data()) c0_addr = s.start;
            if (s.data == c1->data()) c1_addr = s.start;
        }
        Arena::put64(b_chunk_arr, 0, c0_addr);
        Arena::put64(a_chunk_arr, 0, c1_addr);
        std::vector<uint8_t>* hdr = ar.alloc_at(kImageBase + kProfile.guobject_array, 0x30);
        // B view
        Arena::put64(hdr, 0x0, P0);            // Objects
        Arena::put64(hdr, 0x8, 0);             // PreAllocatedObjects = 0
        Arena::put32(hdr, 0x10, 0x30000);      // MaxElements
        Arena::put32(hdr, 0x14, 0x120);        // NumElements
        Arena::put32(hdr, 0x18, 8);            // MaxChunks   (= A.PreAlloc low )
        Arena::put32(hdr, 0x1C, 1);            // NumChunks   (= A.PreAlloc high)
        // A-view scalars live at 0x20..0x30 (embedded TUObjectArray at +0x10):
        Arena::put32(hdr, 0x20, 0x30000);      // A.MaxElements
        Arena::put32(hdr, 0x24, 0x120);        // A.NumElements
        Arena::put32(hdr, 0x28, 8);            // A.MaxChunks
        Arena::put32(hdr, 0x2C, 1);            // A.NumChunks
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                kImageBase, kProfile.guobject_array);
        char d[160];
        std::snprintf(d, sizeof d, "verdict=%s a=%s(%s) b=%s(%s)",
                      verdict_string(r.verdict),
                      verdict_string(r.layout_a.verdict), rcode_string(r.layout_a.codes[0]),
                      verdict_string(r.layout_b.verdict), rcode_string(r.layout_b.codes[0]));
        CHECK(r.verdict == Verdict::Inconclusive &&
              has_code(r.codes, r.code_count, RCode::GOA_E_LAYOUT_AMBIGUOUS) &&
              r.layout_a.verdict == Verdict::Validated &&
              r.layout_b.verdict == Verdict::Validated,
              "T14_gua_inconclusive_layout_ambiguous", d);
    }

    // ============ Code-shape classifiers (always Inconclusive) ============
    {
        Arena ar;  // Tick vaddr, classic push rbp / mov rbp,rsp / push r15 / sub rsp,0x20
        std::vector<uint8_t>* code = ar.alloc_at(kImageBase + kProfile.ugameengine_tick, 16);
        const uint8_t bytes[] = {0x55, 0x48, 0x89, 0xE5, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x20};
        std::memcpy(code->data(), bytes, sizeof bytes);
        CodeProbeResult r = probe_code_shape(ar.spans.data(), ar.spans.size(),
                                             kImageBase, kProfile.ugameengine_tick,
                                             "UGameEngine::Tick");
        CHECK(r.verdict == Verdict::Inconclusive &&
              has_code(r.codes, r.code_count, RCode::CODE_I_PROLOGUE_LIKE),
              "T15_tick_prologue_like_inconclusive", "expected CODE_I_PROLOGUE_LIKE");
    }
    {
        Arena ar;  // StaticConstructObject vaddr, mid-function alignment nop
        std::vector<uint8_t>* code = ar.alloc_at(kImageBase + kProfile.static_construct_object, 16);
        const uint8_t bytes[] = {0x0F, 0x1F, 0x44, 0x00, 0x00, 0x48, 0x8B, 0x01};
        std::memcpy(code->data(), bytes, sizeof bytes);
        CodeProbeResult r = probe_code_shape(ar.spans.data(), ar.spans.size(),
                                             kImageBase, kProfile.static_construct_object,
                                             "StaticConstructObject");
        CHECK(r.verdict == Verdict::Inconclusive &&
              has_code(r.codes, r.code_count, RCode::CODE_I_NOT_PROLOGUE_LIKE),
              "T16_sco_not_prologue_like_inconclusive", "expected CODE_I_NOT_PROLOGUE_LIKE");
    }
    {
        Arena ar;  // code vaddr not readable
        ar.alloc(0x1000);
        CodeProbeResult r = probe_code_shape(ar.spans.data(), ar.spans.size(),
                                             kImageBase, kProfile.ugameengine_tick, nullptr);
        CHECK(r.verdict == Verdict::Inconclusive &&
              has_code(r.codes, r.code_count, RCode::CODE_E_UNREADABLE),
              "T17_code_unreadable_inconclusive", "expected CODE_E_UNREADABLE");
    }
    {
        Arena ar;  // endbr64 frame setup + 0xCC padding before entry
        std::vector<uint8_t>* code = ar.alloc_at(kImageBase + kProfile.ugameengine_tick - 8, 24);
        for (int i = 0; i < 8; ++i) (*code)[i] = 0xCC;
        const uint8_t bytes[] = {0xF3, 0x0F, 0x1E, 0xFA, 0x55, 0x48, 0x89, 0xE5};
        std::memcpy(code->data() + 8, bytes, sizeof bytes);
        CodeProbeResult r = probe_code_shape(ar.spans.data(), ar.spans.size(),
                                             kImageBase, kProfile.ugameengine_tick,
                                             "UGameEngine::Tick");
        CHECK(r.verdict == Verdict::Inconclusive &&
              has_code(r.codes, r.code_count, RCode::CODE_I_PROLOGUE_LIKE) &&
              has_code(r.codes, r.code_count, RCode::CODE_I_PRE_PADDING),
              "T18_tick_endbr_pre_padding", "expected PROLOGUE_LIKE + PRE_PADDING");
    }

    // ---- FName pool: predicate exclusion is itself checked-in data ----
    CHECK(kFNamePoolPredicateExcluded && std::strlen(kFNamePoolExclusionReason) > 0,
          "T19_fname_pool_no_predicate_by_rule",
          "P2 exclusion constant/reason must remain present");


    // ====== T20: live replay of the captured 0x97ba110 bytes ==============
    // Header bytes are the EXACT 96 bytes read at 0x97ba110 in the live
    // target (image_base 0x400000, GOA vaddr 0x93ba110, span
    // [0x970c000,0x9a34000) RW). Live facts exercised: GC pair
    // ObjFirstGCIndex=35477, ObjLastNonGCIndex=35476 (= first-1, the normal
    // settled UE state the old P3 wrongly refuted). The chunk-pointer array
    // and chunk bodies are RECONSTRUCTED scaffolding at the observed Objects
    // pointer 0x13800220a0 so the full P1-P5 chain can run; they are NOT
    // live captures (cfgL span contents were not retained).
    {
        Arena ar;
        const uint64_t kHdrStart = 0x970c000ULL, kHdrSize = 0x328000ULL;
        const uint64_t kGoaAddr = 0x97ba110ULL;  // image_base 0x400000 + 0x93ba110
        static const uint8_t kLive[96] = {0x95, 0x8a, 0x00, 0x00, 0x94, 0x8a, 0x00, 0x00, 0x95, 0x8a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x20, 0x02, 0x80, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x17, 0x19, 0x01, 0x00, 0x21, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x06, 0x01, 0x80, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xd7, 0x32, 0x40, 0x15, 0x00, 0x00, 0x00, 0x6e, 0x00, 0x00, 0x00, 0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        auto* hdr = ar.alloc_at(kHdrStart, (size_t)kHdrSize);
        std::memcpy(hdr->data() + (size_t)(kGoaAddr - kHdrStart), kLive, sizeof kLive);
        const uint64_t kObjects = 0x13800220a0ULL;   // live bytes +0x10
        const uint64_t kC0 = 0x20000000ULL, kC1 = 0x30000000ULL;
        auto* objarr = ar.alloc_at(kObjects, 16);
        Arena::put64(objarr, 0, kC0);
        Arena::put64(objarr, 8, kC1);
        ar.alloc_at(kC0, (size_t)65536 * kFuObjectItemSize);            // chunk0
        ar.alloc_at(kC1, (size_t)(71959 - 65536) * kFuObjectItemSize);  // chunk1
        GuaProbeResult r = probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                                0x400000ULL, 0x93ba110ULL);
        char d[192];
        std::snprintf(d, sizeof d,
                      "verdict=%s layout=%d first=%d last=%d num=%d max=%d chunks=%d/%d",
                      verdict_string(r.verdict), (int)r.observed.layout_matched,
                      r.observed.obj_first_gc_index, r.observed.obj_last_non_gc_index,
                      r.observed.num_elements, r.observed.max_elements,
                      r.observed.num_chunks, r.observed.max_chunks);
        CHECK(r.verdict == Verdict::Validated &&
              r.observed.layout_matched == GuaLayout::FUObjectArray_Outer &&
              r.observed.obj_first_gc_index == 35477 &&
              r.observed.obj_last_non_gc_index == 35476 &&
              r.observed.num_elements == 71959 &&
              r.observed.max_elements == 2162688 &&
              r.observed.max_chunks == 33 && r.observed.num_chunks == 2,
              "T20_gua_live_bytes_layoutA_validated", d);
    }

    std::printf("\n%s: %d failure(s)\n", g_failures ? "TESTS FAILED" : "ALL TESTS PASSED",
                g_failures);
    return g_failures ? 1 : 0;
}
