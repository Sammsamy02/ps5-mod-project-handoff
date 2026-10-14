#pragma once
// Same-image invocation helper; this is not an implementation of the target port.
#include <cstdint>
#include <mono/metadata/loader.h>
#include "Cap10InvocationObservation.hpp" // Actual reviewed same-image observer header.
#if defined(__PROSPERO__) || defined(MODBRIDGE_CAP10_MONO_HOST_VALIDATION)
namespace RC::Cap10Boundary
{
    struct RunReport
    {
        Cap10Observation::Observation native{};
        std::int32_t value{};
        bool invoked{}, had_exception{}, native_accepted{}, int32_valid{}, passed{};
    };
    struct ThrowReport
    {
        Cap10Observation::Observation native{};
        bool invoked{}, had_exception{}, native_accepted{}, exact_exception{}, passed{};
    };
    struct Report
    {
        RunReport first{}, recovery{};
        ThrowReport deliberate{};
        bool inputs_associated{}, completed{}, unexpected_cpp_exception{};
    };
    // HARD PRECONDITIONS: genuine initialized Mono; current thread attached to
    // retained domain/image; admitted conservative native stack/register rooting;
    // real GC/TLS/PInvoke/unwind port; exact loaded artifact association; actual
    // signatures are verified with actual public metadata APIs before invocation.
    // Supplied resolved methods must be those exact admitted methods; this helper
    // cannot validate arbitrary Mono pointers. Loaded image identity is owned by MonoOwner.
    // Actual ready callback and provider/domain/image retained through all cleanup.
    // Fresh cookies recorded by real owner; no detach/unload/shutdown during call.
    // Native observations alone do not prove managed origin or game continuation.
    // Actual Mono public signature checks; caller already owns live methods/GC context.
    bool signatures_admitted(MonoMethod* run, MonoMethod* throw_method);
    Report invoke_boundaries(MonoImage* image, MonoMethod* run, MonoMethod* throw_method,
                             std::int32_t first_cookie, std::int32_t recovery_cookie) noexcept;
}
#endif
