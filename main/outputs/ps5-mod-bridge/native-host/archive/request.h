#ifndef MB_ARCHIVE_REQUEST_H
#define MB_ARCHIVE_REQUEST_H
#include <stddef.h>
#include <stdint.h>
#include "intake.h"

#define MB_ARCHIVE_REQUEST_ABI_V1 1u
#define MB_ARCHIVE_REQUEST_MAX_BYTES 512u
#define MB_ARCHIVE_REQUEST_COMPONENT_BYTES 41u

enum mb_archive_request_status_v1 {
    MB_ARCHIVE_REQUEST_OK = 0,
    MB_ARCHIVE_REQUEST_BAD_ARGUMENT = 1,
    MB_ARCHIVE_REQUEST_INVALID = 2,
    MB_ARCHIVE_REQUEST_LIMIT = 3,
    MB_ARCHIVE_REQUEST_CONTEXT_MISMATCH = 4
};

typedef struct mb_archive_request_v1 {
    uint32_t struct_size, abi_version, status, complete;
    uint64_t source_bytes;
    uint8_t source_sha256[32], context_sha256[32], target_token[32];
    char request_id[33];
    char source_ref[MB_ARCHIVE_REQUEST_COMPONENT_BYTES];
    char context_ref[MB_ARCHIVE_REQUEST_COMPONENT_BYTES];
    char quarantine_ref[40], receipt_ref[48], claim_ref[40];
    char nonce[33], prefix[49], suffix[17];
} mb_archive_request_v1;

typedef struct mb_archive_request_ref_v1 {
    uint32_t struct_size, abi_version, status, complete;
    uint8_t context_sha256[32];
    char request_id[33];
    char source_ref[MB_ARCHIVE_REQUEST_COMPONENT_BYTES];
    char context_ref[MB_ARCHIVE_REQUEST_COMPONENT_BYTES];
    char quarantine_ref[40], receipt_ref[48], claim_ref[40];
} mb_archive_request_ref_v1;

/* Validates the complete request grammar before returning any file reference.
 * The caller may then open only context_ref relative to its already checked
 * inbox directory; the full decoder re-parses request_bytes before binding. */
int mb_archive_request_ref_decode_v1(const uint8_t *request_bytes, size_t request_size,
    mb_archive_request_ref_v1 *out);

/* Parses exact canonical request/context ASCII records and binds the caller's
 * observed context digest to the request. No heap, I/O, retained pointers, or
 * target authority. Known bad pointer/alias preflight is no-write; admitted
 * parse failure clears the complete output object. */
int mb_archive_request_decode_v1(const uint8_t *request_bytes, size_t request_size,
    const uint8_t *context_bytes, size_t context_size,
    const uint8_t observed_context_sha256[32], mb_archive_request_v1 *out);

#endif
