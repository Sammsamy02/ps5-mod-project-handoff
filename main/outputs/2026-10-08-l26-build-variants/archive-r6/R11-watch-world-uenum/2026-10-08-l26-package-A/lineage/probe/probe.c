/* Experimental full-runtime startup: owned memory-only Lua input, no engine API. */
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <probe_config.h>
#include "probe_api.h"
#include "startup_input.h"
#include "memory_lua.h"
#include "../diagnostic.h"
#include "../import_table.h"

extern void __cxa_finalize(void *);
static struct startup_input input;
static pthread_t worker;
static int worker_created;

void probe_event(const char *tag)
{
    diagnostic_event(tag, 0);
}

int probe_runtime_validate_and_preload(void)
{
    int result = probe_imports_validate();
    if (result != 0)
        return result;
    void (*volatile finalizer)(void *) = __cxa_finalize;
    if (finalizer == NULL) {
        diagnostic_event("dso_finalizer_binding_missing", 0);
        return -10;
    }
    return startup_input_load(&input, PROBE_INPUT_PATH,
                              PROBE_INPUT_EXPECTED_BYTES, diagnostic_channel_fd());
}

void probe_runtime_log_status(void)
{
    diagnostic_event("full_runtime_constructors_complete", 0);
}

static void lua_report(void *context, const char *stage, int result)
{
    (void)context;
    diagnostic_event(stage, (uint32_t)result);
}

static int consume_lua(const unsigned char *bytes, size_t length, void *context)
{
    const struct mb_lua_expectation expected = {
        PROBE_LUA_MARKER, sizeof(PROBE_LUA_MARKER) - 1,
        PROBE_LUA_INTEGER, 65536
    };
    return mb_lua_memory_check(bytes, length, &expected, lua_report, context);
}

static void *worker_main(void *unused)
{
    (void)unused;
    diagnostic_event("worker_entry", 0);
    struct timespec remaining = {PROBE_DELAY_SECONDS, 0};
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        struct timespec rest;
        int result = nanosleep(&remaining, &rest);
        int error_number = result != 0 ? errno : 0;
        (void)diagnostic_fd_event(diagnostic_channel_fd(), "worker_sleep_result",
                                  result, error_number);
        if (result == 0) {
            diagnostic_event("worker_sleep_complete", 0);
            diagnostic_event("buffered_lua_begin", (uint32_t)input.length);
            result = startup_input_deliver(&input, consume_lua, NULL);
            diagnostic_event("buffered_lua_result", (uint32_t)result);
            return NULL;
        }
        if (error_number != EINTR) {
            diagnostic_event("worker_sleep_failed", (uint32_t)error_number);
            return NULL;
        }
        remaining = rest;
    }
    diagnostic_event("worker_sleep_retry_exhausted", 0);
    return NULL;
}

int probe_worker_start(void)
{
    if (worker_created)
        return 0;
    int result = pthread_create(&worker, NULL, worker_main, NULL);
    diagnostic_event("worker_create_result", (uint32_t)result);
    if (result == 0)
        worker_created = 1;
    return result;
}

int probe_worker_is_current(void)
{
    return worker_created && pthread_self() == worker;
}

int probe_worker_quiesce_join(void)
{
    if (!worker_created)
        return 0;
    if (probe_worker_is_current())
        return EDEADLK;
    int result = pthread_join(worker, NULL);
    diagnostic_event("worker_join_result", (uint32_t)result);
    if (result == 0)
        worker_created = 0;
    return result;
}

void probe_runtime_release_input(void)
{
    startup_input_release(&input);
}
