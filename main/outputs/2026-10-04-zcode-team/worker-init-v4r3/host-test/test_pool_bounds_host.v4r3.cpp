// test_pool_bounds_host.cpp — host fixtures for the v4 FName-pool admission
// (worker-fname-validator-v4; supersedes the launch10-c suite 52/52).
//
// Drives the REAL staged SeedValidation.hpp functions end-to-end:
//   rc_seed::publish_readable_spans          (bridge-side publication filter, v4)
//   rc_seed::publish_nonimage_readable_spans (RETIRED alias the bridge still
//                                             calls — proven identical, AL1)
//   rc_seed::check_pool_shape                (initializer-side admission, v4)
//   rc_seed::check_none_anchor               (entry-0 'None' content anchor, v4)
//   rc_seed::check_entry_bounds / entry_location (v4-corrected regression guards)
//
// v4 SEMANTICS UNDER TEST (FNAME-POOL-RE.md R1-R3):
//   R1  CurrentBlock is the 0-BASED index of the OPEN block; the engine
//       predicate is id>>16 <= CurrentBlock (getter @0x23d570). The v3
//       `current_block >= 1` and strict `block_index < current_block`
//       FALSE-REFUSED a legitimate pool; both are replaced by the engine
//       invariant (current_block == 0 is VALID).
//   R2  STORAGE SEPARATION: the ALLOCATOR OBJECT is a static image-BSS
//       FMallocBinned2-family object (runtime 0x97089b8), but Blocks[0] is
//       an ALLOCATED BLOCK whose storage class is not pinned (RE section
//       7.1). The [0x970c000,0x9a34000) figure is a REGION-CLASS mapping
//       observation (LOAD7 BSS tail), NOT a captured Blocks[0] pointer
//       value. Image residency therefore no longer refuses: publication and
//       membership accept readable spans of BOTH classes; ownership
//       coverage (span membership) is unchanged.
//   R3  Entry 0 of Blocks[0] is ANSI 'None' (STATIC evidence: Init stores
//       it first, file 0x23a5d0). The content anchor replaces the dropped
//       outside-image clause as the wrong-base/wrong-pool guard.
//
// LABELING LAW (controller directive; provenance verdict worker-
// blocks0-provenance: NO CAPTURE EXISTS — no pinned observation ever
// recorded an actual Blocks[0] pointer value; [0x970c000,0x9a34000) is
// mapping-class only, igf3 v1_vmmap_page seq 4): the image-resident pool
// shape below is labeled candidate-image-resident-shape and is SYNTHETIC.
// BOTH storage classes are plausible-but-unproven and BOTH carry must-pass
// positives: (a) HEAP-RESIDENT class (A8/H3) — static RE predicts the
// ALLOCATION via FMemory::Malloc(0x20000, align 2) -> GMalloc binned ->
// OS-heap, likely OUTSIDE the image, even though the ALLOCATOR OBJECT is
// static BSS; (b) CANDIDATE-IMAGE-RESIDENT class (V1/END-TO-END/H6) — the
// region-class mapping record. Span-membership-either-class admission is
// the designed response to the uncertainty; neither class is 'observed'
// until a real capture (fc1pool probe, future user-coordinated window).
//
// RETAINED from the v3 suite: truncation honesty split (below-envelope =
// unmapped / at-or-above = truncated), the REQUIRED non-image heap positive
// (outside the retired frozen range), span-table-unavailable loud refusals,
// no-frozen-fallback, top-16 cap behavior, span lo/hi boundaries, constant
// pins. RETIRED with cause: the outside-image refusal cases (v3 A13/A14/A18
// semantics, D3 extent-precedence) — inverted or replaced with comments.
//
// Freestanding: clang++ -std=c++20 -Wall -Wextra -I../staging. Exit 0 = all
// pass. The binary writes its receipt to the FILE named by argv[1] (stdout
// pipe writes wedge uninterruptibly in this environment — see v3 REPORT).

#include "SeedValidation.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace rc_seed;

static int g_pass = 0;
static int g_fail = 0;
static FILE* g_out = nullptr;

static auto report(bool ok, const std::string& name, const char* detail = "") -> void
{
    // Null-safe: .failure is nullptr on success and is frequently passed as
    // the detail argument; constructing a std::string from nullptr is UB.
    const char* d = detail != nullptr ? detail : "";
    if (ok)
    {
        ++g_pass;
        std::fprintf(g_out, "[PASS] %s\n", name.c_str());
    }
    else
    {
        ++g_fail;
        std::fprintf(g_out, "[FAIL] %s: %s\n", name.c_str(), d);
    }
}

static auto report(bool ok, const std::string& name, const std::string& detail) -> void
{
    report(ok, name, detail.c_str());
}

static auto contains(const char* text, const char* needle) -> bool
{
    return text != nullptr && std::strstr(text, needle) != nullptr;
}

// The RETIRED launch-10 frozen predicate, replicated VERBATIM (base
// SeedValidation.hpp ca03e754) ONLY for the retained contradiction proof
// (A8/H4): the v4 path must succeed where the frozen bound refused.
static auto old_frozen_predicate(uint64_t blocks0) -> bool
{
    const uint64_t heap_lo = 0x8018c000ull;
    const uint64_t heap_hi = 0x100118000ull;
    if (heap_lo >= heap_hi) { return false; }
    return blocks0 >= heap_lo && blocks0 < heap_hi;
}

int main(int argc, char** argv)
{
    const char* receipt_path = argc > 1 ? argv[1] : "test_pool_bounds_v4.receipt.txt";
    g_out = std::fopen(receipt_path, "w");
    if (g_out == nullptr)
    {
        return 3; // cannot write the receipt: fail loudly by exit code
    }
    const uint64_t base = 0x400000ull; // kBaseObserved (validated per launch; publication-filter argument — v4 no longer derives a refusal from it)

    // ---- G: pinned constants (digit adjudication + floor derivation) ----
    report(kImageFileExtentBytes == 0xbc602c8ull, "G1 extent-bytes constant",
           "kImageFileExtentBytes drifted");
    report(base + kImageFileExtentBytes == 0xc0602c8ull, "G2 runtime extent end",
           "base+extent != 0xc0602c8 (GEOMETRY.md section 3)");
    report(kFNameBlockCapacityBytes == 0x20000ull, "G3 FName block capacity floor",
           "floor != FNameBlockOffsets(1<<16) * stride 2");
    report(kMaxPublishedSpans == 16, "G4 publication cap is 16", "cap drifted");
    report(kVmProtRead == 1u, "G5 PROT_READ bit", "PS5 PROT_READ is 1");

    // ---- Fixture A: realistic walk (igf3 7-span low map + giant vintage
    // span + modern high heap + noise), the launch-10-class shape ----
    const std::vector<VmSpanRecord> walk_a = {
        // image spans (extent [0x400000, 0xc0602c8))
        {0x400000, 0x787c000, 4},    // L0 --X (no READ bit)
        {0x787c000, 0x82bc000, 1},   // L1 R--
        {0x82bc000, 0x953c000, 1},   // L3 RW- (RELRO'd)
        {0x953c000, 0x9a34000, 3},   // L7 RW-  <- LOAD7 BSS tail class
        {0x9a34000, 0xc064000, 0},   // L8 --- (prot none)
        // non-image spans
        {0x8018c000, 0x100118000, 1}, // igf-vintage giant R-- span (the frozen envelope's origin)
        {0x300000000, 0x380000000, 3},// modern heap reservation (512 MiB)
        {0x400000000, 0x4000c000, 3}, // 48 KiB readable non-image (below floor)
        {0x100000, 0x300000, 3},      // below-base mapping, exactly one block capacity
        {0x200000000, 0x200040000, 4},// exec-only non-image (JIT class, no READ bit)
        {0x500000000, 0x500800000, 3},// 8 MiB readable non-image
    };
    AddressSpan pub_a[kMaxPublishedSpans]{};
    const auto pa = publish_readable_spans(walk_a.data(), walk_a.size(),
                                           base, kImageFileExtentBytes,
                                           pub_a, kMaxPublishedSpans);
    // v4: readable records publish IN FULL (no extent clipping). Qualifying:
    // L1, L3, L7 (image readable) + vintage, heap, below-base, noise
    // (non-image) = 7. Not: L0/L8/JIT (no READ bit), 48 KiB (sub-floor).
    report(pa.published == 7, "A1 v4 publishes 7 readable spans (3 image + 4 non-image)",
           "published=" + std::to_string(pa.published));
    report(pa.total == 7, "A2 total qualifying counted", "total=" + std::to_string(pa.total));
    report(pa.cap_saturated == 0, "A3 no cap saturation", "cap=" + std::to_string(pa.cap_saturated));
    report(pa.envelope_end == 0x500800000ull, "A3b envelope = max captured record end",
           "envelope=" + std::to_string(pa.envelope_end));
    bool ascending = true;
    for (size_t i = 1; i < pa.published; ++i)
    {
        if (pub_a[i].lo <= pub_a[i - 1].lo) { ascending = false; }
    }
    report(ascending, "A4 published ascending by address", "order violated");
    bool has_heap = false, has_vintage = false, has_below = false, has_noise = false;
    bool has_l1 = false, has_l3 = false, has_l7 = false;
    for (size_t i = 0; i < pa.published; ++i)
    {
        const auto& s = pub_a[i];
        if (s.lo == 0x300000000ull && s.hi == 0x380000000ull) { has_heap = true; }
        if (s.lo == 0x8018c000ull && s.hi == 0x100118000ull) { has_vintage = true; }
        if (s.lo == 0x100000ull && s.hi == 0x300000ull) { has_below = true; }
        if (s.lo == 0x500000000ull && s.hi == 0x500800000ull) { has_noise = true; }
        if (s.lo == 0x787c000ull && s.hi == 0x82bc000ull) { has_l1 = true; }
        if (s.lo == 0x82bc000ull && s.hi == 0x953c000ull) { has_l3 = true; }
        if (s.lo == 0x953c000ull && s.hi == 0x9a34000ull) { has_l7 = true; }
    }
    report(has_heap && has_vintage && has_below && has_noise,
           "A5 expected non-image spans present", "heap/vintage/below/noise missing");
    // v4 INVERSION of the v3 "A6 no image memory published": readable IMAGE
    // spans publish (allocator OBJECT is image BSS; Blocks[0]'s storage
    // class is not pinned — refusing image residency was the false-refuse).
    report(has_l1 && has_l3 && has_l7,
           "A6i v4: readable image spans (L1/L3/L7) publish in full",
           "an image-readable span is missing from the table");
    bool subfloor_absent = true;
    for (size_t i = 0; i < pa.published; ++i)
    {
        if (pub_a[i].lo == 0x400000000ull) { subfloor_absent = false; } // 48 KiB < one block capacity
    }
    report(subfloor_absent, "A7 sub-floor span excluded (48 KiB < 0x20000)",
           "sub-floor span published");
    bool exec_absent = true; // LOAD0 --X, LOAD8 ---, JIT --X never publish
    for (size_t i = 0; i < pa.published; ++i)
    {
        if (pub_a[i].lo == 0x400000ull || pub_a[i].lo == 0x9a34000ull ||
            pub_a[i].lo == 0x200000000ull) { exec_absent = false; }
    }
    report(exec_absent, "A6ii non-readable records still never publish",
           "exec/prot-none record leaked into the table");

    // ---- A8: REQUIRED POSITIVE — HEAP-RESIDENT CLASS (MUST-PASS,
    // retained from v3) ----
    // blocks0 in the modern heap span: aligned, inside a published readable
    // span, OUTSIDE the retired frozen range (0x300020000 > 0x100118000).
    // Plausible-but-unproven per static RE (allocation via Malloc -> GMalloc
    // binned -> OS-heap, likely outside the image); NOT captured — see the
    // labeling-law note above. Symmetric with V1 (candidate-image-resident).
    {
        const uint64_t blocks0 = 0x300020000ull;
        const auto shape = check_pool_shape(2, 0x10, blocks0, pub_a, pa.published, pa.envelope_end);
        const bool old_would_refuse = !old_frozen_predicate(blocks0);
        report(shape.ok && old_would_refuse &&
               std::string(shape.bounds_source) == "published-readable-spans",
               "A8 REQUIRED POSITIVE: heap blocks0 passes and FAILS the retired frozen bound",
               shape.ok ? ("old predicate says " + std::string(old_would_refuse ? "refuse" : "pass"))
                        : shape.failure);
    }

    // A9: vintage blocks0 (inside the old frozen range) still passes — the
    // retirement is a generalization, not a regression for that vintage.
    {
        const auto shape = check_pool_shape(2, 0x10, 0x80200000ull, pub_a, pa.published, pa.envelope_end);
        report(shape.ok, "A9 vintage-frozen-range blocks0 still passes",
               shape.ok ? "" : shape.failure);
    }

    // ---- V: v4 CurrentBlock-invariant + image-residency relaxation ----
    // V1 CANDIDATE-IMAGE-RESIDENT-SHAPE (SYNTHETIC — LABELING LAW): blocks0
    // = 0x970c000 sits in the LOAD7 readable span [0x953c000,0x9a34000),
    // whose region class at launch-9 was the Blocks[0] MAPPING observation.
    // This is a region-class record, NOT a captured Blocks[0] pointer value;
    // the shape is plausible per FNAME-POOL-RE and MUST pass — v3 refused it
    // twice over (CurrentBlock < 1 for current_block == 0, and the
    // outside-image clause), which is exactly the false-refuse v4 owns.
    {
        const auto shape = check_pool_shape(0, 0x10, 0x970c000ull, pub_a, pa.published, pa.envelope_end);
        report(shape.ok && std::string(shape.bounds_source) == "published-readable-spans",
               "V1 MUST-PASS candidate-image-resident-shape (SYNTHETIC): image-BSS span + CurrentBlock 0",
               shape.ok ? "" : shape.failure);
    }
    // V1b: pre-id-read words (none_id == 0, CurrentBlock == 0 — exactly what
    // FNamePool::Init leaves) must not false-refuse the shape either; the
    // engine invariant 0 <= 0 holds and the caller's id-wait owns the rest.
    {
        const auto shape = check_pool_shape(0, 0, 0x970c000ull, pub_a, pa.published, pa.envelope_end);
        report(shape.ok, "V1b post-Init words (CurrentBlock 0, none_id 0) pass the shape",
               shape.ok ? "" : shape.failure);
    }
    // V2 (directed refusal): the None entry's block index CANNOT exceed the
    // open block — engine invariant id>>16 <= CurrentBlock (@0x23d570).
    {
        const auto shape = check_pool_shape(0, 0x20000ull, 0x970c000ull, pub_a, pa.published, pa.envelope_end);
        report(!shape.ok && contains(shape.failure, "None entry block index exceeds CurrentBlock"),
               "V2 none_id block 1 > CurrentBlock 0 refuses (engine invariant)",
               shape.ok ? "passed" : shape.failure);
    }
    // V3: invariant boundary — none_id in block 1 with CurrentBlock 1 holds.
    {
        const auto shape = check_pool_shape(1, 0x10010ull, 0x970c000ull, pub_a, pa.published, pa.envelope_end);
        report(shape.ok, "V3 none_id block == CurrentBlock passes (0-based open block)",
               shape.ok ? "" : shape.failure);
    }
    // V4a: HONEST LIMIT, pinned — an image-resident value that is NOT a
    // block (the pool label itself, 0x9765ec0, inside LOAD7's readable
    // span) now passes the SHAPE, because shape-level checks are
    // region/word-only. This is the accepted R2 trade: wrong-value
    // protection moved to CONTENT (V4b).
    {
        const auto shape = check_pool_shape(2, 0x10, 0x9765ec0ull, pub_a, pa.published, pa.envelope_end);
        report(shape.ok, "V4a pinned limit: pool-label value passes shape (content check owns it now)",
               shape.ok ? "" : shape.failure);
    }
    // V4b: the content guard — realistic leading bytes at the pool label
    // (CurrentBlock dword, little-endian 02 00 00 00 ...) decode as u16
    // header 0x0002 -> len 0, NOT the 'None' anchor: refused.
    {
        const uint8_t label_bytes[] = {0x02, 0x00, 0x00, 0x00, 0x14, 0x00};
        const auto anchor = check_none_anchor(label_bytes, sizeof(label_bytes));
        report(!anchor.ok && contains(anchor.failure, "not the ANSI 'None' anchor"),
               "V4b entry-0 anchor refuses non-block (pool-label) bytes",
               anchor.ok ? "passed" : anchor.failure);
    }
    // V5 (replaces v3 A14): 0xc0602c0 is 8-aligned and inside the image, but
    // sits in the prot-none L8 record — NOT published, below the envelope:
    // refused by COVERAGE. The v3 'inside the image extent' string is
    // RETIRED; unreadable image memory is still never admitted.
    {
        const auto s_in = check_pool_shape(2, 0x10, 0xc0602c0ull, pub_a, pa.published, pa.envelope_end);
        report(!s_in.ok && contains(s_in.failure, "not covered by any published readable span"),
               "V5 unreadable image address refuses by coverage (extent string retired)", s_in.failure);
    }
    // V6: exec-only image text value (LOAD0 --X) — never published, refused
    // by coverage below the envelope (v3 A13 s2, expectation updated).
    {
        const auto s = check_pool_shape(2, 0x10, 0x3400000ull, pub_a, pa.published, pa.envelope_end);
        report(!s.ok && contains(s.failure, "not covered by any published readable span"),
               "V6 exec-only text value refuses by coverage", s.ok ? "passed" : s.failure);
    }

    // A10-A12: span boundaries — lo inclusive, hi exclusive (retained).
    report(check_pool_shape(1, 0x10, 0x300000000ull, pub_a, pa.published, pa.envelope_end).ok,
           "A10 span lo inclusive", "blocks0 == span lo refused");
    report(check_pool_shape(1, 0x10, 0x37FFFFFF8ull, pub_a, pa.published, pa.envelope_end).ok,
           "A11 span hi-8 accepted", "blocks0 == hi-8 refused");
    {
        const auto shape = check_pool_shape(1, 0x10, 0x380000000ull, pub_a, pa.published, pa.envelope_end);
        report(!shape.ok && contains(shape.failure, "not covered") &&
               contains(shape.failure, "capture complete below the envelope"),
               "A12 span hi exclusive (below-envelope miss = honest unmapped)", shape.failure);
    }

    // A15 (retained): first byte past the extent, below the envelope,
    // unpublished -> honest coverage miss (message unchanged in v4).
    {
        const auto s_out = check_pool_shape(2, 0x10, 0xc0602c8ull, pub_a, pa.published, pa.envelope_end);
        report(!s_out.ok && contains(s_out.failure, "not covered"),
               "A15 first byte past extent = honest coverage miss", s_out.failure);
    }

    // A16/A17: truncation honesty split (retained verbatim).
    {
        const auto shape = check_pool_shape(2, 0x10, 0x390000000ull, pub_a, pa.published, pa.envelope_end);
        report(!shape.ok && contains(shape.failure, "not covered by any published readable span"),
               "A16 below-envelope miss = honest unmapped/unreadable", shape.ok ? "passed" : shape.failure);
    }
    {
        const auto shape = check_pool_shape(2, 0x10, 0x600000000ull, pub_a, pa.published, pa.envelope_end);
        report(!shape.ok && contains(shape.failure, "snapshot truncated") &&
               std::string(shape.bounds_source) == "capture-truncated",
               "A17 beyond-envelope miss refuses as truncation, not unmapped", shape.failure);
    }

    // A19/A20 (retained verbatim; A18 is RETIRED — the 'CurrentBlock < 1'
    // refusal no longer exists by design, replaced by the V-series).
    {
        const auto s2 = check_pool_shape(2, 0x10, 0, pub_a, pa.published, pa.envelope_end);
        report(!s2.ok && std::string(s2.failure) == "Blocks[0] is null",
               "A19 null Blocks[0] legacy string preserved", s2.failure);
        const auto s3 = check_pool_shape(2, 0x10, 0x300020004ull, pub_a, pa.published, pa.envelope_end);
        report(!s3.ok && std::string(s3.failure) == "Blocks[0] is not 8-aligned",
               "A20 unaligned Blocks[0] refuses", s3.failure);
    }

    // A21 (retained): bounds_source provenance on success.
    report(std::string(check_pool_shape(2, 0x10, 0x300020000ull, pub_a, pa.published, pa.envelope_end).bounds_source) ==
               "published-readable-spans",
           "A21 success provenance is published-readable-spans", "source drifted");

    // ---- Fixture B: empty/unavailable span table = LOUD refusal, never a
    // fallback to the frozen envelope (any pool state, any envelope) ----
    {
        const auto s1 = check_pool_shape(2, 0x10, 0x300020000ull, nullptr, 0, 0x500800000ull);
        const auto s2 = check_pool_shape(2, 0x10, 0x300020000ull, pub_a, 0, 0);
        const auto s3 = check_pool_shape(2, 0x10, 0, nullptr, 0, 0);
        const auto s4 = check_pool_shape(0, 0, 0x970c000ull, pub_a, 0, 0);
        report(!s1.ok && contains(s1.failure, "span table unavailable") &&
               std::string(s1.bounds_source) == "span-table-unavailable",
               "B1 null table loud refusal", s1.failure);
        report(!s2.ok && contains(s2.failure, "span table unavailable"),
               "B2 zero-count table loud refusal", s2.failure);
        report(!s3.ok && contains(s3.failure, "span table unavailable"),
               "B3 empty table + null blocks0 still names the table", s3.failure);
        report(!s4.ok && contains(s4.failure, "span table unavailable"),
               "B4 empty table + candidate-image-resident blocks0 still names the table", s4.failure);
        // No frozen fallback: a blocks0 INSIDE the old frozen range must NOT
        // pass when the table is unavailable (the envelope is retired).
        const auto s5 = check_pool_shape(2, 0x10, 0x90000000ull, nullptr, 0, 0);
        report(!s5.ok, "B5 no frozen fallback (in-envelope blocks0 refused without table)", s5.failure);
    }

    // ---- Fixture C: top-16-by-size cap, saturation counted, ascending ----
    {
        std::vector<VmSpanRecord> walk_c;
        for (uint32_t i = 0; i < 20; ++i)
        {
            const uint64_t lo = 0x1000000000ull + i * 0x1000000ull;
            walk_c.push_back({lo, lo + 0x20000ull * (i + 1), 3});
        }
        AddressSpan pub_c[kMaxPublishedSpans]{};
        const auto pc = publish_readable_spans(walk_c.data(), walk_c.size(),
                                               base, kImageFileExtentBytes,
                                               pub_c, kMaxPublishedSpans);
        report(pc.published == 16 && pc.total == 20 && pc.cap_saturated == 1,
               "C1 top-16 cap with honest saturation counters",
               "published=" + std::to_string(pc.published) + " total=" + std::to_string(pc.total));
        report(pc.envelope_end == 0x1000000000ull + 19 * 0x1000000ull + 0x20000ull * 20,
               "C1b envelope = largest captured end", "envelope=" + std::to_string(pc.envelope_end));
        ascending = true;
        for (size_t i = 1; i < pc.published; ++i)
        {
            if (pub_c[i].lo <= pub_c[i - 1].lo) { ascending = false; }
        }
        report(ascending, "C2 capped publication still ascending", "order violated");
        bool smallest_dropped = true; // spans 0..3 (sizes 0x20000..0x80000) must be gone
        for (size_t i = 0; i < pc.published; ++i)
        {
            if (pub_c[i].lo == 0x1000000000ull || pub_c[i].lo == 0x1001000000ull ||
                pub_c[i].lo == 0x1002000000ull || pub_c[i].lo == 0x1003000000ull)
            {
                smallest_dropped = false;
            }
        }
        report(smallest_dropped, "C3 four smallest spans dropped by size", "kept wrong end");
        const auto kept = check_pool_shape(2, 0x10, 0x1000000000ull + 10 * 0x1000000ull + 0x40, pub_c, pc.published, pc.envelope_end);
        report(kept.ok, "C4 kept-span membership passes", kept.failure);
        // A dropped-span pointer below the envelope: honest coverage miss
        // (this is the DOCUMENTED cap false-refuse limit, visible as such).
        const auto dropped = check_pool_shape(2, 0x10, 0x1000000000ull + 1 * 0x1000000ull + 0x40, pub_c, pc.published, pc.envelope_end);
        report(!dropped.ok && contains(dropped.failure, "not covered"),
               "C5 dropped-span pointer refuses (documented cap limit)", dropped.failure);
    }

    // ---- Fixture D: extent straddlers, v4 edition ----
    // v3 clipped straddlers to their outside-extent remainder and refused
    // the image part; v4 publishes the WHOLE readable record (R2) — one
    // span per qualifying record, no clipping, no extent precedence.
    {
        const std::vector<VmSpanRecord> walk_d = {
            {0x9a30000, 0xd0602c8, 3}, // straddler: readable across the extent end
        };
        AddressSpan pub_d[kMaxPublishedSpans]{};
        const auto pd = publish_readable_spans(walk_d.data(), walk_d.size(),
                                               base, kImageFileExtentBytes,
                                               pub_d, kMaxPublishedSpans);
        report(pd.published == 1 && pub_d[0].lo == 0x9a30000ull && pub_d[0].hi == 0xd0602c8ull,
               "D1 v4 straddler publishes IN FULL (no extent clipping)",
               "published=" + std::to_string(pd.published));
        report(pd.envelope_end == 0xd0602c8ull, "D1b envelope counts the record end",
               "envelope=" + std::to_string(pd.envelope_end));
        const auto in_tail = check_pool_shape(2, 0x10, 0xc070000ull, pub_d, pd.published, pd.envelope_end);
        report(in_tail.ok, "D2 past-extent membership passes (retained)", in_tail.failure);
        // D3 INVERTED (was 'extent check precedes membership'): the
        // straddler's IMAGE part is readable and published — membership
        // passes there now. Same candidate-image-resident class as V1
        // (region evidence, not a captured pointer; content anchor owns
        // wrong-value protection).
        const auto in_image = check_pool_shape(2, 0x10, 0xc060000ull, pub_d, pd.published, pd.envelope_end);
        report(in_image.ok, "D3i INVERTED: straddler image part admits (readable image publishes)",
               in_image.ok ? "" : in_image.failure);
    }

    // ---- Fixture E: consumer-side bounded snapshot (fewer copied than stored) ----
    // pub_a ascending: [0]=below-base, [1]=L1, [2]=L3, [3]=L7, [4]=heap,
    // [5]=vintage, [6]=noise. A 2-span snapshot excludes the heap span.
    {
        const auto shape = check_pool_shape(2, 0x10, 0x300020000ull, pub_a, 2, pa.envelope_end);
        report(!shape.ok && contains(shape.failure, "not covered"),
               "E1 partial snapshot refuses honestly", shape.failure);
    }

    // ---- Fixture F: entry-chain regression guards, v4 semantics ----
    // v4 correction (R1): the OPEN block (index == CurrentBlock) legitimately
    // holds entries; CBO bounds them. Older blocks pass pure-side.
    {
        const auto f1 = check_entry_bounds(0x42, 3, 0x1000, 2 + 4); // block 0, older than open block 3
        report(f1.ok, "F1 entry bounds pass (older block)", f1.failure);
        // F2 INVERTED (v3 refused this: 'at or past CurrentBlock'): block 1
        // IS the open block when CurrentBlock == 1 — the entry must merely
        // fit inside CurrentByteOffset. With CBO 0x1000 it does.
        const auto f2 = check_entry_bounds(0x10000, 1, 0x1000, 2 + 4);
        report(f2.ok, "F2i INVERTED: open-block entry passes (CurrentBlock is 0-based open index)",
               f2.ok ? "" : f2.failure);
        // F2r (new refusal): open-block entry past CurrentByteOffset.
        const auto f2r = check_entry_bounds(0x10000, 1, 4, 2 + 4);
        report(!f2r.ok && std::string(f2r.failure) == "entry extends past CurrentByteOffset of the open block",
               "F2r open-block entry past CBO refuses", f2r.failure);
        // F2x (new refusal): block index beyond the open block.
        const auto f2x = check_entry_bounds(0x20000, 1, 0x1000, 2 + 4);
        report(!f2x.ok && std::string(f2x.failure) == "entry block index exceeds CurrentBlock (engine invariant id>>16 <= CurrentBlock)",
               "F2x entry block beyond CurrentBlock refuses", f2x.failure);
        const auto f3 = entry_location(0x12345);
        report(f3.block_index == 1 && f3.byte_offset == 0x468a,
               "F3 id decomposition unchanged (block=1, offset=(0x2345)*2)", "decomposition drifted");
    }

    // ---- AN: entry-0 'None' content anchor (v4 R3) ----
    {
        const uint8_t none_entry[6] = {0x00, 0x01, 'N', 'o', 'n', 'e'}; // header 0x0100: len 4, wide 0
        const auto an1 = check_none_anchor(none_entry, sizeof(none_entry));
        report(an1.ok, "AN1 valid 'None' anchor passes (h>>6==4, h&1==0)", an1.failure);

        const uint8_t wrong_payload[6] = {0x00, 0x01, 'N', 'o', 'n', 'a'};
        const auto an2 = check_none_anchor(wrong_payload, sizeof(wrong_payload));
        report(!an2.ok && contains(an2.failure, "not the ANSI 'None' anchor"),
               "AN2 wrong payload refuses the anchor", an2.ok ? "passed" : an2.failure);

        const uint8_t wide_entry[6] = {0x01, 0x01, 'N', 'o', 'n', 'e'}; // wide bit set
        const auto an3 = check_none_anchor(wide_entry, sizeof(wide_entry));
        report(!an3.ok && contains(an3.failure, "not the ANSI 'None' anchor"),
               "AN3 wide header refuses the anchor", an3.ok ? "passed" : an3.failure);

        const uint8_t zero_header[6] = {0x00, 0x00, 'N', 'o', 'n', 'e'}; // len 0
        const auto an4 = check_none_anchor(zero_header, sizeof(zero_header));
        report(!an4.ok && contains(an4.failure, "not the ANSI 'None' anchor"),
               "AN4 zero header refuses the anchor", an4.ok ? "passed" : an4.failure);

        const auto an6 = check_none_anchor(none_entry, 1); // too small for the u16 header
        report(!an6.ok && contains(an6.failure, "not the ANSI 'None' anchor"),
               "AN6 truncated read refuses the anchor", an6.ok ? "passed" : an6.failure);
    }

    // ---- END-TO-END: candidate-image-resident-shape through the FULL v4
    // chain (SYNTHETIC — labeling law; MUST-PASS: plausible per RE, and the
    // class v3 false-refused). Publication -> shape (CurrentBlock 0,
    // image-BSS span membership) -> entry-0 anchor -> entry bounds for a
    // post-Init pool (None at block0/offset0, ByteProperty at offset 6,
    // CBO 20). This is the pure-function mirror of the initializer's
    // Phase 3 for the corrected semantics.
    {
        const auto shape = check_pool_shape(0, 0x10, 0x970c000ull, pub_a, pa.published, pa.envelope_end);
        const uint8_t none_entry[6] = {0x00, 0x01, 'N', 'o', 'n', 'e'};
        const auto anchor = check_none_anchor(none_entry, sizeof(none_entry));
        const auto none_bounds = check_entry_bounds(0x00000000u, 0, 20, 2 + 4);   // id 0: block 0, offset 0
        const auto bp_bounds = check_entry_bounds(0x00000003u, 0, 20, 2 + 12);    // id 3: block 0, offset 6; 6+14 == 20
        report(shape.ok && anchor.ok && none_bounds.ok && bp_bounds.ok,
               "END-TO-END candidate-image-resident-shape: shape+anchor+entry-bounds all pass",
               shape.ok ? (anchor.ok ? (none_bounds.ok ? bp_bounds.failure : none_bounds.failure) : anchor.failure) : shape.failure);
    }

    // ---- Fixture H: coverage PROOF (retained, v4-updated) ----
    // Complete 16,384-capacity capture vs observed vm_total 10,019; the heap
    // record sits far past any 1,024-record prefix (the truncation class the
    // core-side kMaxVmMappings=1024 buffer lives in).
    {
        const uint64_t heap_lo = 0x300000000ull;
        const uint64_t heap_hi = 0x380000000ull;
        std::vector<VmSpanRecord> walk_h;
        walk_h.reserve(10019);
        // First 8 records: the image low map (inside the first-1,024 prefix).
        walk_h.push_back({0x400000, 0x787c000, 4});
        walk_h.push_back({0x787c000, 0x82bc000, 1});
        walk_h.push_back({0x82bc000, 0x953c000, 1});
        walk_h.push_back({0x953c000, 0x9a34000, 3});
        walk_h.push_back({0x9a34000, 0xc064000, 0});
        walk_h.push_back({0x8018c000, 0x100118000, 1}); // vintage giant span
        walk_h.push_back({0x100118000, 0x100138000, 3}); // 128 KiB tail (exactly one block capacity)
        walk_h.push_back({0x200000000, 0x200040000, 4}); // JIT (exec-only)
        // Records 8..3407: ascending sub-page-scale filler mappings below the
        // heap (sub-floor: only real reservations qualify).
        uint64_t cursor = 0x200400000ull;
        while (walk_h.size() < 3408)
        {
            walk_h.push_back({cursor, cursor + 0x8000, 3});
            cursor += 0x120000;
        }
        // Record 3408: THE heap reservation — record ~3.4k, address ~12.9 GiB,
        // FAR past the first-1,024 prefix a 1,024-capacity capture would keep.
        walk_h.push_back({heap_lo, heap_hi, 3});
        // Records 3410..10018: filler above the heap, enumeration stays
        // address-ascending so max(record end) == last record end.
        cursor = 0x380400000ull;
        while (walk_h.size() < 10019)
        {
            walk_h.push_back({cursor, cursor + 0x8000, 3});
            cursor += 0x120000;
        }
        report(walk_h.size() == 10019, "H0 fixture mirrors observed vm_total 10,019",
               "size=" + std::to_string(walk_h.size()));

        AddressSpan pub_h[kMaxPublishedSpans]{};
        const auto ph = publish_readable_spans(walk_h.data(), walk_h.size(),
                                               base, kImageFileExtentBytes,
                                               pub_h, kMaxPublishedSpans);
        report(ph.envelope_end == walk_h.back().end && ph.envelope_end > heap_hi,
               "H1 complete capture envelope extends past the heap",
               "envelope=" + std::to_string(ph.envelope_end));
        bool heap_published = false;
        bool l7_published = false;
        for (size_t i = 0; i < ph.published; ++i)
        {
            if (pub_h[i].lo == heap_lo && pub_h[i].hi == heap_hi) { heap_published = true; }
            if (pub_h[i].lo == 0x953c000ull && pub_h[i].hi == 0x9a34000ull) { l7_published = true; }
        }
        report(heap_published, "H2 high-address heap span survives publication",
               "heap span missing from table");
        report(l7_published, "H2b LOAD7 readable span survives publication (v4)",
               "L7 span missing from table");
        const auto ok_shape = check_pool_shape(2, 0x10, heap_lo + 0x20000ull, pub_h, ph.published, ph.envelope_end);
        report(ok_shape.ok, "H3 COVERAGE PROVEN: non-image pool blocks0 admitted from the complete capture",
               ok_shape.failure);
        const auto frozen_refuses = !old_frozen_predicate(heap_lo + 0x20000ull);
        report(frozen_refuses, "H4 the same blocks0 is OUTSIDE the retired frozen range",
               "old bound would have passed");
        // H6: the SYNTHETIC candidate-image-resident-shape (labeling law)
        // under the SAME complete capture — both storage classes admit from
        // one walk; ownership coverage identical.
        const auto img_shape = check_pool_shape(0, 0x10, 0x970c000ull, pub_h, ph.published, ph.envelope_end);
        report(img_shape.ok, "H6 candidate-image-resident-shape (SYNTHETIC) admitted from the complete capture",
               img_shape.ok ? "" : img_shape.failure);

        // Honesty when capacity IS exceeded: handing the filter only the
        // first N records (truncated prefix) must move the envelope to the
        // cut, so a beyond-cut blocks0 refuses as TRUNCATED, never unmapped.
        // cut 3405 ends inside the below-heap filler; the heap record
        // (index 3408) is NOT in the prefix.
        const size_t cut = 3405;
        AddressSpan pub_t[kMaxPublishedSpans]{};
        const auto pt = publish_readable_spans(walk_h.data(), cut,
                                               base, kImageFileExtentBytes,
                                               pub_t, kMaxPublishedSpans);
        const auto trunc = check_pool_shape(2, 0x10, heap_lo + 0x20000ull, pub_t, pt.published, pt.envelope_end);
        report(!trunc.ok && contains(trunc.failure, "snapshot truncated"),
               "H5 truncated capture refuses the same blocks0 as truncation, not unmapped",
               trunc.failure);
    }

    // ---- AL: the RETIRED alias (bridge call site pre-rename) is the SAME
    // filter byte-for-byte — what the bridge executes today is what these
    // fixtures proved.
    {
        AddressSpan via_alias[kMaxPublishedSpans]{};
        const auto palias = publish_readable_spans(walk_a.data(), walk_a.size(),
                                                            base, kImageFileExtentBytes,
                                                            via_alias, kMaxPublishedSpans);
        bool identical = palias.published == pa.published &&
                         palias.total == pa.total &&
                         palias.cap_saturated == pa.cap_saturated &&
                         palias.envelope_end == pa.envelope_end;
        for (size_t i = 0; identical && i < pa.published; ++i)
        {
            identical = via_alias[i].lo == pub_a[i].lo && via_alias[i].hi == pub_a[i].hi;
        }
        report(identical, "AL1 retired alias == v4 primary (bridge call site proven)",
               "alias diverges from the primary filter");
    }

    std::fprintf(g_out, "\n%d passed, %d failed\n", g_pass, g_fail);
    std::fclose(g_out);
    return g_fail == 0 ? 0 : 1;
}
