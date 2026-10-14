#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
namespace RC { class CppUserModBase; }
namespace RC::Platform
{
    class CppModModule;
    // A caller-selected loadlist entry, not filesystem discovery. The actual
    // native loader still validates the binary; no PE/ABI translation occurs.
    struct NativeCppModSpec
    {
        std::string name;
        std::string path; // logical ue4ss/Mods/<name> directory
        std::filesystem::path module_path; // exact absolute selected file
        std::uint64_t expected_bytes{};
    };
    // Shared loader ownership survives CppMod uninstall/reinstall.
    // Every object/callback from the module must be quiescent before its last
    // owner is released. The UE4SS host and its exports must remain resident.
    class NativeCppMod final
    {
    public:
        using Start = CppUserModBase* (*)();
        using Uninstall = void (*)(CppUserModBase*);
        ~NativeCppMod();
        NativeCppMod(const NativeCppMod&) = delete;
        auto name() const noexcept -> const std::string& { return m_spec.name; }
        auto path() const noexcept -> const std::string& { return m_spec.path; }
        auto module_path() const noexcept -> const std::filesystem::path& { return m_spec.module_path; }
        auto start_function() const -> Start;
        auto uninstall_function() const -> Uninstall;
    private:
        friend class NativeModSource;
        static auto Prepare(NativeCppModSpec) -> std::shared_ptr<const NativeCppMod>;
        explicit NativeCppMod(NativeCppModSpec);
        auto Retire() const -> void;
        NativeCppModSpec m_spec;
        mutable std::unique_ptr<CppModModule> m_module;
        mutable bool m_retired{};
        Start m_start{};
        Uninstall m_uninstall{};
    };
}
