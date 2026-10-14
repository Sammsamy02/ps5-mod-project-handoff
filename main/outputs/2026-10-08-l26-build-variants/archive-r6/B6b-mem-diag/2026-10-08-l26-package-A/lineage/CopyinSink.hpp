#pragma once
#include <cstdint>
#include <cerrno>

// Shared by the production bridge and host tests. Error recognition precedes
// count arithmetic, so zero-extended SCE errors can never become byte counts.
namespace l13_sink {
inline int error(int64_t raw, int saved_errno) noexcept
{
    const uint32_t bits = static_cast<uint32_t>(raw);
    if (raw >= INT32_MIN && raw <= UINT32_MAX &&
        (bits & 0xffff0000u) == 0x80020000u)
        return static_cast<int>(bits & 0xffffu);
    if (raw == -1) return saved_errno;
    if (raw < -1 && raw >= -INT32_MAX) return static_cast<int>(-raw);
    return raw < 0 ? EIO : 0;
}
// [L27][FX] valid_range is constexpr so the FaultLedger self-test below can
// run in constant expressions (behavior identical to the l13 inline form).
constexpr bool valid_range(uint64_t addr, uint64_t len) noexcept
{
    return len != 0 && len <= 0x1000000u && addr <= UINT64_MAX - len;
}
// [L27][FX fault-ledger desaturation] record() outcome, made explicit so no
// caller-visible state change can hide inside record(): the pre-fix ledger
// saturated (poisoning EVERY overlaps() query) on an INVALID record range and
// at capacity, and a saturated ledger was indistinguishable from a real
// recorded fault at the bridge_copyin_range_faulted admission gate — the
// "known_fault TRUE with zero recorded probes" false-positive class.
enum class RecordResult : uint8_t { Recorded, AlreadyKnown, Full, InvalidRange };
struct FaultLedger {
    struct Range { uint64_t lo, hi; } ranges[64]{};
    uint32_t count = 0;
    bool saturated = false;        // capacity-only: 64 real faulted ranges recorded
    uint32_t rejected_invalid = 0; // invalid-range record() attempts; never recorded
    constexpr RecordResult record(uint64_t addr, uint64_t len) noexcept
    {
        // (a) An INVALID range is not a fault and must never saturate the
        // ledger: reject, count, and let the caller log once. The only
        // record() call site today (the EFAULT arm of
        // bridge_kernel_write_copyin_probe_until) sits behind valid_range(),
        // so this arm is a loud guard for future callers and host tests.
        if (!valid_range(addr, len)) { ++rejected_invalid; return RecordResult::InvalidRange; }
        for (uint32_t i = 0; i < count; ++i)
            if (ranges[i].lo <= addr && ranges[i].hi >= addr + len) return RecordResult::AlreadyKnown;
        if (count == 64) { saturated = true; return RecordResult::Full; }
        ranges[count++] = {addr, addr + len};
        return RecordResult::Recorded;
    }
    // (b) REAL recorded faults only. Capacity saturation no longer answers
    // for every query: it is observable ONLY via saturated_capacity(), an
    // accessor the admission gate (bridge_copyin_range_faulted) deliberately
    // does not consult, so a full ledger can never masquerade as a recorded
    // fault. An unqueryable (invalid) range still fails closed, unchanged.
    constexpr bool overlaps(uint64_t addr, uint64_t len) const noexcept
    {
        if (!valid_range(addr, len)) return true;
        for (uint32_t i = 0; i < count; ++i)
            if (addr < ranges[i].hi && ranges[i].lo < addr + len) return true;
        return false;
    }
    constexpr bool saturated_capacity() const noexcept { return saturated; }
    // (c) Regression self-test ([ST] fault-ledger unit test): record(invalid)
    // + queries on a CLEAN ledger. Bit semantics (all set = pass):
    //   bit0 invalid record() rejected without recording or saturating
    //   bit1 clean-ledger non-overlap query returns false
    //   bit2 a real recorded fault still overlaps; non-overlap still false
    //   bit3 saturation queryable only via saturated_capacity(); a
    //        non-saturated non-overlap query stays false after both records
    static constexpr uint32_t self_test_mask() noexcept
    {
        FaultLedger ledger{};
        const uint32_t mask_invalid =
            (ledger.record(0, 0) == RecordResult::InvalidRange
             && ledger.record(UINT64_MAX, 2) == RecordResult::InvalidRange
             && ledger.count == 0 && !ledger.saturated
             && ledger.rejected_invalid == 2) ? 1u : 0u;
        const uint32_t mask_clean = (!ledger.overlaps(0x1000, 32)) ? 2u : 0u;
        const uint32_t mask_real =
            (ledger.record(0x2000, 32) == RecordResult::Recorded
             && ledger.overlaps(0x2000, 32)
             && ledger.overlaps(0x2010, 16)
             && !ledger.overlaps(0x3000, 16)) ? 4u : 0u;
        const uint32_t mask_sat =
            (!ledger.saturated_capacity()
             && !ledger.overlaps(0x5000, 16)) ? 8u : 0u;
        return mask_invalid | mask_clean | mask_real | mask_sat;
    }
};
static_assert(l13_sink::FaultLedger::self_test_mask() == 0xFu,
              "fault-ledger desaturation regression: record(invalid) must not "
              "poison overlaps(); real recorded faults must still refuse; "
              "capacity saturation must be observable only via "
              "saturated_capacity()");
} // namespace l13_sink
