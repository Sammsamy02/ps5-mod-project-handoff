#pragma once
#include <array>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace RC::Platform
{
    // Source-built native modules use PRX/SPRX names. Retain the historical
    // DLL names last; CppModModule remains responsible for refusing PE content.
    // This is discovery only and never loads or rewrites any file.
    // [CAP09 F1] Never throws a std::filesystem error: every probe uses the
    // error_code overload. An OS error (permission denied, I/O error) stops
    // the lookup and is reported through *error (fail closed: no module). A
    // missing file or a dangling link is not an error (status not_found).
    inline auto FindNativeCppModModule(const std::filesystem::path& directory,
                                      std::string_view mod_name,
                                      std::error_code* error = nullptr) -> std::optional<std::filesystem::path>
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
            // status(p, ec) reports a missing file as file_type::not_found AND
            // sets ec (ENOENT/ENOTDIR): that is "try the next name", not an error.
            std::error_code ec;
            const auto st = std::filesystem::status(path, ec);
            if (st.type() == std::filesystem::file_type::not_found) continue;
            if (ec)
            {
                if (error) *error = ec;
                return std::nullopt;
            }
            if (st.type() == std::filesystem::file_type::regular) return path;
        }
        return std::nullopt;
    }
}
