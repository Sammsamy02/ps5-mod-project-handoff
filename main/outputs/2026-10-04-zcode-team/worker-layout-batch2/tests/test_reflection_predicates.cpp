// test_reflection_predicates.cpp — host synthetic-image tests for
// reflection_layout_predicates (worker-layout-batch2).
//
// Mirrors worker-5-layout-validator's test style: synthetic little-endian
// mapped images, every test cites the predicate chain step and rationale code
// it exercises (see reflection_layout_predicates.hpp provenance block for
// every offset used below — all SET-5_06/SET-4_07/UOA-HPP/UnrealFlags.hpp
// citations, nothing guessed).
//
// Build:  c++ -std=c++17 -Wall -Wextra -Werror -I../src \
//             test_reflection_predicates.cpp \
//             ../src/reflection_layout_predicates.cpp \
//             ../worker-5-layout-validator/src/townfall_layout_probes.cpp \
//             -o test_reflection_predicates

#include "reflection_layout_predicates.hpp"

#include <cstdio>
#include <memory>
#include <vector>

using namespace rfl;

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

// --- synthetic mapped-image arena (sibling's shape) -------------------------
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
        for (auto& s : spans)
            if (s.data == b->data()) return s.start;
        return 0;
    }
    static void put64(std::vector<uint8_t>* b, size_t off, uint64_t v) {
        for (int i = 0; i < 8; ++i) (*b)[off + (size_t)i] = (uint8_t)(v >> (8 * i));
    }
    static void put32(std::vector<uint8_t>* b, size_t off, uint32_t v) {
        for (int i = 0; i < 4; ++i) (*b)[off + (size_t)i] = (uint8_t)(v >> (8 * i));
    }
    static void put16(std::vector<uint8_t>* b, size_t off, uint16_t v) {
        (*b)[off] = (uint8_t)v;
        (*b)[off + 1] = (uint8_t)(v >> 8);
    }
};

// Simpler: build GUA with direct buffer pointers kept by the test.
struct Gua {
    ValidatedGuaView view;
    std::vector<uint8_t>* chunk_buf;
    uint64_t chunk_addr;
    void set_item(Arena& /*ar*/, uint32_t i, uint64_t obj) {
        Arena::put64(chunk_buf, (size_t)i * kItemStride + kItemObject, obj);
    }
};

static Gua build_gua(Arena& ar, int32_t num_elements, uint32_t slots) {
    Gua g;
    auto* chunk_ptrs = ar.alloc(8);          // one chunk pointer
    g.chunk_buf = ar.alloc((size_t)slots * kItemStride);
    g.chunk_addr = ar.addr_of(g.chunk_buf);
    Arena::put64(chunk_ptrs, 0, g.chunk_addr);
    g.view.objects_ptr = ar.addr_of(chunk_ptrs);
    g.view.num_elements = num_elements;
    g.view.num_chunks = 1;
    return g;
}

// UObjectBase synth (SET-5_06 offsets). vtable/class/outer targets are
// separate readable spans so pointer checks succeed.
struct ObjCommon {
    std::vector<uint8_t>* vtable;
    uint64_t vtable_addr;
    std::vector<uint8_t>* cls;
    uint64_t cls_addr;
    std::vector<uint8_t>* outer;
    uint64_t outer_addr;
    ObjCommon(Arena& ar) {
        vtable = ar.alloc(64); vtable_addr = ar.addr_of(vtable);
        cls = ar.alloc(64);    cls_addr = ar.addr_of(cls);
        outer = ar.alloc(64);  outer_addr = ar.addr_of(outer);
    }
};

struct ObjOpts {
    uint32_t flags = 0x1;          // RF_Public (UnrealFlags.hpp)
    uint32_t index = 0;
    uint64_t vptr_override = 0;
    uint64_t class_override = 0;
    uint64_t outer = 0;
};

// UStruct tail: super/children/childprops/size/align (SET-5_06).
struct StructOpts {
    uint64_t super = 0;
    uint64_t children = 0;
    uint64_t child_properties = 0;
    uint32_t properties_size = 0x30;
    uint32_t min_alignment = 0x4;
};
static void make_ustruct(std::vector<uint8_t>* obj, const StructOpts& s) {
    Arena::put64(obj, kUStructSuperStruct, s.super);
    Arena::put64(obj, kUStructChildren, s.children);
    Arena::put64(obj, kUStructChildProperties, s.child_properties);
    Arena::put32(obj, kUStructPropertiesSize, s.properties_size);
    Arena::put32(obj, kUStructMinAlignment, s.min_alignment);
}

// The walk only reads through +0x128 when testing the UClass extension, so
// plain plausible objects need a big enough readable window: allocate
// UClass-sized buffers directly.
static std::vector<uint8_t>* make_uobject_big(Arena& ar, const ObjCommon& c,
                                              const ObjOpts& o) {
    auto* b = ar.alloc(kUClassTotalSize);
    Arena::put64(b, 0x0, o.vptr_override ? o.vptr_override : c.vtable_addr);
    Arena::put32(b, kObjObjectFlags, o.flags);
    Arena::put32(b, kObjInternalIndex, o.index);
    Arena::put64(b, kObjClassPrivate, o.class_override ? o.class_override
                                                       : c.cls_addr);
    Arena::put64(b, kObjNamePrivate, 0xDEADBEEFCAFEBABEULL);
    Arena::put64(b, kObjOuterPrivate, o.outer);
    return b;
}

// FField/FProperty synth (both variants share the 0x70-byte window).
struct FieldOpts {
    // H5 (UE5 FField + 5_06 payload)
    uint64_t h5_class = 0;
    uint64_t h5_owner = 0;
    uint64_t h5_next = 0;
    uint32_t h5_array_dim = 1;
    // H4 (UE4 UObjectBase + 4_07 payload)
    uint64_t h4_vptr = 0;
    uint64_t h4_class = 0;
    uint64_t h4_outer = 0;
    // shared window bytes
    uint64_t qword_8 = 0;        // H5 ClassPrivate / H4 flags|index<<32
    uint32_t flags_8 = 0;        // H4 ObjectFlags (low dword of qword_8)
    uint32_t index_c = 0;        // H4 InternalIndex (high dword of qword_8)
};
static std::vector<uint8_t>* make_field(Arena& ar, const FieldOpts& o) {
    auto* b = ar.alloc(kFPropTotalSize);
    Arena::put64(b, 0x0, o.h4_vptr);
    if (o.qword_8)
        Arena::put64(b, 0x8, o.qword_8);
    else {
        Arena::put32(b, kObjObjectFlags, o.flags_8);
        Arena::put32(b, kObjInternalIndex, o.index_c);
    }
    Arena::put64(b, kObjClassPrivate, o.h4_class);       // H4 class / H5 next-adjacent
    Arena::put64(b, kFFieldNext, o.h5_next);             // 0x18
    // +0x20 bytes: H5 NamePrivate (FName, RAW ONLY, P2) / H4 OuterPrivate.
    // Default 0 = readable-or-null under BOTH readings; the P2-discipline
    // test overwrites with a weird FName value where H4 is pre-refuted.
    Arena::put64(b, kFFieldNamePrivate, 0);
    Arena::put32(b, kFFieldFlagsPrivate, 0);             // raw (no cited mask)
    Arena::put32(b, kFPropArrayDim, o.h5_array_dim);
    Arena::put32(b, kFPropElementSize, 4);
    // link pointers: UE5 0x48..0x60 + RepNotify 0x68; UE4 0x50..0x68 — all 0.
    return b;
}

int main() {
    // ------------------------------------------------------------------
    // UStruct/UClass chain probe
    // ------------------------------------------------------------------
    {
        Arena ar;
        ObjCommon c(ar);
        Gua g = build_gua(ar, /*num_elements=*/4, /*slots=*/8);

        // obj0: plausible UObjectBase + struct-like (super -> obj1 -> null).
        auto* b1 = make_uobject_big(ar, c, {0x1, 1, 0, 0, 0});
        uint64_t o1 = ar.addr_of(b1);
        auto* b0 = make_uobject_big(ar, c, {0x1, 0, 0, 0, 0});
        uint64_t o0 = ar.addr_of(b0);
        make_ustruct(b0, {/*super=*/o1, 0, 0, 0x30, 4});
        make_ustruct(b1, {0, 0, 0, 0x20, 4});
        g.set_item(ar, 0, o0);
        g.set_item(ar, 1, o1);

        StructChainResult r = probe_ustruct_chain(ar.spans.data(),
                                                  ar.spans.size(), g.view);
        CHECK(r.verdict == Verdict::Validated, "chain/validated",
              rcode_string(r.codes[0]));
        CHECK(r.plausible_uobject == 2, "chain/plausible-count", "count");
        CHECK(r.first_ustruct_like == o0, "chain/first-struct", "addr");
        CHECK(r.roundtrips_ok == 2, "chain/roundtrips", "count");
        CHECK(has_code(r.codes, r.code_count, RCode::OK), "chain/ok-code",
              "missing OK");
    }
    {
        // UClass extension: CDO pointer readable (SET-5_06 0x110).
        Arena ar;
        ObjCommon c(ar);
        auto* cdo = ar.alloc(64);
        Gua g = build_gua(ar, 2, 4);
        auto* b0 = make_uobject_big(ar, c, {0x1, 0, 0, 0, 0});
        uint64_t o0 = ar.addr_of(b0);
        make_ustruct(b0, {0, 0, 0, 0x10, 4});
        Arena::put64(b0, kUClassClassDefaultObject, ar.addr_of(cdo));
        g.set_item(ar, 0, o0);
        StructChainResult r = probe_ustruct_chain(ar.spans.data(),
                                                  ar.spans.size(), g.view);
        CHECK(r.verdict == Verdict::Validated && r.first_uclass_like == o0,
              "chain/uclass-ext", "expected UClass hit");
        CHECK(has_code(r.codes, r.code_count, RCode::STR_I_UCLASS_EXT_OK),
              "chain/uclass-code", "missing STR_I_UCLASS_EXT_OK");
    }
    {
        // Adversarial: ObjectFlags outside RF_AllFlags -> base contradiction;
        // no struct-like object -> family Refuted (0x51xx layer).
        Arena ar;
        ObjCommon c(ar);
        Gua g = build_gua(ar, 2, 4);
        auto* b0 = make_uobject_big(ar, c, {/*flags=*/0xF0000001u, 0});
        make_ustruct(b0, {0, 0, 0, 0x10, 4});
        g.set_item(ar, 0, ar.addr_of(b0));
        StructChainResult r = probe_ustruct_chain(ar.spans.data(),
                                                  ar.spans.size(), g.view);
        CHECK(r.verdict == Verdict::Refuted, "chain/refuted-flags", "verdict");
        CHECK(has_code(r.codes, r.code_count, RCode::OBJ_R_FLAGS_MASK),
              "chain/flags-mask-code", "missing OBJ_R_FLAGS_MASK");
    }
    {
        // Adversarial noise tolerance: plausible object whose struct-region
        // bytes are foreign layout (Children garbage) must NOT refute the
        // family (no FName/type identification exists — P2 gap): Inconclusive.
        Arena ar;
        ObjCommon c(ar);
        Gua g = build_gua(ar, 2, 4);
        auto* b0 = make_uobject_big(ar, c, {0x1, 0});
        make_ustruct(b0, {0, /*children=*/0x13ull, 0, 0x10, 4});  // non-canon
        g.set_item(ar, 0, ar.addr_of(b0));
        StructChainResult r = probe_ustruct_chain(ar.spans.data(),
                                                  ar.spans.size(), g.view);
        CHECK(r.verdict == Verdict::Inconclusive, "chain/noise-not-refuted",
              "foreign layout must stay Inconclusive");
        CHECK(has_code(r.codes, r.code_count, RCode::STR_R_CHILDREN_BAD),
              "chain/noise-recorded", "negative evidence still recorded");
        CHECK(has_code(r.codes, r.code_count, RCode::STR_E_NONE_FOUND),
              "chain/none-found", "missing STR_E_NONE_FOUND");
    }
    {
        // Adversarial: SuperStruct self-cycle recorded, but a later valid
        // struct still yields Validated (existence proof semantics).
        Arena ar;
        ObjCommon c(ar);
        Gua g = build_gua(ar, 4, 8);
        auto* bad = make_uobject_big(ar, c, {0x1, 0});
        uint64_t bad_addr = ar.addr_of(bad);
        make_ustruct(bad, {/*super=*/bad_addr, 0, 0, 0x10, 4});
        auto* good = make_uobject_big(ar, c, {0x1, 1});
        make_ustruct(good, {0, 0, 0, 0x10, 4});
        g.set_item(ar, 0, bad_addr);
        g.set_item(ar, 1, ar.addr_of(good));
        StructChainResult r = probe_ustruct_chain(ar.spans.data(),
                                                  ar.spans.size(), g.view);
        CHECK(r.verdict == Verdict::Validated, "chain/cycle-coexists",
              "valid struct must win");
        CHECK(has_code(r.codes, r.code_count, RCode::STR_R_SUPER_CYCLE),
              "chain/cycle-recorded", "missing STR_R_SUPER_CYCLE");
    }
    {
        // Round-trip mismatch refutes the base layer (rung-2 cross-check).
        Arena ar;
        ObjCommon c(ar);
        Gua g = build_gua(ar, 4, 8);
        auto* other = make_uobject_big(ar, c, {0x1, 3});
        Arena::put64(other, kUStructChildren, 0x13);  // not struct-like
        auto* b0 = make_uobject_big(ar, c, {/*flags=*/0x1, /*index=*/3});  // slot 0!
        make_ustruct(b0, {0, 0, 0, 0x10, 4});
        g.set_item(ar, 0, ar.addr_of(b0));
        g.set_item(ar, 3, ar.addr_of(other));
        StructChainResult r = probe_ustruct_chain(ar.spans.data(),
                                                  ar.spans.size(), g.view);
        CHECK(r.verdict == Verdict::Refuted, "chain/refuted-roundtrip",
              "verdict");
        CHECK(has_code(r.codes, r.code_count, RCode::OBJ_R_INDEX_ROUNDTRIP),
              "chain/roundtrip-code", "missing OBJ_R_INDEX_ROUNDTRIP");
    }
    {
        // Walk truncation: >64 elements, only garbage-free plausible objects
        // but no struct -> Inconclusive + truncation code.
        Arena ar;
        ObjCommon c(ar);
        Gua g = build_gua(ar, /*num_elements=*/1000, /*slots=*/1);
        auto* b0 = make_uobject_big(ar, c, {0x1, 0});
        g.set_item(ar, 0, ar.addr_of(b0));  // no struct fields set -> zeros
        // NB: all-zero struct fields (super/children/childprops null, sizes
        // zero) DO pass the structural chain, so this object IS struct-like.
        StructChainResult r = probe_ustruct_chain(ar.spans.data(),
                                                  ar.spans.size(), g.view);
        CHECK(r.verdict == Verdict::Validated && r.walk_truncated,
              "chain/truncated-still-validated", "empty struct is struct-like");
        CHECK(has_code(r.codes, r.code_count, RCode::OK), "chain/trunc-ok",
              "OK");
        // A truly-empty array with no items at all:
        Gua g2 = build_gua(ar, 1000, 1);  // item 0 null
        StructChainResult r2 = probe_ustruct_chain(ar.spans.data(),
                                                   ar.spans.size(), g2.view);
        CHECK(r2.verdict == Verdict::Inconclusive &&
                  has_code(r2.codes, r2.code_count, RCode::OBJ_E_WALK_TRUNCATED),
              "chain/truncated-inconclusive", "truncation code expected");
    }
    {
        // Invalid GUA view: Inconclusive with OBJ_E_GUA_VIEW_INVALID.
        Arena ar;
        ValidatedGuaView bad{0, 0, 0};
        StructChainResult r = probe_ustruct_chain(ar.spans.data(),
                                                  ar.spans.size(), bad);
        CHECK(r.verdict == Verdict::Inconclusive &&
                  has_code(r.codes, r.code_count, RCode::OBJ_E_GUA_VIEW_INVALID),
              "chain/invalid-gua", "expected OBJ_E_GUA_VIEW_INVALID");
    }

    // ------------------------------------------------------------------
    // UFunction classifier (Inconclusive-only)
    // ------------------------------------------------------------------
    {
        Arena ar;
        auto* code = ar.alloc(64);  // native function body target
        auto* fn = ar.alloc(kUFunctionTotalSize);
        Arena::put64(fn, kUFunctionFunc, ar.addr_of(code));
        Arena::put32(fn, kUFunctionFlags, 0x00020400u);  // Native|Public
        (*fn)[kUFunctionNumParms] = 2;
        Arena::put16(fn, kUFunctionParmsSize, 16);
        Arena::put16(fn, kUFunctionRetValOffset, 8);
        UFunctionProbeResult r = probe_ufunction(ar.spans.data(),
                                                 ar.spans.size(),
                                                 ar.addr_of(fn));
        CHECK(r.verdict == Verdict::Inconclusive, "ufunc/always-inconclusive",
              "classifier must never elevate");
        CHECK(has_code(r.codes, r.code_count, RCode::FN_I_FUNC_READABLE) &&
                  has_code(r.codes, r.code_count, RCode::FN_I_FLAGS_IN_MASK) &&
                  has_code(r.codes, r.code_count, RCode::FN_I_PARM_CONSISTENT) &&
                  has_code(r.codes, r.code_count, RCode::FN_I_LINKPTRS_OK),
              "ufunc/support-codes", "expected all support codes");
        CHECK(r.func_ptr == ar.addr_of(code) && r.num_parms == 2 &&
                  r.parms_size == 16 && r.retval_offset == 8,
              "ufunc/raw-obs", "raw observations");
    }
    {
        // Adversarial: bad Func ptr, undefined-only flags bits, parm
        // incoherence — negative codes, verdict STILL Inconclusive.
        Arena ar;
        auto* fn = ar.alloc(kUFunctionTotalSize);
        Arena::put64(fn, kUFunctionFunc, 0x0000700000000001ull);  // misaligned
        Arena::put32(fn, kUFunctionFlags, 0x30u);  // only undefined bits
        (*fn)[kUFunctionNumParms] = 0;
        Arena::put16(fn, kUFunctionParmsSize, 5);  // parms without count
        UFunctionProbeResult r = probe_ufunction(ar.spans.data(),
                                                 ar.spans.size(),
                                                 ar.addr_of(fn));
        CHECK(r.verdict == Verdict::Inconclusive, "ufunc/negative-still-incl",
              "never Refuted by design");
        CHECK(has_code(r.codes, r.code_count, RCode::FN_N_FUNC_BAD) &&
                  has_code(r.codes, r.code_count,
                           RCode::FN_N_FLAGS_UNDEFINED_ONLY) &&
                  has_code(r.codes, r.code_count, RCode::FN_N_PARM_IMPLAUSIBLE),
              "ufunc/negative-codes", "expected negative codes");
    }
    {
        // Unreadable window.
        Arena ar;
        UFunctionProbeResult r = probe_ufunction(ar.spans.data(),
                                                 ar.spans.size(), 0x7fff0000);
        CHECK(r.verdict == Verdict::Inconclusive &&
                  has_code(r.codes, r.code_count, RCode::FN_E_UNREADABLE),
              "ufunc/unreadable", "FN_E_UNREADABLE");
    }

    // ------------------------------------------------------------------
    // FField variant discriminator
    // ------------------------------------------------------------------
    {
        // UE5-positive: ClassPrivate@0x8 = readable canonical ptr whose LOW
        // dword carries bit 30 (outside RF_AllFlags) so H4's flags check
        // structurally fails -> exactly one survivor -> Validated UE5_FField.
        Arena ar;
        ObjCommon c(ar);
        auto* fclass = ar.alloc_at(0x123440000000ull, 64);  // low dword 0x40000000
        (void)fclass;
        Gua g = build_gua(ar, 4, 8);
        FieldOpts o;
        o.qword_8 = 0x123440000000ull;  // H5 class ptr; H4 flags 0x40000000
        o.h4_vptr = c.vtable_addr;
        o.h4_class = c.cls_addr;
        auto* f = make_field(ar, o);
        FieldVariantResult r = probe_ffield_variant(
            ar.spans.data(), ar.spans.size(), ar.addr_of(f), &g.view);
        CHECK(r.verdict == Verdict::Validated &&
                  r.variant == FieldVariant::UE5_FField,
              "ffield/ue5-validated", rcode_string(r.h4_code));
        CHECK(r.h4_verdict == Verdict::Refuted &&
                  r.h4_code == RCode::FLD_H4_R_FLAGS_MASK,
              "ffield/ue5-h4-refuted-by-mask", "expected flags refute");
        // no-gua variant: same image, no view -> still Validated + no-evidence
        FieldVariantResult r2 =
            probe_ffield_variant(ar.spans.data(), ar.spans.size(),
                                 ar.addr_of(f), nullptr);
        CHECK(r2.verdict == Verdict::Validated &&
                  has_code(r2.codes, r2.code_count, RCode::FLD_E_NO_GUA_EVIDENCE),
              "ffield/no-gua-support-code", "FLD_E_NO_GUA_EVIDENCE");
    }
    {
        // UE4-positive: flags|index qword is UNALIGNED as a pointer (H5 class
        // check fails) while index round-trips into the array (H4 support).
        Arena ar;
        ObjCommon c(ar);
        Gua g = build_gua(ar, 4, 8);
        FieldOpts o;
        o.flags_8 = 0x1;   // RF_Public
        o.index_c = 1;     // qword 0x0000000100000001 -> &7 == 1, H5 dies
        o.h4_vptr = c.vtable_addr;
        o.h4_class = c.cls_addr;
        auto* f = make_field(ar, o);
        uint64_t faddr = ar.addr_of(f);
        g.set_item(ar, 1, faddr);  // round-trip target
        FieldVariantResult r = probe_ffield_variant(ar.spans.data(),
                                                    ar.spans.size(), faddr,
                                                    &g.view);
        CHECK(r.verdict == Verdict::Validated &&
                  r.variant == FieldVariant::UE4_UProperty,
              "ffield/ue4-validated", rcode_string(r.h5_code));
        CHECK(r.h5_verdict == Verdict::Refuted &&
                  r.h5_code == RCode::FLD_H5_R_CLASS_BAD,
              "ffield/ue4-h5-refuted", "expected class refute");
        CHECK(has_code(r.codes, r.code_count, RCode::FLD_I_H4_ROUNDTRIP_HIT),
              "ffield/roundtrip-hit", "missing support code");
    }
    {
        // Ambiguous: qword@0x8 readable canonical AND in-mask flags AND
        // round-tripping index; both hypotheses pass -> Inconclusive +
        // settling evidence populated (the exact additional evidence).
        Arena ar;
        ObjCommon c(ar);
        auto* fclass = ar.alloc_at(0x0000000100000008ull, 64);
        (void)fclass;
        Gua g = build_gua(ar, 4, 8);
        FieldOpts o;
        o.qword_8 = 0x0000000100000008ull;  // flags 0x8 in mask, index 1
        o.h4_vptr = c.vtable_addr;
        o.h4_class = c.cls_addr;
        auto* f = make_field(ar, o);
        uint64_t faddr = ar.addr_of(f);
        g.set_item(ar, 1, faddr);
        FieldVariantResult r = probe_ffield_variant(ar.spans.data(),
                                                    ar.spans.size(), faddr,
                                                    &g.view);
        CHECK(r.verdict == Verdict::Inconclusive &&
                  has_code(r.codes, r.code_count, RCode::FLD_E_AMBIGUOUS),
              "ffield/ambiguous", "expected FLD_E_AMBIGUOUS");
        CHECK(r.settling_evidence != nullptr, "ffield/settling-evidence",
              "settling evidence must name the missing width facts");
        CHECK(has_code(r.codes, r.code_count, RCode::FLD_I_H4_ROUNDTRIP_HIT),
              "ffield/ambiguous-roundtrip", "support code");
    }
    {
        // Neither hypothesis: vtable garbage (H4) + null class ptr (H5).
        Arena ar;
        Gua g = build_gua(ar, 4, 8);
        FieldOpts o;
        o.h4_vptr = 0x1;  // non-canonical
        o.qword_8 = 0;    // H5 ClassPrivate == 0
        o.flags_8 = 0;
        o.index_c = 0;
        auto* f = make_field(ar, o);
        FieldVariantResult r = probe_ffield_variant(ar.spans.data(),
                                                    ar.spans.size(),
                                                    ar.addr_of(f), &g.view);
        CHECK(r.verdict == Verdict::Refuted, "ffield/neither-refuted",
              "both hypotheses failed");
        CHECK(has_code(r.codes, r.code_count, RCode::FLD_H5_R_CLASS_NULL) &&
                  has_code(r.codes, r.code_count, RCode::FLD_H4_R_VPTR_BAD),
              "ffield/neither-codes", "expected both refute codes");
    }
    {
        // H5 ArrayDim==0 classifier refute (labeled engine-semantic in hdr).
        Arena ar;
        ObjCommon c(ar);
        auto* fc = ar.alloc_at(0x123440000000ull, 64);  // readable class target
        (void)fc;
        Gua g = build_gua(ar, 4, 8);
        FieldOpts o;
        o.qword_8 = 0x123440000000ull;  // kills H4 via flags mask
        o.h4_vptr = c.vtable_addr;
        o.h4_class = c.cls_addr;
        o.h5_array_dim = 0;
        auto* f = make_field(ar, o);
        FieldVariantResult r = probe_ffield_variant(ar.spans.data(),
                                                    ar.spans.size(),
                                                    ar.addr_of(f), &g.view);
        CHECK(r.verdict == Verdict::Refuted &&
                  r.h5_code == RCode::FLD_H5_R_ARRAYDIM_ZERO,
              "ffield/arraydim-zero", "expected classifier refute");
    }
    {
        // Unreadable anchor window -> Inconclusive (evidence unavailable).
        Arena ar;
        Gua g = build_gua(ar, 4, 8);
        FieldVariantResult r = probe_ffield_variant(ar.spans.data(),
                                                    ar.spans.size(), 0x7fff0000,
                                                    &g.view);
        CHECK(r.verdict == Verdict::Inconclusive &&
                  has_code(r.codes, r.code_count, RCode::FLD_E_ANCHOR_UNREADABLE),
              "ffield/anchor-unreadable", "FLD_E_ANCHOR_UNREADABLE");
    }
    {
        // P2 discipline: FName-width bytes (NamePrivate@0x20 / RepNotifyFunc@
        // 0x68) are never interpreted — weird values never flip verdicts.
        Arena ar;
        ObjCommon c(ar);
        auto* fclass = ar.alloc_at(0x123440000000ull, 64);
        (void)fclass;
        Gua g = build_gua(ar, 4, 8);
        FieldOpts o;
        o.qword_8 = 0x123440000000ull;
        o.h4_vptr = c.vtable_addr;
        o.h4_class = c.cls_addr;
        auto* f = make_field(ar, o);
        Arena::put64(f, kFFieldNamePrivate, 0xFFFFFFFFFFFF0000ull);  // FName-ish
        Arena::put64(f, kFPropRepNotifyUe5, 0xEEEEEEEEEEEEEEEEull);  // FName
        FieldVariantResult r = probe_ffield_variant(ar.spans.data(),
                                                    ar.spans.size(),
                                                    ar.addr_of(f), &g.view);
        CHECK(r.verdict == Verdict::Validated &&
                  r.variant == FieldVariant::UE5_FField,
              "ffield/fname-raw-not-interpreted", "P2 discipline");
    }
    {
        // Struct wrapper: ChildProperties head (UE5 expected) is used first;
        // Children head is the fallback; both null -> Inconclusive.
        Arena ar;
        ObjCommon c(ar);
        Gua g = build_gua(ar, 4, 8);
        auto* st = ar.alloc(kUStructTotalSize);
        // child property: UE5-positive image
        FieldOpts o;
        auto* fclass = ar.alloc_at(0x123440000000ull, 64);
        (void)fclass;
        o.qword_8 = 0x123440000000ull;
        o.h4_vptr = c.vtable_addr;
        o.h4_class = c.cls_addr;
        auto* f = make_field(ar, o);
        Arena::put64(st, kUStructChildProperties, ar.addr_of(f));
        Arena::put64(st, kUStructChildren, c.cls_addr);  // non-null too
        FieldVariantResult r = probe_ustruct_child_variant(
            ar.spans.data(), ar.spans.size(), ar.addr_of(st), &g.view);
        CHECK(r.verdict == Verdict::Validated &&
                  r.variant == FieldVariant::UE5_FField,
              "ffield/wrapper-childprops-first", "ChildProperties head used");
        // both heads null:
        auto* st2 = ar.alloc(kUStructTotalSize);
        FieldVariantResult r2 =
            probe_ustruct_child_variant(ar.spans.data(), ar.spans.size(),
                                        ar.addr_of(st2), &g.view);
        CHECK(r2.verdict == Verdict::Inconclusive &&
                  has_code(r2.codes, r2.code_count, RCode::OBJ_E_NO_CANDIDATES),
              "ffield/wrapper-no-children", "no anchor to test");
        // Children fallback:
        auto* st3 = ar.alloc(kUStructTotalSize);
        Arena::put64(st3, kUStructChildren, ar.addr_of(f));
        FieldVariantResult r3 =
            probe_ustruct_child_variant(ar.spans.data(), ar.spans.size(),
                                        ar.addr_of(st3), &g.view);
        CHECK(r3.verdict == Verdict::Validated, "ffield/wrapper-children-fallback",
              "Children head fallback");
    }

    std::printf("%s (%d failures)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
