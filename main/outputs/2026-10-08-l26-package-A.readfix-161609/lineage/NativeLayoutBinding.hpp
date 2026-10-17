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
// Snapshot only declared, already parsed/staged metadata and fact names. No
// offsets are published and no engine memory/accessor is queried.
struct StagedLayoutFact {std::string name;uint32_t offset,width=0;};
struct StagedFacts { Identity identity; std::vector<StagedLayoutFact> members,vtables; };
auto GetStagedNativeLayoutFacts() -> std::optional<StagedFacts>;
void SetResolvedNativeLayoutAxes(std::optional<bool> non_shipping, std::optional<bool> stats);
void PublishStagedNativeLayoutMaps(uint32_t major, uint32_t minor, bool non_shipping, bool stats);
}
