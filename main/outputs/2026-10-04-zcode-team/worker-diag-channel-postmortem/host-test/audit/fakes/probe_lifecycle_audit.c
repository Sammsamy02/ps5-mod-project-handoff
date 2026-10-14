/* probe_lifecycle_audit.c — fake probe bindings + fake lifecycle state
 * machine for the DELIVERED-entry audit. The entry's own branches are under
 * test; probe_lifecycle_start/stop are replaced by an env-controlled stub:
 *   MRS_FAKE_START_RC   return code of probe_lifecycle_start (default 0)
 *   MRS_FAKE_FINAL_STATE final lifecycle state (0 NEW,1 ACTIVE,2 QUIESCING,
 *                        3 FINISHED; default 1 ACTIVE)
 *   MRS_RECURSIVE_REENTRY=1 -> the stub re-calls module_start while the
 *                        outer call still holds entry_guard (-20 probe).
 * Constructors are counted so the positive controls prove the real op table
 * was driven. */
#include "lifecycle.h"
#include <stdio.h>
#include <stdlib.h>

int module_start(size_t argc, const void *args);
int module_stop(size_t argc, const void *args);

int audit_constructor_calls;

static int env_int(const char *name, int fallback)
{
    const char *v = getenv(name);
    return v != NULL ? atoi(v) : fallback;
}

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
}

int probe_call_constructor_catching(probe_callback callback)
{
    ++audit_constructor_calls;
    if (callback)
        callback();
    return 0;
}

int probe_lifecycle_start(struct probe_lifecycle *lc, const struct probe_lifecycle_ops *ops,
                          probe_callback *init_begin, probe_callback *init_end,
                          probe_callback *fini_begin, probe_callback *fini_end,
                          const void *eh_start, const void *eh_end,
                          const void *eh_hdr_start, const void *eh_hdr_end)
{
    (void)ops; (void)init_begin; (void)init_end; (void)fini_begin; (void)fini_end;
    (void)eh_start; (void)eh_end; (void)eh_hdr_start; (void)eh_hdr_end;
    if (getenv("MRS_RECURSIVE_REENTRY") != NULL) {
        int inner = module_start(0, NULL);
        printf("inner_module_start=%d\n", inner);
    }
    if (lc != NULL)
        lc->state = (enum probe_lifecycle_state)env_int("MRS_FAKE_FINAL_STATE", 1);
    return env_int("MRS_FAKE_START_RC", 0);
}

int probe_lifecycle_stop(struct probe_lifecycle *lc, const struct probe_lifecycle_ops *ops)
{
    (void)ops;
    if (lc != NULL)
        lc->state = PROBE_FINISHED;
    return 0;
}

/* ---- ELF boundary symbols the delivered entry passes to lifecycle_start --
 * One-element arrays so the constructor_count event observes 1 on the host. */
probe_callback __init_array_start[1];
probe_callback __init_array_end[1];
probe_callback __fini_array_start[1];
probe_callback __fini_array_end[1];
unsigned char __eh_frame_start[1];
unsigned char __eh_frame_end[1];
unsigned char __eh_frame_hdr_start[1];
unsigned char __eh_frame_hdr_end[1];
