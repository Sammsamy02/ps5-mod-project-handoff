#pragma once
#include <array>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace RC::Platform
{
    // Source-built native modules use PRX/SPRX names. Retain the historical
    // DLL names last; CppModModule remains responsible for refusing PE content.
    // This is discovery only and never loads or rewrites any file.
    inline auto FindNativeCppModModule(const std::filesystem::path& directory,
                                      std::string_view mod_name) -> std::optional<std::filesystem::path>
    {
        if (!directory.is_absolute() || mod_name.empty() || mod_name == "." || mod_name == ".." ||
            mod_name.find_first_of("/\\\0", 0, 3) != mod_name.npos)
            throw std::invalid_argument("Native C++ mod discovery requires an absolute directory and one mod name");
        const std::string name(mod_name);
        const std::array<std::string, 6> candidates{
            "main.prx", "main.sprx", name + ".prx", name + ".sprx", "main.dll", name + ".dll"};
        for (const auto& candidate : candidates)
        {
            const auto path = directory / candidate;
            if (std::filesystem::is_regular_file(path)) return path;
        }
        return std::nullopt;
    }
}
