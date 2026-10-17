// pool_gap_test.cpp — host test of the initializer's listing-gap resolution
// and bytes-fed validation with the kernel services (worker-ue4ss-kernel,
// 2026-10-06).
//
// The initializer TU itself needs the full UE4SS header tree (cannot compile
// on a host), so this test REPLICATES the patched decision order of diff
// 0001 (hunks 6-10: kernel capture -> bridge capture -> fresh kernel
// safe_read -> copyin probe, then capture-fed anchor/entry validation)
// against the REAL pure validation chain (SeedValidation.hpp — deliberately
// freestanding per its header) and the REAL facade (KekcallServices.hpp)
// over the mock kernel (mock_kekcall.hpp). The replica order is kept in
// lockstep with the diff; drift is a review item.
//
// Cases:
//   A  ModuleNew + DM-class Blocks[0] (the live 0x22C0000000 class): the
//      copyin oracle REFUSES (source-class allowlist) but safe_read
//      succeeds — pool validation must admit via the KERNEL with
//      capture-fed 'None' anchor and entry matches. THE core claim.
//   B  ModuleOld (ENOSYS): full fallback to the copyin oracle; admission
//      via sink capture, byte-for-byte the v5.2/v5.3 semantics.
//   C  ModuleNew + PROT_NONE Blocks[0]: kernel REFUSED — admission refused
//      (fail-closed; the copyin oracle is not spent after a kernel refusal).
//   D  Seed-read guards (hunks 3-5, 7, 9): readable seed addresses proceed;
//      a PROT_NONE seed address refuses; guards no-op to proceed when the
//      kernel service is absent (ModuleOld after latch).
//   E  Bridge Control-D replica (diff 0002): kernel route runs independent
//      of the sink/write route; legacy copyin Control D only on ModuleOld.

#include "mock_kekcall.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// The REAL pure validation chain (freestanding by design).
#include "../../../2026-10-04-zcode-controller/launch11-combined/units/SeedValidation.hpp"

static int failures = 0;
#define CHECK(cond, msg)                                                        \
    do                                                                          \
    {                                                                           \
        if (!(cond))                                                            \
        {                                                                       \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, msg);            \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

// ---------------------------------------------------------------------------
// Test-local kernel capture store + gap helpers — replica of diff 0001
// (patched UnrealInitializer.cpp, SeedArming::kernel_* helpers).
// ---------------------------------------------------------------------------
namespace replica
{
    struct KernelCapture
    {
        uint8_t bytes[3][0x1000];
        uint64_t addr[3];
        uint32_t len[3];
        bool valid[3];
        uint32_t next = 0;

        void register_capture(uint64_t a, const uint8_t *b, uint32_t l)
        {
            if (l > 0x1000) { l = 0x1000; }
            std::memcpy(bytes[next], b, l);
            addr[next] = a;
            len[next] = l;
            valid[next] = true;
            next = (next + 1u) % 3u;
        }
        int lookup(uint64_t a, const uint8_t **out, uint32_t *out_len) const
        {
            for (uint32_t i = 0; i < 3; ++i)
            {
                if (valid[i] && addr[i] == a)
                {
                    *out = bytes[i];
                    *out_len = len[i];
                    return 1;
                }
            }
            return 0;
        }
        void reset() { std::memset(this, 0, sizeof *this); }
    };
    inline KernelCapture g_kernel_capture;

    struct KernelGapResult { ksrv::GapClass cls; int64_t rc; };

    // Replica of SeedArming::kernel_services_ready + kernel_gap_probe: full
    // required range through chunked safe_read; first page captured.
    inline KernelGapResult kernel_gap_probe(uint64_t addr, uint64_t len)
    {
        KernelGapResult r{ksrv::GapClass::Absent, 0};
        if (!ksrv::maybe_available()) { return r; }
        std::vector<uint8_t> bytes(static_cast<size_t>(len));
        r.rc = ksrv::safe_read(addr, bytes.data(), len);
        r.cls = ksrv::gap_class_from_read_rc(r.rc);
        if (r.cls == ksrv::GapClass::Readable)
        {
            g_kernel_capture.register_capture(addr, bytes.data(),
                                              static_cast<uint32_t>(len < 0x1000 ? len : 0x1000));
        }
        return r;
    }

    // Replica of SeedArming::kernel_listing_gap_resolution (fields only;
    // notes abbreviated — the diff carries the full text).
    inline rc_seed::ListingGapResolution kernel_listing_gap_resolution(const KernelGapResult &kr)
    {
        if (kr.cls == ksrv::GapClass::Readable)
        {
            return {true, "kernel-safe-read", "kernel safe-read transferred the full range"};
        }
        if (kr.cls == ksrv::GapClass::Refused)
        {
            return {false, nullptr, "kernel safe-read REFUSED (page-table verdict); no unmapped claim"};
        }
        if (kr.cls == ksrv::GapClass::Unavailable)
        {
            return {false, nullptr, "kernel safe-read unavailable (oracle failure)"};
        }
        return {false, nullptr, "kernel service absent"};
    }

    // Replica of the patched listing-gap block (diff 0001 hunk 6): the
    // kernel-first oracle order with the once-per-boot law preserved.
    struct GapOutcome
    {
        bool admitted = false;
        bool via_kernel = false;
        bool gap_probed = false;
        const char *bounds_source = nullptr;
        const char *note = nullptr;
    };

    inline GapOutcome resolve_listing_gap_order(const rc_seed::PoolShapeCheck &shape,
                                                uint64_t blocks0, uint64_t capacity)
    {
        GapOutcome out;
        if (shape.ok)
        {
            out.admitted = true; // span membership carried it (not the gap path)
            out.bounds_source = shape.bounds_source;
            return out;
        }
        if (!out.gap_probed) { /* first rescuable miss this boot */ }
        bool resolved_by_capture = false;
        const uint8_t *cap = nullptr;
        uint32_t cap_len = 0;
        if (g_kernel_capture.lookup(blocks0, &cap, &cap_len) == 1 && cap != nullptr)
        {
            const auto res = rc_seed::resolve_listing_gap(shape, rc_seed::CopyinProbeClass::Readable);
            out.gap_probed = true;
            out.note = res.note;
            out.admitted = res.ok;
            out.via_kernel = res.ok;
            out.bounds_source = res.bounds_source;
            resolved_by_capture = true;
        }
        else if (mock::bridge_copyin_capture(blocks0, &cap, &cap_len) == 1 && cap != nullptr)
        {
            const auto res = rc_seed::resolve_listing_gap(shape, rc_seed::CopyinProbeClass::Readable);
            out.gap_probed = true;
            out.note = res.note;
            out.admitted = res.ok;
            out.via_kernel = false;
            out.bounds_source = res.bounds_source;
            resolved_by_capture = true;
        }
        if (!resolved_by_capture)
        {
            const auto kr = kernel_gap_probe(blocks0, capacity);
            if (kr.cls != ksrv::GapClass::Absent)
            {
                const auto res = kernel_listing_gap_resolution(kr);
                out.gap_probed = true;
                out.note = res.note;
                out.admitted = res.ok;
                out.via_kernel = res.ok;
                out.bounds_source = res.bounds_source;
            }
            else
            {
                const auto copyin_class = static_cast<rc_seed::CopyinProbeClass>(
                    mock::bridge_copyin_probe(blocks0, capacity));
                const auto res = rc_seed::resolve_listing_gap(shape, copyin_class);
                out.gap_probed = true;
                out.note = res.note;
                out.admitted = res.ok;
                out.via_kernel = false;
                out.bounds_source = res.bounds_source;
            }
        }
        return out;
    }

    // Replica of the patched anchor/entry capture consumption (hunks 7-8):
    // kernel capture first, bridge sink capture second; loud refusal on a
    // miss of both; never a direct read on an oracle-admitted boot.
    inline rc_seed::Check anchor_check(uint64_t blocks0)
    {
        const uint8_t *capture = nullptr;
        uint32_t capture_len = 0;
        if ((g_kernel_capture.lookup(blocks0, &capture, &capture_len) == 1 ||
             mock::bridge_copyin_capture(blocks0, &capture, &capture_len) == 1) &&
            capture != nullptr && capture_len >= 2 + rc_seed::kNoneString.size())
        {
            return rc_seed::check_none_anchor(capture, capture_len);
        }
        return {false, "kernel/copyin capture unavailable or short for the 'None' anchor"};
    }

    inline rc_seed::Check entry_check(uint64_t blocks0, uint32_t id, uint32_t current_block,
                                      uint32_t current_byte_offset, std::string_view expected)
    {
        const auto bounds = rc_seed::check_entry_bounds(id, current_block, current_byte_offset,
                                                        2 + expected.size());
        if (!bounds.ok) { return bounds; }
        const auto loc = rc_seed::entry_location(id);
        const uint8_t *capture = nullptr;
        uint32_t capture_len = 0;
        if (g_kernel_capture.lookup(blocks0, &capture, &capture_len) != 1)
        {
            mock::bridge_copyin_capture(blocks0, &capture, &capture_len);
        }
        if (capture == nullptr || loc.byte_offset + 2 + expected.size() > capture_len)
        {
            return {false, "capture cannot cover the entry bytes (first-page law)"};
        }
        const auto match = rc_seed::entry_matches_ascii(capture + loc.byte_offset,
                                                        capture_len - loc.byte_offset, expected);
        if (!match.ok) { return {false, match.failure}; }
        return {true, nullptr};
    }

    // Replica of the patched probe_seed_read guard (hunks 3-5): chunked
    // safe_probe; kGuardProceed = 127 on absent/badargs.
    static constexpr int32_t kGuardProceed = 127;
    inline int32_t probe_seed_read(uint64_t addr, uint64_t len)
    {
        if (!ksrv::maybe_available()) { return kGuardProceed; }
        const uint64_t kChunk = ksrv::KSR_MAX_LEN;
        uint64_t done = 0;
        while (done < len)
        {
            const uint64_t seg = (len - done) < kChunk ? (len - done) : kChunk;
            const int32_t verdict = ksrv::safe_probe(addr + done, seg);
            switch (verdict)
            {
            case ksrv::KSR_PROBE_READABLE: break;
            case ksrv::KSR_PROBE_NOREAD:
            case ksrv::KSR_PROBE_UNMAPPED:
            case ksrv::KSR_PROBE_FAULT:
                return verdict;
            default:
                return kGuardProceed;
            }
            done += seg;
        }
        return ksrv::KSR_PROBE_READABLE;
    }
} // namespace replica

// ---------------------------------------------------------------------------
// Fixture: a pool whose Blocks[0] lives at blocks0_addr; entries laid out in
// the first page (entry 0 'None', entry 8 'ByteProperty').
// ---------------------------------------------------------------------------
struct Fixture
{
    static constexpr uint64_t kPoolBase = 0x9700000;
    static constexpr uint32_t kNoneId = 0;
    static constexpr uint32_t kBytePropertyId = 8;
    uint64_t blocks0 = 0;
    uint32_t current_block = 0;
    uint32_t current_byte_offset = 0x1000;

    // Published spans: image spans only — deliberately NOT covering blocks0
    // (the listing-gap scenario: shape NotCovered, rescuable).
    rc_seed::AddressSpan spans[2] = {
        {0x400000, 0x7479b1c},          // LOAD0-ish
        {0x747c000, 0x9633418},         // LOAD1..LOAD7-ish
    };
    uint64_t envelope = 0x1100400000ull; // live captured-envelope end

    void write_entries(std::vector<uint8_t> &block) const
    {
        auto put = [&](uint32_t id, const char *s)
        {
            const auto loc = rc_seed::entry_location(id);
            const uint16_t header = static_cast<uint16_t>(std::strlen(s) << 6);
            block[loc.byte_offset + 0] = static_cast<uint8_t>(header & 0xFF);
            block[loc.byte_offset + 1] = static_cast<uint8_t>(header >> 8);
            std::memcpy(block.data() + loc.byte_offset + 2, s, std::strlen(s));
        };
        put(kNoneId, "None");
        put(kBytePropertyId, "ByteProperty");
    }

    rc_seed::PoolShapeCheck shape() const
    {
        return rc_seed::check_pool_shape(current_block, kNoneId, blocks0,
                                         spans, 2, envelope);
    }
};

int main()
{
    const uint64_t kCapacity = rc_seed::kFNameBlockCapacityBytes; // 0x20000

    // ---- Case A: ModuleNew + DM-class Blocks[0] (the live class) ----------
    {
        mock::reset(mock::Module::New);
        Fixture f;
        f.blocks0 = 0x22C0000000ull; // the live observed Blocks[0]
        auto &dm = mock::add_range(f.blocks0, f.blocks0 + kCapacity, mock::RangeKind::DmClass);
        f.write_entries(dm.bytes);
        // an image range for the seed guards
        mock::add_range(0x747c000, 0x9633418, mock::RangeKind::Image);
        mock::install();
        replica::g_kernel_capture.reset();

        const auto shape = f.shape();
        CHECK(!shape.ok, "A: shape fails (Blocks[0] outside published spans)");
        CHECK(shape.failure_class == rc_seed::PoolShapeCheck::FailureClass::CaptureTruncated ||
              shape.failure_class == rc_seed::PoolShapeCheck::FailureClass::NotCovered,
              "A: rescuable failure class (live 0x22C0000000 > envelope 0x1100400000: CaptureTruncated)");

        const auto out = replica::resolve_listing_gap_order(shape, f.blocks0, kCapacity);
        CHECK(out.admitted, "A: admitted via kernel safe_read");
        CHECK(out.via_kernel, "A: provenance is the kernel route");
        CHECK(std::string(out.bounds_source) == "kernel-safe-read", "A: bounds_source");
        CHECK(mock::g_counters.copyin_probe == 0,
              "A: copyin oracle never spent (kernel succeeded; DM-class would have refused it)");
        CHECK(mock::g_counters.safe_read == 2, "A: full 0x20000 range = 2 chunked kekcalls");

        const auto anchor = replica::anchor_check(f.blocks0);
        CHECK(anchor.ok, "A: 'None' anchor from kernel capture");
        if (!anchor.ok) { std::printf("  anchor failure: %s\n", anchor.failure); }

        const auto e1 = replica::entry_check(f.blocks0, Fixture::kNoneId, f.current_block,
                                             f.current_byte_offset, rc_seed::kNoneString);
        CHECK(e1.ok, "A: 'None' entry match from kernel capture");
        if (!e1.ok) { std::printf("  entry1 failure: %s\n", e1.failure); }
        const auto e2 = replica::entry_check(f.blocks0, Fixture::kBytePropertyId, f.current_block,
                                             f.current_byte_offset, rc_seed::kBytePropertyString);
        CHECK(e2.ok, "A: 'ByteProperty' entry match from kernel capture");
        if (!e2.ok) { std::printf("  entry2 failure: %s\n", e2.failure); }

        // Seed guards over a 0x14040-byte pool window (chunked probe).
        CHECK(replica::probe_seed_read(0x747c000 + 0x8, 0x14040) == ksrv::KSR_PROBE_READABLE,
              "A: pool-window guard probes readable (chunked)");
        CHECK(mock::g_counters.safe_probe >= 2, "A: window needed >=2 probe kekcalls");
    }

    // ---- Case B: ModuleOld -> byte-for-byte copyin fallback -----------------
    {
        mock::reset(mock::Module::Old);
        Fixture f;
        f.blocks0 = 0x2000000; // IMAGE-class (copyin-allowlisted), below the
                              // envelope and OUTSIDE every published span
        auto &img = mock::add_range(f.blocks0, f.blocks0 + kCapacity, mock::RangeKind::Image);
        f.write_entries(img.bytes);
        mock::add_range(0x747c000, 0x9633418, mock::RangeKind::Image);
        mock::install();
        replica::g_kernel_capture.reset();

        const auto shape = f.shape();
        CHECK(!shape.ok, "B: shape fails");
        const auto out = replica::resolve_listing_gap_order(shape, f.blocks0, kCapacity);
        CHECK(out.admitted, "B: admitted via the legacy copyin oracle");
        CHECK(!out.via_kernel, "B: provenance is NOT the kernel route");
        CHECK(std::string(out.bounds_source) == "kernel-write-copyin-read", "B: legacy bounds_source");
        CHECK(mock::g_counters.copyin_probe == 1, "B: exactly one copyin probe (fallback spent)");
        CHECK(mock::g_counters.safe_read == 1, "B: one kernel read attempted (ENOSYS) then latched");

        const auto anchor = replica::anchor_check(f.blocks0);
        CHECK(anchor.ok, "B: 'None' anchor from bridge sink capture");
        const auto e2 = replica::entry_check(f.blocks0, Fixture::kBytePropertyId, f.current_block,
                                             f.current_byte_offset, rc_seed::kBytePropertyString);
        CHECK(e2.ok, "B: 'ByteProperty' entry match from sink capture");

        // After the ENOSYS latch, seed guards degrade to proceed (legacy).
        CHECK(replica::probe_seed_read(0x747c000, 8) == replica::kGuardProceed,
              "B: guard no-ops after ENOSYS latch");
    }

    // ---- Case B2: ModuleOld + DM-class Blocks[0]: honest refusal ------------
    {
        mock::reset(mock::Module::Old);
        Fixture f;
        f.blocks0 = 0x22C0000000ull;
        mock::add_range(f.blocks0, f.blocks0 + kCapacity, mock::RangeKind::DmClass);
        mock::install();
        replica::g_kernel_capture.reset();

        const auto out = replica::resolve_listing_gap_order(f.shape(), f.blocks0, kCapacity);
        CHECK(!out.admitted, "B2: old module + DM-class: copyin refuses -> honest refusal");
        CHECK(mock::g_counters.copyin_probe == 1, "B2: copyin oracle spent once");
    }

    // ---- Case C: ModuleNew + PROT_NONE Blocks[0]: kernel refuses, fail-closed
    {
        mock::reset(mock::Module::New);
        Fixture f;
        f.blocks0 = 0xA0000000;
        mock::add_range(f.blocks0, f.blocks0 + kCapacity, mock::RangeKind::ProtNone);
        mock::install();
        replica::g_kernel_capture.reset();

        const auto out = replica::resolve_listing_gap_order(f.shape(), f.blocks0, kCapacity);
        CHECK(!out.admitted, "C: PROT_NONE -> refused");
        CHECK(!out.via_kernel, "C: not admitted");
        CHECK(mock::g_counters.copyin_probe == 0,
              "C: copyin oracle NOT spent after a kernel page-table refusal (single-oracle law)");
    }

    // ---- Case D: seed guards refuse on a PROT_NONE seed address -------------
    {
        mock::reset(mock::Module::New);
        mock::add_range(0x900000, 0x1000000, mock::RangeKind::ProtNone); // a gap/prot region
        mock::install();
        const int32_t v = replica::probe_seed_read(0x900000, 8);
        CHECK(v == ksrv::KSR_PROBE_NOREAD, "D: PROT_NONE seed address -> live refusal verdict");
    }

    // ---- Case E: bridge Control-D replica (diff 0002) -----------------------
    {
        // E1: kernel route runs even when the sink/write route is BROKEN.
        mock::reset(mock::Module::New);
        mock::g_sink_open_fails = true; // Control B would fail -> legacy D skipped
        const uint64_t cand = 0x400000 + 0x7dd2a40ull;
        auto &img = mock::add_range(cand, cand + 64, mock::RangeKind::Image);
        std::memcpy(img.bytes.data(), "WINDOW_E_AES_SBOX_HEAD", 22);
        mock::install();
        {
            // kernel-first Control D (replica of diff 0002's route order)
            uint8_t buf[64];
            const int64_t rc = ksrv::safe_read(cand, buf, 64);
            CHECK(rc == 64, "E1: kernel Control D readable despite broken sink");
            CHECK(std::memcmp(buf, "WINDOW_E_AES_SBOX_HEAD", 22) == 0, "E1: candidate bytes");
            CHECK(mock::g_counters.copyin_probe == 0, "E1: legacy route not touched");
        }
        // E2: old module + working sink -> legacy copyin Control D.
        mock::reset(mock::Module::Old);
        auto &img2 = mock::add_range(cand, cand + 64, mock::RangeKind::Image);
        std::memcpy(img2.bytes.data(), "WINDOW_E_AES_SBOX_HEAD", 22);
        mock::install();
        {
            uint8_t buf[64];
            const int64_t rc = ksrv::safe_read(cand, buf, 64); // ENOSYS -> latched
            CHECK(rc == ksrv::KSR_ERR_ENOSYS, "E2: kernel route absent");
            const int verdict = mock::bridge_copyin_probe(cand, 64);
            CHECK(verdict == 1, "E2: legacy copyin Control D carries the boot (route=0)");
            const uint8_t *cap = nullptr; uint32_t cap_len = 0;
            CHECK(mock::bridge_copyin_capture(cand, &cap, &cap_len) == 1 && cap_len == 64,
                  "E2: sink capture filled for host comparison");
        }
    }

    if (failures == 0) { std::printf("pool_gap_test: ALL PASS\n"); return 0; }
    std::printf("pool_gap_test: %d FAILURES\n", failures);
    return 1;
}
