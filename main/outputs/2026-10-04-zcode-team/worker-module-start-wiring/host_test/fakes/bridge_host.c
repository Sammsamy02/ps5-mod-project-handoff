/* bridge_host.c — host fake of the C++ startup bridge with the exact
 * observable contract of runtime_start_bridge.cpp:
 *   - ownership: ALWAYS consumes (releases) a non-NULL activation;
 *   - success: MRS_OK, empty detail;
 *   - failure: MRS_ERR code from $MRS_FAKE_START_FAIL with $MRS_FAKE_START_DETAIL;
 *   - start/stop calls are recorded to $MRS_BRIDGE_LOG. */
#include "runtime_start_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* mfa_host.c's release function; the fake bridge mirrors the target ownership
 * contract (ActivatedMods deleter) by calling mfa_release itself. */
extern void mfa_release(struct mfa_activation *a);

static void record(const char *line)
{
    const char *p = getenv("MRS_BRIDGE_LOG");
    FILE *f = fopen(p != NULL ? p : "/tmp/mrs-bridge.log", "a");
    if (f != NULL) {
        fputs(line, f);
        fputc('\n', f);
        fclose(f);
    }
}

int modbridge_runtime_start_with_activation(const char *module_file_path,
                                            const char *mod_root,
                                            const char *title_id,
                                            struct mfa_activation *activation,
                                            char *detail,
                                            unsigned detail_capacity)
{
    char line[512];
    snprintf(line, sizeof(line), "bridge_start path=%s root=%s title=%s activation=%s",
             module_file_path != NULL ? module_file_path : "(null)",
             mod_root != NULL ? mod_root : "(null)",
             title_id != NULL ? title_id : "(null)",
             activation != NULL ? "non-null" : "null");
    record(line);

    /* Target ownership contract: the ActivatedMods deleter always runs. On
     * success the real runtime RETAINS the activation through the
     * NativeModSource; the fake records the consumption instead of leaking a
     * malloc'd byte, which is the same observable for the failure paths. */
    if (activation != NULL) {
        mfa_release(activation);
        record("activation_released");
    }

    if (detail != NULL && detail_capacity > 0u) {
        const char *d = getenv("MRS_FAKE_START_DETAIL");
        if (d != NULL) {
            unsigned used = 0;
            while (*d && used + 1u < detail_capacity)
                detail[used++] = *d++;
            detail[used] = '\0';
        } else {
            detail[0] = '\0';
        }
    }

    const char *f = getenv("MRS_FAKE_START_FAIL");
    return f != NULL ? atoi(f) : MRS_OK;
}

int modbridge_runtime_stop(void)
{
    record("bridge_stop");
    const char *r = getenv("MRS_FAKE_STOP_RESULT");
    return r != NULL ? atoi(r) : 0;
}
