#ifndef PAK_CANDIDATE_RECONCILE_H
#define PAK_CANDIDATE_RECONCILE_H

#include "pak_entry_decoder.h"
#include "pak_physical_header.h"

/* Internal pure gate. Inputs are private validated child results; no source
 * reads, hashes, writes, allocation or accepted-result publication occur here. */
typedef enum pak_candidate_reconcile_status {
    PAK_RECONCILE_OK = 0,
    PAK_RECONCILE_BAD_ARGUMENT,
    PAK_RECONCILE_UNSUPPORTED,
    PAK_RECONCILE_HEADER_CONFLICT,
    PAK_RECONCILE_BLOCK_CONFLICT,
    PAK_RECONCILE_RANGE_CONFLICT
} pak_candidate_reconcile_status;

pak_candidate_reconcile_status pak_candidate_reconcile_v11(
    const pak_entry_result_v1 *entry,
    const pak_entry_block_v1 *indexed_blocks, size_t indexed_count,
    const pak_physical_result_v1 *physical,
    const pak_physical_block_v1 *physical_blocks, size_t physical_count,
    uint64_t logical_archive_size);

#endif
