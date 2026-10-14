#ifndef MB_IOSTORE_NONE_PACKER_H
#define MB_IOSTORE_NONE_PACKER_H
#include <stddef.h>
#include <stdint.h>
#define MB_IOSTORE_NONE_PACKER_ABI_V1 1u
#define MB_IOSTORE_NONE_MAX_CHUNKS 8192u
#define MB_IOSTORE_NONE_MAX_BLOCKS 65536u
#define MB_IOSTORE_NONE_BLOCK_BYTES 65536u
#define MB_IOSTORE_NONE_MAX_PAYLOAD_BYTES UINT64_C(536870912)
#define MB_IOSTORE_NONE_MAX_TOC_BYTES 8388608u
typedef enum mb_iostore_none_status_v1 {
 MB_IOSTORE_NONE_OK=0, MB_IOSTORE_NONE_BAD_ARGUMENT=1,
 MB_IOSTORE_NONE_LIMIT=2, MB_IOSTORE_NONE_INVALID_CHUNK=3,
 MB_IOSTORE_NONE_DUPLICATE_ID=4, MB_IOSTORE_NONE_BUFFER_TOO_SMALL=5,
 MB_IOSTORE_NONE_READ_FAILED=6, MB_IOSTORE_NONE_WRITE_FAILED=7
} mb_iostore_none_status_v1;
typedef enum mb_iostore_none_stage_v1 {
 MB_IOSTORE_NONE_PREFLIGHT=0, MB_IOSTORE_NONE_TOC=1,
 MB_IOSTORE_NONE_PAYLOAD=2, MB_IOSTORE_NONE_PADDING=3,
 MB_IOSTORE_NONE_COMPLETE=4
} mb_iostore_none_stage_v1;
typedef struct mb_iostore_none_chunk_v1 {
 uint8_t raw_id[12];
 uint64_t bytes;
} mb_iostore_none_chunk_v1;
typedef int (*mb_iostore_none_read_fn)(void *,uint32_t row,uint64_t offset,
 uint8_t *,uint32_t requested,uint32_t *actual);
typedef int (*mb_iostore_none_write_fn)(void *,uint64_t offset,
 const uint8_t *,uint32_t requested,uint32_t *actual);
typedef struct mb_iostore_none_params_v1 {
 uint32_t struct_size,abi_version;
 const mb_iostore_none_chunk_v1 *chunks;
 uint32_t chunk_count,reserved;
 uint8_t container_id[8];
 const uint8_t *directory_index;
 uint32_t directory_bytes;
 void *io_context;
 mb_iostore_none_read_fn read_chunk;
 mb_iostore_none_write_fn write_ucas;
} mb_iostore_none_params_v1;
typedef struct mb_iostore_none_workspace_v1 {
 uint32_t struct_size,abi_version;
 uint8_t *toc;
 size_t toc_capacity;
 uint8_t *transfer;
 size_t transfer_bytes;
 uint32_t *sort_rows;
 size_t sort_capacity;
} mb_iostore_none_workspace_v1;
typedef struct mb_iostore_none_result_v1 {
 uint32_t struct_size,abi_version,status,stage,complete;
 uint32_t chunk_count,block_count,failed_row,failed_block;
 uint64_t toc_bytes,content_bytes,ucas_bytes,padding_bytes;
 uint64_t requested_read_bytes,requested_write_bytes,failed_offset;
 uint32_t read_calls,write_calls,callback_actual;
 int32_t callback_status;
} mb_iostore_none_result_v1;
/* Pure ordinary-file byte preparation, no heap/file/name/process/network calls.
 * Inputs and backing remain valid/immutable and mutually disjoint for the call.
 * Workspace/result and all writable buffers are disjoint from one another and
 * inputs. Caller supplies exactly64KiB transfer storage and >=chunk_count sort
 * rows (capacity<=8192); TOC cap<=8MiB. All input IDs/geometry/caps are validated
 * before TOC bytes or external callbacks; duplicate detection uses heap sort of
 * workspace row indices O(n log n) without changing input/output row order.
 * Invalid known span/alignment/alias/cap preflight leaves ALL backing untouched;
 * inspect return first. Later preflight refusal initializes result and may alter
 * sort scratch, but leaves TOC/transfer/UCAS untouched. Short TOC reports sizes.
 * Directory bytes are opaque caller-supplied serialized metadata valid for the
 * exact supplied row order; this module neither parses nor authorizes it.
 * Output is v8 Indexed/None, singleUINT64_MAX partition, 64KiB logical blocks,
 * one negative overflow seed and every row in overflow order. Hashes use official
 * portable BLAKE3 first20 bytes. Memorymapped type4/zero chunks/lengths refuse.
 * Physical chunks concatenate without interchunk padding; final zero bytes cover
 * max(block_offset+align16(stored_bytes)), at most15 bytes beyond content cap.
 * Exact callbacks return0 with actual==requested and do not retain backing or
 * mutate descriptors/profile. Writes assume fresh unpublished output at offset0.
 * Operational failure can leave partial TOC/UCAS; complete stays0 and caller must
 * discard/retain unpublished. Complete proves bytes prepared, not durability,
 * hash authentication, installation, container mounting or runtime acceptance. */
mb_iostore_none_status_v1 mb_iostore_none_pack_v1(
 const mb_iostore_none_params_v1 *,mb_iostore_none_workspace_v1 *,
 mb_iostore_none_result_v1 *);
#endif
