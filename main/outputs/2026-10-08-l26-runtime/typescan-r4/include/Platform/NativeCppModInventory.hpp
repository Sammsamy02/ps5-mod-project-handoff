// NativeCppModInventory.hpp -- which native C++ mods start under the activation
// snapshot (#9, review finding B1). Engine-free, header-only, host-tested.
//
// Why this exists: on the console the runtime always runs with an activation
// snapshot (context.mod_source). In that branch UE4SSProgram::setup_mods built
// only Lua mods and returned, NativeModSource lists only mods that have
// Scripts/main.lua, and start_mods<CppMod> returned without starting anything,
// so no C++ mod could ever be created. The snapshot cannot carry the PRX itself
// (the activation refuses every file role except lua_mod/config/script, and
// sceKernelLoadStartModule needs a real path anyway), so:
//   * WHICH mods are enabled comes from the snapshot only, with the upstream
//     UE4SS rule: "Name : 1" rows of ue4ss/Mods/mods.txt in file order, then
//     mods whose ue4ss/Mods/<Name>/enabled.txt is in the snapshot, in bytewise
//     name order (enabled.txt also enables a mods.txt-disabled mod);
//   * WHERE the binary is comes from the real mods directory (the absolute
//     <root>/ue4ss/Mods that R11 already logs as "mods directories: [0] ..."):
//     a sub-directory counts only if it has a dlls/ directory.
// Only non-throwing std::filesystem overloads are used (fs_throw_gate). Every
// skip is reported as one log line; nothing here throws for bad input.
#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace RC::Platform::NativeMods
{
    struct CppModCandidate
    {
        std::string name;              // ASCII mod folder name
        std::filesystem::path mod_dir; // absolute <mods_dir>/<name>
    };

    struct CppModInventory
    {
        std::vector<CppModCandidate> start_order; // enabled + has dlls/, in startup order
        std::vector<std::string> notes;           // one log line each
        size_t dirs_scanned{};
        size_t dirs_with_dlls{};
    };

    inline constexpr size_t k_max_cpp_mod_dirs = 256;
    inline constexpr size_t k_max_cpp_mod_name = 128;

    // Printable ASCII only (the runtime widens names byte by byte), no path
    // separators, no '.'/'..', not "shared" (UE4SS reserves it).
    inline auto valid_cpp_mod_name(std::string_view n) -> bool
    {
        if (n.empty() || n.size() > k_max_cpp_mod_name || n == "." || n == "..") return false;
        for (unsigned char c : n)
            if (c < 0x21 || c > 0x7e || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                return false;
        std::string folded(n);
        for (auto& c : folded)
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        return folded != "shared";
    }

    // mods.txt in the same dialect NativeModSource accepts for Lua mods
    // (BOM, CRLF, ';' comment lines, spaces/tabs removed, "Name:0|1").
    // Returns false (and why) for a malformed file; then no row is used.
    inline auto parse_mods_txt(std::string_view text, std::vector<std::pair<std::string, bool>>& rows, std::string& why) -> bool
    {
        rows.clear();
        if (text.size() >= 3 && text.substr(0, 3) == "\xef\xbb\xbf") text.remove_prefix(3);
        if (text.find('\0') != text.npos)
        {
            why = "mods.txt contains NUL";
            return false;
        }
        while (!text.empty())
        {
            const auto end = text.find('\n');
            std::string line(text.substr(0, end));
            text = end == text.npos ? std::string_view{} : text.substr(end + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.find(';') != line.npos) continue;
            line.erase(std::remove_if(line.begin(), line.end(), [](char c) { return c == ' ' || c == '\t'; }), line.end());
            if (line.empty()) continue;
            const auto colon = line.find(':');
            if (colon == line.npos || colon == 0 || colon + 2 != line.size() || (line.back() != '0' && line.back() != '1'))
            {
                why = "mods.txt contains a malformed row";
                rows.clear();
                return false;
            }
            rows.emplace_back(line.substr(0, colon), line.back() == '1');
        }
        return true;
    }

    // Sub-directories of mods_dir that contain a dlls/ directory (names only,
    // sorted bytewise). Non-throwing overloads only; any OS error -> false.
    inline auto list_dirs_with_dlls(const std::filesystem::path& mods_dir, std::vector<std::string>& names, size_t& scanned, std::string& why) -> bool
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
            if (++scanned > k_max_cpp_mod_dirs)
            {
                why = "mods directory has more than " + std::to_string(k_max_cpp_mod_dirs) + " entries";
                names.clear();
                return false;
            }
            const auto entry_path = it->path();
            std::error_code dir_ec;
            if (it->is_directory(dir_ec) && !dir_ec)
            {
                std::error_code dlls_ec;
                const auto name = entry_path.filename().string();
                if (std::filesystem::is_directory(entry_path / "dlls", dlls_ec) && !dlls_ec) names.push_back(name);
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

    // Pure decision: given the snapshot facts and the directory facts, which
    // mods start and in which order.
    inline auto plan_cpp_mods(const std::filesystem::path& mods_dir, std::optional<std::string_view> mods_txt,
                              const std::function<bool(std::string_view)>& has_enabled_marker, const std::vector<std::string>& dirs_with_dlls)
        -> CppModInventory
    {
        CppModInventory inv;
        inv.dirs_with_dlls = dirs_with_dlls.size();
        std::vector<std::string> usable;
        for (const auto& n : dirs_with_dlls)
        {
            if (valid_cpp_mod_name(n)) usable.push_back(n);
            else inv.notes.push_back("C++ mod folder skipped: name is not a plain ASCII mod name");
        }
        std::sort(usable.begin(), usable.end());
        usable.erase(std::unique(usable.begin(), usable.end()), usable.end());
        auto has_dlls = [&](std::string_view n) { return std::binary_search(usable.begin(), usable.end(), std::string(n)); };
        auto already = [&](std::string_view n) {
            return std::any_of(inv.start_order.begin(), inv.start_order.end(), [&](const CppModCandidate& c) { return c.name == n; });
        };
        auto add = [&](std::string_view n) {
            if (!already(n)) inv.start_order.push_back({std::string(n), mods_dir / std::string(n)});
        };
        if (mods_txt)
        {
            std::vector<std::pair<std::string, bool>> rows;
            std::string why;
            if (!parse_mods_txt(*mods_txt, rows, why)) inv.notes.push_back("mods.txt ignored for C++ mods: " + why);
            for (const auto& [name, on] : rows)
                if (on && has_dlls(name)) add(name);
        }
        for (const auto& n : usable)
        {
            bool marker{};
            try { marker = has_enabled_marker && has_enabled_marker(n); }
            catch (...) { marker = false; inv.notes.push_back("C++ mod " + n + ": enabled.txt lookup failed; not started"); }
            if (marker) add(n);
        }
        for (const auto& n : usable)
            if (!already(n)) inv.notes.push_back("C++ mod " + n + " has dlls/ but is not enabled (no mods.txt row ': 1' and no enabled.txt in the activation); not loaded");
        return inv;
    }

    // What UE4SSProgram::setup_mods calls under the snapshot. Refuses (no C++
    // mod at all, Lua mods unaffected) when the mods directory is not an
    // absolute path or cannot be listed.
    inline auto discover_cpp_mods(const std::filesystem::path& mods_dir, std::optional<std::string_view> mods_txt,
                                  const std::function<bool(std::string_view)>& has_enabled_marker) -> CppModInventory
    {
        CppModInventory inv;
        if (mods_dir.empty() || !mods_dir.is_absolute())
        {
            inv.notes.push_back("refused: C++ mods need the absolute mods directory from the activation root; none loaded");
            return inv;
        }
        std::vector<std::string> names;
        size_t scanned{};
        std::string why;
        if (!list_dirs_with_dlls(mods_dir, names, scanned, why))
        {
            inv.notes.push_back("refused: " + why + "; no C++ mod loaded");
            inv.dirs_scanned = scanned;
            return inv;
        }
        inv = plan_cpp_mods(mods_dir, mods_txt, has_enabled_marker, names);
        inv.dirs_scanned = scanned;
        inv.notes.push_back("C++ mod scan: " + std::to_string(scanned) + " entries, " + std::to_string(inv.dirs_with_dlls) + " with dlls/, " +
                            std::to_string(inv.start_order.size()) + " enabled");
        return inv;
    }
} // namespace RC::Platform::NativeMods
