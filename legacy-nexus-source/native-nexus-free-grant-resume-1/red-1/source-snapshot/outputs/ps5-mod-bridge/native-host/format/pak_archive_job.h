#ifndef MB_PAK_ARCHIVE_JOB_H
#define MB_PAK_ARCHIVE_JOB_H

#include "../archive/pak_selection.h"
#include "examples/pak_mod_union/prepare_union.h"

#define MB_PAK_ARCHIVE_JOB_ABI_V1 1u
#define MB_PAK_ARCHIVE_JOB_ROWS 512u
#define MB_PAK_ARCHIVE_JOB_NAME_BYTES (256u * 1024u)
#define MB_PAK_ARCHIVE_JOB_JOURNAL_CAPACITY (MB_PAK_ARCHIVE_JOB_ROWS + 3u)
#define MB_PAK_ARCHIVE_JOB_MAX_INPUT_BYTES (UINT64_C(512) << 20)
#define MB_PAK_ARCHIVE_JOB_ERROR_BYTES 96u

typedef enum mb_pak_archive_job_status_v1 {
    MB_PAK_ARCHIVE_JOB_OK = 0,
    MB_PAK_ARCHIVE_JOB_BAD_ARGUMENT,
    MB_PAK_ARCHIVE_JOB_FD_REFUSED,
    MB_PAK_ARCHIVE_JOB_SCAN_REFUSED,
    MB_PAK_ARCHIVE_JOB_SELECTION_REFUSED,
    MB_PAK_ARCHIVE_JOB_ADDITIONAL_ACTION_REQUIRED,
    MB_PAK_ARCHIVE_JOB_MATERIALIZATION_REFUSED,
    MB_PAK_ARCHIVE_JOB_MEMBER_REFUSED,
    MB_PAK_ARCHIVE_JOB_OUTPUT_REFUSED,
    MB_PAK_ARCHIVE_JOB_UNION_REFUSED,
    MB_PAK_ARCHIVE_JOB_SOURCE_CHANGED,
    MB_PAK_ARCHIVE_JOB_CLOSE_FAILED,
    MB_PAK_ARCHIVE_JOB_STOPPED
} mb_pak_archive_job_status_v1;

typedef enum mb_pak_archive_job_stage_v1 {
    MB_PAK_ARCHIVE_JOB_STAGE_ARGUMENTS = 0,
    MB_PAK_ARCHIVE_JOB_STAGE_INPUTS,
    MB_PAK_ARCHIVE_JOB_STAGE_SCAN,
    MB_PAK_ARCHIVE_JOB_STAGE_SELECTION,
    MB_PAK_ARCHIVE_JOB_STAGE_QUARANTINE,
    MB_PAK_ARCHIVE_JOB_STAGE_MATERIALIZATION,
    MB_PAK_ARCHIVE_JOB_STAGE_MEMBER,
    MB_PAK_ARCHIVE_JOB_STAGE_OUTPUT,
    MB_PAK_ARCHIVE_JOB_STAGE_UNION,
    MB_PAK_ARCHIVE_JOB_STAGE_SNAPSHOT,
    MB_PAK_ARCHIVE_JOB_STAGE_CLOSE,
    MB_PAK_ARCHIVE_JOB_STAGE_COMPLETE
} mb_pak_archive_job_stage_v1;

/* Synchronous trusted caller callback. Zero means success. Any returned FD
 * belongs to the caller even when the callback/job subsequently fails.
 * Context backing must be disjoint from params, workspace and result. The
 * callback must not mutate those objects or the immutable inputs. */
typedef int (*mb_pak_archive_job_acquire_output_v1)(void *context, int *fd);

typedef struct mb_pak_archive_job_params_v1 {
    uint32_t struct_size, abi_version;
    mb_archive_source_v1 source;
    int base_fd, quarantine_fd, output_fd;
    uint32_t immutable_inputs, private_quarantine, unpublished_output;
    uint32_t policy, reserved;
    uint64_t base_bytes;
    mb_pak_archive_job_acquire_output_v1 acquire_output;
    void *output_context;
} mb_pak_archive_job_params_v1;

/* Entire backing is caller-owned, valid and disjoint for the call. Fresh
 * all-zero journal is required; an admitted failure retains its full journal,
 * inventory names and member diagnostics. Do not reuse without explicit caller
 * recovery. No large automatic or static scratch arrays exist in the job. */
typedef struct mb_pak_archive_job_workspace_v1 {
    uint32_t struct_size, abi_version;
    mb_archive_row_v1 rows[MB_PAK_ARCHIVE_JOB_ROWS];
    mb_archive_row_v1 replay_rows[MB_PAK_ARCHIVE_JOB_ROWS];
    char names[MB_PAK_ARCHIVE_JOB_NAME_BYTES];
    char replay_names[MB_PAK_ARCHIVE_JOB_NAME_BYTES];
    char group_keys[MB_PAK_ARCHIVE_JOB_NAME_BYTES];
    uint32_t scan_sort[MB_PAK_ARCHIVE_JOB_ROWS];
    uint32_t replay_sort[MB_PAK_ARCHIVE_JOB_ROWS];
    uint32_t group_sort[MB_PAK_ARCHIVE_JOB_ROWS];
    uint32_t selected[MB_PAK_ARCHIVE_JOB_ROWS];
    uint8_t transfer[MB_ARCHIVE_CHUNK_BYTES];
    uint8_t replay_transfer[MB_ARCHIVE_CHUNK_BYTES];
    mb_archive_group_v1 groups[MB_PAK_ARCHIVE_JOB_ROWS];
    mb_archive_group_member_v1 group_members[MB_PAK_ARCHIVE_JOB_ROWS];
    mb_pak_archive_member_v1 members[MB_PAK_ARCHIVE_JOB_ROWS];
    mb_archive_owned_file_v1 journal[MB_PAK_ARCHIVE_JOB_JOURNAL_CAPACITY];
} mb_pak_archive_job_workspace_v1;

typedef struct mb_pak_archive_job_result_v1 {
    uint32_t struct_size, abi_version;
    mb_pak_archive_job_status_v1 status;
    mb_pak_archive_job_stage_v1 stage;
    char error[MB_PAK_ARCHIVE_JOB_ERROR_BYTES];
    uint32_t scan_status, selection_status, materialize_status, union_status;
    uint32_t selection_called, materialization_complete;
    uint32_t preparation_complete, output_acquired, source_stable;
    uint32_t installation_supported, runtime_supported;
    int output_fd;
    uint64_t selected_pak_bytes;
    mb_archive_inventory_v1 inventory;
    mb_pak_archive_selection_v1 selection;
    mb_archive_materialized_v1 materialized;
    mb_pak_union_result_v1 union_result;
    mb_archive_budget_v1 budget;
} mb_pak_archive_job_result_v1;

#ifdef __cplusplus
extern "C" {
#endif

/* Borrowed regular read-only source/base FDs, positive declared sizes <=512MiB.
 * Quarantine is borrowed, caller-exclusive, owned mode0700 and initially empty.
 * Output is borrowed writable, nonappend, private, empty and a distinct inode.
 * Either supply output_fd>=0 with no callback, or -1 with acquire_output. The
 * latter is called only after selection is ready, every regular archive member
 * is materialized and the unique selected PAK is opened/pinned to its journal.
 * No callback is invoked on default-policy additional-action refusal.
 *
 * Positional input/output I/O preserves file positions; quarantine directory
 * cursor is unspecified. Only the selected journal member descriptor is owned
 * and closed by the job. The temporary directory-iteration descriptor is also
 * closed internally. All borrowed/acquired descriptors remain caller-owned.
 * No paths, stdout, exit, subprocess, transport, output fsync or publication.
 * Archive materialization retains its existing private-journal sync behavior. Partial
 * outputs and journals must remain private on failure. This prepares the
 * admitted v3 None component against a v11 None base, never install/runtime.
 *
 * Known invalid storage/ABI/assertions and nonzero journal return BAD_ARGUMENT
 * without mutating params, result or workspace. Admitted refusals initialize
 * an owning result and retain diagnostics. Input immutability/private-directory
 * assertions are caller obligations; ordinary fstat snapshots are not seals.
 */
mb_pak_archive_job_status_v1 mb_pak_archive_job_prepare_v1(
    const mb_pak_archive_job_params_v1 *params,
    mb_pak_archive_job_workspace_v1 *workspace,
    mb_pak_archive_job_result_v1 *result);

/* Same strict v1 storage/ownership contract with a borrowed operation gate.
 * NULL/NULL is ungated; NULL gate/non-NULL context refuses before mutation.
 * One latched predicate is shared by archive budget and union preparation;
 * no child deadline/clock is created. Deferred output acquisition is gated
 * before and after the trusted callback. Cleanup cannot be cancelled. Final
 * preparation completion is gated after owned-member close; a stopped call
 * retains private output/journal/resource diagnostics but no artifact-complete
 * claim. The caller owns the reason for a nonzero predicate result. */
mb_pak_archive_job_status_v1 mb_pak_archive_job_prepare_gated_v1(
    const mb_pak_archive_job_params_v1 *params,
    mb_pak_archive_job_workspace_v1 *workspace,
    mb_pak_archive_job_result_v1 *result,
    mb_pak_prepare_gate_v1 gate, void *gate_context);

const char *mb_pak_archive_job_stage_name_v1(mb_pak_archive_job_stage_v1 stage);

#ifdef __cplusplus
}
#endif
#endif
