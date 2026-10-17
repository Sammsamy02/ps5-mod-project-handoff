#ifndef PAK_CANDIDATE_H
#define PAK_CANDIDATE_H

#include "pak_candidate_io.h"
#include "pak_candidate_reconcile.h"
#include "pak_primary_capture.h"
#include "pak_reference_graph.h"
#include "pak_reconciled_payload.h"

#define PAK_CANDIDATE_ABI_VERSION 1u
#define PAK_CANDIDATE_PROFILE_CLASSIC_V11_UNENCRYPTED_STORED_SHA1_V1 1u

typedef enum pak_candidate_stage {
    PAK_CANDIDATE_STAGE_PREFLIGHT = 0,
    PAK_CANDIDATE_STAGE_PRIMARY,
    PAK_CANDIDATE_STAGE_PATH_SECONDARY,
    PAK_CANDIDATE_STAGE_DIRECTORY_SECONDARY,
    PAK_CANDIDATE_STAGE_DIRECTORY_PARSE,
    PAK_CANDIDATE_STAGE_REFERENCE_GRAPH,
    PAK_CANDIDATE_STAGE_SELECTED_EXTENT,
    PAK_CANDIDATE_STAGE_PHYSICAL_READ,
    PAK_CANDIDATE_STAGE_PHYSICAL_PARSE,
    PAK_CANDIDATE_STAGE_RECONCILE,
    PAK_CANDIDATE_STAGE_PAYLOAD,
    PAK_CANDIDATE_STAGE_COMPLETE
} pak_candidate_stage;

typedef struct pak_candidate_limits_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t profile;
    uint32_t max_read_bytes;
    uint32_t max_read_calls;
    uint32_t max_records;
    uint32_t max_directories;
    uint32_t max_name_bytes;
    uint32_t max_graph_steps;
    uint32_t cancel_batch_steps;
    uint64_t max_requested_read_bytes;
    uint64_t max_name_comparison_bytes;
} pak_candidate_limits_v1;

/* Typed private state avoids placing the child results on the console stack.
 * The coordinator erases this entire object on EVERY operational return.
 * Initialize no members: only the disjoint codec budget is caller-initialized. */
typedef struct pak_candidate_state_v1 {
    pak_read_budget_v1 ledger;
    pak_candidate_charging_context charging;
    pak_candidate_tee_context tee;
    pak_source_v1 charged_source;
    pak_source_v1 tee_source;
    pak_limits_v1 reader_limits;
    pak_secondary_limits_v1 secondary_limits;
    pak_directory_limits_v1 directory_limits;
    pak_entry_limits_v1 entry_limits;
    pak_graph_limits_v1 graph_limits;
    pak_physical_limits_v1 physical_limits;
    pak_reconciled_payload_limits_v1 payload_limits;
    pak_primary_capture_result_v1 primary;
    pak_secondary_result_v1 secondary;
    pak_directory_result_v1 directory;
    pak_graph_input_v1 graph_input;
    pak_graph_result_v1 graph;
    pak_physical_result_v1 physical;
    pak_reconciled_payload_result_v1 payload;
} pak_candidate_state_v1;

typedef struct pak_candidate_buffers_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    pak_candidate_state_v1 *state;
    uint8_t *reader_workspace; size_t reader_workspace_bytes; /* Exactly64KiB. */
    uint8_t *primary_capture; size_t primary_capacity; /* 1..4MiB. */
    uint8_t *secondary_workspace; size_t secondary_workspace_bytes; /* Exactly4KiB. */
    uint8_t *directory_capture; size_t directory_capture_capacity; /* 1..4MiB. */
    pak_directory_record_v1 *directories; size_t directories_capacity; /* 1..65536. */
    pak_directory_entry_v1 *files; size_t files_capacity; /* 1..65536. */
    uint8_t *names; size_t names_capacity; /* 1..4MiB. */
    pak_graph_workspace_v1 graph;
    uint8_t *physical_header; size_t physical_header_capacity; /* 53..1081. */
    pak_physical_block_v1 *physical_blocks; size_t physical_blocks_capacity; /* 1..64. */
    pak_reconciled_payload_buffers_v1 payload;
} pak_candidate_buffers_v1;

typedef struct pak_candidate_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    pak_status status;
    pak_candidate_stage stage;
    uint32_t child_status;
    uint32_t child_reason;
    uint32_t profile;
    uint32_t metadata_complete;
    uint32_t selected_payload_complete;
    uint32_t directory_count;
    uint32_t file_count;
    uint32_t name_bytes;
    uint32_t selected_file_index;
    uint32_t selected_directory_index;
    uint32_t read_calls;
    uint32_t provider_observed;
    uint32_t last_provider_status;
    uint32_t last_requested;
    uint32_t last_actual;
    uint32_t io_cause;
    uint64_t requested_read_bytes;
    uint64_t snapshot_token;
    uint64_t graph_steps;
    uint64_t name_comparison_bytes;
    uint64_t decoded_bytes;
    uint32_t path_hash_semantics_verified; /* Always0. */
    uint32_t installation_supported; /* Always0. */
    uint32_t authenticity_verified; /* Always0. */
    uint32_t game_acceptance_verified; /* Always0. */
} pak_candidate_result_v1;

#ifdef __cplusplus
extern "C" {
#endif

void pak_candidate_limits_v1_default(pak_candidate_limits_v1 *limits);

/* One synchronous, read-only archive operation. want_selected is0/1; selected
 * index must be0 when absent. An immutable source/snapshot and exclusive valid
 * C backing storage are caller assertions. All known input/output spans and
 * codec budget must be disjoint; invalid preflight leaves every span/result
 * untouched. No heap allocation, source mutation or retained pointer.
 *
 * Validate captured primary, both raw secondary SHA1s, complete strict UTF-8
 * directory and one-to-one compact reference graph. If requested, reconcile
 * ONE selected physical header and verify stored SHA1/exact None or Zlib
 * payload. This is an explicit unencrypted classic-v11 producer profile;
 * other payloads and path-hash semantics are not validated.
 *
 * Root initializes ONE read ledger once, reserves full requests before provider
 * callbacks, never refunds and never deducts child counters. Caller supplies
 * ONE unreplenished codec budget. Limits/cancellation are cooperative.
 * Whole OK commits complete directory/file/name rows and optional sink only.
 * Every operational failure clears FULL sink/row/name/scratch capacities;
 * child accepted prefixes stay private, but consumed budget/constant causes
 * remain. Success clears all scratch/state and an unrequested sink. Directory
 * names are engine metadata, without filesystem/install authorization.
 */
pak_status pak_validate_v11_candidate(
    const pak_source_v1 *source, const pak_candidate_limits_v1 *limits,
    uint32_t want_selected, uint32_t selected_file_index,
    const pak_candidate_buffers_v1 *buffers, bz_budget *codec_budget,
    pak_graph_cancel_fn cancelled, void *cancel_context,
    pak_candidate_result_v1 *result);

#ifdef __cplusplus
}
#endif

#endif
