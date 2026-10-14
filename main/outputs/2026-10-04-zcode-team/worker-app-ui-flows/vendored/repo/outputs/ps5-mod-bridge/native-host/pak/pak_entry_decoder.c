#include "pak_entry_decoder.h"

#include <limits.h>
#include <string.h>

typedef struct entry_cursor {
    const uint8_t *bytes;
    size_t size;
    size_t start;
    size_t position;
    uint32_t max_record_bytes;
} entry_cursor;

static uint32_t load_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t load_le64(const uint8_t *p)
{
    return (uint64_t)load_le32(p) | ((uint64_t)load_le32(p + 4) << 32);
}

static int add_u64(uint64_t left, uint64_t right, uint64_t *sum)
{
    if (right > UINT64_MAX - left) return 0;
    *sum = left + right;
    return 1;
}

static pak_entry_status take_bytes(entry_cursor *cursor, size_t count,
                                   const uint8_t **bytes, pak_entry_reason *reason)
{
    size_t used;
    if (cursor->position < cursor->start || cursor->position > cursor->size) {
        *reason = PAK_ENTRY_REASON_RECORD_TRUNCATED;
        return PAK_ENTRY_STATUS_MALFORMED;
    }
    used = cursor->position - cursor->start;
    if (used > cursor->max_record_bytes || count > cursor->max_record_bytes - used) {
        *reason = PAK_ENTRY_REASON_RECORD_TOO_LARGE;
        return PAK_ENTRY_STATUS_RESOURCE_LIMIT;
    }
    if (count > cursor->size - cursor->position) {
        *reason = PAK_ENTRY_REASON_RECORD_TRUNCATED;
        return PAK_ENTRY_STATUS_MALFORMED;
    }
    if (bytes != NULL) *bytes = cursor->bytes + cursor->position;
    cursor->position += count;
    return PAK_ENTRY_STATUS_OK;
}

static pak_entry_status read_u32(entry_cursor *cursor, uint32_t *value,
                                 pak_entry_reason *reason)
{
    const uint8_t *bytes;
    pak_entry_status status = take_bytes(cursor, 4u, &bytes, reason);
    if (status == PAK_ENTRY_STATUS_OK) *value = load_le32(bytes);
    return status;
}

static pak_entry_status read_u64(entry_cursor *cursor, uint64_t *value,
                                 pak_entry_reason *reason)
{
    const uint8_t *bytes;
    pak_entry_status status = take_bytes(cursor, 8u, &bytes, reason);
    if (status == PAK_ENTRY_STATUS_OK) *value = load_le64(bytes);
    return status;
}

static int valid_method_table(const pak_entry_method_table_v1 *table)
{
    uint32_t i;
    if (table == NULL || table->struct_size != sizeof(*table) ||
        table->abi_version != PAK_ENTRY_DECODER_ABI_VERSION ||
        table->reserved != 0u || table->count == 0u ||
        table->count > PAK_ENTRY_DECODER_MAX_METHODS ||
        table->methods[0].length != 4u ||
        memcmp(table->methods[0].bytes, "None", 4u) != 0 ||
        table->methods[0].reserved[0] != 0u ||
        table->methods[0].reserved[1] != 0u ||
        table->methods[0].reserved[2] != 0u) {
        return 0;
    }
    for (i = 0; i < table->count; ++i) {
        uint32_t j;
        const pak_entry_method_name *method = &table->methods[i];
        if (method->length == 0u || method->length > PAK_ENTRY_DECODER_MAX_NAME_BYTES ||
            method->reserved[0] != 0u || method->reserved[1] != 0u || method->reserved[2] != 0u) {
            return 0;
        }
        for (j = 0; j < method->length; ++j) {
            if (method->bytes[j] < 0x21u || method->bytes[j] > 0x7eu) return 0;
        }
    }
    return 1;
}

static int valid_limits(const pak_entry_limits_v1 *limits)
{
    if (limits == NULL || limits->struct_size != sizeof(*limits) ||
        limits->abi_version != PAK_ENTRY_DECODER_ABI_VERSION ||
        limits->max_blob_bytes == 0u ||
        limits->max_blob_bytes > PAK_ENTRY_DECODER_MAX_BLOB_BYTES ||
        limits->max_record_bytes < 4u ||
        limits->max_record_bytes > limits->max_blob_bytes ||
        limits->max_blocks > PAK_ENTRY_DECODER_MAX_BLOCKS ||
        limits->max_block_size > UINT32_MAX ||
        limits->max_compressed_size > INT64_MAX ||
        limits->max_uncompressed_size > INT64_MAX ||
        limits->max_total_block_bytes > INT64_MAX) {
        return 0;
    }
    return 1;
}

static void fail_result(pak_entry_result_v1 *result, pak_entry_status status,
                        pak_entry_reason reason, pak_entry_block_v1 *blocks_out,
                        uint32_t blocks_written)
{
    if (blocks_out != NULL && blocks_written != 0u) {
        memset(blocks_out, 0, (size_t)blocks_written * sizeof(*blocks_out));
    }
    memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result);
    result->abi_version = PAK_ENTRY_DECODER_ABI_VERSION;
    result->status = status;
    result->reason = reason;
}

pak_entry_status pak_entry_decode_v11(
    const uint8_t *encoded_blob,
    size_t encoded_blob_bytes,
    size_t record_offset,
    uint64_t logical_archive_size,
    const pak_entry_method_table_v1 *method_table,
    const pak_entry_limits_v1 *limits,
    pak_entry_block_v1 *blocks_out,
    size_t blocks_capacity,
    pak_entry_result_v1 *result)
{
    entry_cursor cursor;
    pak_entry_result_v1 pending;
    pak_entry_status status;
    pak_entry_reason reason = PAK_ENTRY_REASON_NONE;
    uint32_t bitfield, block_count, method_index, block_code;
    uint32_t block_size_word = 0u;
    uint32_t block_written = 0u;
    uint64_t archive_offset, uncompressed_size, compressed_size;
    uint64_t compression_block_size = 0u;
    uint64_t header_bytes, data_offset, data_end;
    uint64_t total_block_bytes = 0u;
    uint64_t uncompressed_cursor = 0u;
    uint32_t i;

    if (result == NULL) return PAK_ENTRY_STATUS_BAD_ARGUMENT;
    memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result);
    result->abi_version = PAK_ENTRY_DECODER_ABI_VERSION;
    result->status = PAK_ENTRY_STATUS_BAD_ARGUMENT;
    result->reason = PAK_ENTRY_REASON_BAD_ARGUMENT;

    if ((encoded_blob == NULL && encoded_blob_bytes != 0u) ||
        (blocks_out == NULL && blocks_capacity != 0u) ||
        (blocks_out != NULL && blocks_capacity > SIZE_MAX / sizeof(*blocks_out))) {
        return PAK_ENTRY_STATUS_BAD_ARGUMENT;
    }
    if (!valid_limits(limits)) {
        result->reason = PAK_ENTRY_REASON_BAD_LIMITS;
        return PAK_ENTRY_STATUS_BAD_ARGUMENT;
    }
    if (!valid_method_table(method_table)) {
        result->reason = PAK_ENTRY_REASON_BAD_METHOD_TABLE;
        return PAK_ENTRY_STATUS_BAD_ARGUMENT;
    }
    if (encoded_blob_bytes > PAK_ENTRY_DECODER_MAX_BLOB_BYTES ||
        encoded_blob_bytes > limits->max_blob_bytes) {
        result->status = PAK_ENTRY_STATUS_RESOURCE_LIMIT;
        result->reason = PAK_ENTRY_REASON_BLOB_TOO_LARGE;
        return PAK_ENTRY_STATUS_RESOURCE_LIMIT;
    }
    if (encoded_blob_bytes == 0u || record_offset > encoded_blob_bytes ||
        logical_archive_size == 0u) {
        result->status = PAK_ENTRY_STATUS_MALFORMED;
        result->reason = PAK_ENTRY_REASON_RECORD_TRUNCATED;
        return PAK_ENTRY_STATUS_MALFORMED;
    }

    memset(&pending, 0, sizeof(pending));
    cursor.bytes = encoded_blob;
    cursor.size = encoded_blob_bytes;
    cursor.start = record_offset;
    cursor.position = record_offset;
    cursor.max_record_bytes = limits->max_record_bytes;

    status = read_u32(&cursor, &bitfield, &reason);
    if (status != PAK_ENTRY_STATUS_OK) goto fail;

    block_code = bitfield & 0x3fu;
    block_count = (bitfield >> 6) & 0xffffu;
    method_index = (bitfield >> 23) & 0x3fu;
    if ((bitfield & (1u << 22)) != 0u) {
        status = PAK_ENTRY_STATUS_UNSUPPORTED;
        reason = PAK_ENTRY_REASON_ENCRYPTED;
        goto fail;
    }
    if (method_index >= method_table->count) {
        status = PAK_ENTRY_STATUS_MALFORMED;
        reason = PAK_ENTRY_REASON_METHOD_INDEX;
        goto fail;
    }
    if (block_count > limits->max_blocks) {
        status = PAK_ENTRY_STATUS_RESOURCE_LIMIT;
        reason = PAK_ENTRY_REASON_BLOCK_COUNT;
        goto fail;
    }
    if ((size_t)block_count > blocks_capacity ||
        (block_count != 0u && blocks_out == NULL)) {
        status = PAK_ENTRY_STATUS_RESOURCE_LIMIT;
        reason = PAK_ENTRY_REASON_BLOCK_CAPACITY;
        goto fail;
    }

    if (block_code == 0x3fu) {
        status = read_u32(&cursor, &block_size_word, &reason);
        if (status != PAK_ENTRY_STATUS_OK) goto fail;
        compression_block_size = block_size_word;
    } else {
        compression_block_size = (uint64_t)block_code << 11;
    }

    if ((bitfield & (1u << 31)) != 0u) {
        uint32_t value32;
        status = read_u32(&cursor, &value32, &reason);
        if (status != PAK_ENTRY_STATUS_OK) goto fail;
        archive_offset = value32;
    } else {
        status = read_u64(&cursor, &archive_offset, &reason);
        if (status != PAK_ENTRY_STATUS_OK) goto fail;
        if (archive_offset > INT64_MAX) {
            status = PAK_ENTRY_STATUS_MALFORMED;
            reason = PAK_ENTRY_REASON_NEGATIVE_FIELD;
            goto fail;
        }
    }

    if ((bitfield & (1u << 30)) != 0u) {
        uint32_t value32;
        status = read_u32(&cursor, &value32, &reason);
        if (status != PAK_ENTRY_STATUS_OK) goto fail;
        uncompressed_size = value32;
    } else {
        status = read_u64(&cursor, &uncompressed_size, &reason);
        if (status != PAK_ENTRY_STATUS_OK) goto fail;
        if (uncompressed_size > INT64_MAX) {
            status = PAK_ENTRY_STATUS_MALFORMED;
            reason = PAK_ENTRY_REASON_NEGATIVE_FIELD;
            goto fail;
        }
    }

    if (method_index == 0u) {
        compressed_size = uncompressed_size;
    } else if ((bitfield & (1u << 29)) != 0u) {
        uint32_t value32;
        status = read_u32(&cursor, &value32, &reason);
        if (status != PAK_ENTRY_STATUS_OK) goto fail;
        compressed_size = value32;
    } else {
        status = read_u64(&cursor, &compressed_size, &reason);
        if (status != PAK_ENTRY_STATUS_OK) goto fail;
        if (compressed_size > INT64_MAX) {
            status = PAK_ENTRY_STATUS_MALFORMED;
            reason = PAK_ENTRY_REASON_NEGATIVE_FIELD;
            goto fail;
        }
    }

    if (compressed_size > limits->max_compressed_size ||
        uncompressed_size > limits->max_uncompressed_size) {
        status = PAK_ENTRY_STATUS_RESOURCE_LIMIT;
        reason = PAK_ENTRY_REASON_SIZE_LIMIT;
        goto fail;
    }
    if (block_count == 1u) {
        compression_block_size = uncompressed_size;
    } else if (block_count == 0u) {
        compression_block_size = 0u;
    }
    if (compression_block_size > limits->max_block_size) {
        status = PAK_ENTRY_STATUS_RESOURCE_LIMIT;
        reason = PAK_ENTRY_REASON_BLOCK_SIZE;
        goto fail;
    }

    header_bytes = 3u * 8u + 2u * 4u + 1u + 20u;
    if (method_index != 0u) {
        uint64_t block_table_bytes = 4u + (uint64_t)block_count * 2u * 8u;
        if (!add_u64(header_bytes, block_table_bytes, &header_bytes)) {
            status = PAK_ENTRY_STATUS_MALFORMED;
            reason = PAK_ENTRY_REASON_BAD_RANGE;
            goto fail;
        }
    }
    if (header_bytes > UINT32_MAX || !add_u64(archive_offset, header_bytes, &data_offset) ||
        data_offset > INT64_MAX || data_offset > logical_archive_size) {
        status = PAK_ENTRY_STATUS_MALFORMED;
        reason = PAK_ENTRY_REASON_BAD_RANGE;
        goto fail;
    }

    if (block_count == 1u) {
        if (!add_u64(compressed_size, 0u, &total_block_bytes) ||
            total_block_bytes > limits->max_total_block_bytes ||
            !add_u64(data_offset, compressed_size, &data_end) ||
            data_end > INT64_MAX || data_end > logical_archive_size) {
            status = total_block_bytes > limits->max_total_block_bytes
                         ? PAK_ENTRY_STATUS_RESOURCE_LIMIT : PAK_ENTRY_STATUS_MALFORMED;
            reason = status == PAK_ENTRY_STATUS_RESOURCE_LIMIT
                         ? PAK_ENTRY_REASON_SIZE_LIMIT : PAK_ENTRY_REASON_BAD_RANGE;
            goto fail;
        }
        blocks_out[0].compressed_offset = data_offset;
        blocks_out[0].compressed_size = compressed_size;
        blocks_out[0].uncompressed_offset = 0u;
        blocks_out[0].uncompressed_size = uncompressed_size;
        block_written = 1u;
    } else if (block_count > 1u) {
        uint64_t expected_blocks;
        uint64_t compressed_cursor = data_offset;
        if (compression_block_size == 0u || uncompressed_size == 0u) {
            status = PAK_ENTRY_STATUS_MALFORMED;
            reason = PAK_ENTRY_REASON_BLOCK_SIZE;
            goto fail;
        }
        expected_blocks = 1u + (uncompressed_size - 1u) / compression_block_size;
        if (expected_blocks != block_count) {
            status = PAK_ENTRY_STATUS_MALFORMED;
            reason = PAK_ENTRY_REASON_BLOCK_COUNT;
            goto fail;
        }
        for (i = 0; i < block_count; ++i) {
            uint32_t length;
            uint64_t end;
            uint64_t block_uncompressed_size;
            status = read_u32(&cursor, &length, &reason);
            if (status != PAK_ENTRY_STATUS_OK) goto fail;
            if (!add_u64(total_block_bytes, length, &total_block_bytes) ||
                total_block_bytes > limits->max_total_block_bytes) {
                status = PAK_ENTRY_STATUS_RESOURCE_LIMIT;
                reason = PAK_ENTRY_REASON_SIZE_LIMIT;
                goto fail;
            }
            if (!add_u64(compressed_cursor, length, &end) ||
                end > INT64_MAX || end > logical_archive_size) {
                status = PAK_ENTRY_STATUS_MALFORMED;
                reason = PAK_ENTRY_REASON_BAD_RANGE;
                goto fail;
            }
            block_uncompressed_size = uncompressed_size - uncompressed_cursor;
            if (block_uncompressed_size > compression_block_size) {
                block_uncompressed_size = compression_block_size;
            }
            blocks_out[i].compressed_offset = compressed_cursor;
            blocks_out[i].compressed_size = length;
            blocks_out[i].uncompressed_offset = uncompressed_cursor;
            blocks_out[i].uncompressed_size = block_uncompressed_size;
            block_written++;
            compressed_cursor = end;
            uncompressed_cursor += block_uncompressed_size;
        }
        if (total_block_bytes != compressed_size) {
            status = PAK_ENTRY_STATUS_MALFORMED;
            reason = PAK_ENTRY_REASON_BLOCK_SUM;
            goto fail;
        }
    } else {
        if (compressed_size > limits->max_total_block_bytes) {
            status = PAK_ENTRY_STATUS_RESOURCE_LIMIT;
            reason = PAK_ENTRY_REASON_SIZE_LIMIT;
            goto fail;
        }
        if (!add_u64(data_offset, compressed_size, &data_end) ||
            data_end > INT64_MAX || data_end > logical_archive_size) {
            status = PAK_ENTRY_STATUS_MALFORMED;
            reason = PAK_ENTRY_REASON_BAD_RANGE;
            goto fail;
        }
    }

    if (cursor.position - cursor.start > limits->max_record_bytes ||
        cursor.position - cursor.start > UINT32_MAX) {
        status = PAK_ENTRY_STATUS_RESOURCE_LIMIT;
        reason = PAK_ENTRY_REASON_RECORD_TOO_LARGE;
        goto fail;
    }

    pending.struct_size = sizeof(pending);
    pending.abi_version = PAK_ENTRY_DECODER_ABI_VERSION;
    pending.status = PAK_ENTRY_STATUS_OK;
    pending.reason = PAK_ENTRY_REASON_NONE;
    pending.record_complete = 1u;
    pending.bytes_consumed = (uint32_t)(cursor.position - cursor.start);
    pending.header_bytes = (uint32_t)header_bytes;
    pending.method_index = method_index;
    pending.method_name_bytes = method_table->methods[method_index].length;
    pending.block_count = block_count;
    memcpy(pending.method_name, method_table->methods[method_index].bytes,
           pending.method_name_bytes);
    pending.archive_offset = archive_offset;
    pending.data_offset = data_offset;
    pending.compressed_size = compressed_size;
    pending.uncompressed_size = uncompressed_size;
    pending.compression_block_size = compression_block_size;
    *result = pending;
    return PAK_ENTRY_STATUS_OK;

fail:
    fail_result(result, status, reason, blocks_out, block_written);
    return status;
}
