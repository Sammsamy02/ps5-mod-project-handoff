#include "pak_entry_pipeline.h"

#include <limits.h>
#include <string.h>

typedef struct pipeline_capture {
    const pak_source_v1 *base_source;
    uint8_t *primary_scratch;
    size_t primary_capacity;
    uint8_t footer[PAK_V11_FOOTER_BYTES];
    uint64_t footer_offset;
    uint64_t primary_offset;
    uint64_t primary_size;
    uint64_t requested_bytes;
    uint32_t read_calls;
    uint64_t footer_copied;
    uint64_t primary_copied;
    uint32_t primary_range_ready;
    uint32_t capture_error;
} pipeline_capture;

typedef struct byte_cursor {
    const uint8_t *bytes;
    size_t size;
    size_t position;
} byte_cursor;

static uint32_t load_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t load_le64(const uint8_t *p)
{
    return (uint64_t)load_le32(p) | ((uint64_t)load_le32(p + 4) << 32);
}

static int add_u64(uint64_t a, uint64_t b, uint64_t *out)
{
    if (b > UINT64_MAX - a) return 0;
    *out = a + b;
    return 1;
}

static int capture_region(pipeline_capture *capture,
                          uint64_t request_offset,
                          const uint8_t *request_bytes,
                          uint32_t actual,
                          uint64_t region_offset,
                          uint64_t region_size,
                          uint8_t *region_buffer,
                          uint64_t *region_copied)
{
    uint64_t request_end, region_end, overlap_start, overlap_end, expected;
    uint64_t bytes;

    if (actual == 0u) return 1;
    if (!add_u64(request_offset, actual, &request_end) ||
        !add_u64(region_offset, region_size, &region_end) ||
        !add_u64(region_offset, *region_copied, &expected)) {
        capture->capture_error = 1u;
        return 0;
    }
    overlap_start = request_offset > region_offset ? request_offset : region_offset;
    overlap_end = request_end < region_end ? request_end : region_end;
    if (overlap_start >= overlap_end) return 1;
    if (overlap_start != expected || overlap_start < request_offset) {
        capture->capture_error = 1u;
        return 0;
    }
    bytes = overlap_end - overlap_start;
    if (bytes > SIZE_MAX || overlap_start - request_offset > actual ||
        bytes > (uint64_t)actual - (overlap_start - request_offset) ||
        *region_copied > region_size || bytes > region_size - *region_copied) {
        capture->capture_error = 1u;
        return 0;
    }
    memcpy(region_buffer + (size_t)*region_copied,
           request_bytes + (size_t)(overlap_start - request_offset),
           (size_t)bytes);
    *region_copied += bytes;
    return 1;
}

static void capture_footer_range(pipeline_capture *capture)
{
    uint64_t primary_end;
    if (capture->footer_copied != PAK_V11_FOOTER_BYTES) return;
    capture->primary_offset = load_le64(capture->footer + 25u);
    capture->primary_size = load_le64(capture->footer + 33u);
    if ((capture->primary_offset & (1ULL << 63)) != 0u ||
        (capture->primary_size & (1ULL << 63)) != 0u ||
        capture->primary_size == 0u ||
        capture->primary_size > capture->primary_capacity ||
        !add_u64(capture->primary_offset, capture->primary_size, &primary_end) ||
        primary_end > capture->footer_offset) {
        return;
    }
    capture->primary_range_ready = 1u;
}

static pak_provider_status pipeline_read_at(void *context,
                                            uint64_t offset,
                                            uint8_t *destination,
                                            uint32_t requested,
                                            uint32_t *actual)
{
    pipeline_capture *capture = (pipeline_capture *)context;
    pak_provider_status status;

    if (capture == NULL || capture->base_source == NULL ||
        capture->base_source->read_at == NULL || destination == NULL || actual == NULL) {
        if (capture != NULL) capture->capture_error = 1u;
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    if (capture->read_calls == UINT32_MAX ||
        capture->requested_bytes > UINT64_MAX - requested) {
        capture->capture_error = 1u;
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    capture->read_calls++;
    capture->requested_bytes += requested;
    status = capture->base_source->read_at(capture->base_source->context,
                                            offset, destination, requested, actual);
    if (status != PAK_PROVIDER_OK || *actual > requested) return status;

    if (!capture_region(capture, offset, destination, *actual,
                        capture->footer_offset, PAK_V11_FOOTER_BYTES,
                        capture->footer, &capture->footer_copied)) {
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    capture_footer_range(capture);
    if (capture->primary_range_ready != 0u) {
        uint64_t before = capture->primary_copied;
        if (!capture_region(capture, offset, destination, *actual,
                            capture->primary_offset, capture->primary_size,
                            capture->primary_scratch, &capture->primary_copied)) {
            return PAK_PROVIDER_CONTRACT_ERROR;
        }
        if (capture->primary_copied != before &&
            capture->primary_copied > capture->primary_capacity) {
            capture->capture_error = 1u;
            return PAK_PROVIDER_CONTRACT_ERROR;
        }
    }
    if (capture->capture_error != 0u) return PAK_PROVIDER_CONTRACT_ERROR;
    return status;
}

static int cursor_take(byte_cursor *cursor, size_t amount, const uint8_t **out)
{
    if (cursor->position > cursor->size || amount > cursor->size - cursor->position) return 0;
    if (out != NULL) *out = cursor->bytes + cursor->position;
    cursor->position += amount;
    return 1;
}

static int cursor_u32(byte_cursor *cursor, uint32_t *value)
{
    const uint8_t *p;
    if (!cursor_take(cursor, 4u, &p)) return 0;
    *value = load_le32(p);
    return 1;
}

static int cursor_i32(byte_cursor *cursor, int64_t *value)
{
    const uint8_t *p;
    uint32_t word;
    if (!cursor_take(cursor, 4u, &p)) return 0;
    word = load_le32(p);
    *value = (word & 0x80000000u) == 0u ? (int64_t)word : (int64_t)word - 0x100000000LL;
    return 1;
}

static int cursor_u64(byte_cursor *cursor, uint64_t *value)
{
    const uint8_t *p;
    if (!cursor_take(cursor, 8u, &p)) return 0;
    *value = load_le64(p);
    return 1;
}

static int skip_fstring(byte_cursor *cursor)
{
    int64_t signed_length;
    uint64_t serialized_bytes;
    if (!cursor_i32(cursor, &signed_length) || signed_length == INT32_MIN) return 0;
    if (signed_length > 0) {
        serialized_bytes = (uint64_t)signed_length;
    } else if (signed_length < 0) {
        uint64_t units = (uint64_t)(-signed_length);
        if (units > UINT64_MAX / 2u) return 0;
        serialized_bytes = units * 2u;
    } else {
        serialized_bytes = 0u;
    }
    if (serialized_bytes > SIZE_MAX) return 0;
    return cursor_take(cursor, (size_t)serialized_bytes, NULL);
}

static int cursor_secondary(byte_cursor *cursor, const pak_secondary_index *expected)
{
    uint64_t offset, size;
    const uint8_t *sha1;
    if (!cursor_u64(cursor, &offset) || !cursor_u64(cursor, &size) ||
        !cursor_take(cursor, 20u, &sha1)) {
        return 0;
    }
    return offset == expected->offset && size == expected->size &&
           memcmp(sha1, expected->sha1, 20u) == 0;
}

static int find_encoded_span(const uint8_t *primary_bytes,
                             size_t primary_size,
                             const pak_primary_result_v1 *primary,
                             uint32_t *span_offset,
                             uint32_t *span_size)
{
    byte_cursor cursor;
    int64_t value;
    uint64_t seed;
    uint32_t present;
    const uint8_t *span;

    cursor.bytes = primary_bytes;
    cursor.size = primary_size;
    cursor.position = 0u;
    if (!skip_fstring(&cursor) || !cursor_i32(&cursor, &value) ||
        value < 0 || (uint64_t)value != primary->declared_file_count ||
        !cursor_u64(&cursor, &seed) || seed != primary->path_hash_seed ||
        !cursor_u32(&cursor, &present) || present != 1u ||
        !cursor_secondary(&cursor, &primary->path_hash_index) ||
        !cursor_u32(&cursor, &present) || present != 1u ||
        !cursor_secondary(&cursor, &primary->directory_index) ||
        !cursor_i32(&cursor, &value) || value < 0 || (uint64_t)value > UINT32_MAX) {
        return 0;
    }
    if ((size_t)value > cursor.size - cursor.position) return 0;
    *span_offset = (uint32_t)cursor.position;
    *span_size = (uint32_t)value;
    if (!cursor_take(&cursor, (size_t)value, &span) ||
        !cursor_i32(&cursor, &value) || value != 0 ||
        cursor.position != cursor.size) {
        return 0;
    }
    (void)span;
    return 1;
}

static int set_method_name(pak_entry_method_name *out, const uint8_t *bytes, uint32_t length)
{
    uint32_t i;
    if (length == 0u || length > PAK_ENTRY_DECODER_MAX_NAME_BYTES) return 0;
    for (i = 0u; i < length; ++i) {
        if (bytes[i] < 0x21u || bytes[i] > 0x7eu) return 0;
    }
    memcpy(out->bytes, bytes, length);
    out->length = (uint8_t)length;
    return 1;
}

static int build_method_table(const pak_primary_result_v1 *primary,
                              pak_entry_method_table_v1 *table)
{
    uint32_t i;
    memset(table, 0, sizeof(*table));
    table->struct_size = sizeof(*table);
    table->abi_version = PAK_ENTRY_DECODER_ABI_VERSION;
    table->count = 1u;
    if (!set_method_name(&table->methods[0], (const uint8_t *)"None", 4u)) return 0;
    for (i = 0u; i < PAK_READER_COMPRESSION_SLOTS; ++i) {
        const uint8_t *name = primary->compression_names[i];
        uint32_t length = 0u;
        while (length < PAK_READER_COMPRESSION_NAME_BYTES && name[length] != 0u) length++;
        if (length == 0u) continue;
        if (length == 4u && memcmp(name, "None", 4u) == 0) return -1;
        if (table->count >= PAK_ENTRY_DECODER_MAX_METHODS ||
            !set_method_name(&table->methods[table->count], name, length)) return 0;
        table->count++;
    }
    return 1;
}

static void initialize_result(pak_entry_pipeline_result_v1 *result)
{
    memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result);
    result->abi_version = PAK_ENTRY_PIPELINE_ABI_VERSION;
    result->status = PAK_ENTRY_PIPELINE_BAD_ARGUMENT;
    result->reason = PAK_ENTRY_PIPELINE_REASON_BAD_ARGUMENT;
    result->reader_status = PAK_STATUS_BAD_ARGUMENT;
    result->reader_reason = PAK_REASON_BAD_ARGUMENT;
    result->entry_status = PAK_ENTRY_STATUS_BAD_ARGUMENT;
    result->entry_reason = PAK_ENTRY_REASON_BAD_ARGUMENT;
    result->primary.struct_size = sizeof(result->primary);
    result->primary.abi_version = PAK_READER_ABI_VERSION;
    result->primary.status = PAK_STATUS_INTERNAL_ERROR;
    result->primary.reason = PAK_REASON_BAD_ARGUMENT;
    result->entry.struct_size = sizeof(result->entry);
    result->entry.abi_version = PAK_ENTRY_DECODER_ABI_VERSION;
    result->entry.status = PAK_ENTRY_STATUS_BAD_ARGUMENT;
    result->entry.reason = PAK_ENTRY_REASON_BAD_ARGUMENT;
}

static void clear_captured_primary(pipeline_capture *capture)
{
    if (capture->primary_scratch != NULL && capture->primary_copied != 0u &&
        capture->primary_copied <= capture->primary_capacity) {
        memset(capture->primary_scratch, 0, (size_t)capture->primary_copied);
    }
}

pak_entry_pipeline_status pak_read_v11_one_entry(
    const pak_source_v1 *source,
    const pak_limits_v1 *reader_limits,
    uint8_t *reader_workspace,
    size_t reader_workspace_bytes,
    uint8_t *primary_scratch,
    size_t primary_scratch_bytes,
    size_t selected_record_offset,
    const pak_entry_limits_v1 *entry_limits,
    pak_entry_block_v1 *blocks_out,
    size_t blocks_capacity,
    pak_entry_pipeline_result_v1 *result)
{
    pipeline_capture capture;
    pak_source_v1 proxy_source;
    pak_limits_v1 bounded_reader_limits;
    pak_primary_result_v1 primary_result;
    pak_entry_method_table_v1 method_table;
    pak_entry_pipeline_status pipeline_status;
    pak_entry_status entry_status;
    uint32_t span_offset = 0u, span_size = 0u;
    int method_table_status;

    if (result == NULL) return PAK_ENTRY_PIPELINE_BAD_ARGUMENT;
    initialize_result(result);
    result->selected_record_offset = selected_record_offset;
    if (source == NULL || reader_limits == NULL || reader_workspace == NULL ||
        primary_scratch == NULL || entry_limits == NULL ||
        (blocks_out == NULL && blocks_capacity != 0u) ||
        (blocks_out != NULL && blocks_capacity > SIZE_MAX / sizeof(*blocks_out)) ||
        primary_scratch_bytes == 0u || reader_limits->struct_size < sizeof(*reader_limits) ||
        reader_limits->abi_version != PAK_READER_ABI_VERSION) {
        return PAK_ENTRY_PIPELINE_BAD_ARGUMENT;
    }

    memset(&capture, 0, sizeof(capture));
    capture.base_source = source;
    capture.primary_scratch = primary_scratch;
    capture.primary_capacity = primary_scratch_bytes > PAK_READER_MAX_PRIMARY_BYTES
                                   ? PAK_READER_MAX_PRIMARY_BYTES
                                   : primary_scratch_bytes;
    if (source->logical_size >= PAK_V11_FOOTER_BYTES) {
        capture.footer_offset = source->logical_size - PAK_V11_FOOTER_BYTES;
    }

    bounded_reader_limits = *reader_limits;
    if (bounded_reader_limits.max_primary_bytes != 0u &&
        bounded_reader_limits.max_primary_bytes <= PAK_READER_MAX_PRIMARY_BYTES &&
        bounded_reader_limits.max_primary_bytes > capture.primary_capacity) {
        bounded_reader_limits.max_primary_bytes = (uint32_t)capture.primary_capacity;
    }

    proxy_source = *source;
    proxy_source.struct_size = sizeof(proxy_source);
    proxy_source.context = &capture;
    proxy_source.read_at = pipeline_read_at;
    memset(&primary_result, 0, sizeof(primary_result));
    (void)pak_read_v11_primary(&proxy_source, &bounded_reader_limits,
                               reader_workspace, reader_workspace_bytes,
                               &primary_result);

    result->aggregate_read_bytes = capture.requested_bytes;
    result->aggregate_read_calls = capture.read_calls;
    result->reader_status = primary_result.status;
    result->reader_reason = primary_result.reason;
    result->primary_sha1_checked = primary_result.primary_sha1_checked;
    result->primary_sha1_matches = primary_result.primary_sha1_matches;
    result->primary_metadata_complete = primary_result.primary_metadata_complete;

    if (primary_result.status != PAK_STATUS_OK ||
        primary_result.primary_metadata_complete != 1u ||
        primary_result.primary_sha1_checked != 1u ||
        primary_result.primary_sha1_matches != 1u) {
        clear_captured_primary(&capture);
        result->status = PAK_ENTRY_PIPELINE_READER_REJECTED;
        result->reason = PAK_ENTRY_PIPELINE_REASON_NONE;
        return result->status;
    }
    if (capture.capture_error != 0u || capture.footer_copied != PAK_V11_FOOTER_BYTES ||
        capture.primary_range_ready != 1u ||
        capture.primary_offset != primary_result.primary_index_offset ||
        capture.primary_size != primary_result.primary_index_size ||
        capture.primary_copied != primary_result.primary_index_size ||
        capture.requested_bytes != primary_result.requested_read_bytes ||
        capture.read_calls != primary_result.read_calls) {
        clear_captured_primary(&capture);
        result->status = PAK_ENTRY_PIPELINE_CAPTURE_REJECTED;
        result->reason = PAK_ENTRY_PIPELINE_REASON_CAPTURE_INCOMPLETE;
        return result->status;
    }

    result->primary_validated = 1u;
    result->primary_capture_complete = 1u;
    result->primary = primary_result;
    if (!find_encoded_span(primary_scratch, (size_t)primary_result.primary_index_size,
                           &primary_result, &span_offset, &span_size)) {
        result->status = PAK_ENTRY_PIPELINE_CAPTURE_REJECTED;
        result->reason = PAK_ENTRY_PIPELINE_REASON_PRIMARY_LAYOUT;
        return result->status;
    }
    result->encoded_span_offset = span_offset;
    result->encoded_span_bytes = span_size;

    method_table_status = build_method_table(&primary_result, &method_table);
    if (method_table_status < 0) {
        result->status = PAK_ENTRY_PIPELINE_UNSUPPORTED;
        result->reason = PAK_ENTRY_PIPELINE_REASON_METHOD_TABLE;
        return result->status;
    }
    if (method_table_status == 0) {
        result->status = PAK_ENTRY_PIPELINE_CAPTURE_REJECTED;
        result->reason = PAK_ENTRY_PIPELINE_REASON_METHOD_TABLE;
        return result->status;
    }

    result->entry_decode_attempted = 1u;
    entry_status = pak_entry_decode_v11(primary_scratch + span_offset, span_size,
                                        selected_record_offset, source->logical_size,
                                        &method_table, entry_limits, blocks_out,
                                        blocks_capacity, &result->entry);
    result->entry_status = result->entry.status;
    result->entry_reason = result->entry.reason;
    result->entry_complete = result->entry.record_complete;
    if (entry_status != PAK_ENTRY_STATUS_OK || result->entry.record_complete != 1u) {
        result->status = PAK_ENTRY_PIPELINE_ENTRY_REJECTED;
        result->reason = PAK_ENTRY_PIPELINE_REASON_NONE;
        return result->status;
    }
    pipeline_status = PAK_ENTRY_PIPELINE_OK;
    result->status = pipeline_status;
    result->reason = PAK_ENTRY_PIPELINE_REASON_NONE;
    return result->status;
}
