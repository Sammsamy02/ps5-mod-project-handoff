#ifndef NATIVE_PAK_RECONCILED_PAYLOAD_EXPERIMENT_H
#define NATIVE_PAK_RECONCILED_PAYLOAD_EXPERIMENT_H

#include <stddef.h>
#include <stdint.h>
#include "pak_reader.h"
#include "pak_entry_decoder.h"
#include "bounded_zlib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAK_RECONCILED_PAYLOAD_ABI_VERSION 1u
#define PAK_RECONCILED_PAYLOAD_MAX_OUTPUT_BYTES ((size_t)4194304)
#define PAK_RECONCILED_PAYLOAD_MAX_BLOCKS 64u
#define PAK_RECONCILED_PAYLOAD_MAX_REQUESTED_BYTES UINT64_C(8388608)
#define PAK_RECONCILED_PAYLOAD_MAX_READ_CALLS 256u
#define PAK_RECONCILED_PAYLOAD_MAX_READ_BYTES 65536u

typedef enum pak_reconciled_payload_profile {
    PAK_RECONCILED_PROFILE_RAW_UNENCRYPTED_STORED_SHA1_V1 = 1
} pak_reconciled_payload_profile;

typedef enum pak_reconciled_payload_status {
    PAK_RECONCILED_PAYLOAD_OK = 0,
    PAK_RECONCILED_PAYLOAD_BAD_ARGUMENT,
    PAK_RECONCILED_PAYLOAD_UNSUPPORTED,
    PAK_RECONCILED_PAYLOAD_MALFORMED,
    PAK_RECONCILED_PAYLOAD_RESOURCE_LIMIT,
    PAK_RECONCILED_PAYLOAD_PROVIDER_ERROR,
    PAK_RECONCILED_PAYLOAD_SHORT_READ,
    PAK_RECONCILED_PAYLOAD_SOURCE_CHANGED,
    PAK_RECONCILED_PAYLOAD_CANCELLED,
    PAK_RECONCILED_PAYLOAD_PROVIDER_CONTRACT,
    PAK_RECONCILED_PAYLOAD_CODEC_REJECTED,
    PAK_RECONCILED_PAYLOAD_DIGEST_MISMATCH,
    PAK_RECONCILED_PAYLOAD_INTERNAL_ERROR
} pak_reconciled_payload_status;

typedef struct pak_reconciled_payload_limits_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t coverage_profile;
    uint32_t max_read_bytes;
    uint32_t max_read_calls;
    uint32_t reserved;
    uint64_t max_requested_read_bytes;
    size_t max_output_bytes;
    bz_limits codec;
} pak_reconciled_payload_limits_v1;

typedef struct pak_reconciled_payload_buffers_v1 {
    uint8_t *compressed;
    size_t compressed_bytes;
    uint8_t *codec_arena;
    size_t codec_arena_bytes;
    bz_state *codec_state;
    uint8_t *output;
    size_t output_capacity;
} pak_reconciled_payload_buffers_v1;

typedef struct pak_reconciled_payload_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    pak_reconciled_payload_status status;
    uint32_t coverage_profile;
    uint32_t complete;
    uint32_t digest_valid;
    uint32_t installation_supported; /* Always zero. */
    uint32_t codec_attempted;
    bz_status codec_status;
    uint32_t read_calls; /* Local reservations, including failed attempts. */
    uint32_t provider_observed;
    uint32_t last_provider_status;
    uint32_t last_requested;
    uint32_t last_actual;
    uint64_t requested_read_bytes;
    uint64_t bytes_hashed;
    uint64_t snapshot_token;
    size_t decoded_bytes; /* Zero on every failure. */
    uint8_t digest[20];   /* Cleared unless complete and digest_valid. */
} pak_reconciled_payload_result_v1;

void pak_reconciled_payload_limits_v1_default(pak_reconciled_payload_limits_v1 *limits);

/* Caller has already reconciled immutable ordinary unencrypted v11 compact
 * metadata with the physical header and supplies its 20-byte stored digest.
 * This seam performs no primary/footer/header reads or reconciliation.
 * blocks_count is the actual immutable block-array extent and must equal the
 * entry count. Inputs, mutable buffers and budget must be valid disjoint
 * caller storage; provider backing must not alias mutable storage. Callbacks
 * are trusted, synchronous, and may not retain destinations or alter metadata.
 *
 * Composition supplies a globally charging source. This operation reserves
 * only its own local counters for bounds; it never touches a shared read
 * ledger or subtracts returned counters from one. Caller initializes the
 * SINGLE bz_budget once; no reset/refund occurs across blocks or failures.
 *
 * Invalid preflight storage/ABI writes nothing. After valid preflight every
 * failure clears full output capacity, compressed/arena/state and any digest
 * publication. Success clears scratch and publishes only after exact decode
 * AND stored-byte SHA1 match. No heap, retained pointer, install/authenticity
 * or hard CPU-interrupt claim. The explicit profile hashes raw None or stored
 * compressed Zlib streams concatenated in order, excluding header bytes.
 */
pak_reconciled_payload_result_v1 pak_decode_reconciled_v11_payload(
    const pak_source_v1 *source,
    const pak_entry_result_v1 *entry,
    const pak_entry_block_v1 *blocks,
    size_t blocks_count,
    const uint8_t expected_stored_sha1[20],
    const pak_reconciled_payload_limits_v1 *limits,
    pak_reconciled_payload_buffers_v1 buffers,
    bz_budget *codec_budget);

#ifdef __cplusplus
}
#endif
#endif
