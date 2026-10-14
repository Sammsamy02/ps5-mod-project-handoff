#pragma once
#include <filesystem>
#include <memory>
#include <string_view>

namespace RC::Platform {
// Owns a native module in this process. This is not a Windows PE loader.
// Configure/use/close this wrapper serially. Successfully acquired code references
// remain pinned for the loaded process lifetime (bounded 64 acquisitions); Close
// only disables this wrapper. There is no unload/retirement authority here.
// This does not retain callback userdata, Program objects or external workers.
class CppModModule final {
public:
    static auto Open(const std::filesystem::path&) -> std::unique_ptr<CppModModule>;
    ~CppModModule() noexcept;
    CppModModule(const CppModModule&) = delete;
    auto operator=(const CppModModule&) -> CppModModule& = delete;
    auto FindExport(std::string_view) const -> void*;
    // Drops wrapper lookup capability; retained code is never passed to dlclose.
    auto Close() -> void;
private:
    CppModModule() = default;
    void* m_handle{};
};
}
