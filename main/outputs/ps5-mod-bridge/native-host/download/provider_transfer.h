#ifndef MB_CURL_PROVIDER_TRANSFER_H
#define MB_CURL_PROVIDER_TRANSFER_H
#include <stddef.h>
#include <stdint.h>
#include "runtime.h"

#define MB_CURL_PROVIDER_BODY_MAX 8388608u
#define MB_CURL_PROVIDER_SECRET_MAX 16384u

typedef struct mb_curl_provider_transfer_params_v1 {
 uint32_t struct_size,abi_version;
 const char *url;
 uint32_t auth_mode; /* 1: API key; 2: bearer. */
 const char *credential;
 size_t credential_bytes;
 const uint8_t *ca_pem;
 size_t ca_bytes;
 const char *resolve;
 uint32_t timeout_ms,connect_timeout_ms;
 void *cancel_context;
 mb_curl_cancel_fn_v1 cancelled;
 uint8_t *body;
 size_t body_capacity;
 void *consume_context;
 int (*consume_complete_body)(void *,const uint8_t *,size_t);
} mb_curl_provider_transfer_params_v1;
typedef struct mb_curl_provider_transfer_result_v1 {
 uint32_t struct_size,abi_version;
 mb_curl_download_status_v1 status;
 uint32_t complete,http_status,reserved;
 int32_t curl_code;
 uint64_t received_bytes;
} mb_curl_provider_transfer_result_v1;

/* Additive fixed-query POST layout; the existing GET layout is unchanged. */
typedef struct mb_curl_provider_post_transfer_params_v1 {
 uint32_t struct_size,abi_version;
 mb_curl_provider_transfer_params_v1 provider;
 const char *request_body;
 size_t request_body_bytes;
} mb_curl_provider_post_transfer_params_v1;

#ifdef __cplusplus
extern "C" {
#endif

/* Private transport for the typed Nexus metadata wrapper. It accepts only
 * HTTPS api.nexusmods.com/v1/... or users.nexusmods.com/oauth/userinfo on 443,
 * without query, fragment or userinfo. The users endpoint requires bearer.
 * No arbitrary headers, redirects, cookies, proxy or account refresh are used.
 * Resolve can map addresses for that exact host:443; TLS trust/name verification
 * remains mandatory. Credential is immutable, 1..16384 bytes in ASCII 33..126,
 * borrowed only for the synchronous operation and never retained or logged.
 * All supplied buffers/params/result and immutable byte inputs are disjoint.
 * A valid independent body buffer is wiped in full on every return. Invalid or
 * aliased storage is untouched. The consumer runs exactly once only after a
 * complete verified HTTP 200 transfer, inside the shared owner window/busy gate.
 * Its zero return accepts the body; nonzero is CONSUMER_FAILED. It must not throw,
 * retain the borrowed body, race hooks or publish until final transport success.
 * Late timeout/owner failure can revoke complete after the consumer returned.
 * No account entitlement, download authorization or install action is created. */
int mb_curl_provider_transfer_v1(const mb_curl_provider_transfer_params_v1 *,
                                mb_curl_provider_transfer_result_v1 *);

/* Private read-only requirements POST. Only the exact fixed /v2/graphql URL
 * and canonical requirements body declared in nexus_requirements_query.h are
 * accepted: a lower-case safe domain and positive INT32 mod ID, with no other
 * query, mutation, fields, whitespace variants or trailing bytes. The request
 * is an immutable borrowed byte span of at most 4096 bytes and need not be NUL
 * terminated; it stays alive through synchronous curl cleanup. JSON Content-
 * Type is explicit. All GET credential, owner, response, consumer, final-gate
 * and cleanup rules above apply. Both layout headers must match ABI v1. The
 * entire outer params and request span participate in storage admission before
 * any wipe or result mutation. Opaque callback contexts remain caller-owned
 * and valid; their known first byte must not alias writable body/result spans.
 * Independent valid raw response storage is fully erased on every return.
 * GET never admits this endpoint; POST never admits a v1 metadata URL. */
int mb_curl_provider_post_transfer_v1(const mb_curl_provider_post_transfer_params_v1 *,
                                     mb_curl_provider_transfer_result_v1 *);

#ifdef __cplusplus
}
#endif
#endif
