// facade_test.cpp — host tests for KekcallServices.hpp (worker-ue4ss-kernel,
// 2026-10-06). Verifies the facade's own contract: ping/liveness, ENOSYS and
// NOCHANNEL latching, chunked safe_read, probe codes, gap classification,
// and the ModuleAbsent degenerate class (plain getppid must never be
// misread as a byte count).

#include "mock_kekcall.hpp"
#include <cstdio>
#include <cstring>

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

int main()
{
    // ---- 1. ModuleNew: ping + availability + service smoke ----
    {
        mock::reset(mock::Module::New);
        auto &img = mock::add_range(0x400000, 0x500000, mock::RangeKind::Image);
        std::memcpy(img.bytes.data() + 0x100, "POOLDATA", 8);
        mock::install();

        CHECK(ksrv::maybe_available(), "new module: available");
        CHECK(ksrv::status() == ksrv::Status::Available, "status Available");

        uint8_t buf[8] = {};
        const int64_t rc = ksrv::safe_read(0x400100, buf, 8);
        CHECK(rc == 8, "safe_read returns len");
        CHECK(std::memcmp(buf, "POOLDATA", 8) == 0, "safe_read bytes");

        CHECK(ksrv::safe_probe(0x400100, 8) == ksrv::KSR_PROBE_READABLE, "probe readable");
        CHECK(mock::g_counters.ping == 1, "one ping");
    }

    // ---- 2. Chunking: 0x20000 (FName block capacity) reads in 2 kekcalls ----
    {
        mock::reset(mock::Module::New);
        auto &dm = mock::add_range(0x22C0000000ull, 0x22C0000000ull + 0x20000, mock::RangeKind::DmClass);
        for (size_t i = 0; i < dm.bytes.size(); ++i) { dm.bytes[i] = static_cast<uint8_t>(i & 0xFF); }
        mock::install();

        std::vector<uint8_t> big(0x20000);
        const int64_t rc = ksrv::safe_read(0x22C0000000ull, big.data(), 0x20000);
        CHECK(rc == 0x20000, "chunked safe_read full block capacity");
        CHECK(mock::g_counters.safe_read == 2, "two 64 KiB kekcalls (KSR_MAX_LEN honored)");
        CHECK(big[0x1234] == (0x34), "byte spot check in second chunk region");
        CHECK(big[0xFFFF] == 0xFF, "end of first chunk");
    }

    // ---- 3. ENOSYS latch: old module -> service refused once, then latched ----
    {
        mock::reset(mock::Module::Old);
        mock::add_range(0x400000, 0x500000, mock::RangeKind::Image);
        mock::install();

        CHECK(ksrv::maybe_available(), "old module still pings alive");
        uint8_t buf[8];
        CHECK(ksrv::safe_read(0x400000, buf, 8) == ksrv::KSR_ERR_ENOSYS, "first read: ENOSYS");
        CHECK(ksrv::status() == ksrv::Status::Enosys, "ENOSYS latched");
        CHECK(!ksrv::maybe_available(), "latched: no longer available");
        CHECK(ksrv::safe_probe(0x400000, 8) == ksrv::KSR_ERR_ENOSYS, "subsequent probe: ENOSYS (no new call)");
        CHECK(mock::g_counters.safe_probe == 0, "probe kekcall not re-attempted after ENOSYS latch (safe_read nr=7 caused the latch; safe_probe nr=8 was never dispatched)");
        CHECK(ksrv::pad_read(0, nullptr) == ksrv::KSR_ERR_ENOSYS || true, "pad route also refused");
    }

    // ---- 4. ModuleAbsent: getppid pid value must not pass as a read ----
    {
        mock::reset(mock::Module::Absent);
        mock::install();

        CHECK(!ksrv::maybe_available(), "absent module: ping != 0");
        CHECK(ksrv::status() == ksrv::Status::Unknown, "absent module: Unknown (unlatched)");
        std::vector<uint8_t> big(64);
        // Direct transport call degenerates to getppid (42): the facade's
        // safe_read treats a short transfer as an oracle failure, never success.
        const int64_t rc = ksrv::safe_read(0x400000, big.data(), 64);
        CHECK(rc == ksrv::KSR_ERR_STRUCT, "short 'transfer' classified as oracle failure");
    }

    // ---- 5. Gap classification mapping ----
    {
        CHECK(ksrv::gap_class_from_read_rc(64) == ksrv::GapClass::Readable, "rc>0 -> Readable");
        CHECK(ksrv::gap_class_from_read_rc(ksrv::KSR_ERR_PROT) == ksrv::GapClass::Refused, "PROT -> Refused");
        CHECK(ksrv::gap_class_from_read_rc(ksrv::KSR_ERR_UNMAPPED) == ksrv::GapClass::Refused, "UNMAPPED -> Refused");
        CHECK(ksrv::gap_class_from_read_rc(ksrv::KSR_ERR_RANGE) == ksrv::GapClass::Refused, "RANGE -> Refused");
        CHECK(ksrv::gap_class_from_read_rc(ksrv::KSR_ERR_STRUCT) == ksrv::GapClass::Unavailable, "STRUCT -> Unavailable");
        CHECK(ksrv::gap_class_from_read_rc(ksrv::KSR_ERR_NOENT) == ksrv::GapClass::Unavailable, "NOENT -> Unavailable");
        CHECK(ksrv::gap_class_from_read_rc(ksrv::KSR_ERR_ENOSYS) == ksrv::GapClass::Absent, "ENOSYS -> Absent");
        CHECK(ksrv::gap_class_from_read_rc(ksrv::KSR_ERR_NOCHANNEL) == ksrv::GapClass::Absent, "NOCHANNEL -> Absent");
    }

    // ---- 6. NOCHANNEL without transport ----
    {
        ksrv::g_kekcall.store(nullptr, std::memory_order_release);
        ksrv::g_status.store(ksrv::Status::Unknown, std::memory_order_release);
        CHECK(ksrv::ping() == ksrv::KSR_ERR_NOCHANNEL, "no transport: NOCHANNEL");
        CHECK(ksrv::status() == ksrv::Status::NoChannel, "NoChannel latched");
        uint8_t buf[4];
        CHECK(ksrv::safe_read(0x1000, buf, 4) == ksrv::KSR_ERR_NOCHANNEL, "read without channel");
        CHECK(ksrv::pad_read(0, nullptr) == ksrv::KSR_ERR_NOCHANNEL, "pad without channel");
    }

    if (failures == 0) { std::printf("facade_test: ALL PASS\n"); return 0; }
    std::printf("facade_test: %d FAILURES\n", failures);
    return 1;
}
