#pragma once
#include "OwnedMappedImageCaller.hpp"  // comp6-drybuild: quoted -> STAGING D2 Diff C copy
#include <array>
#include <cstdint>

namespace RC::Unreal::Platform {
enum class MappedSlotStatus : uint8_t {
    Available, Unavailable, NoMatch, Ambiguous, OperandOrMetadataError,
    InvalidArguments, InvalidImage, RejectedAddress, InvalidResult
};
struct MappedSlotReport {
    MappedSlotStatus status{MappedSlotStatus::Unavailable};
    int32_t endpoint_status{-1}; // -1: this slot has no mapped provider.
    bool requested{};
};
struct OwnedMappedScanReport {
    PsScanResults results{};
    std::array<MappedSlotReport,13> slots{};
    uint32_t requested_mask{};
    uint32_t available_mask{}; // Admitted candidates, regardless of config.
    uint32_t required_missing_mask{};
    bool image_error{};
    auto succeeded() const noexcept -> bool { return !image_error && !required_missing_mask; }
};
inline constexpr uint32_t MappedRequiredSlots = 0x5d;
// Source/lease construction failure: clear requested pointer/enums, retain the
// previous EngineVersion and disabled fields, mark an image-level failure.
auto unavailable_owned_mapped(const PsScanConfig&, const PsScanResults& previous = {})
    noexcept -> OwnedMappedScanReport;
// Invokes all seven genuine mapped core providers once, then applies the original
// config/result rules. Caller retains its synchronous read lease throughout.
// No logging, service calls, global setters, discovery or code execution occurs.
// Admission establishes supplied-span permissions only, not engine identity or
// callable ABI. Consumers must retain their own owners after this borrow ends.
auto collect_owned_mapped(const OwnedMappedImageCaller&, const PsScanConfig&,
    const PsScanResults& previous = {}) noexcept -> OwnedMappedScanReport;
}
