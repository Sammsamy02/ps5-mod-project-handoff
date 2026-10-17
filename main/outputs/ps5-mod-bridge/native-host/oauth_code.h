#ifndef MODBRIDGE_OAUTH_CODE_H
#define MODBRIDGE_OAUTH_CODE_H
#include <stddef.h>
#include <stdint.h>

#define MB_NEXUS_OAUTH_CODE_ABI_V1 1u
#define MB_NEXUS_OAUTH_CLIENT_LIMIT_BYTES 128u
#define MB_NEXUS_OAUTH_REDIRECT_LIMIT_BYTES 4096u
#define MB_NEXUS_OAUTH_CODE_LIMIT_BYTES 2048u
#define MB_NEXUS_OAUTH_VERIFIER_LIMIT_BYTES 128u
#define MB_NEXUS_OAUTH_FORM_LIMIT_BYTES 65536u
#define MB_NEXUS_OAUTH_RAW_LIMIT_BYTES 131072u
#define MB_NEXUS_OAUTH_TOKEN_LIMIT_BYTES 16384u
#define MB_NEXUS_OAUTH_CA_LIMIT_BYTES 1048576u
#define MB_NEXUS_OAUTH_TIMEOUT_LIMIT_MS 30000u

typedef enum mb_nexus_oauth_code_status_v1 {
 MB_NEXUS_OAUTH_CODE_OK=0, MB_NEXUS_OAUTH_CODE_BAD_ARGUMENT,
 MB_NEXUS_OAUTH_CODE_BAD_STATE, MB_NEXUS_OAUTH_CODE_TRANSPORT_FAILED,
 MB_NEXUS_OAUTH_CODE_TOKEN_REJECTED, MB_NEXUS_OAUTH_CODE_CANCELLED,
 MB_NEXUS_OAUTH_CODE_TIMEOUT, MB_NEXUS_OAUTH_CODE_INTERNAL_ERROR
} mb_nexus_oauth_code_status_v1;

typedef struct mb_nexus_oauth_code_fields_v1 {
 const char *client_id; size_t client_id_bytes;
 const char *redirect_uri; size_t redirect_uri_bytes;
 const char *code; size_t code_bytes;
 const char *code_verifier; size_t code_verifier_bytes;
} mb_nexus_oauth_code_fields_v1;

/* Private caller-owned credentials. Do not serialize this block, expose it to
 * browser code or infer account identity from it. Entire block is zero on any
 * supported exchange failure. Expiry is conservatively anchored at call start. */
typedef struct mb_nexus_oauth_tokens_v1 {
 uint32_t access_token_bytes,refresh_token_bytes,expires_in_seconds,reserved;
 uint64_t expires_at_monotonic_ns;
 char access_token[MB_NEXUS_OAUTH_TOKEN_LIMIT_BYTES+1u];
 char refresh_token[MB_NEXUS_OAUTH_TOKEN_LIMIT_BYTES+1u];
} mb_nexus_oauth_tokens_v1;
typedef int (*mb_nexus_oauth_cancel_fn_v1)(void *,uint64_t received);
typedef struct mb_nexus_oauth_exchange_params_v1 {
 uint32_t struct_size,abi_version;
 mb_nexus_oauth_code_fields_v1 fields;
 const uint8_t *ca_pem; size_t ca_bytes;
 const char *users_resolve;
 uint32_t timeout_ms,connect_timeout_ms;
 void *cancel_context;
 mb_nexus_oauth_cancel_fn_v1 cancelled;
 char *form; size_t form_capacity;
 uint8_t *raw_body; size_t raw_capacity;
 mb_nexus_oauth_tokens_v1 *tokens;
} mb_nexus_oauth_exchange_params_v1;
typedef struct mb_nexus_oauth_exchange_result_v1 {
 uint32_t struct_size,abi_version;
 mb_nexus_oauth_code_status_v1 status;
 uint32_t complete,transport_status,http_status;
 int32_t curl_code;
 uint64_t received_bytes;
} mb_nexus_oauth_exchange_result_v1;

#ifdef __cplusplus
#define MB_NEXUS_OAUTH_NOEXCEPT noexcept
extern "C" {
#else
#define MB_NEXUS_OAUTH_NOEXCEPT
#endif

/* Pure bounded writer: fixed authorization_code grant, redirect_uri, client_id,
 * code, code_verifier order. Unreserved bytes remain raw; all others are %HH
 * with uppercase hex. Client grammar is [A-Za-z0-9_-]{1,128}, excluding
 * public_test; code is 1..2048 printable ASCII33..126; verifier is RFC7636
 * unreserved43..128. Redirect is an HTTP/HTTPS origin plus /auth/callback,
 * without userinfo/query/fragment/backslash. The auth context must also bind it
 * to its exact configured origin and consume matching state exactly once.
 * Success writes NUL and written excludes it. Short/invalid independent output
 * is wiped in full with written=0 and BAD_ARGUMENT; no prefix or size refetch.
 * All specified spans, fields and written must be valid/disjoint before mutation.
 * Unsupported or aliased storage is untouched. Borrowed inputs are immutable. */
int mb_nexus_oauth_write_code_form_v1(const mb_nexus_oauth_code_fields_v1 *,
    char *output,size_t capacity,size_t *written) MB_NEXUS_OAUTH_NOEXCEPT;

/* One fixed POST through an already-started, exclusively serialized curl/PSA
 * owner. No URL/header/auth override, client secret, retry, refresh or runtime
 * lifecycle action. CA/name validation remains mandatory. Total budget1..30000
 * includes temporary cleanup; connect budget is nonzero and at most total.
 * form/raw capacities are nonzero and within their independent limits. Token
 * block, scratch, immutable inputs, params/result are valid and disjoint;
 * callback context must not alias writable storage. Invalid layouts are untouched.
 * Full form/raw storage is wiped on supported returns; the whole token block is
 * wiped on failures, including late transport/clock/cancel/cleanup faults.
 * OK/complete=1 delivers a whole private token pair: both printable1..16384,
 * case-insensitive Bearer, integer expiry1..31536000, unexpired on return.
 * This establishes no verified account, visibility, download or install authority.
 * The caller must validate live userinfo before publishing an account.
 * Return equals result.status; underlying transport_status is a curl-runtime
 * status. No body/code/verifier/token or provider error text is returned/logged. */
int mb_nexus_oauth_exchange_code_v1(const mb_nexus_oauth_exchange_params_v1 *,
    mb_nexus_oauth_exchange_result_v1 *) MB_NEXUS_OAUTH_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#undef MB_NEXUS_OAUTH_NOEXCEPT
#endif
