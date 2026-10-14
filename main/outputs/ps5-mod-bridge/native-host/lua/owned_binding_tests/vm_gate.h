#ifndef MB_OWNED_BINDING_TEST_VM_GATE_H
#define MB_OWNED_BINDING_TEST_VM_GATE_H

/* Test-only real-Lua allocator/text runner, promoted from the retained
 * ue4ss-lua-vm experiment. No ignored source is included by these tests. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lua.h"
#include "lualib.h"

#define LUA_MEMORY_CAP (256u * 1024u)
#define HOOK_INTERVAL 100u

typedef enum {
    ABORT_NONE = 0, ABORT_INSTRUCTIONS, ABORT_TIME, ABORT_MEMORY, ABORT_CLOCK
} abort_reason_t;
typedef struct {
    size_t live_bytes, peak_bytes, memory_cap;
    uint64_t instructions, instruction_cap, start_ns, time_cap_ns;
    abort_reason_t abort_reason;
    int allocator_denied;
} vm_context_t;
typedef struct { const char *bytes; size_t length; int delivered; } memory_reader_t;

static inline int monotonic_ns(uint64_t *value) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 ||
        now.tv_nsec < 0 || now.tv_nsec >= 1000000000L ||
        (uint64_t)now.tv_sec > (UINT64_MAX - (uint64_t)now.tv_nsec) / UINT64_C(1000000000))
        return 0;
    *value = (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
    return 1;
}
static inline void *bounded_allocator(void *user, void *pointer,
                                      size_t old_size, size_t new_size) {
    vm_context_t *context = (vm_context_t *)user;
    const size_t old_payload = pointer == NULL ? 0u : old_size;
    if (old_payload > context->live_bytes) {
        context->allocator_denied = 1;
        context->abort_reason = ABORT_MEMORY;
        return NULL;
    }
    const size_t retained = context->live_bytes - old_payload;
    if (new_size == 0u) {
        free(pointer);
        context->live_bytes = retained;
        return NULL;
    }
    if (retained > context->memory_cap || new_size > context->memory_cap - retained) {
        context->allocator_denied = 1;
        context->abort_reason = ABORT_MEMORY;
        return NULL;
    }
    void *replacement = realloc(pointer, new_size);
    if (replacement == NULL) return NULL;
    context->live_bytes = retained + new_size;
    if (context->live_bytes > context->peak_bytes) context->peak_bytes = context->live_bytes;
    return replacement;
}
static inline int raise_abort(lua_State *L, vm_context_t *context,
                               abort_reason_t reason, const char *message) {
    context->abort_reason = reason;
    lua_pushstring(L, message);
    return lua_error(L);
}
static inline void budget_hook(lua_State *L, lua_Debug *debug) {
    vm_context_t *context = *(vm_context_t **)lua_getextraspace(L);
    uint64_t now;
    (void)debug;
    context->instructions += HOOK_INTERVAL;
    if (context->instruction_cap && context->instructions > context->instruction_cap) {
        (void)raise_abort(L, context, ABORT_INSTRUCTIONS, "instruction budget exceeded");
        return;
    }
    if (context->time_cap_ns) {
        if (!monotonic_ns(&now) || now < context->start_ns) {
            (void)raise_abort(L, context, ABORT_CLOCK, "monotonic clock unavailable");
            return;
        }
        if (now - context->start_ns >= context->time_cap_ns)
            (void)raise_abort(L, context, ABORT_TIME, "time budget exceeded");
    }
}
static inline lua_State *new_limited_state(vm_context_t *context) {
    memset(context, 0, sizeof(*context));
    context->memory_cap = LUA_MEMORY_CAP;
    lua_State *L = lua_newstate(bounded_allocator, context);
    if (L != NULL) *(vm_context_t **)lua_getextraspace(L) = context;
    return L;
}
static inline int reset_budget(lua_State *L, vm_context_t *context,
                                uint64_t instruction_cap, uint64_t time_cap_ns) {
    context->instructions = 0u;
    context->instruction_cap = instruction_cap;
    context->time_cap_ns = time_cap_ns;
    context->abort_reason = ABORT_NONE;
    if (time_cap_ns && !monotonic_ns(&context->start_ns)) return 0;
    lua_sethook(L, budget_hook, LUA_MASKCOUNT, (int)HOOK_INTERVAL);
    return 1;
}
static inline const char *memory_reader(lua_State *L, void *user, size_t *size) {
    memory_reader_t *reader = (memory_reader_t *)user;
    (void)L;
    if (reader->delivered) { *size = 0u; return NULL; }
    reader->delivered = 1;
    *size = reader->length;
    return reader->bytes;
}
static inline int run_text(lua_State *L, const char *source) {
    memory_reader_t reader = {source, strlen(source), 0};
    lua_settop(L, 0);
    const int status = lua_load(L, memory_reader, &reader, "owned-fixture", "t");
    return status == LUA_OK ? lua_pcall(L, 0, 0, 0) : status;
}
static inline int global_integer(lua_State *L, const char *name, lua_Integer *value) {
    int is_number = 0;
    lua_getglobal(L, name);
    *value = lua_tointegerx(L, -1, &is_number);
    lua_pop(L, 1);
    return is_number;
}
static inline int global_boolean(lua_State *L, const char *name, int *value) {
    lua_getglobal(L, name);
    const int is_boolean = lua_isboolean(L, -1);
    *value = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return is_boolean;
}
static inline void close_state(lua_State *L, vm_context_t *context) {
    lua_close(L);
    if (context->live_bytes != 0u)
        (void)fprintf(stderr, "allocator leak after lua_close: %lu bytes\n",
                      (unsigned long)context->live_bytes);
}
#endif
