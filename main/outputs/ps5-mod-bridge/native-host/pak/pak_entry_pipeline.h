#ifndef NATIVE_PAK_ENTRY_PIPELINE_EXPERIMENT_H
#define NATIVE_PAK_ENTRY_PIPELINE_EXPERIMENT_H

#include <stddef.h>
#include <stdint.h>

#include "pak_entry_decoder.h"
#include "pak_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAK_ENTRY_PIPELINE_ABI_VERSION 1u

typedef enum pak_entry_pipeline_status {
    PAK_ENTRY_PIPELINE_OK = 0,
    PAK_ENTRY_PIPELINE_BAD_ARGUMENT = 1,
    PAK_ENTRY_PIPELINE_READER_REJECTED = 2,
    PAK_ENTRY_PIPELINE_CAPTURE_REJECTED = 3,
    PAK_ENTRY_PIPELINE_UNSUPPORTED = 4,
    PAK_ENTRY_PIPELINE_ENTRY_REJECTED = 5
} pak_entry_pipeline_status;

typedef enum pak_entry_pipeline_reason {
    PAK_ENTRY_PIPELINE_REASON_NONE = 0,
    PAK_ENTRY_PIPELINE_REASON_BAD_ARGUMENT,
    PAK_ENTRY_PIPELINE_REASON_CAPTURE_ORDER,
    PAK_ENTRY_PIPELINE_REASON_CAPTURE_INCOMPLETE,
    PAK_ENTRY_PIPELINE_REASON_PRIMARY_LAYOUT,
    PAK_ENTRY_PIPELINE_REASON_METHOD_TABLE
} pak_entry_pipeline_reason;

typedef struct pak_entry_pipeline_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    pak_entry_pipeline_status status;
    pak_entry_pipeline_reason reason;
    pak_status reader_status;
    pak_reason reader_reason;
    pak_entry_status entry_status;
    pak_entry_reason entry_reason;
    uint32_t primary_validated;
    uint32_t primary_capture_complete;
    uint32_t entry_decode_attempted;
    uint32_t entry_complete;
    uint32_t primary_sha1_checked;
    uint32_t primary_sha1_matches;
    uint32_t primary_metadata_complete;
    uint32_t encoded_span_offset;
    uint32_t encoded_span_bytes;
    uint64_t selected_record_offset;
    uint64_t aggregate_read_bytes;
    uint32_t aggregate_read_calls;
    uint32_t reserved;
    pak_primary_result_v1 primary;
    pak_entry_result_v1 entry;
} pak_entry_pipeline_result_v1;

/*
 * Validate the ordinary v11 footer and complete primary index through the
 * existing reader, teeing those same immutable read_at bytes into caller
 * scratch. The reader's primary SHA-1 therefore covers the bytes later used
 * to locate and decode one caller-selected encoded record; no span reread is
 * performed. The caller must establish immutable snapshot semantics, and all
 * input/output buffers must be disjoint. No pointers are retained and no heap
 * allocation is performed by this operation.
 *
 * On reader, capture, or primary-layout failure the entry decoder is not
 * called and entry_complete remains false. Any primary bytes tentatively
 * copied by this call are cleared if verification fails. Callers must ignore
 * blocks_out unless overall status is OK and entry_complete is true. This
 * pipeline returns metadata only; it never reads payload blocks or writes
 * files.
 */
pak_entry_pipeline_status pak_read_v11_one_entry(
    const pak_source_v1 *source,
    const pak_limits_v1 *reader_limits,
    uint8_t *reader_workspace,
    size_t reader_workspace_bytes,
    uint8_t *primary_scratch,
    size_t primary_scratch_bytes,
    size_t selected_record_offset,
    const pak_entry_limits_v1 *entry_limits,
    pak_entry_block_v1 *blocks_out,
    size_t blocks_capacity,
    pak_entry_pipeline_result_v1 *result);

#ifdef __cplusplus
}
#endif

#endif
