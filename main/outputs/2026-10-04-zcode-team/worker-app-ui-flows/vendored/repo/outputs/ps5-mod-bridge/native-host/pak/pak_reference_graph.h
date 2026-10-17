#ifndef NATIVE_PAK_REFERENCE_GRAPH_EXPERIMENT_H
#define NATIVE_PAK_REFERENCE_GRAPH_EXPERIMENT_H

#include "pak_entry_decoder.h"
#include "pak_directory_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAK_REFERENCE_GRAPH_ABI_VERSION 1u
#define PAK_GRAPH_MAX_RECORDS 65536u
#define PAK_GRAPH_MAX_DIRECTORIES 65536u
#define PAK_GRAPH_MAX_BLOB_BYTES (4u * 1024u * 1024u)
#define PAK_GRAPH_MAX_NAME_BYTES (4u * 1024u * 1024u)
#define PAK_GRAPH_MAX_STEPS 4194304u
#define PAK_GRAPH_MAX_NAME_COMPARISON_BYTES 67108864u
#define PAK_GRAPH_MAX_SEEN_BYTES ((PAK_GRAPH_MAX_RECORDS + 7u) / 8u)

typedef enum pak_graph_status {
    PAK_GRAPH_OK = 0,
    PAK_GRAPH_UNSUPPORTED,
    PAK_GRAPH_MALFORMED,
    PAK_GRAPH_RESOURCE_LIMIT,
    PAK_GRAPH_BAD_ARGUMENT,
    PAK_GRAPH_CANCELLED,
    PAK_GRAPH_INTERNAL_ERROR
} pak_graph_status;

typedef enum pak_graph_reason {
    PAK_GRAPH_REASON_NONE = 0,
    PAK_GRAPH_REASON_OUTPUT_CAPACITY,
    PAK_GRAPH_REASON_STEPS,
    PAK_GRAPH_REASON_NAME_COMPARISON_BYTES,
    PAK_GRAPH_REASON_CANCELLED,
    PAK_GRAPH_REASON_ENTRY_DECODER,
    PAK_GRAPH_REASON_CODEC_UNSUPPORTED,
    PAK_GRAPH_REASON_NO_PROGRESS,
    PAK_GRAPH_REASON_COUNT_MISMATCH,
    PAK_GRAPH_REASON_DIRECTORY_VIEW,
    PAK_GRAPH_REASON_NAME_VIEW,
    PAK_GRAPH_REASON_REFERENCE_UNSUPPORTED,
    PAK_GRAPH_REASON_REFERENCE_NOT_START,
    PAK_GRAPH_REASON_REFERENCE_ALIAS,
    PAK_GRAPH_REASON_ORPHAN,
    PAK_GRAPH_REASON_DUPLICATE_TUPLE,
    PAK_GRAPH_REASON_SELECTED_BINDING
} pak_graph_reason;

typedef struct pak_graph_input_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t primary_file_count;
    uint32_t directory_count;
    uint32_t file_count;
    uint32_t names_bytes; /* Already parsed UTF-8 arena, including NULs. */
    uint64_t logical_archive_size;
    const uint8_t *encoded_blob;
    size_t encoded_blob_bytes;
    const pak_entry_method_table_v1 *methods;
    const pak_entry_limits_v1 *entry_limits;
    const pak_directory_record_v1 *directories;
    const pak_directory_entry_v1 *files;
    const uint8_t *names;
    uint32_t want_selected; /* Exactly 0 or 1. */
    uint32_t selected_file_index; /* Zero when want_selected=0. */
    uint32_t reserved[2];
} pak_graph_input_v1;

typedef struct pak_graph_limits_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t max_records;
    uint32_t max_directories;
    uint32_t max_name_bytes;
    uint32_t max_steps;
    uint64_t max_name_comparison_bytes; /* Each compared byte of each side. */
    uint32_t cancel_batch_steps; /* 1..256 graph steps, default 64. */
    uint32_t reserved;
} pak_graph_limits_v1;

typedef struct pak_graph_extent_v1 {
    uint64_t archive_start;
    uint64_t archive_end; /* Half-open indexed header + stored-data span. */
    uint32_t encoded_offset;
    uint32_t encoded_bytes;
    uint32_t directory_index;
    uint32_t file_index;
} pak_graph_extent_v1;

typedef struct pak_graph_workspace_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t *starts; /* Ascending true encoded starts, indexed by record. */
    size_t starts_capacity;
    pak_graph_extent_v1 *extents; /* Same record index as starts. */
    size_t extents_capacity;
    uint32_t *sort_indices; /* On success: record indices ordered by extent. */
    size_t sort_capacity;
    uint8_t *seen;
    size_t seen_bytes;
    pak_entry_block_v1 *decode_blocks; /* Private child-decoder scratch. */
    size_t decode_blocks_capacity;
    pak_entry_block_v1 *selected_blocks; /* Valid only on whole OK/selected. */
    size_t selected_blocks_capacity;
} pak_graph_workspace_v1;

typedef struct pak_graph_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    pak_graph_status status;
    pak_graph_reason reason;
    uint64_t steps_used;
    uint64_t name_comparison_bytes;
    pak_entry_status decoder_status;
    pak_entry_reason decoder_reason;
    uint32_t metadata_complete;
    uint32_t directory_count;
    uint32_t file_count;
    uint32_t record_count;
    uint32_t selected_complete;
    uint32_t selected_file_index;
    uint32_t selected_record_index;
    uint32_t selected_directory_index;
    uint32_t reserved[2];
    pak_entry_result_v1 selected_entry; /* Owning scalar metadata copy. */
} pak_graph_result_v1;

typedef int (*pak_graph_cancel_fn)(void *context); /* Nonzero means cancel. */

void pak_graph_limits_v1_default(pak_graph_limits_v1 *limits);

/*
 * Pure metadata operation: no read_at, ledger, payload/header reads or heap.
 * Input pointers/views are immutable, already parsed and strict UTF-8; C
 * object/backing validity is caller-owned. Known writable spans must be
 * disjoint from all known inputs and each other. Preflight errors (including
 * known aliases) return BAD_ARGUMENT without modifying ANY output/result.
 * After valid preflight, operational failure clears full mutable capacities
 * and accepted result counts/selected metadata, retaining only cause/work
 * counters. Whole OK establishes real starts, one-to-one references/counts
 * and exact raw UTF-8 tuple uniqueness, not physical/payload verification.
 * decode_blocks is cleared on success. Other tables/selected blocks are
 * private until whole OK; no pointer is retained after return. Cancellation
 * is cooperative; the existing bounded decoder call cannot be interrupted.
 */
pak_graph_status pak_validate_v11_reference_graph(
    const pak_graph_input_v1 *input,
    const pak_graph_limits_v1 *limits,
    const pak_graph_workspace_v1 *workspace,
    pak_graph_cancel_fn cancelled,
    void *cancel_context,
    pak_graph_result_v1 *result);

#ifdef __cplusplus
}
#endif

#endif
