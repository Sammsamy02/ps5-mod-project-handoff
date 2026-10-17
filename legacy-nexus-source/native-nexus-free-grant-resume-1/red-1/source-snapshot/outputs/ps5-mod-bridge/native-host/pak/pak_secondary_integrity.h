#ifndef NATIVE_PAK_SECONDARY_INTEGRITY_EXPERIMENT_H
#define NATIVE_PAK_SECONDARY_INTEGRITY_EXPERIMENT_H

/* Borrowed ABI, supplied through the corrected primary candidate include path. */
#include "pak_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAK_SECONDARY_ABI_VERSION 1u
#define PAK_SECONDARY_WORKSPACE_BYTES 4096u
#define PAK_SECONDARY_MAX_SPAN_BYTES (4u * 1024u * 1024u)
#define PAK_SECONDARY_MAX_READ_BYTES 4096u
#define PAK_SECONDARY_MAX_READ_CALLS 1024u

typedef enum pak_secondary_reason {
    PAK_SECONDARY_REASON_NONE = 0,
    PAK_SECONDARY_REASON_BAD_ARGUMENT,
    PAK_SECONDARY_REASON_STORAGE_OVERLAP,
    PAK_SECONDARY_REASON_BAD_RANGE,
    PAK_SECONDARY_REASON_SPAN_LIMIT,
    PAK_SECONDARY_REASON_BYTE_BUDGET,
    PAK_SECONDARY_REASON_CALL_BUDGET,
    PAK_SECONDARY_REASON_PROVIDER_IO,
    PAK_SECONDARY_REASON_PROVIDER_UNAVAILABLE,
    PAK_SECONDARY_REASON_PROVIDER_SHORT,
    PAK_SECONDARY_REASON_PROVIDER_CHANGED,
    PAK_SECONDARY_REASON_PROVIDER_CANCELLED,
    PAK_SECONDARY_REASON_PROVIDER_CONTRACT,
    PAK_SECONDARY_REASON_SHA1_BACKEND,
    PAK_SECONDARY_REASON_DIGEST_MISMATCH
} pak_secondary_reason;

typedef struct pak_secondary_limits_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t max_span_bytes;
    uint32_t max_read_bytes;
    uint32_t max_read_calls;
    uint32_t reserved;
} pak_secondary_limits_v1;

/* Initialize once in the composition owner; this verifier never resets it. */
typedef struct pak_read_budget_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t remaining_requested_bytes;
    uint32_t remaining_calls;
    uint32_t reserved;
} pak_read_budget_v1;

typedef struct pak_secondary_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    pak_status status;
    pak_secondary_reason reason;
    uint64_t requested_read_bytes;
    uint64_t bytes_hashed;
    uint64_t snapshot_token;
    uint32_t read_calls;
    uint32_t provider_observed;
    uint32_t last_provider_status;
    uint32_t last_requested;
    uint32_t last_actual;
    uint32_t span_complete;
    uint32_t digest_valid;
    uint32_t reserved;
    uint8_t digest[20];
} pak_secondary_result_v1;

void pak_secondary_limits_v1_default(pak_secondary_limits_v1 *limits);

/*
 * Raw UNENCRYPTED v11 secondary bytes only. Caller has already validated the
 * primary declaration and file-layout disjointness; no parsing is performed.
 * Exact 4096-byte workspace and mutable storage must be disjoint from known
 * inputs and each other. One caller exclusively owns the budget and operation
 * for this call; these are not concurrent atomic counters. Callback is
 * synchronous and may borrow destination only until its
 * return. Valid output ownership is required: writable/aligned result storage
 * that does not alias immutable metadata or the shared budget. An invalid
 * output alias is rejected without writing it; otherwise every failure clears
 * digest/digest_valid. span_complete alone never establishes a digest match.
 * Full requests and failed attempts consume the shared budget before callback.
 * No retries, refunds, heap allocation, or caller-pointer retention.
 */
pak_status pak_verify_v11_secondary_raw_sha1(
    const pak_source_v1 *source,
    const pak_secondary_index *span,
    const pak_secondary_limits_v1 *limits,
    pak_read_budget_v1 *budget,
    uint8_t *workspace,
    size_t workspace_bytes,
    pak_secondary_result_v1 *result);

#ifdef __cplusplus
}
#endif

#endif
