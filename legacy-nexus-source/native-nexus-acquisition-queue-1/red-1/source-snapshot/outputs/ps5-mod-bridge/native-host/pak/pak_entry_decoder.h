#ifndef NATIVE_PAK_ENTRY_DECODER_EXPERIMENT_H
#define NATIVE_PAK_ENTRY_DECODER_EXPERIMENT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAK_ENTRY_DECODER_ABI_VERSION 1u
#define PAK_ENTRY_DECODER_MAX_BLOB_BYTES (4u * 1024u * 1024u)
#define PAK_ENTRY_DECODER_MAX_METHODS 6u
#define PAK_ENTRY_DECODER_MAX_NAME_BYTES 32u
#define PAK_ENTRY_DECODER_MAX_BLOCKS 65535u

typedef enum pak_entry_status {
    PAK_ENTRY_STATUS_OK = 0,
    PAK_ENTRY_STATUS_UNSUPPORTED = 1,
    PAK_ENTRY_STATUS_MALFORMED = 2,
    PAK_ENTRY_STATUS_RESOURCE_LIMIT = 3,
    PAK_ENTRY_STATUS_BAD_ARGUMENT = 4
} pak_entry_status;

typedef enum pak_entry_reason {
    PAK_ENTRY_REASON_NONE = 0,
    PAK_ENTRY_REASON_BAD_ARGUMENT,
    PAK_ENTRY_REASON_BAD_LIMITS,
    PAK_ENTRY_REASON_BAD_METHOD_TABLE,
    PAK_ENTRY_REASON_BLOB_TOO_LARGE,
    PAK_ENTRY_REASON_RECORD_TRUNCATED,
    PAK_ENTRY_REASON_RECORD_TOO_LARGE,
    PAK_ENTRY_REASON_ENCRYPTED,
    PAK_ENTRY_REASON_METHOD_INDEX,
    PAK_ENTRY_REASON_NEGATIVE_FIELD,
    PAK_ENTRY_REASON_BLOCK_COUNT,
    PAK_ENTRY_REASON_BLOCK_SIZE,
    PAK_ENTRY_REASON_SIZE_LIMIT,
    PAK_ENTRY_REASON_BLOCK_CAPACITY,
    PAK_ENTRY_REASON_BAD_RANGE,
    PAK_ENTRY_REASON_BLOCK_SUM
} pak_entry_reason;

typedef struct pak_entry_method_name {
    uint8_t bytes[PAK_ENTRY_DECODER_MAX_NAME_BYTES];
    uint8_t length;
    uint8_t reserved[3];
} pak_entry_method_name;

typedef struct pak_entry_method_table_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t count;
    uint32_t reserved;
    pak_entry_method_name methods[PAK_ENTRY_DECODER_MAX_METHODS];
} pak_entry_method_table_v1;

typedef struct pak_entry_limits_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t max_blob_bytes;
    uint32_t max_record_bytes;
    uint32_t max_blocks;
    uint32_t max_block_size;
    uint64_t max_compressed_size;
    uint64_t max_uncompressed_size;
    uint64_t max_total_block_bytes;
} pak_entry_limits_v1;

typedef struct pak_entry_block_v1 {
    uint64_t compressed_offset;
    uint64_t compressed_size;
    uint64_t uncompressed_offset;
    uint64_t uncompressed_size;
} pak_entry_block_v1;

typedef struct pak_entry_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    pak_entry_status status;
    pak_entry_reason reason;
    uint32_t record_complete;
    uint32_t bytes_consumed;
    uint32_t header_bytes;
    uint32_t method_index;
    uint32_t method_name_bytes;
    uint32_t block_count;
    uint32_t reserved;
    uint8_t method_name[PAK_ENTRY_DECODER_MAX_NAME_BYTES];
    uint64_t archive_offset;
    uint64_t data_offset;
    uint64_t compressed_size;
    uint64_t uncompressed_size;
    uint64_t compression_block_size;
} pak_entry_result_v1;

/*
 * Decode one standard generic v11 FPakEntry record from a caller-owned byte
 * span. The caller must establish that the enclosing archive uses this
 * standard v11 layout and provide a method table validated from its footer;
 * this function does not parse or validate either archive-level fact. The
 * input bytes, method table, and limits remain caller-owned; this
 * routine allocates no memory and retains no pointers after return. The caller
 * must keep the inputs immutable for the call and provide non-overlapping
 * storage for result and blocks_out (also disjoint from every input object).
 *
 * On success, bytes_consumed counts encoded-record bytes read from the span;
 * header_bytes is the physical per-file header size skipped before payload
 * bytes in the logical archive. Neither field means that payload bytes were
 * read, decoded, hashed, or installed. Failure clears the result and any
 * block records already written by this call.
 */
pak_entry_status pak_entry_decode_v11(
    const uint8_t *encoded_blob,
    size_t encoded_blob_bytes,
    size_t record_offset,
    uint64_t logical_archive_size,
    const pak_entry_method_table_v1 *method_table,
    const pak_entry_limits_v1 *limits,
    pak_entry_block_v1 *blocks_out,
    size_t blocks_capacity,
    pak_entry_result_v1 *result);

#ifdef __cplusplus
}
#endif

#endif
