// test_predicate_call.cpp — host tests for the C wrapper predicate_call.
//
// Reuses the sibling fixture pattern (worker-5-layout-validator
// tests/test_layout_probes.cpp Arena/put32/put64) but packs EVERYTHING into
// one contiguous synthetic 96-byte image, because the wrapper exposes exactly
// ONE span (raw pointer + length). Image base is a synthetic live base in the
// worker-1 fact shape.
//
// Each case cites the predicate chain step / rationale code it exercises.
// Layout geometry (all offsets in the 96-byte image, LE):
//
//   Layout-A image (hypothesis A = FUObjectArray outer, header 0x30):
//     0x00 ObjFirstGCIndex, 0x04 ObjLastNonGCIndex, 0x08/0x0C unchecked,
//     0x10 embedded TUObjectArray: 0x10 Objects, 0x18 PreAllocatedObjects,
//     0x20 MaxElements, 0x24 NumElements, 0x28 MaxChunks, 0x2C NumChunks
//     0x30 chunk-pointer array (8B, slot0), 0x38 chunk0 (0x18 = 1 item)
//     -> ends at 0x50 <= 0x60.
//     Hypothesis B reads the same bytes as a bare TUObjectArray: Objects at
//     0x0 == {0,0} -> GOA_R_PTRS_BOTH_NULL, so B is refuted while A runs.
//
//   Layout-B image (hypothesis B = TUObjectArray direct, header 0x20):
//     0x00 Objects, 0x08 PreAllocatedObjects, 0x10 Max, 0x14 Num,
//     0x18 MaxChunks, 0x1C NumChunks, 0x20 chunk-pointer array (8B),
//     0x28 chunk0 (0x18) -> ends at 0x40 <= 0x60.
//     Hypothesis A reads GC fields at 0x0/0x4 = halves of Objects pointer:
//     FirstGC=0x1xxxxxxx, LastNonGC=0 -> GOA_R_GC_ORDER, so A is refuted.
//
// Build (host): c++ -std=c++17 -Wall -Wextra -Werror -I../src
//   test_predicate_call.cpp ../src/predicate_call.c
//   ../src/townfall_layout_probes.cpp -o test_predicate_call

#include "../src/predicate_call.h"

#include <cstdio>
#include <cstring>
#include <vector>

static int g_failures = 0;

#define CHECK(cond, name, detail)                                              \
    do {                                                                       \
        if (cond) {                                                            \
            std::printf("PASS %s\n", name);                                    \
        } else {                                                               \
            ++g_failures;                                                      \
            std::printf("FAIL %s — %s\n", name, detail);                       \
        }                                                                      \
    } while (0)

static const uint64_t kImageBase = 0x10000000ULL;  // synthetic live base
// Label vaddr INSIDE the compact 96-byte image (vaddr 0). The real profile
// fact for the live target is 0x93ba110; the anchor probe passes that with
// the real mapped module image. The wrapper takes vaddr as an argument, so
// the compact fixture simply places the label at 0 (same code path).
static const uint64_t kGoaVaddr = 0x0ULL;

// --- 96-byte synthetic image helpers (sibling Arena put32/put64 pattern) ---
static void put64(uint8_t* b, size_t off, uint64_t v) {
    for (int i = 0; i < 8; ++i) b[off + (size_t)i] = (uint8_t)(v >> (8 * i));
}
static void put32(uint8_t* b, size_t off, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[off + (size_t)i] = (uint8_t)(v >> (8 * i));
}

static bool has_code(const pc_gua_verdict& v, uint16_t c) {
    for (int i = 0; i < v.code_count && i < PC_MAX_CODES; ++i)
        if (v.codes[i] == c) return true;
    return false;
}

// Well-formed layout-A image in exactly 96 bytes. 1 element, 1 chunk.
static void build_image_a(uint8_t* img) {
    std::memset(img, 0, 96);
    put32(img, 0x00, 0x100);              // A.ObjFirstGCIndex  (P3 pass)
    put32(img, 0x04, 0x200);              // A.ObjLastNonGCIndex (P3 pass)
    put64(img, 0x10, kImageBase + 0x30);  // A.Objects -> chunk-ptr array at 0x30
    put64(img, 0x18, 0);                  // A.PreAllocatedObjects (null ok, P1)
    put32(img, 0x20, 0x20000);            // MaxElements (P2)
    put32(img, 0x24, 1);                  // NumElements  (P2)
    put32(img, 0x28, 2);                  // MaxChunks    (P2: Max <= 2*64Ki)
    put32(img, 0x2C, 1);                  // NumChunks    (P2, <= MaxChunks)
    put64(img, 0x30, kImageBase + 0x38);  // chunk slot 0 -> chunk0 at 0x38
    // chunk0 bytes 0x38..0x50 stay zero (FUObjectItem content is unchecked)
}

// Well-formed layout-B image in exactly 96 bytes. 1 element, 1 chunk.
static void build_image_b(uint8_t* img) {
    std::memset(img, 0, 96);
    put64(img, 0x00, kImageBase + 0x20);  // B.Objects -> chunk-ptr array at 0x20
    put64(img, 0x08, 0);                  // B.PreAllocatedObjects
    put32(img, 0x10, 0x20000);            // MaxElements
    put32(img, 0x14, 1);                  // NumElements
    put32(img, 0x18, 2);                  // MaxChunks  (P2: Max <= 2*64Ki)
    put32(img, 0x1C, 1);                  // NumChunks
    put64(img, 0x20, kImageBase + 0x28);  // chunk slot 0 -> chunk0 at 0x28
    // chunk0 bytes 0x28..0x40 stay zero
}

int main() {
    // ---- argument handling -------------------------------------------------
    {
        uint8_t img[96];
        build_image_a(img);
        pc_gua_verdict v;
        std::memset(&v, 0xAA, sizeof(v));
        CHECK(predicate_call_guobject_array(nullptr, 96, kImageBase, kGoaVaddr,
                                            &v) == -1 &&
                  v.verdict == PC_V_NOT_PROBED &&
                  v.codes[0] == PC_R_CALL_E_BAD_ARGS,
              "args_null_image_rejected", "expected rc -1 + BAD_ARGS");
        CHECK(predicate_call_guobject_array(img, 96, kImageBase, kGoaVaddr,
                                            nullptr) == -1,
              "args_null_out_rejected", "expected rc -1");
        CHECK(predicate_call_guobject_array(img, 0, kImageBase, kGoaVaddr,
                                            &v) == -1,
              "args_zero_length_rejected", "expected rc -1");
        std::printf("INFO sizeof(pc_gua_verdict)=%zu\n", sizeof(pc_gua_verdict));
    }

    // ---- T1: hypothesis A validates, B refuted -> Validated (layout A) -----
    {
        uint8_t img[96];
        build_image_a(img);
        pc_gua_verdict v;
        CHECK(predicate_call_guobject_array(img, sizeof img, kImageBase,
                                            kGoaVaddr, &v) == 0,
              "a_call_rc0", "expected rc 0");
        CHECK(v.verdict == PC_V_VALIDATED, "a_verdict_validated",
              "combined must be Validated");
        CHECK(v.layout == PC_LAYOUT_FUOBJECTARRAY_OUTER, "a_layout_matched_A",
              "layout must be FUObjectArray_Outer");
        CHECK(v.layout_a_verdict == PC_V_VALIDATED, "a_sub_A_validated",
              "layout A sub-verdict");
        CHECK(v.layout_b_verdict == PC_V_REFUTED &&
                  has_code(v, PC_R_GOA_R_PTRS_BOTH_NULL) == false,
              "a_sub_B_refuted", "B must be Refuted (B.Objects==null)");
        CHECK(v.obj_first_gc_index == 0x100 &&
                  v.obj_last_non_gc_index == 0x200,
              "a_observed_gc_indices", "A-only observed GC fields");
        CHECK(v.num_elements == 1 && v.max_elements == 0x20000 &&
                  v.max_chunks == 2 && v.num_chunks == 1,
              "a_observed_counts", "observed P2 scalars");
        CHECK(v.objects_ptr == kImageBase + 0x30 &&
                  v.chunk0_ptr == kImageBase + 0x38 && v.chunks_scanned == 1,
              "a_observed_ptrs", "observed pointer fields");
    }

    // ---- T2: hypothesis B validates, A refuted -> Validated (layout B) -----
    {
        uint8_t img[96];
        build_image_b(img);
        pc_gua_verdict v;
        CHECK(predicate_call_guobject_array(img, sizeof img, kImageBase,
                                            kGoaVaddr, &v) == 0,
              "b_call_rc0", "expected rc 0");
        CHECK(v.verdict == PC_V_VALIDATED, "b_verdict_validated",
              "combined must be Validated");
        CHECK(v.layout == PC_LAYOUT_TUOBJECTARRAY_DIRECT, "b_layout_matched_B",
              "layout must be TUObjectArray_Direct");
        CHECK(v.layout_b_verdict == PC_V_VALIDATED, "b_sub_B_validated",
              "layout B sub-verdict");
        // On a B-image, A.Objects = u64@0x10 = {MaxElements, NumElements}
        // = 0x100020000, canonical+aligned but outside the span -> A refutes
        // at chain step P1 (PTR_OUTSIDE_READABLE), never reaching P3. The
        // POD carries the per-layout VERDICT; per-layout rationale codes stay
        // in the C++ GuaProbeResult (by design only combined codes are kept).
        CHECK(v.layout_a_verdict == PC_V_REFUTED,
              "b_sub_A_refuted_outside_readable",
              "A must be Refuted while B validates");
        CHECK(v.obj_first_gc_index == -1 && v.obj_last_non_gc_index == -1,
              "b_gc_indices_minus1", "layout B has no GC fields");
        CHECK(v.objects_ptr == kImageBase + 0x20 &&
                  v.chunk0_ptr == kImageBase + 0x28,
              "b_observed_ptrs", "observed pointer fields");
    }

    // ---- T3: refuted — counts insane under A, B both-null ------------------
    // (P2: NumElements > MaxElements violates counts_ok under A; under B the
    // first 8 bytes are the GC fields {0x100,0x200} => Objects non-null but
    // LastNonGC..wait: B.Objects = 0x0000020000000100, misaligned+noncanonical
    // would refute B via NONCANONICAL; either way the COMBINED verdict is
    // Refuted because A and B are each Refuted.)
    {
        uint8_t img[96];
        build_image_a(img);
        put32(img, 0x24, 200);  // A.NumElements=200 > A.MaxElements=0x20000? no
        put32(img, 0x20, 100);  // MaxElements=100 < NumElements=200 -> P2 fail
        pc_gua_verdict v;
        predicate_call_guobject_array(img, sizeof img, kImageBase, kGoaVaddr,
                                      &v);
        CHECK(v.verdict == PC_V_REFUTED, "refuted_counts_insane",
              "combined must be Refuted");
        CHECK(has_code(v, PC_R_GOA_R_COUNTS_INSANE), "refuted_code_counts",
              "COUNTS_INSANE expected among codes");
        CHECK(v.layout == PC_LAYOUT_NONE, "refuted_layout_none",
              "no layout claimed on Refuted");
    }

    // ---- T4: refuted — non-canonical pointer in the needed header ----------
    // A.Objects = 0x1234 (canonical-but-misaligned => NONCANONICAL under A);
    // B reads Objects at +0x0 = halves {0x1234, 0} => Objects=0x1234 too.
    {
        uint8_t img[96];
        build_image_a(img);
        put64(img, 0x10, 0x1234);
        pc_gua_verdict v;
        predicate_call_guobject_array(img, sizeof img, kImageBase, kGoaVaddr,
                                      &v);
        CHECK(v.verdict == PC_V_REFUTED, "refuted_noncanonical",
              "combined must be Refuted");
        CHECK(has_code(v, PC_R_GOA_R_PTR_NONCANONICAL),
              "refuted_code_noncanonical", "NONCANONICAL expected");
    }

    // ---- T5: refuted — chunk pointer outside the single readable span ------
    // (P1: Objects readable; P5: chunk slot 0 points outside the image ->
    // GOA_R_CHUNK_UNREADABLE under both layouts that parse it as Objects.)
    {
        uint8_t img[96];
        build_image_a(img);
        put64(img, 0x30, kImageBase + 0x10000);  // chunk slot 0 far outside
        // Keep A alive: A.Objects=0x30 ok. chunk0 unreachable -> A refuted
        // CHAIN: P5 read_bytes(objects+0) reads slot AT 0x30 which is fine;
        // chunk_ptr itself is outside -> GOA_R_CHUNK_UNREADABLE for A.
        pc_gua_verdict v;
        predicate_call_guobject_array(img, sizeof img, kImageBase, kGoaVaddr,
                                      &v);
        CHECK(v.verdict == PC_V_REFUTED, "refuted_chunk_unreadable",
              "combined must be Refuted");
        CHECK(has_code(v, PC_R_GOA_R_CHUNK_UNREADABLE),
              "refuted_code_chunk_unreadable", "CHUNK_UNREADABLE expected");
    }

    // ---- T6: inconclusive — header truncated by the span length -----------
    // (Both hypotheses need their full header (0x30 / 0x20) at the label;
    // length 0x10 < 0x20 -> GOA_E_ADDR_UNREADABLE for both -> Inconclusive.)
    {
        uint8_t img[96];
        build_image_b(img);
        pc_gua_verdict v;
        CHECK(predicate_call_guobject_array(img, 0x10, kImageBase, kGoaVaddr,
                                            &v) == 0,
              "inc_call_rc0", "wrapper call itself succeeds");
        CHECK(v.verdict == PC_V_INCONCLUSIVE, "inc_verdict_inconclusive",
              "combined must be Inconclusive");
        CHECK(has_code(v, PC_R_GOA_E_ADDR_UNREADABLE), "inc_code_addr",
              "ADDR_UNREADABLE expected");
        CHECK(v.layout_a_verdict == PC_V_INCONCLUSIVE &&
                  v.layout_b_verdict == PC_V_INCONCLUSIVE,
              "inc_sub_layouts", "both sub-verdicts Inconclusive");
    }

    std::printf(g_failures == 0 ? "ALL TESTS PASSED\n" : "FAILURES: %d\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
