/* diagnostic_shim.h — declares the injected state shared between shims.c,
 * test_main.c, and the diagnostic.h sink shim. */
#ifndef DIAGNOSTIC_SHIM_H
#define DIAGNOSTIC_SHIM_H

#include <stdint.h>
#include "lifecycle.h"

extern int g_kernel_failover_open_result;
extern int g_diag_ready;
extern int g_diag_open_result;
extern int g_stub_start_rc;
extern int g_stub_recursive_reentry;
extern enum probe_lifecycle_state g_stub_final_state;
extern char g_failover_log_path[512];
extern char g_diag_channel_path[512];

struct probe_lifecycle;
struct probe_lifecycle *shim_last_lifecycle(void);

#endif
