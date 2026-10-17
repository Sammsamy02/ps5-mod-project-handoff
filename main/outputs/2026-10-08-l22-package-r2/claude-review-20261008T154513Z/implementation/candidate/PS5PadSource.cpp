#include "PS5PadSource.hpp"
#include <algorithm>
#include <utility>
// worker-pad-coexist: shared composition diagnostic sink (diagnostic.c), the
// same in-link C symbol the wpd1 staging already declares (launch10-build
// EDITION.json edit n=3). Namespace-scope extern "C" declaration only; the
// symbol is DEFINED in-link by the composition, so this adds no new import
// and appears as no new undefined symbol at the combined-supplier boundary.
extern "C" void diagnostic_event(const char* tag, unsigned int value);
namespace RC::Input {
static constexpr uint32_t known_buttons = PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3 | PS5_PAD_BUTTON_OPTIONS |
    PS5_PAD_BUTTON_UP | PS5_PAD_BUTTON_RIGHT | PS5_PAD_BUTTON_DOWN | PS5_PAD_BUTTON_LEFT |
    PS5_PAD_BUTTON_L2 | PS5_PAD_BUTTON_R2 | PS5_PAD_BUTTON_L1 | PS5_PAD_BUTTON_R1 |
    PS5_PAD_BUTTON_TRIANGLE | PS5_PAD_BUTTON_CIRCLE | PS5_PAD_BUTTON_CROSS |
    PS5_PAD_BUTTON_SQUARE | PS5_PAD_BUTTON_TOUCH_PAD;
namespace {
// worker-pad-coexist retry pacing, counted in POLLS (the UpdateThread's
// process_event cadence; ~200 Hz worst case at the documented 5 ms loop sleep,
// so 200 polls ~= 1 s). One attempt per interval, interval doubling per failed
// attempt up to the cap (3200 polls ~= 16 s worst case): bounded retries, no
// busy loop, no timer and no thread of our own. Every attempt re-runs only the
// cheap tail of activate() (pad_initialize + user_list + open; the user-service
// acquire stays short-circuited by m_initialized), i.e. <=4 bounded SDK calls.
constexpr uint32_t retry_interval_initial = 200;
constexpr uint32_t retry_interval_max = 3200;
// pad_open_class evidence value: 0 = scePadOpen(user, PS5_PAD_PORT_TYPE_STANDARD,
// 0, nullptr) — the ONLY open class this source attempts. The pinned SDK stub
// exports further open-class entry points (scePadOpenExt/OpenExt2/DeviceOpen/
// GetHandle), but their second-handle semantics are not decidable from any
// in-tree header (worker-pad-coexist SDK-ANALYSIS.md); they are NOT attempted
// from production code. The launch-window companion probe answers that question.
constexpr unsigned int pad_open_class_standard = 0;
// worker-input-tag: once-per-boot input-arrival evidence flag (acceptance gap
// G-AP1, read half). File-local on purpose: NO header change, NO ABI change;
// written only from poll() on the sole processing thread (the same
// serialized-thread discipline the coexist retry counters are documented
// under), so no synchronization is needed.
bool pad_first_state_read_emitted = false;
}
PS5PadSource::PS5PadSource(std::vector<PadBinding> bindings, PadBackend backend, int user)
 : m_bindings(std::move(bindings)), m_down(m_bindings.size(),false), m_backend(backend), m_requested_user(user) {
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
PS5PadSource::~PS5PadSource() { deactivate(); }
bool PS5PadSource::fail(PadPhase phase, int error) { m_status.phase=phase; m_status.error=error; return false; }
bool PS5PadSource::is_available() { return m_status.handle >= 0; }
bool PS5PadSource::activate() {
    if (!m_valid) return false;
    if (is_available()) return true;
    if (!m_initialized) {
        int err=m_backend.user_initialize(nullptr);
        // Exact already-initialized value accepted by the pinned SDL PS5 backend.
        if (err && static_cast<uint32_t>(err) != 0x80960003u) return fail(PadPhase::UserInitialize,err);
        m_owns_user_service = err == 0;
        m_initialized=true;
    }
    int err=m_backend.pad_initialize();
    if (err) return fail(PadPhase::PadInitialize,err);
    int users[4]{-1,-1,-1,-1};
    err=m_backend.user_list(users);
    if (err) return fail(PadPhase::UserList,err);
    int selected=-1;
    for (int user : users) if (user >= 0 && (m_requested_user == -1 || user == m_requested_user)) { selected=user; break; }
    if (selected < 0) return fail(PadPhase::NoUser,0);
    int handle=m_backend.open(selected,PS5_PAD_PORT_TYPE_STANDARD,0,nullptr);
    if (handle < 0) return fail(PadPhase::Open,handle);
    m_status={PadPhase::Active,0,selected,handle};
    m_have_sample=false; m_timestamp=0; std::fill(m_down.begin(),m_down.end(),false);
    return true;
}
bool PS5PadSource::deactivate() {
    m_events.clear();
    if (m_status.handle >= 0) {
        int err=m_backend.close(m_status.handle);
        if (err) return fail(PadPhase::Close,err); // Retain owner for explicit retry.
        m_status.handle=-1;
    }
    if (m_owns_user_service) {
        int err=m_backend.user_terminate();
        if (err) return fail(PadPhase::UserTerminate,err);
        m_owns_user_service=false;
    }
    m_initialized=false; m_have_sample=false; m_status.user_id=-1;
    if (m_valid) { m_status.phase=PadPhase::Inactive; m_status.error=0; }
    return true;
}
std::vector<InputEvent>& PS5PadSource::process_event(Handler* handler) {
    if (!handler) { m_events.clear(); return m_events; }
    return poll(handler->get_subscribed_keys(),handler->get_allow_input());
}
void PS5PadSource::maybe_retry() {
    if (!m_valid) return;
    if (++m_polls_since_retry < m_retry_interval) return;
    m_polls_since_retry=0;
    ++m_retry_attempts;
    if (m_retry_attempts == 1) diagnostic_event("pad_open_class",pad_open_class_standard);
    diagnostic_event("pad_retry",m_retry_attempts);
    // Same-thread serialized re-activation: poll() runs only on the sole
    // processing thread (PlatformInputSource.hpp lifecycle contract; Handler
    // SAFETY note - no overlap with any other source call, no cross-thread
    // Handler::init re-injection anywhere in this design).
    if (activate()) {
        diagnostic_event("pad_available",static_cast<unsigned int>(m_status.handle));
        m_retry_interval=retry_interval_initial; // Fast first retry after any success.
        return;
    }
    diagnostic_event("pad_retry_failed",static_cast<unsigned int>(m_status.error));
    const uint32_t doubled=m_retry_interval*2;
    m_retry_interval=doubled>retry_interval_max?retry_interval_max:doubled;
}
std::vector<InputEvent>& PS5PadSource::poll(const std::array<bool,max_keys>& subscribed, bool allow) {
    m_events.clear();
    if (!is_available()) {
        // worker-pad-coexist: the failure is no longer cached forever. The
        // exclusive handle may free while we run (disconnect/user-switch/
        // system-menu windows); retry event-driven from the processing thread
        // instead of only at startup (the wpd1 lineage's documented dead-code
        // design, made reachable by the deferred-attached Handler wiring).
        maybe_retry();
        return m_events;
    }
    PS5_PadData data{};
    int err=m_backend.read_state(m_status.handle,&data);
    if (err) { fail(PadPhase::Read,err); return m_events; }
    if (!data.connected) {
        m_status.phase=PadPhase::Disconnected; m_status.error=0; m_have_sample=false;
        std::fill(m_down.begin(),m_down.end(),false); return m_events;
    }
    m_status.phase=PadPhase::Active; m_status.error=0;
    // worker-input-tag: once-per-boot proof that the READ path is alive AFTER
    // the open (pad_available proves only the open path). Fires on the first
    // err-free AND connected read - the input-dead vs read-dead-after-open
    // discriminator: pad_available present with NO pad_first_state_read means
    // reads started failing after a successful open. Value = the RAW buttons
    // word of that first usable state (the baseline sample - deliberately
    // BEFORE the duplicate-timestamp guard, so the tag fires even with no
    // press ever made). Pure diagnostic emission: no behavior change.
    if (!pad_first_state_read_emitted) {
        pad_first_state_read_emitted=true;
        diagnostic_event("pad_first_state_read",static_cast<unsigned int>(data.buttons));
    }
    // ReadState is cached; a duplicate sample cannot create another keydown.
    if (m_have_sample && data.timestamp == m_timestamp) return m_events;
    bool baseline=!m_have_sample;
    for (size_t i=0; i<m_bindings.size(); ++i) {
        const auto& b=m_bindings[i]; bool down=(data.buttons & b.buttons) == b.buttons;
        // l22: observe real configured rising edges even when delivery is
        // suppressed. This does not alter the mapping or synthesize input.
        if (!baseline && down && !m_down[i] &&
            b.buttons == (PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3)) {
            diagnostic_event("pad_l3r3_edge",static_cast<unsigned int>(b.key));
            diagnostic_event("pad_l3r3_edge_allow",allow ? 1u : 0u);
            diagnostic_event("pad_l3r3_edge_subscribed",subscribed[b.key] ? 1u : 0u);
            diagnostic_event("pad_l3r3_edge_modifiers",b.modifiers.keys);
        }
        if (!baseline && down && !m_down[i] && allow && subscribed[b.key]) {
            bool duplicate=false;
            for (const auto& e:m_events) if (e.key==b.key && e.modifier_keys.keys==b.modifiers.keys) duplicate=true;
            if (!duplicate) {
                m_events.push_back({b.key,b.modifiers});
                if (b.buttons == (PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3) && b.key == Key::F7)
                    diagnostic_event("pad_l3r3_f7_dispatched",static_cast<unsigned int>(b.key));
            }
        }
        m_down[i]=down; // Track even while disabled/unsubscribed, preventing phantom edges.
    }
    m_have_sample=true; m_timestamp=data.timestamp;
    return m_events;
}
}
