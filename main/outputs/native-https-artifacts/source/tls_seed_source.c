#include "tls_seed_source.h"
#include <string.h>
#if !HOST_NATIVE
#error "this experiment consumes the reviewed native reader"
#endif

static void owned_clear(void *storage, size_t capacity) {
    volatile unsigned char *bytes = storage;
    for (size_t i = 0; i < capacity; ++i) bytes[i] = 0;
}
static void latch(struct tls_seed_source *source, const char *reason) {
    if (source && !source->failed) {
        source->failed = 1;
        source->failure = reason;
    }
}

int tls_seed_source_bind(struct tls_seed_source *source,
                         struct mb_entropy_budget *budget,
                         unsigned char staging[MB_ENTROPY_BYTES]) {
    if (!source || !budget || !staging) {
        latch(source, "invalid_argument");
        if (source && source->staging) owned_clear(source->staging, MB_ENTROPY_BYTES);
        if (staging) owned_clear(staging, MB_ENTROPY_BYTES);
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }
    if (source->bound || source->closed || source->failed) {
        latch(source, "source_rebind");
        if (source->staging) owned_clear(source->staging, MB_ENTROPY_BYTES);
        owned_clear(staging, MB_ENTROPY_BYTES);
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }
    source->budget = budget;
    source->staging = staging;
    source->bound = 1;
    owned_clear(staging, MB_ENTROPY_BYTES);
    return 0;
}

int tls_seed_source_poll(void *state, unsigned char *output,
                         size_t capacity, size_t *olen) {
    struct tls_seed_source *source = state;
    int bounded_output = output && capacity >= MB_ENTROPY_BYTES && capacity <= 128;
    if (olen) *olen = 0;
    if (source) ++source->callback_count;
    if (!source || !source->bound || !source->budget || !source->staging ||
        !olen || !bounded_output) {
        latch(source, "invalid_argument");
        goto failed;
    }
    if (source->failed) goto failed;
    if (source->closed) { latch(source, "source_closed"); goto failed; }
    if (source->os_call_count >= TLS_SEED_OS_LIMIT) {
        latch(source, "seed_attempts_exhausted");
        goto failed;
    }
    if (source->budget->api_count >= MB_ENTROPY_API_LIMIT) {
        latch(source, "shared_budget_exhausted");
        goto failed;
    }
    /* The unchanged reader alone reserves this shared budget before the OS.
       Policy denials never replace the last actual API metadata. */
    struct mb_entropy_result actual;
    int rc = mb_entropy_read32(&actual, source->staging, source->budget);
    if (actual.call_count) {
        source->last_actual = actual;
        source->has_last = 1;
        source->os_call_count += actual.call_count;
    }
    if (rc) { latch(source, "api_failed"); goto failed; }
    memcpy(output, source->staging, MB_ENTROPY_BYTES);
    *olen = MB_ENTROPY_BYTES;
    owned_clear(source->staging, MB_ENTROPY_BYTES);
    return 0;
failed:
    if (source && source->staging) owned_clear(source->staging, MB_ENTROPY_BYTES);
    if (bounded_output) owned_clear(output, capacity);
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}

void tls_seed_source_close(struct tls_seed_source *source) {
    if (!source) return;
    source->closed = 1;
    if (source->staging) owned_clear(source->staging, MB_ENTROPY_BYTES);
}

int tls_seed_once(struct tls_seed_operation *operation) {
    static const unsigned char personalization[] = "modbridge-owned-https-fixture-v1";
    if (!operation) return MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
    if (operation->seed_attempted) {
        latch(&operation->source, "seed_reentry");
        tls_seed_source_close(&operation->source);
        return MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
    }
    operation->seed_attempted = 1;
    if (tls_seed_source_bind(&operation->source, &operation->budget,
                             operation->staging)) {
        operation->seed_rc = MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
        goto finished;
    }
    mbedtls_entropy_init(&operation->entropy);
    operation->entropy_initialized = 1;
    mbedtls_ctr_drbg_init(&operation->ctr);
    operation->ctr_initialized = 1;
    operation->add_source_rc = mbedtls_entropy_add_source(&operation->entropy,
        tls_seed_source_poll, &operation->source, 64, MBEDTLS_ENTROPY_SOURCE_STRONG);
    if (operation->add_source_rc) {
        latch(&operation->source, "add_source_failed");
        operation->seed_rc = MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
        goto finished;
    }
    operation->seed_rc = mbedtls_ctr_drbg_seed(&operation->ctr, mbedtls_entropy_func,
        &operation->entropy, personalization, sizeof(personalization) - 1);
finished:
    tls_seed_source_close(&operation->source);
    return operation->seed_rc;
}

void tls_seed_cleanup(struct tls_seed_operation *operation) {
    if (!operation) return;
    if (operation->ctr_initialized) mbedtls_ctr_drbg_free(&operation->ctr);
    if (operation->entropy_initialized) mbedtls_entropy_free(&operation->entropy);
    /* This can clear live owned storage, not expired library-local frames. */
    owned_clear(operation, sizeof(*operation));
}
