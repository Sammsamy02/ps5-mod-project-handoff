#ifndef MB_UE4SS_PORTABLE_ASYNC_QUEUE_H
#define MB_UE4SS_PORTABLE_ASYNC_QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include "lua.h"

#define MB_ASYNC_MAX_PENDING 64u
#define MB_ASYNC_CALLBACKS_PER_PUMP 16u
#define MB_ASYNC_MAX_DELAY_MS UINT64_C(86400000)
#define MB_ASYNC_ERROR_CAP 160u

typedef enum mb_async_kind {
    MB_ASYNC_IMMEDIATE = 1,
    MB_ASYNC_DELAYED = 2,
    MB_ASYNC_LOOP = 3
} mb_async_kind;

typedef struct mb_async_task {
    int32_t function_ref;
    uint8_t kind;
    uint64_t due_ms;
    uint64_t delay_ms;
    uint64_t sequence;
} mb_async_task;

typedef struct mb_async_queue {
    lua_State *lua;
    mb_async_task tasks[MB_ASYNC_MAX_PENDING];
    size_t count;
    uint64_t now_ms;
    uint64_t next_sequence;
    uint64_t last_scan_sequence;
    uint64_t callbacks_total;
    uint64_t errors_total;
    uint8_t pumping;
    uint8_t closed;
    char last_error[MB_ASYNC_ERROR_CAP];
} mb_async_queue;

/* Caller owns queue storage and the Lua state. Initialize before loading scripts. */
int mb_async_queue_init(mb_async_queue *queue, lua_State *lua,
                        uint64_t initial_monotonic_ms);
int mb_async_queue_bind(mb_async_queue *queue);

/* Serialized owner only. Each call samples caller-supplied monotonic time once. */
int mb_async_queue_pump(mb_async_queue *queue, uint64_t monotonic_ms,
                        size_t *callbacks_run);

/* Must run before lua_close; caller must stop executing Lua after close. */
void mb_async_queue_close(mb_async_queue *queue);

size_t mb_async_queue_pending(const mb_async_queue *queue);
uint64_t mb_async_queue_callbacks(const mb_async_queue *queue);
uint64_t mb_async_queue_errors(const mb_async_queue *queue);
const char *mb_async_queue_last_error(const mb_async_queue *queue);

#endif
