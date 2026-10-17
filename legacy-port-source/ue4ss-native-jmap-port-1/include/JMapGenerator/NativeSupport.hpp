#pragma once
#include <polyhook2/OwnedMemory.hpp>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>

namespace RC::JMapGenerator::Native
{
    // Current owned-region metadata only. The dump caller must retain registrations
    // and serialize lifetime/protection changes during direct engine traversal.
    class MemoryChecker
    {
    public:
        bool is_readable(const void* pointer, size_t size = 1) const
        {
            const auto begin = reinterpret_cast<uint64_t>(pointer);
            const auto length = size ? size : 1;
            if (!begin || length > std::numeric_limits<uint64_t>::max() - begin)
                return false;
            const auto end = begin + length;
            auto current = begin;
            while (current < end)
            {
                const auto region = PLH::OwnedMemory::query(current, 1);
                if (!region || !(region->protection & PLH::R) || region->size > std::numeric_limits<uint64_t>::max() - region->base)
                    return false;
                const auto region_end = region->base + region->size;
                if (region->base > current || region_end <= current)
                    return false;
                current = region_end;
            }
            return true;
        }
        bool is_executable(const void* pointer) const
        {
            const auto region = PLH::OwnedMemory::query(reinterpret_cast<uint64_t>(pointer), 1);
            return region && (region->protection & PLH::X);
        }
    };

    inline std::string utc_timestamp(std::time_t now)
    {
        std::tm utc{};
        if (now == static_cast<std::time_t>(-1) || !gmtime_r(&now, &utc))
            throw std::runtime_error("Native JMap UTC timestamp unavailable");
        char buffer[32]{};
        if (!std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc))
            throw std::runtime_error("Native JMap timestamp formatting failed");
        return buffer;
    }

    inline std::string source_filename(const std::filesystem::path& source)
    {
        if (source.empty() || !source.has_filename())
            throw std::invalid_argument("Native JMap requires an executable filename");
        return source.filename().string();
    }
}
