#pragma once
// Vendored for rung 5 by worker-real-input, 2026-10-04. Originals immutable.
//
// Vendored from: .superpowers/native-dependency-spike/ue4ss-native-pad-provider-1/port/PS5PadSource.hpp
// (sha256 8eff7245523db26a0b36d488d9f84f120ee7fcd2103dddfbf9d35831c741c792, checkpoint
// native_ps5_pad_provider). Same public API and serialized lifecycle contract; the ONLY
// additions are the rung-5 hardening knobs in PadEdgePolicy (see PadEdgeSource.cpp for
// VENDORED vs ADDED line provenance):
//   - min_rearm_us: per-binding re-arm debounce in the pad-timestamp domain
//   - suppressed edges are dropped, never queued (at-most-once per real press)
//   - backwards pad timestamps re-baseline instead of inventing edges
// The retained RC::Input::PS5PadSource stays untouched and remains the parity oracle in
// host tests (with min_rearm_us == 0 this source must emit exactly its events).

#include <PS5PadSource.hpp> // actual retained provider API: PadBinding, PadBackend, PadStatus, Input/PlatformInputSource.hpp

namespace modbridge::input
{
using RC::Input::InputEvent;
using RC::Input::Key;
using RC::Input::PadBackend;
using RC::Input::PadBinding;
using ::PS5_PadData; // global scope: pinned SDL header (extern "C"), PS5PadSource.hpp:4-7

struct PadEdgePolicy
{
    // ScePadReadState timestamps are microseconds. A rising edge only re-fires a key
    // after at least this much pad time since that key's last emitted edge. 0 keeps
    // exact retained-provider behavior (parity mode).
    uint64_t min_rearm_us{40'000};
};

class PadEdgeSource final : public RC::Input::PlatformInputSource
{
public:
    PadEdgeSource(std::vector<PadBinding> bindings, PadBackend backend, int requested_user = -1,
                  PadEdgePolicy policy = {});
    ~PadEdgeSource() override;
    bool is_available() override;
    bool activate() override;
    bool deactivate() override;
    std::vector<InputEvent>& process_event(RC::Input::Handler*) override;
    // Identical signature/semantics to PS5PadSource::poll (PS5PadSource.hpp:33).
    std::vector<InputEvent>& poll(const std::array<bool, RC::Input::max_keys>& subscriptions, bool allow);
    const char* get_name() override { return "PS5PadEdge"; }
    int source_priority() override { return 100; }
    const RC::Input::PadStatus& status() const { return m_status; }
    // Test/telemetry introspection (host tests only; no runtime dependence).
    size_t suppressed_edge_count() const { return m_suppressed_edges; }

private:
    std::vector<PadBinding> m_bindings;
    std::vector<bool> m_down;          // VENDORED: held tracking incl. unsubscribed/disabled
    std::vector<uint64_t> m_last_emit; // ADDED: pad timestamp of last emitted edge per binding
    std::vector<bool> m_ever_emitted;  // ADDED: first edge never debounces
    std::vector<InputEvent> m_events;
    PadBackend m_backend;
    RC::Input::PadStatus m_status;
    PadEdgePolicy m_policy;
    int m_requested_user;
    bool m_valid{}, m_initialized{}, m_owns_user_service{}, m_have_sample{};
    uint64_t m_timestamp{};
    uint64_t m_suppressed_edges{};     // ADDED: dropped-edge counter (never queued)
    bool fail(RC::Input::PadPhase phase, int error);
};
}
