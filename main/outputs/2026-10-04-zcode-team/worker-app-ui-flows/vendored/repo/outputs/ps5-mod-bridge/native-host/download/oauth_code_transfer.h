#ifndef MB_CURL_OAUTH_CODE_TRANSFER_H
#define MB_CURL_OAUTH_CODE_TRANSFER_H
#include "provider_transfer.h"

#define MB_CURL_OAUTH_CODE_FORM_MAX 65536u
#define MB_CURL_OAUTH_CODE_BODY_MAX 131072u
#define MB_CURL_OAUTH_CODE_TIMEOUT_MAX_MS 30000u

typedef struct mb_curl_oauth_code_transfer_params_v1 {
 uint32_t struct_size,abi_version;
 const char *form; size_t form_bytes;
 const uint8_t *ca_pem; size_t ca_bytes;
 const char *resolve;
 uint32_t timeout_ms,connect_timeout_ms;
 void *cancel_context; mb_curl_cancel_fn_v1 cancelled;
 uint8_t *body; size_t body_capacity;
 void *consume_context;
 int (*consume_complete_body)(void *,const uint8_t *,size_t);
} mb_curl_oauth_code_transfer_params_v1;
typedef mb_curl_provider_transfer_result_v1 mb_curl_oauth_code_transfer_result_v1;

#ifdef __cplusplus
extern "C" {
#endif
/* Private fixed POST https://users.nexusmods.com/oauth/token. Only the canonical
 * authorization_code form is admitted. No URL/method/header/auth override,
 * API key, Authorization, cookies, redirects, proxy, refresh or automatic retry.
 * The immutable form is borrowed through synchronous easy-handle cleanup.
 * Exact-host resolve does not relax mandatory CA/name verification. Raw response
 * is bounded1..131072, complete HTTP200 only; consumer runs once inside the same
 * runtime busy/entropy window. Full independent raw capacity is wiped on every
 * supported return. Invalid/aliased storage is untouched. Late timeout/owner
 * failure revokes consumer success. Consumer must not throw, retain body bytes,
 * race hooks or publish tokens before final transport success. Curl's opaque
 * internals have their own lifetime; no secure-erasure claim is made for them. */
int mb_curl_oauth_code_transfer_v1(const mb_curl_oauth_code_transfer_params_v1 *,
                                 mb_curl_oauth_code_transfer_result_v1 *);
#ifdef __cplusplus
}
#endif
#endif
