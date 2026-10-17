#ifndef MB_PAK_ARCHIVE_SELECTION_H
#define MB_PAK_ARCHIVE_SELECTION_H

#include "prepared.h"

#define MB_PAK_ARCHIVE_SELECTION_ABI_V1 1u

typedef enum mb_pak_archive_status_v1 {
    MB_PAK_ARCHIVE_OK = 0,
    MB_PAK_ARCHIVE_BAD_ARGUMENT,
    MB_PAK_ARCHIVE_INCOMPLETE,
    MB_PAK_ARCHIVE_INVALID_INVENTORY,
    MB_PAK_ARCHIVE_LIMIT,
    MB_PAK_ARCHIVE_GROUP_REJECTED,
    MB_PAK_ARCHIVE_UNRESOLVED,
    MB_PAK_ARCHIVE_NO_CANDIDATE,
    MB_PAK_ARCHIVE_SELECTION_REQUIRED,
    MB_PAK_ARCHIVE_ADDITIONAL_ACTION_REQUIRED
} mb_pak_archive_status_v1;

typedef enum mb_pak_archive_policy_v1 {
    MB_PAK_ARCHIVE_POLICY_DEFAULT = 0,
    MB_PAK_ARCHIVE_POLICY_PAK_COMPONENT_ONLY = 1
} mb_pak_archive_policy_v1;

typedef enum mb_pak_archive_role_v1 {
    MB_PAK_ARCHIVE_ROLE_PAK = 1,
    MB_PAK_ARCHIVE_ROLE_RETAINED_EXTRA = 2
} mb_pak_archive_role_v1;

typedef struct mb_pak_archive_member_v1 {
    uint32_t row_index, role, retained_unhandled, reserved;
    uint64_t bytes;
    uint8_t sha256[32];
} mb_pak_archive_member_v1;

typedef struct mb_pak_archive_selection_v1 {
    uint32_t struct_size, abi_version, status, policy;
    uint32_t member_list_complete, selection_complete;
    uint32_t preparation_ready, whole_bundle_handled;
    uint32_t group_count, candidate_count, unresolved_group_count, member_count;
    uint32_t pak_row, retained_extra_count, problem_row, group_inner_status;
    uint64_t source_bytes, path_work_used;
    uint8_t source_sha256[32], inventory_sha256[32], target_token[32];
    uint32_t format_validated, platform_compatible;
    uint32_t installation_supported, runtime_supported;
} mb_pak_archive_selection_v1;

#ifdef __cplusplus
extern "C" {
#endif

/* Pure deterministic selection over a complete immutable intake inventory.
 * Reuses groups.c: exactly one resolved standalone PAK group is eligible.
 * Any unresolved container group prevents downgrade; multiple standalone PAKs
 * require selection. A same-stem IoStore PAK companion is never standalone.
 * No per-mod filename, path, archive ordinal or expected member hash is input.
 *
 * materialize_rows[] and members[] enumerate EVERY regular row in original
 * inventory order, including README/config/scripts/DLLs/other containers.
 * Only the unique chosen PAK gets ROLE_PAK. Every other regular row is an
 * explicitly retained, unhandled extra; its contents/optionality are unknown.
 * Directories remain inventory metadata. Names are never file destinations.
 * All member sizes/hashes and source/inventory/target identities are copied
 * from the caller-owned inventory, not independently authenticated here.
 *
 * Default policy returns ADDITIONAL_ACTION_REQUIRED and preparation_ready0
 * when extras exist. PAK_COMPONENT_ONLY permits preparation of the unique
 * chosen component while retaining/exposing all extras; whole_bundle_handled
 * remains0. That field describes row coverage only, not mod compatibility.
 * Format/platform/install/runtime authority flags are always0. The selected
 * PAK must subsequently pass the appropriate format/payload reader.
 *
 * Workspace/caps are the existing prepared grouping workspace. All controls,
 * input/output and workspace spans must be valid and disjoint for the call.
 * Known BAD_ARGUMENT preflight errors write nothing. Admitted structural
 * failures clear output arrays and publish no member list or chosen row.
 * After a valid complete grouping, semantic refusals retain the complete
 * explicit member list as diagnostics/materialization metadata; inspect
 * status plus selection_complete/preparation_ready before preparing anything.
 * No heap, I/O, retained pointers, text interpretation or installation.
 */
mb_pak_archive_status_v1 mb_pak_archive_select_v1(
    const mb_archive_prepared_view_v1 *view,
    mb_pak_archive_policy_v1 policy,
    const mb_archive_prepared_workspace_v1 *workspace,
    uint32_t *materialize_rows, uint32_t row_capacity,
    mb_pak_archive_member_v1 *members, uint32_t member_capacity,
    mb_pak_archive_selection_v1 *result);

#ifdef __cplusplus
}
#endif
#endif
