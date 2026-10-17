// test_rung3_addon.cpp
//
// HOST TEST SUITE for the rung-3 chunk/object add-on (worker-rung3-chunk-probe).
// Conventions: same as the batch2/batch3 suites (-Wall -Wextra -Werror, ASan+
// UBSan runs, committed logs, two-run determinism diff).
//
// Fixtures are SYNTHETIC beyond the chunk-table pointer: the only live-derived
// numbers are the already-validated GUA header set (GUA 0x97ba110 in span
// [0x970c000,0x9a34000), Objects** = 0x13800220a0, MaxElements 2162688,
// NumElements 71959, MaxChunks 33, NumChunks 2, first_gc 35477,
// last_non_gc 35476 — worker-staged-pair-live / worker-anchor-validation).
// Chunk/object storage addresses (0x1381.., 0x1382.., 0x1383.., ...) are
// synthetic stand-ins for addresses only the next console probe can observe.
//
// The synthetic kernel VMMAP buffer uses variable kve_structsize records
// (144-byte path-stripped records, one 152-byte record) per the recovered
// map shape (worker-span-arbitration), with KVME-encoded prot words.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

#include "rung3_chunk_addon.h"
#include "rung3_batch2_bridge.hpp"

// ---------------------------------------------------------------------------
// Live-shaped constants (cited provenance in rung3_chunk_addon.h)
// ---------------------------------------------------------------------------
static const uint64_t GUA        = 0x97ba110ull;
static const uint64_t GUA_SP_LO  = 0x970c000ull;   // captured GUA span
static const uint64_t GUA_SP_HI  = 0x9a34000ull;
static const uint64_t OBJTABLE   = 0x13800220a0ull; // live Objects** value
static const uint64_t CHUNK0     = 0x1381000000ull; // synthetic storage below
static const uint64_t CHUNK1     = 0x1382000000ull;
static const uint64_t OBJ_PLAIN  = 0x1383000000ull;
static const uint64_t STRUCT_A   = 0x1383000400ull;
static const uint64_t STRUCT_B   = 0x1383000800ull;
static const uint64_t UCLASS_C   = 0x1383000c00ull;
static const uint64_t OBJ2       = 0x1384000000ull;
static const uint64_t CLASSP     = 0x1385000000ull;
static const uint64_t CDO        = 0x1386000000ull;
static const uint64_t VTPAGE     = 0x1390000000ull;
static const uint64_t IMG_LO     = 0x400000ull;     // X-only image stand-in
static const uint64_t IMG_HI     = 0x401000ull;

static const uint32_t FIRST_GC = 35477u, LAST_NON_GC = 35476u;
static const uint32_t MAX_ELEM = 2162688u, NUM_ELEM = 71959u;
static const uint32_t MAX_CHUNKS = 33u, NUM_CHUNKS = 2u;

// KVME prot words.
static const uint32_t KVME_RW = 3u, KVME_R = 1u, KVME_X = 4u, KVME_W = 2u;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

struct FsSpan {
    uint64_t start, end;
    uint32_t prot;
    std::vector<uint8_t> bytes;
    FsSpan(uint64_t s, uint64_t e, uint32_t p)
        : start(s), end(e), prot(p), bytes(size_t(e - s), 0) {}
    size_t off(uint64_t a) const { return size_t(a - start); }
    // Host storage may exceed the MAP extent (start..end is what the walk
    // records and what igu_read honors); writers auto-extend so a shrunken
    // span (straddle test) still holds coherent bytes. Never shrinks.
    void ensure(size_t n) { if (bytes.size() < n) bytes.resize(n, 0); }
    void wr64(uint64_t a, uint64_t v) {
        ensure(off(a) + 8);
        for (int i = 0; i < 8; ++i) bytes[off(a) + i] = uint8_t(v >> (8 * i));
    }
    void wr32(uint64_t a, uint32_t v) {
        ensure(off(a) + 4);
        for (int i = 0; i < 4; ++i) bytes[off(a) + i] = uint8_t(v >> (8 * i));
    }
    void wrname(uint64_t a) {
        ensure(off(a) + 8);
        for (int i = 0; i < 8; ++i) bytes[off(a) + i] = uint8_t(0x11 + i);
    }
};

struct Fixture {
    std::vector<FsSpan> spans;
    std::vector<uint8_t> kbuf;

    FsSpan* find(uint64_t start) {
        for (auto& s : spans)
            if (s.start == start) return &s;
        return nullptr;
    }
    FsSpan& add(uint64_t s, uint64_t e, uint32_t p) {
        spans.push_back(FsSpan(s, e, p));
        return spans.back();
    }
    void add_krec(uint64_t s, uint64_t e, uint32_t p, uint32_t ksz = 144u) {
        std::vector<uint8_t> r(ksz, 0);
        auto st32 = [&](size_t o, uint32_t v) {
            for (int i = 0; i < 4; ++i) r[o + i] = uint8_t(v >> (8 * i));
        };
        auto st64 = [&](size_t o, uint64_t v) {
            for (int i = 0; i < 8; ++i) r[o + i] = uint8_t(v >> (8 * i));
        };
        st32(0, ksz);          // kve_structsize
        st64(8, s);            // kve_start
        st64(16, e);           // kve_end
        st32(56, p);           // kve_protection (KVME bits)
        kbuf.insert(kbuf.end(), r.begin(), r.end());
    }
    // Publish every span into the walk buffer as a kernel record.
    void materialize_kernel_records() {
        for (const auto& s : spans) add_krec(s.start, s.end, s.prot);
    }
    void add_dummies(uint32_t count, uint64_t base, uint32_t prot) {
        for (uint32_t i = 0; i < count; ++i)
            add_krec(base + uint64_t(i) * 0x4000,
                     base + uint64_t(i) * 0x4000 + 0x4000, prot);
    }
};

static uint64_t span_base(uint64_t a) {
    if (a >= OBJ_PLAIN && a < STRUCT_A) return OBJ_PLAIN;
    if (a >= STRUCT_A && a < STRUCT_A + 0x100) return STRUCT_A;
    if (a >= STRUCT_B && a < STRUCT_B + 0x100) return STRUCT_B;
    if (a >= UCLASS_C && a < UCLASS_C + 0x140) return UCLASS_C;
    return a;  // span-aligned addresses (chunk bases, page bases)
}

// UObjectBase (SET-5_06: vptr 0x0, flags 0x8, index 0xC, class 0x10,
// name 0x18 raw, outer 0x20) + optional UStruct fields at 0x40..0x50.
static void write_uobject(Fixture& f, uint64_t a, uint64_t vptr,
                          uint32_t flags, uint32_t idx, uint64_t cls,
                          uint64_t outer) {
    FsSpan* s = f.find(span_base(a));
    if (s == nullptr) return;  // span omitted by the variant under test
    s->wr64(a + 0x00, vptr);
    s->wr32(a + 0x08, flags);
    s->wr32(a + 0x0C, idx);
    s->wr64(a + 0x10, cls);
    s->wrname(a + 0x18);                    // RAW 8B, never interpreted
    s->wr64(a + 0x20, outer);
}
static void write_ustruct(Fixture& f, uint64_t a, uint64_t super,
                          uint64_t children, uint64_t childprops) {
    FsSpan* s = f.find(span_base(a));
    if (s == nullptr) return;
    s->wr64(a + 0x40, super);
    s->wr64(a + 0x48, children);
    s->wr64(a + 0x50, childprops);
}
static void write_item(Fixture& f, uint64_t chunk_base, uint32_t slot,
                       uint64_t object, uint32_t flags, uint32_t cluster,
                       uint32_t serial) {
    FsSpan* s = f.find(chunk_base);
    if (s == nullptr) return;
    const uint64_t a = chunk_base + uint64_t(slot) * 0x18ull;
    s->wr64(a + 0x00, object);
    s->wr32(a + 0x08, flags);
    s->wr32(a + 0x0C, cluster);
    s->wr32(a + 0x10, serial);
}

struct BuildOpts {
    bool chunktable = true, chunk0 = true, chunk1 = true;
    bool objplain = true, struct_a = true, struct_b = true, uclass_c = true;
    uint32_t chunktable_prot = KVME_RW;
    uint64_t objplain_end = OBJ_PLAIN + 0x400;   // shrink for the straddle test
    uint64_t plain_vptr = VTPAGE + 0x100;
    uint64_t plain_super = 0x7;                  // non-canonical: not struct-like
    uint64_t objects_value = OBJTABLE;
    uint32_t num_chunks_field = NUM_CHUNKS;
    uint32_t num_elements_field = NUM_ELEM;
};

// Build the standard fixture. Order matters for the early-stop test: dummies
// first (retained>=8 before the needed spans), needed spans, dummies after
// (must NOT be consumed when all targets are covered).
static Fixture build_fixture(const BuildOpts& o) {
    Fixture f;
    // GUA span: live extent; fixture bytes only at the GUA window.
    f.add(GUA_SP_LO, GUA_SP_HI, KVME_RW);
    if (o.chunktable) f.add(0x1380022000ull, 0x1380022200ull, o.chunktable_prot);
    if (o.chunk0)     f.add(CHUNK0, CHUNK0 + 0x1000, KVME_RW);
    if (o.chunk1)     f.add(CHUNK1, CHUNK1 + 0x1000, KVME_RW);
    if (o.objplain)   f.add(OBJ_PLAIN, o.objplain_end, KVME_RW);
    if (o.struct_a)   f.add(STRUCT_A, STRUCT_A + 0x100, KVME_RW);
    if (o.struct_b)   f.add(STRUCT_B, STRUCT_B + 0x100, KVME_RW);
    if (o.uclass_c)   f.add(UCLASS_C, UCLASS_C + 0x140, KVME_RW);
    f.add(OBJ2, OBJ2 + 0x100, KVME_RW);
    f.add(CLASSP, CLASSP + 0x1000, KVME_RW);
    f.add(CDO, CDO + 0x1000, KVME_RW);
    f.add(VTPAGE, VTPAGE + 0x1000, KVME_R);
    f.add(IMG_LO, IMG_HI, KVME_X);   // X-only image stand-in (KVME prot 4)

    // GUA header (layout A), live observed values.
    FsSpan* g = f.find(GUA_SP_LO);
    g->wr32(GUA + 0x00, FIRST_GC);
    g->wr32(GUA + 0x04, LAST_NON_GC);
    g->wr32(GUA + 0x20, MAX_ELEM);
    g->wr32(GUA + 0x24, o.num_elements_field);
    g->wr32(GUA + 0x28, MAX_CHUNKS);
    g->wr32(GUA + 0x2C, o.num_chunks_field);
    g->wr64(GUA + 0x10, o.objects_value);

    // Chunk table: both live-needed chunks.
    if (o.chunktable) {
        FsSpan* ct = f.find(0x1380022000ull);
        ct->wr64(OBJTABLE + 0, o.chunk0 ? CHUNK0 : 0ull);
        ct->wr64(OBJTABLE + 8, o.chunk1 ? CHUNK1 : 0ull);
    }
    // Chunk items. Chunk0: slots 0..4 null, 5/6/7 populated (live-shaped:
    // first 64 slots of chunk0 hold the first objects). Chunk1: slot 3.
    if (o.chunk0 && o.objplain)
        write_item(f, CHUNK0, 5, OBJ_PLAIN, 0x01020304u, 0xAABBCCDDu, 0x11111111u);
    if (o.chunk0 && o.struct_a)
        write_item(f, CHUNK0, 6, STRUCT_A, 1u, 0u, 0x22222222u);
    if (o.chunk0 && o.uclass_c)
        write_item(f, CHUNK0, 7, UCLASS_C, 1u, 0u, 0x33333333u);
    if (o.chunk1)
        write_item(f, CHUNK1, 3, OBJ2, 1u, 0u, 0x44444444u);

    // Objects. obj_plain: plausible UObjectBase, NOT struct-like (super 0x7).
    if (o.objplain) {
        write_uobject(f, OBJ_PLAIN, o.plain_vptr, 0x00090000u, 5u, CLASSP, 0ull);
        write_ustruct(f, OBJ_PLAIN, o.plain_super, 0ull, 0ull);
    }
    if (o.struct_a) {
        write_uobject(f, STRUCT_A, VTPAGE + 0x140, 0x00030000u, 6u, CLASSP, 0ull);
        write_ustruct(f, STRUCT_A, STRUCT_B, 0ull, 0ull);
    }
    if (o.struct_b) {
        write_uobject(f, STRUCT_B, VTPAGE + 0x180, 0x00030000u, 0xFFFFFFu, CLASSP, 0ull);
        write_ustruct(f, STRUCT_B, 0ull, 0ull, 0ull);
    }
    if (o.uclass_c) {
        write_uobject(f, UCLASS_C, VTPAGE + 0x1C0, 0x00030000u, 7u, CLASSP, 0ull);
        write_ustruct(f, UCLASS_C, STRUCT_B, 0ull, 0ull);
        FsSpan* s = f.find(UCLASS_C);
        s->wr64(UCLASS_C + 0x110, CDO);       // ClassDefaultObject
        s->wr64(UCLASS_C + 0x118, 0ull);      // SparseClassData
        s->wr64(UCLASS_C + 0x120, 0ull);      // SparseClassDataStruct
        s->wr32(UCLASS_C + 0x128, 0x11223344u); // FuncMap raw u32
    }
    {
        FsSpan* s = f.find(OBJ2);
        s->wr64(OBJ2 + 0x00, VTPAGE + 0x200);
        s->wr32(OBJ2 + 0x08, 0x00020000u);
        s->wr32(OBJ2 + 0x0C, 65536u + 3u);    // 65539: chunk1 slot 3
        s->wr64(OBJ2 + 0x10, CLASSP);
        s->wrname(OBJ2 + 0x18);
        s->wr64(OBJ2 + 0x20, 0ull);
    }

    f.add_dummies(12, 0x50000000ull, KVME_RW);  // pre: retained>=8 gate
    f.materialize_kernel_records();             // needed spans
    f.add_dummies(20, 0x60000000ull, KVME_RW);  // tail: early-stop skips
    return f;
}

// Walk targets: GUA + chunk table + both chunk windows + object/class/cdo/
// vtable page bases (the probe's read set).
static void default_targets(uint64_t* t, uint32_t& n) {
    n = 0;
    t[n++] = GUA;
    t[n++] = OBJTABLE;
    t[n++] = CHUNK0;
    t[n++] = CHUNK1;
    t[n++] = OBJ_PLAIN;
    t[n++] = UCLASS_C;
    t[n++] = CLASSP;
    t[n++] = CDO;
    t[n++] = VTPAGE;
}

// Walk + re-point span data into fixture bytes (host discipline).
static uint32_t walk_fixture(Fixture& f, igu_span* out, uint32_t cap,
                             rc3_vmstats& st, const uint64_t* targets,
                             uint32_t ntargets) {
    const uint32_t n = rc3_vmmap_walk_min(f.kbuf.data(), f.kbuf.size(), 1160u,
                                          out, cap, targets, ntargets, &st);
    for (uint32_t i = 0; i < n; ++i) {
        FsSpan* fs = f.find(out[i].start);
        if (fs != nullptr) out[i].data = fs->bytes.data();
    }
    return n;
}

// ---------------------------------------------------------------------------
// Harness
// ---------------------------------------------------------------------------
static int g_tests_run = 0, g_tests_failed = 0, g_asserts = 0, g_fails = 0,
           g_total_fails = 0;
#define CHECK(cond) do { \
    ++g_asserts; \
    if (!(cond)) { ++g_fails; ++g_total_fails; \
        std::printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

static void begin(const char* name) {
    ++g_tests_run;
    std::printf("[t%02d] %s\n", g_tests_run, name);
}
static void end() {
    if (g_fails == 0) std::printf("    PASS (%d asserts)\n", g_asserts);
    else { ++g_tests_failed; std::printf("    FAIL (%d asserts failed)\n", g_fails); }
    g_fails = 0;
}

static const uint32_t SPAN_CAP = 4096u;

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void t01_walk_and_happy_probe() {
    begin("walk trust + happy probe: chunk0/chunk1 items -> UObjectBase, all checks PASS");
    Fixture f = build_fixture(BuildOpts{});
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    CHECK(st.trustworthy == 1);
    CHECK(st.stop_reason == RC3_WALK_STOP_TARGETS);   // covered before tail
    CHECK(st.targets_covered == nt);
    CHECK(st.ksz_144 >= 21u);                          // path-stripped records
    CHECK(st.insane == 0u);

    igu_span_table table{spans, n};
    rc3_record rec;
    const int rc = rc3_probe_chunks(&table, &st, GUA, 0, &rec);
    CHECK(rc == IGU_OK);
    CHECK(rc3_class(&rec) == 0);
    CHECK(rec.map_trusted == 1);
    CHECK(rec.st_hdr == IGU_OK);
    CHECK(rec.first_gc == FIRST_GC && rec.last_non_gc == LAST_NON_GC);
    CHECK(rec.max_elements == MAX_ELEM && rec.num_elements == NUM_ELEM);
    CHECK(rec.max_chunks == MAX_CHUNKS && rec.num_chunks == NUM_CHUNKS);
    CHECK(rec.info_max_eq_chunks == IGU_CSK_PASS);
    CHECK(rec.info_num_le_max == IGU_CSK_PASS);
    CHECK(rec.info_nchunks_ok == IGU_CSK_PASS);
    CHECK(rec.st_a == IGU_OK && rec.objects_ptr == OBJTABLE);
    // chunk[0]: pointer, first non-null item at slot 5.
    CHECK(rec.chunk[0].st_ptr == IGU_OK && rec.chunk[0].ptr == CHUNK0);
    CHECK(rec.chunk[0].chk_needed == IGU_CSK_PASS);
    CHECK(rec.chunk[0].st_item == IGU_OK);
    CHECK(rec.chunk[0].item_object == OBJ_PLAIN);
    CHECK(rec.chunk[0].item_flags == 0x01020304u);     // raw, never judged
    CHECK(rec.chunk[0].item_cluster == 0xAABBCCDDu);
    CHECK(rec.chunk[0].item_serial == 0x11111111u);
    CHECK(rec.chunk[0].items_scanned == 6u);
    CHECK(rec.chunk[0].item_nonnull == 1);
    // chunk[1]: pointer + first non-null item at slot 3.
    CHECK(rec.chunk[1].st_ptr == IGU_OK && rec.chunk[1].ptr == CHUNK1);
    CHECK(rec.chunk[1].chk_needed == IGU_CSK_PASS);
    CHECK(rec.chunk[1].st_item == IGU_OK);
    CHECK(rec.chunk[1].item_object == OBJ2);
    CHECK(rec.chunk[1].items_scanned == 4u);
    // UObjectBase behind the first non-null item across chunks (chunk0 wins).
    CHECK(rec.st_obj == IGU_OK && rec.obj_source_chunk == 0);
    CHECK(rec.obj_addr == OBJ_PLAIN);
    CHECK(rec.vptr == VTPAGE + 0x100);
    CHECK(rec.obj_flags == 0x00090000u);
    CHECK(rec.internal_index == 5);
    CHECK(rec.class_ptr == CLASSP);
    for (int i = 0; i < 8; ++i) CHECK(rec.name_raw[i] == uint8_t(0x11 + i));
    CHECK(rec.outer_ptr == 0ull);
    CHECK(rec.chk_vptr == IGU_CSK_PASS && rec.chk_flags == IGU_CSK_PASS);
    CHECK(rec.chk_index_bounds == IGU_CSK_PASS);
    CHECK(rec.chk_roundtrip == IGU_CSK_PASS);
    CHECK(rec.chk_class == IGU_CSK_PASS && rec.chk_outer == IGU_CSK_PASS);
    end();
}

static void t02_chunktable_gap() {
    begin("chunk table not in walked map -> Inconclusive, exact missing fact");
    BuildOpts o{};
    o.chunktable = false;
    Fixture f = build_fixture(o);
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    CHECK(st.trustworthy == 1);
    CHECK(st.stop_reason == RC3_WALK_STOP_END);        // never early-stopped
    CHECK((st.targets_mask & (1u << 1)) == 0u);        // OBJTABLE bit clear
    CHECK(st.targets_covered == nt - 1u);

    igu_span_table table{spans, n};
    rc3_record rec;
    const int rc = rc3_probe_chunks(&table, &st, GUA, 0, &rec);
    CHECK(rec.st_hdr == IGU_OK && rec.st_a == IGU_OK); // header still proves
    CHECK(rec.chunk[0].st_ptr == IGU_E_NO_SPAN);       // not_covered_current_map
    CHECK(rec.chunk[1].st_ptr == IGU_E_NO_SPAN);
    CHECK(rec.chunk[0].chk_needed == IGU_CSK_UNAVAILABLE);
    CHECK(rc != IGU_OK);
    CHECK(rc3_class(&rec) == 1);                       // Inconclusive, no guess
    end();
}

static void t04_early_stop_minimal() {
    begin("minimal walk: early stop once targets covered; tail never consumed");
    Fixture f = build_fixture(BuildOpts{});
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    // 12 dummies + the needed spans; the walk stops right after the vtable
    // page (the last-covered target), so the X-only image record and the 20
    // tail dummies stay unconsumed.
    CHECK(st.stop_reason == RC3_WALK_STOP_TARGETS);
    CHECK(st.total == 24u);
    CHECK(st.retained == 24u);
    CHECK(st.truncated == 0u);
    CHECK(st.targets_mask == (nt >= 32u ? 0xFFFFFFFFu : ((1u << nt) - 1u)));
    // The probe still fully validates on the minimal table.
    igu_span_table table{spans, n};
    rc3_record rec;
    CHECK(rc3_probe_chunks(&table, &st, GUA, 0, &rec) == IGU_OK);
    CHECK(rc3_class(&rec) == 0);
    end();
}

static void t05_covered_not_readable() {
    begin("chunk table covered but W-only (KVME) -> covered_not_readable");
    BuildOpts o{};
    o.chunktable_prot = KVME_W;
    Fixture f = build_fixture(o);
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    CHECK((st.targets_mask & (1u << 1)) == 0u);        // READ coverage required
    igu_span_table table{spans, n};
    rc3_record rec;
    rc3_probe_chunks(&table, &st, GUA, 0, &rec);
    CHECK(rec.chunk[0].st_ptr == IGU_E_NOT_READABLE);
    CHECK(rc3_class(&rec) == 1);
    end();
}

static void t06_straddle() {
    begin("UObjectBase window straddles span end -> IGU_E_STRADDLE");
    BuildOpts o{};
    o.objplain_end = OBJ_PLAIN + 0x10;                 // cuts the 0x28 window
    Fixture f = build_fixture(o);
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    igu_span_table table{spans, n};
    rc3_record rec;
    rc3_probe_chunks(&table, &st, GUA, 0, &rec);
    CHECK(rec.chunk[0].st_item == IGU_OK);             // item read succeeded
    CHECK(rec.st_obj == IGU_E_STRADDLE);               // object window partial
    CHECK(rc3_class(&rec) == 1);
    end();
}

static void t07_noncanonical_objects() {
    begin("non-canonical Objects** -> Refuted");
    BuildOpts o{};
    o.objects_value = 0xC000000000000001ull;
    Fixture f = build_fixture(o);
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    igu_span_table table{spans, n};
    rc3_record rec;
    CHECK(rc3_probe_chunks(&table, &st, GUA, 0, &rec) == IGU_R_NONCANONICAL);
    CHECK(rec.st_a == IGU_R_NONCANONICAL);
    CHECK(rec.chunk[0].st_ptr == RC3_ST_NOT_RUN);      // never attempted
    CHECK(rc3_class(&rec) == 2);
    end();
}

static void t08_chunk0_null_needed() {
    begin("chunk[0] NULL while needed -> Refuted (worker-5 rule); chunk1 evidence kept");
    BuildOpts o{};
    o.chunk0 = false;                                  // table[0] = 0
    Fixture f = build_fixture(o);
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    igu_span_table table{spans, n};
    rc3_record rec;
    rc3_probe_chunks(&table, &st, GUA, 0, &rec);
    CHECK(rec.chunk[0].st_ptr == IGU_R_SKIPPED);
    CHECK(rec.chunk[0].chk_needed == IGU_CSK_CONTRADICTED);  // 71959 > 65536
    CHECK(rec.chunk[1].st_ptr == IGU_OK);              // partial evidence kept
    CHECK(rec.chunk[1].item_object == OBJ2);
    CHECK(rec.obj_source_chunk == 1);                  // object from chunk1
    CHECK(rec.st_obj == IGU_OK);
    CHECK(rc3_class(&rec) == 2);                       // refute wins
    end();
}

static void t09_numchunks1() {
    begin("num_chunks=1 -> chunk[1] NOT_RUN (neutral), probe still Validated-class");
    BuildOpts o{};
    o.chunk1 = false;
    o.num_chunks_field = 1u;
    Fixture f = build_fixture(o);
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    igu_span_table table{spans, n};
    rc3_record rec;
    CHECK(rc3_probe_chunks(&table, &st, GUA, 0, &rec) == IGU_OK);
    CHECK(rec.chunk[0].st_ptr == IGU_OK);
    CHECK(rec.chunk[1].st_ptr == RC3_ST_NOT_RUN);      // not commissioned
    CHECK(rec.chunk[1].st_item == RC3_ST_NOT_RUN);
    CHECK(rec.chunk[1].chk_needed == IGU_CSK_NOT_RUN);
    CHECK(rc3_class(&rec) == 0);
    end();
}

static void t10_chunk1_gap_partial_evidence() {
    begin("chunk[1] window uncovered -> chunk0 evidence kept, class Inconclusive");
    BuildOpts o{};
    o.chunk1 = false;                                  // no window, ptr still set
    Fixture f = build_fixture(o);
    f.find(0x1380022000ull)->wr64(OBJTABLE + 8, CHUNK1); // ptr claims chunk1
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    igu_span_table table{spans, n};
    rc3_record rec;
    rc3_probe_chunks(&table, &st, GUA, 0, &rec);
    CHECK(rec.chunk[0].st_ptr == IGU_OK && rec.chunk[0].st_item == IGU_OK);
    CHECK(rec.chunk[1].st_ptr == IGU_OK);              // pointer itself read fine
    CHECK(rec.chunk[1].st_item == IGU_E_NO_SPAN);      // item window uncovered
    CHECK(rec.chunk[1].items_scanned == 0u);
    CHECK(rc3_class(&rec) == 1);                       // commissioned set incomplete
    end();
}

static void t11_null_scan_exhaustion() {
    begin("all items null in both chunks -> exhausted scan, Inconclusive");
    Fixture f = build_fixture(BuildOpts{});
    FsSpan* c0 = f.find(CHUNK0);
    std::memset(c0->bytes.data() + 5 * 0x18, 0, 0x18 * 3);  // slots 5,6,7
    FsSpan* c1 = f.find(CHUNK1);
    std::memset(c1->bytes.data(), 0, 0x18 * 4);             // slot 3
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    igu_span_table table{spans, n};
    rc3_record rec;
    rc3_probe_chunks(&table, &st, GUA, 64, &rec);
    CHECK(rec.chunk[0].st_item == IGU_R_NULL);
    CHECK(rec.chunk[0].items_scanned == 64u);
    CHECK(rec.chunk[1].st_item == IGU_R_NULL);
    CHECK(rec.st_obj == IGU_R_NULL);
    CHECK(rc3_class(&rec) == 1);
    end();
}

static void t12_posix_translation() {
    begin("POSIX prot-6 span translated via igu_posix_mode_to_kvme -> probe passes");
    Fixture f = build_fixture(BuildOpts{});
    // Manual table (no walk), POSIX mode bits like old probe-side logs.
    static igu_span manual[13];
    uint32_t n = 0;
    for (const auto& s : f.spans) {
        manual[n].start = s.start;
        manual[n].end = s.end;
        manual[n].prot = igu_posix_mode_to_kvme(6u);   // RW -> KVME R|W = 3
        manual[n].data = s.bytes.data();
        ++n;
    }
    igu_span_table table{manual, n};
    rc3_vmstats vm{};
    vm.retained = n;
    vm.insane = 0;
    vm.total = n;
    vm.walk_mode = 1;
    CHECK(rc3_vmmap_trustworthy(&vm) == 1);
    rc3_record rec;
    CHECK(rc3_probe_chunks(&table, &vm, GUA, 0, &rec) == IGU_OK);
    CHECK(rc3_class(&rec) == 0);
    end();
}

static void t13_insane_structsize() {
    begin("insane kve_structsize -> walk stops, trust gate fails, probe refuses all reads");
    Fixture f = build_fixture(BuildOpts{});
    // Corrupt record 0's structsize in the kernel buffer.
    f.kbuf[0] = 0x03; f.kbuf[1] = 0x00; f.kbuf[2] = 0x00; f.kbuf[3] = 0x00;
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = rc3_vmmap_walk_min(f.kbuf.data(), f.kbuf.size(), 1160u,
                                          spans, SPAN_CAP, tg, nt, &st);
    CHECK(n == 0u);
    CHECK(st.stop_reason == RC3_WALK_STOP_INSANE);
    CHECK(st.trustworthy == 0);
    igu_span_table table{spans, n};
    rc3_record rec;
    const int rc = rc3_probe_chunks(&table, &st, GUA, 0, &rec);
    CHECK(rc == RC3_ST_NO_MAP);
    CHECK(rec.map_trusted == 0);
    CHECK(rec.st_hdr == RC3_ST_NO_MAP);
    CHECK(rc3_class(&rec) == 1);
    end();
}

static void t14_determinism() {
    begin("two-run determinism: walk + probe byte-identical");
    static igu_span a[SPAN_CAP], b[SPAN_CAP];
    rc3_vmstats sa, sb;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    {
        Fixture f1 = build_fixture(BuildOpts{});
        Fixture f2 = build_fixture(BuildOpts{});
        const uint32_t na = walk_fixture(f1, a, SPAN_CAP, sa, tg, nt);
        const uint32_t nb = walk_fixture(f2, b, SPAN_CAP, sb, tg, nt);
        CHECK(na == nb);
        // Span VALUES must match byte-for-byte. span->data are per-fixture
        // host pointers (and identity vaddrs for walk-retained dummy spans
        // that have no fixture bytes), so data itself is NOT dereferenced
        // here; the fixtures' kernel buffers are compared instead.
        bool spans_equal = true;
        for (uint32_t i = 0; i < na; ++i) {
            if (a[i].start != b[i].start || a[i].end != b[i].end ||
                a[i].prot != b[i].prot) {
                spans_equal = false;
            }
        }
        CHECK(spans_equal);
        CHECK(f1.kbuf.size() == f2.kbuf.size());
        CHECK(std::memcmp(f1.kbuf.data(), f2.kbuf.data(), f1.kbuf.size()) == 0);
        CHECK(std::memcmp(&sa, &sb, sizeof(sa)) == 0);
        igu_span_table ta{a, na}, tb{b, nb};
        rc3_record ra, rb;
        CHECK(rc3_probe_chunks(&ta, &sa, GUA, 0, &ra) ==
              rc3_probe_chunks(&tb, &sb, GUA, 0, &rb));
        CHECK(std::memcmp(&ra, &rb, sizeof(ra)) == 0);
        const rc3b::Batch2Result ba =
            rc3b::run_ustruct_chain(ra, sa, a, na);
        const rc3b::Batch2Result bb =
            rc3b::run_ustruct_chain(rb, sb, b, nb);
        CHECK(std::memcmp(&ba, &bb, sizeof(ba)) == 0);
    }
    end();
}

static void t15_bridge_ustruct_uclass() {
    begin("bridge: probe_ustruct_chain Validated; UStruct anchor + UClass CDO anchor");
    Fixture f = build_fixture(BuildOpts{});
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    igu_span_table table{spans, n};
    rc3_record rec;
    CHECK(rc3_probe_chunks(&table, &st, GUA, 0, &rec) == IGU_OK);

    tlp::MemSpan views[512];
    size_t dropped = 0;
    const size_t published = rc3b::publish_memspans(spans, n, views, 512, &dropped);
    CHECK(published == n);      // every walked span is readable in this fixture
    CHECK(dropped == 0);
    CHECK(rc3b::publish_memspans(nullptr, 0, views, 512, &dropped) == 0);

    const rc3b::Batch2Result r = rc3b::run_ustruct_chain(rec, st, spans, n);
    CHECK(r.ran == 1u && r.gate == rc3b::BRIDGE_GATE_OK);
    CHECK(r.verdict == uint8_t(tlp::Verdict::Validated));
    CHECK(r.items_examined == 3u);                     // slots 5,6,7 non-null
    CHECK(r.plausible_uobject == 3u);
    CHECK(r.roundtrips_ok == 3u);
    CHECK(r.struct_like_count == 2u);                  // STRUCT_A + UCLASS_C
    CHECK(r.first_plausible == OBJ_PLAIN);
    CHECK(r.first_ustruct_like == STRUCT_A);
    CHECK(r.first_uclass_like == UCLASS_C);            // CDO@0x110 = CDO span
    CHECK(r.walk_truncated == 1u);                     // 71959 > 64 cap
    bool saw_ext_ok = false, saw_ok = false, saw_super_bad = false;
    for (uint8_t i = 0; i < r.code_count; ++i) {
        if (r.codes[i] == uint16_t(rfl::RCode::STR_I_UCLASS_EXT_OK)) saw_ext_ok = true;
        if (r.codes[i] == uint16_t(rfl::RCode::OK)) saw_ok = true;
        if (r.codes[i] == uint16_t(rfl::RCode::STR_R_SUPER_BAD)) saw_super_bad = true;
    }
    CHECK(saw_ext_ok && saw_ok);
    CHECK(saw_super_bad);   // obj_plain's +0x40 garbage: negative evidence only
    end();
}

static void t16_bridge_gate_step() {
    begin("bridge gate: chunk-table gap record -> predicate does NOT run");
    BuildOpts o{};
    o.chunktable = false;
    Fixture f = build_fixture(o);
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    igu_span_table table{spans, n};
    rc3_record rec;
    rc3_probe_chunks(&table, &st, GUA, 0, &rec);
    const rc3b::Batch2Result r = rc3b::run_ustruct_chain(rec, st, spans, n);
    CHECK(r.ran == 0u);
    CHECK(r.gate == rc3b::BRIDGE_GATE_STEP_FAILED);
    CHECK(r.verdict == uint8_t(tlp::Verdict::NotProbed));
    end();
}

static void t17_bridge_gate_info() {
    begin("bridge gate: info-check contradiction blocks handoff (probe non-gating)");
    BuildOpts o{};
    o.num_elements_field = 999999999u;                 // > MaxElements
    Fixture f = build_fixture(o);
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    igu_span_table table{spans, n};
    rc3_record rec;
    CHECK(rc3_probe_chunks(&table, &st, GUA, 0, &rec) == IGU_OK);
    CHECK(rec.info_num_le_max == IGU_CSK_CONTRADICTED); // recorded evidence
    CHECK(rc3_class(&rec) == 0);                        // non-gating at probe
    const rc3b::Batch2Result r = rc3b::run_ustruct_chain(rec, st, spans, n);
    CHECK(r.ran == 0u);
    CHECK(r.gate == rc3b::BRIDGE_GATE_INFO_CONTRADICTED);
    end();
}

static void t18_publish_gatekeeper() {
    begin("gatekeeper: W-only spans are never published to batch2");
    BuildOpts o{};
    o.chunktable_prot = KVME_W;
    Fixture f = build_fixture(o);
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    tlp::MemSpan views[512];
    size_t dropped = 99;
    const size_t published = rc3b::publish_memspans(spans, n, views, 512, &dropped);
    // Two withheld: the W-only chunk table AND the X-only image stand-in
    // (KVME prot 4 has no READ bit — never published to trusted readers).
    CHECK(dropped == 2u);
    CHECK(published == n - 2u);
    // And the withheld chunk table is exactly the one missing from views.
    bool table_published = false;
    for (size_t i = 0; i < published; ++i)
        if (views[i].start == 0x1380022000ull) table_published = true;
    CHECK(!table_published);
    end();
}

static void t19_vtable_xonly_image() {
    begin("vptr inside X-only image record -> UNAVAILABLE (Inconclusive, never Refuted)");
    BuildOpts o{};
    o.plain_vptr = IMG_LO + 0x600;                     // inside the X-only span
    Fixture f = build_fixture(o);
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    igu_span_table table{spans, n};
    rc3_record rec;
    rc3_probe_chunks(&table, &st, GUA, 0, &rec);
    CHECK(rec.chk_vptr == IGU_CSK_UNAVAILABLE);        // covered-not-readable
    CHECK(rc3_class(&rec) == 1);
    end();
}

static void t22_bridge_gate_map() {
    begin("bridge gate: untrusted walk stats -> predicate does NOT run");
    Fixture f = build_fixture(BuildOpts{});
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    igu_span_table table{spans, n};
    rc3_record rec;
    CHECK(rc3_probe_chunks(&table, &st, GUA, 0, &rec) == IGU_OK);
    rc3_vmstats bogus = st;
    bogus.retained = 2; bogus.insane = 0;              // retained < 8
    CHECK(rc3_vmmap_trustworthy(&bogus) == 0);
    const rc3b::Batch2Result r = rc3b::run_ustruct_chain(rec, bogus, spans, n);
    CHECK(r.ran == 0u);
    CHECK(r.gate == rc3b::BRIDGE_GATE_MAP_UNTRUSTED);
    end();
}

static void t21_arg_guards() {
    begin("argument guards and scan_max clamping");
    rc3_record rec;
    rc3_vmstats vm{};
    vm.retained = 8; vm.total = 8; vm.walk_mode = 1;
    CHECK(rc3_probe_chunks(nullptr, &vm, GUA, 0, &rec) == IGU_E_ARG);
    igu_span_table empty{nullptr, 0};
    CHECK(rc3_probe_chunks(&empty, &vm, GUA, 0, &rec) == IGU_E_ARG);
    CHECK(rc3_vmmap_trustworthy(nullptr) == 0);
    CHECK(rc3_class(nullptr) == IGU_E_ARG);
    // scan_max 0 -> clamp to 64 (IGU_SCAN_MAX_ITEMS parity).
    Fixture f = build_fixture(BuildOpts{});
    static igu_span spans[SPAN_CAP];
    rc3_vmstats st;
    uint64_t tg[16]; uint32_t nt;
    default_targets(tg, nt);
    const uint32_t n = walk_fixture(f, spans, SPAN_CAP, st, tg, nt);
    igu_span_table table{spans, n};
    CHECK(rc3_probe_chunks(&table, &st, GUA, 0, &rec) == IGU_OK);
    CHECK(rec.chunk[0].items_scanned == 6u);
    end();
}

int main() {
    std::printf("== rung3_chunk_addon host suite ==\n");
    t01_walk_and_happy_probe();
    t02_chunktable_gap();
    t04_early_stop_minimal();
    t05_covered_not_readable();
    t06_straddle();
    t07_noncanonical_objects();
    t08_chunk0_null_needed();
    t09_numchunks1();
    t10_chunk1_gap_partial_evidence();
    t11_null_scan_exhaustion();
    t12_posix_translation();
    t13_insane_structsize();
    t14_determinism();
    t15_bridge_ustruct_uclass();
    t16_bridge_gate_step();
    t17_bridge_gate_info();
    t18_publish_gatekeeper();
    t19_vtable_xonly_image();
    t22_bridge_gate_map();
    t21_arg_guards();
    std::printf("== RESULT: %d/%d tests passed, %d assertions failed ==\n",
                g_tests_run - g_tests_failed, g_tests_run, g_total_fails);
    return g_tests_failed == 0 ? 0 : 1;
}
