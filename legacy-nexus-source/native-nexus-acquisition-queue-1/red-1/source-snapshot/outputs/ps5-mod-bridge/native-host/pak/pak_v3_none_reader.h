#ifndef MB_PAK_V3_NONE_READER_H
#define MB_PAK_V3_NONE_READER_H
#include "pak_reader.h"

#define MB_PAK_V3_ABI_VERSION 1u
#define MB_PAK_V3_FOOTER_BYTES 44u
#define MB_PAK_V3_HEADER_BYTES 53u
#define MB_PAK_V3_MAX_ARCHIVE_BYTES UINT64_C(536870912)
#define MB_PAK_V3_MAX_INDEX_BYTES 4194304u
#define MB_PAK_V3_MAX_FILES 8192u
#define MB_PAK_V3_MAX_NAME_BYTES 4194304u
#define MB_PAK_V3_MAX_PATH_BYTES 1024u
#define MB_PAK_V3_MAX_SELECTED_BYTES 4194304u
#define MB_PAK_V3_TRANSFER_BYTES 65536u
#define MB_PAK_V3_MAX_REQUESTED_BYTES UINT64_C(20971520)
#define MB_PAK_V3_MAX_READ_CALLS 32768u
#define MB_PAK_V3_MAX_COMPARISON_BYTES UINT64_C(134217728)

typedef enum mb_pak_v3_status {
 MB_PAK_V3_OK=0, MB_PAK_V3_BAD_ARGUMENT, MB_PAK_V3_UNSUPPORTED,
 MB_PAK_V3_MALFORMED, MB_PAK_V3_RESOURCE_LIMIT, MB_PAK_V3_BUFFER_TOO_SMALL,
 MB_PAK_V3_READ_ERROR, MB_PAK_V3_SHORT_READ, MB_PAK_V3_SOURCE_CHANGED,
 MB_PAK_V3_CANCELLED, MB_PAK_V3_PROVIDER_CONTRACT, MB_PAK_V3_HASH_ERROR,
 MB_PAK_V3_DIGEST_MISMATCH
} mb_pak_v3_status;
typedef enum mb_pak_v3_stage {
 MB_PAK_V3_PREFLIGHT=0, MB_PAK_V3_FOOTER, MB_PAK_V3_INDEX,
 MB_PAK_V3_ENTRIES, MB_PAK_V3_PHYSICAL_HEADERS, MB_PAK_V3_PAYLOAD,
 MB_PAK_V3_COMPLETE
} mb_pak_v3_stage;
typedef struct mb_pak_v3_limits_v1 {
 uint32_t struct_size,abi_version,max_files,max_index_bytes,max_name_bytes;
 uint32_t max_read_bytes,max_read_calls,max_selected_bytes;
 uint64_t max_archive_bytes,max_requested_bytes,max_comparison_bytes;
} mb_pak_v3_limits_v1;
typedef struct mb_pak_v3_entry_v1 {
 uint32_t path_offset,path_bytes;
 uint64_t archive_offset,data_offset,payload_bytes;
 uint8_t sha1[20];
} mb_pak_v3_entry_v1;
typedef struct mb_pak_v3_buffers_v1 {
 uint32_t struct_size,abi_version;
 uint8_t *index;size_t index_capacity;
 mb_pak_v3_entry_v1 *entries;uint32_t entries_capacity;
 uint8_t *names;size_t names_capacity;
 uint32_t *sort_indices;uint32_t sort_capacity;
 uint8_t *transfer;size_t transfer_bytes;
 uint8_t *selected;size_t selected_capacity;
} mb_pak_v3_buffers_v1;
typedef struct mb_pak_v3_result_v1 {
 uint32_t struct_size,abi_version;
 mb_pak_v3_status status;mb_pak_v3_stage stage;
 uint32_t metadata_complete,selected_complete,file_count,name_bytes;
 uint32_t selected_row,mount_bytes;
 uint8_t mount[MB_PAK_V3_MAX_PATH_BYTES+1];
 uint64_t index_offset,index_bytes,selected_bytes,snapshot_token;
 uint8_t index_sha1[20],selected_sha1[20];
 uint32_t read_calls,last_provider_status,last_requested,last_actual;
 uint64_t requested_bytes,comparison_bytes;
 uint32_t installation_supported; /* Always0. */
} mb_pak_v3_result_v1;

void mb_pak_v3_limits_v1_default(mb_pak_v3_limits_v1 *);
/* Read-only classic v3 None producer profile, separate from v11 dispatch.
 * Exactly44-byte footer; verified index ends at footer; complete flat index;
 * strict UTF8/UTF16 FString decoding; exact paths unique; every53-byte header
 * reconciled and all file extents disjoint/before index. Only method0, flags0,
 * blocksize0 admitted. Physical offset may be0 or its indexed archive offset.
 * Selected output is published only after SHA1 match; other payloads are NOT
 * hashed. Metadata names/mount are engine facts, never install paths/authority.
 *
 * All buffers/results/inputs are valid, disjoint caller-owned spans (including
 * immutable provider backing). Callbacks are synchronous exact read_at,
 * immutable snapshot asserted by caller; no retained pointers, heap, writes to
 * source, filesystem, network or runtime operation. Known argument/alias errors
 * write nothing. After admitted preflight, failure clears FULL mutable buffer
 * capacities, preserves spent counters/status, and publishes no complete data.
 * Success retains complete rows/names/optional selected bytes, erases scratch.
 * want_selected0 requires selected_row0. Source max512MiB; index/name max4MiB;
 * files8192; selectedmax4MiB; callbacks<=64KiB with one bounded read ledger.
 */
mb_pak_v3_status mb_pak_v3_read_none_v1(
 const pak_source_v1 *,const mb_pak_v3_limits_v1 *,
 uint32_t want_selected,uint32_t selected_row,
 const mb_pak_v3_buffers_v1 *,mb_pak_v3_result_v1 *);
#endif
