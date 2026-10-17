#pragma once
#include "modbridge/core.hpp"
#include <optional>
#include <vector>
namespace modbridge {
inline constexpr std::uint64_t dds_max_subresources = 65536;
inline constexpr std::uint64_t dds_max_normal_bytes = 268435456;
Json inspect_dds(std::span<const std::uint8_t> bytes,
                 const std::string& path, std::uint64_t total_size);
struct DdsBlockPayload { Json description; std::vector<std::uint8_t> bytes; };
class DdsNeutral {
public:
    const Json& metadata() const;
    std::span<const std::uint8_t> header() const;
    const std::vector<DdsBlockPayload>& subresources() const;
    std::span<const std::uint8_t> trailer() const;
    std::vector<std::uint8_t> lossless_dds() const;
private:
    DdsNeutral() = default;
    Json metadata_;
    std::vector<std::uint8_t> header_, trailer_;
    std::vector<DdsBlockPayload> subresources_;
    friend std::optional<DdsNeutral> normalize_dds(std::span<const std::uint8_t>, std::string*);
};
std::optional<DdsNeutral> normalize_dds(std::span<const std::uint8_t> bytes,
                                      std::string* reason = nullptr);
}
