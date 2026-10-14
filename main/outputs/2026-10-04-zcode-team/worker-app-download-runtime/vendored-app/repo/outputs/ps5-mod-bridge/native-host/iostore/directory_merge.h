#ifndef MB_IOSTORE_DIRECTORY_MERGE_H
#define MB_IOSTORE_DIRECTORY_MERGE_H
#include <stddef.h>
#include <stdint.h>

#define MB_DIRECTORY_MERGE_ABI_VERSION 1u
#define MB_DIRECTORY_MERGE_MAX_INDEX_BYTES (8u*1024u*1024u)
#define MB_DIRECTORY_MERGE_MAX_PATH_BYTES (16u*1024u)
#define MB_DIRECTORY_MERGE_MAX_PATH_ARENA_BYTES (32u*1024u*1024u)
#define MB_DIRECTORY_MERGE_MAX_WORKSPACE_BYTES (128u*1024u*1024u)
#define MB_DIRECTORY_MERGE_MAX_ROWS 200000u
#define MB_DIRECTORY_MERGE_MAX_ID_ROWS 2000000u

typedef enum mb_directory_merge_status_v1 {
 MB_DIRECTORY_MERGE_OK=0, MB_DIRECTORY_MERGE_BAD_ARGUMENT,
 MB_DIRECTORY_MERGE_INVALID, MB_DIRECTORY_MERGE_RESOURCE_LIMIT,
 MB_DIRECTORY_MERGE_WORKSPACE_TOO_SMALL, MB_DIRECTORY_MERGE_OUTPUT_TOO_SMALL
} mb_directory_merge_status_v1;
typedef enum mb_directory_merge_reason_v1 {
 MB_DIRECTORY_MERGE_REASON_NONE=0, MB_DIRECTORY_MERGE_REASON_CAPTURE,
 MB_DIRECTORY_MERGE_REASON_STRING, MB_DIRECTORY_MERGE_REASON_COUNTS,
 MB_DIRECTORY_MERGE_REASON_GRAPH, MB_DIRECTORY_MERGE_REASON_PATH,
 MB_DIRECTORY_MERGE_REASON_MOUNT_ROOT, MB_DIRECTORY_MERGE_REASON_ID,
 MB_DIRECTORY_MERGE_REASON_WORKSPACE, MB_DIRECTORY_MERGE_REASON_OUTPUT
} mb_directory_merge_reason_v1;
typedef struct mb_directory_merge_bytes_v1 {const uint8_t *data;size_t bytes;} mb_directory_merge_bytes_v1;
typedef struct mb_directory_merge_ids_v1 {const uint8_t *data;uint32_t count;} mb_directory_merge_ids_v1;
typedef struct mb_directory_merge_params_v1 {
 uint32_t struct_size,abi_version;
 mb_directory_merge_bytes_v1 base_directory,mod_directory;
 mb_directory_merge_ids_v1 base_ids,mod_ids,final_ids;
} mb_directory_merge_params_v1;
typedef struct mb_directory_merge_workspace_v1 {
 uint32_t struct_size,abi_version;
 uint8_t *storage;size_t storage_bytes; /* caller storage aligned for uint64_t */
 uint8_t *paths;size_t path_bytes; /* separate canonical component-tuple arena */
} mb_directory_merge_workspace_v1;
typedef struct mb_directory_merge_result_v1 {
 uint32_t struct_size,abi_version,status,reason,source_index,reserved;
 uint64_t required_output_bytes,output_bytes,workspace_used,path_arena_used;
 uint64_t required_workspace_bytes,required_path_bytes;
 uint32_t base_directories,base_files,base_strings,mod_directories,mod_files,mod_strings;
 uint32_t merged_directories,merged_files,merged_strings,exact_path_overrides;
 uint32_t metadata_complete,userdata_remapped_by_id;
 uint32_t payload_verified,authenticity_verified,runtime_supported,installation_supported;
} mb_directory_merge_result_v1;

/* Pure v8 plain directory-index merge. IDs are contiguous raw 12-byte rows;
 * final_ids is the complete desired ordered TOC table, with unique IDs.
 * No heap, callbacks, filesystem path mapping or I/O. UTF8/UTF16 FStrings
 * are validated and normalized to UTF8; output order/encoding matches the
 * retained Python merge. Exact path collisions choose mod; aliases and empty
 * directories survive. Mount/root semantics must match exactly.
 *
 * Known invalid controls/backing/aliases leave every byte untouched. Inputs
 * may overlap each other, but controls, workspaces, output and result must be
 * disjoint from each other and inputs. Other admitted failures may update
 * workspace/result but never output. Null output/capacity0 validates and
 * measures, returning OUTPUT_TOO_SMALL with completion0. Insufficient arenas
 * report the minimum bytes needed at the failed allocation, not a complete
 * workspace sizing oracle. Workspaces are borrowed and reset on each call.
 * Index/output <=8MiB; strings/components <=4096B; each source decoded strings
 * <=8MiB and virtual file paths <=32MiB; each virtual path <=16KiB. Combined
 * stored directory component tuples use a <=32MiB arena (extra storage cap).
 * Arrays/counts <=200000 per source/final output, IDs <=2000000.
 * No payload/authenticity/runtime/installation authority is returned.
 */
mb_directory_merge_status_v1 mb_iostore_directory_merge_v1(
 const mb_directory_merge_params_v1 *params,
 mb_directory_merge_workspace_v1 *workspace,
 uint8_t *output,size_t output_capacity,mb_directory_merge_result_v1 *result);
#endif
