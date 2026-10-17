#define _POSIX_C_SOURCE 200809L
#include "drbg_internal.h"
#if HOST_NATIVE
#include "mbedtls/version.h"
#include <string.h>
#include <time.h>
#ifndef MB_CRYPTO_CONFIG_ID
#error Native consumers require the identified crypto configuration
#endif

static void owned_clear(void *storage, size_t capacity) {
    volatile unsigned char *bytes = storage;
    for (size_t i = 0; i < capacity; ++i) bytes[i] = 0;
}
static int64_t nanoseconds(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) || value.tv_sec < 0 ||
        value.tv_sec > (INT64_MAX - 999999999) / 1000000000 ||
        value.tv_nsec < 0 || value.tv_nsec >= 1000000000L) return -1;
    return (int64_t)value.tv_sec * 1000000000 + value.tv_nsec;
}
int mb_drbg_source(void *state, unsigned char *output, size_t capacity, size_t *olen) {
    struct mb_drbg_source_state *source = state;
    int bounded_output = output && capacity >= MB_ENTROPY_BYTES && capacity <= 128;
    if (olen) *olen = 0;
    if (source) ++source->callback_count;
    if (!source || !source->budget || !source->staging || !olen || !bounded_output) {
        if (source) {
            source->failed = 1;
            if (!source->policy_failure) source->policy_failure = "invalid_argument";
        }
        goto failed;
    }
    if (source->failed) goto failed;
    if (source->budget->api_count >= MB_ENTROPY_API_LIMIT) {
        source->failed = 1;
        source->policy_failure = "budget_exhausted";
        goto failed;
    }
    struct mb_entropy_result actual;
    int rc = mb_entropy_read32(&actual, source->staging, source->budget);
    if (actual.call_count) {
        source->last = actual;
        source->has_last = 1;
        source->os_call_count += actual.call_count;
    }
    if (rc) { source->failed = 1; goto failed; }
    memcpy(output, source->staging, MB_ENTROPY_BYTES);
    *olen = MB_ENTROPY_BYTES;
    owned_clear(source->staging, MB_ENTROPY_BYTES);
    return 0;
failed:
    if (source && source->staging) owned_clear(source->staging, MB_ENTROPY_BYTES);
    if (bounded_output) owned_clear(output, capacity);
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}

void mb_drbg_cleanup(struct mb_drbg_workspace *workspace) {
    if (workspace->ctr_initialized) mbedtls_ctr_drbg_free(&workspace->ctr);
    if (workspace->entropy_initialized) mbedtls_entropy_free(&workspace->entropy);
    /* Free restores nonzero lifecycle defaults; explicitly erase the entire
       still-live caller-owned workspace after library cleanup. */
    owned_clear(workspace, sizeof(*workspace));
}

void mb_random_checkpoint_in(struct mb_entropy_result *raw, struct mb_drbg_result *result,
                             struct mb_drbg_workspace *workspace) {
    static const unsigned char personalization[] = "modbridge-owned-drbg-checkpoint-v1";
    *workspace = (struct mb_drbg_workspace){0};
    *result = (struct mb_drbg_result){
        .status="unavailable", .reason="raw_prerequisite_failed",
        .config_id=MB_CRYPTO_CONFIG_ID, .mbedtls_version=MBEDTLS_VERSION_STRING,
        .api_call_limit=MB_ENTROPY_API_LIMIT, .elapsed_ms=-1
    };
    workspace->source.budget = &workspace->budget;
    workspace->source.staging = workspace->staging;
    int64_t before = nanoseconds();
    int raw_rc = mb_entropy_read32(raw, workspace->staging, &workspace->budget);
    owned_clear(workspace->staging, sizeof(workspace->staging));
    if (raw_rc) goto cleanup;
    mbedtls_entropy_init(&workspace->entropy);
    workspace->entropy_initialized = 1;
    mbedtls_ctr_drbg_init(&workspace->ctr);
    workspace->ctr_initialized = 1;
    result->status = "failed";
    result->reason = "add_source_failed";
    result->add_source_attempted = 1;
    result->add_source_rc = mbedtls_entropy_add_source(&workspace->entropy, mb_drbg_source,
        &workspace->source, MB_ENTROPY_BYTES, MBEDTLS_ENTROPY_SOURCE_STRONG);
    if (result->add_source_rc) goto cleanup;
    result->reason = "seed_failed";
    result->seed_attempted = 1;
    result->seed_rc = mbedtls_ctr_drbg_seed(&workspace->ctr, mbedtls_entropy_func,
        &workspace->entropy, personalization, sizeof(personalization)-1);
    if (result->seed_rc) goto cleanup;
    result->reason = "generate_failed";
    result->generate_attempted = 1;
    result->generate_rc = mbedtls_ctr_drbg_random(&workspace->ctr,
        workspace->generation, sizeof(workspace->generation));
    if (result->generate_rc) goto cleanup;
    result->completed_length = sizeof(workspace->generation);
    if (workspace->source.failed || workspace->source.callback_count != 2 ||
        workspace->source.os_call_count != 2 || workspace->budget.api_count != 3) {
        result->reason = "unexpected_call_counts";
        goto cleanup;
    }
#ifdef MB_DRBG_TESTING
    mb_drbg_test_observe(workspace->generation, sizeof(workspace->generation));
#endif
    result->status = "passed";
    result->reason = "checkpoint_complete";
cleanup:
    result->source_callback_count = workspace->source.callback_count;
    result->source_os_call_count = workspace->source.os_call_count;
    result->total_api_call_count = workspace->budget.api_count;
    result->source_failed = workspace->source.failed;
    result->has_source_result = workspace->source.has_last;
    result->source_result = workspace->source.last;
    result->policy_failure = workspace->source.policy_failure;
    mb_drbg_cleanup(workspace);
    int64_t after = nanoseconds();
    if (before >= 0 && after >= before) result->elapsed_ms = (after-before)/1000000;
}
#endif

void mb_random_checkpoint(struct mb_entropy_result *raw, struct mb_drbg_result *result) {
#if HOST_NATIVE
    struct mb_drbg_workspace workspace;
    mb_random_checkpoint_in(raw, result, &workspace);
#else
    unsigned char scratch[MB_ENTROPY_BYTES];
    mb_entropy_observe(raw, scratch);
    *result = (struct mb_drbg_result){
        .status="unavailable", .reason="desktop_fixture",
        .api_call_limit=MB_ENTROPY_API_LIMIT, .elapsed_ms=-1
    };
#endif
}
