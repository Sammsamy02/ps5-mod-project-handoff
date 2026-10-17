#ifndef NATIVE_PAK_READER_EXPERIMENT_H
#define NATIVE_PAK_READER_EXPERIMENT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAK_READER_ABI_VERSION 1u
#define PAK_V11_FOOTER_BYTES 221u
#define PAK_READER_WORKSPACE_BYTES 65536u
#define PAK_READER_MAX_READ_BYTES 65536u
#define PAK_READER_MAX_PRIMARY_BYTES (4u * 1024u * 1024u)
#define PAK_READER_MAX_TOTAL_READ_BYTES (8u * 1024u * 1024u)
#define PAK_READER_MAX_READ_CALLS 256u
#define PAK_READER_MAX_FILES 65536u
#define PAK_READER_MAX_MOUNT_BYTES 1024u
#define PAK_READER_COMPRESSION_SLOTS 5u
#define PAK_READER_COMPRESSION_NAME_BYTES 32u

typedef enum pak_provider_status {
    PAK_PROVIDER_OK = 0,
    PAK_PROVIDER_IO_ERROR = 1,
    PAK_PROVIDER_UNAVAILABLE = 2,
    PAK_PROVIDER_CHANGED = 3,
    PAK_PROVIDER_CANCELLED = 4,
    PAK_PROVIDER_CONTRACT_ERROR = 5
} pak_provider_status;

typedef enum pak_status {
    PAK_STATUS_OK = 0,
    PAK_STATUS_UNSUPPORTED = 1,
    PAK_STATUS_MALFORMED = 2,
    PAK_STATUS_CHECKSUM_MISMATCH = 3,
    PAK_STATUS_RESOURCE_LIMIT = 4,
    PAK_STATUS_PROVIDER_ERROR = 5,
    PAK_STATUS_PROVIDER_SHORT_READ = 6,
    PAK_STATUS_PROVIDER_CHANGED = 7,
    PAK_STATUS_CANCELLED = 8,
    PAK_STATUS_PROVIDER_CONTRACT = 9,
    PAK_STATUS_BAD_ARGUMENT = 10,
    PAK_STATUS_INTERNAL_ERROR = 11
} pak_status;

typedef enum pak_reason {
    PAK_REASON_NONE = 0,
    PAK_REASON_BAD_ARGUMENT,
    PAK_REASON_TRAILER_TOO_SMALL,
    PAK_REASON_BAD_MAGIC,
    PAK_REASON_UNSUPPORTED_VERSION,
    PAK_REASON_ENCRYPTED_INDEX,
    PAK_REASON_BAD_COMPRESSION_NAME,
    PAK_REASON_UNSUPPORTED_COMPRESSION_NAME,
    PAK_REASON_BAD_INDEX_RANGE,
    PAK_REASON_INDEX_TOO_LARGE,
    PAK_REASON_BAD_SECONDARY_RANGE,
    PAK_REASON_OVERLAPPING_RANGES,
    PAK_REASON_BAD_MOUNT_STRING,
    PAK_REASON_MOUNT_TOO_LARGE,
    PAK_REASON_BAD_FILE_COUNT,
    PAK_REASON_TOO_MANY_FILES,
    PAK_REASON_BAD_BOOLEAN,
    PAK_REASON_MISSING_SECONDARY_INDEX,
    PAK_REASON_BAD_ENCODED_SIZE,
    PAK_REASON_BAD_NONENCODED_COUNT,
    PAK_REASON_NONENCODED_ENTRIES_UNSUPPORTED,
    PAK_REASON_TRAILING_PRIMARY_BYTES,
    PAK_REASON_PRIMARY_SHA1_MISMATCH,
    PAK_REASON_READ_LIMIT,
    PAK_REASON_READ_CALL_LIMIT,
    PAK_REASON_PROVIDER_IO,
    PAK_REASON_PROVIDER_UNAVAILABLE,
    PAK_REASON_PROVIDER_SHORT_READ,
    PAK_REASON_PROVIDER_CHANGED,
    PAK_REASON_PROVIDER_CANCELLED,
    PAK_REASON_PROVIDER_CONTRACT,
    PAK_REASON_SHA1_BACKEND
} pak_reason;

typedef enum pak_snapshot_scope {
    PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE = 1
} pak_snapshot_scope;

typedef pak_provider_status (*pak_read_at_fn)(
    void *context,
    uint64_t offset,
    uint8_t *destination,
    uint32_t requested,
    uint32_t *actual);

typedef struct pak_source_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t logical_size;
    uint64_t snapshot_token;
    uint32_t snapshot_scope;
    uint32_t reserved;
    void *context;
    pak_read_at_fn read_at;
} pak_source_v1;

typedef struct pak_limits_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t max_read_bytes;
    uint32_t max_primary_bytes;
    uint64_t max_total_read_bytes;
    uint32_t max_read_calls;
    uint32_t max_declared_files;
    uint32_t max_mount_bytes;
    uint32_t reserved;
} pak_limits_v1;

typedef struct pak_secondary_index {
    uint64_t offset;
    uint64_t size;
    uint8_t sha1[20];
} pak_secondary_index;

typedef enum pak_mount_encoding {
    PAK_MOUNT_ENCODING_NONE = 0,
    PAK_MOUNT_ENCODING_ANSI_BYTES = 1,
    PAK_MOUNT_ENCODING_UTF16LE_CODE_UNITS = 2
} pak_mount_encoding;

typedef struct pak_primary_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    pak_status status;
    pak_reason reason;
    uint32_t footer_parsed;
    uint32_t primary_sha1_checked;
    uint32_t primary_sha1_matches;
    uint32_t primary_metadata_complete;
    uint32_t encrypted_index;
    uint32_t pak_version;
    uint32_t mount_encoding;
    uint32_t mount_bytes;
    uint8_t encryption_key_guid[16];
    uint8_t mount_raw[PAK_READER_MAX_MOUNT_BYTES];
    uint32_t declared_file_count;
    uint64_t path_hash_seed;
    pak_secondary_index path_hash_index;
    pak_secondary_index directory_index;
    uint8_t compression_names[PAK_READER_COMPRESSION_SLOTS][PAK_READER_COMPRESSION_NAME_BYTES];
    uint64_t primary_index_offset;
    uint64_t primary_index_size;
    uint8_t primary_index_sha1[20];
    uint64_t requested_read_bytes;
    uint32_t read_calls;
    uint32_t reserved;
} pak_primary_result_v1;

void pak_limits_v1_default(pak_limits_v1 *limits);

pak_status pak_read_v11_primary(
    const pak_source_v1 *source,
    const pak_limits_v1 *limits,
    uint8_t *workspace,
    size_t workspace_bytes,
    pak_primary_result_v1 *result);

#ifdef __cplusplus
}
#endif

#endif
