#pragma once
// Real Mono embedding owner, not an implementation of the missing target port.
#include "InvocationBoundary.hpp"
#if defined(__PROSPERO__) || defined(MODBRIDGE_CAP10_MONO_HOST_VALIDATION)
namespace RC::Cap10Host
{
    enum class Stage : unsigned { NotStarted, ExistingRuntime, InitFailed, ResolverFailed,
                                  ImageFailed, AssemblyIdentityFailed, MethodsFailed, Invoked, SignalStateFailed };
    struct Result
    {
        Cap10Boundary::Report calls{};
        Stage stage{Stage::NotStarted};
        bool initialized{}, exact_image{}, resolver_load{}, resolver_abi{}, resolver_probe{}, completed{};
    };
    // Sole process-wide Mono owner. Runs synchronously in actual ready admission.
    // Parent module, initialized interpreter/GC/thread/TLS/EH port and matching
    // runtime corlib stay retained until this entire call and cleanup return.
    // No other Mono users/owners permitted. Runtime initialization is one-shot.
    // Caller must exclusively own changes to Mono-affected signal dispositions
    // from entry through cleanup, and keep this same init thread's signal mask
    // under this owner's control. Mono cleanup restores standard actions itself;
    // concurrent handler changes, profiling/crash-handler installation and
    // independent signal-mask changes during the call are unsupported.
    // SIGPIPE is restored only while it still matches the observed Mono IGN action;
    // an identifiable replacement is retained and completion is refused. POSIX
    // sigaction has no atomic compare-and-set; this check does not provide locking.
    // Requires matching Mono saved-action lifetime fix before enabling chaining.
    // Restoration evidence applies after successful init/cleanup; a failed or
    // fatal partial init is not a proved recoverable state. No cleanup on null domain.
    // Requires the exact reserved-owned-loader source patch; no ambient resolution.
    // mono_set_dirs takes the assembly prefix (PREFIX/lib) and configuration
    // prefix (PREFIX/etc), not PREFIX/lib/mono/4.5 or PREFIX/etc/mono.
    Result run_embedded_probe_once() noexcept;
}
#endif
