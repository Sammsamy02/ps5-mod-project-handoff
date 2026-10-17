#ifndef CAP03_DETOUR_PLAN_HPP
#define CAP03_DETOUR_PLAN_HPP

#include "CodePatch.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace cap03 {

constexpr size_t kAbsoluteJumpBytes = 14;
// At most seven two-byte short branches can begin before the 14-byte
// overwrite threshold is reached.
constexpr size_t kMaxShortBranchIslands = kAbsoluteJumpBytes / 2;
constexpr size_t kMaxTrampolineBytes = kMaxPatchBytes + kAbsoluteJumpBytes +
                                        kMaxShortBranchIslands * kAbsoluteJumpBytes;

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
    std::array<uint8_t, kMaxPatchBytes> original_bytes{};
    std::array<uint8_t, kMaxPatchBytes> target_patch{};
    size_t trampoline_length = 0;
    std::array<uint8_t, kMaxTrampolineBytes> trampoline{};
};

// Read-only preparation. The caller supplies an already-readable snapshot,
// a caller-owned trampoline address, and a handler address. No allocation,
// memory permission change, or code write occurs here. Short external
// branches may use absolute-jump islands after the jump-back, within the
// bounded trampoline array. original_bytes can be passed to
// patch_code_if_original_ex to refuse a stale target snapshot. The
// resulting bytes still require quiescence, executable-memory preparation,
// and the existing patch/restore protocol before use.
DetourPlanResult plan_detour(const uint8_t* original, size_t available,
                             uint64_t target_address,
                             uint64_t trampoline_address,
                             uint64_t handler_address,
                             DetourPlan* out);

} // namespace cap03

#endif
