#ifndef MODBRIDGE_DRBG_H
#define MODBRIDGE_DRBG_H
#include "entropy.h"

/* Cached scalar metadata only; no library contexts, private arrays or pointers
   to owned random storage escape through this ABI. Strings are fixed literals. */
struct mb_drbg_result {
    const char *status, *reason, *config_id, *mbedtls_version;
    int add_source_attempted, seed_attempted, generate_attempted;
    int add_source_rc, seed_rc, generate_rc;
    size_t completed_length;
    unsigned source_callback_count, source_os_call_count, total_api_call_count;
    unsigned api_call_limit;
    int64_t elapsed_ms;
    int source_failed, has_source_result;
    struct mb_entropy_result source_result;
    const char *policy_failure;
};

/* raw and drbg must be live distinct output records. Performs the single
   startup sequence and clears private owned storage on every returned path.
   Calls are synchronous and have no hard syscall deadline. */
void mb_random_checkpoint(struct mb_entropy_result *raw, struct mb_drbg_result *drbg);
#endif
