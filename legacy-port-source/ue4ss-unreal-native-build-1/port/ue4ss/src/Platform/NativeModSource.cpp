#include <Platform/NativeModSource.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <Platform/NativeCppModPath.hpp>
#include <stdexcept>

namespace RC::Platform
{
    namespace
    {
        void validate_path(std::string_view path)
        {
            if (!path.starts_with("ue4ss/") || path.size() > 4096 || path.back() == '/')
                throw std::invalid_argument("Native mod file requires a relative key below ue4ss/");
            for (unsigned char c : path)
                if (c < 0x20 || c == 0x7f || c == '\\' || c == ':')
                    throw std::invalid_argument("Native mod file key contains an invalid character");
            // UE4SS converts names to UTF-16. Reject malformed input rather than
            // allowing replacement characters to merge distinct byte keys.
            for (size_t i = 0; i < path.size();)
            {
                const auto lead = static_cast<unsigned char>(path[i++]);
                if (lead < 0x80) continue;
                const unsigned count = lead >= 0xc2 && lead <= 0xdf ? 1 :
                    lead >= 0xe0 && lead <= 0xef ? 2 : lead >= 0xf0 && lead <= 0xf4 ? 3 : 0;
                if (!count || count > path.size() - i)
                    throw std::invalid_argument("Native mod file key is not UTF-8");
                const auto first = static_cast<unsigned char>(path[i]);
                if ((lead == 0xe0 && first < 0xa0) || (lead == 0xed && first >= 0xa0) ||
                    (lead == 0xf0 && first < 0x90) || (lead == 0xf4 && first >= 0x90))
                    throw std::invalid_argument("Native mod file key is not UTF-8");
                for (unsigned n = 0; n < count; ++n)
                    if ((static_cast<unsigned char>(path[i++]) & 0xc0) != 0x80)
                        throw std::invalid_argument("Native mod file key is not UTF-8");
            }
            size_t offset = 0;
            while (offset < path.size())
            {
                const auto end = path.find('/', offset);
                const auto part = path.substr(offset, end == path.npos ? path.size() - offset : end - offset);
                if (part.empty() || part == "." || part == "..")
                    throw std::invalid_argument("Native mod file key is not canonical");
                if (end == path.npos) break;
                offset = end + 1;
            }
        }
    }

    auto NativeModSource::read(std::string_view path) const -> std::optional<NativeFileBuffer>
    {
        validate_path(path);
        const auto found = std::lower_bound(m_files.begin(), m_files.end(), path,
            [](const NativeModFile& file, std::string_view key) { return file.path < key; });
        if (found == m_files.end() || found->path != path) return std::nullopt;
        return found->buffer;
    }

    auto NativeModSource::retire_cpp_mods() const -> void
    {
        for (const auto& mod : m_cpp_mods) mod->Retire();
    }

    NativeModSource::NativeModSource(std::vector<NativeModFile> files)
        : NativeModSource(std::move(files), {}) {}

    auto NativeModSource::Prepare(std::vector<NativeModFile> files, std::vector<NativeCppModSpec> specs)
        -> std::shared_ptr<const NativeModSource>
    {
        auto source = std::shared_ptr<NativeModSource>(new NativeModSource(std::move(files), specs));
        std::sort(specs.begin(), specs.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
        source->m_cpp_mods.reserve(specs.size());
        for (auto& spec : specs) source->m_cpp_mods.push_back(NativeCppMod::Prepare(std::move(spec)));
        return source;
    }

    NativeModSource::NativeModSource(std::vector<NativeModFile> files,
                                   const std::vector<NativeCppModSpec>& specs) : m_files(std::move(files))
    {
        if (m_files.size() > 4096) throw std::invalid_argument("Native mod snapshot exceeds file limit");
        size_t total = 0;
        for (const auto& file : m_files)
        {
            validate_path(file.path);
            if (!file.buffer.owner || (!file.buffer.bytes.data() && !file.buffer.bytes.empty()))
                throw std::invalid_argument("Native mod file requires an owning immutable buffer");
            if (file.buffer.bytes.size() > 8u * 1024u * 1024u)
                throw std::invalid_argument("Native mod file exceeds byte limit");
            total += file.buffer.bytes.size();
            if (total > 64u * 1024u * 1024u)
                throw std::invalid_argument("Native mod snapshot exceeds byte limit");
        }
        std::sort(m_files.begin(), m_files.end(), [](const auto& left, const auto& right) { return left.path < right.path; });
        for (size_t i = 1; i < m_files.size(); ++i)
            if (m_files[i - 1].path == m_files[i].path)
                throw std::invalid_argument("Native mod snapshot contains a duplicate file key");

        // A loadlist package id is deliberately not used as a Lua directory name.
        std::map<std::string, std::string> script_roots;
        constexpr std::string_view prefix = "ue4ss/Mods/";
        for (const auto& file : m_files)
        {
            if (!std::string_view(file.path).starts_with(prefix)) continue;
            auto suffix = std::string_view(file.path).substr(prefix.size());
            const auto split = suffix.find('/');
            if (split == suffix.npos) continue;
            std::string name(suffix.substr(0, split));
            suffix.remove_prefix(split + 1);
            const auto script_end = suffix.find('/');
            if (script_end == suffix.npos) continue;
            const auto scripts = suffix.substr(0, script_end);
            if (scripts != "Scripts" && scripts != "scripts") continue;
            auto [found, inserted] = script_roots.emplace(name, scripts);
            if (!inserted && found->second != scripts)
                throw std::invalid_argument("Native mod has ambiguous Scripts/scripts directories");
        }
        for (const auto& [name, scripts] : script_roots)
        {
            auto folded_name = name;
            for (auto& c : folded_name) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
            if (folded_name == "shared") continue;
            const std::string path = std::string(prefix) + name;
            const std::string scripts_path = path + "/" + scripts;
            if (read(scripts_path + "/main.lua"))
                m_lua_mods.push_back({name, path, scripts_path});
        }
        std::set<std::string, std::less<>> mod_names, native_names;
        for (const auto& mod : m_lua_mods) mod_names.insert(mod.name);
        if (specs.size() > 4096 || specs.size() + m_files.size() > 4096)
            throw std::invalid_argument("Native mod inventory exceeds file limit");
        for (const auto& spec : specs)
        {
            validate_path(spec.path + "/dlls/main.prx");
            if (spec.path != std::string(prefix) + spec.name ||
                spec.name.find('/') != spec.name.npos || !native_names.insert(spec.name).second)
                throw std::invalid_argument("Native C++ inventory has an invalid or duplicate mod name");
            auto folded = spec.name;
            for (auto& c : folded) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
            if (folded == "shared") throw std::invalid_argument("Native C++ mod name is reserved");
            const auto& path = spec.module_path;
            const auto& bytes = path.native();
            if (!path.is_absolute() || path.lexically_normal() != path || bytes.size() > 1023 ||
                bytes.find('\0') != bytes.npos || spec.expected_bytes < 2)
                throw std::invalid_argument("Native C++ module path/size is invalid");
            const auto candidates = NativeCppModCandidates(spec.name);
            if (std::find(candidates.begin(), candidates.end(), path.filename().string()) == candidates.end())
                throw std::invalid_argument("Native C++ module filename is unsupported");
            if (!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) != spec.expected_bytes)
                throw std::invalid_argument("Native C++ module is missing or its listed size changed");
            mod_names.insert(spec.name);
        }
        auto enable = [&](std::string_view name) {
            if (mod_names.contains(name) &&
                std::find(m_startup_mod_names.begin(), m_startup_mod_names.end(), name) == m_startup_mod_names.end())
                m_startup_mod_names.emplace_back(name);
        };
        if (const auto mods = read("ue4ss/Mods/mods.txt"))
        {
            auto text = mods->bytes;
            if (text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
            if (text.find('\0') != text.npos) throw std::invalid_argument("Native mods.txt contains NUL");
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
                if (colon == line.npos || colon == 0 || colon + 2 != line.size() ||
                    (line.back() != '0' && line.back() != '1'))
                    throw std::invalid_argument("Native mods.txt contains a malformed enabled row");
                if (line.back() == '1') enable(std::string_view(line).substr(0, colon));
            }
        }
        for (const auto& name : mod_names)
            if (read(std::string(prefix) + name + "/enabled.txt")) enable(name);
    }
}
