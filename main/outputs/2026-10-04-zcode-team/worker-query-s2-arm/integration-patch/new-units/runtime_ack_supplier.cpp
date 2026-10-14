// runtime_ack_supplier — module_start RuntimeAckConfig supplier (composition-4
// extra unit). See runtime_ack_supplier.hpp for the contract. Facts:
//  - getpid(): precedent inside this very closure is composition-2's
//    lmf-sce-ops (live_module_os_ops_sce.cpp:108,210); measured in the frozen
//    import contract (WIRING-SPEC S2).
//  - clock_gettime(CLOCK_MONOTONIC) -> milliseconds: byte-for-byte the
//    semantics of the vendored component's own posix_now_ms
//    (worker-installer-ps5io/vendored/modforge_activation.c); measured in the
//    frozen import contract (WIRING-SPEC S2).
//  - status_root MUST be the exact mfa_config::root string (S3): the vendored
//    mfa_status_open joins runtime-status.txt under it, which is precisely the
//    path RUNTIME-ACK.md section 1 defines and the e2e installer records read.
//    Never the activation_root.

#include "runtime_ack_supplier.hpp"

#include <ctime>
#include <unistd.h>
#include <utility>

#ifndef NATIVE_ACK_SUPPLIER_HOST_TEST
#include "ue4ss_backend.hpp"
#include <NativeQueryHarness.hpp>
#endif

namespace modbridge::native
{
namespace
{
// Same shape as the vendored posix_now_ms (CLOCK_MONOTONIC,
// sec*1000 + nsec/1000000). Degraded 0 still records a value in the header.
auto now_ms_monotonic() noexcept -> std::uint64_t
{
    timespec ts{};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0;
    }
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000u
         + static_cast<std::uint64_t>(ts.tv_nsec) / 1000000u;
}
}

auto MakeRuntimeAckConfig(const char* mod_root) -> RuntimeAckConfig
{
    RuntimeAckConfig runtime_ack;
    if (mod_root != nullptr && *mod_root != '\0')
    {
        runtime_ack.status_root = mod_root;  // the EXACT mfa_config::root string
    }
    runtime_ack.pid = static_cast<long>(getpid());
    runtime_ack.now_ms = now_ms_monotonic();
    return runtime_ack;
}

auto StopRuntimeWithAck(std::unique_ptr<RuntimeOwner>& owner) -> bool
{
    if (!owner)
    {
        return false;
    }
    const bool stopped = owner->stop();
    owner.reset();  // destructor emits at most one further 'stopped' line (none after stop)
    return stopped;
}

#ifndef NATIVE_ACK_SUPPLIER_HOST_TEST
auto StartActivatedRuntimeWithAck(RC::Platform::NativeProgramContext context,
                                  ActivatedMods activation,
                                  std::string_view expected_title,
                                  const char* mod_root)
    -> std::unique_ptr<RuntimeOwner>
{
    // S1: the ONLY RuntimeAckConfig birth; consumed by const& below.
    RuntimeAckConfig runtime_ack = MakeRuntimeAckConfig(mod_root);

    // worker-query-harness-integration S2-ARM.md section 5 prerequisite 1:
    // install the four engine seams exactly once, BEFORE RuntimeOwner::start()
    // spawns the serialized init thread. Host builds refuse the default
    // binding outright; on target it binds IsGameThreadInitialized /
    // IsInGameThreadRaw / GetGameThreadIdRaw / UObjectArray::GetNumElements.
    RC::Platform::QueryHarness::Global().InstallDefaultEngineBindings();

    // The 4-arg ack-enabled prepare (composition-3 object closure): opens the
    // single RuntimeAckSink on <status_root>/runtime-status.txt inside
    // module_start after the activation is validated (WIRING-SPEC S3 timing).
    auto owner = PrepareActivatedUE4SSRuntime(std::move(context),
                                              std::move(activation),
                                              expected_title,
                                              runtime_ack);
    // entry_guard makes double start impossible; false would mean a refused
    // start already reflected in RuntimeStatus - the caller owns that check.
    owner->start();
    return owner;
}
#endif
}
