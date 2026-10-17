// test_preflight_host.cpp — worker-preflight-runner host fixture.
//
// ONE frozen synthetic snapshot (deterministic generator, no RNG) driven
// through the PRODUCTION chain:
//   capture/retention -> span publication -> owned-read registration
//   -> FName coverage/validator
// with every stage timed by clock_gettime(CLOCK_MONOTONIC).
//
// Proven live facts encoded in the snapshot (host-fixture evidence ONLY —
// this fixture proves code behavior, never game/console state):
//   - Main image: base kCandidateBase = 0x400000
//     (runtime_start_bridge_r14_diag_spanpub.cpp:220), runtime extent
//     [0x400000, 0xc0602c8) = base + kImageExtentVaddr 0xbc602c8
//     (spanpub:228; SeedValidation.hpp kImageFileExtentBytes), pinned 5-record
//     LOAD geometry with three large readable image mappings.
//   - Required heap mapping: the FName pool Blocks[0] span containing
//     0x22C0000000 — readable, 2 MiB >= kFNameBlockCapacityBytes (0x20000).
//     The heap span's BYTES stay explicitly UNKNOWN: the fixture never reads
//     and never fabricates pool contents.
//   - Live capture size reference: 10070 VMMAP entries walked to completion
//     (vm_total class: complete; 10070 <= kBridgeVmmapEntries).
//   - Capture capacities: kMaxVmMappings = 32768
//     (launch10-build/staging/live_module_facts.hpp:91),
//     kBridgeVmmapEntries = 32768 (spanpub:240), kMaxPublishedSpans = 64
//     (SeedValidation.hpp:143).
//
// Bulk stress: 80 non-image readable qualifying spans (>= 70 required), of
// which exactly 63 are strictly larger than the 2 MiB heap span and 16 are
// smaller (128 KiB .. 768 KiB, all at/above the 128 KiB publication floor).
// With the CURRENT policy (top-64-by-size INCLUDING image spans) the three
// readable image mappings are also larger than the heap span, so 66
// larger-than-heap candidates compete for 64 slots: the heap span is dropped
// and the in-game refusal class (cap saturated, required address uncovered)
// is reproduced. With the CORRECTED policy (image range excluded from
// candidacy) exactly 63 non-image spans beat the heap span: the heap span
// survives capture -> publication -> registration -> validator.
//
// Harness policy-agnostic: it only uses publish_readable_spans /
// check_pool_shape, whose signatures are stable across both policies.
// Mode argv[1] = "repro" (expects the refusal), "corrected" (expects
// admission with bytes still unknown), or "livegap" (exact-live-values
// regression: the listing-gap failure class + resolve_listing_gap evidence
// contract; compiled ONLY with -DRC_SEED_V5=1 against the corrected header,
// which owns failure_class / resolve_listing_gap). Exit 0 = the mode's
// expectation is demonstrated. Writes ALL output to files (stdout pipes wedge
// in this environment): argv[2] json, argv[3] stable (no timings;
// determinism diff), argv[4] receipt. argv[5] optional header sha256
// (metadata only).
//
// Freestanding: clang++ -std=c++20 -Wall -Wextra -I<dir-with-SeedValidation.hpp>

#include "SeedValidation.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---- pinned production constants (provenance in the file header) ----------
static constexpr uint64_t kCandidateBase = 0x400000ull;        // spanpub:220
static constexpr uint64_t kImageExtentVaddr = 0xbc602c8ull;    // spanpub:228
static constexpr uint64_t kRuntimeImageEnd = kCandidateBase + kImageExtentVaddr; // 0xc0602c8
static constexpr size_t kMaxVmMappings = 32768;   // staging/live_module_facts.hpp:91
static constexpr size_t kBridgeVmmapEntries = 32768; // spanpub:240
static constexpr size_t kWalkTotal = 10070;       // live reference: complete walk
static constexpr uint64_t kHeapBlocks0 = 0x22C0000000ull;      // required address
static constexpr uint64_t kHeapSpanSize = 0x200000ull;         // 2 MiB (>= 0x20000 floor)
static constexpr uint32_t kCurBlock = 2;          // fixture-class pool words (22k fixture)
static constexpr uint32_t kNoneId = 0x10;         // none_id>>16 = 0 <= 2

using namespace rc_seed;

// ---- timing -----------------------------------------------------------------
static timespec now_ts()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t;
}
static double elapsed_ms(const timespec& t0, const timespec& t1)
{
    return (static_cast<double>(t1.tv_sec - t0.tv_sec) * 1e3 +
            static_cast<double>(t1.tv_nsec - t0.tv_nsec) / 1e6);
}

// ---- output plumbing --------------------------------------------------------
static int g_pass = 0, g_fail = 0;
static FILE* g_receipt = nullptr;
static std::string g_stable; // canonical timing-free lines
static std::string g_json_checks; // json fragments appended per check

static auto report(bool ok, const std::string& name, const char* detail = "") -> void
{
    const char* d = detail != nullptr ? detail : "";
    if (ok) { ++g_pass; } else { ++g_fail; }
    std::fprintf(g_receipt, "[%s] %s%s%s\n", ok ? "PASS" : "FAIL", name.c_str(),
                 (d[0] != 0) ? ": " : "", d);
    g_stable += std::string(ok ? "PASS " : "FAIL ") + name + "\n";
    std::string esc;
    for (const char* p = d; *p != 0; ++p)
    {
        if (*p == '"' || *p == '\\') { esc += '\\'; }
        if (*p == '\n') { esc += ' '; continue; }
        esc += *p;
    }
    g_json_checks += std::string("{\"name\":\"") + name + "\",\"pass\":" +
                     (ok ? "true" : "false") + ",\"detail\":\"" + esc + "\"},\n";
}

static auto contains(const char* text, const char* needle) -> bool
{
    return text != nullptr && std::strstr(text, needle) != nullptr;
}

static auto hex64(uint64_t v) -> std::string
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%llx", static_cast<unsigned long long>(v));
    return std::string(buf);
}

// FNV-1a 64 over a byte range (determinism fingerprint).
static auto fnv1a(const void* data, size_t len) -> uint64_t
{
    const unsigned char* p = static_cast<const unsigned char*>(data);
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

// ---- the frozen synthetic snapshot ------------------------------------------
struct RawVmMapping // field parity with lmf::OsVmMapping start/end/protection
{
    uint64_t start;
    uint64_t end;
    uint32_t protection;
};

struct Snapshot
{
    std::vector<VmSpanRecord> records;
    size_t heap_index = 0;
    uint64_t heap_lo = 0, heap_hi = 0;
    uint64_t image_readable[3] = {0, 0, 0}; // [L1, L3, L7] runtime spans
    size_t nonimage_qualifying = 0;
    size_t nonimage_larger_than_heap = 0;
};

// Deterministic, address-ascending, non-overlapping. Pinned special indices;
// filler pads the rest. The giant prot-0 reservation before the heap span is
// what brings the cursor exactly to 0x22C0000000 without ever qualifying.
static auto generate_snapshot() -> Snapshot
{
    Snapshot s;
    auto& w = s.records;
    w.reserve(kWalkTotal);

    // records 0-4: pinned main-image geometry inside [0x400000, 0xc0602c8).
    // LOAD0 --X, LOAD1 R--, LOAD3 RW-, LOAD7 RW-, LOAD8 --- (to extent end).
    w.push_back({0x400000, 0x787c000, 4});
    w.push_back({0x787c000, 0x82bc000, 1});
    s.image_readable[0] = 0x787c000;
    w.push_back({0x82bc000, 0x953c000, 3});
    s.image_readable[1] = 0x82bc000;
    w.push_back({0x953c000, 0x9a34000, 3});
    s.image_readable[2] = 0x953c000;
    w.push_back({0x9a34000, kRuntimeImageEnd, 0});

    uint64_t cursor = 0x10000000ull; // strictly above the runtime image end
    size_t filler_i = 0;
    auto filler = [&w, &cursor, &filler_i]() {
        // Realistic mix: mostly RW, some prot-none reservations, some --X.
        uint32_t prot = 3;
        if (filler_i % 9u == 4u) { prot = 0; }
        else if (filler_i % 13u == 7u) { prot = 4; }
        w.push_back({cursor, cursor + 0x8000, prot}); // 32 KiB: sub-floor
        cursor += 0x120000ull;
        ++filler_i;
    };
    // 63 spans strictly larger than the 2 MiB heap span: distinct pattern
    // {4,8,16,32,64,128,256,512} MiB (all > 2 MiB).
    static const uint64_t kBulkMiB[8] = {4, 8, 16, 32, 64, 128, 256, 512};
    auto bulk_large = [&w, &cursor](size_t i) {
        const uint64_t size = kBulkMiB[i % 8] * 0x100000ull;
        w.push_back({cursor, cursor + size, 3});
        cursor += size + 0x120000ull;
    };
    // 16 qualifying spans strictly SMALLER than the heap span (>= 128 KiB floor).
    static const uint64_t kSmallKiB[16] = {128, 160, 192, 224, 256, 288, 320,
                                           352, 384, 416, 448, 480, 512, 576,
                                           640, 768};
    auto small_span = [&w, &cursor](size_t i) {
        const uint64_t size = kSmallKiB[i] * 1024ull;
        w.push_back({cursor, cursor + size, 3});
        cursor += size + 0x120000ull;
    };

    // bulk-large #0..54 at pinned indices 100,102,...,208 (pre-heap).
    for (size_t i = 0; i < 55; ++i)
    {
        while (w.size() < 100 + 2 * i) { filler(); }
        bulk_large(i);
    }
    // small spans at pinned indices 300,302,...,330.
    for (size_t i = 0; i < 16; ++i)
    {
        while (w.size() < 300 + 2 * i) { filler(); }
        small_span(i);
    }
    // fillers up to index 4999, then the approach reservation: one giant
    // prot-0 record ending EXACTLY at the required heap span base (never
    // qualifies: no READ bit). It must be the record immediately before the
    // heap record so the cursor never marches past 0x22c0000000.
    while (w.size() < 4999) { filler(); }
    w.push_back({cursor, kHeapBlocks0, 0});
    cursor = kHeapBlocks0;

    // heap record pinned at index 5000.
    s.heap_index = w.size();
    s.heap_lo = kHeapBlocks0;
    s.heap_hi = kHeapBlocks0 + kHeapSpanSize;
    w.push_back({s.heap_lo, s.heap_hi, 3}); // readable, 2 MiB
    cursor = s.heap_hi + 0x120000ull;

    // bulk-large #55..62 at pinned indices 6000,6002,...,6014 (post-heap).
    for (size_t i = 55; i < 63; ++i)
    {
        while (w.size() < 6000 + 2 * (i - 55)) { filler(); }
        bulk_large(i);
    }
    while (w.size() < kWalkTotal) { filler(); }

    // facts for expectations
    for (size_t i = 0; i < w.size(); ++i)
    {
        const bool readable = (w[i].protection & kVmProtRead) != 0;
        const bool image = w[i].start < kRuntimeImageEnd; // starts inside image extent
        const uint64_t size = w[i].end - w[i].start;
        if (readable && !image && size >= kFNameBlockCapacityBytes)
        {
            ++s.nonimage_qualifying;
            if (size > kHeapSpanSize) { ++s.nonimage_larger_than_heap; }
        }
    }
    return s;
}

// ---- production-chain mirrors -------------------------------------------------
static RawVmMapping g_lmf_capture[kMaxVmMappings];       // lmf retention buffer
static VmSpanRecord g_spanpub_records[kBridgeVmmapEntries]; // bridge frozen snapshot
static AddressSpan g_published[kMaxPublishedSpans];
static AddressSpan g_registry[kMaxPublishedSpans];

static size_t g_registry_count = 0;
static auto register_owned_span(const AddressSpan& s) -> bool
{
    if (g_registry_count >= kMaxPublishedSpans) { return false; }
    g_registry[g_registry_count] = s;
    ++g_registry_count;
    return true;
}
static auto owned_span_containing(uint64_t addr, uint64_t len) -> const AddressSpan*
{
    for (size_t i = 0; i < g_registry_count; ++i)
    {
        if (addr >= g_registry[i].lo && addr + len <= g_registry[i].hi)
        {
            return &g_registry[i];
        }
    }
    return nullptr;
}
static auto published_contains(const AddressSpan* pub, size_t n, const AddressSpan& s,
                               size_t* index_out) -> bool
{
    for (size_t i = 0; i < n; ++i)
    {
        if (pub[i].lo == s.lo && pub[i].hi == s.hi)
        {
            if (index_out != nullptr) { *index_out = i; }
            return true;
        }
    }
    return false;
}

int main(int argc, char** argv)
{
    if (argc < 5)
    {
        std::fprintf(stderr,
                     "usage: %s <repro|corrected> <out.json> <out.stable> <out.receipt> [header_sha]\n",
                     argv[0]);
        return 3;
    }
    const std::string mode = argv[1];
    const bool repro_mode = (mode == "repro");
    const bool livegap_mode = (mode == "livegap");
    if (!repro_mode && mode != "corrected" && !livegap_mode) { return 3; }
    if (livegap_mode &&
#ifdef RC_SEED_V5
        false
#else
        true
#endif
    ) { return 3; } // livegap requires the corrected header (RC_SEED_V5 build)
    const std::string header_sha = argc > 5 ? argv[5] : "unrecorded";

    g_receipt = std::fopen(argv[4], "w");
    if (g_receipt == nullptr) { return 3; }

    const timespec t_program0 = now_ts();
    std::fprintf(g_receipt, "preflight mode=%s header_sha256=%s\n", mode.c_str(),
                 header_sha.c_str());

    // ---- stage: snapshot_build ------------------------------------------------
    timespec t0 = now_ts();
    const Snapshot snap = generate_snapshot();
    const std::vector<VmSpanRecord>& full = snap.records;
    uint64_t snapshot_hash = fnv1a(full.data(), full.size() * sizeof(VmSpanRecord));
    // pinned-fact checks (policy-agnostic)
    report(full.size() == kWalkTotal, "C03 snapshot built with exactly 10070 records",
           ("size=" + std::to_string(full.size())).c_str());
    bool ascending = true;
    size_t bad_idx = 0;
    for (size_t i = 1; i < full.size(); ++i)
    {
        if (full[i].start < full[i - 1].end)
        {
            ascending = false;
            bad_idx = i;
            break;
        }
    }
    if (!ascending)
    {
        char dbg[256];
        std::snprintf(dbg, sizeof(dbg), "first violation at index %zu: prev=[%llx,%llx) cur=[%llx,%llx)",
                      bad_idx,
                      static_cast<unsigned long long>(full[bad_idx - 1].start),
                      static_cast<unsigned long long>(full[bad_idx - 1].end),
                      static_cast<unsigned long long>(full[bad_idx].start),
                      static_cast<unsigned long long>(full[bad_idx].end));
        report(ascending, "C04 snapshot address-ascending and non-overlapping", dbg);
    }
    else
    {
        report(ascending, "C04 snapshot address-ascending and non-overlapping", "");
    }
    {
        const VmSpanRecord& h = full[snap.heap_index];
        const bool heap_ok = snap.heap_index == 5000 && h.start == kHeapBlocks0 &&
                             h.end == kHeapBlocks0 + kHeapSpanSize &&
                             (h.protection & kVmProtRead) != 0 &&
                             (h.end - h.start) >= kFNameBlockCapacityBytes;
        report(heap_ok,
               "C05 required heap span pinned: index 5000, [0x22c0000000,0x22c0200000) readable 2MiB >= block floor",
           heap_ok ? "" : "heap record drifted");
    }
    {
        size_t image_records = 0, image_readable_count = 0;
        for (size_t i = 0; i < 5; ++i)
        {
            if (full[i].start >= kCandidateBase && full[i].end <= kRuntimeImageEnd)
            {
                ++image_records;
            }
            if ((full[i].protection & kVmProtRead) != 0) { ++image_readable_count; }
        }
        report(image_records == 5 && image_readable_count == 3,
               "C06 image encoded: 5 records inside [0x400000,0xc0602c8), 3 large readable",
               ("image_records=" + std::to_string(image_records) +
                " readable=" + std::to_string(image_readable_count))
                   .c_str());
    }
    report(snap.nonimage_qualifying >= 70,
           "C07 non-image readable qualifying spans >= 70",
           ("count=" + std::to_string(snap.nonimage_qualifying)).c_str());
    report(snap.nonimage_larger_than_heap == 63,
           "C08 non-image qualifying spans strictly larger than heap span == 63 (< 64)",
           ("count=" + std::to_string(snap.nonimage_larger_than_heap)).c_str());
    report((kHeapBlocks0 & 0x7ull) == 0 && kHeapBlocks0 >= snap.heap_lo &&
               kHeapBlocks0 < snap.heap_hi,
           "C09 blocks0 pinned 0x22c0000000 (8-aligned, inside the heap span)", "");
    report(kMaxPublishedSpans == 64 && kFNameBlockCapacityBytes == 0x20000ull &&
               kImageFileExtentBytes == 0xbc602c8ull && kVmProtRead == 1u,
           "C01 header constants pinned (cap 64 / floor 0x20000 / extent 0xbc602c8 / PROT_READ 1)",
           "");
    report(kCandidateBase == 0x400000ull && kMaxVmMappings == 32768 &&
               kBridgeVmmapEntries == 32768 && kWalkTotal == 10070,
           "C02 production constants pinned (base 0x400000 / 32768 / 32768 / 10070)", "");
    const double ms_build = elapsed_ms(t0, now_ts());

    // ---- stage: capture_walk (production retention mirror) --------------------
    t0 = now_ts();
    size_t walked = 0;
    size_t stored = 0;
    uint32_t stop_reason = 0; // mirror bridge_vmmap_walk contract: 0 = clean end
    for (size_t i = 0; i < full.size(); ++i)
    {
        ++walked; // counts past the capacity like the bridge walker
        if (walked <= kMaxVmMappings)
        {
            g_lmf_capture[stored].start = full[i].start;
            g_lmf_capture[stored].end = full[i].end;
            g_lmf_capture[stored].protection = full[i].protection;
            ++stored;
        }
    }
    const bool complete = walked <= kMaxVmMappings && walked <= kBridgeVmmapEntries;
    if (!complete) { stop_reason = 2; } // would be the honest truncation class
    report(walked == kWalkTotal && stop_reason == 0,
           "C10 capture walk complete: 10070 entries, stop=0 (clean end)",
           ("walked=" + std::to_string(walked)).c_str());
    report(stored == kWalkTotal && complete,
           "C11 retention buffers hold all 10070 records (10070 <= 32768 lmf / 32768 bridge)",
           ("stored=" + std::to_string(stored)).c_str());
    // bridge_spanpub_publish copy loop: WHOLE frozen snapshot into the
    // rc_seed record array (same-capture rule; non-readable records kept for
    // enumeration coverage).
    const size_t bounded = walked < kBridgeVmmapEntries ? walked : kBridgeVmmapEntries;
    for (size_t i = 0; i < bounded; ++i)
    {
        g_spanpub_records[i].start = g_lmf_capture[i].start;
        g_spanpub_records[i].end = g_lmf_capture[i].end;
        g_spanpub_records[i].protection = g_lmf_capture[i].protection;
    }
    const double ms_walk = elapsed_ms(t0, now_ts());

    // ---- stage: publication ---------------------------------------------------
    t0 = now_ts();
    const PublicationResult pub = publish_readable_spans(
        g_spanpub_records, bounded, kCandidateBase, kImageFileExtentBytes,
        g_published, kMaxPublishedSpans);
    const uint64_t published_hash = fnv1a(g_published, pub.published * sizeof(AddressSpan));
    size_t heap_pub_index = 0;
    const bool heap_published =
        published_contains(g_published, pub.published, AddressSpan{snap.heap_lo, snap.heap_hi},
                           &heap_pub_index);
    const bool img0 = published_contains(g_published, pub.published,
                                         AddressSpan{snap.image_readable[0], 0x82bc000}, nullptr);
    const bool img1 = published_contains(g_published, pub.published,
                                         AddressSpan{snap.image_readable[1], 0x953c000}, nullptr);
    const bool img2 = published_contains(g_published, pub.published,
                                         AddressSpan{snap.image_readable[2], 0x9a34000}, nullptr);
    uint64_t max_end = 0;
    for (size_t i = 0; i < bounded; ++i)
    {
        if (g_spanpub_records[i].end > max_end) { max_end = g_spanpub_records[i].end; }
    }
    report(pub.envelope_end == max_end && max_end == full.back().end,
           "C12 envelope_end == max record end == last record end (complete capture)",
           ("envelope=" + hex64(pub.envelope_end)).c_str());
    const double ms_publish = elapsed_ms(t0, now_ts());

    // ---- stage: owned-read registration ---------------------------------------
    t0 = now_ts();
    g_registry_count = 0;
    bool reg_all = true;
    for (size_t i = 0; i < pub.published; ++i)
    {
        if (!register_owned_span(g_published[i])) { reg_all = false; }
    }
    bool reg_matches = reg_all && g_registry_count == pub.published;
    for (size_t i = 0; i < g_registry_count && reg_matches; ++i)
    {
        if (g_registry[i].lo != g_published[i].lo || g_registry[i].hi != g_published[i].hi)
        {
            reg_matches = false;
        }
    }
    report(reg_matches, "C13 owned-read registry registered every published span, 1:1",
           ("registered=" + std::to_string(g_registry_count)).c_str());
    const AddressSpan* owned = owned_span_containing(kHeapBlocks0, 8);
    const bool heap_owned = owned != nullptr;
    const double ms_register = elapsed_ms(t0, now_ts());

    // ---- stage: coverage_validator --------------------------------------------
    t0 = now_ts();
    const PoolShapeCheck shape = check_pool_shape(kCurBlock, kNoneId, kHeapBlocks0,
                                                  g_registry, g_registry_count,
                                                  pub.envelope_end);
    const double ms_validate = elapsed_ms(t0, now_ts());

    // ---- expectations ----------------------------------------------------------
    if (repro_mode)
    {
        report(pub.cap_saturated == 1 && pub.total == 83 && pub.published == 64,
               "R1 publication cap saturated: 83 qualifying, top-64 window full",
               ("published=" + std::to_string(pub.published) +
                " total=" + std::to_string(pub.total) +
                " cap=" + std::to_string(pub.cap_saturated))
                   .c_str());
        report(!heap_published,
               "R2 required heap span [0x22c0000000,0x22c0200000) ABSENT from the published set",
               heap_published ? "heap span published" : "");
        report(img0 && img1 && img2,
               "R3 image spans are publication candidates (L1+L3+L7 all published under the current policy)",
               "image presence drifted");
        report(!shape.ok && shape.bounds_source != nullptr &&
                   std::string(shape.bounds_source) == "published-readable-spans" &&
                   contains(shape.failure, "not covered by any published readable span"),
               "R4 validator refuses: Blocks[0] not covered by any published readable span",
               shape.ok ? "WRONGLY ADMITTED" : (shape.failure != nullptr ? shape.failure : ""));
        report(kHeapBlocks0 < pub.envelope_end && !contains(shape.failure, "truncated"),
               "R5 refusal is the in-game uncovered class, NOT the truncation class (blocks0 below envelope)",
               "");
        report(!heap_owned,
               "R6 owned-read registration REFUSES reads at 0x22c0000000 (not ownership-covered)",
               heap_owned ? "coverage leaked" : "");
    }
#ifdef RC_SEED_V5
    else if (livegap_mode)
    {
        // Exact-live-values programmatic regression (coordinator directive,
        // user arithmetic directive). ALL comparisons computed in code, never
        // hand-decoded. These pin the LIVE refusal class: the kernel listing
        // itself returned NO record covering candidate Blocks[0] — the walk
        // was COMPLETE (10070 entries, stop=0, capacity 32768), the envelope
        // 0x1100400000 (68 GiB) covers every decoded record, and blocks0
        // 0x22C0000000 (139 GiB) is ABOVE it. Above-envelope miss == listing
        // gap; publication policy is irrelevant there. In this mode the
        // published table from the same frozen snapshot is used as the table
        // argument to prove the CLASS does not depend on table contents.
        const uint64_t envelope_live = (0x11ull << 32) | 0x00400000ull; // 0x1100400000
        const uint64_t blocks0_live = 0x22C0000000ull;
        const uint32_t spanpub_total_live = 0x7eu;     // 126
        const uint32_t spanpub_published_live = 64u;   // observed live window
        report(envelope_live == 0x1100400000ull && blocks0_live == 0x22C0000000ull &&
                   spanpub_total_live == 126u && spanpub_published_live == 64u,
               "L1 live constants computed in code: envelope 0x1100400000 / blocks0 0x22c0000000 / total 126 (0x7e) / published 64",
               "");
        report(blocks0_live >= envelope_live,
               "L2 blocks0 (139 GiB) >= envelope (68 GiB) holds: the listing-gap precondition",
               "");
        // The class decision depends on (blocks0, envelope, table coverage):
        // (i)  with the corrected published table of THIS snapshot, blocks0
        //      IS covered -> admits (the enumerated case, fixed by v5);
        // (ii) with a table that does NOT cover blocks0 — the LIVE case: the
        //      listing returned NO record covering it — the same pure
        //      function refuses with the CaptureTruncated class (listing gap;
        //      publication policy is irrelevant there).
        const AddressSpan dummy_table[1] = {AddressSpan{0x1000, 0x2000}};
        const PoolShapeCheck shapeA = check_pool_shape(kCurBlock, kNoneId, blocks0_live,
                                                       g_registry, g_registry_count,
                                                       envelope_live);
        const PoolShapeCheck shapeGap = check_pool_shape(kCurBlock, kNoneId, blocks0_live,
                                                         dummy_table, 1, envelope_live);
        report(shapeA.ok && !shapeGap.ok &&
                   shapeGap.failure_class == PoolShapeCheck::FailureClass::CaptureTruncated &&
                   shapeGap.bounds_source != nullptr &&
                   std::string(shapeGap.bounds_source) == "capture-truncated",
               "L3 live refusal class reproduced: listed coverage admits, listing-gap coverage refuses CaptureTruncated (machine failure_class)",
               shapeGap.ok ? "gap WRONGLY ADMITTED"
                           : (shapeGap.failure != nullptr ? shapeGap.failure : ""));
        // resolve_listing_gap evidence contract (v5), driven from the GAP-class
        // shape (the live case; shapeA was already admitted by membership):
        const auto gap_mapped = resolve_listing_gap(shapeGap, RangeMappedVerdict::Mapped,
                                                    ReadableCoverageSource::None);
        report(!gap_mapped.ok && gap_mapped.note != nullptr &&
                   std::strstr(gap_mapped.note, "UNPROVEN") != nullptr,
               "L4 mapped-but-unreadable refuses: Mapped verdict NEVER authorizes a read alone (mapped!=readable; exec-only text exists)",
               gap_mapped.ok ? "WRONGLY ADMITTED" : gap_mapped.note);
        const auto gap_notmapped = resolve_listing_gap(shapeGap, RangeMappedVerdict::NotMapped,
                                                       ReadableCoverageSource::None);
        report(!gap_notmapped.ok && gap_notmapped.note != nullptr &&
                   std::strstr(gap_notmapped.note, "genuine unmapped proof") != nullptr,
               "L5 NotMapped refuses with the stronger genuine-unmapped note",
               gap_notmapped.ok ? "WRONGLY ADMITTED" : gap_notmapped.note);
        // Structural failures are never rescuable — even with both a mapped
        // verdict and a listed span offered.
        const PoolShapeCheck shape_struct = check_pool_shape(kCurBlock, kNoneId, 0,
                                                             g_registry, g_registry_count,
                                                             envelope_live);
        const auto gap_struct = resolve_listing_gap(shape_struct, RangeMappedVerdict::Mapped,
                                                    ReadableCoverageSource::ListedSpan);
        report(shape_struct.failure_class == PoolShapeCheck::FailureClass::Structural &&
                   !gap_struct.ok && gap_struct.note != nullptr &&
                   std::strstr(gap_struct.note, "structural checks gate everything") != nullptr,
               "L6 structural failure (Blocks[0] null) never rescued by any verdict/source",
               gap_struct.ok ? "WRONGLY ADMITTED" : gap_struct.note);
        // Pure code-path contrast (NOT a live claim): the header's contract is
        // that a listed READ bit plus a mapped range verdict admits. For a
        // LISTING-ABSENT address the ListedSpan source is unreachable by
        // construction — this only pins the helper's pure behavior.
        const auto gap_listed = resolve_listing_gap(shapeGap, RangeMappedVerdict::Mapped,
                                                    ReadableCoverageSource::ListedSpan);
        report(gap_listed.ok && gap_listed.bounds_source != nullptr &&
                   std::string(gap_listed.bounds_source) == "listed-span-plus-range-verdict",
               "L7 pure-path contrast: ListedSpan+Mapped admits (listed-span-plus-range-verdict) — unreachable for listing-absent addresses by construction",
               gap_listed.ok ? "" : (gap_listed.note != nullptr ? gap_listed.note : ""));
        report(pub.envelope_end != envelope_live,
               "L8 scope note: this snapshot's envelope is the synthetic walk end; the LIVE gap case is pinned by L1-L6 constants, not by this snapshot",
               "");
    }
#endif

    else
    {
        report(pub.cap_saturated == 1 && pub.total == 80,
               "K1 publication cap still saturated honestly: 80 non-image qualifying > 64",
               ("published=" + std::to_string(pub.published) +
                " total=" + std::to_string(pub.total) +
                " cap=" + std::to_string(pub.cap_saturated))
                   .c_str());
        report(heap_published,
               "K2 required heap span [0x22c0000000,0x22c0200000) SURVIVES publication",
               ("heap_pub_index=" + std::to_string(heap_pub_index)).c_str());
        report(!img0 && !img1 && !img2,
               "K3 image range excluded from publication candidates (L1+L3+L7 all absent)",
               "image span leaked into the table");
        report(heap_owned,
               "K4 owned-read registration COVERS reads at 0x22c0000000 (ownership only)",
               "");
        report(shape.ok && shape.bounds_source != nullptr &&
                   std::string(shape.bounds_source) == "published-readable-spans",
               "K5 validator admits: Blocks[0] inside exactly one published readable span",
               shape.ok ? "" : (shape.failure != nullptr ? shape.failure : ""));
        report(!contains(shape.failure != nullptr ? shape.failure : "", "None") &&
                   kHeapBlocks0 < pub.envelope_end,
               "K6 heap BYTES remain unknown: membership coverage only, no fabricated content check",
               "pool bytes never read by this fixture");
        report(pub.envelope_end == full.back().end,
               "K7 envelope still spans the WHOLE snapshot (image records keep enumeration coverage)",
               ("envelope=" + hex64(pub.envelope_end)).c_str());
    }

    const bool expectation_met = (g_fail == 0);
    const char* verdict_str =
        repro_mode ? (expectation_met ? "REPRODUCED" : "NOT-REPRODUCED") :
        livegap_mode ? (expectation_met ? "LIVE-GAP-REPRODUCED" : "LIVE-GAP-NOT-REPRODUCED") :
                       (expectation_met ? "CORRECTED-PASS" : "CORRECTED-FAIL");
    const double ms_total = elapsed_ms(t_program0, now_ts());

    // ---- stable (timing-free) output: determinism diff input -------------------
    std::string stable;
    stable += "mode=" + mode + "\n";
    stable += "header_sha256=" + header_sha + "\n";
    stable += "snapshot_records=" + std::to_string(full.size()) + "\n";
    stable += "snapshot_fnv1a=" + hex64(snapshot_hash) + "\n";
    stable += "published_count=" + std::to_string(pub.published) + "\n";
    stable += "published_total=" + std::to_string(pub.total) + "\n";
    stable += "cap_saturated=" + std::to_string(pub.cap_saturated) + "\n";
    stable += "envelope_end=" + hex64(pub.envelope_end) + "\n";
    stable += "published_fnv1a=" + hex64(published_hash) + "\n";
    stable += "heap_published=" + std::string(heap_published ? "1" : "0") + "\n";
    stable += "heap_pub_index=" + std::to_string(heap_pub_index) + "\n";
    stable += "image_published_L1=" + std::string(img0 ? "1" : "0") + "\n";
    stable += "image_published_L3=" + std::string(img1 ? "1" : "0") + "\n";
    stable += "image_published_L7=" + std::string(img2 ? "1" : "0") + "\n";
    stable += "registry_count=" + std::to_string(g_registry_count) + "\n";
    stable += "blocks0_owned=" + std::string(heap_owned ? "1" : "0") + "\n";
    stable += "heap_bytes_status=unknown (fixture never reads or fabricates pool bytes)\n";
    stable += "validator_ok=" + std::string(shape.ok ? "1" : "0") + "\n";
    stable += "validator_bounds_source=" +
              std::string(shape.bounds_source != nullptr ? shape.bounds_source : "null") + "\n";
    stable += "validator_failure=" +
              std::string(shape.failure != nullptr ? shape.failure : "null") + "\n";
    stable += "verdict=" + std::string(verdict_str) + "\n";
    stable += g_stable;
    FILE* sf = std::fopen(argv[3], "w");
    if (sf != nullptr) { std::fputs(stable.c_str(), sf); std::fclose(sf); }

    // ---- json output ------------------------------------------------------------
    // strip trailing comma from the checks fragment
    std::string checks = g_json_checks;
    if (checks.size() >= 2) { checks.resize(checks.size() - 2); checks += "\n"; }
    FILE* jf = std::fopen(argv[2], "w");
    if (jf != nullptr)
    {
        std::fprintf(jf,
            "{\n"
            "  \"worker\": \"worker-preflight-runner\",\n"
            "  \"mode\": \"%s\",\n"
            "  \"policy\": \"%s\",\n"
            "  \"policy_source\": \"%s\",\n"
            "  \"header_sha256\": \"%s\",\n"
            "  \"snapshot\": {\n"
            "    \"records\": %zu,\n"
            "    \"walk_total\": %zu,\n"
            "    \"stop_reason\": %u,\n"
            "    \"complete\": %s,\n"
            "    \"nonimage_qualifying\": %zu,\n"
            "    \"nonimage_larger_than_heap\": %zu,\n"
            "    \"heap_span\": [\"0x22c0000000\", \"0x22c0200000\"],\n"
            "    \"blocks0\": \"0x22c0000000\",\n"
            "    \"heap_bytes_status\": \"unknown (never read; fixture proves code behavior, not pool contents)\",\n"
            "    \"fnv1a\": \"%s\"\n"
            "  },\n"
            "  \"publication\": {\n"
            "    \"published\": %u,\n"
            "    \"total\": %u,\n"
            "    \"cap_saturated\": %u,\n"
            "    \"envelope_end\": \"%s\",\n"
            "    \"heap_span_published\": %s,\n"
            "    \"heap_pub_index\": %zu,\n"
            "    \"image_L1_published\": %s,\n"
            "    \"image_L3_published\": %s,\n"
            "    \"image_L7_published\": %s,\n"
            "    \"fnv1a\": \"%s\"\n"
            "  },\n"
            "  \"owned_read_registration\": {\n"
            "    \"registered\": %zu,\n"
            "    \"blocks0_covered\": %s\n"
            "  },\n"
            "  \"validator\": {\n"
            "    \"ok\": %s,\n"
            "    \"failure\": \"%s\",\n"
            "    \"bounds_source\": \"%s\"\n"
            "  },\n"
            "  \"stage_elapsed_ms\": {\n"
            "    \"snapshot_build\": %.3f,\n"
            "    \"capture_walk\": %.3f,\n"
            "    \"publication\": %.3f,\n"
            "    \"owned_read_registration\": %.3f,\n"
            "    \"coverage_validator\": %.3f,\n"
            "    \"total_program\": %.3f\n"
            "  },\n"
            "  \"verdict\": \"%s\",\n"
            "  \"expectation_met\": %s,\n"
            "  \"checks\": [\n%s  ]\n"
            "}\n",
            mode.c_str(),
            repro_mode ? "production-top64-by-size-including-image" : "corrected-image-excluded",
            repro_mode ? "final-units/SeedValidation.hpp (frozen staging copy)"
                       : "corrected SeedValidation.hpp (controller edit, sha recorded below)",
            header_sha.c_str(),
            full.size(), walked, stop_reason, complete ? "true" : "false",
            snap.nonimage_qualifying, snap.nonimage_larger_than_heap,
            hex64(snapshot_hash).c_str(),
            pub.published, pub.total, pub.cap_saturated,
            hex64(pub.envelope_end).c_str(),
            heap_published ? "true" : "false", heap_pub_index,
            img0 ? "true" : "false", img1 ? "true" : "false", img2 ? "true" : "false",
            hex64(published_hash).c_str(),
            g_registry_count, heap_owned ? "true" : "false",
            shape.ok ? "true" : "false",
            shape.failure != nullptr ? shape.failure : "null",
            shape.bounds_source != nullptr ? shape.bounds_source : "null",
            ms_build, ms_walk, ms_publish, ms_register, ms_validate, ms_total,
            verdict_str,
            expectation_met ? "true" : "false",
            checks.c_str());
        std::fclose(jf);
    }

    std::fprintf(g_receipt, "\nverdict=%s (%d passed, %d failed)\n", verdict_str,
                 g_pass, g_fail);
    std::fprintf(g_receipt,
                 "stage_ms: build=%.3f walk=%.3f publish=%.3f register=%.3f validate=%.3f total=%.3f\n",
                 ms_build, ms_walk, ms_publish, ms_register, ms_validate, ms_total);
    std::fclose(g_receipt);
    return expectation_met ? 0 : 1;
}
