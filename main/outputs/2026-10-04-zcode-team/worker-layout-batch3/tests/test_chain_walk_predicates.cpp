// test_chain_walk_predicates.cpp — host synthetic-image tests for the
// batch-3 chain-walk predicates (worker-layout-batch3).
//
// Mirrors the sibling test style (worker-5 / worker-layout-batch2): synthetic
// little-endian mapped images; every test cites the predicate and rationale
// code it exercises. All offsets are the SET-5_06 citations in
// chain_walk_predicates.hpp — nothing guessed. Adversarial cases: owner
// back-pointer mismatch, self-cycle, round-trip mismatch, flag-mask
// violation, non-canonical heads/next, unreadable windows, invalid GUA
// views, negative-code cases that must NOT flip the verdict, and a parity
// check against batch2's probe_ustruct_chain on the same synthetic object.
//
// Build:
//   c++ -std=c++17 -Wall -Wextra -Werror -I../src \
//       tests/test_chain_walk_predicates.cpp \
//       src/chain_walk_predicates.cpp \
//       ../worker-layout-batch2/src/reflection_layout_predicates.cpp \
//       ../worker-5-layout-validator/src/townfall_layout_probes.cpp \
//       -o tests/test_chain_walk_predicates

#include "chain_walk_predicates.hpp"

#include <cstdio>
#include <memory>
#include <vector>

using namespace rfl3;
using rfl::ValidatedGuaView;

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

static const char* verdict_name(Verdict v) {
    switch (v) {
        case Verdict::Validated: return "Validated";
        case Verdict::Inconclusive: return "Inconclusive";
        case Verdict::Refuted: return "Refuted";
        default: return "NotProbed";
    }
}

// --- synthetic mapped-image arena (sibling shape) ---------------------------
struct Arena {
    std::vector<std::unique_ptr<std::vector<uint8_t>>> owned;
    std::vector<MemSpan> spans;
    uint64_t next = 0x0000200000000000ULL;

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
    uint64_t addr_of(const std::vector<uint8_t>* b) const {
        for (const auto& s : spans)
            if (s.data == b->data()) return s.start;
        return 0;
    }
    std::vector<uint8_t>* buf_at(uint64_t addr) const {
        for (const auto& s : spans)
            if (addr >= s.start && addr < s.start + s.size)
                return owned[(&s - spans.data())].get();
        return nullptr;
    }
    static void put64(std::vector<uint8_t>* b, size_t off, uint64_t v) {
        for (int i = 0; i < 8; ++i)
            (*b)[off + (size_t)i] = (uint8_t)(v >> (8 * i));
    }
    static void put32(std::vector<uint8_t>* b, size_t off, uint32_t v) {
        for (int i = 0; i < 4; ++i)
            (*b)[off + (size_t)i] = (uint8_t)(v >> (8 * i));
    }
    void patch64(uint64_t addr, size_t off, uint64_t v) {
        put64(buf_at(addr), off, v);
    }
};

struct Gua {
    ValidatedGuaView view{};
    std::vector<uint8_t>* chunk_ptrs = nullptr;
    std::vector<uint8_t>* chunk_buf = nullptr;

    void set_item(uint32_t i, uint64_t obj) {
        Arena::put64(chunk_buf, (size_t)i * kItemStride + kItemObject, obj);
    }
};

static Gua build_gua(Arena& ar, int32_t num_elements) {
    Gua g;
    g.chunk_ptrs = ar.alloc(8);
    g.chunk_buf = ar.alloc((size_t)num_elements * kItemStride);
    Arena::put64(g.chunk_ptrs, 0, ar.addr_of(g.chunk_buf));
    g.view.objects_ptr = ar.addr_of(g.chunk_ptrs);
    g.view.num_elements = num_elements;
    g.view.num_chunks = 1;
    return g;
}

// Build a plausible UObjectBase-shaped object (UField node): vtable readable,
// flags inside RF_AllFlags, index round-tripping through the GUA,
// ClassPrivate readable, OuterPrivate set by caller.
static uint64_t make_uobject(Arena& ar, Gua& gua, uint32_t idx,
                             uint64_t outer, uint32_t flags = 0x301) {
    auto* obj = ar.alloc(0x30);
    auto* vtable = ar.alloc(8);
    Arena::put64(vtable, 0, 0xdeadbeefULL);
    auto* cls = ar.alloc(8);
    Arena::put64(cls, 0, 0x0badf00dULL);
    const uint64_t a = ar.addr_of(obj);
    Arena::put64(obj, 0, ar.addr_of(vtable));
    Arena::put32(obj, kObjObjectFlags, flags);
    Arena::put32(obj, kObjInternalIndex, idx);
    Arena::put64(obj, kObjClassPrivate, ar.addr_of(cls));
    Arena::put64(obj, 0x18, 0);  // NamePrivate — raw, never interpreted
    Arena::put64(obj, kObjOuterPrivate, outer);
    gua.set_item(idx, a);
    return a;
}

// Build an FField-shaped node (UE5): ClassPrivate readable, Owner, Next,
// ArrayDim/ElementSize/PropertyFlags/Offset_Internal payload.
static uint64_t make_ffield(Arena& ar, uint64_t owner, uint64_t next,
                            uint32_t array_dim = 1, uint32_t element_size = 4,
                            uint32_t offset_internal = 0) {
    auto* node = ar.alloc(0x50);
    auto* fclass = ar.alloc(8);
    Arena::put64(fclass, 0, 0x0feedbacULL);
    const uint64_t a = ar.addr_of(node);
    Arena::put64(node, kFFieldClassPrivate, ar.addr_of(fclass));
    Arena::put64(node, kFFieldOwner, owner);
    Arena::put64(node, kFFieldNext, next);
    Arena::put64(node, 0x20, 0);  // NamePrivate — raw, never interpreted
    Arena::put32(node, 0x28, 0);  // FlagsPrivate
    Arena::put32(node, kFPropArrayDim, array_dim);
    Arena::put32(node, kFPropElementSize, element_size);
    Arena::put32(node, 0x38, 0);  // PropertyFlags — raw only (no width)
    Arena::put32(node, 0x44, offset_internal);
    Arena::put64(node, 0x48, 0);  // PropertyLinkNext
    return a;
}

// UStruct-shaped anchor: Children@0x48, ChildProperties@0x50,
// PropertiesSize@0x58.
static uint64_t make_ustruct(Arena& ar, uint64_t children,
                             uint64_t childprops, uint32_t propsize) {
    auto* s = ar.alloc(0x60);
    const uint64_t a = ar.addr_of(s);
    Arena::put64(s, 0x40, 0);  // SuperStruct (not consumed by these probes)
    Arena::put64(s, kUStructChildren, children);
    Arena::put64(s, kUStructChildProperties, childprops);
    Arena::put32(s, kUStructPropertiesSize, propsize);
    Arena::put32(s, 0x5C, 8);  // MinAlignment
    return a;
}

// UClass-shaped anchor: UStruct fields + CDO@0x110 + link heads.
static uint64_t make_uclass(Arena& ar, uint64_t cdo, uint64_t plink,
                            uint64_t rlink, uint64_t dlink,
                            uint32_t propsize) {
    auto* c = ar.alloc(0x118);
    const uint64_t a = ar.addr_of(c);
    Arena::put64(c, kUStructChildren, 0);
    Arena::put64(c, kUStructChildProperties, 0);
    Arena::put32(c, kUStructPropertiesSize, propsize);
    Arena::put64(c, kUStructPropertyLink, plink);
    Arena::put64(c, kUStructRefLink, rlink);
    Arena::put64(c, kUStructDestructorLink, dlink);
    Arena::put64(c, kUClassCDO, cdo);
    return a;
}

// ---------------------------------------------------------------------------
// probe_ustruct_children_walk
// ---------------------------------------------------------------------------

static void test_children_validated() {
    Arena ar;
    Gua gua = build_gua(ar, 8);
    const uint64_t s = make_ustruct(ar, 0, 0, 0x40);
    const uint64_t child2 = make_uobject(ar, gua, 1, s);
    const uint64_t child1 = make_uobject(ar, gua, 0, s);
    ar.patch64(child1, kUFieldNext, child2);
    ar.patch64(s, kUStructChildren, child1);

    const auto r = probe_ustruct_children_walk(ar.spans.data(),
                                               ar.spans.size(), s, gua.view);
    CHECK(r.verdict == Verdict::Validated, "children/validated",
          verdict_name(r.verdict));
    CHECK(r.nodes_visited == 2 && r.nodes_validated == 2,
          "children/validated-count", "visited/validated wrong");
    CHECK(has_code(r.codes, r.code_count, RCode::CH_I_CHAIN_OK),
          "children/validated-code", "CH_I_CHAIN_OK missing");
}

static void test_children_owner_mismatch() {
    Arena ar;
    Gua gua = build_gua(ar, 4);
    const uint64_t other = make_uobject(ar, gua, 2, 0);
    const uint64_t s = make_ustruct(ar, 0, 0, 0x40);
    const uint64_t child = make_uobject(ar, gua, 0, other);  // wrong outer
    ar.patch64(s, kUStructChildren, child);

    const auto r = probe_ustruct_children_walk(ar.spans.data(),
                                               ar.spans.size(), s, gua.view);
    CHECK(r.verdict == Verdict::Refuted, "children/owner-mismatch",
          verdict_name(r.verdict));
    CHECK(has_code(r.codes, r.code_count, RCode::CH_R_OWNER_MISMATCH),
          "children/owner-mismatch-code", "CH_R_OWNER_MISMATCH missing");
}

static void test_children_self_cycle() {
    Arena ar;
    Gua gua = build_gua(ar, 4);
    const uint64_t s = make_ustruct(ar, 0, 0, 0x40);
    const uint64_t child = make_uobject(ar, gua, 0, s);
    ar.patch64(child, kUFieldNext, child);  // self-loop
    ar.patch64(s, kUStructChildren, child);

    const auto r = probe_ustruct_children_walk(ar.spans.data(),
                                               ar.spans.size(), s, gua.view);
    CHECK(r.verdict == Verdict::Refuted, "children/self-cycle",
          verdict_name(r.verdict));
    CHECK(has_code(r.codes, r.code_count, RCode::CH_R_CYCLE),
          "children/self-cycle-code", "CH_R_CYCLE missing");
}

static void test_children_empty_and_bad_gua() {
    Arena ar;
    Gua gua = build_gua(ar, 4);
    const uint64_t s = make_ustruct(ar, 0, 0, 0x40);
    const auto r1 = probe_ustruct_children_walk(ar.spans.data(),
                                                ar.spans.size(), s, gua.view);
    CHECK(r1.verdict == Verdict::Inconclusive, "children/empty",
          verdict_name(r1.verdict));
    CHECK(has_code(r1.codes, r1.code_count, RCode::CH_I_CHAIN_EMPTY),
          "children/empty-code", "CH_I_CHAIN_EMPTY missing");

    ValidatedGuaView bad{};
    const auto r2 = probe_ustruct_children_walk(ar.spans.data(),
                                                ar.spans.size(), s, bad);
    CHECK(r2.verdict == Verdict::Inconclusive, "children/bad-gua",
          verdict_name(r2.verdict));
    CHECK(has_code(r2.codes, r2.code_count, RCode::CH_E_NO_GUA_VIEW),
          "children/bad-gua-code", "CH_E_NO_GUA_VIEW missing");
}

static void test_children_anchor_unreadable() {
    Arena ar;
    Gua gua = build_gua(ar, 4);
    const auto r = probe_ustruct_children_walk(ar.spans.data(),
                                               ar.spans.size(), 0x1234,
                                               gua.view);
    CHECK(r.verdict == Verdict::Inconclusive, "children/anchor-unreadable",
          verdict_name(r.verdict));
    CHECK(has_code(r.codes, r.code_count, RCode::CH_E_WINDOW_UNREADABLE),
          "children/anchor-unreadable-code",
          "CH_E_WINDOW_UNREADABLE missing");
}

static void test_children_flag_mask_and_roundtrip() {
    Arena ar;
    Gua gua = build_gua(ar, 4);
    const uint64_t s = make_ustruct(ar, 0, 0, 0x40);
    const uint64_t child = make_uobject(ar, gua, 0, s, /*flags=*/0xF0000000u);
    ar.patch64(s, kUStructChildren, child);
    const auto r = probe_ustruct_children_walk(ar.spans.data(),
                                               ar.spans.size(), s, gua.view);
    CHECK(r.verdict == Verdict::Refuted, "children/flag-mask",
          verdict_name(r.verdict));
    CHECK(has_code(r.codes, r.code_count, RCode::CH_R_NODE_FLAGS_MASK),
          "children/flag-mask-code", "CH_R_NODE_FLAGS_MASK missing");

    Arena ar2;
    Gua gua2 = build_gua(ar2, 4);
    const uint64_t s2 = make_ustruct(ar2, 0, 0, 0x40);
    const uint64_t child2 = make_uobject(ar2, gua2, 0, s2);
    gua2.set_item(0, child2 + 0x10);  // round-trip now misses
    ar2.patch64(s2, kUStructChildren, child2);
    const auto r2 = probe_ustruct_children_walk(ar2.spans.data(),
                                                ar2.spans.size(), s2,
                                                gua2.view);
    CHECK(r2.verdict == Verdict::Refuted, "children/roundtrip",
          verdict_name(r2.verdict));
    CHECK(has_code(r2.codes, r2.code_count, RCode::CH_R_NODE_INDEX),
          "children/roundtrip-code", "CH_R_NODE_INDEX missing");
}

static void test_children_bad_head_and_next() {
    Arena ar;
    Gua gua = build_gua(ar, 4);
    const uint64_t s = make_ustruct(ar, 0xBADF00DULL, 0, 0x40);  // unassigned
    const auto r = probe_ustruct_children_walk(ar.spans.data(),
                                               ar.spans.size(), s, gua.view);
    CHECK(r.verdict == Verdict::Refuted, "children/bad-head",
          verdict_name(r.verdict));
    CHECK(has_code(r.codes, r.code_count, RCode::CH_R_HEAD_BAD),
          "children/bad-head-code", "CH_R_HEAD_BAD missing");

    Arena ar2;
    Gua gua2 = build_gua(ar2, 4);
    const uint64_t s2 = make_ustruct(ar2, 0, 0, 0x40);
    const uint64_t child = make_uobject(ar2, gua2, 0, s2);
    ar2.patch64(child, kUFieldNext, 0x0000DEADBEEFULL);  // non-canonical
    ar2.patch64(s2, kUStructChildren, child);
    const auto r2 = probe_ustruct_children_walk(ar2.spans.data(),
                                                ar2.spans.size(), s2,
                                                gua2.view);
    CHECK(r2.verdict == Verdict::Refuted, "children/bad-next",
          verdict_name(r2.verdict));
    CHECK(has_code(r2.codes, r2.code_count, RCode::CH_R_NEXT_BAD),
          "children/bad-next-code", "CH_R_NEXT_BAD missing");
}

// ---------------------------------------------------------------------------
// probe_ustruct_childproperties_walk
// ---------------------------------------------------------------------------

static void test_childprops_validated_and_negative_only() {
    Arena ar;
    const uint64_t s = make_ustruct(ar, 0, 0, 0x40);
    const uint64_t p1 = make_ffield(ar, s, 0);
    const uint64_t p0 = make_ffield(ar, s, p1);
    ar.patch64(s, kUStructChildProperties, p0);

    const auto r = probe_ustruct_childproperties_walk(ar.spans.data(),
                                                      ar.spans.size(), s);
    CHECK(r.verdict == Verdict::Validated, "childprops/validated",
          verdict_name(r.verdict));
    CHECK(r.nodes_visited == 2, "childprops/validated-count", "count wrong");
    CHECK(has_code(r.codes, r.code_count, RCode::CP_I_CHAIN_OK),
          "childprops/validated-code", "CP_I_CHAIN_OK missing");

    // Negative-only codes must NOT flip the verdict.
    Arena ar2;
    const uint64_t s2 = make_ustruct(ar2, 0, 0, 0x40);
    const uint64_t q = make_ffield(ar2, s2, 0, /*array_dim=*/1,
                                   /*element_size=*/0, /*offset_internal=*/0x80);
    ar2.patch64(s2, kUStructChildProperties, q);
    const auto r2 = probe_ustruct_childproperties_walk(ar2.spans.data(),
                                                       ar2.spans.size(), s2);
    CHECK(r2.verdict == Verdict::Validated, "childprops/negative-only",
          verdict_name(r2.verdict));
    CHECK(has_code(r2.codes, r2.code_count, RCode::CP_N_ELEMENT_SIZE_ZERO) &&
              has_code(r2.codes, r2.code_count, RCode::CP_N_OFFSET_BEYOND_SIZE),
          "childprops/negative-only-codes", "CP_N_* codes missing");
}

static void test_childprops_owner_mismatch_and_arraydim() {
    Arena ar;
    const uint64_t stranger = ar.addr_of(ar.alloc(0x40));
    const uint64_t s = make_ustruct(ar, 0, 0, 0x40);
    const uint64_t p = make_ffield(ar, stranger, 0);  // wrong owner
    ar.patch64(s, kUStructChildProperties, p);
    const auto r = probe_ustruct_childproperties_walk(ar.spans.data(),
                                                      ar.spans.size(), s);
    CHECK(r.verdict == Verdict::Refuted, "childprops/owner-mismatch",
          verdict_name(r.verdict));
    CHECK(has_code(r.codes, r.code_count, RCode::CP_R_OWNER_MISMATCH),
          "childprops/owner-mismatch-code", "CP_R_OWNER_MISMATCH missing");

    Arena ar2;
    const uint64_t s2 = make_ustruct(ar2, 0, 0, 0x40);
    const uint64_t q = make_ffield(ar2, s2, 0, /*array_dim=*/0);
    ar2.patch64(s2, kUStructChildProperties, q);
    const auto r2 = probe_ustruct_childproperties_walk(ar2.spans.data(),
                                                       ar2.spans.size(), s2);
    CHECK(r2.verdict == Verdict::Refuted, "childprops/arraydim-zero",
          verdict_name(r2.verdict));
    CHECK(has_code(r2.codes, r2.code_count, RCode::CP_R_ARRAYDIM_ZERO),
          "childprops/arraydim-code", "CP_R_ARRAYDIM_ZERO missing");
}

static void test_childprops_cycle_empty_unreadable() {
    Arena ar;
    const uint64_t s = make_ustruct(ar, 0, 0, 0x40);
    const uint64_t p = make_ffield(ar, s, 0);
    ar.patch64(p, kFFieldNext, p);  // self-loop
    ar.patch64(s, kUStructChildProperties, p);
    const auto r = probe_ustruct_childproperties_walk(ar.spans.data(),
                                                      ar.spans.size(), s);
    CHECK(r.verdict == Verdict::Refuted, "childprops/cycle",
          verdict_name(r.verdict));
    CHECK(has_code(r.codes, r.code_count, RCode::CP_R_CYCLE),
          "childprops/cycle-code", "CP_R_CYCLE missing");

    Arena ar2;
    const uint64_t s2 = make_ustruct(ar2, 0, 0, 0x40);
    const auto r2 = probe_ustruct_childproperties_walk(ar2.spans.data(),
                                                       ar2.spans.size(), s2);
    CHECK(r2.verdict == Verdict::Inconclusive, "childprops/empty",
          verdict_name(r2.verdict));
    CHECK(has_code(r2.codes, r2.code_count, RCode::CP_I_CHAIN_EMPTY),
          "childprops/empty-code", "CP_I_CHAIN_EMPTY missing");

    Arena ar3;
    const auto r3 = probe_ustruct_childproperties_walk(ar3.spans.data(),
                                                       ar3.spans.size(),
                                                       0x1234);
    CHECK(r3.verdict == Verdict::Inconclusive, "childprops/unreadable",
          verdict_name(r3.verdict));
    CHECK(has_code(r3.codes, r3.code_count, RCode::CP_E_WINDOW_UNREADABLE),
          "childprops/unreadable-code", "CP_E_WINDOW_UNREADABLE missing");
}

static void test_childprops_class_bad() {
    Arena ar;
    const uint64_t s = make_ustruct(ar, 0, 0, 0x40);
    auto* node = ar.alloc(0x50);
    Arena::put64(node, kFFieldClassPrivate, 0);  // ClassPrivate null
    Arena::put64(node, kFFieldOwner, s);
    Arena::put64(node, kFFieldNext, 0);
    const uint64_t p = ar.addr_of(node);
    ar.patch64(s, kUStructChildProperties, p);
    const auto r = probe_ustruct_childproperties_walk(ar.spans.data(),
                                                      ar.spans.size(), s);
    CHECK(r.verdict == Verdict::Refuted, "childprops/class-null",
          verdict_name(r.verdict));
    CHECK(has_code(r.codes, r.code_count, RCode::CP_R_CLASS_BAD),
          "childprops/class-null-code", "CP_R_CLASS_BAD missing");
}

// ---------------------------------------------------------------------------
// probe_uclass_property_links
// ---------------------------------------------------------------------------

static void test_uclass_validated_with_cdo_and_inherited_owner() {
    Arena ar;
    Gua gua = build_gua(ar, 8);
    const uint64_t super_struct = make_ustruct(ar, 0, 0, 0x80);
    const uint64_t cls = make_uclass(ar, 0, 0, 0, 0, 0x80);
    // Property owned by a SUPER struct but present in this class's link —
    // owner equality must NOT be required (inherited-property semantics).
    const uint64_t prop = make_ffield(ar, super_struct, 0,
                                      /*array_dim=*/1, /*element_size=*/4,
                                      /*offset_internal=*/0x10);
    ar.patch64(cls, kUStructPropertyLink, prop);
    const uint64_t cdo = make_uobject(ar, gua, 0, cls);
    ar.patch64(cdo, kObjClassPrivate, cls);
    ar.patch64(cls, kUClassCDO, cdo);

    const auto r = probe_uclass_property_links(ar.spans.data(),
                                               ar.spans.size(), cls, gua.view);
    CHECK(r.verdict == Verdict::Validated, "uclass/validated",
          verdict_name(r.verdict));
    CHECK(has_code(r.codes, r.code_count, RCode::UC_I_LINKS_OK),
          "uclass/validated-code", "UC_I_LINKS_OK missing");
}

static void test_uclass_cdo_mismatch_and_all_null() {
    Arena ar;
    Gua gua = build_gua(ar, 8);
    const uint64_t cls = make_uclass(ar, 0, 0, 0, 0, 0x40);
    const uint64_t cdo = make_uobject(ar, gua, 0, 0);
    // CDO is fully plausible but its ClassPrivate points at a DIFFERENT
    // readable buffer (not the class anchor) — the back-pointer mismatch.
    const uint64_t other_class = ar.addr_of(ar.alloc(8));
    ar.patch64(cdo, kObjClassPrivate, other_class);
    ar.patch64(cls, kUClassCDO, cdo);
    const auto r = probe_uclass_property_links(ar.spans.data(),
                                               ar.spans.size(), cls, gua.view);
    CHECK(r.verdict == Verdict::Refuted, "uclass/cdo-mismatch",
          verdict_name(r.verdict));
    CHECK(has_code(r.codes, r.code_count, RCode::UC_R_CDO_CLASS_MISMATCH),
          "uclass/cdo-mismatch-code", "UC_R_CDO_CLASS_MISMATCH missing");

    Arena ar2;
    Gua gua2 = build_gua(ar2, 4);
    const uint64_t cls2 = make_uclass(ar2, 0, 0, 0, 0, 0x40);
    const auto r2 = probe_uclass_property_links(ar2.spans.data(),
                                                ar2.spans.size(), cls2,
                                                gua2.view);
    CHECK(r2.verdict == Verdict::Inconclusive, "uclass/all-null",
          verdict_name(r2.verdict));
    CHECK(has_code(r2.codes, r2.code_count, RCode::UC_E_ALL_EVIDENCE_NULL) &&
              has_code(r2.codes, r2.code_count, RCode::UC_I_NO_CDO),
          "uclass/all-null-codes", "UC_E_ALL_EVIDENCE_NULL missing");
}

static void test_uclass_links_only_and_bad_next() {
    // CDO null + one valid link chain: links-only evidence is sufficient.
    Arena ar;
    Gua gua = build_gua(ar, 8);
    const uint64_t owner = make_ustruct(ar, 0, 0, 0x80);
    const uint64_t cls = make_uclass(ar, 0, 0, 0, 0, 0x80);
    const uint64_t p2 = make_ffield(ar, owner, 0, 1, 4, 0x20);
    const uint64_t p1 = make_ffield(ar, owner, 0, 1, 4, 0x10);
    // Link chains chase PropertyLinkNext@0x48, not FField::Next@0x18.
    ar.patch64(p1, kFPropPropertyLinkNextUe5, p2);
    ar.patch64(cls, kUStructPropertyLink, p1);
    const auto r = probe_uclass_property_links(ar.spans.data(),
                                               ar.spans.size(), cls, gua.view);
    CHECK(r.verdict == Verdict::Validated, "uclass/links-only",
          verdict_name(r.verdict));
    CHECK(r.nodes_visited == 2 && r.nodes_validated == 2,
          "uclass/links-only-count", "counts wrong");
    CHECK(has_code(r.codes, r.code_count, RCode::UC_I_NO_CDO),
          "uclass/links-only-nocdo", "UC_I_NO_CDO missing");

    // Bad PropertyLinkNext refutes.
    Arena ar2;
    Gua gua2 = build_gua(ar2, 8);
    const uint64_t cls2 = make_uclass(ar2, 0, 0, 0, 0, 0x40);
    const uint64_t q = make_ffield(ar2, 0, 0, 1, 4, 0x10);
    ar2.patch64(q, kFPropPropertyLinkNextUe5, 0x0000DEADBEEFULL);
    ar2.patch64(cls2, kUStructPropertyLink, q);
    const auto r2 = probe_uclass_property_links(ar2.spans.data(),
                                                ar2.spans.size(), cls2,
                                                gua2.view);
    CHECK(r2.verdict == Verdict::Refuted, "uclass/bad-next",
          verdict_name(r2.verdict));
    CHECK(has_code(r2.codes, r2.code_count, RCode::UC_R_NEXT_BAD),
          "uclass/bad-next-code", "UC_R_NEXT_BAD missing");
}

static void test_uclass_gua_and_window() {
    Arena ar;
    Gua gua = build_gua(ar, 4);
    const uint64_t cls = make_uclass(ar, 0, 0, 0, 0, 0x40);
    ValidatedGuaView bad{};
    const auto r = probe_uclass_property_links(ar.spans.data(),
                                               ar.spans.size(), cls, bad);
    CHECK(r.verdict == Verdict::Inconclusive, "uclass/bad-gua",
          verdict_name(r.verdict));
    CHECK(has_code(r.codes, r.code_count, RCode::UC_E_NO_GUA_VIEW),
          "uclass/bad-gua-code", "UC_E_NO_GUA_VIEW missing");

    const auto r2 = probe_uclass_property_links(ar.spans.data(),
                                                ar.spans.size(), 0x1234,
                                                gua.view);
    CHECK(r2.verdict == Verdict::Inconclusive, "uclass/window",
          verdict_name(r2.verdict));
    CHECK(has_code(r2.codes, r2.code_count, RCode::UC_E_WINDOW_UNREADABLE),
          "uclass/window-code", "UC_E_WINDOW_UNREADABLE missing");
}

static void test_uclass_link_cycle() {
    Arena ar;
    Gua gua = build_gua(ar, 8);
    const uint64_t cls = make_uclass(ar, 0, 0, 0, 0, 0x40);
    const uint64_t p = make_ffield(ar, 0, 0, 1, 4, 0);
    ar.patch64(p, kFPropPropertyLinkNextUe5, p);  // self-loop
    ar.patch64(cls, kUStructPropertyLink, p);
    const auto r = probe_uclass_property_links(ar.spans.data(),
                                               ar.spans.size(), cls, gua.view);
    CHECK(r.verdict == Verdict::Refuted, "uclass/cycle",
          verdict_name(r.verdict));
    CHECK(has_code(r.codes, r.code_count, RCode::UC_R_CYCLE),
          "uclass/cycle-code", "UC_R_CYCLE missing");
}

// ---------------------------------------------------------------------------
// batch2 parity: the mirrored UObjectBase classification must agree with
// batch2's probe_ustruct_chain on the same synthetic object (plausible).
// ---------------------------------------------------------------------------

static void test_batch2_parity_plausible_object() {
    Arena ar;
    Gua gua = build_gua(ar, 4);
    const uint64_t s = make_ustruct(ar, 0, 0, 0x40);
    const uint64_t obj = make_uobject(ar, gua, 0, s);
    ar.patch64(s, kUStructChildren, obj);
    // Batch2's chain probe walks the GUA itself: the object must count as
    // plausible and struct-like under the same checks this worker mirrors.
    const auto chain = rfl::probe_ustruct_chain(ar.spans.data(),
                                                ar.spans.size(), gua.view);
    CHECK(chain.plausible_uobject >= 1, "parity/batch2-plausible",
          "batch2 found no plausible object");
    CHECK(chain.first_plausible == obj, "parity/batch2-first",
          "batch2 first plausible differs");
}

int main() {
    test_children_validated();
    test_children_owner_mismatch();
    test_children_self_cycle();
    test_children_empty_and_bad_gua();
    test_children_anchor_unreadable();
    test_children_flag_mask_and_roundtrip();
    test_children_bad_head_and_next();
    test_childprops_validated_and_negative_only();
    test_childprops_owner_mismatch_and_arraydim();
    test_childprops_cycle_empty_unreadable();
    test_childprops_class_bad();
    test_uclass_validated_with_cdo_and_inherited_owner();
    test_uclass_cdo_mismatch_and_all_null();
    test_uclass_links_only_and_bad_next();
    test_uclass_gua_and_window();
    test_uclass_link_cycle();
    test_batch2_parity_plausible_object();

    std::printf(g_failures == 0 ? "ALL TESTS PASS\n" : "FAILURES: %d\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
