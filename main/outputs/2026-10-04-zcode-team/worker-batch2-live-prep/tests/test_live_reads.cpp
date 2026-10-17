// test_live_reads.cpp
//
// Host synthetic suite for the rung-3 live-read module (live_gua_reads.c).
// Team: 2026-10-04-zcode-team / worker-batch2-live-prep.
//
// The happy-path fixture reuses the LIVE-VALIDATED observed GUA set
// (worker-staged-pair-live/CLAIM.json; worker-predicate-gc-fix REPORT):
//   GUA 0x97ba110 in captured span [0x970c000,0x9a34000) prot 6,
//   first_gc=35477, last_non_gc=35476, max_not_gc=35477, open_dfgc=0,
//   Objects**=0x13800220a0, MaxElements=2162688, NumElements=71959,
//   MaxChunks=33, NumChunks=2. Chunk/object/storage addresses beyond the
//   chunk table are SYNTHETIC (nothing on target has ever been read past
//   the GUA header); they exist to prove module behavior, not target facts.
//
// Build (from this directory):
//   c++ -std=c++17 -Wall -Wextra -Werror -I../src test_live_reads.cpp \
//       ../src/live_gua_reads.c -o test_live_reads

#include "live_gua_reads.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (cond) { ++g_pass; } else { ++g_fail;                           \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); }  \
    } while (0)

constexpr uint64_t GUA_ADDR   = 0x97ba110ull;   // live-validated header addr
constexpr uint64_t GUA_SPAN   = 0x970c000ull;   // captured span start
constexpr uint64_t GUA_SPAN_SZ= 0x328000ull;    // [0x970c000,0x9a34000)
constexpr uint64_t CHUNKS_ADDR= 0x1380022000ull; // synthetic cover of the
                                                  // live value 0x13800220a0
constexpr uint64_t OBJPTR_VAL = 0x13800220a0ull; // live-validated Objects**
constexpr uint64_t CHUNK0     = 0x1380400000ull;
constexpr uint64_t CHUNK1     = 0x1380500000ull;  // 0x180000 apart
                                              // from CHUNK0, disjoint from
                                              // the objects region
constexpr uint64_t OBJ_A      = 0x1380700010ull;
constexpr uint64_t OBJ_UCLASS = 0x1380700110ull;
constexpr uint64_t OBJ_B      = 0x1380700210ull;
constexpr uint64_t OBJ_C      = 0x1380700310ull;
constexpr uint64_t VTBL       = 0x4600a0ull;

struct Region {
    uint64_t vaddr;
    size_t size;
    uint32_t prot;
    std::vector<uint8_t> bytes;
    Region(uint64_t a, size_t sz, uint32_t p)
        : vaddr(a), size(sz), prot(p), bytes(sz, 0) {}
    uint64_t end() const { return vaddr + bytes.size(); }
};

struct Fixture {
    // Ordered by vaddr for deterministic covering.
    std::vector<Region> regions;

    Region& add(uint64_t a, size_t sz, uint32_t p) {
        regions.emplace_back(a, sz, p);
        return regions.back();
    }
    void poke(uint64_t addr, const void* src, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            Region& r = region_for(addr + i);
            r.bytes.at(addr + i - r.vaddr) =
                static_cast<const uint8_t*>(src)[i];
        }
    }
    void poke_u64(uint64_t addr, uint64_t v) {
        uint8_t b[8];
        for (int i = 0; i < 8; ++i) b[i] = (uint8_t)(v >> (8 * i));
        poke(addr, b, 8);
    }
    void poke_u32(uint64_t addr, uint32_t v) {
        uint8_t b[4];
        for (int i = 0; i < 4; ++i) b[i] = (uint8_t)(v >> (8 * i));
        poke(addr, b, 4);
    }
    Region& region_for(uint64_t addr) {
        for (Region& r : regions)
            if (addr >= r.vaddr && addr < r.end()) return r;
        std::printf("FATAL poke at %llx outside fixture\n",
                    (unsigned long long)addr);
        std::exit(2);
    }
    igu_span_table table() {
        // igu_span array must outlive the probe call: caller keeps the
        // returned vector alive.
        return igu_span_table{nullptr, 0}; // replaced by spans(), below
    }
    std::vector<igu_span> spans_vec() const {
        std::vector<igu_span> v;
        for (const Region& r : regions) {
            igu_span s;
            s.start = r.vaddr;
            s.end = r.end();
            s.prot = r.prot;
            s.data = r.bytes.data();
            v.push_back(s);
        }
        return v;
    }
};

// Strip regions whose vaddr equals `except` (to simulate map holes).
std::vector<igu_span> spans_without(const Fixture& f, uint64_t except) {
    std::vector<igu_span> v;
    for (const Region& r : f.regions) {
        if (r.vaddr == except) continue;
        igu_span s;
        s.start = r.vaddr;
        s.end = r.end();
        s.prot = r.prot;
        s.data = r.bytes.data();
        v.push_back(s);
    }
    return v;
}

// Object layout helper (SET-5_06 UObjectBase).
void place_object(Fixture& f, uint64_t addr, uint64_t vptr, uint32_t flags,
                  int32_t idx, uint64_t cls, const char* name8,
                  uint64_t outer) {
    f.poke_u64(addr + 0x00, vptr);
    f.poke_u32(addr + 0x08, flags);
    f.poke_u32(addr + 0x0C, (uint32_t)idx);
    f.poke_u64(addr + 0x10, cls);
    f.poke(addr + 0x18, name8, 8);
    f.poke_u64(addr + 0x20, outer);
}

// FUObjectItem helper (SET-5_06: Object 0x0, Flags 0x8, Cluster 0xC,
// Serial 0x10, stride 0x18).
void place_item(Fixture& f, uint64_t chunk_base, uint32_t slot, uint64_t obj,
                uint32_t flags, uint32_t cluster, uint32_t serial) {
    const uint64_t a = chunk_base + (uint64_t)slot * 0x18ull;
    f.poke_u64(a + 0x00, obj);
    f.poke_u32(a + 0x08, flags);
    f.poke_u32(a + 0x0C, cluster);
    f.poke_u32(a + 0x10, serial);
}

// The live-shaped fixture. If first_item_object is 0, items 0..4 are null
// slots and item[5] points at OBJ_A (scan exercised); otherwise item[0]
// carries the given object.
Fixture make_live_fixture(bool drop_chunks_region = false,
                          uint32_t chunks_prot = 3,
                          bool /*drop_image_region, kept for callers' clarity*/ = false,
                          uint64_t first_item_object = 0,
                          uint64_t obj_a_flags = 0x00030000ull,
                          int32_t obj_a_idx = 5,
                          uint64_t item5_object_override = 0,
                          bool straddle_obj_a = false,
                          uint32_t gua_prot = 3) {  // KVME R|W (the probe
    // logs recorded the GUA span POSIX-style as "prot 6"; KVME translation
    // is exercised explicitly in t20).
    Fixture f;
    f.add(GUA_SPAN, (size_t)GUA_SPAN_SZ, gua_prot);
    f.add(CHUNKS_ADDR, 0x1000, chunks_prot);
    f.add(CHUNK0, 0x180000, 3);  // 65536 items * 0x18
    f.add(CHUNK1, 0x180000, 3);
    Region& objects = f.add(0x1380700000, 0x10000, 3);
    (void)objects;
    f.add(0x400000, 0x387c000, 5); // image-like, readable in fixture
    f.add(0x1380900000, 0x1000, 0); // PROT_NONE reservation analog
                                    // (covered_not_readable target)

    // GUA header (layout A) with the live-validated values.
    f.poke_u32(GUA_ADDR + 0x00, 35477);
    f.poke_u32(GUA_ADDR + 0x04, 35476);
    f.poke_u32(GUA_ADDR + 0x08, 35477);
    f.poke_u32(GUA_ADDR + 0x0C, 0);
    f.poke_u64(GUA_ADDR + 0x10, OBJPTR_VAL);
    f.poke_u32(GUA_ADDR + 0x20, 2162688);
    f.poke_u32(GUA_ADDR + 0x24, 71959);
    f.poke_u32(GUA_ADDR + 0x28, 33);
    f.poke_u32(GUA_ADDR + 0x2C, 2);

    // Chunk table at the live Objects** value: chunk[0], chunk[1].
    f.poke_u64(OBJPTR_VAL + 0, CHUNK0);
    f.poke_u64(OBJPTR_VAL + 8, CHUNK1);

    // vtable bytes in the image region.
    f.poke_u64(VTBL, 0x1122334455667788ull);

    // Objects.
    place_object(f, OBJ_A, VTBL, (uint32_t)obj_a_flags, obj_a_idx,
                 OBJ_UCLASS, "NamePriv", 0);
    place_object(f, OBJ_B, VTBL, 1, 70000, OBJ_UCLASS, "Bxxxxxxx", OBJ_A);
    place_object(f, OBJ_C, VTBL, 2, 65536, OBJ_UCLASS, "Cxxxxxxx", 0);
    // OBJ_UCLASS needs only 8 readable bytes.
    f.poke_u64(OBJ_UCLASS, VTBL);

    if (straddle_obj_a) {
        // Shrink the objects region so OBJ_A's 0x28 window leaves it.
        // (Lookup at use time: the earlier reference would dangle across
        // emplace_back reallocations.)
        f.region_for(0x1380700000).bytes.resize(
            (size_t)(OBJ_A - 0x1380700000) + 0x10);
    }

    // Items: chunk0 slots 0..4 null, slot 5 -> OBJ_A; chunk1 slot 4464 ->
    // OBJ_B (index 70000 = chunk 1, slot 4464); chunk1 slot 0 -> OBJ_C
    // (index 65536).
    if (first_item_object != 0) {
        // The caller wants the very first slot (scan accepts immediately).
        place_item(f, CHUNK0, 0, first_item_object, 0x02000201u, 0, 1);
    } else {
        for (uint32_t s = 0; s < 5; ++s) place_item(f, CHUNK0, s, 0, 0, 0, 0);
        place_item(f, CHUNK0, 5,
                   item5_object_override ? item5_object_override : OBJ_A,
                   0x02000201u, 0, 6);
    }
    place_item(f, CHUNK1, 0, OBJ_C, 0x02000200u, 0, 65537);
    place_item(f, CHUNK1, 4464, OBJ_B, 0x02000200u, 0, 70001);
    // When the scan must accept OBJ_B via item[0], fix its claimed index
    // resolution through item[70000] (already placed above).
    if (drop_chunks_region) {
        // Remove the chunk-table cover: region_for(CHUNKS_ADDR) would
        // still hold bytes but no span is published for it.
        // (Handled by spans_without in the caller.)
    }
    (void)drop_chunks_region;
    return f;
}

igu_span_table make_table(const std::vector<igu_span>& v) {
    igu_span_table t;
    t.spans = v.data();
    t.count = v.size();
    return t;
}

// ------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------

void t01_happy_path_live_shaped() {
    Fixture f = make_live_fixture();
    std::vector<igu_span> sv = f.spans_vec();
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    CHECK(rc == IGU_OK);
    CHECK(igu_rung3_class(&rec) == 0);
    CHECK(rec.st_hdr == IGU_OK && rec.st_a == IGU_OK && rec.st_b == IGU_OK &&
          rec.st_c == IGU_OK && rec.st_d == IGU_OK);
    // Live-validated header values round-trip exactly.
    CHECK(rec.first_gc == 35477 && rec.last_non_gc == 35476);
    CHECK(rec.max_not_gc == 35477 && rec.open_dfgc == 0);
    CHECK(rec.objects_ptr == 0x13800220a0ull);
    CHECK(rec.max_elements == 2162688 && rec.num_elements == 71959);
    CHECK(rec.max_chunks == 33 && rec.num_chunks == 2);
    CHECK(rec.info_max_eq_chunks == IGU_CSK_PASS);   // 2162688 == 33*65536
    CHECK(rec.info_num_le_max == IGU_CSK_PASS);
    CHECK(rec.info_nchunks_ok == IGU_CSK_PASS);
    CHECK(rec.chunk_index == 0 && rec.chunk_ptr == CHUNK0);
    CHECK(rec.items_scanned == 6);                    // slots 0..4 null
    CHECK(rec.item_object == OBJ_A);
    CHECK(rec.item_flags == 0x02000201u && rec.item_serial == 6);
    CHECK(rec.vptr == VTBL && rec.obj_flags == 0x00030000u);
    CHECK(rec.internal_index == 5);
    CHECK(rec.class_ptr == OBJ_UCLASS);
    CHECK(rec.outer_ptr == 0);
    CHECK(std::memcmp(rec.name_raw, "NamePriv", 8) == 0);
    CHECK(rec.chk_vptr == IGU_CSK_PASS && rec.chk_flags == IGU_CSK_PASS);
    CHECK(rec.chk_index_bounds == IGU_CSK_PASS);
    CHECK(rec.chk_roundtrip == IGU_CSK_PASS);
    CHECK(rec.chk_class == IGU_CSK_PASS && rec.chk_outer == IGU_CSK_PASS);
}

void t02_scan_bound_exhausted_is_inconclusive() {
    Fixture f = make_live_fixture();
    std::vector<igu_span> sv = f.spans_vec();
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, 3, &rec); // slots 0..2 all null
    CHECK(rc == IGU_R_NULL);
    CHECK(igu_rung3_class(&rec) == 1); // OBJ_E_NO_CANDIDATES analog
    CHECK(rec.items_scanned == 3 && rec.st_c == IGU_R_NULL);
    CHECK(rec.item_object == 0);
}

void t03_roundtrip_cross_chunk_via_item0() {
    // item[0] -> OBJ_B whose InternalIndex 70000 lives in chunk 1 slot
    // 4464: the round-trip must walk Objects**[1] (65536-element chunk
    // math), not chunk 0.
    Fixture f = make_live_fixture(/*drop=*/false, /*chunks_prot=*/3,
                                  /*drop_image=*/false,
                                  /*first_item_object=*/OBJ_B);
    std::vector<igu_span> sv = f.spans_vec();
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    CHECK(rc == IGU_OK);
    CHECK(igu_rung3_class(&rec) == 0);
    CHECK(rec.items_scanned == 1 && rec.item_object == OBJ_B);
    CHECK(rec.internal_index == 70000);
    CHECK(rec.chk_roundtrip == IGU_CSK_PASS);
    CHECK(rec.chk_outer == IGU_CSK_PASS); // outer -> OBJ_A readable
}

void t04_chunk_table_not_covered() {
    Fixture f = make_live_fixture();
    std::vector<igu_span> sv = spans_without(f, CHUNKS_ADDR);
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    CHECK(rc == IGU_E_NO_SPAN);
    CHECK(igu_rung3_class(&rec) == 1); // not_covered_current_map
    CHECK(rec.st_a == IGU_OK && rec.objects_ptr == OBJPTR_VAL);
    CHECK(rec.st_b == IGU_E_NO_SPAN && rec.st_c == 0 && rec.st_d == 0);
}

void t05_chunk_table_prot_none() {
    Fixture f = make_live_fixture(false, /*chunks_prot=*/0);
    std::vector<igu_span> sv = f.spans_vec();
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    CHECK(rc == IGU_E_NOT_READABLE);
    CHECK(igu_rung3_class(&rec) == 1); // covered_not_readable
    CHECK(rec.st_b == IGU_E_NOT_READABLE);
}

void t06_objects_ptr_noncanonical() {
    Fixture f = make_live_fixture();
    f.poke_u64(GUA_ADDR + 0x10, 0xFFFFFFFFFFFFFFF0ull);
    std::vector<igu_span> sv = f.spans_vec();
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    CHECK(rc == IGU_R_NONCANONICAL);
    CHECK(igu_rung3_class(&rec) == 2); // refuted: non-canonical forbids
    CHECK(rec.st_a == IGU_R_NONCANONICAL);
}

void t07_item_object_noncanonical() {
    Fixture f = make_live_fixture(/*drop=*/false, /*chunks_prot=*/3,
                                  /*drop_image=*/false,
                                  /*first_item_object=*/0xFFFFFFFFFFFFFFF0ull);
    std::vector<igu_span> sv = f.spans_vec();
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    CHECK(rc == IGU_R_NONCANONICAL);
    CHECK(igu_rung3_class(&rec) == 2);
    CHECK(rec.st_c == IGU_R_NONCANONICAL && rec.items_scanned == 1);
}

void t08_flags_outside_rf_mask() {
    Fixture f = make_live_fixture(0, 3, false, 0, /*obj_a_flags=*/0xF0000000ull);
    std::vector<igu_span> sv = f.spans_vec();
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    CHECK(rc == IGU_R_CHECK_FAIL);
    CHECK(igu_rung3_class(&rec) == 2);
    CHECK(rec.chk_flags == IGU_CSK_CONTRADICTED);
    CHECK(rec.chk_roundtrip == IGU_CSK_PASS); // unrelated checks still ran
}

void t09_index_negative() {
    Fixture f = make_live_fixture(0, 3, false, 0, 0x00030000ull,
                                  /*obj_a_idx=*/-1);
    std::vector<igu_span> sv = f.spans_vec();
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    CHECK(rc == IGU_R_CHECK_FAIL);
    CHECK(igu_rung3_class(&rec) == 2);
    CHECK(rec.chk_index_bounds == IGU_CSK_CONTRADICTED);
    CHECK(rec.chk_roundtrip == IGU_CSK_NOT_RUN);
}

void t10_roundtrip_mismatch() {
    // item[5] claims OBJ_A, but OBJ_A says index 6 while item[6] holds a
    // different pointer -> refuted round-trip.
    Fixture f = make_live_fixture(0, 3, false, 0, 0x00030000ull,
                                  /*obj_a_idx=*/6,
                                  /*item5_object_override=*/0x1380700fffull);
    std::vector<igu_span> sv = f.spans_vec();
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    CHECK(rc == IGU_R_CHECK_FAIL);
    CHECK(igu_rung3_class(&rec) == 2);
    CHECK(rec.chk_roundtrip == IGU_CSK_CONTRADICTED);
}

void t11_vtable_uncovered_is_inconclusive() {
    Fixture f = make_live_fixture();
    std::vector<igu_span> sv = spans_without(f, 0x400000);
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    CHECK(rc == IGU_E_NO_SPAN);
    CHECK(igu_rung3_class(&rec) == 1);
    CHECK(rec.chk_vptr == IGU_CSK_UNAVAILABLE);
    CHECK(rec.chk_flags == IGU_CSK_PASS && rec.chk_roundtrip == IGU_CSK_PASS);
}

void t12_vtable_prot_none_is_inconclusive() {
    Fixture f = make_live_fixture();
    // Point the vtable inside the dedicated covered-not-readable region.
    f.poke_u64(OBJ_A + 0, 0x1380900008ull);
    std::vector<igu_span> sv = f.spans_vec();
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    // Module policy: any UNAVAILABLE check => overall IGU_E_NO_SPAN
    // (coverage-class); the per-check field distinguishes
    // covered_not_readable from not_covered via a direct classification.
    CHECK(rc == IGU_E_NO_SPAN);
    CHECK(igu_rung3_class(&rec) == 1);
    CHECK(rec.chk_vptr == IGU_CSK_UNAVAILABLE);
    uint32_t prot = 0;
    CHECK(igu_find_covering(&t, 0x1380900008ull, 8, &prot) >= 0);
    CHECK(!(prot & IGU_PROT_R)); // covered_not_readable, not no-span
}

void t13_class_ptr_null_and_uncovered() {
    {
        Fixture f = make_live_fixture();
        f.poke_u64(OBJ_A + 0x10, 0); // ClassPrivate null
        std::vector<igu_span> sv = f.spans_vec();
        igu_span_table t = make_table(sv);
        igu_rung3_record rec;
        int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
        CHECK(rc == IGU_R_CHECK_FAIL);
        CHECK(igu_rung3_class(&rec) == 2);
        CHECK(rec.chk_class == IGU_CSK_CONTRADICTED);
    }
    {
        Fixture f = make_live_fixture();
        f.poke_u64(OBJ_A + 0x10, 0x9999999abcull); // uncovered
        std::vector<igu_span> sv = f.spans_vec();
        igu_span_table t = make_table(sv);
        igu_rung3_record rec;
        int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
        CHECK(rc == IGU_E_NO_SPAN);
        CHECK(igu_rung3_class(&rec) == 1); // coverage failure, never refute
        CHECK(rec.chk_class == IGU_CSK_UNAVAILABLE);
    }
}

void t14_outer_bad_vs_readable() {
    {
        Fixture f = make_live_fixture();
        f.poke_u64(OBJ_A + 0x20, 0xFFFFFFFFFFFFFFE0ull); // non-canonical
        std::vector<igu_span> sv = f.spans_vec();
        igu_span_table t = make_table(sv);
        igu_rung3_record rec;
        int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
        CHECK(rc == IGU_R_CHECK_FAIL);
        CHECK(rec.chk_outer == IGU_CSK_CONTRADICTED);
    }
    {
        Fixture f = make_live_fixture(/*drop=*/false, /*chunks_prot=*/3,
                                  /*drop_image=*/false,
                                  /*first_item_object=*/OBJ_B);
        std::vector<igu_span> sv = f.spans_vec();
        igu_span_table t = make_table(sv);
        igu_rung3_record rec;
        int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
        CHECK(rc == IGU_OK);
        CHECK(rec.outer_ptr == OBJ_A && rec.chk_outer == IGU_CSK_PASS);
    }
}

void t15_object_window_straddle() {
    Fixture f = make_live_fixture(false, 3, false, 0, 0, 5, 0, true);
    std::vector<igu_span> sv = f.spans_vec();
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    CHECK(rc == IGU_E_STRADDLE);
    CHECK(igu_rung3_class(&rec) == 1);
    CHECK(rec.st_d == IGU_E_STRADDLE);
}

void t16_read_api_classification() {
    Fixture f = make_live_fixture();
    std::vector<igu_span> sv = f.spans_vec();
    igu_span_table t = make_table(sv);
    uint64_t v = 0;
    // LE assembly correctness.
    CHECK(igu_read_u64(&t, GUA_ADDR + 0x10, &v) == IGU_OK);
    CHECK(v == 0x13800220a0ull);
    uint32_t u = 0;
    CHECK(igu_read_u32(&t, GUA_ADDR + 0x24, &u) == IGU_OK);
    CHECK(u == 71959u);
    // End-exclusivity: last byte of the GUA span is readable, one past the
    // end is not covered.
    CHECK(igu_read_u32(&t, GUA_SPAN + GUA_SPAN_SZ - 4, &u) == IGU_OK);
    CHECK(igu_read_u32(&t, GUA_SPAN + GUA_SPAN_SZ, &u) == IGU_E_NO_SPAN);
    // Straddle: GUA span ends at 0x9a34000; a window leaving it straddles.
    uint8_t buf[8];
    CHECK(igu_read(&t, GUA_SPAN + GUA_SPAN_SZ - 4, buf, 8) == IGU_E_STRADDLE);
    // PROT_NONE region: covered but not readable.
    CHECK(igu_read_u64(&t, CHUNKS_ADDR, &v) == IGU_OK); // prot 3 here
    Fixture g = make_live_fixture(false, /*chunks_prot=*/0);
    std::vector<igu_span> sgv = g.spans_vec();
    igu_span_table tg = make_table(sgv);
    CHECK(igu_read_u64(&tg, CHUNKS_ADDR, &v) == IGU_E_NOT_READABLE);
    // Arg guards.
    CHECK(igu_find_covering(&tg, CHUNKS_ADDR, 0, nullptr) == IGU_E_ARG);
    CHECK(igu_read(&t, CHUNKS_ADDR, nullptr, 8) == IGU_E_ARG);
}

void t17_header_uncovered() {
    Fixture f = make_live_fixture();
    std::vector<igu_span> sv = spans_without(f, GUA_SPAN);
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    CHECK(rc == IGU_E_NO_SPAN);
    CHECK(igu_rung3_class(&rec) == 1);
    CHECK(rec.st_hdr == IGU_E_NO_SPAN);
    CHECK(rec.st_a == 0 && rec.chk_vptr == IGU_CSK_NOT_RUN);
}

void t18_first_chunk_null_falls_through_to_chunk1() {
    Fixture f = make_live_fixture();
    f.poke_u64(OBJPTR_VAL + 0, 0); // chunk[0] = NULL
    std::vector<igu_span> sv = f.spans_vec();
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    CHECK(rc == IGU_OK);
    CHECK(rec.chunk_index == 1 && rec.chunk_ptr == CHUNK1);
    CHECK(rec.items_scanned == 1 && rec.item_object == OBJ_C);
    CHECK(rec.internal_index == 65536); // 65536 = chunk 1 slot 0
    CHECK(rec.chk_roundtrip == IGU_CSK_PASS);
}

void t19_gua_prot_none_is_inconclusive() {
    Fixture f = make_live_fixture(false, 3, false, 0, 0, 5, 0, false,
                                  /*gua_prot=*/0);
    std::vector<igu_span> sv = f.spans_vec();
    igu_span_table t = make_table(sv);
    igu_rung3_record rec;
    int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
    CHECK(rc == IGU_E_NOT_READABLE);
    CHECK(rec.st_hdr == IGU_E_NOT_READABLE);
    CHECK(igu_rung3_class(&rec) == 1);
}

void t20_posix_mode_translation() {
    // The probe-side logs carry POSIX mode bits; the module requires KVME
    // bits. The validated GUA span was logged "prot 6" (POSIX RW): fed in
    // raw it is W|X under KVME and correctly classifies NOT readable;
    // translated it is R|W and reads succeed.
    CHECK(igu_posix_mode_to_kvme(6) == 3u);   // RW  -> R|W
    CHECK(igu_posix_mode_to_kvme(5) == 5u);   // RX  -> R|X
    CHECK(igu_posix_mode_to_kvme(4) == 1u);   // R   -> R
    CHECK(igu_posix_mode_to_kvme(7) == 7u);   // RWX -> RWX
    {
        Fixture f = make_live_fixture(false, 3, false, 0, 0, 5, 0, false,
                                      /*gua_prot=*/6); // POSIX 6 fed raw
        std::vector<igu_span> sv = f.spans_vec();
        igu_span_table t = make_table(sv);
        igu_rung3_record rec;
        int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
        CHECK(rc == IGU_E_NOT_READABLE);
        CHECK(rec.st_hdr == IGU_E_NOT_READABLE);
    }
    {
        Fixture f = make_live_fixture(); // KVME-encoded happy path
        std::vector<igu_span> sv = f.spans_vec();
        sv[0].prot = igu_posix_mode_to_kvme(6); // translated GUA span
        igu_span_table t = make_table(sv);
        igu_rung3_record rec;
        int rc = igu_probe_rung3(&t, GUA_ADDR, IGU_SCAN_MAX_ITEMS, &rec);
        CHECK(rc == IGU_OK);
        CHECK(igu_rung3_class(&rec) == 0);
    }
}

} // namespace

int main() {
    t01_happy_path_live_shaped();
    t02_scan_bound_exhausted_is_inconclusive();
    t03_roundtrip_cross_chunk_via_item0();
    t04_chunk_table_not_covered();
    t05_chunk_table_prot_none();
    t06_objects_ptr_noncanonical();
    t07_item_object_noncanonical();
    t08_flags_outside_rf_mask();
    t09_index_negative();
    t10_roundtrip_mismatch();
    t11_vtable_uncovered_is_inconclusive();
    t12_vtable_prot_none_is_inconclusive();
    t13_class_ptr_null_and_uncovered();
    t14_outer_bad_vs_readable();
    t15_object_window_straddle();
    t16_read_api_classification();
    t17_header_uncovered();
    t18_first_chunk_null_falls_through_to_chunk1();
    t19_gua_prot_none_is_inconclusive();
    t20_posix_mode_translation();
    std::printf("%d/%d PASS\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
