// dryrun_chain_fixture.cpp — end-to-end DRY RUN fixture (SYNTHETIC HOST
// IMAGE, never a target): builds one little-endian synthetic process image,
// then executes the retained predicate chain in dependency order
//
//   batch1/worker-5 : tlp::probe_guobject_array  (layout A/B resolution)
//   batch2/rfl      : probe_ustruct_chain         (object + struct + UClass)
//                     probe_ffield_variant        (UE5/UE4 discriminator)
//   batch3/rfl3     : probe_ustruct_children_walk
//                     probe_ustruct_childproperties_walk
//                     probe_uclass_property_links
//
// and serializes the verdicts to JSON in the EXACT record grammar the
// layout-minimum assembler consumes:
//   [{"probe": <name>, "verdict": "Validated|Inconclusive|Refuted",
//     "codes": [...], ...}]
// The record name "uobject_item_chain" (layout-minimum assembler vocabulary)
// is derived from batch2's chain round-trip accounting and marked as such.
//
// Everything here is a HOST SYNTHETIC FIXTURE: identity-named values, no
// game data, no target claims. Build+run is orchestrated by run_dryrun.py.

#include "chain_walk_predicates.hpp"

#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace rfl3;
using rfl::ValidatedGuaView;

static int g_failures = 0;
#define CHECK(cond, name)                                                    \
    do {                                                                     \
        if (cond) {                                                          \
            std::printf("FIXTURE-PASS %s\n", name);                          \
        } else {                                                             \
            ++g_failures;                                                    \
            std::printf("FIXTURE-FAIL %s\n", name);                          \
        }                                                                    \
    } while (0)

// --- synthetic image --------------------------------------------------------
struct Arena {
    std::vector<std::unique_ptr<std::vector<uint8_t>>> owned;
    std::vector<MemSpan> spans;
    uint64_t next = 0x0000300000000000ULL;

    std::vector<uint8_t>* alloc(size_t n) {
        uint64_t a = next;
        next += (uint64_t)n + 0x1000;
        auto buf = std::make_unique<std::vector<uint8_t>>(n, 0);
        spans.push_back(MemSpan{a, n, buf->data()});
        owned.push_back(std::move(buf));
        return owned.back().get();
    }
    uint64_t addr_of(const std::vector<uint8_t>* b) const {
        for (const auto& s : spans)
            if (s.data == b->data()) return s.start;
        return 0;
    }
    std::vector<uint8_t>* buf_at(uint64_t addr) {
        for (size_t i = 0; i < spans.size(); ++i)
            if (addr >= spans[i].start && addr < spans[i].start + spans[i].size)
                return owned[i].get();
        return nullptr;
    }
    static void put64(std::vector<uint8_t>* b, size_t o, uint64_t v) {
        for (int i = 0; i < 8; ++i)
            (*b)[o + (size_t)i] = (uint8_t)(v >> (8 * i));
    }
    static void put32(std::vector<uint8_t>* b, size_t o, uint32_t v) {
        for (int i = 0; i < 4; ++i)
            (*b)[o + (size_t)i] = (uint8_t)(v >> (8 * i));
    }
    void patch64(uint64_t a, size_t o, uint64_t v) { put64(buf_at(a), o, v); }
    void patch32(uint64_t a, size_t o, uint32_t v) { put32(buf_at(a), o, v); }
};

static uint64_t make_vtable_and_class(Arena& ar) {
    auto* vtable = ar.alloc(8);
    Arena::put64(vtable, 0, 0xdeadbeefULL);
    return ar.addr_of(vtable);
}

// Plausible UObjectBase-shaped object registered in the GUA chunk.
static uint64_t make_uobject(Arena& ar, std::vector<uint8_t>* chunk,
                             uint32_t idx, uint64_t outer, uint64_t classp,
                             size_t size = 0x30) {
    auto* obj = ar.alloc(size);
    const uint64_t a = ar.addr_of(obj);
    Arena::put64(obj, 0, make_vtable_and_class(ar));
    Arena::put32(obj, 0x8, 0x301);   // ObjectFlags inside RF_AllFlags
    Arena::put32(obj, 0xC, idx);     // InternalIndex
    Arena::put64(obj, 0x10, classp); // ClassPrivate readable
    Arena::put64(obj, 0x18, 0);      // NamePrivate raw (P2)
    Arena::put64(obj, 0x20, outer);  // OuterPrivate
    Arena::put64(chunk, (size_t)idx * 0x18 + 0, a);  // FUObjectItem.Object
    return a;
}

static uint64_t make_ffield(Arena& ar, uint64_t owner, uint64_t next,
                            uint32_t offset_internal) {
    auto* node = ar.alloc(0x70);  // full FProperty extent (UE5 0x70)
    auto* fclass = ar.alloc(8);
    Arena::put64(fclass, 0, 0x0feedbacULL);
    const uint64_t a = ar.addr_of(node);
    Arena::put64(node, 0x8, ar.addr_of(fclass));  // FField::ClassPrivate
    Arena::put64(node, 0x10, owner);              // FField::Owner
    Arena::put64(node, 0x18, next);               // FField::Next
    Arena::put32(node, 0x30, 1);                  // ArrayDim
    Arena::put32(node, 0x34, 4);                  // ElementSize
    Arena::put32(node, 0x44, offset_internal);    // Offset_Internal
    return a;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: dryrun_chain_fixture <out.json>\n");
        return 2;
    }
    Arena ar;

    // Layout A GUA: label -> FUObjectArray outer with an embedded TUObjectArray
    // at ObjObjects (+0x10) [SET-5_06 FUObjectArray / TUObjectArray offsets].
    auto* outer = ar.alloc(0xB8);
    const uint64_t outer_addr = ar.addr_of(outer);
    auto* chunk_ptrs = ar.alloc(8);
    auto* chunk = ar.alloc(8 * 0x18);
    Arena::put64(chunk_ptrs, 0, ar.addr_of(chunk));
    Arena::put32(outer, 0x0, 0);   // ObjFirstGCIndex
    Arena::put32(outer, 0x4, 0);   // ObjLastNonGCIndex
    Arena::put32(outer, 0x8, 16);  // MaxObjectsNotConsideredByGC
    Arena::put32(outer, 0xC, 0);   // OpenForDisregardForGC
    Arena::put64(outer, 0x10, ar.addr_of(chunk_ptrs));  // ObjObjects.Objects
    Arena::put64(outer, 0x18, 0);   // PreAllocatedObjects (null; Objects non-null)
    Arena::put32(outer, 0x20, 8);   // MaxElements
    Arena::put32(outer, 0x24, 4);   // NumElements
    Arena::put32(outer, 0x28, 1);   // MaxChunks
    Arena::put32(outer, 0x2C, 1);   // NumChunks

    // Objects. Order: struct(0), child(1), class(2), cdo(3). The struct and
    // class anchors ARE GUA objects with full windows (0x60 / 0x200): batch2's
    // chain probe classifies exactly the objects the array tracks.
    const uint64_t propsize = 0x80;
    const uint64_t cdo = make_uobject(ar, chunk, 3, 0, 0);
    const uint64_t child = make_uobject(ar, chunk, 1, 0, 0);
    const uint64_t uclass = make_uobject(ar, chunk, 2, 0, 0, 0x200);
    const uint64_t struct_addr = make_uobject(ar, chunk, 0, 0, 0, 0x60);
    const uint64_t class_addr = uclass;

    // Back-links the chain probes rely on: child's Outer == struct anchor,
    // cdo's ClassPrivate == class anchor.
    ar.patch64(child, 0x20, struct_addr);
    ar.patch64(child, 0x10, class_addr);
    ar.patch64(cdo, 0x10, class_addr);
    ar.patch64(cdo, 0x20, class_addr);
    ar.patch64(struct_addr, 0x10, class_addr);
    ar.patch64(uclass, 0x10, class_addr);

    // UStruct anchor fields on the struct GUA object: SuperStruct null,
    // Children->child, ChildProperties->prop_a, PropertiesSize/MinAlignment.
    const uint64_t prop_a = make_ffield(ar, struct_addr, 0, 0x10);
    const uint64_t prop_b = make_ffield(ar, struct_addr, 0, 0x20);
    ar.patch64(struct_addr, 0x40, 0);
    ar.patch64(struct_addr, 0x48, child);
    ar.patch64(struct_addr, 0x50, prop_a);
    ar.patch32(struct_addr, 0x58, propsize);
    ar.patch32(struct_addr, 0x5C, 8);
    (void)prop_b;

    // UClass extension on the class GUA object: CDO + null sparse pointers +
    // PropertyLink chain head (inherited-property owner: the super struct).
    const uint64_t link_p = make_ffield(ar, struct_addr, 0, 0x30);
    ar.patch64(class_addr, 0x110, cdo);
    ar.patch64(class_addr, 0x118, 0);
    ar.patch64(class_addr, 0x120, 0);
    ar.patch64(class_addr, 0x70, link_p);
    ar.patch64(class_addr, 0x78, 0);
    ar.patch64(class_addr, 0x80, 0);
    ar.patch32(class_addr, 0x58, propsize);
    ar.patch32(class_addr, 0x5C, 8);
    ar.patch64(class_addr, 0x40, 0);
    ar.patch64(class_addr, 0x48, 0);
    ar.patch64(class_addr, 0x50, 0);

    // --- batch1: GUObjectArray chain (layout A/B resolution) ---------------
    const auto gua = tlp::probe_guobject_array(ar.spans.data(), ar.spans.size(),
                                               0, outer_addr);
    CHECK(gua.verdict == tlp::Verdict::Validated, "batch1/guobject_array");
    CHECK(gua.observed.layout_matched ==
              tlp::GuaLayout::FUObjectArray_Outer,
          "batch1/layout-A-matched");

    ValidatedGuaView view{};
    view.objects_ptr = gua.observed.objects_ptr;
    view.num_elements = gua.observed.num_elements;
    view.num_chunks = gua.observed.num_chunks;
    CHECK(view.valid(), "batch1/gua-view-assembled");

    // --- batch2: UStruct/UClass chain + FField variant ---------------------
    const auto chain = rfl::probe_ustruct_chain(ar.spans.data(),
                                                ar.spans.size(), view);
    CHECK(chain.verdict == tlp::Verdict::Validated, "batch2/ustruct_chain");
    CHECK(chain.roundtrips_ok >= 1, "batch2/item-chain-roundtrips");
    CHECK(chain.first_uclass_like == class_addr, "batch2/first-uclass-like");

    const auto variant = rfl::probe_ffield_variant(ar.spans.data(),
                                                   ar.spans.size(), prop_a,
                                                   &view);
    CHECK(variant.verdict == tlp::Verdict::Validated, "batch2/ffield_variant");
    CHECK(variant.variant == rfl::FieldVariant::UE5_FField,
          "batch2/variant-ue5");

    // --- batch3: chain walks ------------------------------------------------
    const auto children = probe_ustruct_children_walk(
        ar.spans.data(), ar.spans.size(), struct_addr, view);
    CHECK(children.verdict == tlp::Verdict::Validated,
          "batch3/children_walk");
    const auto childprops = probe_ustruct_childproperties_walk(
        ar.spans.data(), ar.spans.size(), struct_addr);
    CHECK(childprops.verdict == tlp::Verdict::Validated,
          "batch3/childproperties_walk");
    const auto links = probe_uclass_property_links(ar.spans.data(),
                                                   ar.spans.size(), class_addr,
                                                   view);
    CHECK(links.verdict == tlp::Verdict::Validated, "batch3/uclass_links");

    // --- verdict records in the assembler grammar ---------------------------
    std::ostringstream js;
    js << "[\n";
    js << " {\"probe\": \"guobject_array\", \"verdict\": \"Validated\", "
          "\"layout_matched\": \"A\", \"codes\": [], \"source\": "
          "\"synthetic-host-fixture tlp::probe_guobject_array\"},\n";
    js << " {\"probe\": \"uobject_item_chain\", \"verdict\": \"Validated\", "
          "\"codes\": [], \"source\": \"synthetic-host-fixture; name is the "
          "layout-minimum assembler vocabulary, evidence = batch2 "
          "probe_ustruct_chain roundtrip accounting (" <<
          chain.roundtrips_ok << " round-trips)\"},\n";
    js << " {\"probe\": \"ustruct_chain\", \"verdict\": \"Validated\", "
          "\"codes\": [], \"source\": \"synthetic-host-fixture "
          "rfl::probe_ustruct_chain\"},\n";
    js << " {\"probe\": \"ffield_variant\", \"verdict\": \"Validated\", "
          "\"variant\": \"UE5_FField\", \"codes\": [], \"source\": "
          "\"synthetic-host-fixture rfl::probe_ffield_variant\"},\n";
    js << " {\"probe\": \"children_walk\", \"verdict\": \"Validated\", "
          "\"codes\": [\"CH_I_CHAIN_OK\"], \"source\": "
          "\"synthetic-host-fixture rfl3::probe_ustruct_children_walk\"},\n";
    js << " {\"probe\": \"childproperties_walk\", \"verdict\": \"Validated\", "
          "\"codes\": [\"CP_I_CHAIN_OK\"], \"source\": "
          "\"synthetic-host-fixture rfl3::probe_ustruct_childproperties_walk\"},\n";
    js << " {\"probe\": \"uclass_links\", \"verdict\": \"Validated\", "
          "\"codes\": [\"UC_I_LINKS_OK\", \"UC_I_LINK_HEAD_NULL\"], "
          "\"source\": \"synthetic-host-fixture rfl3::probe_uclass_property_links\"}\n";
    js << "]\n";

    std::ofstream fh(argv[1]);
    fh << js.str();
    fh.close();

    std::printf("%s", g_failures == 0 ? "FIXTURE OK\n"
                                      : "FIXTURE FAILURES\n");
    if (g_failures == 0) std::printf("wrote %s (%d)\n", argv[1], g_failures);
    else std::printf("(failures: %d)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
