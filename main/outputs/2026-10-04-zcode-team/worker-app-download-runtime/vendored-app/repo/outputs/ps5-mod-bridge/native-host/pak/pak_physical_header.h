#ifndef NATIVE_PAK_PHYSICAL_HEADER_EXPERIMENT_H
#define NATIVE_PAK_PHYSICAL_HEADER_EXPERIMENT_H

#include <stddef.h>
#include <stdint.h>

#include "pak_entry_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAK_PHYSICAL_HEADER_ABI_VERSION 1u
#define PAK_PHYSICAL_HEADER_MAX_BLOCKS 64u
#define PAK_PHYSICAL_HEADER_MAX_BYTES (53u + 4u + 16u * PAK_PHYSICAL_HEADER_MAX_BLOCKS)
#define PAK_PHYSICAL_HEADER_MAX_METHOD_NAME_BYTES 32u

typedef enum pak_physical_status {
    PAK_PHYSICAL_STATUS_OK = 0,
    PAK_PHYSICAL_STATUS_UNSUPPORTED = 1,
    PAK_PHYSICAL_STATUS_MALFORMED = 2,
    PAK_PHYSICAL_STATUS_RESOURCE_LIMIT = 3,
    PAK_PHYSICAL_STATUS_BAD_ARGUMENT = 4
} pak_physical_status;

typedef enum pak_physical_reason {
    PAK_PHYSICAL_REASON_NONE = 0,
    PAK_PHYSICAL_REASON_BAD_ARGUMENT,
    PAK_PHYSICAL_REASON_BAD_LIMITS,
    PAK_PHYSICAL_REASON_BAD_METHOD_TABLE,
    PAK_PHYSICAL_REASON_INPUT_OUTPUT_OVERLAP,
    PAK_PHYSICAL_REASON_HEADER_TRUNCATED,
    PAK_PHYSICAL_REASON_HEADER_TOO_LARGE,
    PAK_PHYSICAL_REASON_HEADER_TRAILING_BYTES,
    PAK_PHYSICAL_REASON_NEGATIVE_FIELD,
    PAK_PHYSICAL_REASON_METHOD_INDEX,
    PAK_PHYSICAL_REASON_UNSUPPORTED_CODEC,
    PAK_PHYSICAL_REASON_BLOCK_COUNT,
    PAK_PHYSICAL_REASON_BLOCK_CAPACITY,
    PAK_PHYSICAL_REASON_SIZE_LIMIT,
    PAK_PHYSICAL_REASON_UNSUPPORTED_FLAGS,
    PAK_PHYSICAL_REASON_BLOCK_RANGE,
    PAK_PHYSICAL_REASON_BLOCK_OVERLAP
} pak_physical_reason;

typedef struct pak_physical_limits_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t max_header_bytes;
    uint32_t max_blocks;
    uint32_t max_physical_block_size;
    uint32_t reserved;
    uint64_t max_compressed_size;
    uint64_t max_uncompressed_size;
} pak_physical_limits_v1;

typedef struct pak_physical_block_v1 {
    uint64_t relative_start;
    uint64_t relative_end;
} pak_physical_block_v1;

typedef struct pak_physical_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    pak_physical_status status;
    pak_physical_reason reason;
    uint32_t record_complete;
    uint32_t bytes_consumed;
    uint32_t header_bytes;
    uint32_t method_index;
    uint32_t method_name_bytes;
    uint32_t block_count;
    uint32_t flags;
    uint32_t physical_block_size;
    uint64_t stored_offset;
    uint64_t compressed_size;
    uint64_t uncompressed_size;
    uint8_t method_name[PAK_PHYSICAL_HEADER_MAX_METHOD_NAME_BYTES];
    uint8_t sha1[20];
} pak_physical_result_v1;

/*
 * Parse exactly one standard generic v11 repeated physical FPakEntry header.
 * The enclosing caller must already establish ordinary v11 layout and supply
 * a validated method table whose slot 0 is "None". This routine supports
 * exactly the names "None" and "Zlib"; it performs metadata parsing only.
 *
 * The input span must be immutable and exactly 53 bytes for None or
 * 53 + 4 + 16*N bytes for Zlib (1 <= N <= 64). All fields and block ranges
 * are copied into caller-owned output; this function allocates no memory and
 * retains no pointers. Output storage must be disjoint from input, methods,
 * and limits. Non-OK results after output ranges are validated clear the
 * result and supplied block capacity. An invalid/overlapping output view is
 * rejected without writing through aliased pointers. The stored offset and
 * SHA1 are preserved but not reconciled or verified; no payload bytes are
 * read or decoded.
 */
pak_physical_status pak_physical_header_parse_v11(
    const uint8_t *header_bytes,
    size_t header_span_bytes,
    const pak_entry_method_table_v1 *method_table,
    const pak_physical_limits_v1 *limits,
    pak_physical_block_v1 *blocks_out,
    size_t blocks_capacity,
    pak_physical_result_v1 *result);

#ifdef __cplusplus
}
#endif

#endif
