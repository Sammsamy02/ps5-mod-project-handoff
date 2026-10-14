// test_capture22k_host.cpp — host fixtures for the >CAPTURE-CAPACITY class
// (worker-capture-22k-fixture). Extends the v4 suite pattern
// (worker-fname-validator-v4/host-test, 69/69) against the FOLDED v4 header:
// staging/SeedValidation.hpp is a byte-identical copy of
// outputs/2026-10-04-zcode-controller/launch10-build/final-units/SeedValidation.hpp
// (sha256 f09ec9b319985cf86a5899894a30fc8d20a905072008ac8d4a35efbcaec3a5de).
// Originals untouched; this suite is staged-only.
//
// REAL-RUN CLASS UNDER TEST (worker-verdict-first-real G7, preserved fixtures):
//   run1 live walk: bridge_vmentry_entries = 22,761 against the 16,384 snapshot
//   capacity (truncated-honest; walk_stop = 0 — the sysctl payload parsed
//   cleanly to its end; the cut is the SNAPSHOT capacity, not a malformed
//   tail). run3: vmentry = 10,028 (complete; the production norm).
//   The v4 envelope logic refuses 'snapshot truncated: cannot prove heap
//   coverage' when blocks0 falls at/above the captured envelope — designed
//   for exactly this shape — but NO prior fixture exercised the >capacity
//   shape with real-ish numbers. This suite does.
//
// FIXTURE SHAPE (one synthetic 22,761-record kernel walk, address-ascending;
// kernel VMMAP enumeration is address-ordered, so a truncated capture is a
// PREFIX — SeedValidation.hpp PublicationResult.envelope_end contract):
//   records 0-7     image low map + vintage giant + JIT (pinned geometry,
//                   same numbers as the v3/v4 suites)
//   record 8000     BELOW-cut heap-class reservation, 512 MiB readable
//   records 9000..  16 medium readable spans (4 MiB) below the cut: the
//                   prefix QUALIFIES 22 spans (incl. the at-floor 128 KiB
//                   tail), so the top-16 window saturates — mirrors the real
//                   boots (G9: spanpub_cap=1, saturated at 16, on both runs)
//   records 17000 / 19000 / 22600   ABOVE-cut heap-class reservations
//                   (256 MiB / 128 MiB / 512 MiB; the last sits near the
//                   walk end — the TRUE heap top past the cut)
// Bridge parity: the snapshot stores only the FIRST 16,384 records
// (kBridgeVmmapEntries, runtime_start_bridge_r14_diag_spanpub.cpp:240) and
// bridge_spanpub_publish hands exactly that prefix (bounded = min(count,
// capacity)) to rc_seed::publish_readable_spans. This fixture mirrors that
// faithfully: the filter NEVER sees records 16,384..22,760.
//
//   (a) blocks0 inside the BELOW-cut heap span -> ADMITS (coverage proof
//       holds below the cut)
//   (b) blocks0 inside each ABOVE-cut heap span -> refuses 'snapshot
//       truncated' (bounds_source capture-truncated) — NEVER the
//       'not covered / unmapped' verdict class
//   (c) the published table reflects ONLY the first 16,384 records:
//       envelope end = max captured record end (the cut), strictly below
//       the true heap top; asserted on the real numbers (22,761 total /
//       16,384 kept / 6,377 omitted)
//   (d) complete-capture contrast: the SAME above-cut blocks0 values ADMIT
//       when the whole walk is published — the capacity-bump lever class —
//       proving the refusal is a capture-coverage property, not a property
//       of the addresses.
//
// Freestanding: clang++ -std=c++20 -Wall -Wextra -I../staging. Exit 0 = all
// pass. The binary writes its receipt to the FILE named by argv[1] (stdout
// pipe writes wedge uninterruptibly in this environment — see the v3/v4
// REPORTs).

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

// ---- real-run pinned numbers (worker-verdict-first-real G7 + bridge r14) ----
static constexpr size_t kCaptureCapacity = 16384; // kBridgeVmmapEntries (bridge:240, static bss)
static constexpr size_t kWalkTotal = 22761;       // run1 bridge_vmentry_entries (truncated-honest)
static constexpr size_t kRun3Total = 10028;       // run3 bridge_vmentry_entries (complete)
static constexpr size_t kOmitted = kWalkTotal - kCaptureCapacity; // 6,377 records past the cut

struct GeneratedWalk
{
    std::vector<VmSpanRecord> records;
    uint64_t heap_below_lo = 0, heap_below_hi = 0;
    uint64_t span_a_lo = 0, span_a_hi = 0;
    uint64_t span_b_lo = 0, span_b_hi = 0;
    uint64_t span_c_lo = 0, span_c_hi = 0;
    size_t heap_below_index = 0, span_a_index = 0, span_b_index = 0, span_c_index = 0;
};

// Deterministic, address-ascending, non-overlapping. Every special span takes
// the next record slot after filler fills up to its pinned index; all
// addresses/sizes stay 16 KiB-aligned so blocks0 probes are 8-aligned.
static auto generate_walk_22761() -> GeneratedWalk
{
    GeneratedWalk g;
    auto& w = g.records;
    w.reserve(kWalkTotal);
    // Records 0-7: pinned image low map + vintage giant + JIT (v3/v4 suite numbers).
    w.push_back({0x400000, 0x787c000, 4});     // L0 --X (no READ bit)
    w.push_back({0x787c000, 0x82bc000, 1});    // L1 R--
    w.push_back({0x82bc000, 0x953c000, 1});    // L3 RW-
    w.push_back({0x953c000, 0x9a34000, 3});    // L7 RW-
    w.push_back({0x9a34000, 0xc064000, 0});    // L8 --- (prot none)
    w.push_back({0x8018c000, 0x100118000, 1}); // vintage giant R--
    w.push_back({0x100118000, 0x100138000, 3}); // 128 KiB tail
    w.push_back({0x200000000, 0x200040000, 4}); // JIT --X

    uint64_t cursor = 0x200400000ull;
    auto filler = [&w, &cursor]() {
        w.push_back({cursor, cursor + 0x8000, 3}); // 32 KiB: sub-floor, never publishes
        cursor += 0x120000ull;
    };
    auto readable_span = [&w, &cursor](uint64_t size) {
        w.push_back({cursor, cursor + size, 3});
        cursor += size + 0x120000ull;
    };

    while (w.size() < 8000) { filler(); }
    g.heap_below_index = w.size(); // 8000 — BELOW the 16,384 cut
    g.heap_below_lo = cursor;
    g.heap_below_hi = cursor + 0x20000000ull; // 512 MiB
    readable_span(0x20000000ull);

    for (uint32_t i = 0; i < 16; ++i)
    {
        while (w.size() < 9000 + static_cast<size_t>(i) * 400) { filler(); }
        readable_span(0x400000ull); // 4 MiB mediums at records 9000..15000, all below the cut
    }

    while (w.size() < 17000) { filler(); }
    g.span_a_index = w.size(); // 17000 — ABOVE the cut
    g.span_a_lo = cursor;
    g.span_a_hi = cursor + 0x10000000ull; // 256 MiB
    readable_span(0x10000000ull);

    while (w.size() < 19000) { filler(); }
    g.span_b_index = w.size(); // 19000 — ABOVE the cut
    g.span_b_lo = cursor;
    g.span_b_hi = cursor + 0x8000000ull; // 128 MiB
    readable_span(0x8000000ull);

    while (w.size() < 22600) { filler(); }
    g.span_c_index = w.size(); // 22600 — ABOVE the cut, near the walk end
    g.span_c_lo = cursor;
    g.span_c_hi = cursor + 0x20000000ull; // 512 MiB — the TRUE heap top
    readable_span(0x20000000ull);

    while (w.size() < kWalkTotal) { filler(); }
    return g;
}

int main(int argc, char** argv)
{
    const char* receipt_path = argc > 1 ? argv[1] : "test_capture22k.receipt.txt";
    g_out = std::fopen(receipt_path, "w");
    if (g_out == nullptr)
    {
        return 3; // cannot write the receipt: fail loudly by exit code
    }
    const uint64_t base = 0x400000ull; // kBaseObserved (publication-filter argument, call-site stability)

    // ---- R: real-run pinned constants ----
    report(kCaptureCapacity == 16384, "R1 snapshot capacity pinned at 16,384 (kBridgeVmmapEntries, static bss)",
           "capacity drifted");
    report(kWalkTotal == 22761, "R2 run1 walk total pinned at 22,761 (bridge_vmentry_entries, G7)",
           "total drifted");
    report(kRun3Total <= kCaptureCapacity, "R3 run3 vm_total 10,028 fits the 16,384 capacity (production norm: complete)",
           "run3 class drifted");
    report(kOmitted == 6377 && kOmitted > 0, "R4 >capacity class: 6,377 records omitted past the cut",
           "omitted count drifted");
    report(kImageFileExtentBytes == 0xbc602c8ull && kFNameBlockCapacityBytes == 0x20000ull &&
               kMaxPublishedSpans == 16 && kVmProtRead == 1u,
           "R5 folded-header constants unchanged (extent/block-floor/cap/PROT_READ)",
           "constant drifted");

    // ---- W: the 22,761-record synthetic walk ----
    const GeneratedWalk gen = generate_walk_22761();
    const std::vector<VmSpanRecord>& full = gen.records;
    report(full.size() == kWalkTotal, "W0 walk built with exactly 22,761 records",
           "size=" + std::to_string(full.size()));
    bool ascending_nonoverlap = true;
    for (size_t i = 1; i < full.size(); ++i)
    {
        if (full[i].start < full[i - 1].end) { ascending_nonoverlap = false; }
    }
    report(ascending_nonoverlap, "W1 walk is address-ascending and non-overlapping (kernel enumeration order)",
           "order violated");
    report(gen.heap_below_index == 8000 && gen.heap_below_index < kCaptureCapacity,
           "W2 below-cut heap span sits at record 8000, inside the captured prefix",
           "index=" + std::to_string(gen.heap_below_index));
    report(gen.span_a_index == 17000 && gen.span_b_index == 19000 && gen.span_c_index == 22600 &&
               gen.span_a_index >= kCaptureCapacity && gen.span_b_index >= kCaptureCapacity &&
               gen.span_c_index >= kCaptureCapacity,
           "W3 all three heap-class tail spans sit BEYOND record 16,384",
           "indices drifted");
    bool mediums_below_cut = true;
    size_t medium_count = 0;
    for (size_t i = 0; i < full.size(); ++i)
    {
        if (full[i].end - full[i].start == 0x400000ull)
        {
            ++medium_count;
            if (i >= kCaptureCapacity) { mediums_below_cut = false; }
        }
    }
    report(mediums_below_cut && medium_count == 16, "W4 the 16 medium readable spans are all below the cut",
           "medium placement drifted");

    // ---- Bridge parity: store only the first 16,384 records (the snapshot
    // prefix; bridge_vmmap_walk stores while entries < capacity) and publish
    // exactly that prefix — the filter never sees records 16,384..22,760. ----
    const std::vector<VmSpanRecord> prefix(full.begin(),
                                           full.begin() + static_cast<std::ptrdiff_t>(kCaptureCapacity));
    report(prefix.size() == kCaptureCapacity, "W5 snapshot prefix holds exactly the first 16,384 records",
           "prefix size drifted");

    // ---- P: prefix publication (what bridge_spanpub_publish produces) ----
    AddressSpan pub[kMaxPublishedSpans]{};
    const auto pp = publish_readable_spans(prefix.data(), prefix.size(), base,
                                           kImageFileExtentBytes, pub, kMaxPublishedSpans);
    // Qualifying in the prefix: L1 + L3 + L7 (image readable) + vintage giant +
    // the 128 KiB tail (EXACTLY one block capacity: at-floor qualifies) +
    // below-cut heap + 16 mediums = 22; top-16 keeps the 16 largest
    // (vintage ~2 GiB, heap 512 MiB, L3/L1/L7, then the first 11 mediums —
    // equal sizes never replace; the tail and the last 5 mediums are dropped).
    report(pp.published == 16 && pp.total == 22 && pp.cap_saturated == 1,
           "P1 prefix qualifies 22 spans; top-16 window saturates (mirrors real spanpub_cap=1, G9)",
           "published=" + std::to_string(pp.published) + " total=" + std::to_string(pp.total) +
               " cap=" + std::to_string(pp.cap_saturated));

    // (c) envelope math on the real numbers: envelope = max captured record
    // end = the cut; NOT the true heap top.
    uint64_t max_prefix_end = 0;
    for (size_t i = 0; i < prefix.size(); ++i)
    {
        if (prefix[i].end > max_prefix_end) { max_prefix_end = prefix[i].end; }
    }
    report(pp.envelope_end == max_prefix_end,
           "E1 envelope end = max end over the CAPTURED prefix (explicit max loop)",
           "envelope=" + std::to_string(pp.envelope_end) + " max=" + std::to_string(max_prefix_end));
    report(pp.envelope_end == prefix.back().end,
           "E2 ascending non-overlapping enumeration: max captured end == last captured record end (the cut)",
           "envelope=" + std::to_string(pp.envelope_end) + " last=" + std::to_string(prefix.back().end));
    report(pp.envelope_end > gen.heap_below_hi,
           "P3 the below-cut heap span lies fully UNDER the envelope",
           "envelope=" + std::to_string(pp.envelope_end) + " heap_hi=" + std::to_string(gen.heap_below_hi));
    report(pp.envelope_end < gen.span_a_lo && pp.envelope_end < gen.span_b_lo && pp.envelope_end < gen.span_c_lo,
           "E3 the cut sits strictly BELOW every above-cut heap span (all 6,377 omitted records past the envelope)",
           "envelope=" + std::to_string(pp.envelope_end) + " span_a_lo=" + std::to_string(gen.span_a_lo));
    report(full[kCaptureCapacity].start >= pp.envelope_end,
           "E4 the FIRST omitted record (16,384) starts at/above the envelope (omitted <=> at/above the cut)",
           "first omitted start=" + std::to_string(full[kCaptureCapacity].start));
    report(pp.envelope_end < full.back().end,
           "E5 envelope end is NOT the true walk end (the true heap top lives past the cut)",
           "envelope=" + std::to_string(pp.envelope_end) + " true_end=" + std::to_string(full.back().end));

    bool pub_ascending = true;
    for (size_t i = 1; i < pp.published; ++i)
    {
        if (pub[i].lo <= pub[i - 1].lo) { pub_ascending = false; }
    }
    report(pub_ascending, "P5 published table ascending by address", "order violated");

    bool heap_below_published = false;
    bool any_above_cut_published = false;
    bool all_published_under_envelope = true;
    for (size_t i = 0; i < pp.published; ++i)
    {
        if (pub[i].lo == gen.heap_below_lo && pub[i].hi == gen.heap_below_hi) { heap_below_published = true; }
        if (pub[i].lo == gen.span_a_lo || pub[i].lo == gen.span_b_lo || pub[i].lo == gen.span_c_lo)
        {
            any_above_cut_published = true;
        }
        if (pub[i].hi > pp.envelope_end) { all_published_under_envelope = false; }
    }
    report(heap_below_published, "P6 the below-cut 512 MiB heap span survives the top-16 window",
           "heap span missing from the table");
    report(!any_above_cut_published,
           "P7 (c) the published table reflects ONLY the first 16,384 records: no above-cut span present",
           "an above-cut span leaked into the table");
    report(all_published_under_envelope, "P8 every published span ends at/below the envelope",
           "a published span crosses the cut");

    // ---- (a) blocks0 inside the below-cut heap span ADMITS ----
    {
        const uint64_t blocks0 = gen.heap_below_lo + 0x20000ull;
        const auto shape = check_pool_shape(2, 0x10, blocks0, pub, pp.published, pp.envelope_end);
        report(shape.ok && std::string(shape.bounds_source) == "published-readable-spans",
               "A1 (a) ADMITS: blocks0 inside a published span BELOW the cut (coverage proof holds)",
               shape.ok ? "" : shape.failure);
        const auto lo_shape = check_pool_shape(2, 0x10, gen.heap_below_lo, pub, pp.published, pp.envelope_end);
        const auto hi_shape = check_pool_shape(2, 0x10, gen.heap_below_hi - 8ull, pub, pp.published, pp.envelope_end);
        report(lo_shape.ok && hi_shape.ok, "A2 span boundaries below the cut: lo inclusive, hi-8 admitted",
               lo_shape.ok ? hi_shape.failure : lo_shape.failure);

        // END-TO-END through the folded v4 chain at that blocks0: shape +
        // entry-0 'None' anchor + entry bounds (mirror of the initializer's
        // Phase-3 pure side).
        const uint8_t none_entry[6] = {0x00, 0x01, 'N', 'o', 'n', 'e'}; // header 0x0100: len 4, wide 0
        const auto anchor = check_none_anchor(none_entry, sizeof(none_entry));
        const auto none_bounds = check_entry_bounds(0x00000000u, 0, 20, 2 + 4);
        const auto bp_bounds = check_entry_bounds(0x00000003u, 0, 20, 2 + 12);
        report(shape.ok && anchor.ok && none_bounds.ok && bp_bounds.ok,
               "A3 END-TO-END below-cut admission: shape + 'None' anchor + entry bounds all pass",
               shape.ok ? (anchor.ok ? (none_bounds.ok ? bp_bounds.failure : none_bounds.failure)
                                       : anchor.failure)
                         : shape.failure);
    }

    // ---- (b) blocks0 inside each ABOVE-cut heap span refuses as TRUNCATED ----
    {
        const uint64_t probes[3] = {gen.span_a_lo + 0x20000ull, gen.span_b_lo + 0x20000ull,
                                    gen.span_c_lo + 0x20000ull};
        const char* names[3] = {"spanA (256 MiB, record 17000)", "spanB (128 MiB, record 19000)",
                                "spanC (512 MiB, record 22600, true heap top)"};
        for (int i = 0; i < 3; ++i)
        {
            const auto shape = check_pool_shape(2, 0x10, probes[i], pub, pp.published, pp.envelope_end);
            const bool honest = !shape.ok && contains(shape.failure, "snapshot truncated") &&
                                std::string(shape.bounds_source) == "capture-truncated" &&
                                !contains(shape.failure, "not covered");
            report(honest,
                   std::string("T") + static_cast<char>('1' + i) + " (b) " + names[i] +
                       ": blocks0 above the cut refuses 'snapshot truncated', NEVER 'unmapped'",
                   shape.ok ? "WRONGLY ADMITTED" : (shape.failure ? shape.failure : ""));
        }
        // Cut-boundary honesty split:
        // exactly AT the envelope -> truncation class.
        const auto at_cut = check_pool_shape(2, 0x10, pp.envelope_end, pub, pp.published, pp.envelope_end);
        report(!at_cut.ok && contains(at_cut.failure, "snapshot truncated") &&
                   std::string(at_cut.bounds_source) == "capture-truncated",
               "T4 blocks0 == envelope end refuses as truncation (at/above the cut)",
               at_cut.ok ? "WRONGLY ADMITTED" : at_cut.failure);
        // 8 below the envelope: inside the last sub-floor filler record (never
        // published), capture is a complete prefix there -> the honest
        // unmapped/unreadable class, never truncation.
        const auto just_below = check_pool_shape(2, 0x10, pp.envelope_end - 8ull, pub, pp.published,
                                                 pp.envelope_end);
        report(!just_below.ok && contains(just_below.failure, "not covered") &&
                   contains(just_below.failure, "capture complete below the envelope") &&
                   !contains(just_below.failure, "snapshot truncated"),
               "T5 envelope-8 (below the cut, unpublished sub-floor record) = honest unmapped class",
               just_below.ok ? "WRONGLY ADMITTED" : just_below.failure);
    }

    // ---- (d) complete-capture contrast: the capacity-bump lever class ----
    {
        AddressSpan pub_full[kMaxPublishedSpans]{};
        const auto pf = publish_readable_spans(full.data(), full.size(), base, kImageFileExtentBytes,
                                               pub_full, kMaxPublishedSpans);
        report(pf.envelope_end == full.back().end,
               "C1 complete capture: envelope end == the TRUE walk end (the true heap top)",
               "envelope=" + std::to_string(pf.envelope_end));
        const uint64_t probes[3] = {gen.span_a_lo + 0x20000ull, gen.span_b_lo + 0x20000ull,
                                    gen.span_c_lo + 0x20000ull};
        bool all_admit = true;
        for (int i = 0; i < 3; ++i)
        {
            const auto shape = check_pool_shape(2, 0x10, probes[i], pub_full, pf.published, pf.envelope_end);
            if (!shape.ok || std::string(shape.bounds_source) != "published-readable-spans") { all_admit = false; }
        }
        report(all_admit,
               "C2 (d) the SAME above-cut blocks0 values ADMIT once the capture holds them (truncation is a capture-coverage property, not an address property)",
               "contrast failed");
        report(pf.published == 16 && pf.total == 25 && pf.cap_saturated == 1,
               "C3 complete capture qualifies 25 spans and still saturates the top-16 window (cap saturation is orthogonal to truncation)",
               "published=" + std::to_string(pf.published) + " total=" + std::to_string(pf.total));
    }

    // ---- D: the envelope is a caller contract (pinned honest limit) ----
    // Handing check_pool_shape a prefix-only table with an INFLATED envelope
    // (past the real cut) flips the above-cut refusal to the unmapped class —
    // the header trusts the caller. The initializer owns the contract:
    // capture_envelope = max(bridge_capture_envelope, max published span end),
    // and bridge_capture_envelope reports the WHOLE-snapshot max record end
    // (runtime_start_bridge_r14_diag_spanpub.cpp:767), so the shipped call
    // site cannot inflate it.
    {
        const auto shape = check_pool_shape(2, 0x10, gen.span_a_lo + 0x20000ull, pub, pp.published,
                                            full.back().end);
        report(!shape.ok && contains(shape.failure, "not covered"),
               "D1 pinned limit: inflated envelope + prefix table misreads truncation as unmapped (caller contract; bridge owns the true envelope)",
               shape.ok ? "WRONGLY ADMITTED" : shape.failure);
    }

    std::fprintf(g_out, "\n%d passed, %d failed\n", g_pass, g_fail);
    std::fclose(g_out);
    return g_fail == 0 ? 0 : 1;
}
