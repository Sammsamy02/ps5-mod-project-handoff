#ifndef MB_IOSTORE_TOC_OVERLAY_H
#define MB_IOSTORE_TOC_OVERLAY_H
#include <stddef.h>
#include <stdint.h>
#include "toc_reader.h"

#define MB_TOC_OVERLAY_ABI_VERSION 1u
#define MB_TOC_OVERLAY_MAX_DIRECTORY_BYTES (UINT64_C(32)<<20)
#define MB_TOC_OVERLAY_MAX_SIDECAR_BYTES (UINT64_C(512)<<20)

typedef enum mb_toc_overlay_status_v1 {
 MB_TOC_OVERLAY_OK=0, MB_TOC_OVERLAY_BAD_ARGUMENT,
 MB_TOC_OVERLAY_UNSUPPORTED, MB_TOC_OVERLAY_METADATA_REJECTED,
 MB_TOC_OVERLAY_RESOURCE_LIMIT, MB_TOC_OVERLAY_WORKSPACE_TOO_SMALL,
 MB_TOC_OVERLAY_OUTPUT_TOO_SMALL
} mb_toc_overlay_status_v1;
typedef enum mb_toc_overlay_reason_v1 {
 MB_TOC_OVERLAY_REASON_NONE=0, MB_TOC_OVERLAY_REASON_HEADER,
 MB_TOC_OVERLAY_REASON_POLICY, MB_TOC_OVERLAY_REASON_ID,
 MB_TOC_OVERLAY_REASON_OVERFLOW, MB_TOC_OVERLAY_REASON_META,
 MB_TOC_OVERLAY_REASON_BLOCK, MB_TOC_OVERLAY_REASON_CHUNK,
 MB_TOC_OVERLAY_REASON_HEADER_CHUNK, MB_TOC_OVERLAY_REASON_PARTITION,
 MB_TOC_OVERLAY_REASON_OUTPUT
} mb_toc_overlay_reason_v1;

typedef struct mb_toc_overlay_params_v1 {
 uint32_t struct_size,abi_version;
 mb_iostore_span_v1 base_toc,small_toc,merged_directory;
 uint64_t base_ucas_bytes,small_ucas_bytes;
} mb_toc_overlay_params_v1;
typedef struct mb_toc_overlay_id_row_v1 {
 uint8_t id[12]; uint32_t row;
} mb_toc_overlay_id_row_v1;
typedef struct mb_toc_overlay_workspace_v1 {
 uint32_t struct_size,abi_version;
 mb_toc_overlay_id_row_v1 *base_rows,*small_rows;
 uint32_t base_capacity,small_capacity;
 uint32_t *scratch; size_t scratch_capacity;
} mb_toc_overlay_workspace_v1;
typedef struct mb_toc_overlay_result_v1 {
 uint32_t struct_size,abi_version,status,reason;
 uint32_t source_index,inner_status,inner_reason,reserved;
 uint64_t required_output_bytes,output_bytes,required_scratch_u32;
 uint32_t required_base_rows,required_small_rows;
 uint32_t original_chunks,small_chunks,final_chunks,replaced_chunks,added_chunks,unaffected_chunks;
 uint32_t original_blocks,small_blocks,final_blocks,partition_count;
 uint64_t container_id,partition_bytes,base_physical_end,base_aligned_end;
 uint64_t small_physical_end,small_aligned_end;
 uint32_t metadata_complete,original_blocks_preserved,unaffected_mappings_preserved;
 uint32_t payload_hash_verified,authenticity_verified,runtime_supported,installation_supported;
} mb_toc_overlay_result_v1;

/* Pure counterpart of the retained Python build_overlay for v8 Indexed-only,
 * unsigned/unencrypted, None, zero-reserved, one UINT64_MAX partition inputs.
 * The small TOC contains the merged base ContainerHeader under the base ID,
 * plus desired non-memory-mapped replacements/additions. Lengths are observed
 * immutable ordinary file lengths. small_ucas_bytes is ALREADY padded through
 * every offset+align16(stored) read extent, with at most 15 trailing bytes.
 * This function sees no payload: padding zero/content hashes/cooking are the
 * caller's responsibility. Directory bytes are opaque caller-merged metadata.
 *
 * All inputs, controls, workspace buffers, output and result must be valid
 * disjoint backing except that immutable input spans may overlap each other.
 * Known bad arguments/aliases leave result/workspace/output untouched.
 * Admitted refusals may change result/workspace, never output. A null output
 * with capacity 0 fully validates using workspace and returns OUTPUT_TOO_SMALL
 * with exact required_output_bytes; metadata_complete remains 0. Insufficient
 * workspace reports exact required row/scratch capacities after header checks.
 * Success writes exactly output_bytes and leaves output tail untouched.
 * No heap, I/O, callbacks, retained pointers or installation authority.
 * Workspace rows end sorted lexicographically by raw 12-byte ID and retain
 * their input row indices, useful to callers assembling a final ID map.
 */
mb_toc_overlay_status_v1 mb_iostore_toc_overlay_v1(
 const mb_toc_overlay_params_v1 *params,
 mb_toc_overlay_workspace_v1 *workspace,
 uint8_t *output,size_t output_capacity,mb_toc_overlay_result_v1 *result);
#endif
