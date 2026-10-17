#include <mbedtls/private_access.h>
#include <mbedtls/sha1.h>

#include <stddef.h>

#if defined(MBEDTLS_SHA1_ALT)
#error "the format and transport profiles must share the canonical context"
#endif

_Static_assert(sizeof(mbedtls_sha1_context) == 92u,
               "Mbed TLS 3.6.7 SHA1 context size changed");
_Static_assert(offsetof(mbedtls_sha1_context, MBEDTLS_PRIVATE(total)) == 0u,
               "Mbed TLS 3.6.7 SHA1 total offset changed");
_Static_assert(offsetof(mbedtls_sha1_context, MBEDTLS_PRIVATE(state)) == 8u,
               "Mbed TLS 3.6.7 SHA1 state offset changed");
_Static_assert(offsetof(mbedtls_sha1_context, MBEDTLS_PRIVATE(buffer)) == 28u,
               "Mbed TLS 3.6.7 SHA1 buffer offset changed");

int mb_format_sha1_context_abi_probe(void) {
    return (int) sizeof(mbedtls_sha1_context);
}
