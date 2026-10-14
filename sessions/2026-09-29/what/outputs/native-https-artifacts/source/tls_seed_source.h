#ifndef EXP_TLS_SEED_SOURCE_H
#define EXP_TLS_SEED_SOURCE_H
#include "entropy.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#define TLS_SEED_OS_LIMIT 2u

/* Single-owner operation only. Objects and staging must be valid, distinct,
   live caller-owned storage. The shared budget is never initialized here.
   Cleanup ends the lifetime; do not rebind/reuse an operation after cleanup. */
struct tls_seed_source {
    struct mb_entropy_budget *budget;
    unsigned char *staging;
    struct mb_entropy_result last_actual;
    const char *failure;
    unsigned callback_count;
    unsigned os_call_count;
    unsigned bound;
    unsigned has_last;
    unsigned failed;
    unsigned closed;
};

struct tls_seed_operation {
    struct mb_entropy_budget budget;
    unsigned char staging[MB_ENTROPY_BYTES];
    struct tls_seed_source source;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr;
    unsigned entropy_initialized;
    unsigned ctr_initialized;
    unsigned seed_attempted;
    int add_source_rc;
    int seed_rc;
};

/* Source starts from zeroed storage, shares the supplied existing budget,
   and cannot be rebound, retried, or reopened. */
int tls_seed_source_bind(struct tls_seed_source *source,
                         struct mb_entropy_budget *budget,
                         unsigned char staging[MB_ENTROPY_BYTES]);
int tls_seed_source_poll(void *state, unsigned char *output,
                         size_t capacity, size_t *olen);
void tls_seed_source_close(struct tls_seed_source *source);
/* Caller zeroes a fresh operation once and may preload its shared budget.
   The actual Mbed seed is attempted once; the source closes on every exit. */
int tls_seed_once(struct tls_seed_operation *operation);
void tls_seed_cleanup(struct tls_seed_operation *operation);
#endif
