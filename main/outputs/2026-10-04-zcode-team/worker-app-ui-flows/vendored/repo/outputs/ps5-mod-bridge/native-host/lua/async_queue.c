#include "async_queue.h"

#include <string.h>
#include "lauxlib.h"

static mb_async_queue *upvalue_queue(lua_State *lua) {
    return (mb_async_queue *)lua_touserdata(lua, lua_upvalueindex(1));
}

static void record_error(mb_async_queue *queue, const char *message,
                         size_t length) {
    size_t copied = length;
    if (queue->errors_total != UINT64_MAX)
        queue->errors_total++;
    if (copied >= sizeof(queue->last_error))
        copied = sizeof(queue->last_error) - 1u;
    if (copied != 0u)
        memcpy(queue->last_error, message, copied);
    queue->last_error[copied] = '\0';
}

static void remove_at(mb_async_queue *queue, size_t index) {
    luaL_unref(queue->lua, LUA_REGISTRYINDEX,
               queue->tasks[index].function_ref);
    if (index + 1u < queue->count) {
        memmove(&queue->tasks[index], &queue->tasks[index + 1u],
                (queue->count - index - 1u) * sizeof(queue->tasks[0]));
    }
    queue->count--;
    memset(&queue->tasks[queue->count], 0, sizeof(queue->tasks[0]));
}

static int enqueue(lua_State *lua, mb_async_queue *queue,
                   mb_async_kind kind, uint64_t delay_ms,
                   int function_index) {
    mb_async_task *task;
    if (queue == NULL || queue->closed)
        return luaL_error(lua, "async queue is closed");
    if (queue->count >= MB_ASYNC_MAX_PENDING)
        return luaL_error(lua, "async queue capacity exceeded");
    if (queue->next_sequence == UINT64_MAX)
        return luaL_error(lua, "async sequence space exhausted");
    if (delay_ms > MB_ASYNC_MAX_DELAY_MS ||
        queue->now_ms > UINT64_MAX - delay_ms)
        return luaL_error(lua, "async delay outside bounded range");

    lua_pushvalue(lua, function_index);
    task = &queue->tasks[queue->count];
    task->function_ref = luaL_ref(lua, LUA_REGISTRYINDEX);
    task->kind = (uint8_t)kind;
    task->delay_ms = delay_ms;
    task->due_ms = queue->now_ms + delay_ms;
    task->sequence = queue->next_sequence++;
    queue->count++;
    return 0;
}

static int lua_execute_async(lua_State *lua) {
    mb_async_queue *queue = upvalue_queue(lua);
    if (lua_gettop(lua) != 1 || !lua_isfunction(lua, 1))
        return luaL_error(lua, "ExecuteAsync expects one function");
    return enqueue(lua, queue, MB_ASYNC_IMMEDIATE, 0u, 1);
}

static int lua_delay(lua_State *lua, mb_async_kind kind) {
    mb_async_queue *queue = upvalue_queue(lua);
    lua_Integer signed_delay;
    uint64_t delay_ms;
    if (lua_gettop(lua) != 2 || !lua_isinteger(lua, 1) ||
        !lua_isfunction(lua, 2))
        return luaL_error(lua, "delayed async call expects (integer, function)");
    signed_delay = lua_tointeger(lua, 1);
    /* Upstream compares elapsed time against signed delay: negatives are immediately due. */
    delay_ms = signed_delay <= 0 ? 0u : (uint64_t)signed_delay;
    return enqueue(lua, queue, kind, delay_ms, 2);
}

static int lua_execute_with_delay(lua_State *lua) {
    return lua_delay(lua, MB_ASYNC_DELAYED);
}

static int lua_loop_async(lua_State *lua) {
    return lua_delay(lua, MB_ASYNC_LOOP);
}

static void bind_one(lua_State *lua, mb_async_queue *queue,
                     const char *name, lua_CFunction function) {
    lua_pushlightuserdata(lua, queue);
    lua_pushcclosure(lua, function, 1);
    lua_setglobal(lua, name);
}

int mb_async_queue_init(mb_async_queue *queue, lua_State *lua,
                        uint64_t initial_monotonic_ms) {
    if (queue == NULL || lua == NULL)
        return 0;
    memset(queue, 0, sizeof(*queue));
    queue->lua = lua;
    queue->now_ms = initial_monotonic_ms;
    queue->next_sequence = 1u;
    return 1;
}

int mb_async_queue_bind(mb_async_queue *queue) {
    if (queue == NULL || queue->lua == NULL || queue->closed)
        return 0;
    bind_one(queue->lua, queue, "ExecuteAsync", lua_execute_async);
    bind_one(queue->lua, queue, "ExecuteWithDelay", lua_execute_with_delay);
    bind_one(queue->lua, queue, "LoopAsync", lua_loop_async);
    return 1;
}

static size_t find_sequence(const mb_async_queue *queue, uint64_t sequence) {
    size_t index;
    for (index = 0u; index < queue->count; ++index) {
        if (queue->tasks[index].sequence == sequence)
            return index;
    }
    return queue->count;
}

static void invoke(mb_async_queue *queue, size_t index, int returns_one,
                   int *call_ok, int *loop_cancel) {
    lua_State *lua = queue->lua;
    int base = lua_gettop(lua);
    int status;
    *call_ok = 0;
    *loop_cancel = 0;
    lua_rawgeti(lua, LUA_REGISTRYINDEX, queue->tasks[index].function_ref);
    status = lua_pcall(lua, 0, returns_one ? 1 : 0, 0);
    if (status != LUA_OK) {
        size_t error_length = 0u;
        const char *error = lua_tolstring(lua, -1, &error_length);
        static const char fallback[] = "Lua callback failed";
        record_error(queue, error != NULL ? error : fallback,
                     error != NULL ? error_length : sizeof(fallback) - 1u);
    } else {
        *call_ok = 1;
        if (returns_one)
            *loop_cancel = lua_isboolean(lua, -1) && lua_toboolean(lua, -1);
    }
    lua_settop(lua, base);
}

int mb_async_queue_pump(mb_async_queue *queue, uint64_t monotonic_ms,
                        size_t *callbacks_run) {
    uint64_t snapshot[MB_ASYNC_MAX_PENDING];
    uint64_t cutoff;
    size_t snapshot_count = 0u;
    size_t ran = 0u;
    size_t i;
    if (callbacks_run != NULL)
        *callbacks_run = 0u;
    if (queue == NULL || queue->lua == NULL || queue->closed || queue->pumping ||
        monotonic_ms < queue->now_ms)
        return 0;
    queue->now_ms = monotonic_ms;
    queue->pumping = 1u;
    cutoff = queue->next_sequence - 1u;

    /* Snapshot task IDs: reentrant registrations cannot enter this pump. */
    for (i = 0u; i < queue->count; ++i) {
        if (queue->tasks[i].sequence > queue->last_scan_sequence &&
            queue->tasks[i].sequence <= cutoff)
            snapshot[snapshot_count++] = queue->tasks[i].sequence;
    }
    for (i = 0u; i < queue->count; ++i) {
        if (queue->tasks[i].sequence <= queue->last_scan_sequence &&
            queue->tasks[i].sequence <= cutoff)
            snapshot[snapshot_count++] = queue->tasks[i].sequence;
    }

    for (i = 0u; i < snapshot_count && ran < MB_ASYNC_CALLBACKS_PER_PUMP; ++i) {
        uint64_t sequence = snapshot[i];
        size_t index = find_sequence(queue, sequence);
        int call_ok, loop_cancel;
        uint8_t kind;
        uint64_t delay_ms;
        if (index == queue->count)
            continue;
        queue->last_scan_sequence = sequence;
        if (queue->tasks[index].due_ms > monotonic_ms)
            continue;
        kind = queue->tasks[index].kind;
        delay_ms = queue->tasks[index].delay_ms;
        invoke(queue, index, kind == MB_ASYNC_LOOP, &call_ok, &loop_cancel);
        ran++;
        if (queue->callbacks_total != UINT64_MAX)
            queue->callbacks_total++;
        if (kind != MB_ASYNC_LOOP || !call_ok || loop_cancel) {
            remove_at(queue, index);
        } else if (monotonic_ms > UINT64_MAX - delay_ms) {
            static const char overflow[] = "async loop deadline overflow";
            record_error(queue, overflow, sizeof(overflow) - 1u);
            remove_at(queue, index);
        } else {
            queue->tasks[index].due_ms = monotonic_ms + delay_ms;
        }
    }

    queue->pumping = 0u;
    if (callbacks_run != NULL)
        *callbacks_run = ran;
    return 1;
}

void mb_async_queue_close(mb_async_queue *queue) {
    if (queue == NULL || queue->closed)
        return;
    if (queue->lua != NULL) {
        while (queue->count != 0u)
            remove_at(queue, queue->count - 1u);
        lua_pushnil(queue->lua);
        lua_setglobal(queue->lua, "ExecuteAsync");
        lua_pushnil(queue->lua);
        lua_setglobal(queue->lua, "ExecuteWithDelay");
        lua_pushnil(queue->lua);
        lua_setglobal(queue->lua, "LoopAsync");
    }
    queue->closed = 1u;
}

size_t mb_async_queue_pending(const mb_async_queue *queue) {
    return queue != NULL ? queue->count : 0u;
}

uint64_t mb_async_queue_callbacks(const mb_async_queue *queue) {
    return queue != NULL ? queue->callbacks_total : 0u;
}

uint64_t mb_async_queue_errors(const mb_async_queue *queue) {
    return queue != NULL ? queue->errors_total : 0u;
}

const char *mb_async_queue_last_error(const mb_async_queue *queue) {
    return queue != NULL ? queue->last_error : "";
}
