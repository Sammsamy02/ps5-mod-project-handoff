#ifndef MODBRIDGE_TITLE_CATALOG_H
#define MODBRIDGE_TITLE_CATALOG_H

#include "source.h"

#define MB_TITLE_CATALOG_ABI_VERSION 1u
#define MB_TITLE_CATALOG_MAX_ENTRIES 256u
#define MB_TITLE_CATALOG_MAX_CANDIDATES 128u
#define MB_TITLE_CATALOG_MAX_RECORDS 128u
#define MB_TITLE_CATALOG_MAX_ISSUES 32u
#define MB_TITLE_CATALOG_MAX_CANDIDATE_BYTES 64u
#define MB_TITLE_CATALOG_ENTRY_NAME_BYTES 256u
#define MB_TITLE_CATALOG_CANDIDATE_BYTES 65u
#define MB_TITLE_CATALOG_SOURCE_WORKSPACE_BYTES MB_TITLE_SOURCE_WORKSPACE_BYTES
#define MB_TITLE_CATALOG_OPERATION_LIMIT 8192u
#define MB_TITLE_CATALOG_CHARGED_BYTE_LIMIT 8405120u
#define MB_TITLE_CATALOG_SOURCE_SCOPE_SYSTEM_PRIVATE_METADATA_CACHE 1u
#define MB_TITLE_CATALOG_SOURCE_SCOPE_USER_METADATA_CACHE 2u
#define MB_TITLE_CATALOG_ASSERT_IMMUTABLE_SCAN 1u

typedef enum mb_title_catalog_status {
    MB_TITLE_CATALOG_STATUS_OBSERVED=0,
    MB_TITLE_CATALOG_STATUS_PARTIAL,
    MB_TITLE_CATALOG_STATUS_UNAVAILABLE,
    MB_TITLE_CATALOG_STATUS_ERROR,
    /* Call return only; never stored in result.status. */
    MB_TITLE_CATALOG_STATUS_BAD_ARGUMENT
} mb_title_catalog_status;

typedef enum mb_title_catalog_reason {
    MB_TITLE_CATALOG_REASON_NONE=0,
    MB_TITLE_CATALOG_REASON_PLATFORM_PROVIDER_UNAVAILABLE,
    MB_TITLE_CATALOG_REASON_PROVIDER_ERROR,
    MB_TITLE_CATALOG_REASON_INVALID_PROVIDER_CAPTURE,
    MB_TITLE_CATALOG_REASON_INVENTORY_RESOURCE_LIMIT,
    MB_TITLE_CATALOG_REASON_SOURCE_CHANGED,
    MB_TITLE_CATALOG_REASON_PROVIDER_OBSERVATION_INCOMPLETE
} mb_title_catalog_reason;

typedef enum mb_title_catalog_issue {
    MB_TITLE_CATALOG_ISSUE_PLATFORM_PROVIDER_UNAVAILABLE=1,
    MB_TITLE_CATALOG_ISSUE_PROVIDER_ERROR,
    MB_TITLE_CATALOG_ISSUE_INVALID_PROVIDER_CAPTURE,
    MB_TITLE_CATALOG_ISSUE_INVENTORY_RESOURCE_LIMIT,
    MB_TITLE_CATALOG_ISSUE_SOURCE_CHANGED,
    MB_TITLE_CATALOG_ISSUE_PROVIDER_OBSERVATION_INCOMPLETE
} mb_title_catalog_issue;

typedef struct mb_title_catalog_request_v1 {
    uint32_t struct_size, abi_version, profile, scan_assertion;
    uint32_t reserved[2];
} mb_title_catalog_request_v1;

typedef struct mb_title_catalog_ledger_v1 {
    uint32_t struct_size, abi_version;
    mb_title_source_budget_v1 source_io;
    uint32_t entries_seen, candidates_seen;
    uint64_t entry_name_bytes;
    uint32_t reserved[2];
} mb_title_catalog_ledger_v1;

typedef struct mb_title_catalog_result_v1 {
    uint32_t struct_size, abi_version, status, reason;
    uint32_t profile, source_scope, complete, record_count;
    uint32_t issue_count, entries_seen, candidates_seen, root_identity_checked;
    uint32_t enumeration_eof, cleanup_complete, reserved[2];
    uint64_t operations_attempted, operations_reserved;
    uint64_t requested_read_bytes, actual_read_bytes, entry_name_bytes;
} mb_title_catalog_result_v1;

#ifdef __cplusplus
#define MB_TITLE_CATALOG_NOEXCEPT noexcept
extern "C" {
#else
#define MB_TITLE_CATALOG_NOEXCEPT
#endif

/* Read-only one-level scan of exactly one fixed appmeta cache profile. The
 * caller assertion is required but is not evidence of stability: observed /
 * complete is published only after clean EOF, root identity rechecks, every
 * candidate's source identity checks, and successful bounded capture. The
 * source_scope is profile-specific metadata cache scope, never installed,
 * running, active, or complete game inventory. No path,
 * directory handle or borrowed string is returned or retained.
 *
 * Every examined d_name byte is charged before copying it into the bounded
 * 256-byte scratch. Only a validated <=64-byte candidate reaches the child
 * source call. Enumeration and all child captures charge the exact same
 * caller-owned ledger; caps are never replenished between candidates. Record
 * and issue capacities, both scratch capacities, and source workspace capacity
 * must equal their constants. The caller initializes source_io with exactly
 * the global operation/read ceilings and zero catalog counters. Examined name
 * bytes debit remaining_read_bytes, while source captures reserve requested
 * read bytes there, sharing the charged-byte ceiling without labeling names
 * as file reads. Known backing overlaps or invalid preflight are
 * no-write. The caller must discard any old result and record/issue backing
 * whenever the return is not a fresh successful publication. After preflight,
 * any operational error clears all record/issue
 * outputs and publishes no complete prefix. Candidate-level uncertainty may
 * return partial metadata with complete=0; resource, internal, or duplicate-ID
 * failure clears the whole result. A clean empty scope is observed/complete.
 *
 * Each owning record is the exact decoded metadata result. It must have exact
 * ABI/status, candidate-bound title ID, and zero installed/running/build,
 * engine/runtime/install/authenticity/game authority flags. Metadata SHA256 is
 * descriptive only. This API does not enumerate installed/running games or
 * establish build compatibility; the platform provider remains disabled.
 */
mb_title_catalog_status mb_title_catalog_scan_v1(
    const mb_title_catalog_request_v1 *request,
    mb_title_catalog_ledger_v1 *ledger,
    char *entry_name_scratch,size_t entry_name_scratch_bytes,
    char *candidate_scratch,size_t candidate_scratch_bytes,
    uint8_t *source_workspace,size_t source_workspace_bytes,
    mb_title_metadata_result_v1 *records,uint32_t record_capacity,
    uint32_t *issue_codes,uint32_t issue_capacity,
    mb_title_catalog_result_v1 *result) MB_TITLE_CATALOG_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#undef MB_TITLE_CATALOG_NOEXCEPT
#endif
