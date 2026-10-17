#include "NearTrampoline.hpp"

#include <climits>
#include <cstring>

#if defined(__APPLE__) || defined(__unix__) || defined(__PROSPERO__)
#include <sys/mman.h>
#define CAP03_POSIX_MMAP 1
#else
#define CAP03_POSIX_MMAP 0
#endif

namespace cap03 {
namespace {

constexpr uint64_t kScanStride = 32ULL * 1024 * 1024;
constexpr uint64_t kScanSteps = 64;

bool power_of_two(size_t value)
{
    return value && !(value & (value - 1));
}

bool within_near_window(uint64_t candidate, uint64_t target)
{
    const __int128 delta = static_cast<__int128>(candidate) -
                           static_cast<__int128>(target);
    const __int128 limit = static_cast<__int128>(INT32_MAX) -
                           static_cast<__int128>(kMaxTrampolineBytes);
    return delta >= -limit && delta <= limit;
}

bool add_hint(uint64_t anchor, uint64_t offset, bool above,
              uint64_t* out)
{
    if (above)
    {
        if (anchor > UINT64_MAX - offset) return false;
        *out = anchor + offset;
    }
    else
    {
        if (anchor < offset) return false;
        *out = anchor - offset;
    }
    return true;
}

#if CAP03_POSIX_MMAP
#if defined(MAP_ANONYMOUS)
constexpr int kAnonymousMapFlag = MAP_ANONYMOUS;
#elif defined(MAP_ANON)
constexpr int kAnonymousMapFlag = MAP_ANON;
#elif defined(__PROSPERO__)
// Pinned ps5-payload-sdk v0.43 hides MAP_ANON under strict _XOPEN_SOURCE.
constexpr int kAnonymousMapFlag = 0x1000;
#else
#error "anonymous mmap flag is unavailable"
#endif

bool posix_reserve(void*, uint64_t hint, size_t bytes, void** out)
{
    if (!out) return false;
    void* address = mmap(reinterpret_cast<void*>(hint), bytes,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | kAnonymousMapFlag, -1, 0);
    if (address == MAP_FAILED) return false;
    if (!address) { (void)munmap(address, bytes); return false; }
    *out = address;
    return true;
}

bool posix_seal(void*, void* address, size_t bytes)
{
    return mprotect(address, bytes, PROT_READ | PROT_EXEC) == 0;
}

void posix_release(void*, void* address, size_t bytes)
{
    if (address && bytes) (void)munmap(address, bytes);
}
#endif

} // namespace

void PreparedNearDetour::reset() noexcept
{
    if (trampoline && memory.release)
        memory.release(memory.user, trampoline, mapping_bytes);
    trampoline = nullptr;
    mapping_bytes = 0;
    memory = {};
    plan = {};
}

NearPrepareResult prepare_near_detour(const uint8_t* original, size_t available,
                                      uint64_t target_address,
                                      uint64_t handler_address,
                                      size_t page_size, NearMemoryOps ops,
                                      PreparedNearDetour* out)
{
    NearPrepareResult result{};
    if (!out || out->trampoline || !original || !target_address ||
        !handler_address || available < kAbsoluteJumpBytes ||
        !power_of_two(page_size) || page_size < kMaxTrampolineBytes ||
        !ops.reserve_writable || !ops.seal_executable || !ops.release)
        return result;

    const uint64_t anchor = target_address & ~uint64_t(page_size - 1);
    const size_t bytes = page_size;
    result.status = NearPrepareStatus::MappingUnavailable;
    for (uint64_t step = 0; step <= kScanSteps; ++step)
    {
        const uint64_t offset = step ? step * kScanStride : page_size;
        for (int side = 0; side < 2; ++side)
        {
            uint64_t hint = 0;
            if (!add_hint(anchor, offset, side == 0, &hint)) continue;
            void* mapping = nullptr;
            if (!ops.reserve_writable(ops.user, hint, bytes, &mapping))
                continue;
            ++result.candidates_checked;
            if (!mapping)
            {
                result.status = NearPrepareStatus::BadArgs;
                return result; // backend violated its success contract
            }
            const uint64_t candidate = reinterpret_cast<uint64_t>(mapping);
            if (candidate % page_size ||
                candidate > UINT64_MAX - bytes ||
                !within_near_window(candidate, target_address))
            {
                ops.release(ops.user, mapping, bytes);
                result.plan_status = DetourPlanResult::RelativeOutOfRange;
                continue;
            }
            DetourPlan plan{};
            const auto status = plan_detour(original, available, target_address,
                                            candidate, handler_address, &plan);
            result.plan_status = status;
            if (status != DetourPlanResult::Ok)
            {
                ops.release(ops.user, mapping, bytes);
                if (status == DetourPlanResult::RelativeOutOfRange ||
                    status == DetourPlanResult::Overlap ||
                    status == DetourPlanResult::AddressOverflow)
                    continue;
                result.status = NearPrepareStatus::PlanRejected;
                return result;
            }
            std::memcpy(mapping, plan.trampoline.data(), plan.trampoline_length);
            __builtin___clear_cache(static_cast<char*>(mapping),
                static_cast<char*>(mapping) + plan.trampoline_length);
            if (!ops.seal_executable(ops.user, mapping, bytes))
            {
                ops.release(ops.user, mapping, bytes);
                result.status = NearPrepareStatus::SealFailed;
                return result;
            }
            out->plan = plan;
            out->trampoline = mapping;
            out->mapping_bytes = bytes;
            out->memory = ops;
            result.status = NearPrepareStatus::Ok;
            return result;
        }
    }
    return result;
}

NearMemoryOps posix_near_memory_ops() noexcept
{
#if CAP03_POSIX_MMAP
    return {nullptr, posix_reserve, posix_seal, posix_release};
#else
    return {};
#endif
}

} // namespace cap03
