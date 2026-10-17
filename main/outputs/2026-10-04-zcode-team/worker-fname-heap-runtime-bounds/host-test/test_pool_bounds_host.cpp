// test_pool_bounds_host.cpp — host fixtures for the launch10-c FName-pool
// runtime-bounds admission (worker-fname-heap-runtime-bounds).
//
// Drives the REAL staged SeedValidation.hpp functions end-to-end:
//   rc_seed::publish_nonimage_readable_spans  (bridge-side publication filter)
//   rc_seed::check_pool_shape                 (initializer-side admission)
//   rc_seed::check_entry_bounds / entry_location (regression guards)
//
// REQUIRED POSITIVE TEST (controller directive, non-negotiable): one
// successful end-to-end pass with a valid heap Blocks[0] that is (a) inside a
// published non-image readable span from a realistic walk fixture, (b)
// outside the image extent, AND (c) OUTSIDE the retired frozen range
// [0x8018c000,0x100118000) — proving the new path succeeds where the old
// frozen bound would have refused (old predicate replicated verbatim in this
// fixture ONLY for that contradiction proof).
//
// TRUNCATION HONESTY (same-capture rule): a coverage miss AT/ABOVE the
// captured envelope refuses as 'snapshot truncated' (omitted mapping, never
// 'unmapped'); a miss BELOW the envelope is a genuine unmapped/unreadable
// verdict. Fixture H proves the option-(a) coverage bar: a complete
// 16,384-record capture (observed vm_total 10,019) publishes the heap span
// and admits the real pool.
//
// Freestanding: clang++ -std=c++20 -I../staging. Exit 0 = all pass.

#include "SeedValidation.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace rc_seed;

static int g_pass = 0;
static int g_fail = 0;
// Receipt is written to a file the binary opens ITSELF (argv[1]); stdout is
// never used — writes to the harness pipe block uninterruptibly in the
// sandboxed environment (observed: binary wedged post-C3 in UE state with
// zero CPU; self-written files complete instantly).
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

// The OLD launch-10 admission predicate, replicated VERBATIM (base
// SeedValidation.hpp ca03e754, check_pool_shape lines 63-85) ONLY for the
// frozen-bound contradiction proof. The staged header no longer contains it.
static auto old_frozen_predicate(uint64_t blocks0) -> bool
{
    const uint64_t heap_lo = 0x8018c000ull;
    const uint64_t heap_hi = 0x100118000ull;
    if (heap_lo >= heap_hi) { return false; }
    return blocks0 >= heap_lo && blocks0 < heap_hi;
}

int main(int argc, char** argv)
{
    const char* receipt_path = argc > 1 ? argv[1] : "test_pool_bounds.receipt.txt";
    g_out = std::fopen(receipt_path, "w");
    if (g_out == nullptr)
    {
        return 3; // cannot write the receipt: fail loudly by exit code
    }
    const uint64_t base = 0x400000ull; // kBaseObserved (validated per launch)

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
        {0x953c000, 0x9a34000, 3},   // L7 RW-
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
    const auto pa = publish_nonimage_readable_spans(walk_a.data(), walk_a.size(),
                                                    base, kImageFileExtentBytes,
                                                    pub_a, kMaxPublishedSpans);
    report(pa.published == 4, "A1 publishes exactly the 4 qualifying spans",
           "published=" + std::to_string(pa.published));
    report(pa.total == 4, "A2 total qualifying counted", "total=" + std::to_string(pa.total));
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
    bool image_leak = false;
    for (size_t i = 0; i < pa.published; ++i)
    {
        const auto& s = pub_a[i];
        if (s.lo == 0x300000000ull && s.hi == 0x380000000ull) { has_heap = true; }
        if (s.lo == 0x8018c000ull && s.hi == 0x100118000ull) { has_vintage = true; }
        if (s.lo == 0x100000ull && s.hi == 0x300000ull) { has_below = true; }
        if (s.lo == 0x500000000ull && s.hi == 0x500800000ull) { has_noise = true; }
        if (s.lo < base + kImageFileExtentBytes && s.hi > base) { image_leak = true; }
    }
    report(has_heap && has_vintage && has_below && has_noise, "A5 expected spans present",
           "heap/vintage/below/noise missing");
    report(!image_leak, "A6 no image memory published", "image span leaked into table");
    bool subfloor_absent = true;
    for (size_t i = 0; i < pa.published; ++i)
    {
        if (pub_a[i].lo == 0x400000000ull) { subfloor_absent = false; } // 48 KiB < one block capacity
    }
    report(subfloor_absent, "A7 sub-floor span excluded (48 KiB < 0x20000)",
           "sub-floor span published");

    // ---- A8: REQUIRED POSITIVE TEST ----
    // blocks0 in the modern heap span: aligned, outside the image extent,
    // and OUTSIDE the retired frozen range (0x300020000 > 0x100118000).
    {
        const uint64_t blocks0 = 0x300020000ull;
        const auto shape = check_pool_shape(2, blocks0, pub_a, pa.published, pa.envelope_end, base);
        const bool old_would_refuse = !old_frozen_predicate(blocks0);
        report(shape.ok && old_would_refuse &&
               std::string(shape.bounds_source) == "published-readable-spans",
               "A8 REQUIRED POSITIVE: heap blocks0 passes NEW and FAILS OLD frozen bound",
               shape.ok ? ("old predicate says " + std::string(old_would_refuse ? "refuse" : "pass"))
                        : shape.failure);
    }

    // A9: vintage blocks0 (inside the old frozen range) still passes — the
    // retirement is a generalization, not a regression for that vintage.
    {
        const auto shape = check_pool_shape(2, 0x80200000ull, pub_a, pa.published, pa.envelope_end, base);
        report(shape.ok, "A9 vintage-frozen-range blocks0 still passes",
               shape.ok ? "" : shape.failure);
    }

    // A10-A12: span boundaries — lo inclusive, hi exclusive.
    report(check_pool_shape(1, 0x300000000ull, pub_a, pa.published, pa.envelope_end, base).ok,
           "A10 span lo inclusive", "blocks0 == span lo refused");
    report(check_pool_shape(1, 0x37FFFFFF8ull, pub_a, pa.published, pa.envelope_end, base).ok,
           "A11 span hi-8 accepted", "blocks0 == hi-8 refused");
    {
        const auto shape = check_pool_shape(1, 0x380000000ull, pub_a, pa.published, pa.envelope_end, base);
        report(!shape.ok && contains(shape.failure, "not covered") &&
               contains(shape.failure, "capture complete below the envelope"),
               "A12 span hi exclusive (below-envelope miss = honest unmapped)", shape.failure);
    }

    // A13: image-resident blocks0 (wrong-base arithmetic classes) refuses.
    {
        const auto s1 = check_pool_shape(2, 0x9765ec0ull, pub_a, pa.published, pa.envelope_end, base); // pool label itself
        const auto s2 = check_pool_shape(2, 0x3400000ull, pub_a, pa.published, pa.envelope_end, base); // text-region value
        report(!s1.ok && contains(s1.failure, "inside the image extent") &&
               !s2.ok && contains(s2.failure, "inside the image extent"),
               "A13 image-resident blocks0 refuses (wrong-base)",
               s1.ok || s2.ok ? "an image-resident pointer passed" : "");
    }

    // A14/A15: extent end boundary — 0xc0602c0 (8-aligned, inside) refuses
    // with the extent message; 0xc0602c8 outside the extent and below the
    // envelope but unpublished.
    {
        const auto s_in = check_pool_shape(2, 0xc0602c0ull, pub_a, pa.published, pa.envelope_end, base);
        const auto s_out = check_pool_shape(2, 0xc0602c8ull, pub_a, pa.published, pa.envelope_end, base);
        report(!s_in.ok && contains(s_in.failure, "inside the image extent"),
               "A14 extent end exclusive (0xc0602c0 inside)", s_in.failure);
        report(!s_out.ok && contains(s_out.failure, "not covered"),
               "A15 first byte past extent = honest coverage miss", s_out.failure);
    }

    // A16: below-envelope, aligned, non-image pointer with NO published
    // coverage refuses as genuinely unmapped/unreadable (capture is a
    // complete prefix below the envelope).
    {
        const auto shape = check_pool_shape(2, 0x390000000ull, pub_a, pa.published, pa.envelope_end, base);
        report(!shape.ok && contains(shape.failure, "not covered by any published readable span"),
               "A16 below-envelope miss = honest unmapped/unreadable", shape.ok ? "passed" : shape.failure);
    }

    // A17: AT/ABOVE the envelope the same class of miss must NEVER read as
    // 'unmapped' — it refuses with the dedicated truncation message.
    {
        const auto shape = check_pool_shape(2, 0x600000000ull, pub_a, pa.published, pa.envelope_end, base);
        report(!shape.ok && contains(shape.failure, "snapshot truncated") &&
               std::string(shape.bounds_source) == "capture-truncated",
               "A17 beyond-envelope miss refuses as truncation, not unmapped", shape.failure);
    }

    // A18-A20: legacy structural strings preserved (table live).
    {
        const auto s1 = check_pool_shape(0, 0x300020000ull, pub_a, pa.published, pa.envelope_end, base);
        report(!s1.ok && std::string(s1.failure) == "CurrentBlock < 1 (pool not initialized or wrong pool/base)",
               "A18 CurrentBlock < 1 legacy string preserved", s1.failure);
        const auto s2 = check_pool_shape(2, 0, pub_a, pa.published, pa.envelope_end, base);
        report(!s2.ok && std::string(s2.failure) == "Blocks[0] is null",
               "A19 null Blocks[0] legacy string preserved", s2.failure);
        const auto s3 = check_pool_shape(2, 0x300020004ull, pub_a, pa.published, pa.envelope_end, base);
        report(!s3.ok && std::string(s3.failure) == "Blocks[0] is not 8-aligned",
               "A20 unaligned Blocks[0] refuses", s3.failure);
    }

    // A21: bounds_source provenance on success.
    report(std::string(check_pool_shape(2, 0x300020000ull, pub_a, pa.published, pa.envelope_end, base).bounds_source) ==
               "published-readable-spans",
           "A21 success provenance is published-readable-spans", "source drifted");

    // ---- Fixture B: empty/unavailable span table = LOUD refusal, never a
    // fallback to the frozen envelope (any pool state, any envelope) ----
    {
        const auto s1 = check_pool_shape(2, 0x300020000ull, nullptr, 0, 0x500800000ull, base);
        const auto s2 = check_pool_shape(2, 0x300020000ull, pub_a, 0, 0, base);
        const auto s3 = check_pool_shape(2, 0, nullptr, 0, 0, base);
        const auto s4 = check_pool_shape(0, 0x300020000ull, pub_a, 0, 0, base);
        report(!s1.ok && contains(s1.failure, "span table unavailable") &&
               std::string(s1.bounds_source) == "span-table-unavailable",
               "B1 null table loud refusal", s1.failure);
        report(!s2.ok && contains(s2.failure, "span table unavailable"),
               "B2 zero-count table loud refusal", s2.failure);
        report(!s3.ok && contains(s3.failure, "span table unavailable"),
               "B3 empty table + null blocks0 still names the table", s3.failure);
        report(!s4.ok && contains(s4.failure, "span table unavailable"),
               "B4 empty table + CurrentBlock=0 still names the table", s4.failure);
        // No frozen fallback: a blocks0 INSIDE the old frozen range must NOT
        // pass when the table is unavailable (the envelope is retired).
        const auto s5 = check_pool_shape(2, 0x90000000ull, nullptr, 0, 0, base);
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
        const auto pc = publish_nonimage_readable_spans(walk_c.data(), walk_c.size(),
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
        const auto kept = check_pool_shape(2, 0x1000000000ull + 10 * 0x1000000ull + 0x40, pub_c, pc.published, pc.envelope_end, base);
        report(kept.ok, "C4 kept-span membership passes", kept.failure);
        // A dropped-span pointer below the envelope: honest coverage miss
        // (this is the DOCUMENTED cap false-refuse limit, visible as such).
        const auto dropped = check_pool_shape(2, 0x1000000000ull + 1 * 0x1000000ull + 0x40, pub_c, pc.published, pc.envelope_end, base);
        report(!dropped.ok && contains(dropped.failure, "not covered"),
               "C5 dropped-span pointer refuses (documented cap limit)", dropped.failure);
    }

    // ---- Fixture D: extent straddlers ----
    {
        const std::vector<VmSpanRecord> walk_d = {
            {0x9a30000, 0xc070000, 3},  // straddler, tail 0x3d38 < floor -> excluded
            {0x9a30000, 0xd0602c8, 3},  // straddler, tail 0x1000000 >= floor -> published clipped
        };
        AddressSpan pub_d[kMaxPublishedSpans]{};
        const auto pd = publish_nonimage_readable_spans(walk_d.data(), walk_d.size(),
                                                        base, kImageFileExtentBytes,
                                                        pub_d, kMaxPublishedSpans);
        report(pd.published == 1 && pub_d[0].lo == base + kImageFileExtentBytes &&
               pub_d[0].hi == 0xd0602c8ull,
               "D1 straddler clipped to outside-extent remainder",
               "published=" + std::to_string(pd.published));
        report(pd.envelope_end == 0xd0602c8ull, "D1b envelope counts prot-none-free records too",
               "envelope=" + std::to_string(pd.envelope_end));
        const auto in_tail = check_pool_shape(2, 0xc070000ull, pub_d, pd.published, pd.envelope_end, base);
        report(in_tail.ok, "D2 clipped-remainder membership passes", in_tail.failure);
        const auto in_image = check_pool_shape(2, 0xc060000ull, pub_d, pd.published, pd.envelope_end, base);
        report(!in_image.ok && contains(in_image.failure, "inside the image extent"),
               "D3 extent check precedes membership for straddler spans", in_image.failure);
    }

    // ---- Fixture E: consumer-side bounded snapshot (fewer copied than stored) ----
    {
        const auto shape = check_pool_shape(2, 0x300020000ull, pub_a, 2, pa.envelope_end, base); // heap span is index 2
        report(!shape.ok && contains(shape.failure, "not covered"),
               "E1 partial snapshot refuses honestly", shape.failure);
    }

    // ---- Fixture F: entry-chain regression guards (unchanged functions) ----
    {
        const auto f1 = check_entry_bounds(0x42, 3, 0x1000, 2 + 4);
        report(f1.ok, "F1 entry bounds pass unchanged", f1.failure);
        const auto f2 = check_entry_bounds(0x10000, 1, 0x1000, 2 + 4); // block 1 >= CurrentBlock 1
        report(!f2.ok && std::string(f2.failure) == "entry block index at or past CurrentBlock",
               "F2 entry bounds legacy string preserved", f2.failure);
        const auto f3 = entry_location(0x12345);
        report(f3.block_index == 1 && f3.byte_offset == 0x468a,
               "F3 id decomposition unchanged (block=1, offset=(0x2345)*2)", "decomposition drifted");
    }

    // ---- Fixture H: coverage PROOF (option (a), the production bar) ----
    // Simulates the launch-9 reality as a COMPLETE capture: kernel VMMAP
    // enumeration is address-ordered; vm_total 10,019 <= bridge capacity
    // 16,384 (static bss, launch-2 ENOMEM fix) -> one capture covers every
    // mapping including the high heap. The heap span sits at record ~9,500
    // (address ~2.9 GiB — FAR past the first-1,024 prefix that a 1,024-
    // capacity capture would keep, which is exactly the truncation class the
    // core-side kMaxVmMappings=1024 stack buffer lives in).
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
        walk_h.push_back({0x100118000, 0x100138000, 3}); // 128 KiB tail
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
        const auto ph = publish_nonimage_readable_spans(walk_h.data(), walk_h.size(),
                                                        base, kImageFileExtentBytes,
                                                        pub_h, kMaxPublishedSpans);
        report(ph.envelope_end == walk_h.back().end && ph.envelope_end > heap_hi,
               "H1 complete capture envelope extends past the heap",
               "envelope=" + std::to_string(ph.envelope_end));
        bool heap_published = false;
        for (size_t i = 0; i < ph.published; ++i)
        {
            if (pub_h[i].lo == heap_lo && pub_h[i].hi == heap_hi) { heap_published = true; }
        }
        report(heap_published, "H2 high-address heap span survives publication",
               "heap span missing from table");
        const auto ok_shape = check_pool_shape(2, heap_lo + 0x20000ull, pub_h, ph.published, ph.envelope_end, base);
        report(ok_shape.ok, "H3 COVERAGE PROVEN: real pool blocks0 admitted from the complete capture",
               ok_shape.failure);
        const auto frozen_refuses = !old_frozen_predicate(heap_lo + 0x20000ull);
        report(frozen_refuses, "H4 the same blocks0 is OUTSIDE the retired frozen range",
               "old bound would have passed");

        // Honesty when capacity IS exceeded: handing the filter only the
        // first N records (truncated prefix) must move the envelope to the
        // cut, so a beyond-cut blocks0 refuses as TRUNCATED, never unmapped.
        // cut 3405 ends inside the below-heap filler; the heap record
        // (index 3408) is NOT in the prefix.
        const size_t cut = 3405;
        AddressSpan pub_t[kMaxPublishedSpans]{};
        const auto pt = publish_nonimage_readable_spans(walk_h.data(), cut,
                                                        base, kImageFileExtentBytes,
                                                        pub_t, kMaxPublishedSpans);
        const auto trunc = check_pool_shape(2, heap_lo + 0x20000ull, pub_t, pt.published, pt.envelope_end, base);
        report(!trunc.ok && contains(trunc.failure, "snapshot truncated"),
               "H5 truncated capture refuses the same blocks0 as truncation, not unmapped",
               trunc.failure);
    }

    std::fprintf(g_out, "\n%d passed, %d failed\n", g_pass, g_fail);
    std::fclose(g_out);
    return g_fail == 0 ? 0 : 1;
}
