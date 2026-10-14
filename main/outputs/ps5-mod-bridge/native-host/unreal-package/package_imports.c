#include "package_imports.h"

#include <limits.h>
#include <string.h>

typedef struct byte_range {
    uintptr_t begin;
    uintptr_t end;
    int present;
} byte_range;

typedef struct imported_names_layout {
    uint32_t offset;
    uint32_t bytes;
    uint32_t count;
    uint32_t declared_string_bytes;
    uint32_t string_bytes;
    uint64_t hash_version;
    size_t hashes_offset;
    size_t headers_offset;
    size_t strings_offset;
    size_t numbers_offset;
} imported_names_layout;

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

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

static int add_size(size_t a, size_t b, size_t *out) {
    if (b > SIZE_MAX - a) return 0;
    *out = a + b;
    return 1;
}

static int mul_size(size_t a, size_t b, size_t *out) {
    if (a && b > SIZE_MAX / a) return 0;
    *out = a * b;
    return 1;
}

static int range_for(const void *ptr, size_t bytes, byte_range *out) {
    if (!bytes) {
        out->begin = 0; out->end = 0; out->present = 0;
        return ptr != NULL || bytes == 0;
    }
    if (!ptr) return 0;
    uintptr_t begin = (uintptr_t)ptr;
    if (bytes > UINTPTR_MAX - begin) return 0;
    out->begin = begin; out->end = begin + bytes; out->present = 1;
    return 1;
}

static int overlaps(byte_range a, byte_range b) {
    return a.present && b.present && a.begin < b.end && b.begin < a.end;
}

static int aligned_for(const void *ptr, size_t alignment) {
    return ptr && ((uintptr_t)ptr % alignment) == 0;
}

static int preflight(const mb_zen_summary_view_v1 *view,
    const mb_zen_summary_result_v1 *summary, const mb_zen_import_result_v1 *import_map,
    const mb_zen_import_ref_v1 *references, uint32_t references_capacity,
    const mb_zen_package_imports_limits_v1 *limits,
    mb_zen_package_imports_result_v1 *result,
    mb_zen_imported_package_name_v1 *names, uint32_t names_capacity,
    uint8_t *name_arena, uint32_t name_arena_capacity,
    mb_zen_package_import_binding_v1 *bindings, uint32_t bindings_capacity) {
    byte_range inputs[7], outputs[4];
    size_t reference_bytes, name_bytes, binding_bytes;
    if (!view || !summary || !import_map || !result ||
        !aligned_for(view, _Alignof(mb_zen_summary_view_v1)) ||
        !aligned_for(summary, _Alignof(mb_zen_summary_result_v1)) ||
        !aligned_for(import_map, _Alignof(mb_zen_import_result_v1)) ||
        !aligned_for(result, _Alignof(mb_zen_package_imports_result_v1)) ||
        (limits && !aligned_for(limits, _Alignof(mb_zen_package_imports_limits_v1))) ||
        (references_capacity && (!references || !aligned_for(references, _Alignof(mb_zen_import_ref_v1)))) ||
        (names_capacity && (!names || !aligned_for(names, _Alignof(mb_zen_imported_package_name_v1)))) ||
        (name_arena_capacity && !name_arena) ||
        (bindings_capacity && (!bindings || !aligned_for(bindings, _Alignof(mb_zen_package_import_binding_v1)))) ||
        references_capacity > MB_ZEN_IMPORT_MAP_MAX_ENTRIES ||
        names_capacity > MB_ZEN_PACKAGE_IMPORTS_MAX_NAMES ||
        name_arena_capacity > MB_ZEN_PACKAGE_IMPORTS_MAX_NAME_BYTES ||
        bindings_capacity > MB_ZEN_PACKAGE_IMPORTS_MAX_BINDINGS ||
        view->struct_size != sizeof(*view) || view->abi_version != MB_ZEN_SUMMARY_ABI_VERSION ||
        summary->struct_size != sizeof(*summary) || summary->abi_version != MB_ZEN_SUMMARY_ABI_VERSION ||
        import_map->struct_size != sizeof(*import_map) || import_map->abi_version != MB_ZEN_IMPORT_MAP_ABI_VERSION ||
        result->struct_size != sizeof(*result) || result->abi_version != MB_ZEN_PACKAGE_IMPORTS_ABI_VERSION ||
        (limits && (limits->struct_size != sizeof(*limits) ||
            limits->abi_version != MB_ZEN_PACKAGE_IMPORTS_ABI_VERSION))) return 0;
    if (!mul_size((size_t)references_capacity, sizeof(*references), &reference_bytes) ||
        !mul_size((size_t)names_capacity, sizeof(*names), &name_bytes) ||
        !mul_size((size_t)bindings_capacity, sizeof(*bindings), &binding_bytes)) return 0;
    if ((view->captured_bytes && !view->header_bytes) ||
        !range_for(view, sizeof(*view), &inputs[0]) ||
        !range_for(summary, sizeof(*summary), &inputs[1]) ||
        !range_for(import_map, sizeof(*import_map), &inputs[2]) ||
        !range_for(references, reference_bytes, &inputs[3]) ||
        !range_for(limits, limits ? sizeof(*limits) : 0, &inputs[4]) ||
        !range_for(view->header_bytes, view->captured_bytes, &inputs[5]) ||
        !range_for(result, sizeof(*result), &outputs[0]) ||
        !range_for(names, name_bytes, &outputs[1]) ||
        !range_for(name_arena, name_arena_capacity, &outputs[2]) ||
        !range_for(bindings, binding_bytes, &outputs[3])) return 0;
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = i + 1; j < 4; ++j)
            if (overlaps(outputs[i], outputs[j])) return 0;
    for (size_t i = 0; i < 6; ++i)
        for (size_t j = 0; j < 4; ++j)
            if (overlaps(inputs[i], outputs[j])) return 0;
    return 1;
}

static void clear_outputs(mb_zen_package_imports_result_v1 *result,
    mb_zen_imported_package_name_v1 *names, uint32_t names_capacity,
    uint8_t *name_arena, uint32_t name_arena_capacity,
    mb_zen_package_import_binding_v1 *bindings, uint32_t bindings_capacity) {
    memset(result, 0, sizeof(*result));
    result->struct_size = (uint32_t)sizeof(*result);
    result->abi_version = MB_ZEN_PACKAGE_IMPORTS_ABI_VERSION;
    if (names_capacity) memset(names, 0, (size_t)names_capacity * sizeof(*names));
    if (name_arena_capacity) memset(name_arena, 0, name_arena_capacity);
    if (bindings_capacity) memset(bindings, 0, (size_t)bindings_capacity * sizeof(*bindings));
}

static mb_zen_package_imports_status fail(mb_zen_package_imports_result_v1 *result,
    mb_zen_package_imports_status status, mb_zen_package_imports_reason reason) {
    result->status = (uint32_t)status;
    result->reason = (uint32_t)reason;
    return status;
}

void mb_zen_package_imports_limits_v1_default(mb_zen_package_imports_limits_v1 *limits) {
    if (!limits) return;
    *limits = (mb_zen_package_imports_limits_v1){
        (uint32_t)sizeof(*limits), MB_ZEN_PACKAGE_IMPORTS_ABI_VERSION,
        MB_ZEN_PACKAGE_IMPORTS_MAX_NAMES, MB_ZEN_PACKAGE_IMPORTS_MAX_NAME_BYTES,
        MB_ZEN_PACKAGE_IMPORTS_MAX_BINDINGS, 0
    };
}

static mb_zen_package_imports_status parse_names(
    const mb_zen_summary_view_v1 *view, const mb_zen_summary_result_v1 *summary,
    const mb_zen_package_imports_limits_v1 *limits, imported_names_layout *layout,
    mb_zen_package_imports_reason *reason) {
    const uint8_t *bytes = view->header_bytes;
    if (view->captured_bytes > MB_ZEN_SUMMARY_MAX_HEADER_BYTES)
        { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_CAPTURE; return MB_ZEN_PACKAGE_IMPORTS_RESOURCE_LIMIT; }
    if (view->captured_bytes < 52)
        { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_CAPTURE; return MB_ZEN_PACKAGE_IMPORTS_TRUNCATED; }
    if (summary->status != MB_ZEN_SUMMARY_OK || summary->reason != MB_ZEN_SUMMARY_REASON_NONE ||
        summary->summary_complete != 1 || summary->name_map_complete != 1 ||
        summary->versioning_info_flag_raw != 0 || read_u32(bytes) != 0 ||
        summary->header_size != read_u32(bytes + 4))
        { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_SUMMARY; return MB_ZEN_PACKAGE_IMPORTS_BAD_ARGUMENT; }
    if (!summary->header_size || summary->header_size > MB_ZEN_SUMMARY_MAX_HEADER_BYTES)
        { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_CAPTURE; return MB_ZEN_PACKAGE_IMPORTS_RESOURCE_LIMIT; }
    if ((uint64_t)summary->header_size > view->logical_package_bytes)
        { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_CAPTURE; return MB_ZEN_PACKAGE_IMPORTS_OUT_OF_RANGE; }
    if (summary->header_size > view->captured_bytes)
        { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_CAPTURE; return MB_ZEN_PACKAGE_IMPORTS_TRUNCATED; }

    const int32_t raw_hash_offset = read_i32(bytes + 24);
    const int32_t raw_import_offset = read_i32(bytes + 28);
    const int32_t raw_export_offset = read_i32(bytes + 32);
    const int32_t raw_export_bundle_offset = read_i32(bytes + 36);
    const int32_t raw_dependency_headers_offset = read_i32(bytes + 40);
    const int32_t raw_dependency_entries_offset = read_i32(bytes + 44);
    const int32_t raw_names_offset = read_i32(bytes + 48);
    if (raw_hash_offset < 0 || raw_import_offset < 0 || raw_export_offset < 0 ||
        raw_export_bundle_offset < 0 || raw_dependency_headers_offset < 0 ||
        raw_dependency_entries_offset < 0 || raw_names_offset < 0 ||
        raw_hash_offset != summary->imported_public_export_hashes_offset ||
        raw_import_offset != summary->import_map_offset || raw_export_offset != summary->export_map_offset ||
        raw_export_bundle_offset != summary->export_bundle_entries_offset ||
        raw_dependency_headers_offset != summary->dependency_bundle_headers_offset ||
        raw_dependency_entries_offset != summary->dependency_bundle_entries_offset ||
        raw_names_offset != summary->imported_package_names_offset ||
        (uint32_t)raw_hash_offset > summary->header_size ||
        (uint32_t)raw_import_offset > summary->header_size ||
        (uint32_t)raw_export_offset > summary->header_size ||
        (uint32_t)raw_export_bundle_offset > summary->header_size ||
        (uint32_t)raw_dependency_headers_offset > summary->header_size ||
        (uint32_t)raw_dependency_entries_offset > summary->header_size ||
        (uint32_t)raw_names_offset > summary->header_size ||
        raw_hash_offset > raw_import_offset || raw_import_offset > raw_export_offset)
        { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_OFFSET; return MB_ZEN_PACKAGE_IMPORTS_MALFORMED; }
    if (!raw_names_offset)
        { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_OFFSET; return MB_ZEN_PACKAGE_IMPORTS_UNSUPPORTED; }
    if (raw_export_offset > raw_names_offset)
        { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_OFFSET; return MB_ZEN_PACKAGE_IMPORTS_MALFORMED; }

    size_t cursor = (size_t)(uint32_t)raw_names_offset;
    size_t end;
    if (!add_size(cursor, 4u, &end) || end > summary->header_size)
        { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_NAME_COUNT; return MB_ZEN_PACKAGE_IMPORTS_MALFORMED; }
    layout->offset = (uint32_t)raw_names_offset;
    layout->count = read_u32(bytes + cursor);
    cursor = end;
    if (layout->count > limits->max_names || layout->count > MB_ZEN_PACKAGE_IMPORTS_MAX_NAMES)
        { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_NAME_COUNT; return MB_ZEN_PACKAGE_IMPORTS_RESOURCE_LIMIT; }
    layout->declared_string_bytes = 0;
    layout->hash_version = 0;
    layout->string_bytes = 0;
    if (!layout->count) {
        layout->hashes_offset = cursor;
        layout->headers_offset = cursor;
        layout->strings_offset = cursor;
        layout->numbers_offset = cursor;
        end = cursor;
    } else {
        if (!add_size(cursor, 12u, &end) || end > summary->header_size)
            { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_NAME_HEADERS; return MB_ZEN_PACKAGE_IMPORTS_MALFORMED; }
        layout->declared_string_bytes = read_u32(bytes + cursor);
        layout->hash_version = read_u64(bytes + cursor + 4);
        if (layout->hash_version != MB_ZEN_PACKAGE_NAME_HASH_VERSION)
            { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_HASH_VERSION; return MB_ZEN_PACKAGE_IMPORTS_UNSUPPORTED; }
        cursor = end;
        layout->hashes_offset = cursor;
        size_t hashes_size, headers_size;
        if (!mul_size(layout->count, 8u, &hashes_size) || !add_size(cursor, hashes_size, &end) ||
            end > summary->header_size)
            { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_NAME_HEADERS; return MB_ZEN_PACKAGE_IMPORTS_MALFORMED; }
        cursor = end; layout->headers_offset = cursor;
        if (!mul_size(layout->count, 2u, &headers_size) || !add_size(cursor, headers_size, &end) ||
            end > summary->header_size)
            { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_NAME_HEADERS; return MB_ZEN_PACKAGE_IMPORTS_MALFORMED; }
        cursor = end; layout->strings_offset = cursor;
        for (uint32_t i = 0; i < layout->count; ++i) {
            const uint16_t encoded = read_be16(bytes + layout->headers_offset + (size_t)i * 2u);
            const uint32_t length = (encoded & UINT16_C(0x8000)) ? (encoded & UINT16_C(0x7fff)) : encoded;
            const uint32_t width = (encoded & UINT16_C(0x8000)) ? 2u : 1u;
            size_t name_bytes;
            if (!mul_size(length, width, &name_bytes) || name_bytes > UINT32_MAX - layout->string_bytes)
                { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_NAME_BYTES; return MB_ZEN_PACKAGE_IMPORTS_MALFORMED; }
            layout->string_bytes += (uint32_t)name_bytes;
            if (layout->string_bytes > limits->max_name_bytes ||
                layout->string_bytes > MB_ZEN_PACKAGE_IMPORTS_MAX_NAME_BYTES)
                { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_NAME_BYTES; return MB_ZEN_PACKAGE_IMPORTS_RESOURCE_LIMIT; }
        }
        if (!add_size(cursor, layout->string_bytes, &end) || end > summary->header_size)
            { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_NAME_BYTES; return MB_ZEN_PACKAGE_IMPORTS_MALFORMED; }
        cursor = end; layout->numbers_offset = cursor;
        size_t numbers_size;
        if (!mul_size(layout->count, 4u, &numbers_size) || !add_size(cursor, numbers_size, &end) ||
            end > summary->header_size)
            { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_NUMBER_ARRAY; return MB_ZEN_PACKAGE_IMPORTS_MALFORMED; }
    }
    if (end < summary->header_size)
        { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_TRAILING_BYTES; return MB_ZEN_PACKAGE_IMPORTS_MALFORMED; }
    if (end != summary->header_size)
        { *reason = MB_ZEN_PACKAGE_IMPORTS_REASON_TRAILING_BYTES; return MB_ZEN_PACKAGE_IMPORTS_MALFORMED; }
    layout->bytes = (uint32_t)(end - layout->offset);
    return MB_ZEN_PACKAGE_IMPORTS_OK;
}

mb_zen_package_imports_status mb_zen_read_package_imports_v1(
    const mb_zen_summary_view_v1 *view,
    const mb_zen_summary_result_v1 *summary,
    const mb_zen_import_result_v1 *import_map,
    const mb_zen_import_ref_v1 *references,
    uint32_t references_capacity,
    const mb_zen_package_imports_limits_v1 *given_limits,
    mb_zen_package_imports_result_v1 *result,
    mb_zen_imported_package_name_v1 *names,
    uint32_t names_capacity,
    uint8_t *name_arena,
    uint32_t name_arena_capacity,
    mb_zen_package_import_binding_v1 *bindings,
    uint32_t bindings_capacity) {
    if (!preflight(view, summary, import_map, references, references_capacity, given_limits,
            result, names, names_capacity, name_arena, name_arena_capacity, bindings, bindings_capacity))
        return MB_ZEN_PACKAGE_IMPORTS_BAD_ARGUMENT;
    clear_outputs(result, names, names_capacity, name_arena, name_arena_capacity,
        bindings, bindings_capacity);

    mb_zen_package_imports_limits_v1 defaults;
    mb_zen_package_imports_limits_v1_default(&defaults);
    const mb_zen_package_imports_limits_v1 *limits = given_limits ? given_limits : &defaults;
    if (limits->reserved || limits->max_names > MB_ZEN_PACKAGE_IMPORTS_MAX_NAMES ||
        limits->max_name_bytes > MB_ZEN_PACKAGE_IMPORTS_MAX_NAME_BYTES ||
        limits->max_bindings > MB_ZEN_PACKAGE_IMPORTS_MAX_BINDINGS)
        return fail(result, MB_ZEN_PACKAGE_IMPORTS_BAD_ARGUMENT, MB_ZEN_PACKAGE_IMPORTS_REASON_ABI);

    if (import_map->status != MB_ZEN_IMPORT_MAP_OK || import_map->reason != MB_ZEN_IMPORT_MAP_REASON_NONE ||
        import_map->complete != 1 || import_map->reference_count > references_capacity ||
        import_map->reference_count > MB_ZEN_IMPORT_MAP_MAX_ENTRIES || view->captured_bytes < 52)
        return fail(result, MB_ZEN_PACKAGE_IMPORTS_BAD_ARGUMENT, MB_ZEN_PACKAGE_IMPORTS_REASON_IMPORT_MAP);

    const uint8_t *bytes = view->header_bytes;
    const int32_t raw_hash = read_i32(bytes + 24);
    const int32_t raw_import = read_i32(bytes + 28);
    const int32_t raw_export = read_i32(bytes + 32);
    if (raw_hash < 0 || raw_import < 0 || raw_export < 0 ||
        import_map->public_hash_offset != (uint32_t)raw_hash ||
        import_map->import_map_offset != (uint32_t)raw_import ||
        raw_hash > raw_import || raw_import > raw_export ||
        import_map->public_hash_bytes != (uint32_t)(raw_import - raw_hash) ||
        import_map->import_map_bytes != (uint32_t)(raw_export - raw_import) ||
        (import_map->public_hash_bytes & 7u) || (import_map->import_map_bytes & 7u) ||
        import_map->public_hash_count != import_map->public_hash_bytes / 8u ||
        import_map->reference_count != import_map->import_map_bytes / 8u ||
        (uint32_t)raw_hash > summary->header_size || (uint32_t)raw_import > summary->header_size ||
        (uint32_t)raw_export > summary->header_size)
        return fail(result, MB_ZEN_PACKAGE_IMPORTS_BAD_ARGUMENT, MB_ZEN_PACKAGE_IMPORTS_REASON_IMPORT_MAP);

    imported_names_layout layout = {0};
    mb_zen_package_imports_reason reason = MB_ZEN_PACKAGE_IMPORTS_REASON_NONE;
    mb_zen_package_imports_status status = parse_names(view, summary, limits, &layout, &reason);
    if (status != MB_ZEN_PACKAGE_IMPORTS_OK) return fail(result, status, reason);

    uint32_t kind_counts[4] = {0, 0, 0, 0};
    for (uint32_t i = 0; i < import_map->reference_count; ++i) {
        const uint64_t raw = read_u64(bytes + (uint32_t)raw_import + (size_t)i * 8u);
        const uint32_t kind = (uint32_t)(raw >> 62);
        const uint64_t value = raw & ((UINT64_C(1) << 62) - 1u);
        const mb_zen_import_ref_v1 *ref = references + i;
        ++kind_counts[kind];
        if (ref->raw != raw || ref->value != value || ref->kind != kind ||
            ref->is_null != (uint32_t)(raw == UINT64_MAX) || ref->reserved)
            return fail(result, MB_ZEN_PACKAGE_IMPORTS_BAD_ARGUMENT, MB_ZEN_PACKAGE_IMPORTS_REASON_IMPORT_MAP);
        if ((kind == MB_ZEN_IMPORT_EXPORT && ref->export_index != (uint32_t)raw) ||
            (kind != MB_ZEN_IMPORT_EXPORT && ref->export_index != 0) ||
            (kind == MB_ZEN_IMPORT_PACKAGE &&
                (ref->imported_package_index != (uint32_t)(value >> 32) ||
                 ref->public_export_hash_index != (uint32_t)raw)) ||
            (kind != MB_ZEN_IMPORT_PACKAGE &&
                (ref->imported_package_index != 0 || ref->public_export_hash_index != 0)))
            return fail(result, MB_ZEN_PACKAGE_IMPORTS_BAD_ARGUMENT, MB_ZEN_PACKAGE_IMPORTS_REASON_IMPORT_MAP);
        if (kind == MB_ZEN_IMPORT_PACKAGE && (uint32_t)raw >= import_map->public_hash_count)
            return fail(result, MB_ZEN_PACKAGE_IMPORTS_MALFORMED, MB_ZEN_PACKAGE_IMPORTS_REASON_PUBLIC_HASH_INDEX);
    }
    for (unsigned i = 0; i < 4; ++i)
        if (kind_counts[i] != import_map->kind_counts[i])
            return fail(result, MB_ZEN_PACKAGE_IMPORTS_BAD_ARGUMENT, MB_ZEN_PACKAGE_IMPORTS_REASON_IMPORT_MAP);

    uint32_t binding_count = 0;
    for (uint32_t i = 0; i < import_map->reference_count; ++i) {
        if (references[i].kind != MB_ZEN_IMPORT_PACKAGE) continue;
        if (references[i].imported_package_index >= layout.count)
            return fail(result, MB_ZEN_PACKAGE_IMPORTS_MALFORMED, MB_ZEN_PACKAGE_IMPORTS_REASON_PACKAGE_INDEX);
        if (references[i].public_export_hash_index >= import_map->public_hash_count)
            return fail(result, MB_ZEN_PACKAGE_IMPORTS_MALFORMED, MB_ZEN_PACKAGE_IMPORTS_REASON_PUBLIC_HASH_INDEX);
        ++binding_count;
    }
    if (layout.count > names_capacity || layout.string_bytes > name_arena_capacity ||
        binding_count > limits->max_bindings || binding_count > bindings_capacity)
        return fail(result, MB_ZEN_PACKAGE_IMPORTS_RESOURCE_LIMIT, MB_ZEN_PACKAGE_IMPORTS_REASON_OUTPUT_CAPACITY);

    uint32_t arena_offset = 0;
    size_t string_offset = layout.strings_offset;
    for (uint32_t i = 0; i < layout.count; ++i) {
        const uint16_t encoded = read_be16(bytes + layout.headers_offset + (size_t)i * 2u);
        const int utf16 = (encoded & UINT16_C(0x8000)) != 0;
        const uint32_t length = utf16 ? (encoded & UINT16_C(0x7fff)) : encoded;
        const uint32_t byte_length = utf16 ? length * 2u : length;
        names[i].hash_raw = read_u64(bytes + layout.hashes_offset + (size_t)i * 8u);
        names[i].arena_offset = arena_offset;
        names[i].byte_length = byte_length;
        names[i].code_units = utf16 ? length : 0;
        names[i].encoding = utf16 ? MB_ZEN_NAME_UTF16LE : MB_ZEN_NAME_UTF8;
        names[i].number = read_i32(bytes + layout.numbers_offset + (size_t)i * 4u);
        if (byte_length) memcpy(name_arena + names[i].arena_offset, bytes + string_offset, byte_length);
        arena_offset += byte_length;
        string_offset += byte_length;
        names[i].reserved = 0;
    }

    uint32_t binding_index = 0;
    for (uint32_t i = 0; i < import_map->reference_count; ++i) {
        const mb_zen_import_ref_v1 *ref = references + i;
        if (ref->kind != MB_ZEN_IMPORT_PACKAGE) continue;
        mb_zen_package_import_binding_v1 *binding = bindings + binding_index++;
        binding->import_reference_index = i;
        binding->imported_package_index = ref->imported_package_index;
        binding->public_export_hash_index = ref->public_export_hash_index;
        binding->object_index_raw = ref->raw;
        binding->public_export_hash = read_u64(bytes + import_map->public_hash_offset +
            (size_t)ref->public_export_hash_index * 8u);
    }

    result->table_offset = layout.offset;
    result->table_bytes = layout.bytes;
    result->name_count = layout.count;
    result->name_bytes_used = layout.string_bytes;
    result->declared_string_bytes_raw = layout.declared_string_bytes;
    result->name_hash_version_raw = layout.hash_version;
    result->package_binding_count = binding_count;
    result->complete = 1;
    result->status = MB_ZEN_PACKAGE_IMPORTS_OK;
    result->reason = MB_ZEN_PACKAGE_IMPORTS_REASON_NONE;
    return MB_ZEN_PACKAGE_IMPORTS_OK;
}
