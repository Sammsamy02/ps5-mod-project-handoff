#include "pak_physical_header.h"

#include <limits.h>
#include <string.h>

typedef struct parsed_header {
    pak_physical_result_v1 result;
    pak_physical_block_v1 blocks[PAK_PHYSICAL_HEADER_MAX_BLOCKS];
} parsed_header;

static uint32_t load_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t load_le64(const uint8_t *p)
{
    return (uint64_t)load_le32(p) | ((uint64_t)load_le32(p + 4u) << 32);
}

static int ranges_disjoint(const void *left, size_t left_bytes,
                           const void *right, size_t right_bytes)
{
    uintptr_t left_start;
    uintptr_t right_start;
    uintptr_t left_end;
    uintptr_t right_end;

    if (left_bytes == 0u || right_bytes == 0u) return 1;
    if (left == NULL || right == NULL) return 0;
    left_start = (uintptr_t)left;
    right_start = (uintptr_t)right;
    if (left_bytes > UINTPTR_MAX - left_start || right_bytes > UINTPTR_MAX - right_start) {
        return 0;
    }
    left_end = left_start + left_bytes;
    right_end = right_start + right_bytes;
    return left_end <= right_start || right_end <= left_start;
}

static int valid_method_table(const pak_entry_method_table_v1 *table)
{
    uint32_t i;
    if (table == NULL || table->struct_size != sizeof(*table) ||
        table->abi_version != PAK_ENTRY_DECODER_ABI_VERSION || table->reserved != 0u ||
        table->count == 0u || table->count > PAK_ENTRY_DECODER_MAX_METHODS ||
        table->methods[0].length != 4u ||
        memcmp(table->methods[0].bytes, "None", 4u) != 0) {
        return 0;
    }
    for (i = 0u; i < table->count; ++i) {
        uint32_t j;
        const pak_entry_method_name *method = &table->methods[i];
        if (method->length == 0u || method->length > PAK_ENTRY_DECODER_MAX_NAME_BYTES ||
            method->reserved[0] != 0u || method->reserved[1] != 0u ||
            method->reserved[2] != 0u) {
            return 0;
        }
        if (i != 0u && method->length == 4u &&
            memcmp(method->bytes, "None", 4u) == 0) {
            return 0;
        }
        for (j = 0u; j < method->length; ++j) {
            if (method->bytes[j] < 0x21u || method->bytes[j] > 0x7eu) return 0;
        }
    }
    return 1;
}

static int valid_limits(const pak_physical_limits_v1 *limits)
{
    if (limits == NULL || limits->struct_size != sizeof(*limits) ||
        limits->abi_version != PAK_PHYSICAL_HEADER_ABI_VERSION || limits->reserved != 0u ||
        limits->max_header_bytes < 53u ||
        limits->max_header_bytes > PAK_PHYSICAL_HEADER_MAX_BYTES ||
        limits->max_blocks > PAK_PHYSICAL_HEADER_MAX_BLOCKS ||
        limits->max_compressed_size > INT64_MAX ||
        limits->max_uncompressed_size > INT64_MAX) {
        return 0;
    }
    return 1;
}

static void initialize_result(pak_physical_result_v1 *result,
                              pak_physical_status status,
                              pak_physical_reason reason)
{
    memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result);
    result->abi_version = PAK_PHYSICAL_HEADER_ABI_VERSION;
    result->status = status;
    result->reason = reason;
}

static pak_physical_status fail(pak_physical_result_v1 *result,
                                pak_physical_block_v1 *blocks_out,
                                size_t blocks_capacity,
                                pak_physical_status status,
                                pak_physical_reason reason)
{
    if (blocks_out != NULL && blocks_capacity != 0u) {
        memset(blocks_out, 0, blocks_capacity * sizeof(*blocks_out));
    }
    initialize_result(result, status, reason);
    return status;
}

static int is_name(const pak_entry_method_name *name, const char *text, size_t length)
{
    return name->length == length && memcmp(name->bytes, text, length) == 0;
}

pak_physical_status pak_physical_header_parse_v11(
    const uint8_t *header_bytes,
    size_t header_span_bytes,
    const pak_entry_method_table_v1 *method_table,
    const pak_physical_limits_v1 *limits,
    pak_physical_block_v1 *blocks_out,
    size_t blocks_capacity,
    pak_physical_result_v1 *result)
{
    parsed_header pending;
    uint64_t stored_offset;
    uint64_t compressed_size;
    uint64_t uncompressed_size;
    uint64_t payload_end;
    uint64_t payload_end_wide;
    uint32_t method_index;
    uint32_t block_count = 0u;
    uint32_t flags;
    uint32_t physical_block_size;
    size_t expected_bytes;
    size_t flags_offset;
    size_t i;
    const pak_entry_method_name *method;

    if (result == NULL) return PAK_PHYSICAL_STATUS_BAD_ARGUMENT;
    if (blocks_capacity > PAK_PHYSICAL_HEADER_MAX_BLOCKS) return PAK_PHYSICAL_STATUS_BAD_ARGUMENT;
    if (!ranges_disjoint(result, sizeof(*result), header_bytes, header_span_bytes) ||
        !ranges_disjoint(result, sizeof(*result), method_table,
                         method_table == NULL ? 0u : sizeof(*method_table)) ||
        !ranges_disjoint(result, sizeof(*result), limits,
                         limits == NULL ? 0u : sizeof(*limits)) ||
        !ranges_disjoint(result, sizeof(*result), blocks_out,
                         blocks_capacity * sizeof(*blocks_out)) ||
        !ranges_disjoint(blocks_out, blocks_capacity * sizeof(*blocks_out),
                         header_bytes, header_span_bytes) ||
        !ranges_disjoint(blocks_out, blocks_capacity * sizeof(*blocks_out), method_table,
                         method_table == NULL ? 0u : sizeof(*method_table)) ||
        !ranges_disjoint(blocks_out, blocks_capacity * sizeof(*blocks_out), limits,
                         limits == NULL ? 0u : sizeof(*limits))) {
        return PAK_PHYSICAL_STATUS_BAD_ARGUMENT;
    }

    initialize_result(result, PAK_PHYSICAL_STATUS_BAD_ARGUMENT,
                      PAK_PHYSICAL_REASON_BAD_ARGUMENT);
    if (blocks_out != NULL && blocks_capacity != 0u) {
        memset(blocks_out, 0, blocks_capacity * sizeof(*blocks_out));
    }
    if (header_bytes == NULL || method_table == NULL || limits == NULL ||
        (blocks_out == NULL && blocks_capacity != 0u)) {
        return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_BAD_ARGUMENT,
                    PAK_PHYSICAL_REASON_BAD_ARGUMENT);
    }
    if (!valid_limits(limits)) {
        return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_BAD_ARGUMENT,
                    PAK_PHYSICAL_REASON_BAD_LIMITS);
    }
    if (!valid_method_table(method_table)) {
        return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_BAD_ARGUMENT,
                    PAK_PHYSICAL_REASON_BAD_METHOD_TABLE);
    }
    if (header_span_bytes > PAK_PHYSICAL_HEADER_MAX_BYTES ||
        header_span_bytes > limits->max_header_bytes) {
        return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_RESOURCE_LIMIT,
                    PAK_PHYSICAL_REASON_HEADER_TOO_LARGE);
    }
    if (header_span_bytes < 53u) {
        return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                    PAK_PHYSICAL_REASON_HEADER_TRUNCATED);
    }

    stored_offset = load_le64(header_bytes);
    compressed_size = load_le64(header_bytes + 8u);
    uncompressed_size = load_le64(header_bytes + 16u);
    method_index = load_le32(header_bytes + 24u);
    if (stored_offset > INT64_MAX || compressed_size > INT64_MAX ||
        uncompressed_size > INT64_MAX || method_index > INT32_MAX) {
        return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                    PAK_PHYSICAL_REASON_NEGATIVE_FIELD);
    }
    if (compressed_size > limits->max_compressed_size ||
        uncompressed_size > limits->max_uncompressed_size) {
        return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_RESOURCE_LIMIT,
                    PAK_PHYSICAL_REASON_SIZE_LIMIT);
    }
    if (method_index >= method_table->count) {
        return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                    PAK_PHYSICAL_REASON_METHOD_INDEX);
    }
    method = &method_table->methods[method_index];
    if (is_name(method, "None", 4u)) {
        if (method_index != 0u) {
            return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_BAD_ARGUMENT,
                        PAK_PHYSICAL_REASON_BAD_METHOD_TABLE);
        }
        expected_bytes = 53u;
        flags_offset = 48u;
    } else if (is_name(method, "Zlib", 4u)) {
        uint32_t raw_block_count;
        if (header_span_bytes < 52u) {
            return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                        PAK_PHYSICAL_REASON_HEADER_TRUNCATED);
        }
        raw_block_count = load_le32(header_bytes + 48u);
        if (raw_block_count > INT32_MAX) {
            return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                        PAK_PHYSICAL_REASON_NEGATIVE_FIELD);
        }
        if (raw_block_count == 0u) {
            return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                        PAK_PHYSICAL_REASON_BLOCK_COUNT);
        }
        if (raw_block_count > PAK_PHYSICAL_HEADER_MAX_BLOCKS ||
            raw_block_count > limits->max_blocks) {
            return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_RESOURCE_LIMIT,
                        PAK_PHYSICAL_REASON_BLOCK_COUNT);
        }
        block_count = raw_block_count;
        expected_bytes = 53u + 4u + (size_t)block_count * 16u;
        flags_offset = 52u + (size_t)block_count * 16u;
        if (expected_bytes > PAK_PHYSICAL_HEADER_MAX_BYTES ||
            expected_bytes > limits->max_header_bytes) {
            return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_RESOURCE_LIMIT,
                        PAK_PHYSICAL_REASON_HEADER_TOO_LARGE);
        }
        if (header_span_bytes < expected_bytes) {
            return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                        PAK_PHYSICAL_REASON_HEADER_TRUNCATED);
        }
        if (header_span_bytes > expected_bytes) {
            return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                        PAK_PHYSICAL_REASON_HEADER_TRAILING_BYTES);
        }
        if (block_count > blocks_capacity || blocks_out == NULL) {
            return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_RESOURCE_LIMIT,
                        PAK_PHYSICAL_REASON_BLOCK_CAPACITY);
        }
    } else {
        return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_UNSUPPORTED,
                    PAK_PHYSICAL_REASON_UNSUPPORTED_CODEC);
    }

    if (header_span_bytes < expected_bytes) {
        return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                    PAK_PHYSICAL_REASON_HEADER_TRUNCATED);
    }
    if (header_span_bytes > expected_bytes) {
        return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                    PAK_PHYSICAL_REASON_HEADER_TRAILING_BYTES);
    }
    flags = header_bytes[flags_offset];
    if (flags != 0u) {
        return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_UNSUPPORTED,
                    PAK_PHYSICAL_REASON_UNSUPPORTED_FLAGS);
    }
    physical_block_size = load_le32(header_bytes + flags_offset + 1u);
    if (physical_block_size > limits->max_physical_block_size) {
        return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_RESOURCE_LIMIT,
                    PAK_PHYSICAL_REASON_SIZE_LIMIT);
    }
    if (block_count != 0u) {
        if (header_span_bytes > UINT64_MAX - compressed_size) {
            return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                        PAK_PHYSICAL_REASON_BLOCK_RANGE);
        }
        payload_end_wide = (uint64_t)header_span_bytes + compressed_size;
        if (payload_end_wide > INT64_MAX) {
            return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                        PAK_PHYSICAL_REASON_BLOCK_RANGE);
        }
        payload_end = payload_end_wide;
        for (i = 0u; i < block_count; ++i) {
            const size_t pair_offset = 52u + i * 16u;
            const uint64_t start = load_le64(header_bytes + pair_offset);
            const uint64_t end = load_le64(header_bytes + pair_offset + 8u);
            size_t j;
            if (start > INT64_MAX || end > INT64_MAX) {
                return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                            PAK_PHYSICAL_REASON_NEGATIVE_FIELD);
            }
            if (start < expected_bytes || start >= end || end > payload_end) {
                return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                            PAK_PHYSICAL_REASON_BLOCK_RANGE);
            }
            for (j = 0u; j < i; ++j) {
                const uint64_t prior_start = pending.blocks[j].relative_start;
                const uint64_t prior_end = pending.blocks[j].relative_end;
                if (start < prior_end && prior_start < end) {
                    return fail(result, blocks_out, blocks_capacity, PAK_PHYSICAL_STATUS_MALFORMED,
                                PAK_PHYSICAL_REASON_BLOCK_OVERLAP);
                }
            }
            pending.blocks[i].relative_start = start;
            pending.blocks[i].relative_end = end;
        }
    }

    memset(&pending.result, 0, sizeof(pending.result));
    pending.result.struct_size = sizeof(pending.result);
    pending.result.abi_version = PAK_PHYSICAL_HEADER_ABI_VERSION;
    pending.result.status = PAK_PHYSICAL_STATUS_OK;
    pending.result.reason = PAK_PHYSICAL_REASON_NONE;
    pending.result.record_complete = 1u;
    pending.result.bytes_consumed = (uint32_t)expected_bytes;
    pending.result.header_bytes = (uint32_t)expected_bytes;
    pending.result.method_index = method_index;
    pending.result.method_name_bytes = method->length;
    pending.result.block_count = block_count;
    pending.result.flags = flags;
    pending.result.physical_block_size = physical_block_size;
    pending.result.stored_offset = stored_offset;
    pending.result.compressed_size = compressed_size;
    pending.result.uncompressed_size = uncompressed_size;
    memcpy(pending.result.method_name, method->bytes, method->length);
    memcpy(pending.result.sha1, header_bytes + 28u, sizeof(pending.result.sha1));
    if (block_count != 0u) {
        memcpy(blocks_out, pending.blocks, (size_t)block_count * sizeof(*blocks_out));
    }
    memcpy(result, &pending.result, sizeof(*result));
    return PAK_PHYSICAL_STATUS_OK;
}
