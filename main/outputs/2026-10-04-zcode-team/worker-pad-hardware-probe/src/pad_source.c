// pad_source.c — C PORT of the retained provider (worker-pad-hardware-probe,
// team 2026-10-04). Derived-origin:
// .superpowers/native-dependency-spike/ue4ss-native-pad-provider-1/port/
// PS5PadSource.cpp (RETAINED, read-only). Function-by-function port; the
// only structural change is fixed-capacity members instead of std::vector
// and int return codes instead of bool. Semantics preserved exactly:
//   - constructor validity gate incl. known_buttons subset check;
//   - activate(): user_initialize with the pinned already-initialized
//     acceptance (0x80960003, SDL PS5 backend value), pad_initialize,
//     user_list over int[4], user selection (-1 = first login user),
//     open(PS5_PAD_PORT_TYPE_STANDARD, index 0);
//   - deactivate(): close then user_terminate ONLY when the lease is ours;
//   - poll(): read_state error -> Read phase, !connected -> Disconnected +
//     edge-state reset, duplicate-timestamp suppression, baseline first
//     sample, rising-edge delivery gated by subscribed[key] && allow, and
//     edge-state tracked even while gated (no phantom edges on re-enable).
#include "pad_source.h"
#include "ingame_libc.h"

#include <stddef.h>

// Retained layout asserts (PS5PadNative.cpp:7-9) re-verified at build time
// on host AND target:
_Static_assert(sizeof(PS5_PadData) == 120, "PS5_PadData layout drifted");
_Static_assert(offsetof(PS5_PadData, buttons) == 0, "buttons offset");
_Static_assert(offsetof(PS5_PadData, connected) == 76, "connected offset");
_Static_assert(offsetof(PS5_PadData, timestamp) == 80, "timestamp offset");
_Static_assert(offsetof(PS5_PadData, count) == 104, "count offset");

static const uint32_t pp_known_buttons =
    PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3 | PS5_PAD_BUTTON_OPTIONS |
    PS5_PAD_BUTTON_UP | PS5_PAD_BUTTON_RIGHT | PS5_PAD_BUTTON_DOWN |
    PS5_PAD_BUTTON_LEFT | PS5_PAD_BUTTON_L2 | PS5_PAD_BUTTON_R2 |
    PS5_PAD_BUTTON_L1 | PS5_PAD_BUTTON_R1 | PS5_PAD_BUTTON_TRIANGLE |
    PS5_PAD_BUTTON_CIRCLE | PS5_PAD_BUTTON_CROSS | PS5_PAD_BUTTON_SQUARE |
    PS5_PAD_BUTTON_TOUCH_PAD;

// Retained gate modifiers (PS5PadSource.cpp:15): SHIFT|CONTROL|ALT; the C
// probe carries no modifier slot, so the empty set is trivially valid.

const char *pp_phase_name(PP_PadPhase phase)
{
    switch (phase) {
        case PP_PHASE_UNINITIALIZED:   return "Uninitialized";
        case PP_PHASE_INVALID_BINDINGS: return "InvalidBindings";
        case PP_PHASE_USER_INITIALIZE: return "UserInitialize";
        case PP_PHASE_PAD_INITIALIZE:  return "PadInitialize";
        case PP_PHASE_USER_LIST:       return "UserList";
        case PP_PHASE_NO_USER:         return "NoUser";
        case PP_PHASE_OPEN:            return "Open";
        case PP_PHASE_ACTIVE:          return "Active";
        case PP_PHASE_DISCONNECTED:    return "Disconnected";
        case PP_PHASE_READ:            return "Read";
        case PP_PHASE_CLOSE:           return "Close";
        case PP_PHASE_USER_TERMINATE:  return "UserTerminate";
        case PP_PHASE_INACTIVE:        return "Inactive";
    }
    return "Unknown";
}

void pp_source_init(PP_PadSource *s, const PP_PadBinding *bindings,
                    size_t binding_count, PP_PadBackend backend, int user)
{
    size_t i;
    int valid;
    for (i = 0; i < PP_SOURCE_MAX_BINDINGS; ++i) s->down[i] = 0;
    s->event_count = 0;
    s->binding_count = 0;
    s->backend = backend;
    s->status.phase = PP_PHASE_UNINITIALIZED;
    s->status.error = 0;
    s->status.user_id = -1;
    s->status.handle = -1;
    s->requested_user = user;
    s->initialized = 0;
    s->owns_user_service = 0;
    s->have_sample = 0;
    s->timestamp = 0;

    valid = (user >= -1) && (binding_count > 0) &&
            (binding_count <= PP_SOURCE_MAX_BINDINGS) &&
            backend.user_initialize && backend.pad_initialize &&
            backend.user_list && backend.open && backend.read_state &&
            backend.close && backend.user_terminate;
    if (valid) {
        for (i = 0; i < binding_count; ++i) {
            const PP_PadBinding *b = &bindings[i];
            /* key range: the retained gate checks
               RESERVED_START_OF_ENUM < key < max_keys; this probe's
               subscription domain is the 256-slot subscribed table
               (pad_probe_core.h), so the port bounds key to 1..255. */
            valid = valid && b->buttons != 0 &&
                    (b->buttons & ~pp_known_buttons) == 0 &&
                    b->key > 0 && b->key < 256;
        }
    }
    s->valid = valid;
    if (!valid)
        pp_source_fail(s, PP_PHASE_INVALID_BINDINGS, -1);
    if (valid) {
        for (i = 0; i < binding_count; ++i) s->bindings[i] = bindings[i];
        s->binding_count = binding_count;
    }
}

int pp_source_fail(PP_PadSource *s, PP_PadPhase phase, int error)
{
    s->status.phase = phase;
    s->status.error = error;
    return 0;
}

int pp_source_is_available(const PP_PadSource *s)
{
    return s->status.handle >= 0;
}

int pp_source_activate(PP_PadSource *s)
{
    int users[4];
    int err, selected = -1, i, handle;
    if (!s->valid) return 0;
    if (pp_source_is_available(s)) return 1;
    if (!s->initialized) {
        err = s->backend.user_initialize(0);
        // Exact already-initialized value accepted by the pinned SDL PS5
        // backend (retained PS5PadSource.cpp:29-31).
        if (err && (uint32_t)err != 0x80960003u)
            return pp_source_fail(s, PP_PHASE_USER_INITIALIZE, err);
        s->owns_user_service = (err == 0);
        s->initialized = 1;
    }
    err = s->backend.pad_initialize();
    if (err) return pp_source_fail(s, PP_PHASE_PAD_INITIALIZE, err);
    for (i = 0; i < 4; ++i) users[i] = -1;
    err = s->backend.user_list(users);
    if (err) return pp_source_fail(s, PP_PHASE_USER_LIST, err);
    for (i = 0; i < 4; ++i) {
        if (users[i] >= 0 &&
            (s->requested_user == -1 || users[i] == s->requested_user)) {
            selected = users[i];
            break;
        }
    }
    if (selected < 0) return pp_source_fail(s, PP_PHASE_NO_USER, 0);
    handle = s->backend.open(selected, PS5_PAD_PORT_TYPE_STANDARD, 0, 0);
    if (handle < 0) return pp_source_fail(s, PP_PHASE_OPEN, handle);
    s->status.phase = PP_PHASE_ACTIVE;
    s->status.error = 0;
    s->status.user_id = selected;
    s->status.handle = handle;
    s->have_sample = 0;
    s->timestamp = 0;
    {
        size_t k;
        for (k = 0; k < PP_SOURCE_MAX_BINDINGS; ++k) s->down[k] = 0;
    }
    return 1;
}

int pp_source_deactivate(PP_PadSource *s)
{
    int err;
    s->event_count = 0;
    if (s->status.handle >= 0) {
        err = s->backend.close(s->status.handle);
        if (err) return pp_source_fail(s, PP_PHASE_CLOSE, err); // owner retained for explicit retry
        s->status.handle = -1;
    }
    if (s->owns_user_service) {
        err = s->backend.user_terminate();
        if (err) return pp_source_fail(s, PP_PHASE_USER_TERMINATE, err);
        s->owns_user_service = 0;
    }
    s->initialized = 0;
    s->have_sample = 0;
    s->status.user_id = -1;
    if (s->valid) {
        s->status.phase = PP_PHASE_INACTIVE;
        s->status.error = 0;
    }
    return 1;
}

size_t pp_source_poll(PP_PadSource *s, const uint8_t *subscribed, int allow)
{
    PS5_PadData data;
    int err, baseline;
    size_t i;
    s->event_count = 0;
    if (!pp_source_is_available(s)) return 0;
    pp_memzero(&data, sizeof data);    err = s->backend.read_state(s->status.handle, &data);
    if (err) {
        pp_source_fail(s, PP_PHASE_READ, err);
        return 0;
    }
    if (!data.connected) {
        size_t k;
        s->status.phase = PP_PHASE_DISCONNECTED;
        s->status.error = 0;
        s->have_sample = 0;
        for (k = 0; k < PP_SOURCE_MAX_BINDINGS; ++k) s->down[k] = 0;
        return 0;
    }
    s->status.phase = PP_PHASE_ACTIVE;
    s->status.error = 0;
    // ReadState is cached; a duplicate sample cannot create another keydown
    // (retained PS5PadSource.cpp:80-81).
    if (s->have_sample && data.timestamp == s->timestamp) return 0;
    baseline = !s->have_sample;
    for (i = 0; i < s->binding_count; ++i) {
        const PP_PadBinding *b = &s->bindings[i];
        int down = (data.buttons & b->buttons) == b->buttons;
        if (!baseline && down && !s->down[i] && allow && subscribed[b->key]) {
            size_t e;
            int duplicate = 0;
            for (e = 0; e < s->event_count; ++e)
                if (s->events[e].key == b->key) duplicate = 1;
            if (!duplicate && s->event_count < PP_SOURCE_MAX_EVENTS)
                s->events[s->event_count++].key = b->key;
        }
        s->down[i] = (uint8_t)down; // tracked even while gated (no phantom edges)
    }
    s->have_sample = 1;
    s->timestamp = data.timestamp;
    return s->event_count;
}
