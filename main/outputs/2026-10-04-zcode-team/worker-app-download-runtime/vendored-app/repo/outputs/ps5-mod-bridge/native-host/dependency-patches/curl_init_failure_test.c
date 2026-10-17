/* Ordinary-host regression driver. No handles, requests, console code, or
 * entropy-owner reset. Observers call the real pinned Mbed TLS free functions. */
#include "entropy.h"
#include "clock_decl.h"
#include <curl/curl.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <psa/crypto.h>
#include <stdio.h>
#include <string.h>

static unsigned drbg_frees, entropy_frees, psa_frees;
static unsigned drbg_released = 1, entropy_released = 1;

static int all_zero(const void *data, size_t size)
{
    const unsigned char *p = data;
    for(size_t i = 0; i < size; ++i)
        if(p[i]) return 0;
    return 1;
}

void test_mbedtls_ctr_drbg_free(mbedtls_ctr_drbg_context *ctx)
{
    mbedtls_ctr_drbg_free(ctx);
    ++drbg_frees;
    if(!ctx) { drbg_released = 0; return; }
    /* The actual free routine zeroizes then restores these two defaults. */
    mbedtls_ctr_drbg_context copy;
    memcpy(&copy, ctx, sizeof(copy));
    drbg_released &= copy.reseed_counter == -1 &&
                     copy.reseed_interval == MBEDTLS_CTR_DRBG_RESEED_INTERVAL;
    copy.reseed_counter = copy.reseed_interval = 0;
    drbg_released &= all_zero(&copy, sizeof(copy));
}

void test_mbedtls_entropy_free(mbedtls_entropy_context *ctx)
{
    mbedtls_entropy_free(ctx);
    ++entropy_frees;
    if(!ctx) { entropy_released = 0; return; }
    mbedtls_entropy_context copy;
    memcpy(&copy, ctx, sizeof(copy));
    entropy_released &= copy.accumulator_started == -1 && copy.source_count == 0;
    copy.accumulator_started = 0;
    entropy_released &= all_zero(&copy, sizeof(copy));
}

void test_mbedtls_psa_crypto_free(void)
{
    mbedtls_psa_crypto_free();
    ++psa_frees;
}

int main(int argc, char **argv)
{
    if(argc != 2 || (strcmp(argv[1], "refuse") && strcmp(argv[1], "healthy")))
        return 2;
    unsigned refuse = !strcmp(argv[1], "refuse");
    if(clock_start() || process_source_begin(256, 256, 8192,
                                            refuse ? SOURCE_CURL_INIT : 0))
        return 2;
    psa_status_t psa = psa_crypto_init();
    if(psa != PSA_SUCCESS || process_source_phase(SOURCE_CURL_INIT))
        return 2;
    CURLcode first = curl_global_init(CURL_GLOBAL_DEFAULT);
    struct process_source_snapshot one, two;
    process_source_snapshot(&one);
    /* This negative regression call catches a false initialized latch. It
     * grants no production retry and does not rebind the terminal source. */
    CURLcode second = curl_global_init(CURL_GLOBAL_DEFAULT);
    process_source_snapshot(&two);
    curl_global_cleanup();
    curl_global_cleanup();
    /* Public curl cleanup does nothing after a failed init. The independent
     * PSA owner still performs its normal final teardown; this is idempotent. */
    mbedtls_psa_crypto_free();
    process_source_close();
    int outcome = refuse ? (first == CURLE_FAILED_INIT &&
                             second == CURLE_FAILED_INIT && two.failed &&
                             one.callbacks == two.callbacks &&
                             one.api_count == two.api_count) :
                            (first == CURLE_OK && second == CURLE_OK &&
                             !two.failed && one.callbacks > 0 && one.api_count > 0 &&
                             one.callbacks == two.callbacks &&
                             one.api_count == two.api_count);
    int released = drbg_frees == 1 && entropy_frees == 1 && psa_frees == 1 &&
                   drbg_released && entropy_released;
    int passed = outcome && released;
    printf("{\"passed\":%s,\"mode\":\"%s\",\"first\":%d,\"second\":%d,"
           "\"source_failed\":%u,\"callbacks_first\":%u,\"callbacks_second\":%u,"
           "\"api_first\":%u,\"api_second\":%u,\"drbg_frees\":%u,"
           "\"entropy_frees\":%u,\"psa_frees\":%u,\"drbg_released\":%u,"
           "\"entropy_released\":%u,\"console_executed\":false}\n",
           passed ? "true" : "false", argv[1], (int)first, (int)second,
           two.failed, one.callbacks, two.callbacks, one.api_count, two.api_count,
           drbg_frees, entropy_frees, psa_frees, drbg_released, entropy_released);
    return passed ? 0 : 1;
}
