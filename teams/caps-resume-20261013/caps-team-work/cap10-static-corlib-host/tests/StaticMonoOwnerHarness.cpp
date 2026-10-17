// Real matching Mono HOST VALIDATION. No UE4SS readiness or strict owner patch claim.
#include "InvocationBoundary.hpp"
#include "ProbeImage.hpp"
#include <mono/mini/jit.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>
#include <mono/metadata/image.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/object.h>
#include "MonoGcRegionApi.hpp"
#include <cstdio>
#include <cstdlib>
extern "C" void cap10_host_begin(std::int32_t, std::int32_t);
extern "C" std::int32_t cap10_host_read(std::int32_t);
namespace RC::Cap10Observation
{
    // HOST-ONLY observer: actual C calls in this native host, not engine readiness.
    static bool active;
    static WindowKind kind;
    static std::int32_t cookie;
    static std::uint64_t sequence;
    static int identity;
    bool begin(WindowKind requested, std::int32_t expected, WindowToken* token) noexcept
    {
        if (active || !token) return false;
        active = true; kind = requested; cookie = expected;
        *token = {&identity, ++sequence};
        cap10_host_begin(requested == WindowKind::RunImports ? 1 : 0, expected);
        return true;
    }
    bool finish(WindowToken token, Observation* out) noexcept
    {
        if (!active || !out || token.provider_tls != &identity || token.serial != sequence) return false;
        active = false;
        *out = {};
        out->kind = kind;out->expected_cookie = cookie;
        out->abi_entries = static_cast<std::uint8_t>(cap10_host_read(0));
        out->abi_exits = static_cast<std::uint8_t>(cap10_host_read(5));
        out->probe_entries = static_cast<std::uint8_t>(cap10_host_read(1));
        out->probe_exits = static_cast<std::uint8_t>(cap10_host_read(6));
        out->probe_cookie = cap10_host_read(3);out->probe_reply = cap10_host_read(4);
        out->abi_result = cap10_host_read(7);out->probe_status = cap10_host_read(8);out->probe_reply_valid = cap10_host_read(9) == 1;
        out->accepted = cap10_host_read(2) == 0 && (kind == WindowKind::NoNativeCalls ?
            out->abi_entries == 0 && out->abi_exits == 0 && out->probe_entries == 0 && out->probe_exits == 0 :
            out->abi_entries == 1 && out->abi_exits == 1 && out->probe_entries == 1 && out->probe_exits == 1 &&
            out->abi_result == 1 && out->probe_status == 0 && out->probe_reply_valid && out->probe_cookie == cookie && out->probe_reply == cookie);
        return out->accepted;
    }
    void abort(WindowToken token) noexcept
    {
        if (token.provider_tls == &identity && token.serial == sequence) active = false;
    }
}

#include "MonoOwner.hpp"
// Pinned upstream emits a pointer variable; its value is the real file-info address.
// This matches the upstream generated-driver declaration and public jit.h API.
extern "C" void* mono_aot_module_mscorlib_info;
extern "C" void diagnostic_event(const char* tag, unsigned value)
{ std::printf("%s %u\n", tag, value); }
int main()
{
    mono_aot_register_module(static_cast<void**>(mono_aot_module_mscorlib_info));
    const auto result = RC::Cap10Host::run_embedded_probe_once();
    std::printf("{\"verdict\":\"%s\",\"stage\":%u,\"completed\":%s,\"real_managed_runs\":%u,\"real_managed_exceptions\":%u,\"run_passed\":%s,\"throw_passed\":%s,\"recovery_passed\":%s,\"aot_interp_mode\":true,\"host_only_observer\":true,\"patched_loader\":true,\"static_corlib_registered\":true,\"townfall_proof\":false}\n",
        result.completed ? "REAL_MONO_OWNER_STATIC_CORLIB_HOST_PASS" : "REAL_MONO_OWNER_STATIC_CORLIB_HOST_FAIL",
        static_cast<unsigned>(result.stage), result.completed ? "true" : "false",
        unsigned(result.calls.first.invoked) + unsigned(result.calls.recovery.invoked),
        unsigned(result.calls.deliberate.had_exception),
        result.calls.first.passed ? "true" : "false", result.calls.deliberate.passed ? "true" : "false",
        result.calls.recovery.passed ? "true" : "false");
    return result.completed ? 0 : 1;
}
