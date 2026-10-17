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
inline bool valid_range(uint64_t addr, uint64_t len) noexcept
{
    return len != 0 && len <= 0x1000000u && addr <= UINT64_MAX - len;
}
struct FaultLedger {
    struct Range { uint64_t lo, hi; } ranges[64]{};
    uint32_t count = 0;
    bool saturated = false;
    void record(uint64_t addr, uint64_t len) noexcept
    {
        if (!valid_range(addr, len)) { saturated = true; return; }
        for (uint32_t i = 0; i < count; ++i)
            if (ranges[i].lo <= addr && ranges[i].hi >= addr + len) return;
        if (count == 64) { saturated = true; return; }
        ranges[count++] = {addr, addr + len};
    }
    bool overlaps(uint64_t addr, uint64_t len) const noexcept
    {
        if (saturated || !valid_range(addr, len)) return true;
        for (uint32_t i = 0; i < count; ++i)
            if (addr < ranges[i].hi && ranges[i].lo < addr + len) return true;
        return false;
    }
};
} // namespace l13_sink
