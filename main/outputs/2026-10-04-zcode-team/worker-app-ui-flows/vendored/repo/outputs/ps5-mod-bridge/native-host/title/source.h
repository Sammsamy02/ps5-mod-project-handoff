#ifndef MODBRIDGE_TITLE_SOURCE_H
#define MODBRIDGE_TITLE_SOURCE_H

#include "metadata.h"

#define MB_TITLE_SOURCE_ABI_VERSION 1u
#define MB_TITLE_SOURCE_WORKSPACE_BYTES 65536u
#define MB_TITLE_SOURCE_READ_REQUEST_LIMIT 4096u
#define MB_TITLE_SOURCE_OPERATION_LIMIT 128u
#define MB_TITLE_SOURCE_REQUESTED_BYTE_LIMIT 65537u
#define MB_TITLE_SOURCE_METADATA_CACHE_SCOPE 1u
#define MB_TITLE_SOURCE_HOST_FIXTURE_SCOPE 2u

typedef enum mb_title_source_profile {
    MB_TITLE_CACHE_SYSTEM_PRIVATE=1, MB_TITLE_CACHE_USER=2
} mb_title_source_profile;
typedef enum mb_title_source_status {
    MB_TITLE_SOURCE_OK=0, MB_TITLE_SOURCE_BAD_ARGUMENT,
    MB_TITLE_SOURCE_UNAVAILABLE, MB_TITLE_SOURCE_UNSAFE_TYPE,
    MB_TITLE_SOURCE_RESOURCE_LIMIT, MB_TITLE_SOURCE_SHORT_READ,
    MB_TITLE_SOURCE_CHANGED, MB_TITLE_SOURCE_METADATA_REJECTED,
    MB_TITLE_SOURCE_ID_MISMATCH, MB_TITLE_SOURCE_CLEANUP_FAILED,
    MB_TITLE_SOURCE_INTERNAL_ERROR
} mb_title_source_status;
typedef enum mb_title_source_reason {
    MB_TITLE_SOURCE_REASON_NONE=0, MB_TITLE_SOURCE_OPERATION_BUDGET,
    MB_TITLE_SOURCE_READ_BUDGET, MB_TITLE_SOURCE_OPEN_FAILED,
    MB_TITLE_SOURCE_STAT_FAILED, MB_TITLE_SOURCE_SYMLINK,
    MB_TITLE_SOURCE_NOT_DIRECTORY, MB_TITLE_SOURCE_NOT_REGULAR,
    MB_TITLE_SOURCE_FILE_TOO_LARGE, MB_TITLE_SOURCE_READ_FAILED,
    MB_TITLE_SOURCE_INCOMPLETE_READ, MB_TITLE_SOURCE_IDENTITY_CHANGED,
    MB_TITLE_SOURCE_GROWTH_OBSERVED, MB_TITLE_SOURCE_DECODE_FAILED,
    MB_TITLE_SOURCE_CANDIDATE_MISMATCH, MB_TITLE_SOURCE_CLOSE_FAILED,
    MB_TITLE_SOURCE_INTERNAL_FAILURE
} mb_title_source_reason;
typedef struct mb_title_source_request_v1 {
    uint32_t struct_size, abi_version, profile, candidate_bytes;
    const char *candidate;
    uint32_t reserved[2];
} mb_title_source_request_v1;
typedef struct mb_title_source_budget_v1 {
    uint32_t struct_size, abi_version;
    uint64_t remaining_operations, reserved_operations;
    uint64_t remaining_read_bytes, reserved_read_bytes;
    uint32_t reserved[2];
} mb_title_source_budget_v1;
typedef struct mb_title_source_result_v1 {
    uint32_t struct_size, abi_version, status, reason;
    uint32_t requested_profile, source_scope, source_complete, identity_checked;
    uint32_t candidate_matches, cleanup_complete, operations_attempted, last_errno;
    uint64_t operations_reserved, requested_read_bytes, actual_read_bytes;
    uint32_t metadata_status, metadata_reason;
    mb_title_metadata_result_v1 metadata;
} mb_title_source_result_v1;

#ifdef __cplusplus
#define MB_TITLE_SOURCE_NOEXCEPT noexcept
extern "C" {
#else
#define MB_TITLE_SOURCE_NOEXCEPT
#endif

/* Capture exactly <fixed enum root>/<generic candidate>/param.json. Roots are
 * /system_data/priv/appmeta and /user/appmeta, never caller/browser paths.
 * No enumeration, symlink traversal, permission adjustment or source write.
 * Full regular-file input <=64KiB, requests <=4096, exact-read policy + EOF
 * probe. Root/component/file identity is observed before/after; this does not
 * lock or atomically freeze a hostile filesystem. No I/O wall-time guarantee.
 *
 * Caller owns exclusive valid disjoint request/candidate/budget/workspace/out
 * backing. Workspace capacity must equal 64KiB. Exact ABI/reserved/candidate
 * validation and known aliases are no-write preflight. After preflight, full
 * workspace is cleared on every return; only whole OK publishes metadata.
 * One caller-owned unreplenished finite ledger covers every attempted OS
 * operation and requested read, including failures. Its global remaining and
 * reserved totals may exceed a single call's caps; checked uint64 sums cannot
 * overflow. Each call separately caps reservations at 128 operation tokens
 * and 65537 requested read bytes. Before each open, reserve both open
 * and one finite close token. Failed opens do not refund unused close tokens;
 * diagnostics distinguish reserved tokens from actual operation attempts.
 * Cleanup uses those reserved close tokens even after work budget exhaustion.
 * No close retries; a close error cannot publish a successful capture.
 * Metadata digest/version remain descriptive, with all decoder authority flags
 * zero. Native provider remains unavailable; this API enables no app provider.
 */
mb_title_source_status mb_title_source_capture_v1(
    const mb_title_source_request_v1 *request,mb_title_source_budget_v1 *budget,
    uint8_t *workspace,size_t workspace_bytes,
    mb_title_source_result_v1 *result) MB_TITLE_SOURCE_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#undef MB_TITLE_SOURCE_NOEXCEPT
#endif
