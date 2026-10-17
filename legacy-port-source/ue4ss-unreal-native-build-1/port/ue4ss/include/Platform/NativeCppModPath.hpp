#pragma once
#include <array>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace RC::Platform
{
    inline auto NativeCppModCandidates(std::string_view mod_name) -> std::array<std::string, 6>
    {
        if (mod_name.empty() || mod_name == "." || mod_name == ".." ||
            mod_name.find_first_of("/\\\0", 0, 3) != mod_name.npos)
            throw std::invalid_argument("Native C++ mod requires one mod name");
        const std::string name(mod_name);
        return {"main.prx", "main.sprx", name + ".prx", name + ".sprx", "main.dll", name + ".dll"};
    }
    // Source-built modules use PRX/SPRX names; historical DLL names remain last.
    // This is discovery only. CppModModule remains responsible for refusing PE.
    inline auto FindNativeCppModModule(const std::filesystem::path& directory,
                                      std::string_view mod_name) -> std::optional<std::filesystem::path>
    {
        if (!directory.is_absolute())
            throw std::invalid_argument("Native C++ mod discovery requires an absolute directory");
        const auto candidates = NativeCppModCandidates(mod_name);
        for (const auto& candidate : candidates)
        {
            const auto path = directory / candidate;
            if (std::filesystem::is_regular_file(path)) return path;
        }
        return std::nullopt;
    }
}
