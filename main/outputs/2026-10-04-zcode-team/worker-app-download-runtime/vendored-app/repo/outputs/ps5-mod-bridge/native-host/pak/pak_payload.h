#ifndef NATIVE_PAK_PAYLOAD_PIPELINE_EXPERIMENT_H
#define NATIVE_PAK_PAYLOAD_PIPELINE_EXPERIMENT_H

#include <stddef.h>
#include <stdint.h>
#include "pak_entry_pipeline.h"
#include "bounded_zlib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAK_PAYLOAD_MAX_OUTPUT_BYTES ((size_t)4194304)
#define PAK_PAYLOAD_MAX_BLOCKS 64U

typedef enum {
    PAK_PAYLOAD_OK = 0,
    PAK_PAYLOAD_BAD_ARGUMENT,
    PAK_PAYLOAD_METADATA_REJECTED,
    PAK_PAYLOAD_UNSUPPORTED,
    PAK_PAYLOAD_RESOURCE_LIMIT,
    PAK_PAYLOAD_MALFORMED,
    PAK_PAYLOAD_PROVIDER_ERROR,
    PAK_PAYLOAD_SHORT_READ,
    PAK_PAYLOAD_SOURCE_CHANGED,
    PAK_PAYLOAD_CANCELLED,
    PAK_PAYLOAD_PROVIDER_CONTRACT,
    PAK_PAYLOAD_CODEC_REJECTED,
    PAK_PAYLOAD_INTERNAL_ERROR
} pak_payload_status;

typedef struct {
    uint8_t *reader_workspace;
    size_t reader_workspace_bytes;
    uint8_t *primary_scratch;
    size_t primary_scratch_bytes;
    pak_entry_block_v1 *blocks;
    size_t blocks_capacity;
    uint8_t *compressed_scratch;
    size_t compressed_scratch_bytes;
    uint8_t *codec_arena;
    size_t codec_arena_bytes;
    bz_state *codec_state;
    uint8_t *output;
    size_t output_capacity;
} pak_payload_buffers;

typedef struct {
    size_t max_output_bytes;
    bz_limits codec;
} pak_payload_limits;

typedef struct {
    pak_payload_status status;
    uint32_t payload_complete;
    uint32_t installation_supported; /* Always zero: raw transformation only. */
    uint32_t primary_validated;
    size_t decoded_bytes;            /* Zero on every failure. */
    pak_entry_pipeline_status metadata_status;
    pak_status reader_status;
    pak_reason reader_reason;
    pak_entry_status entry_status;
    pak_entry_reason entry_reason;
    uint32_t codec_attempted;
    bz_status codec_status;
    uint64_t aggregate_read_bytes;   /* Includes full failed requests. */
    uint32_t aggregate_read_calls;
    uint64_t payload_read_bytes;
    uint32_t payload_read_calls;
} pak_payload_result;

pak_payload_limits pak_payload_default_limits(void);

/* Raw bytes only, never extraction/install or archive authenticity. All
 * mutable buffers and budget must be valid disjoint caller objects, also
 * disjoint from source/limit descriptors. Provider-owned snapshot storage
 * must not alias them; read_at is a trusted synchronous caller callback.
 * Codec budget is caller initialized ONCE across blocks and retained after
 * failure; the wrapper never resets/refunds it. Preflight rejection writes
 * nothing. After valid preflight, every failure clears the full output
 * capacity. All supplied scratch is cleared on every operational exit.
 * Only payload_complete permits publication. No pointer is retained.
 */
pak_payload_result pak_read_v11_selected_payload(
    const pak_source_v1 *source, const pak_limits_v1 *reader_limits,
    const pak_entry_limits_v1 *entry_limits, size_t selected_record_offset,
    pak_payload_buffers buffers, pak_payload_limits limits,
    bz_budget *codec_budget);

#ifdef __cplusplus
}
#endif
#endif
