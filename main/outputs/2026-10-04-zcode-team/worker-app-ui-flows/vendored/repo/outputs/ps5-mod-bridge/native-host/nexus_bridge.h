#ifndef MODBRIDGE_NEXUS_BRIDGE_H
#define MODBRIDGE_NEXUS_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#define MB_NEXUS_INPUT_LIMIT_BYTES 8388608u
#define MB_NEXUS_OUTPUT_LIMIT_BYTES 16777216u
#define MB_NEXUS_DOMAIN_LIMIT_BYTES 80u
#define MB_NEXUS_URL_LIMIT_BYTES 256u

enum mb_nexus_kind {
    MB_NEXUS_GAMES = 0, MB_NEXUS_MODS = 1, MB_NEXUS_MOD = 2,
    MB_NEXUS_FILES = 3, MB_NEXUS_OAUTH_USER = 4, MB_NEXUS_API_KEY_USER = 5,
    MB_NEXUS_REQUIREMENTS = 6
};
enum mb_nexus_status {
    MB_NEXUS_OK = 0, MB_NEXUS_INVALID_ARGUMENT = 1,
    MB_NEXUS_BUFFER_TOO_SMALL = 2, MB_NEXUS_INTERNAL_ERROR = 3,
    MB_NEXUS_INPUT_LIMIT = 4, MB_NEXUS_OUTPUT_LIMIT = 5
};

#ifdef __cplusplus
#define MB_NEXUS_NOEXCEPT noexcept
extern "C" {
#else
#define MB_NEXUS_NOEXCEPT
#endif

/* Decode metadata only. The caller must establish authenticated, complete HTTP
 * input separately; this function authorizes no account/download/install action.
 * All storage is caller-owned and mutually disjoint. Inputs remain immutable for
 * the call. Nothing is retained. Domain length is explicit (no NUL required).
 * Games/users use empty domain and zero mod ID; Mods uses a domain and zero ID;
 * Mod/Files require a domain and positive signed-63-bit ID; Requirements uses
 * a domain and positive signed-32-bit GraphQL Int source ID. Requirements
 * returns a requirements object, not records, and establishes no satisfaction.
 *
 * length is required. NULL pointers require zero associated length/capacity.
 * Success copies an entire NUL-terminated report; length excludes that NUL.
 * A size query/short buffer returns BUFFER_TOO_SMALL, supplies required length,
 * and empties writable output. Other ABI failures empty writable output and set
 * length=0. Metadata rejection is an owning JSON report, not an ABI failure.
 * Empty input is permitted and produces malformed metadata. A zero-capacity
 * output is not touched. Error reports never contain raw provider values.
 */
int mb_nexus_decode(int kind, const unsigned char *bytes, size_t available,
                    const char *domain, size_t domain_length,
                    uint64_t mod_id, char *output, size_t capacity,
                    size_t *length) MB_NEXUS_NOEXCEPT;

/* Build a fixed-origin HTTPS metadata URL only. Same context and whole-output
 * rules as decode; capacity must not exceed MB_NEXUS_URL_LIMIT_BYTES.
 * Invalid domain grammar is INVALID_ARGUMENT. No network operation, credentials
 * or download/install authorization are created. Inputs/output are disjoint.
 */
int mb_nexus_metadata_url(int kind, const char *domain, size_t domain_length,
                         uint64_t mod_id, char *output, size_t capacity,
                         size_t *length) MB_NEXUS_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#undef MB_NEXUS_NOEXCEPT
#endif
