#ifndef MB_EXPERIMENT_LUA_INPUT_REGISTRY_H
#define MB_EXPERIMENT_LUA_INPUT_REGISTRY_H
#include <stddef.h>
#include <stdint.h>
#include "lua.h"

#define MB_INPUT_BINDING_CAP 64u
#define MB_INPUT_MODIFIER_CAP 3u

typedef struct {
    int reference;
    uint8_t key, modifier_count, modifiers[MB_INPUT_MODIFIER_CAP];
} mb_input_binding;

typedef struct {
    lua_State *owner;
    mb_input_binding bindings[MB_INPUT_BINDING_CAP];
    uint32_t count, delivered, callback_errors, rejected;
    uint8_t installed, active, dispatching;
    char last_error[256];
} mb_input_registry;

typedef struct {
    uint8_t key, repeat;
    uint8_t held_keys[32];
} mb_input_keydown;

typedef enum {
    MB_INPUT_OK = 0, MB_INPUT_BAD_ARGUMENT, MB_INPUT_INACTIVE,
    MB_INPUT_BUSY, MB_INPUT_CALLBACK_ERROR
} mb_input_status;

/* Installs real registry-backed RegisterKeyBind(key, callback) and
 * RegisterKeyBind(key, {modifier-key bytes}, callback), returning no Lua values.
 * Subset: at most 64 bindings, three dense-array modifiers, key-down edges only.
 * A caller must supply actual key events; this module reads no input device.
 * No Unreal/game-thread globals, hooks, native DLLs or game objects are supplied.
 * The zeroed context and Lua state must have one serialized owner; context
 * storage outlives the Lua state, including after dispose. No reinstall.
 * Caller supplies Lua memory/instruction/time limits for registration and calls.
 */
int mb_input_install(lua_State *L, mb_input_registry *context);
/* Event is immutable/disjoint from registry storage. New registrations made
 * from a callback are eligible only for a later event. Reentrant dispatch fails.
 * Callback errors are recorded; subsequent existing bindings still run.
 * Event modifiers require those keys held; extra held keys are permitted.
 */
mb_input_status mb_input_dispatch(mb_input_registry *context,
                                 const mb_input_keydown *event);
/* Invoke only outside dispatch, before lua_close. Global closure then refuses
 * registration; no callback references remain. Returns false while dispatching.
 */
int mb_input_dispose(mb_input_registry *context);
#endif
