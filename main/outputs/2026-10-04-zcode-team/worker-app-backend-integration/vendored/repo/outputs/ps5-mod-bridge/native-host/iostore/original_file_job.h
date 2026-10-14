#ifndef MB_IOSTORE_ORIGINAL_FILE_JOB_H
#define MB_IOSTORE_ORIGINAL_FILE_JOB_H
#include <stddef.h>
#include <stdint.h>

#define MB_IOSTORE_FILE_JOB_ABI_V1 1u
#define MB_IOSTORE_FILE_JOB_ALLOCATION_CAP (UINT64_C(512) << 20)
#define MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT 5u

typedef enum mb_iostore_file_job_status_v1 {
 MB_IOSTORE_FILE_JOB_OK=0, MB_IOSTORE_FILE_JOB_BAD_ARGUMENT,
 MB_IOSTORE_FILE_JOB_FD_REFUSED, MB_IOSTORE_FILE_JOB_INPUT_ALIAS,
 MB_IOSTORE_FILE_JOB_RESOURCE_LIMIT, MB_IOSTORE_FILE_JOB_NO_MEMORY,
 MB_IOSTORE_FILE_JOB_READ_ERROR, MB_IOSTORE_FILE_JOB_FORMAT_REFUSED,
 MB_IOSTORE_FILE_JOB_HASH_MISMATCH, MB_IOSTORE_FILE_JOB_CREATE_ERROR,
 MB_IOSTORE_FILE_JOB_WRITE_ERROR, MB_IOSTORE_FILE_JOB_SYNC_ERROR,
 MB_IOSTORE_FILE_JOB_READBACK_ERROR, MB_IOSTORE_FILE_JOB_SOURCE_CHANGED,
 MB_IOSTORE_FILE_JOB_CLOSE_ERROR, MB_IOSTORE_FILE_JOB_CONFLICT,
 MB_IOSTORE_FILE_JOB_STOPPED
} mb_iostore_file_job_status_v1;
typedef enum mb_iostore_file_job_stage_v1 {
 MB_IOSTORE_FILE_JOB_PREFLIGHT=0, MB_IOSTORE_FILE_JOB_INPUTS,
 MB_IOSTORE_FILE_JOB_HEADERS, MB_IOSTORE_FILE_JOB_PLANS,
 MB_IOSTORE_FILE_JOB_HEADER_MERGE, MB_IOSTORE_FILE_JOB_SMALL_DIRECTORY,
 MB_IOSTORE_FILE_JOB_PACK, MB_IOSTORE_FILE_JOB_OVERLAY,
 MB_IOSTORE_FILE_JOB_DIRECTORY, MB_IOSTORE_FILE_JOB_METADATA,
 MB_IOSTORE_FILE_JOB_SNAPSHOT, MB_IOSTORE_FILE_JOB_COMPLETE
} mb_iostore_file_job_stage_v1;
typedef int (*mb_iostore_file_job_read_at_fn_v1)(void *,int,uint64_t,uint8_t *,uint32_t,uint32_t *);
typedef int (*mb_iostore_file_job_write_at_fn_v1)(void *,int,uint64_t,const uint8_t *,uint32_t,uint32_t *);
typedef void *(*mb_iostore_file_job_allocate_fn_v1)(void *,size_t);
typedef void (*mb_iostore_file_job_release_fn_v1)(void *,void *);

#define MB_IOSTORE_FILE_JOB_CONTROL_ABI_V1 1u
/* Borrowed synchronous predicate: nonzero continues, zero stops. A stop is
 * latched for the admitted job and the predicate is not called again. */
typedef int (*mb_iostore_file_job_gate_fn_v1)(void *);
typedef enum mb_iostore_file_job_artifact_event_v1 {
 MB_IOSTORE_FILE_JOB_ARTIFACT_CREATED=1,
 MB_IOSTORE_FILE_JOB_ARTIFACT_CLOSED=2,
 MB_IOSTORE_FILE_JOB_ARTIFACT_CLOSE_FAILED=3
} mb_iostore_file_job_artifact_event_v1;
typedef int (*mb_iostore_file_job_artifact_event_fn_v1)(void *,uint32_t,int,uint32_t);
typedef struct mb_iostore_file_job_control_v1 {
 uint32_t struct_size,abi_version;
 mb_iostore_file_job_gate_fn_v1 gate;
 void *gate_context;
 mb_iostore_file_job_artifact_event_fn_v1 artifact_event;
 void *artifact_context;
} mb_iostore_file_job_control_v1;

typedef struct mb_iostore_file_job_params_v1 {
 uint32_t struct_size,abi_version;
 int mod_toc_fd,mod_ucas_fd,base_toc_fd,base_header_fd,destination_dir_fd;
 uint32_t immutable_inputs,private_unpublished_directory;
 uint64_t observed_base_ucas_bytes;
 void *io_context;
 mb_iostore_file_job_read_at_fn_v1 read_at;
 mb_iostore_file_job_write_at_fn_v1 write_at;
 void *allocator_context;
 mb_iostore_file_job_allocate_fn_v1 allocate;
 mb_iostore_file_job_release_fn_v1 release;
} mb_iostore_file_job_params_v1;

typedef struct mb_iostore_file_job_result_v1 {
 uint32_t struct_size,abi_version,status,stage,prepared;
 uint32_t created_artifact_mask,failed_artifact_index;
 int system_error;
 uint32_t installation_authority;
 uint64_t artifact_bytes[MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT];
 uint64_t allocation_peak_bytes,allocation_live_after_cleanup;
 uint32_t allocations,releases,owned_fds_closed;
 uint64_t source_read_calls,source_read_bytes,write_calls;
 uint32_t original_mod_chunks,emitted_chunks,incoming_asset_hashes_verified;
 uint32_t source_header_hash_verified,captured_base_header_hash_verified,source_snapshots_stable;
 uint64_t base_container_id;
 uint32_t merged_packages,replaced_packages,added_packages,sidecar_blocks;
 uint32_t directory_files,directory_overrides,final_chunks;
} mb_iostore_file_job_result_v1;

/* Artifact indices and immutable basenames: sidecar=0, small TOC=1, header=2,
 * directory=3, final TOC=4. The caller provides a private destination directory;
 * every file is created exclusively and remains unpublished, including partial
 * files after failure. Existing files are never changed or removed.
 *
 * Sources must be distinct nonempty regular files opened O_RDONLY. All borrowed
 * descriptors remain open and their positions unchanged. The caller guarantees
 * immutable sources, a private/disjoint directory and disjoint callback backing.
 * Callbacks must supply exact requested positional bytes, report actual progress,
 * and neither mutate this API's params/result nor close/seek descriptors. Null
 * I/O callbacks use bounded pread/pwrite; allocation callbacks are paired or null
 * (malloc/free). Allocation backing must be disjoint and suitably aligned.
 *
 * Known params/result aliases and invalid argument fields return BAD_ARGUMENT
 * without writing either object or creating files. Admitted calls initialize the
 * result and release all job allocations/owned FDs on every exit. Prepared means
 * this file-format job completed; it grants no publication/install authority.
 */
const char *mb_iostore_file_job_artifact_name_v1(uint32_t index);
mb_iostore_file_job_status_v1 mb_iostore_original_file_job_v1(
 const mb_iostore_file_job_params_v1 *,mb_iostore_file_job_result_v1 *);
/* Additive borrowed operation control; NULL preserves the old entrypoint.
 * Checks bracket I/O, allocation and bounded CPU phases. This is cooperative:
 * a blocking syscall or bounded child helper cannot be forcibly interrupted.
 * Gate/event callbacks must not mutate params/result/control or re-enter the
 * job. Their backing remains valid/disjoint from API storage for the call;
 * opaque backing extents beyond the known first byte are caller guarantees.
 * A NULL callback requires its corresponding context to be NULL. Known API
 * aliases and invalid control fields refuse untouched before any callback.
 *
 * CREATED is delivered immediately after exclusive creation and owned-FD
 * registration, before a post-create stop check or any file output. It must
 * return nonzero to accept; zero fails CREATE_ERROR. CLOSED follows confirmed
 * physical close; CLOSE_FAILED reports uncertain close. Both cleanup-event
 * return values are ignored, and their fd argument is for identity bookkeeping
 * only. Cleanup always closes owned FDs/releases allocations, even after stop.
 * A final gate after cleanup precedes prepared publication; stopped jobs leave
 * private partial artifacts and prepared=0. No unlink/publication is added. */
mb_iostore_file_job_status_v1 mb_iostore_original_file_job_gated_v1(
 const mb_iostore_file_job_params_v1 *,mb_iostore_file_job_result_v1 *,
 const mb_iostore_file_job_control_v1 *);

#define MB_IOSTORE_BATCH_FILE_JOB_MAX_SOURCES 8u
typedef struct mb_iostore_batch_source_v1 {int mod_toc_fd,mod_ucas_fd;} mb_iostore_batch_source_v1;
typedef enum mb_iostore_batch_conflict_v1 {
 MB_IOSTORE_BATCH_CONFLICT_NONE=0, MB_IOSTORE_BATCH_CONFLICT_SOURCE_IDENTITY,
 MB_IOSTORE_BATCH_CONFLICT_CHUNK_ID, MB_IOSTORE_BATCH_CONFLICT_PACKAGE_ID,
 MB_IOSTORE_BATCH_CONFLICT_DIRECTORY_PATH
} mb_iostore_batch_conflict_v1;
typedef struct mb_iostore_batch_file_job_params_v1 {
 uint32_t struct_size,abi_version;
 const mb_iostore_batch_source_v1 *sources;
 uint32_t source_count;
 int base_toc_fd,base_header_fd,destination_dir_fd;
 uint32_t immutable_inputs,private_unpublished_directory;
 uint64_t observed_base_ucas_bytes;
 void *io_context;
 mb_iostore_file_job_read_at_fn_v1 read_at;
 mb_iostore_file_job_write_at_fn_v1 write_at;
 void *allocator_context;
 mb_iostore_file_job_allocate_fn_v1 allocate;
 mb_iostore_file_job_release_fn_v1 release;
} mb_iostore_batch_file_job_params_v1;
typedef struct mb_iostore_batch_file_job_result_v1 {
 uint32_t struct_size,abi_version;
 mb_iostore_file_job_result_v1 preparation;
 uint32_t source_count,conflict_kind,conflict_source_a,conflict_source_b;
 uint8_t conflict_chunk_id[12];
 uint64_t conflict_package_id; /* Zero when the detected overlap API has no ID. */
} mb_iostore_batch_file_job_result_v1;

/* Original-source order is retained, omitting each foreign ContainerHeader and
 * appending the merged base-ID header. Strict disjoint mod scopes only: repeated
 * source inodes, nonheader chunk IDs, package IDs or exact directory-file paths
 * refuse; base overlap remains a normal replacement. No priority is inferred.
 * Chunk/file conflicts expose contributing source indices; package overlap may
 * expose an ID only if available from the bounded header API.
 * Source array and API objects are immutable/disjoint from result. Other v1
 * borrowed-FD, exclusive-artifact and cleanup contracts above remain unchanged.
 */
mb_iostore_file_job_status_v1 mb_iostore_batch_file_job_v1(
 const mb_iostore_batch_file_job_params_v1 *,mb_iostore_batch_file_job_result_v1 *);
/* Same control contract and existing strict disjoint-source batch policy. */
mb_iostore_file_job_status_v1 mb_iostore_batch_file_job_gated_v1(
 const mb_iostore_batch_file_job_params_v1 *,mb_iostore_batch_file_job_result_v1 *,
 const mb_iostore_file_job_control_v1 *);
#endif
