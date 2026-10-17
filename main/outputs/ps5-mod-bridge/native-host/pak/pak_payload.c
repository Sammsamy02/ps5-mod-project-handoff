#include "pak_payload.h"
#include <string.h>

typedef struct { uintptr_t begin, end; } byte_range;

static int range_for(const void *pointer, size_t bytes, byte_range *range) {
    uintptr_t address = (uintptr_t)pointer;
    if ((bytes && !pointer) || bytes > UINTPTR_MAX - address) return 0;
    *range = (byte_range){address, address + bytes};
    return 1;
}

static int overlap(byte_range a, byte_range b) {
    return a.begin != a.end && b.begin != b.end && a.begin < b.end && b.begin < a.end;
}

static void clear_bytes(void *pointer, size_t bytes) {
    volatile uint8_t *p = (volatile uint8_t *)pointer;
    for (size_t i = 0; i < bytes; ++i) p[i] = 0;
}

static int valid_buffers(const pak_source_v1 *source, const pak_limits_v1 *reader,
                        const pak_entry_limits_v1 *entry, pak_payload_buffers b,
                        pak_payload_limits limits, bz_budget *budget) {
    if (!source || !reader || !entry || !budget || !b.codec_state ||
        source->struct_size < sizeof(*source) || source->abi_version != PAK_READER_ABI_VERSION ||
        source->reserved || !source->read_at || !source->snapshot_token ||
        source->snapshot_scope != PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE ||
        reader->struct_size < sizeof(*reader) || reader->abi_version != PAK_READER_ABI_VERSION ||
        reader->reserved || !reader->max_read_bytes || reader->max_read_bytes > PAK_READER_MAX_READ_BYTES ||
        !reader->max_primary_bytes || reader->max_primary_bytes > PAK_READER_MAX_PRIMARY_BYTES ||
        !reader->max_total_read_bytes || reader->max_total_read_bytes > PAK_READER_MAX_TOTAL_READ_BYTES ||
        !reader->max_read_calls || reader->max_read_calls > PAK_READER_MAX_READ_CALLS ||
        reader->max_declared_files > PAK_READER_MAX_FILES || reader->max_mount_bytes > PAK_READER_MAX_MOUNT_BYTES ||
        b.reader_workspace_bytes != PAK_READER_WORKSPACE_BYTES ||
        !b.primary_scratch_bytes || b.primary_scratch_bytes > PAK_READER_MAX_PRIMARY_BYTES ||
        b.blocks_capacity > PAK_PAYLOAD_MAX_BLOCKS ||
        !b.compressed_scratch_bytes || b.compressed_scratch_bytes > BZ_MAX_BLOCK_BYTES ||
        !b.codec_arena_bytes || b.codec_arena_bytes > BZ_MAX_ARENA_BYTES ||
        b.output_capacity > PAK_PAYLOAD_MAX_OUTPUT_BYTES ||
        limits.max_output_bytes > PAK_PAYLOAD_MAX_OUTPUT_BYTES ||
        limits.codec.max_input_bytes > BZ_MAX_BLOCK_BYTES ||
        limits.codec.max_output_bytes > BZ_MAX_BLOCK_BYTES ||
        limits.codec.max_arena_bytes > BZ_MAX_ARENA_BYTES ||
        b.codec_arena_bytes > limits.codec.max_arena_bytes ||
        !limits.codec.input_per_step || limits.codec.input_per_step > BZ_MAX_STEP_BYTES ||
        !limits.codec.output_per_step || limits.codec.output_per_step > BZ_MAX_STEP_BYTES)
        return 0;
    byte_range ranges[11];
    const void *pointers[11] = {source, reader, entry, b.reader_workspace, b.primary_scratch,
        b.blocks, b.compressed_scratch, b.codec_arena, b.codec_state, b.output, budget};
    const size_t sizes[11] = {sizeof(*source), sizeof(*reader), sizeof(*entry), b.reader_workspace_bytes,
        b.primary_scratch_bytes, b.blocks_capacity * sizeof(*b.blocks), b.compressed_scratch_bytes,
        b.codec_arena_bytes, sizeof(*b.codec_state), b.output_capacity, sizeof(*budget)};
    for (size_t i = 0; i < 11; ++i) {
        if (!range_for(pointers[i], sizes[i], &ranges[i])) return 0;
        for (size_t j = 0; j < i; ++j) if (overlap(ranges[i], ranges[j])) return 0;
    }
    return 1;
}

static void clean(pak_payload_buffers b, int failed) {
    if (failed) clear_bytes(b.output, b.output_capacity);
    clear_bytes(b.reader_workspace, b.reader_workspace_bytes);
    clear_bytes(b.primary_scratch, b.primary_scratch_bytes);
    clear_bytes(b.blocks, b.blocks_capacity * sizeof(*b.blocks));
    clear_bytes(b.compressed_scratch, b.compressed_scratch_bytes);
    clear_bytes(b.codec_arena, b.codec_arena_bytes);
    clear_bytes(b.codec_state, sizeof(*b.codec_state));
}

static pak_payload_status read_payload(const pak_source_v1 *source,
    const pak_limits_v1 *limits, uint64_t offset, uint8_t *destination,
    uint32_t requested, pak_payload_result *result) {
    if (!requested || requested > limits->max_read_bytes ||
        offset > source->logical_size || requested > source->logical_size - offset)
        return PAK_PAYLOAD_MALFORMED;
    if (result->aggregate_read_calls >= limits->max_read_calls ||
        result->aggregate_read_bytes > limits->max_total_read_bytes ||
        requested > limits->max_total_read_bytes - result->aggregate_read_bytes)
        return PAK_PAYLOAD_RESOURCE_LIMIT;
    ++result->aggregate_read_calls;
    result->aggregate_read_bytes += requested;
    ++result->payload_read_calls;
    result->payload_read_bytes += requested;
    uint32_t actual = 0;
    pak_provider_status status = source->read_at(source->context, offset, destination, requested, &actual);
    if (actual > requested) return PAK_PAYLOAD_PROVIDER_CONTRACT;
    switch (status) {
        case PAK_PROVIDER_OK: return actual == requested ? PAK_PAYLOAD_OK : PAK_PAYLOAD_SHORT_READ;
        case PAK_PROVIDER_CHANGED: return PAK_PAYLOAD_SOURCE_CHANGED;
        case PAK_PROVIDER_CANCELLED: return PAK_PAYLOAD_CANCELLED;
        case PAK_PROVIDER_IO_ERROR:
        case PAK_PROVIDER_UNAVAILABLE: return PAK_PAYLOAD_PROVIDER_ERROR;
        case PAK_PROVIDER_CONTRACT_ERROR:
        default: return PAK_PAYLOAD_PROVIDER_CONTRACT;
    }
}

static int reserve_copy(bz_budget *budget, uint64_t bytes) {
    if (budget->work_used > budget->work_limit || budget->steps_used >= budget->step_limit ||
        budget->allocations_used > budget->allocation_limit || bytes > budget->work_limit - budget->work_used)
        return 0;
    budget->work_used += bytes;
    ++budget->steps_used;
    return 1;
}

static pak_payload_status geometry(const pak_source_v1 *source, const pak_entry_result_v1 *entry,
    pak_payload_buffers b, pak_payload_limits limits, int *is_none) {
    *is_none = entry->method_index == 0 && entry->method_name_bytes == 4 &&
               memcmp(entry->method_name, "None", 4) == 0;
    const int is_zlib = entry->method_index != 0 && entry->method_name_bytes == 4 &&
                        memcmp(entry->method_name, "Zlib", 4) == 0;
    if (!*is_none && !is_zlib) return PAK_PAYLOAD_UNSUPPORTED;
    if (entry->uncompressed_size > limits.max_output_bytes || entry->uncompressed_size > b.output_capacity)
        return PAK_PAYLOAD_RESOURCE_LIMIT;
    if (*is_none) {
        if (entry->block_count) return PAK_PAYLOAD_UNSUPPORTED;
        if (entry->compressed_size != entry->uncompressed_size) return PAK_PAYLOAD_MALFORMED;
        if (entry->uncompressed_size > BZ_MAX_BLOCK_BYTES) return PAK_PAYLOAD_RESOURCE_LIMIT;
        return entry->data_offset > source->logical_size ||
               entry->compressed_size > source->logical_size - entry->data_offset
               ? PAK_PAYLOAD_MALFORMED : PAK_PAYLOAD_OK;
    }
    if (!entry->block_count || entry->block_count > b.blocks_capacity) return PAK_PAYLOAD_UNSUPPORTED;
    uint64_t output_position = 0, compressed_position = entry->data_offset;
    for (uint32_t i = 0; i < entry->block_count; ++i) {
        const pak_entry_block_v1 *block = &b.blocks[i];
        if (block->compressed_size > BZ_MAX_BLOCK_BYTES ||
            block->compressed_size > limits.codec.max_input_bytes ||
            block->compressed_size > b.compressed_scratch_bytes ||
            block->uncompressed_size > BZ_MAX_BLOCK_BYTES ||
            block->uncompressed_size > limits.codec.max_output_bytes)
            return PAK_PAYLOAD_RESOURCE_LIMIT;
        if (!block->compressed_size || block->compressed_offset != compressed_position ||
            block->uncompressed_offset != output_position ||
            compressed_position > source->logical_size ||
            block->compressed_size > source->logical_size - compressed_position ||
            output_position > entry->uncompressed_size ||
            block->uncompressed_size > entry->uncompressed_size - output_position)
            return PAK_PAYLOAD_MALFORMED;
        compressed_position += block->compressed_size;
        output_position += block->uncompressed_size;
    }
    return output_position == entry->uncompressed_size &&
           compressed_position - entry->data_offset == entry->compressed_size
           ? PAK_PAYLOAD_OK : PAK_PAYLOAD_MALFORMED;
}

pak_payload_limits pak_payload_default_limits(void) {
    pak_payload_limits limits;
    limits.max_output_bytes = PAK_PAYLOAD_MAX_OUTPUT_BYTES;
    limits.codec = bz_default_limits();
    return limits;
}

pak_payload_result pak_read_v11_selected_payload(
    const pak_source_v1 *source, const pak_limits_v1 *reader_limits,
    const pak_entry_limits_v1 *entry_limits, size_t selected_record_offset,
    pak_payload_buffers buffers, pak_payload_limits limits,
    bz_budget *codec_budget) {
    pak_payload_result result = {0};
    result.status = PAK_PAYLOAD_BAD_ARGUMENT;
    result.metadata_status = PAK_ENTRY_PIPELINE_BAD_ARGUMENT;
    result.reader_status = PAK_STATUS_BAD_ARGUMENT;
    result.reader_reason = PAK_REASON_BAD_ARGUMENT;
    result.entry_status = PAK_ENTRY_STATUS_BAD_ARGUMENT;
    result.entry_reason = PAK_ENTRY_REASON_BAD_ARGUMENT;
    if (!valid_buffers(source, reader_limits, entry_limits, buffers, limits, codec_budget))
        return result;
    const pak_source_v1 stable_source = *source;
    const pak_limits_v1 stable_limits = *reader_limits;
    const pak_entry_limits_v1 stable_entry_limits = *entry_limits;
    pak_entry_pipeline_result_v1 metadata;
    result.metadata_status = pak_read_v11_one_entry(&stable_source, &stable_limits,
        buffers.reader_workspace, buffers.reader_workspace_bytes,
        buffers.primary_scratch, buffers.primary_scratch_bytes, selected_record_offset,
        &stable_entry_limits, buffers.blocks, buffers.blocks_capacity, &metadata);
    result.reader_status = metadata.reader_status;
    result.reader_reason = metadata.reader_reason;
    result.entry_status = metadata.entry_status;
    result.entry_reason = metadata.entry_reason;
    result.primary_validated = metadata.primary_validated;
    result.aggregate_read_bytes = metadata.aggregate_read_bytes;
    result.aggregate_read_calls = metadata.aggregate_read_calls;
    if (result.aggregate_read_bytes > stable_limits.max_total_read_bytes ||
        result.aggregate_read_calls > stable_limits.max_read_calls) {
        result.status = PAK_PAYLOAD_INTERNAL_ERROR;
        goto done;
    }
    if (result.metadata_status != PAK_ENTRY_PIPELINE_OK || !metadata.entry_complete ||
        !metadata.primary_validated || !metadata.primary_sha1_matches ||
        !metadata.primary_sha1_checked || !metadata.primary_metadata_complete) {
        result.status = PAK_PAYLOAD_METADATA_REJECTED;
        goto done;
    }
    int is_none;
    result.status = geometry(&stable_source, &metadata.entry, buffers, limits, &is_none);
    if (result.status != PAK_PAYLOAD_OK) goto done;
    if (is_none) {
        size_t copied = 0;
        while (copied < metadata.entry.uncompressed_size) {
            size_t request = (size_t)metadata.entry.uncompressed_size - copied;
            if (request > stable_limits.max_read_bytes) request = stable_limits.max_read_bytes;
            if (request > buffers.compressed_scratch_bytes) request = buffers.compressed_scratch_bytes;
            result.status = read_payload(&stable_source, &stable_limits,
                metadata.entry.data_offset + copied, buffers.compressed_scratch, (uint32_t)request, &result);
            if (result.status != PAK_PAYLOAD_OK) goto done;
            if (!reserve_copy(codec_budget, request)) {
                result.status = PAK_PAYLOAD_RESOURCE_LIMIT;
                goto done;
            }
            memcpy(buffers.output + copied, buffers.compressed_scratch, request);
            copied += request;
        }
    } else {
        for (uint32_t i = 0; i < metadata.entry.block_count; ++i) {
            const pak_entry_block_v1 block = buffers.blocks[i];
            size_t loaded = 0;
            while (loaded < block.compressed_size) {
                size_t request = (size_t)block.compressed_size - loaded;
                if (request > stable_limits.max_read_bytes) request = stable_limits.max_read_bytes;
                result.status = read_payload(&stable_source, &stable_limits,
                    block.compressed_offset + loaded, buffers.compressed_scratch + loaded,
                    (uint32_t)request, &result);
                if (result.status != PAK_PAYLOAD_OK) goto done;
                loaded += request;
            }
            result.codec_attempted = 1;
            uint8_t *block_output = buffers.output ? buffers.output + (size_t)block.uncompressed_offset : NULL;
            bz_result decoded = bz_decode(buffers.compressed_scratch, (size_t)block.compressed_size,
                (size_t)block.uncompressed_size, block_output, (size_t)block.uncompressed_size,
                buffers.codec_arena, buffers.codec_arena_bytes, buffers.codec_state, codec_budget, limits.codec);
            result.codec_status = decoded.status;
            if (decoded.status != BZ_OK || !decoded.complete || decoded.decoded_size != block.uncompressed_size) {
                result.status = decoded.status == BZ_RESOURCE_LIMIT ? PAK_PAYLOAD_RESOURCE_LIMIT : PAK_PAYLOAD_CODEC_REJECTED;
                goto done;
            }
        }
    }
    result.status = PAK_PAYLOAD_OK;
    result.payload_complete = 1;
    result.decoded_bytes = (size_t)metadata.entry.uncompressed_size;
done:
    clean(buffers, result.status != PAK_PAYLOAD_OK);
    return result;
}
