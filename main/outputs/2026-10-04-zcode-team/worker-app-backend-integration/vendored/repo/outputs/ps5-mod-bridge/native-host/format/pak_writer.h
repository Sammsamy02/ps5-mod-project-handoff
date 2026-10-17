#ifndef MODBRIDGE_PAK_WRITER_H
#define MODBRIDGE_PAK_WRITER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_PAK_WRITER_ABI_VERSION 1u
#define MB_PAK_WRITER_MAX_FILES 256u
#define MB_PAK_WRITER_MAX_DIRECTORIES 2048u
#define MB_PAK_WRITER_MAX_PATH_BYTES 1024u
#define MB_PAK_WRITER_MAX_NAME_BYTES 65536u
#define MB_PAK_WRITER_MAX_PAYLOAD_BYTES UINT64_C(4194304)
#define MB_PAK_WRITER_MAX_ARCHIVE_BYTES UINT64_C(16777216)
#define MB_PAK_WRITER_MAX_WORK UINT64_C(67108864)

typedef enum mb_pak_writer_status {
    MB_PAK_WRITE_OK = 0,
    MB_PAK_WRITE_BAD_ARGUMENT,
    MB_PAK_WRITE_INVALID_PATH,
    MB_PAK_WRITE_UNSUPPORTED_NAME,
    MB_PAK_WRITE_PATH_CONFLICT,
    MB_PAK_WRITE_RESOURCE_LIMIT,
    MB_PAK_WRITE_BUFFER_TOO_SMALL,
    MB_PAK_WRITE_SHA1_ERROR,
    MB_PAK_WRITE_INTERNAL_ERROR
} mb_pak_writer_status;

typedef struct mb_pak_writer_profile_v1 {
    uint32_t struct_size, abi_version;
    const uint8_t *mount;
    uint32_t mount_bytes, reserved;
    uint64_t path_hash_seed;
} mb_pak_writer_profile_v1;

typedef struct mb_pak_writer_file_v1 {
    uint32_t struct_size, abi_version;
    const uint8_t *path;
    uint32_t path_bytes, reserved;
    const uint8_t *payload;
    uint64_t payload_bytes;
} mb_pak_writer_file_v1;

typedef struct mb_pak_writer_limits_v1 {
    uint32_t struct_size, abi_version;
    uint32_t max_files, max_directories, max_path_bytes, max_name_bytes;
    uint64_t max_payload_bytes, max_archive_bytes, max_work;
} mb_pak_writer_limits_v1;

/* Scratch types expose storage size/alignment only; their contents are private,
 * unpublished and erased after the call. No pointer is saved in scratch. */
typedef struct mb_pak_writer_record_scratch {
    uint32_t input_index, physical_offset, filename_offset, directory_id;
    uint64_t path_hash;
    uint8_t sha1[20];
} mb_pak_writer_record_scratch;
typedef struct mb_pak_writer_directory_scratch {
    uint32_t input_index, prefix_bytes, file_count;
} mb_pak_writer_directory_scratch;
typedef struct mb_pak_writer_workspace_v1 {
    mb_pak_writer_record_scratch records[MB_PAK_WRITER_MAX_FILES];
    mb_pak_writer_directory_scratch directories[MB_PAK_WRITER_MAX_DIRECTORIES];
    uint32_t order[MB_PAK_WRITER_MAX_FILES];
} mb_pak_writer_workspace_v1;

typedef struct mb_pak_writer_result_v1 {
    uint32_t struct_size, abi_version;
    mb_pak_writer_status status;
    uint32_t archive_complete, file_count, directory_count;
    uint64_t archive_bytes, required_bytes, work_used;
    uint8_t primary_sha1[20], path_index_sha1[20], directory_sha1[20];
} mb_pak_writer_result_v1;

void mb_pak_writer_limits_v1_default(mb_pak_writer_limits_v1 *limits);

/* Synchronous classic-v11/None byte preparation only. No heap, I/O, signing,
 * engine/runtime/cook/install operation or retained input pointer. All inputs
 * must be valid immutable C backing for the call. Paths are nonempty relative
 * printable ASCII slash paths; no empty/dot/dot-dot component, backslash,
 * colon, wildcards, or component >255 bytes. Casefold duplicates, ancestors,
 * directory aliases and FNV hash collisions fail closed. Mount is trusted
 * metadata: '/' or a relative slash-terminated path, with leading '..' allowed.
 * Every input/control object must be disjoint from output/workspace/result;
 * immutable input spans may share backing. No concurrent mutation is allowed.
 *
 * Invalid backing/alignment/known aliases or ABI preflight write nothing.
 * After admitted preflight, result is owning, scratch is erased on ALL returns,
 * and failure has archive_complete/archive_bytes/counts/digests zero. A short
 * output (including NULL/0 size query) reports required_bytes after validation.
 * Validation/limit/capacity failures leave output untouched; a backend failure
 * after writes erases the proposed archive span. Bytes after archive_bytes are
 * never written. Only MB_PAK_WRITE_OK publishes an archive. Limits only tighten;
 * work is weighted validation/comparison/byte-processing, not a CPU deadline.
 * Fixed scratch cleanup occurs even if work is exhausted. */
mb_pak_writer_status mb_pak_write_v11_none(
    const mb_pak_writer_profile_v1 *profile,
    const mb_pak_writer_file_v1 *files, uint32_t file_count,
    const mb_pak_writer_limits_v1 *limits,
    mb_pak_writer_workspace_v1 *workspace,
    uint8_t *output, size_t output_capacity,
    mb_pak_writer_result_v1 *result);

#ifdef __cplusplus
}
#endif
#endif
