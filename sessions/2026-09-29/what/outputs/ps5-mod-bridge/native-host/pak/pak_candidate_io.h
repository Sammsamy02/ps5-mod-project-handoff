#ifndef PAK_CANDIDATE_IO_H
#define PAK_CANDIDATE_IO_H

#include "pak_secondary_integrity.h"

#define PAK_CANDIDATE_MAX_READ_BYTES 65536u
#define PAK_CANDIDATE_MAX_TOTAL_BYTES UINT64_C(20971520)
#define PAK_CANDIDATE_MAX_READ_CALLS 4096u

/* Internal synchronous contexts prepared by the composition owner after its
 * disjoint-storage preflight. They borrow pointers only during that operation
 * and are erased by its final cleanup. Neither context resets a read ledger. */
typedef enum pak_candidate_io_cause {
    PAK_CANDIDATE_IO_NONE = 0,
    PAK_CANDIDATE_IO_BAD_REQUEST,
    PAK_CANDIDATE_IO_BYTE_BUDGET,
    PAK_CANDIDATE_IO_CALL_BUDGET,
    PAK_CANDIDATE_IO_PROVIDER_CONTRACT,
    PAK_CANDIDATE_IO_TEE_RANGE
} pak_candidate_io_cause;

typedef struct pak_candidate_charging_context {
    const pak_source_v1 *base;
    pak_read_budget_v1 *budget;
    uint32_t max_read_bytes;
    pak_candidate_io_cause cause;
    uint32_t original_callback_calls;
    uint32_t provider_observed;
    uint32_t last_provider_status;
    uint32_t last_requested;
    uint32_t last_actual;
} pak_candidate_charging_context;

typedef struct pak_candidate_tee_context {
    const pak_source_v1 *raw_base;
    uint64_t span_offset;
    uint64_t span_bytes;
    uint8_t *capture;
    size_t capture_capacity;
    uint64_t copied;
    pak_candidate_io_cause cause;
    uint32_t original_callback_calls;
} pak_candidate_tee_context;

pak_provider_status pak_candidate_charging_read_at(
    void *context, uint64_t offset, uint8_t *destination,
    uint32_t requested, uint32_t *actual);

/* No ledger pointer: the secondary verifier reserved before calling this tee.
 * It delegates to the ORIGINAL provider, captures exactly sequential successful
 * bytes into the private directory buffer, and never adds a second charge. */
pak_provider_status pak_candidate_directory_tee_read_at(
    void *context, uint64_t offset, uint8_t *destination,
    uint32_t requested, uint32_t *actual);

#endif
