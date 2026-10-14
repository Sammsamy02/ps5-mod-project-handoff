#ifndef EXP_TLS_EFFECTIVE_H
#define EXP_TLS_EFFECTIVE_H
#include <stdint.h>
#include <limits.h>
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/version.h"
#include "mbedtls/bignum.h"
#if !defined(MBEDTLS_HAVE_TIME_DATE) || !defined(MBEDTLS_PLATFORM_TIME_MACRO) || !defined(MBEDTLS_PLATFORM_GMTIME_R_ALT)
#error "requires guarded wall clock and certificate dates"
#endif
#if !defined(MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES) || !defined(MBEDTLS_NO_PLATFORM_ENTROPY)
#error "requires explicit caller-owned entropy; automatic and platform sources disabled"
#endif
#if !defined(MBEDTLS_SHA384_C) || !defined(MBEDTLS_MD_CAN_SHA384) || !defined(MBEDTLS_ECP_DP_SECP384R1_ENABLED) || !defined(MBEDTLS_RSA_C) || !defined(MBEDTLS_PKCS1_V15)
#error "observed chain support missing"
#endif
#if defined(MBEDTLS_PSA_CRYPTO_C) || defined(MBEDTLS_USE_PSA_CRYPTO) || defined(MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO) || defined(MBEDTLS_SSL_PROTO_TLS1_3) || defined(MBEDTLS_SSL_SRV_C) || defined(MBEDTLS_SSL_SESSION_TICKETS) || defined(MBEDTLS_SSL_RENEGOTIATION) || defined(MBEDTLS_THREADING_C) || defined(MBEDTLS_ECP_RESTARTABLE) || defined(MBEDTLS_DEBUG_C) || defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED) || defined(MBEDTLS_KEY_EXCHANGE_RSA_ENABLED)
#error "experimental profile broadened"
#endif
_Static_assert(sizeof(time_t)==8 && (time_t)-1<0,"signed64 epoch required");
_Static_assert(sizeof(mbedtls_time_t)==sizeof(time_t),"time alias mismatch");
_Static_assert(MBEDTLS_ENTROPY_BLOCK_SIZE==64 && MBEDTLS_ENTROPY_MAX_GATHER==128,"accumulator mismatch");
_Static_assert(MBEDTLS_CTR_DRBG_ENTROPY_LEN==48 && MBEDTLS_CTR_DRBG_KEYSIZE==32,"DRBG mismatch");
_Static_assert(MBEDTLS_VERSION_NUMBER==0x03060700,"pinned Mbed version mismatch");
_Static_assert(MBEDTLS_ECP_MAX_BITS==384,"P256/P384 maximum curve size mismatch");
_Static_assert(MBEDTLS_MPI_MAX_SIZE>=512,"RSA4096 support capacity required");
#if !defined(MBEDTLS_NO_UDBL_DIVISION) || defined(MBEDTLS_HAVE_UDBL)
#error "requires upstream portable half-word division without double-width compiler helper"
#endif
#if !defined(MBEDTLS_HAVE_INT64) || defined(MBEDTLS_HAVE_INT32) || defined(MBEDTLS_NO_64BIT_MULTIPLICATION)
#error "preserve native 64-bit MPI limbs and upstream multiplication policy"
#endif
_Static_assert(sizeof(mbedtls_mpi_uint)==8,"native 64-bit MPI limbs required");
#endif
