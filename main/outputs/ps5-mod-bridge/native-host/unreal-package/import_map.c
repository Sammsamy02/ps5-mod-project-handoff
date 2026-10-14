#include "import_map.h"

#include <limits.h>
#include <string.h>

typedef struct span_range {
    uintptr_t begin;
    uintptr_t end;
    int present;
} span_range;

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t read_i32(const uint8_t *p) {
    const uint32_t raw = read_u32(p);
    if (raw <= INT32_MAX) return (int32_t)raw;
    return (int32_t)(-((int64_t)(UINT32_MAX - raw)) - 1);
}

static uint64_t read_u64(const uint8_t *p) {
    return (uint64_t)read_u32(p) | ((uint64_t)read_u32(p + 4) << 32);
}

static int mul_size(size_t a, size_t b, size_t *out) {
    if (a && b > SIZE_MAX / a) return 0;
    *out = a * b;
    return 1;
}

static int range_for(const void *ptr, size_t bytes, span_range *out) {
    if (!bytes) {
        out->begin = 0;
        out->end = 0;
        out->present = 0;
        return ptr != NULL || bytes == 0;
    }
    if (!ptr) return 0;
    const uintptr_t begin = (uintptr_t)ptr;
    if (bytes > UINTPTR_MAX - begin) return 0;
    out->begin = begin;
    out->end = begin + bytes;
    out->present = 1;
    return 1;
}

static int overlaps(span_range a, span_range b) {
    return a.present && b.present && a.begin < b.end && b.begin < a.end;
}

static int preflight(const mb_zen_summary_view_v1 *view,
    const mb_zen_summary_result_v1 *summary,
    const mb_zen_import_limits_v1 *limits, mb_zen_import_result_v1 *result,
    mb_zen_import_ref_v1 *references, uint32_t references_capacity) {
    span_range ranges[6];
    size_t reference_bytes;
    if (!view || !summary || !result ||
        (uintptr_t)view % _Alignof(mb_zen_summary_view_v1) ||
        (uintptr_t)summary % _Alignof(mb_zen_summary_result_v1) ||
        (uintptr_t)result % _Alignof(mb_zen_import_result_v1) ||
        (limits && (uintptr_t)limits % _Alignof(mb_zen_import_limits_v1)) ||
        (references_capacity && (!references ||
            (uintptr_t)references % _Alignof(mb_zen_import_ref_v1))) ||
        references_capacity > MB_ZEN_IMPORT_MAP_MAX_ENTRIES ||
        view->struct_size != sizeof(*view) || view->abi_version != MB_ZEN_SUMMARY_ABI_VERSION ||
        summary->struct_size != sizeof(*summary) || summary->abi_version != MB_ZEN_SUMMARY_ABI_VERSION ||
        result->struct_size != sizeof(*result) || result->abi_version != MB_ZEN_IMPORT_MAP_ABI_VERSION ||
        (limits && (limits->struct_size != sizeof(*limits) ||
            limits->abi_version != MB_ZEN_IMPORT_MAP_ABI_VERSION))) return 0;
    if (references_capacity && !references) return 0;
    if (!mul_size((size_t)references_capacity, sizeof(*references), &reference_bytes)) return 0;
    if ((view->captured_bytes && !view->header_bytes) ||
        !range_for(view, sizeof(*view), &ranges[0]) ||
        !range_for(summary, sizeof(*summary), &ranges[1]) ||
        !range_for(limits, limits ? sizeof(*limits) : 0, &ranges[2]) ||
        !range_for(view->header_bytes, view->captured_bytes, &ranges[3]) ||
        !range_for(result, sizeof(*result), &ranges[4]) ||
        !range_for(references, reference_bytes, &ranges[5])) return 0;
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 4; j < 6; ++j) if (overlaps(ranges[i], ranges[j])) return 0;
    }
    if (overlaps(ranges[4], ranges[5])) return 0;
    return 1;
}

static void clear_outputs(mb_zen_import_result_v1 *result,
    mb_zen_import_ref_v1 *references, uint32_t references_capacity) {
    memset(result, 0, sizeof(*result));
    result->struct_size = (uint32_t)sizeof(*result);
    result->abi_version = MB_ZEN_IMPORT_MAP_ABI_VERSION;
    if (references_capacity) memset(references, 0,
        (size_t)references_capacity * sizeof(*references));
}

static mb_zen_import_map_status fail(mb_zen_import_result_v1 *result,
    mb_zen_import_map_status status, mb_zen_import_map_reason reason) {
    result->status = (uint32_t)status;
    result->reason = (uint32_t)reason;
    return status;
}

void mb_zen_import_limits_v1_default(mb_zen_import_limits_v1 *limits) {
    if (!limits) return;
    *limits = (mb_zen_import_limits_v1){
        (uint32_t)sizeof(*limits), MB_ZEN_IMPORT_MAP_ABI_VERSION,
        MB_ZEN_IMPORT_MAP_MAX_ENTRIES, 0
    };
}

mb_zen_import_map_status mb_zen_read_import_map_v1(
    const mb_zen_summary_view_v1 *view,
    const mb_zen_summary_result_v1 *summary,
    const mb_zen_import_limits_v1 *given_limits,
    mb_zen_import_result_v1 *result,
    mb_zen_import_ref_v1 *references,
    uint32_t references_capacity) {
    if (!preflight(view, summary, given_limits, result, references, references_capacity))
        return MB_ZEN_IMPORT_MAP_BAD_ARGUMENT;

    clear_outputs(result, references, references_capacity);
    mb_zen_import_limits_v1 defaults;
    mb_zen_import_limits_v1_default(&defaults);
    const mb_zen_import_limits_v1 *limits = given_limits ? given_limits : &defaults;
    if (limits->reserved || limits->max_imports > MB_ZEN_IMPORT_MAP_MAX_ENTRIES)
        return fail(result, MB_ZEN_IMPORT_MAP_BAD_ARGUMENT, MB_ZEN_IMPORT_MAP_REASON_ABI);
    if (view->captured_bytes > MB_ZEN_SUMMARY_MAX_HEADER_BYTES)
        return fail(result, MB_ZEN_IMPORT_MAP_RESOURCE_LIMIT, MB_ZEN_IMPORT_MAP_REASON_CAPTURE);
    if (view->captured_bytes < 52)
        return fail(result, MB_ZEN_IMPORT_MAP_TRUNCATED, MB_ZEN_IMPORT_MAP_REASON_CAPTURE);

    const uint8_t *bytes = view->header_bytes;
    if (summary->status != MB_ZEN_SUMMARY_OK || summary->reason != MB_ZEN_SUMMARY_REASON_NONE ||
        summary->summary_complete != 1 || summary->name_map_complete != 1 ||
        summary->versioning_info_flag_raw != 0 || read_u32(bytes) != 0 ||
        summary->header_size != read_u32(bytes + 4))
        return fail(result, MB_ZEN_IMPORT_MAP_BAD_ARGUMENT, MB_ZEN_IMPORT_MAP_REASON_SUMMARY);
    if (!summary->header_size || summary->header_size > MB_ZEN_SUMMARY_MAX_HEADER_BYTES)
        return fail(result, MB_ZEN_IMPORT_MAP_RESOURCE_LIMIT, MB_ZEN_IMPORT_MAP_REASON_CAPTURE);
    if ((uint64_t)summary->header_size > view->logical_package_bytes)
        return fail(result, MB_ZEN_IMPORT_MAP_OUT_OF_RANGE, MB_ZEN_IMPORT_MAP_REASON_CAPTURE);
    if (summary->header_size > view->captured_bytes)
        return fail(result, MB_ZEN_IMPORT_MAP_TRUNCATED, MB_ZEN_IMPORT_MAP_REASON_CAPTURE);

    const int32_t raw_hash_offset = read_i32(bytes + 24);
    const int32_t raw_import_offset = read_i32(bytes + 28);
    const int32_t raw_export_offset = read_i32(bytes + 32);
    if (raw_hash_offset < 0 || raw_import_offset < 0 || raw_export_offset < 0 ||
        raw_hash_offset != summary->imported_public_export_hashes_offset ||
        raw_import_offset != summary->import_map_offset ||
        raw_export_offset != summary->export_map_offset ||
        (uint32_t)raw_hash_offset > summary->header_size ||
        (uint32_t)raw_import_offset > summary->header_size ||
        (uint32_t)raw_export_offset > summary->header_size ||
        raw_hash_offset > raw_import_offset || raw_import_offset > raw_export_offset)
        return fail(result, MB_ZEN_IMPORT_MAP_MALFORMED, MB_ZEN_IMPORT_MAP_REASON_OFFSETS);

    const uint32_t public_hash_bytes = (uint32_t)(raw_import_offset - raw_hash_offset);
    const uint32_t import_bytes = (uint32_t)(raw_export_offset - raw_import_offset);
    if ((public_hash_bytes & 7u) || (import_bytes & 7u))
        return fail(result, MB_ZEN_IMPORT_MAP_MALFORMED, MB_ZEN_IMPORT_MAP_REASON_STRIDE);
    const uint32_t public_hash_count = public_hash_bytes / 8u;
    const uint32_t import_count = import_bytes / 8u;
    if (import_count > limits->max_imports || import_count > MB_ZEN_IMPORT_MAP_MAX_ENTRIES)
        return fail(result, MB_ZEN_IMPORT_MAP_RESOURCE_LIMIT, MB_ZEN_IMPORT_MAP_REASON_OUTPUT_CAPACITY);
    if (import_count > references_capacity)
        return fail(result, MB_ZEN_IMPORT_MAP_RESOURCE_LIMIT, MB_ZEN_IMPORT_MAP_REASON_OUTPUT_CAPACITY);

    uint32_t counts[4] = {0, 0, 0, 0};
    for (uint32_t i = 0; i < import_count; ++i) {
        const uint64_t raw = read_u64(bytes + (uint32_t)raw_import_offset + (size_t)i * 8u);
        const uint32_t kind = (uint32_t)(raw >> 62);
        ++counts[kind];
        if (kind == MB_ZEN_IMPORT_PACKAGE && (uint32_t)raw >= public_hash_count)
            return fail(result, MB_ZEN_IMPORT_MAP_MALFORMED,
                MB_ZEN_IMPORT_MAP_REASON_PACKAGE_HASH_INDEX);
    }

    result->import_map_offset = (uint32_t)raw_import_offset;
    result->import_map_bytes = import_bytes;
    result->public_hash_offset = (uint32_t)raw_hash_offset;
    result->public_hash_bytes = public_hash_bytes;
    result->public_hash_count = public_hash_count;
    result->reference_count = import_count;
    memcpy(result->kind_counts, counts, sizeof(counts));

    const uint64_t index_mask = (UINT64_C(1) << 62) - 1u;
    for (uint32_t i = 0; i < import_count; ++i) {
        const uint64_t raw = read_u64(bytes + (uint32_t)raw_import_offset + (size_t)i * 8u);
        const uint32_t kind = (uint32_t)(raw >> 62);
        mb_zen_import_ref_v1 *ref = references + i;
        ref->raw = raw;
        ref->value = raw & index_mask;
        ref->kind = kind;
        ref->is_null = raw == UINT64_MAX;
        if (kind == MB_ZEN_IMPORT_EXPORT) ref->export_index = (uint32_t)raw;
        if (kind == MB_ZEN_IMPORT_PACKAGE) {
            ref->imported_package_index = (uint32_t)(ref->value >> 32);
            ref->public_export_hash_index = (uint32_t)raw;
        }
    }
    result->complete = 1;
    result->status = MB_ZEN_IMPORT_MAP_OK;
    result->reason = MB_ZEN_IMPORT_MAP_REASON_NONE;
    return MB_ZEN_IMPORT_MAP_OK;
}
