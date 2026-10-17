#ifndef CAP03_DETOUR_PLAN_HPP
#define CAP03_DETOUR_PLAN_HPP

#include "CodePatch.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace cap03 {

constexpr size_t kAbsoluteJumpBytes = 14;

enum class DetourPlanResult {
    Ok,
    BadArgs,
    DecodeFailed,
    RelocationUnsupported,
    RelativeOutOfRange,
    AddressOverflow,
    Overlap
};

struct DetourPlan {
    size_t stolen_length = 0;
    std::array<uint8_t, kMaxPatchBytes> target_patch{};
    size_t trampoline_length = 0;
    std::array<uint8_t, kMaxPatchBytes + kAbsoluteJumpBytes> trampoline{};
};

// Read-only preparation. The caller supplies an already-readable snapshot,
// a caller-owned trampoline address, and a handler address. No allocation,
// memory permission change, or code write occurs here. The resulting bytes
// still require quiescence, executable-memory preparation, and the existing
// compare-before-write patch/restore protocol before use.
DetourPlanResult plan_detour(const uint8_t* original, size_t available,
                             uint64_t target_address,
                             uint64_t trampoline_address,
                             uint64_t handler_address,
                             DetourPlan* out);

} // namespace cap03

#endif
