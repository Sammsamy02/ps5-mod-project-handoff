#ifndef MODBRIDGE_TITLE_METADATA_H
#define MODBRIDGE_TITLE_METADATA_H

#include <stddef.h>
#include <stdint.h>

#define MB_TITLE_METADATA_ABI_VERSION 1u
#define MB_TITLE_METADATA_INPUT_LIMIT 65536u
#define MB_TITLE_METADATA_DEPTH_LIMIT 16u
#define MB_TITLE_METADATA_NODE_LIMIT 4096u
#define MB_TITLE_METADATA_STRING_LIMIT 4096u
#define MB_TITLE_PARAM_JSON_ROOT_V1 1u
#define MB_TITLE_SOURCE_METADATA_CAPTURE 1u
#define MB_TITLE_IDENTITY_CAPTURED_BYTES_SHA256 1u

typedef enum mb_title_metadata_status {
    MB_TITLE_METADATA_OK=0, MB_TITLE_METADATA_BAD_ARGUMENT,
    MB_TITLE_METADATA_MALFORMED, MB_TITLE_METADATA_SCHEMA_ERROR,
    MB_TITLE_METADATA_RESOURCE_LIMIT, MB_TITLE_METADATA_ALLOCATION_FAILED,
    MB_TITLE_METADATA_INTERNAL_ERROR
} mb_title_metadata_status;
typedef enum mb_title_metadata_reason {
    MB_TITLE_METADATA_REASON_NONE=0, MB_TITLE_METADATA_EMPTY_INPUT,
    MB_TITLE_METADATA_INPUT_TOO_LARGE, MB_TITLE_METADATA_INVALID_UTF8,
    MB_TITLE_METADATA_INVALID_JSON, MB_TITLE_METADATA_DUPLICATE_KEY,
    MB_TITLE_METADATA_DEPTH_EXCEEDED, MB_TITLE_METADATA_NODES_EXCEEDED,
    MB_TITLE_METADATA_STRING_EXCEEDED, MB_TITLE_METADATA_ROOT_NOT_OBJECT,
    MB_TITLE_METADATA_TITLE_ID_MISSING, MB_TITLE_METADATA_TITLE_ID_TYPE,
    MB_TITLE_METADATA_TITLE_ID_INVALID, MB_TITLE_METADATA_CONTENT_VERSION_TYPE,
    MB_TITLE_METADATA_CONTENT_VERSION_INVALID, MB_TITLE_METADATA_LOCALIZED_TYPE,
    MB_TITLE_METADATA_DEFAULT_LOCALE_TYPE, MB_TITLE_METADATA_DEFAULT_LOCALE_INVALID,
    MB_TITLE_METADATA_SELECTED_LOCALE_TYPE, MB_TITLE_METADATA_TITLE_NAME_TYPE,
    MB_TITLE_METADATA_TITLE_NAME_INVALID, MB_TITLE_METADATA_ALLOCATION_FAILURE,
    MB_TITLE_METADATA_INTERNAL_FAILURE
} mb_title_metadata_reason;

typedef struct mb_title_metadata_input_v1 {
    uint32_t struct_size, abi_version;
    const uint8_t *bytes;
    uint64_t available;
    uint32_t profile, reserved;
} mb_title_metadata_input_v1;

typedef struct mb_title_metadata_result_v1 {
    uint32_t struct_size, abi_version;
    uint32_t status, reason;
    uint64_t captured_bytes;
    uint32_t source_scope, metadata_identity_scope;
    uint32_t metadata_complete, title_id_available, name_available;
    uint32_t version_available, locale_available, metadata_sha256_available;
    char title_id[65], name[257], version[129], locale[65], metadata_sha256[65];
    uint32_t installed_observed, running_observed, build_fingerprint_available;
    uint32_t engine_acceptance_verified, runtime_supported, installation_supported;
    uint32_t authenticity_verified, game_acceptance_verified, reserved[2];
} mb_title_metadata_result_v1;

#ifdef __cplusplus
#define MB_TITLE_METADATA_NOEXCEPT noexcept
extern "C" {
#else
#define MB_TITLE_METADATA_NOEXCEPT
#endif

/* Pure root-field param.json decoder; complete input is caller-owned immutable
 * backing. No I/O, retained pointer or title-prefix policy. Only explicitly
 * declared defaultLanguage selects a locale; absent fields stay unavailable.
 * Present optional strings preserve exact UTF-8, including empty strings;
 * embedded NUL is rejected because owned C fields are NUL terminated.
 *
 * Exact input ABI/profile/reserved and known disjoint spans are preflight;
 * invalid preflight writes nothing. Output needs valid full typed backing, not
 * prior initialization. Every operational failure replaces the whole output
 * with zero owning fields/availability/authority and constant status/reason.
 * C++ parsing allocates within fixed input bounds and contains exceptions.
 * Metadata SHA256 identifies full captured bytes; it is not a build fingerprint
 * or authenticity/installed/running/engine/runtime/install proof.
 */
mb_title_metadata_status mb_title_metadata_decode_v1(
    const mb_title_metadata_input_v1 *input,
    mb_title_metadata_result_v1 *result) MB_TITLE_METADATA_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#undef MB_TITLE_METADATA_NOEXCEPT
#endif
