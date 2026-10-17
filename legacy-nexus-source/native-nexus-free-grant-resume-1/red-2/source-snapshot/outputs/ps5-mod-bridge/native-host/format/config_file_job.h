#ifndef PM_CONFIG_FILE_JOB_H
#define PM_CONFIG_FILE_JOB_H
#include <stddef.h>
#include <stdint.h>
#include "ini_scalar_merge.h"
#include "pak_stream_writer.h"
#include "config_target_selector.h"

#define PM_CONFIG_FILE_JOB_ABI_V1 1u
#define PM_CONFIG_FILE_JOB_ABI_V2 2u
#define PM_CONFIG_FILE_JOB_MAX_ALLOCATED_BYTES UINT64_C(67108864)
#define PM_CONFIG_FILE_JOB_MAX_IO_BYTES UINT64_C(2147483648)
#define PM_CONFIG_FILE_JOB_MAX_IO_CALLS 131072u

typedef enum pm_config_file_job_status {
 PM_CONFIG_FILE_JOB_OK=0, PM_CONFIG_FILE_JOB_BAD_ARGUMENT,
 PM_CONFIG_FILE_JOB_FD_REFUSED, PM_CONFIG_FILE_JOB_OUTPUT_ALIAS,
 PM_CONFIG_FILE_JOB_RESOURCE_LIMIT, PM_CONFIG_FILE_JOB_NO_MEMORY,
 PM_CONFIG_FILE_JOB_READ_ERROR, PM_CONFIG_FILE_JOB_FORMAT_REFUSED,
 PM_CONFIG_FILE_JOB_TARGET_NOT_UNIQUE, PM_CONFIG_FILE_JOB_INI_REFUSED,
 PM_CONFIG_FILE_JOB_HASH_ERROR, PM_CONFIG_FILE_JOB_WRITE_ERROR,
 PM_CONFIG_FILE_JOB_SOURCE_CHANGED, PM_CONFIG_FILE_JOB_OUTPUT_SIZE_MISMATCH,
 PM_CONFIG_FILE_JOB_STOPPED
} pm_config_file_job_status;
typedef enum pm_config_file_job_stage {
 PM_CONFIG_FILE_JOB_PREFLIGHT=0, PM_CONFIG_FILE_JOB_PRIMARY,
 PM_CONFIG_FILE_JOB_SECONDARY, PM_CONFIG_FILE_JOB_DIRECTORY,
 PM_CONFIG_FILE_JOB_ENTRIES, PM_CONFIG_FILE_JOB_INI,
 PM_CONFIG_FILE_JOB_WRITER, PM_CONFIG_FILE_JOB_SNAPSHOT,
 PM_CONFIG_FILE_JOB_COMPLETE
} pm_config_file_job_stage;
typedef int (*pm_config_file_read_at_fn)(void *,int,uint64_t,uint8_t *,uint32_t,uint32_t *);
typedef int (*pm_config_file_write_at_fn)(void *,int,uint64_t,const uint8_t *,uint32_t,uint32_t *);
typedef void *(*pm_config_file_allocate_fn)(void *,size_t);
typedef void (*pm_config_file_release_fn)(void *,void *);
typedef struct pm_config_file_job_params_v1 {
 uint32_t struct_size,abi_version;
 int original_fd,member_fd,output_fd;
 uint32_t immutable_inputs,unpublished_output;
 uint64_t original_bytes,member_bytes;
 pm_ini_bytes target_entry,mount;
 uint64_t path_hash_seed;
 const pm_ini_bytes *sections;
 uint32_t section_count,reserved;
 void *io_context;
 pm_config_file_read_at_fn read_at;
 pm_config_file_write_at_fn write_at;
 void *allocator_context;
 pm_config_file_allocate_fn allocate;
 pm_config_file_release_fn release;
} pm_config_file_job_params_v1;
typedef struct pm_config_file_job_result_v1 {
 uint32_t struct_size,abi_version;
 pm_config_file_job_status status;
 pm_config_file_job_stage stage;
 uint32_t component_status,component_detail;
 uint32_t archive_complete,source_snapshot_stable;
 uint64_t base_bytes,source_bytes,merged_bytes;
 uint32_t scalar_winners,replacements,additions;
 uint32_t source_entries_sha1_checked;
 uint8_t merged_sha256[32];
 uint64_t allocation_peak_bytes,allocation_live_after_cleanup;
 uint32_t allocation_calls,release_calls;
 uint64_t requested_read_bytes,requested_write_bytes;
 uint32_t read_calls,write_calls;
 mb_pak_stream_result_v1 writer;
} pm_config_file_job_result_v1;

/* Ordinary immutable-file preparation, not installation or publication.
 * All descriptors are BORROWED and remain open. Inputs must be regular files
 * opened read-only. Output must be a distinct regular inode, writable, empty,
 * unpublished and without O_APPEND. Caller holds all inputs/profile spans
 * stable and disjoint from result/allocator backing for the whole job; caller
 * guarantees exclusive access to output and owns sync/readback/publication.
 * Exact I/O callbacks return 0 with actual==requested, never retain pointers,
 * and use ordinary pread/pwrite semantics (no descriptor-position mutation).
 * Allocator returns distinct writable blocks and release accepts each block
 * exactly once. Peak measures requested job-managed bytes, excluding allocator
 * overhead and total process/library heap. Every admitted exit releases all
 * job-managed blocks. No FILE, argv, stdout, exit, filename operations, close,
 * network, runtime hook or target build. Failed output may contain partial
 * bytes and MUST remain unpublished/discarded by its caller. Complete means
 * prepared only; it does not imply fsync/durability/install/runtime support. */
pm_config_file_job_status pm_config_file_job_v1(
 const pm_config_file_job_params_v1 *,pm_config_file_job_result_v1 *);
/* Additive cooperative gate using the caller's existing operation lifetime.
 * NULL/NULL preserves the old entrypoint behavior. NULL gate with non-NULL
 * context, ABI errors and known aliases refuse before mutation. Gate context
 * backing is borrowed and must remain valid, disjoint from params/result,
 * profile spans and all job-managed allocation backing, for the whole call.
 * Only its first byte is available for checking known opaque-context aliases;
 * the full backing extent remains the caller's contract. The predicate must
 * not re-enter the job or mutate supplied inputs, result or owned workspace.
 * A first nonzero return is latched, with no further predicate invocation.
 * Checks bracket borrowed callbacks, bounded row/phase work and the gated
 * writer. Cleanup always releases admitted allocations, even after a stop;
 * final completion is published only after cleanup and the final gate.
 * STOPPED revokes completion and merged/writer digests; diagnostic counts and
 * component details remain. Borrowed FDs stay open and private partial output
 * remains caller-owned. Blocking callbacks and bounded child parser, selector,
 * scalar-merge and hash calls cannot be interrupted by this predicate. */
pm_config_file_job_status pm_config_file_job_gated_v1(
 const pm_config_file_job_params_v1 *,pm_config_file_job_result_v1 *,
 mb_pak_prepare_gate_v1 gate,void *gate_context);

typedef struct pm_config_file_job_params_v2 {
 uint32_t struct_size,abi_version;
 pm_config_file_job_params_v1 base;
 pm_ini_bytes platform;
} pm_config_file_job_params_v2;
typedef struct pm_config_file_job_result_v2 {
 uint32_t struct_size,abi_version;
 uint32_t selector_status,resolved_row,resolved_path_bytes,resolved_mount_bytes;
 uint32_t selector_root_count,selector_target_count;
 uint64_t resolved_path_hash_seed;
 uint8_t resolved_path[MB_PAK_STREAM_MAX_PATH_BYTES+1];
 uint8_t resolved_mount[1025];
 pm_config_file_job_result_v1 preparation;
} pm_config_file_job_result_v2;
/* Automatic Unreal project-platform target selection from all validated
 * decoded PAK entries. base target_entry/mount MUST both be {NULL,0} and base
 * path_hash_seed MUST be0. No project/fulltarget/container-layout facts supplied.
 * platform is a disjoint immutable ASCII alnum/_/- span1..32 bytes. Missing or
 * ambiguous target refuses before INI merge/output. Writer mount/seed derive
 * from captured verified metadata. Owning resolved facts are diagnostic only;
 * preparation.archive_complete remains the sole preparation completion flag.
 * Known aliases/bad arguments leave the WHOLEv2result untouched. Same borrowed
 * FD/allocator/immutable/publication contract as v1; v1 behavior unchanged. */
pm_config_file_job_status pm_config_file_job_v2(
 const pm_config_file_job_params_v2 *,pm_config_file_job_result_v2 *);
/* Same gate/cleanup contract as gated v1; validation covers the WHOLE v2
 * params/result and platform span before either result is initialized. */
pm_config_file_job_status pm_config_file_job_gated_v2(
 const pm_config_file_job_params_v2 *,pm_config_file_job_result_v2 *,
 mb_pak_prepare_gate_v1 gate,void *gate_context);
#endif
