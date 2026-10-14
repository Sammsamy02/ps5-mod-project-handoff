/* Owned delta over pinned Mbed TLS 3.6.7. No vendor source is changed. */
#ifndef MODBRIDGE_CRYPTO_CONFIG_H
#define MODBRIDGE_CRYPTO_CONFIG_H

#define MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES
#define MBEDTLS_NO_PLATFORM_ENTROPY

#if defined(MBEDTLS_ENTROPY_ALT) || defined(MBEDTLS_CTR_DRBG_ALT) || \
    defined(MBEDTLS_ENTROPY_HARDWARE_ALT) || defined(MBEDTLS_ENTROPY_NV_SEED) || \
    defined(MBEDTLS_TEST_NULL_ENTROPY) || defined(MBEDTLS_ENTROPY_FORCE_SHA256) || \
    defined(MBEDTLS_CTR_DRBG_USE_128_BIT_KEY) || \
    defined(MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH) || \
    defined(MBEDTLS_THREADING_C) || defined(MBEDTLS_THREADING_PTHREAD) || \
    defined(MBEDTLS_THREADING_ALT) || defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO) || \
    defined(MBEDTLS_AES_ALT) || defined(MBEDTLS_AES_SETKEY_ENC_ALT) || \
    defined(MBEDTLS_AES_SETKEY_DEC_ALT) || defined(MBEDTLS_AES_ENCRYPT_ALT) || \
    defined(MBEDTLS_AES_DECRYPT_ALT) || defined(MBEDTLS_SHA512_ALT) || \
    defined(MBEDTLS_SHA512_PROCESS_ALT) || defined(MBEDTLS_PSA_INJECT_ENTROPY) || \
    defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
#error "Unsupported alternative to the owned SHA512/AES256 entropy/CTR-DRBG path"
#endif

#endif

/* crypto_effective.h re-enters here after BOTH public headers have derived
 * their settings. The user overlay alone is read too early in build_info.h. */
#ifdef MODBRIDGE_CRYPTO_CHECK_EFFECTIVE
#if !defined(MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES) || \
    !defined(MBEDTLS_NO_PLATFORM_ENTROPY) || !defined(MBEDTLS_ENTROPY_C) || \
    !defined(MBEDTLS_CTR_DRBG_C) || !defined(MBEDTLS_AES_C) || \
    !defined(MBEDTLS_MD_C) || !defined(MBEDTLS_SHA512_C) || \
    !defined(MBEDTLS_ENTROPY_SHA512_ACCUMULATOR) || \
    defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO) || defined(MBEDTLS_MD_SHA512_VIA_PSA)
#error "The effective crypto configuration must use legacy AES and SHA512"
#endif
#if MBEDTLS_ENTROPY_BLOCK_SIZE != 64 || MBEDTLS_ENTROPY_MAX_GATHER != 128 || \
    MBEDTLS_CTR_DRBG_KEYSIZE != 32 || MBEDTLS_CTR_DRBG_ENTROPY_LEN != 48 || \
    MBEDTLS_CTR_DRBG_ENTROPY_NONCE_LEN != 0 || MBEDTLS_CTR_DRBG_PR_OFF != 0 || \
    MBEDTLS_CTR_DRBG_RESEED_INTERVAL != 10000
#error "The effective crypto defaults differ from the owned checkpoint contract"
#endif
#if defined(__cplusplus)
static_assert(MBEDTLS_ENTROPY_MD == MBEDTLS_MD_SHA512, "SHA512 accumulator required");
#else
_Static_assert(MBEDTLS_ENTROPY_MD == MBEDTLS_MD_SHA512, "SHA512 accumulator required");
#endif
#endif
