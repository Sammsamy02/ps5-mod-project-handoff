#pragma once
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <Platform/NativeCppMod.hpp>

namespace RC::Platform
{
    // The returned bytes remain valid while this buffer is retained, even after
    // the source itself is destroyed. The owner must own immutable byte storage.
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
    struct NativeLuaMod
    {
        std::string name;
        std::string path;
        std::string scripts_path;
    };

    // A bounded immutable snapshot of preloaded files. Paths are exact UTF-8
    // logical keys below ue4ss/, never filesystem paths. The caller must bind
    // each view to its actual immutable owner; this API cannot prove that bind.
    class NativeModSource final
    {
        std::vector<NativeModFile> m_files;
        std::vector<NativeLuaMod> m_lua_mods;
        std::vector<std::shared_ptr<const NativeCppMod>> m_cpp_mods;
        std::vector<std::string> m_startup_mod_names;
        NativeModSource(std::vector<NativeModFile>, const std::vector<NativeCppModSpec>&);
    public:
        // Validate the full inventory/configuration before any load, then load
        // every listed C++ mod and resolve its lifecycle exports before return.
        // A failure releases every acquired owner and publishes no source.
        static auto Prepare(std::vector<NativeModFile>, std::vector<NativeCppModSpec>)
            -> std::shared_ptr<const NativeModSource>;
        // Runtime shutdown only: after all callbacks have stopped and every
        // CppMod is uninstalled/destroyed, before host services disappear.
        // Invalidates export access even through external source/record copies.
        // One Program is the exclusive active consumer. External copies must
        // be inert at retirement; already handed-out pointers cannot be revoked.
        // Serialize with every consumer; failure forbids host teardown.
        auto retire_cpp_mods() const -> void;
        auto cpp_mods() const noexcept -> const std::vector<std::shared_ptr<const NativeCppMod>>& { return m_cpp_mods; }
        explicit NativeModSource(std::vector<NativeModFile> files);
        // Invalid/noncanonical keys throw; an absent key returns nullopt.
        auto read(std::string_view path) const -> std::optional<NativeFileBuffer>;
        auto lua_mods() const noexcept -> const std::vector<NativeLuaMod>& { return m_lua_mods; }
        // mods.txt-enabled rows first, then marker-enabled mods in bytewise name
        // order. As in UE4SS, enabled.txt also enables a mods.txt-disabled mod.
        auto startup_mod_names() const noexcept -> const std::vector<std::string>& { return m_startup_mod_names; }
    };
}
