#include "pak_primary_capture.h"

#include <limits.h>
#include <string.h>

typedef struct capture_state {
    const pak_source_v1 *base_source;
    uint8_t *primary_capture;
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
} capture_state;

typedef struct byte_cursor {
    const uint8_t *bytes;
    size_t size;
    size_t position;
} byte_cursor;

typedef struct address_range {
    uintptr_t begin;
    uintptr_t end;
} address_range;

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

static int make_range(const void *pointer, size_t length, address_range *range)
{
    uintptr_t begin;
    if (pointer == NULL || range == NULL || length == 0u) return 0;
    begin = (uintptr_t)pointer;
    if (length > UINTPTR_MAX - begin) return 0;
    range->begin = begin;
    range->end = begin + length;
    return range->end > range->begin;
}

static int ranges_disjoint(const address_range *a, const address_range *b)
{
    return a->end <= b->begin || b->end <= a->begin;
}

static int valid_storage(const pak_source_v1 *source,
                         const pak_limits_v1 *limits,
                         uint8_t *workspace,
                         size_t workspace_bytes,
                         uint8_t *primary_capture,
                         size_t primary_capture_capacity,
                         pak_primary_capture_result_v1 *result)
{
    address_range ranges[5];
    size_t i, j;
    if (source == NULL || limits == NULL || workspace == NULL ||
        primary_capture == NULL || result == NULL ||
        workspace_bytes != PAK_READER_WORKSPACE_BYTES ||
        primary_capture_capacity == 0u ||
        primary_capture_capacity > PAK_READER_MAX_PRIMARY_BYTES ||
        !make_range(source, sizeof(*source), &ranges[0]) ||
        !make_range(limits, sizeof(*limits), &ranges[1]) ||
        !make_range(workspace, workspace_bytes, &ranges[2]) ||
        !make_range(primary_capture, primary_capture_capacity, &ranges[3]) ||
        !make_range(result, sizeof(*result), &ranges[4])) {
        return 0;
    }
    for (i = 0u; i < 5u; ++i) {
        for (j = i + 1u; j < 5u; ++j) {
            if (!ranges_disjoint(&ranges[i], &ranges[j])) return 0;
        }
    }
    /* The opaque provider context's full extent is caller-owned and not knowable here. */
    if (source->context == (void *)source || source->context == (void *)limits ||
        source->context == (void *)workspace || source->context == (void *)primary_capture ||
        source->context == (void *)result) {
        return 0;
    }
    return 1;
}

static void initialize_result(pak_primary_capture_result_v1 *result)
{
    memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result);
    result->abi_version = PAK_PRIMARY_CAPTURE_ABI_VERSION;
    result->status = PAK_PRIMARY_CAPTURE_BAD_ARGUMENT;
    result->reason = PAK_PRIMARY_CAPTURE_REASON_BAD_ARGUMENT;
    result->reader_status = PAK_STATUS_BAD_ARGUMENT;
    result->reader_reason = PAK_REASON_BAD_ARGUMENT;
}

static void clear_outputs(uint8_t *workspace,
                          size_t workspace_bytes,
                          uint8_t *primary_capture,
                          size_t primary_capture_capacity)
{
    memset(workspace, 0, workspace_bytes);
    memset(primary_capture, 0, primary_capture_capacity);
}

static pak_primary_capture_status fail_call(
    uint8_t *workspace,
    size_t workspace_bytes,
    uint8_t *primary_capture,
    size_t primary_capture_capacity,
    pak_primary_capture_result_v1 *result,
    pak_primary_capture_status status,
    pak_primary_capture_reason reason,
    pak_status reader_status,
    pak_reason reader_reason,
    uint64_t requested_bytes,
    uint32_t read_calls)
{
    clear_outputs(workspace, workspace_bytes, primary_capture, primary_capture_capacity);
    initialize_result(result);
    result->status = status;
    result->reason = reason;
    result->reader_status = reader_status;
    result->reader_reason = reader_reason;
    result->aggregate_read_bytes = requested_bytes;
    result->aggregate_read_calls = read_calls;
    return status;
}

static int capture_region(capture_state *capture,
                          uint64_t request_offset,
                          const uint8_t *request_bytes,
                          uint32_t actual,
                          uint64_t region_offset,
                          uint64_t region_size,
                          uint8_t *region_buffer,
                          uint64_t *region_copied)
{
    uint64_t request_end, region_end, overlap_start, overlap_end, expected;
    uint64_t bytes, request_displacement;

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
    request_displacement = overlap_start - request_offset;
    if (bytes > SIZE_MAX || request_displacement > actual ||
        bytes > (uint64_t)actual - request_displacement ||
        *region_copied > region_size || bytes > region_size - *region_copied) {
        capture->capture_error = 1u;
        return 0;
    }
    memcpy(region_buffer + (size_t)*region_copied,
           request_bytes + (size_t)request_displacement,
           (size_t)bytes);
    *region_copied += bytes;
    return 1;
}

static void capture_footer_range(capture_state *capture)
{
    uint64_t primary_end;
    if (capture->footer_copied != PAK_V11_FOOTER_BYTES) return;
    capture->primary_offset = load_le64(capture->footer + 25u);
    capture->primary_size = load_le64(capture->footer + 33u);
    if ((capture->primary_offset & (UINT64_C(1) << 63)) != 0u ||
        (capture->primary_size & (UINT64_C(1) << 63)) != 0u ||
        capture->primary_size == 0u ||
        capture->primary_size > capture->primary_capacity ||
        !add_u64(capture->primary_offset, capture->primary_size, &primary_end) ||
        primary_end > capture->footer_offset) {
        return;
    }
    capture->primary_range_ready = 1u;
}

static pak_provider_status capture_read_at(void *context,
                                           uint64_t offset,
                                           uint8_t *destination,
                                           uint32_t requested,
                                           uint32_t *actual)
{
    capture_state *capture = (capture_state *)context;
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
    *actual = 0u;
    status = capture->base_source->read_at(capture->base_source->context,
                                            offset, destination, requested, actual);
    if (status != PAK_PROVIDER_OK) return status;
    if (*actual > requested) {
        capture->capture_error = 1u;
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    if (!capture_region(capture, offset, destination, *actual,
                        capture->footer_offset, PAK_V11_FOOTER_BYTES,
                        capture->footer, &capture->footer_copied)) {
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    capture_footer_range(capture);
    if (capture->primary_range_ready != 0u &&
        !capture_region(capture, offset, destination, *actual,
                        capture->primary_offset, capture->primary_size,
                        capture->primary_capture, &capture->primary_copied)) {
        return PAK_PROVIDER_CONTRACT_ERROR;
    }
    if (capture->capture_error != 0u) return PAK_PROVIDER_CONTRACT_ERROR;
    return PAK_PROVIDER_OK;
}

static int cursor_take(byte_cursor *cursor, size_t amount, const uint8_t **out)
{
    if (cursor->position > cursor->size || amount > cursor->size - cursor->position) return 0;
    if (out != NULL) *out = cursor->bytes + cursor->position;
    cursor->position += amount;
    return 1;
}

static int cursor_i32(byte_cursor *cursor, int64_t *value)
{
    const uint8_t *p;
    uint32_t word;
    if (!cursor_take(cursor, 4u, &p)) return 0;
    word = load_le32(p);
    *value = (word & UINT32_C(0x80000000)) == 0u ? (int64_t)word :
             (int64_t)word - INT64_C(0x100000000);
    return 1;
}

static int cursor_u32(byte_cursor *cursor, uint32_t *value)
{
    const uint8_t *p;
    if (!cursor_take(cursor, 4u, &p)) return 0;
    *value = load_le32(p);
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
    if (!skip_fstring(&cursor) || !cursor_i32(&cursor, &value) || value < 0 ||
        (uint64_t)value != primary->declared_file_count ||
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
        !cursor_i32(&cursor, &value) || value != 0 || cursor.position != cursor.size) {
        return 0;
    }
    (void)span;
    return 1;
}

static int set_method_name(pak_entry_method_name *out,
                            const uint8_t *bytes,
                            uint32_t length)
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

/* Returns 1 for success, 0 for malformed names, and -1 for unsupported duplicate None. */
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

pak_primary_capture_status pak_capture_v11_primary(
    const pak_source_v1 *source,
    const pak_limits_v1 *reader_limits,
    uint8_t *reader_workspace,
    size_t reader_workspace_bytes,
    uint8_t *primary_capture,
    size_t primary_capture_capacity,
    pak_primary_capture_result_v1 *result)
{
    capture_state capture;
    pak_source_v1 proxy_source;
    pak_limits_v1 bounded_limits;
    pak_primary_result_v1 primary;
    pak_entry_method_table_v1 method_table;
    pak_primary_capture_reason failure_reason;
    pak_primary_capture_status failure_status;
    pak_status reader_status;
    pak_reason reader_reason;
    uint32_t span_offset = 0u, span_size = 0u;
    int method_status;

    if (result == NULL) return PAK_PRIMARY_CAPTURE_BAD_ARGUMENT;
    if (!valid_storage(source, reader_limits, reader_workspace, reader_workspace_bytes,
                       primary_capture, primary_capture_capacity, result)) {
        /* Aliased or unbounded storage cannot safely be initialized or cleared. */
        return PAK_PRIMARY_CAPTURE_BAD_ARGUMENT;
    }
    initialize_result(result);

    if (source->struct_size < sizeof(*source) || source->abi_version != PAK_READER_ABI_VERSION ||
        source->reserved != 0u || source->read_at == NULL || source->snapshot_token == 0u ||
        source->snapshot_scope != PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE ||
        reader_limits->struct_size < sizeof(*reader_limits) ||
        reader_limits->abi_version != PAK_READER_ABI_VERSION || reader_limits->reserved != 0u) {
        return fail_call(reader_workspace, reader_workspace_bytes, primary_capture,
                         primary_capture_capacity, result,
                         PAK_PRIMARY_CAPTURE_BAD_ARGUMENT,
                         PAK_PRIMARY_CAPTURE_REASON_BAD_ARGUMENT,
                         PAK_STATUS_BAD_ARGUMENT, PAK_REASON_BAD_ARGUMENT, 0u, 0u);
    }

    memset(&capture, 0, sizeof(capture));
    capture.base_source = source;
    capture.primary_capture = primary_capture;
    capture.primary_capacity = primary_capture_capacity;
    if (source->logical_size >= PAK_V11_FOOTER_BYTES) {
        capture.footer_offset = source->logical_size - PAK_V11_FOOTER_BYTES;
    }

    bounded_limits = *reader_limits;
    if (bounded_limits.max_primary_bytes != 0u &&
        bounded_limits.max_primary_bytes <= PAK_READER_MAX_PRIMARY_BYTES &&
        bounded_limits.max_primary_bytes > primary_capture_capacity) {
        bounded_limits.max_primary_bytes = (uint32_t)primary_capture_capacity;
    }
    proxy_source = *source;
    proxy_source.struct_size = sizeof(proxy_source);
    proxy_source.context = &capture;
    proxy_source.read_at = capture_read_at;
    memset(&primary, 0, sizeof(primary));
    (void)pak_read_v11_primary(&proxy_source, &bounded_limits,
                               reader_workspace, reader_workspace_bytes, &primary);
    reader_status = primary.status;
    reader_reason = primary.reason;

    if (primary.status != PAK_STATUS_OK || primary.primary_metadata_complete != 1u ||
        primary.primary_sha1_checked != 1u || primary.primary_sha1_matches != 1u) {
        return fail_call(reader_workspace, reader_workspace_bytes, primary_capture,
                         primary_capture_capacity, result,
                         PAK_PRIMARY_CAPTURE_READER_REJECTED,
                         PAK_PRIMARY_CAPTURE_REASON_READER_REJECTED,
                         reader_status, reader_reason,
                         capture.requested_bytes, capture.read_calls);
    }
    if (capture.capture_error != 0u || capture.footer_copied != PAK_V11_FOOTER_BYTES ||
        capture.primary_range_ready != 1u ||
        capture.primary_offset != primary.primary_index_offset ||
        capture.primary_size != primary.primary_index_size ||
        capture.primary_copied != primary.primary_index_size ||
        capture.requested_bytes != primary.requested_read_bytes ||
        capture.read_calls != primary.read_calls) {
        return fail_call(reader_workspace, reader_workspace_bytes, primary_capture,
                         primary_capture_capacity, result,
                         PAK_PRIMARY_CAPTURE_CAPTURE_REJECTED,
                         PAK_PRIMARY_CAPTURE_REASON_INCOMPLETE_CAPTURE,
                         reader_status, reader_reason,
                         capture.requested_bytes, capture.read_calls);
    }
    if (primary.primary_index_size > UINT32_MAX ||
        !find_encoded_span(primary_capture, (size_t)primary.primary_index_size,
                           &primary, &span_offset, &span_size)) {
        return fail_call(reader_workspace, reader_workspace_bytes, primary_capture,
                         primary_capture_capacity, result,
                         PAK_PRIMARY_CAPTURE_CAPTURE_REJECTED,
                         PAK_PRIMARY_CAPTURE_REASON_PRIMARY_LAYOUT,
                         reader_status, reader_reason,
                         capture.requested_bytes, capture.read_calls);
    }
    method_status = build_method_table(&primary, &method_table);
    if (method_status < 0) {
        failure_status = PAK_PRIMARY_CAPTURE_UNSUPPORTED;
        failure_reason = PAK_PRIMARY_CAPTURE_REASON_METHOD_TABLE;
    } else if (method_status == 0) {
        failure_status = PAK_PRIMARY_CAPTURE_CAPTURE_REJECTED;
        failure_reason = PAK_PRIMARY_CAPTURE_REASON_METHOD_TABLE;
    } else {
        failure_status = PAK_PRIMARY_CAPTURE_OK;
        failure_reason = PAK_PRIMARY_CAPTURE_REASON_NONE;
    }
    if (failure_status != PAK_PRIMARY_CAPTURE_OK) {
        return fail_call(reader_workspace, reader_workspace_bytes, primary_capture,
                         primary_capture_capacity, result, failure_status,
                         failure_reason, reader_status, reader_reason,
                         capture.requested_bytes, capture.read_calls);
    }

    memset(reader_workspace, 0, reader_workspace_bytes);
    result->status = PAK_PRIMARY_CAPTURE_OK;
    result->reason = PAK_PRIMARY_CAPTURE_REASON_NONE;
    result->reader_status = reader_status;
    result->reader_reason = reader_reason;
    result->primary_validated = 1u;
    result->primary_capture_complete = 1u;
    result->encoded_span_located = 1u;
    result->primary_capture_bytes = (uint32_t)primary.primary_index_size;
    result->encoded_span_offset = span_offset;
    result->encoded_span_bytes = span_size;
    result->aggregate_read_bytes = capture.requested_bytes;
    result->aggregate_read_calls = capture.read_calls;
    result->primary = primary;
    result->method_table = method_table;
    return PAK_PRIMARY_CAPTURE_OK;
}
