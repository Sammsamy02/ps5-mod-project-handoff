#ifndef MODBRIDGE_NEXUS_DOWNLOAD_H
#define MODBRIDGE_NEXUS_DOWNLOAD_H

#include <stddef.h>
#include <stdint.h>

#define MB_NEXUS_DOWNLOAD_ABI_V1 1u
#define MB_NEXUS_DOWNLOAD_CREDENTIAL_LIMIT_BYTES 16384u
#define MB_NEXUS_DOWNLOAD_KEY_LIMIT_BYTES 1024u
#define MB_NEXUS_DOWNLOAD_DOMAIN_LIMIT_BYTES 80u
#define MB_NEXUS_DOWNLOAD_CA_LIMIT_BYTES 1048576u
#define MB_NEXUS_DOWNLOAD_RAW_LIMIT_BYTES 8388608u
#define MB_NEXUS_DOWNLOAD_REPORT_LIMIT_BYTES 16777216u
#define MB_NEXUS_DOWNLOAD_URI_LIMIT_BYTES 8192u
#define MB_NEXUS_DOWNLOAD_TIMEOUT_LIMIT_MS 300000u

typedef enum mb_nexus_download_auth_v1 {
    MB_NEXUS_DOWNLOAD_API_KEY = 1,
    MB_NEXUS_DOWNLOAD_BEARER = 2
} mb_nexus_download_auth_v1;

typedef enum mb_nexus_download_status_v1 {
    MB_NEXUS_DOWNLOAD_OK = 0,
    MB_NEXUS_DOWNLOAD_BAD_ARGUMENT = 1,
    MB_NEXUS_DOWNLOAD_AUTH_REQUIRED = 2,
    MB_NEXUS_DOWNLOAD_CONTENT_HIDDEN = 3,
    MB_NEXUS_DOWNLOAD_TRANSPORT_FAILED = 4,
    MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED = 5,
    MB_NEXUS_DOWNLOAD_TIMEOUT = 6,
    MB_NEXUS_DOWNLOAD_CANCELLED = 7,
    MB_NEXUS_DOWNLOAD_OUTPUT_TOO_SMALL = 8,
    MB_NEXUS_DOWNLOAD_INTERNAL_ERROR = 9
} mb_nexus_download_status_v1;

typedef enum mb_nexus_download_stage_v1 {
    MB_NEXUS_DOWNLOAD_STAGE_ADMISSION = 0,
    MB_NEXUS_DOWNLOAD_STAGE_PROFILE = 1,
    MB_NEXUS_DOWNLOAD_STAGE_AUTHORIZATION = 2,
    MB_NEXUS_DOWNLOAD_STAGE_MOD = 3,
    MB_NEXUS_DOWNLOAD_STAGE_LINK = 4,
    MB_NEXUS_DOWNLOAD_STAGE_COMPLETE = 5
} mb_nexus_download_stage_v1;

typedef int (*mb_nexus_download_cancel_fn_v1)(void *, uint64_t received);

typedef struct mb_nexus_download_grant_v1 {
    uint32_t struct_size, abi_version;
    const char *domain;
    size_t domain_bytes;
    uint64_t mod_id, file_id, user_id, expires;
    const char *key;
    size_t key_bytes;
} mb_nexus_download_grant_v1;

typedef struct mb_nexus_download_params_v1 {
    uint32_t struct_size, abi_version;
    uint32_t auth_mode;
    const char *credential;
    size_t credential_bytes;
    const char *domain;
    size_t domain_bytes;
    uint64_t mod_id, file_id;
    const mb_nexus_download_grant_v1 *grant;
    const uint8_t *ca_pem;
    size_t ca_bytes;
    /* Optional address mappings for the exact provider host:443. TLS peer/name
     * checks against ca_pem remain mandatory; these are not trust overrides. */
    const char *api_resolve, *users_resolve;
    uint32_t timeout_ms, connect_timeout_ms;
    void *cancel_context;
    mb_nexus_download_cancel_fn_v1 cancelled;
    uint8_t *raw_body;
    size_t raw_capacity;
    char *metadata_report;
    size_t report_capacity;
    char *uri;
    size_t uri_capacity;
} mb_nexus_download_params_v1;

typedef struct mb_nexus_download_result_v1 {
    uint32_t struct_size, abi_version;
    mb_nexus_download_status_v1 status;
    mb_nexus_download_stage_v1 stage;
    uint32_t complete, is_premium, grant_used;
    /* -1: unknown; 0/1: explicitly false/true in the live user profile. */
    int32_t adult_content;
    uint64_t user_id, mod_id, file_id;
    size_t uri_bytes;
} mb_nexus_download_result_v1;

#ifdef __cplusplus
#define MB_NEXUS_DOWNLOAD_NOEXCEPT noexcept
extern "C" {
#else
#define MB_NEXUS_DOWNLOAD_NOEXCEPT
#endif

/* One serialized resolution through the already-started exclusive curl/PSA
 * runtime: fresh live user, entitlement/grant binding, visible mod, then a
 * complete provider link response. No runtime start/close, retry, auth refresh,
 * archive transfer, file write or install occurs. Calls/hooks must not race.
 * All inputs are borrowed and immutable until return. Credentials/key are
 * nonempty ASCII 33..126; strings with explicit sizes need no terminating NUL.
 * Domain is [a-z0-9][a-z0-9_-]{0,79}; all IDs/expiry are positive signed-63-bit.
 * A supplied grant must match domain/mod/file and the freshly fetched user,
 * and remain unexpired even for a premium user. There is no caller premium flag.
 *
 * Every specified span (including params/result, grant and its nested inputs)
 * must be valid, without address overflow, and mutually disjoint. Opaque cancel
 * context must remain valid and not alias writable buffers/result. Invalid
 * layout/alias arguments leave all storage untouched. Once layout is valid,
 * raw/report scratch is wiped across its full capacity on every return; URI
 * storage is wiped in full on failure. Callback exceptions fail as CANCELLED.
 * Scratch/output capacities are nonzero and limited by the constants above.
 * Resolve strings are optional NUL-terminated strings shorter than 8192 bytes.
 * timeout_ms (1..300000) is the whole operation's real monotonic budget;
 * connect_timeout_ms is nonzero and at most timeout_ms. The implementation
 * owns its clock samples and grant expiry checks, never caller clock claims.
 *
 * Only OK/complete=1 publishes a NUL-terminated provider-issued HTTPS URI;
 * uri_bytes excludes NUL. All mirrors must validate first, and final transport
 * cleanup, total deadline and grant expiry may revoke an initially selected
 * URI. Every failure has complete=0 and uri_bytes=0, including short output.
 * The URI may itself contain short-lived provider query credentials. A later
 * archive request must use it without account authorization headers.
 * Result contains scalar facts only: never a URL, key, credential or body.
 * Success establishes resolution for this live identity/source, and proves
 * no archive preparation, installation or game execution. Nothing is retained.
 */
int mb_nexus_resolve_download_v1(const mb_nexus_download_params_v1 *,
                               mb_nexus_download_result_v1 *) MB_NEXUS_DOWNLOAD_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#undef MB_NEXUS_DOWNLOAD_NOEXCEPT
#endif
