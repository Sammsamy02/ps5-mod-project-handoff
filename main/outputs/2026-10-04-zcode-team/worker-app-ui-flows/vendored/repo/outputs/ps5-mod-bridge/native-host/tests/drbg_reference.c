/* Fixture only: direct public APIs, never the adapter/checkpoint under test.
   KAT: pinned tests/suites/test_suite_ctr_drbg.data:1..3, matching .function.
   Owned baseline: source index 0, two chunks (0x40+i), (0x80+i), default
   SHA512 entropy48/nonce0/PRoff, exact personalization, first random32. */
#include "crypto_effective.h"
#include "mbedtls/platform_util.h"
#include <stdio.h>
#include <string.h>

static int kat_entropy(void *state, unsigned char *out, size_t length) {
    size_t *offset = state;
    if (*offset + length > 48) return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    for (size_t i = 0; i < length; ++i) out[i] = (unsigned char)(*offset + i);
    *offset += length;
    return 0;
}

static int kat(void) {
    static const unsigned char expected[32] = {
        0x8d,0xa6,0xcc,0x59,0xe7,0x03,0xce,0xd0,0x7d,0x58,0xd9,0x6e,0x5b,0x6d,0x78,0x36,
        0xc3,0x25,0x99,0x73,0x5b,0x73,0x4f,0x88,0xc1,0xa7,0x3b,0x53,0xc7,0xa6,0xd8,0x2e
    };
    mbedtls_ctr_drbg_context context;
    unsigned char custom[16], output[32];
    size_t offset = 0;
    int rc, ok = 0;
    for (size_t i = 0; i < sizeof(custom); ++i) custom[i] = (unsigned char)(0x20 + i);
    mbedtls_ctr_drbg_init(&context);
    mbedtls_ctr_drbg_set_entropy_len(&context, 48);
    rc = mbedtls_ctr_drbg_set_nonce_len(&context, 0);
    if (!rc) rc = mbedtls_ctr_drbg_seed(&context, kat_entropy, &offset, custom, sizeof(custom));
    if (!rc) rc = mbedtls_ctr_drbg_random_with_add(&context, output, sizeof(output), NULL, 0);
    if (!rc) rc = mbedtls_ctr_drbg_random_with_add(&context, output, sizeof(output), NULL, 0);
    if (!rc) ok = offset == 48 && memcmp(output, expected, sizeof(output)) == 0;
    mbedtls_ctr_drbg_free(&context);
    mbedtls_platform_zeroize(&context, sizeof(context));
    mbedtls_platform_zeroize(custom, sizeof(custom));
    mbedtls_platform_zeroize(output, sizeof(output));
    return ok;
}

static int reference_source(void *state, unsigned char *out, size_t capacity, size_t *length) {
    unsigned *calls = state;
    *length = 0;
    if (capacity != 128 || *calls >= 2) return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    ++*calls;
    for (size_t i = 0; i < 32; ++i) out[i] = (unsigned char)(*calls * 0x40 + i);
    *length = 32;
    return 0;
}

int main(int argc, char **argv) {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr;
    unsigned char output[32] = {0};
    unsigned calls = 0;
    int rc = 0, ok = kat();
    const unsigned char personal[] = "modbridge-owned-drbg-checkpoint-v1";
    if (!ok) { puts("{\"kat\":false}"); return 1; }
    if (argc == 1) { puts("{\"kat\":true}"); return 0; }
    if (argc != 3 || strcmp(argv[1], "--freeze")) return 2;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr);
    rc = mbedtls_entropy_add_source(&entropy, reference_source, &calls, 32, MBEDTLS_ENTROPY_SOURCE_STRONG);
    if (!rc) rc = mbedtls_ctr_drbg_seed(&ctr, mbedtls_entropy_func, &entropy, personal, sizeof(personal) - 1);
    if (!rc) rc = mbedtls_ctr_drbg_random(&ctr, output, sizeof(output));
    if (!rc && calls == 2) {
        FILE *file = fopen(argv[2], "wb");
        if (!file) rc = -1;
        else {
            if (fwrite(output, 1, sizeof(output), file) != sizeof(output)) rc = -1;
            if (fclose(file)) rc = -1;
        }
    } else rc = -1;
    mbedtls_ctr_drbg_free(&ctr);
    mbedtls_entropy_free(&entropy);
    mbedtls_platform_zeroize(&ctr, sizeof(ctr));
    mbedtls_platform_zeroize(&entropy, sizeof(entropy));
    mbedtls_platform_zeroize(output, sizeof(output));
    printf("{\"kat\":true,\"owned_reference_frozen\":%s,\"source_calls\":%u}\n", rc ? "false" : "true", calls);
    return rc ? 1 : 0;
}
