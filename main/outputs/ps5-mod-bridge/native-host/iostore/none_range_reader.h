#ifndef MB_IOSTORE_NONE_RANGE_READER_H
#define MB_IOSTORE_NONE_RANGE_READER_H
#include <stddef.h>
#include <stdint.h>
#include "toc_reader.h"

#define MB_NONE_RANGE_ABI_VERSION 1u
#define MB_NONE_RANGE_MAX_BYTES 65536u
#define MB_NONE_RANGE_MAX_CHUNK_BYTES (UINT64_C(512)<<20)
#define MB_NONE_RANGE_MAX_CHUNK_BLOCKS 65536u

typedef enum mb_none_range_status_v1 {
 MB_NONE_RANGE_OK=0, MB_NONE_RANGE_BAD_ARGUMENT, MB_NONE_RANGE_UNSUPPORTED,
 MB_NONE_RANGE_METADATA_REJECTED, MB_NONE_RANGE_RESOURCE_LIMIT,
 MB_NONE_RANGE_OUT_OF_RANGE, MB_NONE_RANGE_BUFFER_TOO_SMALL,
 MB_NONE_RANGE_PLAN_MISMATCH, MB_NONE_RANGE_READ_ERROR, MB_NONE_RANGE_SHORT_READ
} mb_none_range_status_v1;
/* Provider may write at most requested bytes. Return0 and exact received for
 * success; short reads/errors are refused without retries. Ordinary partition
 * file offsets only. Provider/context are trusted caller backing. */
typedef int (*mb_none_range_read_fn_v1)(void *context,uint32_t partition,
 uint64_t offset,uint8_t *destination,size_t requested,size_t *received);
typedef struct mb_none_range_source_v1 {
 uint32_t struct_size,abi_version;
 mb_iostore_span_v1 toc; /* Full immutable TOC; offset0. */
 const uint64_t *partition_lengths;
 uint32_t partition_count,reserved;
 mb_none_range_read_fn_v1 read;
 void *read_context;
} mb_none_range_source_v1;
/* Successful planning publishes this owning descriptor. No magic validation
 * cookie: callers must use the actual successful plan output and keep it
 * immutable. Plan is not authentication. Borrowed TOC must remain immutable;
 * borrowed provider/context must remain valid/coherent for every use (provider
 * may update its own cursor/counters). Lengths are copied.
 * Range calls rebind the captured geometry to the TOC, but cannot authenticate
 * a fabricated caller object or detect concurrent input mutation. */
typedef struct mb_none_range_plan_v1 {
 uint32_t struct_size,abi_version;
 mb_iostore_span_v1 toc;
 uint64_t partition_lengths[2];
 uint32_t partition_count,toc_row;
 mb_none_range_read_fn_v1 read;
 void *read_context;
 uint64_t logical_offset,logical_bytes,blocks_offset,partition_bytes;
 uint32_t first_block,last_block,block_count,total_blocks;
 uint8_t chunk_id[12];
 uint32_t reserved;
} mb_none_range_plan_v1;
typedef struct mb_none_range_result_v1 {
 uint32_t struct_size,abi_version,status,inner_status,inner_reason,toc_row;
 uint32_t full_chunk_coverage_scanned,selected_blocks_checked,plan_geometry_rebound;
 uint32_t read_calls,range_complete,reserved;
 uint64_t chunk_bytes,range_offset,range_bytes,read_bytes;
 uint32_t failed_partition,failed_block;
 uint64_t failed_physical_offset,failed_received;
 int32_t provider_code;
 uint32_t full_toc_validated,payload_hash_verified,authenticity_verified;
 uint32_t runtime_supported,installation_supported;
} mb_none_range_result_v1;

/* Exact v8 Indexed-only/None/unsigned/unencrypted profile; 65536B blocks,
 * one UINT64_MAX partition or two finite partitions, nonempty selected chunk,
 * non-memory-mapped, zero selected meta flags/padding and strict header fields.
 * Planning scans/checks ALL selected chunk block coverage and physical bounds
 * once, <=65536 blocks /512MiB. It invokes no read callback. It publishes plan
 * only on success (plan unchanged on any failure). No full-TOC scan or hashes.
 * Known invalid ABI/backing/aliases leave all backing unchanged; admitted
 * failures may write result. No heap, filesystem, network or runtime actions. */
mb_none_range_status_v1 mb_iostore_none_plan_v1(
 const mb_none_range_source_v1 *source,uint32_t toc_row,
 mb_none_range_plan_v1 *plan,mb_none_range_result_v1 *result);

/* O(1) header/chunk rebind plus only 0..2 requested block checks (not full
 * coverage again). offset<=chunkBytes; requested0..65536 and must fit chunk.
 * Full selected coverage is a fact of the earlier successful immutable plan,
 * not a repeated range-call claim: full_chunk_coverage_scanned remains0 here.
 * Both participating blocks validate before any callback. Zero range at EOF
 * is complete with no read and may use NULL output/capacity0.
 * Output/capacity<=65536. Known invalid ABI/backing/aliases leave every byte
 * unchanged; admitted metadata/range/capacity failures do not touch output.
 * Once a callback starts, error/short read zeroes the ENTIRE requested range
 * only (including any not-yet-written bytes); output tail stays unchanged.
 * Partial reads are never exposed as complete. Result never grants hash,
 * authenticity, runtime or installation authority. */
mb_none_range_status_v1 mb_iostore_none_range_v1(
 const mb_none_range_plan_v1 *plan,uint64_t chunk_offset,uint32_t requested,
 uint8_t *output,size_t output_capacity,mb_none_range_result_v1 *result);
#endif
