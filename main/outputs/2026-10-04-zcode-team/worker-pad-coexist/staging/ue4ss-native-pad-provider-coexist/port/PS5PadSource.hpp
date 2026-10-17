#pragma once

#include <cstddef>
#include <Input/PlatformInputSource.hpp>
extern "C" {
// Exact pinned SDL declaration header, unchanged. cstddef supplies its size_t.
#include "../primary-sources/SDL/src/joystick/ps5/SDL_ps5joystick.h"
}
namespace RC::Input {
struct PadBinding { uint32_t buttons; Key key; ModifierKeys modifiers{}; };
enum class PadPhase { Uninitialized, InvalidBindings, UserInitialize, PadInitialize, UserList,
    NoUser, Open, Active, Disconnected, Read, Close, UserTerminate, Inactive };
struct PadStatus { PadPhase phase{PadPhase::Uninitialized}; int error{}; int user_id{-1}; int handle{-1}; };
// Serialized owner lifecycle: construction/activation, sole processing thread, then
// deactivation after it joins. Backend overrides are for explicit owned tests only.
struct PadBackend {
    int (*user_initialize)(void*);
    int (*pad_initialize)();
    int (*user_list)(int[4]);
    int (*open)(int,int,int,void*);
    int (*read_state)(int,PS5_PadData*);
    int (*close)(int);
    int (*user_terminate)();
};
class PS5PadSource final : public PlatformInputSource {
public:
    PS5PadSource(std::vector<PadBinding> bindings, PadBackend backend, int requested_user = -1);
    ~PS5PadSource() override;
    bool is_available() override;
    bool activate() override;
    bool deactivate() override;
    std::vector<InputEvent>& process_event(Handler*) override;
    // Allows identical mapping controls with caller-owned subscription state.
    std::vector<InputEvent>& poll(const std::array<bool,max_keys>& subscriptions, bool allow);
    const char* get_name() override { return "PS5Pad"; }
    int source_priority() override { return 100; }
    const PadStatus& status() const { return m_status; }
private:
    std::vector<PadBinding> m_bindings;
    std::vector<bool> m_down;
    std::vector<InputEvent> m_events;
    PadBackend m_backend;
    PadStatus m_status;
    int m_requested_user;
    bool m_valid{}, m_initialized{}, m_owns_user_service{}, m_have_sample{};
    uint64_t m_timestamp{};
    // worker-pad-coexist: event-driven retry state. Appended AFTER every
    // pre-existing member, so all earlier offsets (including m_status, read by
    // the inline status() emitted in every including TU) are byte-identical to
    // the base revision and every translation unit that still includes the
    // base header stays ABI-consistent with objects of this class.
    uint32_t m_polls_since_retry{};   // polls since the last activate() attempt
    uint32_t m_retry_interval{200};   // next attempt after this many polls
    uint32_t m_retry_attempts{};      // cumulative activate() attempts (evidence)
    void maybe_retry();
    bool fail(PadPhase phase, int error);
};
// Eagerly activates to satisfy Handler's available-before-activate contract;
// unavailable returned objects retain exact errors and can be retried explicitly.
std::shared_ptr<PS5PadSource> make_native_ps5_pad_source(std::vector<PadBinding>, int user_id = -1);
}
