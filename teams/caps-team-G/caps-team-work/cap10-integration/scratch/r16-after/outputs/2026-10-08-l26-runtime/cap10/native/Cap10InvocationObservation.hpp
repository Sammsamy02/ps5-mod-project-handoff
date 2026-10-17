#pragma once
// SOURCE STAGE ONLY. Same-image native embedding interface; no public import ABI.
// Provider image, actual attached thread/domain and GC contract remain owner obligations.
#include <cstdint>
#if defined(__PROSPERO__) || defined(MODBRIDGE_CAP10_MONO_HOST_VALIDATION)
namespace RC::Cap10Observation
{
    enum class WindowKind : std::uint8_t { RunImports, NoNativeCalls };
    struct WindowToken { const void* provider_tls{}; std::uint64_t serial{}; };
    struct Observation
    {
        WindowKind kind{};
        std::uint8_t abi_entries{}, abi_exits{}, probe_entries{}, probe_exits{};
        std::int32_t expected_cookie{}, probe_cookie{}, abi_result{}, probe_status{}, probe_reply{};
        bool probe_reply_valid{}, poisoned{}, accepted{};
    };
    // Valid caller-owned output storage is required; only null can be checked.
    // Native observations alone cannot establish managed origin or an exception.
    bool begin(WindowKind kind, std::int32_t expected_cookie, WindowToken* token) noexcept;
    // Matching token consumed on success or failure; wrong local token poisons.
    // Caller must reject the whole proof after ANY refusal, not retry finish.
    bool finish(WindowToken token, Observation* observed) noexcept;
    void abort(WindowToken token) noexcept;
}
#endif
