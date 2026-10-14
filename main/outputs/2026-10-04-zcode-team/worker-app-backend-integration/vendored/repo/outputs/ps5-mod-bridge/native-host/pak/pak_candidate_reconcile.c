#include "pak_candidate_reconcile.h"
#include <string.h>

#define MAX_SIZE UINT64_C(4194304)
#define MAX_BLOCK UINT64_C(1048576)

pak_candidate_reconcile_status pak_candidate_reconcile_v11(
    const pak_entry_result_v1 *entry,
    const pak_entry_block_v1 *indexed_blocks, size_t indexed_count,
    const pak_physical_result_v1 *physical,
    const pak_physical_block_v1 *physical_blocks, size_t physical_count,
    uint64_t logical_archive_size)
{
    uint64_t stored_cursor, output_cursor;
    size_t i;
    int is_none, is_zlib;
    uint32_t expected_header;
    if (!entry || !physical || !logical_archive_size ||
        entry->struct_size != sizeof(*entry) || entry->abi_version != 1u ||
        physical->struct_size != sizeof(*physical) || physical->abi_version != 1u ||
        entry->status != PAK_ENTRY_STATUS_OK || entry->record_complete != 1u ||
        physical->status != PAK_PHYSICAL_STATUS_OK || physical->record_complete != 1u ||
        indexed_count != entry->block_count || physical_count != physical->block_count ||
        indexed_count > 64u || physical_count > 64u ||
        (indexed_count && !indexed_blocks) || (physical_count && !physical_blocks))
        return PAK_RECONCILE_BAD_ARGUMENT;
    if (physical->flags != 0u) return PAK_RECONCILE_UNSUPPORTED;
    if (entry->method_name_bytes != 4u || physical->method_name_bytes != 4u ||
        entry->method_index != physical->method_index ||
        memcmp(entry->method_name, physical->method_name, 4u) != 0)
        return PAK_RECONCILE_HEADER_CONFLICT;
    is_none = memcmp(entry->method_name, "None", 4u) == 0;
    is_zlib = memcmp(entry->method_name, "Zlib", 4u) == 0;
    if (!is_none && !is_zlib) return PAK_RECONCILE_UNSUPPORTED;
    expected_header = is_none ? 53u : 57u + 16u * (uint32_t)indexed_count;
    if ((is_none && indexed_count != 0u) || (is_zlib && indexed_count == 0u) ||
        indexed_count != physical_count || entry->header_bytes != expected_header ||
        physical->header_bytes != expected_header || physical->bytes_consumed != expected_header ||
        entry->compressed_size != physical->compressed_size ||
        entry->uncompressed_size != physical->uncompressed_size ||
        (physical->stored_offset != 0u && physical->stored_offset != entry->archive_offset))
        return PAK_RECONCILE_HEADER_CONFLICT;
    if (entry->compressed_size > MAX_SIZE || entry->uncompressed_size > MAX_SIZE ||
        physical->physical_block_size > MAX_BLOCK)
        return PAK_RECONCILE_UNSUPPORTED;
    if (entry->archive_offset > logical_archive_size ||
        expected_header > logical_archive_size - entry->archive_offset ||
        entry->data_offset != entry->archive_offset + expected_header ||
        entry->compressed_size > logical_archive_size - entry->data_offset)
        return PAK_RECONCILE_RANGE_CONFLICT;
    if (is_none) {
        if (entry->compressed_size != entry->uncompressed_size)
            return PAK_RECONCILE_HEADER_CONFLICT;
        /* None streams in bounded reads under the total MAX_SIZE cap above. */
        return PAK_RECONCILE_OK;
    }
    if (indexed_count == 1u) {
        if (!physical->physical_block_size ||
            physical->physical_block_size < entry->uncompressed_size)
            return PAK_RECONCILE_UNSUPPORTED;
    } else if (!entry->compression_block_size || entry->compression_block_size > MAX_BLOCK ||
               entry->compression_block_size != physical->physical_block_size) {
        return PAK_RECONCILE_BLOCK_CONFLICT;
    }
    stored_cursor = entry->data_offset;
    output_cursor = 0u;
    for (i = 0u; i < indexed_count; ++i) {
        const pak_entry_block_v1 *b = &indexed_blocks[i];
        const pak_physical_block_v1 *p = &physical_blocks[i];
        if (b->compressed_offset != stored_cursor || !b->compressed_size ||
            b->compressed_size > MAX_BLOCK || b->compressed_size > logical_archive_size - stored_cursor ||
            b->uncompressed_offset != output_cursor || !b->uncompressed_size ||
            b->uncompressed_size > MAX_BLOCK || output_cursor > entry->uncompressed_size ||
            b->uncompressed_size > entry->uncompressed_size - output_cursor ||
            p->relative_start > logical_archive_size - entry->archive_offset ||
            p->relative_end > logical_archive_size - entry->archive_offset ||
            entry->archive_offset + p->relative_start != stored_cursor ||
            entry->archive_offset + p->relative_end != stored_cursor + b->compressed_size ||
            (indexed_count > 1u &&
             ((i + 1u < indexed_count && b->uncompressed_size != entry->compression_block_size) ||
              b->uncompressed_size > entry->compression_block_size)))
            return PAK_RECONCILE_BLOCK_CONFLICT;
        stored_cursor += b->compressed_size;
        output_cursor += b->uncompressed_size;
    }
    if (stored_cursor != entry->data_offset + entry->compressed_size ||
        output_cursor != entry->uncompressed_size)
        return PAK_RECONCILE_BLOCK_CONFLICT;
    return PAK_RECONCILE_OK;
}
