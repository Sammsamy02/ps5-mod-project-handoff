#ifndef MODBRIDGE_ARCHIVE_PREPARED_H
#define MODBRIDGE_ARCHIVE_PREPARED_H
#include "groups.h"

#define MB_ARCHIVE_PREPARED_ABI_V1 1u
#define MB_ARCHIVE_PREPARED_NAME_BYTES 128u

enum mb_archive_prepared_status_v1 {
    MB_ARCHIVE_PREPARED_OK = 0, MB_ARCHIVE_PREPARED_BAD_ARGUMENT = 1,
    MB_ARCHIVE_PREPARED_INCOMPLETE = 2, MB_ARCHIVE_PREPARED_UNRESOLVED = 3,
    MB_ARCHIVE_PREPARED_LIMIT = 4, MB_ARCHIVE_PREPARED_IDENTITY_MISMATCH = 5
};
enum mb_archive_prepared_phase_v1 {
    MB_ARCHIVE_PREPARED_CANDIDATE = 1, MB_ARCHIVE_PREPARED_MATERIALIZED = 2
};
typedef struct mb_archive_prepared_view_v1 {
    const mb_archive_inventory_v1 *inventory;
    const mb_archive_row_v1 *rows; uint32_t row_capacity;
    const char *names; uint32_t name_capacity;
} mb_archive_prepared_view_v1;
/* Trusted caller context, not engine/platform/installation evidence. The
 * nonce is exactly 32 lowercase hex bytes plus NUL. Prefix is nonempty ASCII
 * alnum/_/-; suffix admits the same bytes or empty. Loading conventions belong
 * in this profile. Neither field is a path. No randomness is acquired here. */
typedef struct mb_archive_prepared_profile_v1 {
    uint32_t struct_size, abi_version;
    uint8_t target_token[32];
    char nonce[33], prefix[49], suffix[17];
} mb_archive_prepared_profile_v1;
typedef struct mb_archive_prepared_workspace_v1 {
    uint32_t *sort_indices; uint32_t sort_capacity;
    mb_archive_group_v1 *groups; uint32_t group_capacity;
    mb_archive_group_member_v1 *members; uint32_t member_capacity;
    char *keys; uint32_t key_capacity;
} mb_archive_prepared_workspace_v1;
typedef struct mb_archive_prepared_file_v1 {
    uint32_t group_index, row_index, role, reserved;
    uint64_t bytes, device, inode;
    uint8_t sha256[32];
    char source_name[40], destination_name[MB_ARCHIVE_PREPARED_NAME_BYTES];
} mb_archive_prepared_file_v1;
typedef struct mb_archive_prepared_result_v1 {
    uint32_t struct_size, abi_version, status, complete;
    uint32_t phase, group_count, file_count, reserved;
    uint64_t total_bytes, path_work_used;
    uint64_t source_size, source_device, source_inode;
    uint64_t quarantine_device, quarantine_inode;
    uint8_t source_sha256[32], inventory_sha256[32], target_token[32];
    char nonce[33], prefix[49], suffix[17], manifest_name[40];
    uint32_t installation_supported, runtime_supported;
    uint32_t platform_compatible, format_validated;
} mb_archive_prepared_result_v1;
typedef struct mb_archive_prepared_materialization_v1 {
    const mb_archive_materialized_v1 *result;
    const mb_archive_owned_file_v1 *journal; uint32_t journal_capacity;
    uint64_t quarantine_device, quarantine_inode;
} mb_archive_prepared_materialization_v1;

#ifdef __cplusplus
extern "C" {
#endif
/* Reuses the actual group classifier, selecting every complete candidate
 * group. Any unresolved group rejects the whole operation. Noncontainer rows
 * are not selected; no groups yields an owning complete empty candidate.
 * Selection/publication order per group is UCAS, optional PAK, then UTOC.
 * Generated names use profile prefix+nonce+'-'+8-digit group+suffix+extension;
 * bytes and logical archive names are unchanged. The selected array directly
 * feeds mb_archive_materialize_v1. All authority flags remain zero.
 *
 * Valid immutable backing is a caller obligation. All control/input/output
 * spans must be disjoint. Known pointer/ABI/cap/alias preflight errors are
 * no-write BAD_ARGUMENT. Admitted failure clears the entire output capacity
 * and workspace; only OK publishes complete facts. Workspace is also cleared
 * on success. Sort capacity must cover all inventory rows for the later
 * row-index ownership map. No allocation, I/O, RNG, game destinations or
 * retained pointers. */
int mb_archive_prepare_groups_v1(const mb_archive_prepared_view_v1 *view,
    const mb_archive_prepared_profile_v1 *profile,
    const mb_archive_prepared_workspace_v1 *workspace,
    uint32_t *selected, uint32_t selected_capacity,
    mb_archive_prepared_file_v1 *files, uint32_t file_capacity,
    mb_archive_prepared_result_v1 *result);

/* Re-derives the candidate from the same owning inventory/profile, then
 * binds exact successful materializer rows/control journal to owning files.
 * This is observation of the trusted materializer's result, not a fresh disk
 * verification or an installation grant. Caller retains its recovery journal
 * unchanged on every return and revalidates files before any later writes.
 * Candidate, materializer and outputs must have separate backing. No groups
 * is not materializable through this binding operation. */
int mb_archive_bind_prepared_v1(const mb_archive_prepared_view_v1 *view,
    const mb_archive_prepared_profile_v1 *profile,
    const mb_archive_prepared_workspace_v1 *workspace,
    const mb_archive_prepared_result_v1 *candidate,
    const mb_archive_prepared_file_v1 *candidate_files, uint32_t candidate_capacity,
    const mb_archive_prepared_materialization_v1 *materialization,
    mb_archive_prepared_file_v1 *files, uint32_t file_capacity,
    mb_archive_prepared_result_v1 *result);
#ifdef __cplusplus
}
#endif
#endif
