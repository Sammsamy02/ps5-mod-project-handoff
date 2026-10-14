#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>

namespace RC::Platform {
// Owns a native module in this process. This is not a Windows PE loader.
// Configure/use/close serially. No code or object from this module may survive Close.
class CppModModule final {
public:
    static auto Open(const std::filesystem::path&) -> std::unique_ptr<CppModModule>;
    ~CppModModule() noexcept;
    CppModModule(const CppModModule&) = delete;
    auto operator=(const CppModModule&) -> CppModModule& = delete;
    auto FindExport(std::string_view) const -> void*;
    // Prospero: a failed unload retains the handle and disables further use;
    // repeated Close reports that failure without retrying the native call.
    // POSIX: consumes its dlopen owner once, including on a dlclose error.
    auto Close() -> void;
private:
    CppModModule() = default;
#ifdef __PROSPERO__
    enum class NativeState { Empty, Loaded, RetainedAfterFailedUnload };
    NativeState m_native_state{NativeState::Empty};
    std::uint32_t m_native_handle{}; // Zero is a valid loaded module handle.
    int m_native_unload_error{};
#else
    void* m_handle{};
#endif
};
}
