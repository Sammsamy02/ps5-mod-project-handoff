#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <NativeLayoutFiles.hpp>
namespace RC::Platform::NativeLayouts {
// Stage an entire pair without publishing engine maps. One loaded lifetime.
void StageNativeLayoutFiles(const std::filesystem::path& directory);
// Parses the same complete pair from activation-owned startup buffers. No
// filesystem access and no engine-map publication occur here.
void StageNativeLayoutTexts(std::string_view members, std::string_view virtuals);
void SetResolvedNativeLayoutAxes(std::optional<bool> non_shipping, std::optional<bool> stats);
void PublishStagedNativeLayoutMaps(uint32_t major, uint32_t minor, bool non_shipping, bool stats);
}
