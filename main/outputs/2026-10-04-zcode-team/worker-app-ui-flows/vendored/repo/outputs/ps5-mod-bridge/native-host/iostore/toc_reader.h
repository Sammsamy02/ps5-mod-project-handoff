#ifndef MODBRIDGE_IOSTORE_TOC_READER_H
#define MODBRIDGE_IOSTORE_TOC_READER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_IOSTORE_ABI_VERSION 1u
#define MB_IOSTORE_HEADER_BYTES 144u
#define MB_IOSTORE_MAX_CHUNKS 1048576u
#define MB_IOSTORE_MAX_BLOCKS 4194304u
#define MB_IOSTORE_MAX_PARTITIONS 16u
#define MB_IOSTORE_MAX_METHODS 16u
#define MB_IOSTORE_MAX_METHOD_NAME_BYTES 64u
#define MB_IOSTORE_MAX_TOC_BYTES UINT64_C(268435456)

typedef enum mb_iostore_status {
    MB_IOSTORE_OK = 0, MB_IOSTORE_BAD_ARGUMENT, MB_IOSTORE_TRUNCATED,
    MB_IOSTORE_MALFORMED, MB_IOSTORE_UNSUPPORTED, MB_IOSTORE_RESOURCE_LIMIT,
    MB_IOSTORE_OUT_OF_RANGE
} mb_iostore_status;
typedef enum mb_iostore_reason {
    MB_IOSTORE_REASON_NONE = 0, MB_IOSTORE_REASON_CAPTURE,
    MB_IOSTORE_REASON_MAGIC, MB_IOSTORE_REASON_VERSION,
    MB_IOSTORE_REASON_FLAGS, MB_IOSTORE_REASON_HEADER_WIDTH,
    MB_IOSTORE_REASON_TABLE_WIDTH, MB_IOSTORE_REASON_COUNT_LIMIT,
    MB_IOSTORE_REASON_TOC_RANGE, MB_IOSTORE_REASON_PARTITION,
    MB_IOSTORE_REASON_BLOCK_SIZE, MB_IOSTORE_REASON_DIRECTORY_FLAG,
    MB_IOSTORE_REASON_INDEX, MB_IOSTORE_REASON_SPAN_BINDING,
    MB_IOSTORE_REASON_CHUNK_RANGE, MB_IOSTORE_REASON_METHOD,
    MB_IOSTORE_REASON_NONE_SIZE, MB_IOSTORE_REASON_UCAS_RANGE
} mb_iostore_reason;

typedef struct mb_iostore_span_v1 {
    const uint8_t *data;
    size_t bytes;
    uint64_t offset;
} mb_iostore_span_v1;
typedef struct mb_iostore_view_v1 {
    uint32_t struct_size, abi_version;
    mb_iostore_span_v1 header; /* Offset0, >=144 bytes available; only144 read. */
    uint64_t toc_logical_bytes;
} mb_iostore_view_v1;
typedef struct mb_iostore_limits_v1 {
    uint32_t struct_size, abi_version;
    uint32_t max_chunks, max_blocks, max_hash_seeds, max_overflow_chunks;
    uint32_t max_partitions, max_methods, max_method_name_bytes, reserved;
    uint64_t max_toc_bytes;
} mb_iostore_limits_v1;
typedef struct mb_iostore_range_v1 { uint64_t offset, bytes; } mb_iostore_range_v1;

typedef struct mb_iostore_header_result_v1 {
    uint32_t struct_size, abi_version, status, reason;
    uint32_t header_parsed, table_geometry_valid, version, container_flags_raw;
    uint32_t chunk_count, block_count, block_entry_bytes, compression_block_bytes;
    uint32_t method_name_count, method_name_bytes, partition_count, hash_seed_count;
    uint32_t overflow_chunk_count, metadata_entry_bytes;
    uint64_t container_id, partition_bytes, logical_block_capacity, declared_layout_end;
    uint8_t key_guid_raw[16];
    mb_iostore_range_v1 chunk_ids, chunk_offset_lengths, hash_seeds, overflow_chunks;
    mb_iostore_range_v1 compression_blocks, compression_methods, directory_index, entry_metadata;
    uint32_t full_toc_validated, logical_block_coverage_verified, payload_verified;
    uint32_t authenticity_verified, runtime_supported, installation_supported;
} mb_iostore_header_result_v1;
typedef struct mb_iostore_chunk_result_v1 {
    uint32_t struct_size, abi_version, status, reason;
    uint32_t descriptor_valid, toc_index, chunk_index, chunk_type_raw, padding_raw;
    uint32_t first_block, last_block, block_count;
    uint64_t chunk_id, logical_offset, logical_bytes, logical_end;
    uint8_t chunk_id_raw[12];
    uint32_t logical_block_coverage_verified, paired_ranges_verified, payload_verified;
    uint32_t authenticity_verified, runtime_supported, installation_supported;
} mb_iostore_chunk_result_v1;
typedef struct mb_iostore_block_result_v1 {
    uint32_t struct_size, abi_version, status, reason;
    uint32_t descriptor_valid, toc_index, partition_index, method_index;
    uint32_t stored_bytes, decoded_bytes, built_in_none, paired_extent_checked;
    uint64_t physical_offset, partition_offset, partition_end;
    uint32_t full_toc_validated, logical_block_coverage_verified, payload_verified;
    uint32_t authenticity_verified, runtime_supported, installation_supported;
} mb_iostore_block_result_v1;

void mb_iostore_limits_v1_default(mb_iostore_limits_v1 *limits);

/* Pure v8 unencrypted/unsigned metadata inspection, with fixed bounded work:
 * 144 header bytes, one selected row/pair, <=16 supplied partition lengths.
 * No I/O, allocation, table scan/copy, retained pointers or payload/cook parsing.
 * Count/TOC limits only tighten. UCAS lengths and chunk descriptor lengths do
 * not inherit payload buffer caps. Built-in method0 is None; other in-range
 * method indices are declared metadata, without method-name/decoder proof.
 *
 * Caller asserts valid immutable C backing from one coherent captured TOC and
 * paired length observations for the call. Every input/control must be disjoint
 * from the owning result. ABI/backing/alignment/known-alias preflight leaves all
 * bytes untouched. An admitted failure clears every positive fact and writes
 * only ABI/status/reason. Only OK publishes descriptor/declared-range facts.
 * Reserved v8 bytes stay uninterpreted. Raw flags/ID/GUID do not prove a target
 * build, signature policy or runtime compatibility. Computed table intervals
 * are merely bounded by declared TOC size; directory/meta/seed contents and any
 * trailing bytes are unparsed. Chunk ranges check address capacity only, never
 * all involved blocks or whole-file logical coverage. A selected block extent
 * is checked against caller lengths, without reading or authenticating UCAS.
 * All full-validation/payload/authenticity/runtime/install flags remain0. */
mb_iostore_status mb_iostore_read_header_v1(const mb_iostore_view_v1 *view,
    const mb_iostore_limits_v1 *limits, mb_iostore_header_result_v1 *result);
mb_iostore_status mb_iostore_read_chunk_v1(const mb_iostore_view_v1 *view,
    const mb_iostore_limits_v1 *limits, uint32_t toc_index,
    const mb_iostore_span_v1 *chunk_id, const mb_iostore_span_v1 *offset_length,
    mb_iostore_chunk_result_v1 *result);
mb_iostore_status mb_iostore_read_block_v1(const mb_iostore_view_v1 *view,
    const mb_iostore_limits_v1 *limits, uint32_t toc_index,
    const mb_iostore_span_v1 *block, const uint64_t *paired_partition_lengths,
    uint32_t paired_partition_count, mb_iostore_block_result_v1 *result);

#ifdef __cplusplus
}
#endif
#endif
