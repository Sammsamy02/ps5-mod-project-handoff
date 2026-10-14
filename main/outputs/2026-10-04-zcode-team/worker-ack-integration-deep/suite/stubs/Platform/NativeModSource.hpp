// Host-test stub of the canonical port NativeModSource surface consumed by
// activated_mod_source.cpp. Prepare retains the supplied buffers; read()
// answers logical-key lookups so the activation views are genuinely served.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace RC::Platform
{
    struct NativeFileBuffer
    {
        std::shared_ptr<const void> owner;
        std::string_view bytes;
    };
    struct NativeModFile
    {
        std::string path;
        NativeFileBuffer buffer;
    };
    struct NativeCppModSpec
    {
        std::string name;
        std::string logical_path;
        std::filesystem::path module_path;
        uint64_t size{};
    };
    struct NativeLuaMod
    {
        std::string name;
        std::string path;
        std::string scripts_path;
    };

    class NativeModSource final
    {
        std::vector<NativeModFile> m_files;
        std::vector<NativeLuaMod> m_lua_mods;
        std::vector<std::string> m_startup_mod_names;
    public:
        static auto Prepare(std::vector<NativeModFile> files, std::vector<NativeCppModSpec> specs)
            -> std::shared_ptr<const NativeModSource>
        {
            (void)specs; // No host CppMod loader; the specs remain validated inputs.
            auto source = std::make_shared<NativeModSource>();
            for (auto& file : files)
            {
                const std::string prefix = "ue4ss/Mods/";
                if (file.path.starts_with(prefix) &&
                    file.path.substr(prefix.size()).find("/Scripts/main.lua") != std::string::npos)
                {
                    const auto name = file.path.substr(prefix.size(),
                        file.path.substr(prefix.size()).find('/'));
                    source->m_lua_mods.push_back({name, file.path,
                        file.path.substr(0, file.path.find("/Scripts/main.lua")) + "/Scripts"});
                    source->m_startup_mod_names.push_back(name);
                }
                source->m_files.push_back(std::move(file));
            }
            return source;
        }
        auto retire_cpp_mods() const -> void {}
        auto read(std::string_view path) const -> std::optional<NativeFileBuffer>
        {
            for (const auto& file : m_files)
                if (file.path == path) return file.buffer;
            return std::nullopt;
        }
        auto lua_mods() const noexcept -> const std::vector<NativeLuaMod>& { return m_lua_mods; }
        auto startup_mod_names() const noexcept -> const std::vector<std::string>&
        {
            return m_startup_mod_names;
        }
    };
}
