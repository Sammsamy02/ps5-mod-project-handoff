// pad_source.h — C PORT of the retained provider for the standalone pad
// probe PRX (worker-pad-hardware-probe, team 2026-10-04).
// Derived-origin: .superpowers/native-dependency-spike/ue4ss-native-pad-
// provider-1/port/PS5PadSource.hpp (RETAINED, read-only, never modified).
// The retained C++ source needs std::vector/shared_ptr (libc++), which the
// ploader PRX lineage does not carry; this port reproduces the exact
// PadBackend vtable, validity gates, activate() backend sequence, poll()
// edge/timestamp semantics, and PadPhase/PadStatus shapes so the probe's
// console evidence maps 1:1 onto the retained provider's behavior. The one
// fixed binding (OPTIONS -> F7) and the dispatch-gate subscription are owned
// by pad_probe_core.c (the Handler.cpp port side).
#ifndef PAD_PROBE_SOURCE_H
#define PAD_PROBE_SOURCE_H

#include "pad_sdl_ps5.h"

#include <stddef.h>
#include <stdint.h>

// PadPhase enum port (exact retained order, PS5PadSource.hpp:10-11).
typedef enum PP_PadPhase {
    PP_PHASE_UNINITIALIZED = 0,
    PP_PHASE_INVALID_BINDINGS,
    PP_PHASE_USER_INITIALIZE,
    PP_PHASE_PAD_INITIALIZE,
    PP_PHASE_USER_LIST,
    PP_PHASE_NO_USER,
    PP_PHASE_OPEN,
    PP_PHASE_ACTIVE,
    PP_PHASE_DISCONNECTED,
    PP_PHASE_READ,
    PP_PHASE_CLOSE,
    PP_PHASE_USER_TERMINATE,
    PP_PHASE_INACTIVE
} PP_PadPhase;

const char *pp_phase_name(PP_PadPhase phase);  // retained enum names, verbatim

typedef struct PP_PadStatus {
    PP_PadPhase phase;  // PP_PHASE_UNINITIALIZED
    int error;
    int user_id;   // -1
    int handle;    // -1
} PP_PadStatus;

// PadBackend vtable port (PS5PadSource.hpp:15-23; the console table is
// exactly PS5PadNative.cpp's scePad*/sceUserService* function set).
typedef struct PP_PadBackend {
    int (*user_initialize)(void *);
    int (*pad_initialize)(void);
    int (*user_list)(int /*users*/[4]);
    int (*open)(int, int, int, void *);
    int (*read_state)(int, PS5_PadData *);
    int (*close)(int);
    int (*user_terminate)(void);
} PP_PadBackend;

// One binding: buttons mask -> key id. The probe owns exactly one
// (PS5_PAD_BUTTON_OPTIONS -> the F7 key id); Key/ModifierKeys collapse to an
// int key id because the C probe's gate has a single subscription slot.
typedef struct PP_PadBinding {
    uint32_t buttons;
    int key;
} PP_PadBinding;

// Event delivered by a rising edge (InputEvent port; the probe gate has no
// modifier slots — the retained default ModifierKeys{} is the empty set).
typedef struct PP_InputEvent {
    int key;
} PP_InputEvent;

#define PP_SOURCE_MAX_BINDINGS 128
#define PP_SOURCE_MAX_EVENTS   PP_SOURCE_MAX_BINDINGS

typedef struct PP_PadSource {
    PP_PadBinding bindings[PP_SOURCE_MAX_BINDINGS];
    size_t binding_count;
    uint8_t down[PP_SOURCE_MAX_BINDINGS];     // m_down port
    PP_InputEvent events[PP_SOURCE_MAX_EVENTS];
    size_t event_count;
    PP_PadBackend backend;
    PP_PadStatus status;
    int requested_user;
    int valid;          // m_valid
    int initialized;    // m_initialized
    int owns_user_service;  // m_owns_user_service (lease ownership)
    int have_sample;    // m_have_sample
    uint64_t timestamp; // m_timestamp
} PP_PadSource;

// Port of the constructor + validity gate (PS5PadSource.cpp:10-21). Never
// fails hard: invalid input records PP_PHASE_INVALID_BINDINGS instead.
void pp_source_init(PP_PadSource *s, const PP_PadBinding *bindings,
                    size_t binding_count, PP_PadBackend backend, int user);

// Port of the private fail() helper (PS5PadSource.cpp:23). Returns 0.
int pp_source_fail(PP_PadSource *s, PP_PadPhase phase, int error);

// Port of is_available/activate/deactivate (PS5PadSource.cpp:24-64).
int pp_source_is_available(const PP_PadSource *s);
int pp_source_activate(PP_PadSource *s);
int pp_source_deactivate(PP_PadSource *s);

// Port of poll() (PS5PadSource.cpp:69-94): one scePadReadState-equivalent
// read, duplicate-timestamp suppression, rising-edge events for the caller's
// subscription (subscribed[key] && allow mirror the Handler gate inputs).
// Returns the event count; events are valid until the next poll call.
size_t pp_source_poll(PP_PadSource *s, const uint8_t *subscribed,
                      int allow);

#endif  // PAD_PROBE_SOURCE_H
