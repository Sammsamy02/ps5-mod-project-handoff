#ifndef NATIVE_PAK_PRIMARY_CAPTURE_EXPERIMENT_H
#define NATIVE_PAK_PRIMARY_CAPTURE_EXPERIMENT_H

#include <stddef.h>
#include <stdint.h>

#include "pak_reader.h"
#include "pak_entry_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAK_PRIMARY_CAPTURE_ABI_VERSION 1u

typedef enum pak_primary_capture_status {
    PAK_PRIMARY_CAPTURE_OK = 0,
    PAK_PRIMARY_CAPTURE_BAD_ARGUMENT = 1,
    PAK_PRIMARY_CAPTURE_READER_REJECTED = 2,
    PAK_PRIMARY_CAPTURE_CAPTURE_REJECTED = 3,
    PAK_PRIMARY_CAPTURE_UNSUPPORTED = 4
} pak_primary_capture_status;

typedef enum pak_primary_capture_reason {
    PAK_PRIMARY_CAPTURE_REASON_NONE = 0,
    PAK_PRIMARY_CAPTURE_REASON_BAD_ARGUMENT,
    PAK_PRIMARY_CAPTURE_REASON_READER_REJECTED,
    PAK_PRIMARY_CAPTURE_REASON_INCOMPLETE_CAPTURE,
    PAK_PRIMARY_CAPTURE_REASON_PRIMARY_LAYOUT,
    PAK_PRIMARY_CAPTURE_REASON_METHOD_TABLE
} pak_primary_capture_reason;

typedef struct pak_primary_capture_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    pak_primary_capture_status status;
    pak_primary_capture_reason reason;
    pak_status reader_status;
    pak_reason reader_reason;
    uint32_t primary_validated;
    uint32_t primary_capture_complete;
    uint32_t encoded_span_located;
    uint32_t primary_capture_bytes;
    uint32_t encoded_span_offset;
    uint32_t encoded_span_bytes;
    uint64_t aggregate_read_bytes;
    uint32_t aggregate_read_calls;
    uint32_t reserved;
    pak_primary_result_v1 primary;
    pak_entry_method_table_v1 method_table;
} pak_primary_capture_result_v1;

/*
 * Capture and validate ordinary unencrypted v11 primary metadata without
 * decoding any encoded-entry record (including offset zero). The caller must
 * assert that read_at observes one immutable archive snapshot. On success,
 * `primary_capture` still contains the exact full primary bytes the reader
 * hashed, and result owns copies of primary metadata, method names, encoded
 * span location/size and aggregate read counters. A located zero-byte encoded
 * span is valid metadata success and is distinguished by encoded_span_located.
 *
 * `reader_workspace_bytes` must be exactly 64 KiB; capture capacity must be
 * 1..4 MiB. The source object, limits, result, workspace and capture ranges
 * must be pairwise disjoint. The opaque provider context/backing memory must
 * also remain disjoint and stable, as asserted by the caller. Invalid or
 * overlapping storage is rejected before any object is changed. Once storage
 * preflight passes, operational failure clears the full supplied workspace
 * and capture range; result metadata-complete flags and owned primary/method
 * fields remain zero while status, reader cause and child request counters
 * remain available. No pointer is retained and no heap allocation is
 * performed. This operation does not inspect secondary contents or establish
 * installability.
 */
pak_primary_capture_status pak_capture_v11_primary(
    const pak_source_v1 *source,
    const pak_limits_v1 *reader_limits,
    uint8_t *reader_workspace,
    size_t reader_workspace_bytes,
    uint8_t *primary_capture,
    size_t primary_capture_capacity,
    pak_primary_capture_result_v1 *result);

#ifdef __cplusplus
}
#endif

#endif
