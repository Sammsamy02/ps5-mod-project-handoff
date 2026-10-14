#ifndef MODBRIDGE_NEXUS_CLIENT_H
#define MODBRIDGE_NEXUS_CLIENT_H

#include <stddef.h>
#include <stdint.h>
#include "nexus_bridge.h"

#define MB_NEXUS_CLIENT_ABI_V1 1u
#define MB_NEXUS_CLIENT_CREDENTIAL_LIMIT_BYTES 16384u
#define MB_NEXUS_CLIENT_CA_LIMIT_BYTES 1048576u
#define MB_NEXUS_CLIENT_TIMEOUT_LIMIT_MS 300000u
#define MB_NEXUS_CLIENT_DECODE_NOT_ATTEMPTED (-1)

typedef enum mb_nexus_client_auth_v1 {
    MB_NEXUS_CLIENT_API_KEY = 1,
    MB_NEXUS_CLIENT_BEARER = 2
} mb_nexus_client_auth_v1;

typedef enum mb_nexus_client_status_v1 {
    MB_NEXUS_CLIENT_OK = 0,
    MB_NEXUS_CLIENT_BAD_ARGUMENT,
    MB_NEXUS_CLIENT_TRANSPORT_FAILED,
    MB_NEXUS_CLIENT_OUTPUT_TOO_SMALL,
    MB_NEXUS_CLIENT_DECODE_FAILED
} mb_nexus_client_status_v1;

typedef int (*mb_nexus_client_cancel_fn_v1)(void *, uint64_t received);

typedef struct mb_nexus_client_params_v1 {
    uint32_t struct_size, abi_version;
    int32_t kind;
    uint32_t auth_mode;
    const char *credential;
    size_t credential_bytes;
    const char *domain;
    size_t domain_bytes;
    uint64_t mod_id;
    const uint8_t *ca_pem;
    size_t ca_bytes;
    /* Optional trusted DNS mapping; the fixed provider hostname is still
     * authenticated against ca_pem. This is never a TLS trust override. */
    const char *resolve;
    uint32_t timeout_ms, connect_timeout_ms;
    void *cancel_context;
    mb_nexus_client_cancel_fn_v1 cancelled;
    uint8_t *raw_body;
    size_t raw_capacity;
    char *report;
    size_t report_capacity;
} mb_nexus_client_params_v1;

typedef struct mb_nexus_client_result_v1 {
    uint32_t struct_size, abi_version;
    mb_nexus_client_status_v1 status;
    uint32_t complete, metadata_decoded, http_status;
    int32_t curl_code, decode_status;
    uint64_t received_bytes;
    size_t report_bytes;
} mb_nexus_client_result_v1;

#ifdef __cplusplus
#define MB_NEXUS_CLIENT_NOEXCEPT noexcept
extern "C" {
#else
#define MB_NEXUS_CLIENT_NOEXCEPT
#endif

/* One serialized read-only request through the already-started exclusive
 * curl/PSA runtime. The URL is generated from kind/domain/mod_id, never supplied
 * by the caller. OAuthUser and Requirements require Bearer; ApiKeyUser
 * requires API_KEY; catalog metadata GET kinds accept either. Requirements
 * uses the fixed read-only v2 GraphQL query and positive INT32 source mod ID.
 * Credentials are borrowed, immutable,
 * nonempty ASCII bytes 33..126 (no terminating NUL required), and are never
 * retained, returned or logged. No auth acquisition/refresh/retry is performed.
 *
 * All specified storage spans, including params/result, must be valid and
 * mutually disjoint. Callback context remains valid and must not alias writable
 * storage. raw_capacity is 1..MB_NEXUS_INPUT_LIMIT_BYTES; report_capacity is
 * 0..MB_NEXUS_OUTPUT_LIMIT_BYTES. NULL pointers require zero associated size.
 * Invalid layout/alias arguments leave storage untouched. Once layout is
 * validated, all raw capacity is wiped before return, and all report capacity
 * is wiped on failure, including a transport/clock fault after decoding.
 * Nothing retains input, callback contexts, body bytes or reports.
 *
 * OK/complete=1 means a complete authenticated response produced one whole
 * NUL-terminated owning report. A malformed/resource-limit metadata envelope
 * is still OK; metadata_decoded=1 only for its status "decoded". These facts
 * establish no visibility, entitlement, download or installation authority.
 * report_bytes excludes NUL. A zero-capacity query or short buffer returns
 * OUTPUT_TOO_SMALL and its required report_bytes after transfer; raw bytes are
 * wiped, so a subsequent call makes another request. There is no automatic
 * refetch. Required length is retained only if transport finishes with the
 * consumer's short-buffer refusal, never after a later transport/owner fault.
 * Other failures return complete=metadata_decoded=0 and report_bytes=0.
 * decode_status is an mb_nexus_status, or DECODE_NOT_ATTEMPTED.
 */
int mb_nexus_client_request_v1(const mb_nexus_client_params_v1 *,
                              mb_nexus_client_result_v1 *) MB_NEXUS_CLIENT_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#undef MB_NEXUS_CLIENT_NOEXCEPT
#endif
