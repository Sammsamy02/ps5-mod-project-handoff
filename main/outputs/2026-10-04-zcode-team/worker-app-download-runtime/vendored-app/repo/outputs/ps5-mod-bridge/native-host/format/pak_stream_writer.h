#ifndef MB_PAK_STREAM_WRITER_H
#define MB_PAK_STREAM_WRITER_H
#include <stddef.h>
#include <stdint.h>

#define MB_PAK_STREAM_ABI_VERSION 1u
#define MB_PAK_STREAM_MAX_FILES 8192u
#define MB_PAK_STREAM_MAX_DIRECTORIES 16384u
#define MB_PAK_STREAM_MAX_PATH_BYTES 1024u
#define MB_PAK_STREAM_MAX_NAME_BYTES 4194304u
#define MB_PAK_STREAM_MAX_PAYLOAD_BYTES UINT64_C(536870912)
#define MB_PAK_STREAM_MAX_ARCHIVE_BYTES UINT64_C(536870912)
#define MB_PAK_STREAM_MAX_WORK UINT64_C(8589934592)
#define MB_PAK_STREAM_MAX_INDEX_BYTES 8388608u
#define MB_PAK_STREAM_TRANSFER_BYTES 65536u
#define MB_PAK_STREAM_MAX_IO_BYTES UINT64_C(2147483648)
#define MB_PAK_STREAM_MAX_IO_CALLS 65536u

/* Borrowed synchronous operation predicate: zero continues, any nonzero value
 * stops. The caller owns its deadline/cancellation state and terminal reason.
 * Each admitted operation latches a stop and does not invoke this callback
 * again. It must not mutate supplied inputs/workspace/result, retain borrowed
 * storage, or re-enter preparation. Its context backing must remain valid and
 * disjoint from those objects for the call; NULL context is permitted. */
typedef int (*mb_pak_prepare_gate_v1)(void *context);

typedef enum mb_pak_stream_status {
 MB_PAK_STREAM_OK=0, MB_PAK_STREAM_BAD_ARGUMENT, MB_PAK_STREAM_INVALID_PATH,
 MB_PAK_STREAM_UNSUPPORTED_NAME, MB_PAK_STREAM_PATH_CONFLICT,
 MB_PAK_STREAM_RESOURCE_LIMIT, MB_PAK_STREAM_BUFFER_TOO_SMALL,
 MB_PAK_STREAM_SHA1_ERROR, MB_PAK_STREAM_INTERNAL_ERROR,
 MB_PAK_STREAM_PROVIDER_ERROR, MB_PAK_STREAM_SHORT_IO,
 MB_PAK_STREAM_PAYLOAD_SHA1_MISMATCH, MB_PAK_STREAM_STOPPED
} mb_pak_stream_status;
typedef struct mb_pak_stream_profile_v1 {
 uint32_t struct_size, abi_version;
 const uint8_t *mount;
 uint32_t mount_bytes, reserved;
 uint64_t path_hash_seed;
} mb_pak_stream_profile_v1;
typedef struct mb_pak_stream_file_v1 {
 uint32_t struct_size, abi_version;
 const uint8_t *path;
 uint32_t path_bytes, verify_sha1;
 uint64_t payload_bytes;
 uint8_t expected_sha1[20];
 uint32_t reserved;
} mb_pak_stream_file_v1;
typedef struct mb_pak_stream_limits_v1 {
 uint32_t struct_size, abi_version;
 uint32_t max_files,max_directories,max_path_bytes,max_name_bytes;
 uint64_t max_payload_bytes,max_archive_bytes,max_work;
} mb_pak_stream_limits_v1;
typedef struct mb_pak_stream_record_scratch {
 uint32_t input_index,physical_offset,filename_offset,directory_id;
 uint64_t path_hash;
 uint8_t sha1[20];
} mb_pak_stream_record_scratch;
typedef struct mb_pak_stream_directory_scratch {
 uint32_t input_index,prefix_bytes,file_count;
} mb_pak_stream_directory_scratch;
typedef struct mb_pak_stream_workspace_v1 {
 mb_pak_stream_record_scratch *records;
 uint32_t records_capacity;
 mb_pak_stream_directory_scratch *directories;
 uint32_t directories_capacity;
 uint32_t *order;
 uint32_t order_capacity;
 uint8_t *index_arena;
 size_t index_capacity;
 uint8_t *transfer;
 size_t transfer_bytes;
} mb_pak_stream_workspace_v1;
/* Exact synchronous I/O: return 0 with *actual==requested. Providers must
 * assert immutable/disjoint backing and never retain a borrowed pointer. */
typedef int (*mb_pak_stream_read_fn)(void *,uint32_t,uint64_t,uint8_t *,uint32_t,uint32_t *);
typedef int (*mb_pak_stream_write_fn)(void *,uint64_t,const uint8_t *,uint32_t,uint32_t *);
typedef struct mb_pak_stream_io_v1 {
 uint32_t struct_size,abi_version;
 void *context;
 mb_pak_stream_read_fn read_payload;
 mb_pak_stream_write_fn write_at;
 uint32_t immutable_source,unpublished_destination;
} mb_pak_stream_io_v1;
typedef struct mb_pak_stream_result_v1 {
 uint32_t struct_size,abi_version;
 mb_pak_stream_status status;
 uint32_t archive_complete,file_count,directory_count;
 uint64_t archive_bytes,required_bytes,work_used,index_bytes;
 uint64_t requested_read_bytes,requested_write_bytes;
 uint32_t read_calls,write_calls;
 uint8_t primary_sha1[20],path_index_sha1[20],directory_sha1[20];
} mb_pak_stream_result_v1;
void mb_pak_stream_limits_v1_default(mb_pak_stream_limits_v1 *);
/* Prototype pure format adapter: classic unsigned/unencrypted v11 None,
 * printable ASCII names and trusted mount/seed only. No heap/filesystem,
 * network, install/runtime operation. All geometry/work/callback totals are
 * bounded before first callback. Caller supplies disjoint mutable storage
 * and stable immutable descriptors/paths/provider backing. Optional expected
 * SHA1 is checked after one streaming pass before the file header is emitted.
 * Metadata stays in <=8MiB arena; payload workspace exactly64KiB.
 * Only archive_complete publishes an artifact. On I/O failure the destination
 * may contain partial bytes and MUST remain unpublished/discarded by owner;
 * no truncate/erase or original-file mutation is performed. All scratch clears
 * after admitted preflight. Known aliases/ABI errors write nothing. */
mb_pak_stream_status mb_pak_stream_write_v11_none(
 const mb_pak_stream_profile_v1 *,const mb_pak_stream_file_v1 *,uint32_t,
 const mb_pak_stream_limits_v1 *,const mb_pak_stream_io_v1 *,
 mb_pak_stream_workspace_v1 *,mb_pak_stream_result_v1 *);
/* Additive cooperative gate. No child clock/deadline is started. NULL/NULL
 * retains the v1 behavior; NULL gate with non-NULL context is BAD_ARGUMENT
 * without mutation. Checks cover bounded validation/work batches and both
 * sides of provider calls. Scratch cleanup always runs; final completion and
 * digests are published only after cleanup and the final gate. Blocking calls
 * and bounded child computations cannot be interrupted by this predicate. */
mb_pak_stream_status mb_pak_stream_write_v11_none_gated_v1(
 const mb_pak_stream_profile_v1 *,const mb_pak_stream_file_v1 *,uint32_t,
 const mb_pak_stream_limits_v1 *,const mb_pak_stream_io_v1 *,
 mb_pak_stream_workspace_v1 *,mb_pak_stream_result_v1 *,
 mb_pak_prepare_gate_v1 gate,void *gate_context);
#endif
