#include <mbedtls/sha1.h>
#include <mbedtls/private_access.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(MBEDTLS_SHA1_ALT)
#error "format SHA1 provider must use the canonical Mbed TLS context"
#endif

_Static_assert(sizeof(mbedtls_sha1_context) == 92u,
               "unexpected Mbed TLS 3.6.7 SHA1 context size");
_Static_assert(offsetof(mbedtls_sha1_context, MBEDTLS_PRIVATE(total)) == 0u,
               "unexpected SHA1 total offset");
_Static_assert(offsetof(mbedtls_sha1_context, MBEDTLS_PRIVATE(state)) == 8u,
               "unexpected SHA1 state offset");
_Static_assert(offsetof(mbedtls_sha1_context, MBEDTLS_PRIVATE(buffer)) == 28u,
               "unexpected SHA1 buffer offset");

static int matches(const unsigned char digest[20], const char expected[41]) {
    static const char hex[] = "0123456789abcdef";
    char actual[41];
    for (size_t i = 0; i < 20; ++i) {
        actual[i * 2] = hex[digest[i] >> 4];
        actual[i * 2 + 1] = hex[digest[i] & 15u];
    }
    actual[40] = '\0';
    return memcmp(actual, expected, sizeof(actual)) == 0;
}

int main(void) {
    static const unsigned char abc[] = "abc";
    static const unsigned char long_vector[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    unsigned char digest[20];
    mbedtls_sha1_context ctx;

    if (mbedtls_sha1(abc, sizeof(abc) - 1u, digest) != 0 ||
        !matches(digest, "a9993e364706816aba3e25717850c26c9cd0d89d")) {
        return 1;
    }

    mbedtls_sha1_init(&ctx);
    if (mbedtls_sha1_starts(&ctx) != 0 ||
        mbedtls_sha1_update(&ctx, long_vector, 19u) != 0 ||
        mbedtls_sha1_update(&ctx, long_vector + 19u,
                            sizeof(long_vector) - 1u - 19u) != 0 ||
        mbedtls_sha1_finish(&ctx, digest) != 0 ||
        !matches(digest, "84983e441c3bd26ebaae4aa1f95129e5e54670f1")) {
        mbedtls_sha1_free(&ctx);
        return 1;
    }
    mbedtls_sha1_free(&ctx);
    return 0;
}
