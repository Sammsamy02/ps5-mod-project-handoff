// Cap10CSharpMods.hpp -- may the C# runtime (#10) start in this process?
// Engine-free, header-only, host-tested (Team G, 2026-10-13).
//
// Rule (generic, data-driven, same enable rule as #9 C++ mods):
//   * WHICH mods are enabled comes from the activation snapshot only:
//     "Name : 1" rows of ue4ss/Mods/mods.txt, or ue4ss/Mods/<Name>/enabled.txt
//     present in the snapshot (NativeMods::parse_mods_txt, the #9 parser);
//   * a mod is a C# mod when its folder <mods_dir>/<Name>/ has a csharp/
//     sub-directory on disk (the snapshot cannot carry assemblies);
//   * this runtime step can execute exactly one managed image: the built-in
//     C# probe embedded in the PRX. An enabled C# mod whose csharp/ holds a
//     .dll byte-identical to that image requests the probe; any other
//     assembly is reported and NOT run (no arbitrary C# loading yet).
// Mono is initialized only when probe_requested is true. With no enabled C#
// mod the runtime never calls into Mono: no init, no signal changes, no
// threads, no mappings (see the caller in UE4SSProgram.cpp).
// Only non-throwing std::filesystem overloads are used (fs_throw_gate). Every
// skip is one log line; nothing here throws for bad input.
#pragma once

#include <Platform/NativeCppModInventory.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace RC::Cap10Mods
{
    inline constexpr const char* k_csharp_dir = "csharp";
    inline constexpr std::size_t k_max_mod_dirs = 256;     // same bound as #9
    inline constexpr std::size_t k_max_assemblies = 64;    // per C# mod

    struct CSharpPlan
    {
        std::vector<std::string> csharp_mods; // enabled C# mods, in decision order
        std::vector<std::string> notes;       // one log line each
        std::size_t dirs_scanned{};
        std::size_t dirs_with_csharp{};
        std::size_t assemblies_checked{};
        bool probe_requested{};               // the only condition that starts Mono
    };

    inline auto ascii_ends_with_dll(std::string_view n) -> bool
    {
        if (n.size() < 5) return false;
        const auto tail = n.substr(n.size() - 4);
        return tail[0] == '.' && (tail[1] | 0x20) == 'd' && (tail[2] | 0x20) == 'l' && (tail[3] | 0x20) == 'l';
    }

    // Byte-exact comparison of one file with the built-in image. 1 = equal,
    // 0 = different, -1 = could not be read. No exceptions, bounded read.
    inline auto file_equals_image(const std::filesystem::path& file, const std::uint8_t* image, std::size_t image_size) -> int
    {
        std::FILE* f = std::fopen(file.c_str(), "rb");
        if (!f) return -1;
        int result = 1;
        std::uint8_t buffer[512];
        std::size_t offset = 0;
        for (;;)
        {
            const std::size_t got = std::fread(buffer, 1, sizeof(buffer), f);
            if (got == 0) break;
            if (offset + got > image_size || std::memcmp(buffer, image + offset, got) != 0)
            {
                result = 0;
                break;
            }
            offset += got;
        }
        if (result == 1 && (std::ferror(f) != 0)) result = -1;
        if (result == 1 && offset != image_size) result = 0;
        std::fclose(f);
        return result;
    }

    // Sub-directories of mods_dir that contain a csharp/ directory (names
    // only, sorted bytewise). Any OS error -> false (no C# mod at all).
    inline auto list_dirs_with_csharp(const std::filesystem::path& mods_dir, std::vector<std::string>& names, std::size_t& scanned, std::string& why) -> bool
    {
        names.clear();
        scanned = 0;
        std::error_code ec;
        std::filesystem::directory_iterator it(mods_dir, ec);
        if (ec)
        {
            why = "cannot list the mods directory (" + ec.message() + ")";
            return false;
        }
        const std::filesystem::directory_iterator end;
        while (it != end)
        {
            if (++scanned > k_max_mod_dirs)
            {
                why = "mods directory has more than " + std::to_string(k_max_mod_dirs) + " entries";
                names.clear();
                return false;
            }
            const auto entry_path = it->path();
            std::error_code dir_ec;
            if (it->is_directory(dir_ec) && !dir_ec)
            {
                std::error_code cs_ec;
                if (std::filesystem::is_directory(entry_path / k_csharp_dir, cs_ec) && !cs_ec) names.push_back(entry_path.filename().string());
            }
            it.increment(ec);
            if (ec)
            {
                why = "listing the mods directory failed (" + ec.message() + ")";
                names.clear();
                return false;
            }
        }
        std::sort(names.begin(), names.end());
        return true;
    }

    // The .dll files in <mod_dir>/csharp/ (regular files only, sorted).
    inline auto list_assemblies(const std::filesystem::path& csharp_dir, std::vector<std::filesystem::path>& files, std::string& why) -> bool
    {
        files.clear();
        std::error_code ec;
        std::filesystem::directory_iterator it(csharp_dir, ec);
        if (ec)
        {
            why = "cannot list csharp/ (" + ec.message() + ")";
            return false;
        }
        const std::filesystem::directory_iterator end;
        std::size_t seen = 0;
        while (it != end)
        {
            if (++seen > k_max_assemblies)
            {
                why = "csharp/ has more than " + std::to_string(k_max_assemblies) + " entries";
                files.clear();
                return false;
            }
            const auto entry_path = it->path();
            std::error_code file_ec;
            if (it->is_regular_file(file_ec) && !file_ec && ascii_ends_with_dll(entry_path.filename().string())) files.push_back(entry_path);
            it.increment(ec);
            if (ec)
            {
                why = "listing csharp/ failed (" + ec.message() + ")";
                files.clear();
                return false;
            }
        }
        std::sort(files.begin(), files.end());
        return true;
    }

    // Pure decision: which of the on-disk C# mod folders are enabled by the
    // snapshot (same rule and order as #9: mods.txt rows, then enabled.txt).
    inline auto enabled_csharp_mods(std::optional<std::string_view> mods_txt, const std::function<bool(std::string_view)>& has_enabled_marker,
                                    const std::vector<std::string>& dirs_with_csharp, std::vector<std::string>& notes) -> std::vector<std::string>
    {
        std::vector<std::string> usable;
        for (const auto& n : dirs_with_csharp)
        {
            if (Platform::NativeMods::valid_cpp_mod_name(n)) usable.push_back(n);
            else notes.push_back("C# mod folder skipped: name is not a plain ASCII mod name");
        }
        std::sort(usable.begin(), usable.end());
        usable.erase(std::unique(usable.begin(), usable.end()), usable.end());
        std::vector<std::string> order;
        auto has = [&](std::string_view n) { return std::binary_search(usable.begin(), usable.end(), std::string(n)); };
        auto already = [&](std::string_view n) { return std::find(order.begin(), order.end(), std::string(n)) != order.end(); };
        if (mods_txt)
        {
            std::vector<std::pair<std::string, bool>> rows;
            std::string why;
            if (!Platform::NativeMods::parse_mods_txt(*mods_txt, rows, why)) notes.push_back("mods.txt ignored for C# mods: " + why);
            for (const auto& [name, on] : rows)
                if (on && has(name) && !already(name)) order.push_back(name);
        }
        for (const auto& n : usable)
        {
            bool marker{};
            try { marker = has_enabled_marker && has_enabled_marker(n); }
            catch (...) { marker = false; notes.push_back("C# mod " + n + ": enabled.txt lookup failed; not used"); }
            if (marker && !already(n)) order.push_back(n);
        }
        for (const auto& n : usable)
            if (!already(n)) notes.push_back("C# mod " + n + " has csharp/ but is not enabled (no mods.txt row ': 1' and no enabled.txt in the activation); not used");
        return order;
    }

    // What UE4SSProgram::setup_mods calls under the snapshot. Refuses (no C#
    // runtime at all; Lua and C++ mods unaffected) on any doubt.
    inline auto plan_csharp_mods(const std::filesystem::path& mods_dir, std::optional<std::string_view> mods_txt,
                                 const std::function<bool(std::string_view)>& has_enabled_marker, const std::uint8_t* probe_image,
                                 std::size_t probe_size) -> CSharpPlan
    {
        CSharpPlan plan;
        if (mods_dir.empty() || !mods_dir.is_absolute())
        {
            plan.notes.push_back("refused: C# mods need the absolute mods directory from the activation root; C# runtime not started");
            return plan;
        }
        if (!probe_image || probe_size == 0)
        {
            plan.notes.push_back("refused: no built-in C# probe image; C# runtime not started");
            return plan;
        }
        std::vector<std::string> names;
        std::string why;
        if (!list_dirs_with_csharp(mods_dir, names, plan.dirs_scanned, why))
        {
            plan.notes.push_back("refused: " + why + "; C# runtime not started");
            return plan;
        }
        plan.dirs_with_csharp = names.size();
        plan.csharp_mods = enabled_csharp_mods(mods_txt, has_enabled_marker, names, plan.notes);
        for (const auto& mod : plan.csharp_mods)
        {
            std::vector<std::filesystem::path> files;
            std::string list_why;
            if (!list_assemblies(mods_dir / mod / k_csharp_dir, files, list_why))
            {
                plan.notes.push_back("C# mod " + mod + ": " + list_why + "; not used");
                continue;
            }
            if (files.empty()) plan.notes.push_back("C# mod " + mod + ": csharp/ has no .dll; nothing to run");
            for (const auto& file : files)
            {
                ++plan.assemblies_checked;
                const int same = file_equals_image(file, probe_image, probe_size);
                const auto shown = mod + "/csharp/" + file.filename().string();
                if (same == 1)
                {
                    plan.probe_requested = true;
                    plan.notes.push_back("C# mod " + mod + ": " + shown + " is the built-in C# probe; probe requested");
                }
                else if (same == 0)
                    plan.notes.push_back("C# mod " + mod + ": " + shown + " not run: only the built-in C# probe can run in this runtime (arbitrary C# assemblies are not supported yet)");
                else
                    plan.notes.push_back("C# mod " + mod + ": " + shown + " could not be read; not run");
            }
        }
        plan.notes.push_back("C# mod scan: " + std::to_string(plan.dirs_scanned) + " entries, " + std::to_string(plan.dirs_with_csharp) + " with csharp/, " +
                             std::to_string(plan.csharp_mods.size()) + " enabled, " + std::to_string(plan.assemblies_checked) + " assemblies checked, probe " +
                             (plan.probe_requested ? "requested" : "not requested"));
        return plan;
    }
} // namespace RC::Cap10Mods
