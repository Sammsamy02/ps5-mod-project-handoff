#ifndef MODBRIDGE_NEXUS_AUTH_H
#define MODBRIDGE_NEXUS_AUTH_H
#include <stddef.h>
#include <stdint.h>
#include "nexus_download.h"

#define MB_NEXUS_AUTH_ABI_V1 1u
#define MB_NEXUS_AUTH_SESSION_LIMIT 16u
#define MB_NEXUS_AUTH_SESSION_SECONDS 1800u
#define MB_NEXUS_AUTH_PENDING_SECONDS 300u
#define MB_NEXUS_AUTH_TOKEN_BYTES 43u
#define MB_NEXUS_AUTH_TOKEN_CAPACITY 44u
#define MB_NEXUS_AUTH_CLIENT_ID_LIMIT 128u
#define MB_NEXUS_AUTH_ORIGIN_LIMIT 128u
#define MB_NEXUS_AUTH_SCOPE_LIMIT 1024u
#define MB_NEXUS_AUTH_CA_LIMIT 1048576u
#define MB_NEXUS_AUTH_RESOLVE_LIMIT 8192u
#define MB_NEXUS_AUTH_CALLBACK_LIMIT 8192u
#define MB_NEXUS_AUTH_OUTPUT_LIMIT 8192u
#define MB_NEXUS_AUTH_CATALOG_OUTPUT_LIMIT 16777216u
#define MB_NEXUS_AUTH_ACQUISITION_INTENT_LIMIT 4096u
#define MB_NEXUS_AUTH_ACQUISITION_OUTPUT_LIMIT 1048576u
#define MB_NEXUS_AUTH_REGISTRATION_METADATA_LIMIT (128u * 1024u)
#define MB_NEXUS_AUTH_ACQUISITION_CAPTURE_LIMIT (2u * 1024u * 1024u)
#define MB_NEXUS_AUTH_TIMEOUT_LIMIT_MS 30000u

typedef struct mb_nexus_auth_context_v1 mb_nexus_auth_context_v1;
typedef int (*mb_nexus_auth_entropy_fn_v1)(void *, uint8_t output[32]);
typedef int (*mb_nexus_auth_cancel_fn_v1)(void *, uint64_t received);

typedef enum mb_nexus_auth_status_v1 {
    MB_NEXUS_AUTH_OK = 0, MB_NEXUS_AUTH_BAD_ARGUMENT = 1,
    MB_NEXUS_AUTH_NOT_CONFIGURED = 2, MB_NEXUS_AUTH_SESSION_REQUIRED = 3,
    MB_NEXUS_AUTH_REFUSED = 4, MB_NEXUS_AUTH_AUTHORIZATION_CANCELLED = 5,
    MB_NEXUS_AUTH_RECONNECT_REQUIRED = 6, MB_NEXUS_AUTH_BUSY = 7,
    MB_NEXUS_AUTH_TRANSPORT_FAILED = 8, MB_NEXUS_AUTH_PAYLOAD_FAILED = 9,
    MB_NEXUS_AUTH_OUTPUT_TOO_SMALL = 10, MB_NEXUS_AUTH_TIMEOUT = 11,
    MB_NEXUS_AUTH_CANCELLED = 12, MB_NEXUS_AUTH_SOURCE_FAILED = 13,
    MB_NEXUS_AUTH_INTERNAL_ERROR = 14, MB_NEXUS_AUTH_CONTENT_HIDDEN = 15
} mb_nexus_auth_status_v1;

typedef struct mb_nexus_auth_result_v1 {
    uint32_t struct_size, abi_version;
    mb_nexus_auth_status_v1 status;
    uint32_t complete, configured, new_session, credentials_present;
    size_t output_bytes;
} mb_nexus_auth_result_v1;

typedef struct mb_nexus_auth_create_params_v1 {
    uint32_t struct_size, abi_version;
    const char *origin; size_t origin_bytes;
    const char *client_id; size_t client_id_bytes;
    const char *redirect_uri; size_t redirect_uri_bytes;
    const char *scope; size_t scope_bytes;
    const uint8_t *ca_pem; size_t ca_bytes;
    const char *api_resolve; size_t api_resolve_bytes;
    const char *users_resolve; size_t users_resolve_bytes;
    void *entropy_context;
    mb_nexus_auth_entropy_fn_v1 entropy_read32;
} mb_nexus_auth_create_params_v1;

typedef struct mb_nexus_auth_session_params_v1 {
    uint32_t struct_size, abi_version;
    const char *cookie; size_t cookie_bytes;
    char *cookie_output; size_t cookie_capacity;
    char *csrf_output; size_t csrf_capacity;
} mb_nexus_auth_session_params_v1;

typedef struct mb_nexus_auth_start_params_v1 {
    uint32_t struct_size, abi_version;
    const char *cookie; size_t cookie_bytes;
    const char *csrf; size_t csrf_bytes;
    const char *origin; size_t origin_bytes;
    char *authorization_url; size_t authorization_capacity;
} mb_nexus_auth_start_params_v1;

typedef struct mb_nexus_auth_callback_params_v1 {
    uint32_t struct_size, abi_version;
    const char *cookie; size_t cookie_bytes;
    const char *target; size_t target_bytes;
    uint32_t timeout_ms, connect_timeout_ms;
    void *cancel_context;
    mb_nexus_auth_cancel_fn_v1 cancelled;
    char *summary; size_t summary_capacity;
} mb_nexus_auth_callback_params_v1;

typedef struct mb_nexus_auth_account_params_v1 {
    uint32_t struct_size, abi_version;
    const char *cookie; size_t cookie_bytes;
    uint32_t timeout_ms, connect_timeout_ms;
    void *cancel_context;
    mb_nexus_auth_cancel_fn_v1 cancelled;
    char *summary; size_t summary_capacity;
} mb_nexus_auth_account_params_v1;

typedef struct mb_nexus_auth_disconnect_params_v1 {
    uint32_t struct_size, abi_version;
    const char *cookie; size_t cookie_bytes;
    const char *csrf; size_t csrf_bytes;
    const char *origin; size_t origin_bytes;
} mb_nexus_auth_disconnect_params_v1;

typedef enum mb_nexus_auth_admission_v1 {
    MB_NEXUS_AUTH_ADMISSION_READ = 1,
    MB_NEXUS_AUTH_ADMISSION_MUTATION = 2
} mb_nexus_auth_admission_v1;

typedef struct mb_nexus_auth_admit_params_v1 {
    uint32_t struct_size, abi_version;
    const char *cookie; size_t cookie_bytes;
    const char *csrf; size_t csrf_bytes;
    const char *origin; size_t origin_bytes;
    mb_nexus_auth_admission_v1 admission;
    uint32_t require_account;
} mb_nexus_auth_admit_params_v1;

typedef enum mb_nexus_auth_catalog_kind_v1 {
    MB_NEXUS_AUTH_CATALOG_GAMES = 1,
    MB_NEXUS_AUTH_CATALOG_MODS = 2,
    MB_NEXUS_AUTH_CATALOG_MOD = 3,
    MB_NEXUS_AUTH_CATALOG_FILES = 4,
    MB_NEXUS_AUTH_CATALOG_REQUIREMENTS = 5
} mb_nexus_auth_catalog_kind_v1;

typedef struct mb_nexus_auth_catalog_params_v1 {
    uint32_t struct_size, abi_version;
    const char *cookie; size_t cookie_bytes;
    mb_nexus_auth_catalog_kind_v1 kind;
    const char *domain; size_t domain_bytes;
    uint64_t mod_id;
    uint32_t timeout_ms, connect_timeout_ms;
    void *cancel_context;
    mb_nexus_auth_cancel_fn_v1 cancelled;
    char *output; size_t output_capacity;
} mb_nexus_auth_catalog_params_v1;

typedef struct mb_nexus_auth_acquisition_plan_params_v1 {
    uint32_t struct_size, abi_version;
    const char *cookie; size_t cookie_bytes;
    const char *csrf; size_t csrf_bytes;
    const char *origin; size_t origin_bytes;
    const char *intent; size_t intent_bytes;
    uint32_t timeout_ms, connect_timeout_ms;
    void *cancel_context;
    mb_nexus_auth_cancel_fn_v1 cancelled;
    char *output; size_t output_capacity;
} mb_nexus_auth_acquisition_plan_params_v1;

typedef struct mb_nexus_auth_acquisition_capture_v1 {
    uint32_t struct_size, abi_version;
    char *output; size_t output_capacity;
    size_t output_bytes;
} mb_nexus_auth_acquisition_capture_v1;

typedef struct mb_nexus_auth_resolve_params_v1 {
    uint32_t struct_size, abi_version;
    const char *cookie; size_t cookie_bytes;
    const char *csrf; size_t csrf_bytes;
    const char *origin; size_t origin_bytes;
    /* Existing selected-file request with auth_mode=0, credential=NULL/0,
     * ca_pem=NULL/0 and both resolve pointers NULL. The context injects them.
     * timeout_ms is the whole wrapper budget and must be <=30000. */
    const mb_nexus_download_params_v1 *download;
} mb_nexus_auth_resolve_params_v1;

typedef struct mb_nexus_auth_resolve_result_v1 {
    uint32_t struct_size, abi_version;
    mb_nexus_auth_result_v1 auth;
    mb_nexus_download_result_v1 download;
} mb_nexus_auth_resolve_result_v1;

#ifdef __cplusplus
#define MB_NEXUS_AUTH_NOEXCEPT noexcept
extern "C" {
#else
#define MB_NEXUS_AUTH_NOEXCEPT
#endif

/* Process-lifetime, serialized ownership; all calls/hooks must not race. Only
 * handles returned by create may be used. No runtime start/close, PSA operation,
 * file persistence, refresh, password capture or credential getter exists.
 * Create owns configuration/trust/address copies; only the checked entropy
 * callback/context remain borrowed through free. Zero client_id_bytes selects
 * an unconfigured context (all other OAuth/trust/address spans must be empty).
 * Origin is still required. Configured redirect is exactly origin+/auth/callback.
 * Entropy success means exactly 32 fresh checked platform bytes, without retry.
 * State/cookie/CSRF are fresh 43-character base64url strings; verifier is private.
 *
 * Every specified input/output span, params/result and nested download/grant
 * storage must be valid, disjoint and without address overflow, including this
 * context's owned storage. Invalid layout/alias arguments leave storage intact.
 * Valid output spans are fully wiped on failure; output_bytes is zero. Output
 * capacities are 1..8192; cookie/CSRF need at least44 including NUL. Successful
 * outputs are NUL-terminated and lengths exclude NUL. Session outputs are each
 * 43 bytes; common output_bytes is used for URL/summary/URI, not that pair.
 * No zero-size query/refetch.
 *
 * Start/disconnect/resolve require current cookie, matching CSRF and exact Origin.
 * Callback requires exact /auth/callback? target, current pending cookie/state,
 * one code (1..2048 ASCII33..126) or access_denied, and no extra/duplicate fields.
 * It consumes a valid transaction before token POST and verifies live userinfo
 * before atomic replacement. Every failed replacement preserves old credentials.
 * Account GET always verifies live userinfo under stored unexpired bearer; a
 * stored token alone never gives a connected/verified summary. Expired tokens
 * require reconnect. Summary contains public identity facts and constant status
 * only, never tokens/code/verifier/query/body. Scratch/form/candidate cleanup and
 * final real monotonic/token expiry gates precede replacement/output publication.
 * The last sample is a publication gate, not a hard interrupt during the
 * bounded non-failing ownership swap, old-token erasure and scalar publication.
 * Cancel exceptions fail closed; provider total/connect budgets are 1..30000ms.
 * Root HTTP owns Host/frame guards and cookie attributes HttpOnly;SameSite=Lax;
 * Path=/. This layer checks observed token/Origin bytes, never a caller auth flag.
 * Free(NULL-handle) is idempotent; free rejects busy calls and otherwise wipes
 * active/pending/session secrets and sets the caller's handle to NULL.
 *
 * Admission is a serialized local gate, not a provider verification. READ
 * requires only a current session cookie and empty CSRF/Origin spans. MUTATION
 * additionally requires the matching CSRF token and exact configured Origin.
 * require_account is 0/1; when 1 the configured context must hold an unexpired
 * OAuth access token. No account status is inferred from possession alone.
 * Catalog reads accept only one of the five named kind values, a current
 * session cookie, configured context and active unexpired OAuth access token.
 * Games, Mods, Mod and Files use their fixed typed metadata routes;
 * Requirements uses the fixed read-only GraphQL query and never accepts a
 * caller URL.
 * Mod and files require a safe domain and positive signed-63-bit mod ID; games
 * requires empty domain/zero ID; mods requires safe domain/zero ID. The wrapper
 * calls the existing fixed-route typed client and returns only a complete
 * owning {"items":[...]} or {"item":{...}} projection. It never accepts a
 * credential, API key, CA, URL or trust/address override from its caller. Hidden
 * or unpublished mods are omitted from lists and rejected for direct mod/files
 * reads. Adult content is visible only after a fresh OAuth userinfo preference
 * check; unknown preference or malformed visibility fields fail closed. The
 * requirements kind first verifies the source mod is visible, then performs
 * the fixed read-only requirements query under the same session, token and
 * deadline gates. It returns a normalized owning {"requirements":...} object;
 * every declared relation remains unresolved and grants no file, version,
 * prerequisite-satisfaction or installation authority. Its positive source
 * mod ID is limited to INT32_MAX. The whole output capacity is wiped on
 * failure; capacity is 1..16 MiB, no size
 * query/refetch. Raw provider body and decoder report remain private and wiped.
 * The final cookie/session, cancellation, monotonic-time and token-expiry gates
 * precede publication.
 *
 * Acquisition planning is an explicit bounded read operation using the same
 * session, CSRF, Origin and unexpired OAuth gates. The intent is an exact JSON
 * byte span of 1..MB_NEXUS_AUTH_ACQUISITION_INTENT_LIMIT bytes with fields
 * {game,mod_ids,file_choices,file_policy}; no credential, trust, URL or path
 * is accepted. One operation/deadline covers all private metadata and
 * requirements reads. The output is an owning {"plan":...} projection capped
 * by MB_NEXUS_AUTH_ACQUISITION_OUTPUT_LIMIT. It publishes no ready prefix on
 * provider, deadline, cancellation, malformed response or cleanup failure.
 * Relation satisfaction, compatibility, download authorization and
 * installation remain false/unresolved; selected_sources is empty unless the
 * entire bounded same-game closure and all file choices are resolved. The
 * output span is independent and fully wiped on failure.
 */
int mb_nexus_auth_create_v1(const mb_nexus_auth_create_params_v1 *,
    mb_nexus_auth_context_v1 **, mb_nexus_auth_result_v1 *) MB_NEXUS_AUTH_NOEXCEPT;
int mb_nexus_auth_free_v1(mb_nexus_auth_context_v1 **) MB_NEXUS_AUTH_NOEXCEPT;
int mb_nexus_auth_session_v1(mb_nexus_auth_context_v1 *,
    const mb_nexus_auth_session_params_v1 *, mb_nexus_auth_result_v1 *) MB_NEXUS_AUTH_NOEXCEPT;
int mb_nexus_auth_start_v1(mb_nexus_auth_context_v1 *,
    const mb_nexus_auth_start_params_v1 *, mb_nexus_auth_result_v1 *) MB_NEXUS_AUTH_NOEXCEPT;
int mb_nexus_auth_callback_v1(mb_nexus_auth_context_v1 *,
    const mb_nexus_auth_callback_params_v1 *, mb_nexus_auth_result_v1 *) MB_NEXUS_AUTH_NOEXCEPT;
int mb_nexus_auth_account_v1(mb_nexus_auth_context_v1 *,
    const mb_nexus_auth_account_params_v1 *, mb_nexus_auth_result_v1 *) MB_NEXUS_AUTH_NOEXCEPT;
int mb_nexus_auth_disconnect_v1(mb_nexus_auth_context_v1 *,
    const mb_nexus_auth_disconnect_params_v1 *, mb_nexus_auth_result_v1 *) MB_NEXUS_AUTH_NOEXCEPT;
int mb_nexus_auth_admit_v1(mb_nexus_auth_context_v1 *,
    const mb_nexus_auth_admit_params_v1 *, mb_nexus_auth_result_v1 *) MB_NEXUS_AUTH_NOEXCEPT;
int mb_nexus_auth_catalog_v1(mb_nexus_auth_context_v1 *,
    const mb_nexus_auth_catalog_params_v1 *, mb_nexus_auth_result_v1 *) MB_NEXUS_AUTH_NOEXCEPT;
int mb_nexus_auth_acquisition_plan_v1(mb_nexus_auth_context_v1 *,
    const mb_nexus_auth_acquisition_plan_params_v1 *, mb_nexus_auth_result_v1 *) MB_NEXUS_AUTH_NOEXCEPT;
/* Trusted native capture of already-discovered registration metadata. The
 * public plan output and discovery requests are identical to plan_v1. Capture
 * is independent caller-owned NUL-terminated JSON, capacity 1..2MiB:
 * {items:[{game,mod_id,file_id,metadata:{source:{name?,author?,version?},
 * requirements:<full normalized object>}}]}. Ready items follow the exact
 * selected_sources order; a non-ready plan has no captured items. Name/author
 * come from the visible mod; version is the selected file's version. Missing
 * optional display fields are omitted. A snapshot exceeding 128KiB is null
 * metadata (unknown), never a truncated complete requirements object.
 * All capture/plan/result/input spans must be disjoint. After valid layout,
 * failure wipes both whole output buffers and sets capture.output_bytes=0.
 * Both outputs publish only after final auth/session/deadline/cancel gates.
 * No cache, second fetch, capture callback, file path, or browser-supplied metadata. */
int mb_nexus_auth_acquisition_plan_capture_v1(mb_nexus_auth_context_v1 *,
    const mb_nexus_auth_acquisition_plan_params_v1 *, mb_nexus_auth_acquisition_capture_v1 *,
    mb_nexus_auth_result_v1 *) MB_NEXUS_AUTH_NOEXCEPT;
int mb_nexus_auth_resolve_download_v1(mb_nexus_auth_context_v1 *,
    const mb_nexus_auth_resolve_params_v1 *, mb_nexus_auth_resolve_result_v1 *) MB_NEXUS_AUTH_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef MB_NEXUS_AUTH_NOEXCEPT
#endif
