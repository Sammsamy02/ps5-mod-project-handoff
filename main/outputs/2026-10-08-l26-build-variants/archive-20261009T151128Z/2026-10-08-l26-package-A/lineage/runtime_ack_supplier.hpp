#pragma once

// runtime_ack_supplier — the module_start RuntimeAckConfig supplier unit.
// Implements worker-ack-integration-deep WIRING-SPEC S1/S2/S3 exactly:
// the ONLY place a RuntimeAckConfig may be born, pid via getpid(),
// now_ms via CLOCK_MONOTONIC, status_root = the EXACT mfa_config::root
// string the game-bound entry passed to mfa_activate. The ack sink itself is
// opened inside PrepareActivatedUE4SSRuntime (RuntimeAckSink::Open) during
// module_start while /data is reachable; this unit never opens a second one.
//
// Host tests compile this file with -DNATIVE_ACK_SUPPLIER_HOST_TEST: the
// target-only wiring below (which needs ue4ss_backend.hpp /
// NativeQueryHarness.hpp / NativeProgramContext) is excluded, and the
// host-visible helpers are tested against the REAL canonical
// runtime_owner.cpp + vendored modforge_activation.c.

#include <memory>
#include <string_view>

#include "activated_mod_source.hpp"
#include "runtime_owner.hpp"

#ifndef NATIVE_ACK_SUPPLIER_HOST_TEST
// Declaration-only forward reference at file scope; the complete type (and the
// UE4SS port include environment it needs) is pulled in by
// runtime_ack_supplier.cpp via "ue4ss_backend.hpp" BEFORE the definition.
namespace RC::Platform { struct NativeProgramContext; }
#endif

namespace modbridge::native
{
// S1/S2: born here, passed by const& into PrepareActivatedUE4SSRuntime.
// status_root is the EXACT mfa_config::root string (e.g.
// /data/modbridge/PPSA33286); an empty/absent mod_root yields an empty
// status_root, which leaves the session unacknowledged and startup unchanged.
// pid = getpid() (frozen import contract); now_ms = monotonic milliseconds in
// the vendored posix_now_ms shape (0 only if clock_gettime fails).
auto MakeRuntimeAckConfig(const char* mod_root) -> RuntimeAckConfig;

// S1 shutdown half, called by the loader control thread (module_stop):
// owner->stop() joins the init thread and emits at most one 'stopped' line,
// then the owner is destroyed (closes the retained descriptor). Returns
// stop()'s result; returns false for an already-empty owner.
auto StopRuntimeWithAck(std::unique_ptr<RuntimeOwner>& owner) -> bool;

#ifndef NATIVE_ACK_SUPPLIER_HOST_TEST
// Full module_start-side wiring (target composition only). Birth of the ack
// config, harness engine-seam install (exactly once, BEFORE
// RuntimeOwner::start), activation handoff to the 4-arg
// PrepareActivatedUE4SSRuntime(context, activation, expected_title,
// runtime_ack), then start(). The returned owner is stopped by module_stop
// via StopRuntimeWithAck. Throws exactly what the called interfaces throw
// (std::logic_error on double binding install; validation refusals from the
// prepare call) - module_entry's entry_guard makes in-process re-entry
// impossible, so the install is genuinely once-per-process.
auto StartActivatedRuntimeWithAck(RC::Platform::NativeProgramContext context,
                                  ActivatedMods activation,
                                  std::string_view expected_title,
                                  const char* mod_root)
    -> std::unique_ptr<RuntimeOwner>;
#endif
}
