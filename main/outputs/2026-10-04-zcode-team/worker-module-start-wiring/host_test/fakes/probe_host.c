/* probe_host.c — host fakes for probe_api.h: the lifecycle machinery
 * (lifecycle.c) is REAL, only its probe bindings are faked. Constructors are
 * tracked so the test can prove the UE4SS units (init_array) ran. */
#include "lifecycle.h"
#include "probe_api.h"
#include <stdlib.h>

int probe_constructor_calls;
int probe_worker_started;
int probe_input_released;

void probe_event(const char *tag)
{
    (void)tag;
}

int probe_runtime_validate_and_preload(void)
{
    return 0;
}

void probe_runtime_log_status(void)
{
}

int probe_worker_start(void)
{
    ++probe_worker_started;
    return 0;
}

int probe_worker_quiesce_join(void)
{
    return 0;
}

int probe_worker_is_current(void)
{
    return 0;
}

void probe_runtime_release_input(void)
{
    ++probe_input_released;
}

int probe_call_constructor_catching(probe_callback callback)
{
    ++probe_constructor_calls;
    if (callback)
        callback();
    return 0;
}
