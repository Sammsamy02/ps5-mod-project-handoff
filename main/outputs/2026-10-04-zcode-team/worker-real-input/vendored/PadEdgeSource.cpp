// Vendored for rung 5 by worker-real-input, 2026-10-04. Originals immutable.
//
// VENDORED (unchanged logic) from ue4ss-native-pad-provider-1/port/PS5PadSource.cpp
// (sha256 54ed47d9a4485f5ede2ea9ed64051463d3f18d9a29123a19582afa07bdc85184):
//   constructor validation, activate/deactivate lifecycle, fail(), is_available(),
//   process_event(), and the poll() core: connected/Read-error handling, cached-read
//   skip (data.timestamp == m_timestamp), baseline swallow, rising-edge rule,
//   held-state tracking while unsubscribed/disabled, per-poll duplicate collapse.
// ADDED for rung 5 (marked "ADDED:" below): re-arm debounce (min_rearm_us),
//   drop-not-queue of suppressed edges, backwards-timestamp re-baseline, counters.
#include "PadEdgeSource.hpp"
#include <algorithm>
#include <utility>

namespace modbridge::input
{
using namespace RC::Input;

static constexpr uint32_t known_buttons = PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3 | PS5_PAD_BUTTON_OPTIONS |
    PS5_PAD_BUTTON_UP | PS5_PAD_BUTTON_RIGHT | PS5_PAD_BUTTON_DOWN | PS5_PAD_BUTTON_LEFT |
    PS5_PAD_BUTTON_L2 | PS5_PAD_BUTTON_R2 | PS5_PAD_BUTTON_L1 | PS5_PAD_BUTTON_R1 |
    PS5_PAD_BUTTON_TRIANGLE | PS5_PAD_BUTTON_CIRCLE | PS5_PAD_BUTTON_CROSS |
    PS5_PAD_BUTTON_SQUARE | PS5_PAD_BUTTON_TOUCH_PAD;

PadEdgeSource::PadEdgeSource(std::vector<PadBinding> bindings, PadBackend backend, int user,
                             PadEdgePolicy policy)
 : m_bindings(std::move(bindings)), m_down(m_bindings.size(), false),
   m_last_emit(m_bindings.size(), 0), m_ever_emitted(m_bindings.size(), false),
   m_backend(backend), m_requested_user(user), m_policy(policy)
{
    // VENDORED: constructor validation (PS5PadSource.cpp:7-21), policy bounds ADDED.
    m_valid = user >= -1 && !m_bindings.empty() && m_bindings.size() <= 128 &&
        backend.user_initialize && backend.pad_initialize && backend.user_list && backend.open &&
        backend.read_state && backend.close && backend.user_terminate;
    constexpr uint32_t modifiers = (1u << SHIFT) | (1u << CONTROL) | (1u << ALT);
    for (const auto& b : m_bindings)
        m_valid = m_valid && b.buttons && !(b.buttons & ~known_buttons) &&
            b.key > RESERVED_START_OF_ENUM && b.key < max_keys && !(b.modifiers.keys & ~modifiers);
    if (!m_valid) fail(PadPhase::InvalidBindings, -1);
    m_events.reserve(m_bindings.size());
}

PadEdgeSource::~PadEdgeSource() { deactivate(); }
bool PadEdgeSource::fail(PadPhase phase, int error) { m_status.phase = phase; m_status.error = error; return false; }
bool PadEdgeSource::is_available() { return m_status.handle >= 0; }

bool PadEdgeSource::activate()
{
    // VENDORED: exact activation sequence (PS5PadSource.cpp:24-47).
    if (!m_valid) return false;
    if (is_available()) return true;
    if (!m_initialized)
    {
        int err = m_backend.user_initialize(nullptr);
        if (err && static_cast<uint32_t>(err) != 0x80960003u) return fail(PadPhase::UserInitialize, err);
        m_owns_user_service = err == 0;
        m_initialized = true;
    }
    int err = m_backend.pad_initialize();
    if (err) return fail(PadPhase::PadInitialize, err);
    int users[4]{-1, -1, -1, -1};
    err = m_backend.user_list(users);
    if (err) return fail(PadPhase::UserList, err);
    int selected = -1;
    for (int user : users) if (user >= 0 && (m_requested_user == -1 || user == m_requested_user)) { selected = user; break; }
    if (selected < 0) return fail(PadPhase::NoUser, 0);
    int handle = m_backend.open(selected, PS5_PAD_PORT_TYPE_STANDARD, 0, nullptr);
    if (handle < 0) return fail(PadPhase::Open, handle);
    m_status = {PadPhase::Active, 0, selected, handle};
    m_have_sample = false; m_timestamp = 0; std::fill(m_down.begin(), m_down.end(), false);
    // ADDED: reset per-binding edge bookkeeping on (re)activation.
    std::fill(m_last_emit.begin(), m_last_emit.end(), 0);
    std::fill(m_ever_emitted.begin(), m_ever_emitted.end(), false);
    m_suppressed_edges = 0;
    return true;
}

bool PadEdgeSource::deactivate()
{
    // VENDORED: exact teardown (PS5PadSource.cpp:48-64).
    m_events.clear();
    if (m_status.handle >= 0)
    {
        int err = m_backend.close(m_status.handle);
        if (err) return fail(PadPhase::Close, err);
        m_status.handle = -1;
    }
    if (m_owns_user_service)
    {
        int err = m_backend.user_terminate();
        if (err) return fail(PadPhase::UserTerminate, err);
        m_owns_user_service = false;
    }
    m_initialized = false; m_have_sample = false; m_status.user_id = -1;
    if (m_valid) { m_status.phase = PadPhase::Inactive; m_status.error = 0; }
    return true;
}

std::vector<InputEvent>& PadEdgeSource::process_event(RC::Input::Handler* handler)
{
    // VENDORED: PS5PadSource.cpp:65-67.
    if (!handler) { m_events.clear(); return m_events; }
    return poll(handler->get_subscribed_keys(), handler->get_allow_input());
}

std::vector<InputEvent>& PadEdgeSource::poll(const std::array<bool, max_keys>& subscribed, bool allow)
{
    // VENDORED: poll core (PS5PadSource.cpp:69-93) with the three ADDED guards.
    m_events.clear();
    if (!is_available()) return m_events;
    PS5_PadData data{};
    int err = m_backend.read_state(m_status.handle, &data);
    if (err) { fail(PadPhase::Read, err); return m_events; }
    if (!data.connected)
    {
        m_status.phase = PadPhase::Disconnected; m_status.error = 0; m_have_sample = false;
        std::fill(m_down.begin(), m_down.end(), false);
        // ADDED: a disconnected pad invalidates debounce history; re-attach is a baseline.
        std::fill(m_ever_emitted.begin(), m_ever_emitted.end(), false);
        return m_events;
    }
    m_status.phase = PadPhase::Active; m_status.error = 0;
    // VENDORED: cached-read skip; a duplicate sample cannot create another keydown.
    if (m_have_sample && data.timestamp == m_timestamp) return m_events;
    // ADDED: backwards pad timestamps re-baseline instead of fabricating edges.
    if (m_have_sample && data.timestamp < m_timestamp) { m_have_sample = false; m_down.assign(m_down.size(), false); }
    const bool baseline = !m_have_sample;
    for (size_t i = 0; i < m_bindings.size(); ++i)
    {
        const auto& b = m_bindings[i];
        const bool down = (data.buttons & b.buttons) == b.buttons;
        if (!baseline && down && !m_down[i] && allow && subscribed[b.key])
        {
            bool duplicate = false;
            for (const auto& e : m_events)
                if (e.key == b.key && e.modifier_keys.keys == b.modifiers.keys) duplicate = true;
            // ADDED: re-arm debounce. First edge per activation never debounces; later
            // edges need min_rearm_us of pad time since the last EMITTED edge. A
            // suppressed edge is dropped, never queued (at-most-once per real press).
            const bool debounce_ok = !m_ever_emitted[i] || m_policy.min_rearm_us == 0 ||
                (data.timestamp > m_last_emit[i] &&
                 data.timestamp - m_last_emit[i] >= m_policy.min_rearm_us);
            if (!duplicate && debounce_ok)
            {
                m_events.push_back({b.key, b.modifiers});
                m_last_emit[i] = data.timestamp;
                m_ever_emitted[i] = true;
            }
            else if (!duplicate)
            {
                ++m_suppressed_edges;
            }
        }
        m_down[i] = down; // VENDORED: track even while disabled/unsubscribed, preventing phantom edges.
    }
    m_have_sample = true; m_timestamp = data.timestamp;
    return m_events;
}
}
