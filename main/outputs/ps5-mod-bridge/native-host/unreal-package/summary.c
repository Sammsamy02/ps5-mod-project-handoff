#include "summary.h"

#include <limits.h>
#include <string.h>

typedef struct span_range {
    uintptr_t begin;
    uintptr_t end;
    int present;
} span_range;

typedef struct parsed_summary {
    uint32_t versioning_flag;
    uint32_t header_size;
    uint32_t mapped_name_word;
    uint32_t mapped_name_extra;
    uint32_t package_flags;
    uint32_t cooked_header_size;
    int32_t offsets[7];
    uint32_t cells_present;
    int32_t cell_offsets[2];
    uint32_t name_count;
    uint32_t declared_string_bytes;
    uint64_t hash_version;
    size_t hashes_offset;
    size_t name_headers_offset;
    size_t strings_offset;
    size_t name_bytes;
    size_t name_end;
} parsed_summary;

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t read_u64(const uint8_t *p) {
    return (uint64_t)read_u32(p) | ((uint64_t)read_u32(p + 4) << 32);
}

static int32_t read_i32(const uint8_t *p) {
    const uint32_t raw = read_u32(p);
    if (raw <= INT32_MAX) return (int32_t)raw;
    return (int32_t)(-((int64_t)(UINT32_MAX - raw)) - 1);
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

static int ranges_overlap(span_range a, span_range b) {
    return a.present && b.present && a.begin < b.end && b.begin < a.end;
}

static int aligned_for(const void *ptr, size_t alignment) {
    return ptr && ((uintptr_t)ptr % alignment) == 0;
}

static int all_outputs_disjoint(const mb_zen_summary_view_v1 *view,
    const mb_zen_summary_profile_v1 *profile, const mb_zen_summary_limits_v1 *limits,
    mb_zen_summary_result_v1 *result, mb_zen_name_v1 *names,
    uint32_t names_capacity, uint8_t *arena, uint32_t arena_capacity) {
    span_range ranges[7];
    size_t records_bytes;
    if (!mul_size((size_t)names_capacity, sizeof(*names), &records_bytes)) return 0;
    if (!range_for(view, sizeof(*view), &ranges[0]) ||
        !range_for(profile, sizeof(*profile), &ranges[1]) ||
        !range_for(limits, limits ? sizeof(*limits) : 0, &ranges[2]) ||
        !range_for(view->header_bytes, view->captured_bytes, &ranges[3]) ||
        !range_for(result, sizeof(*result), &ranges[4]) ||
        !range_for(names, records_bytes, &ranges[5]) ||
        !range_for(arena, arena_capacity, &ranges[6])) return 0;
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 4; j < 7; ++j) {
            if (ranges_overlap(ranges[i], ranges[j])) return 0;
        }
    }
    for (size_t i = 4; i < 7; ++i) {
        for (size_t j = i + 1; j < 7; ++j) {
            if (ranges_overlap(ranges[i], ranges[j])) return 0;
        }
    }
    return 1;
}

static void clear_admitted_outputs(mb_zen_summary_result_v1 *result,
    mb_zen_name_v1 *names, uint32_t names_capacity,
    uint8_t *arena, uint32_t arena_capacity) {
    memset(result, 0, sizeof(*result));
    result->struct_size = (uint32_t)sizeof(*result);
    result->abi_version = MB_ZEN_SUMMARY_ABI_VERSION;
    if (names_capacity) memset(names, 0, (size_t)names_capacity * sizeof(*names));
    if (arena_capacity) memset(arena, 0, arena_capacity);
}

static mb_zen_summary_status fail(mb_zen_summary_result_v1 *result,
    mb_zen_summary_status status, mb_zen_summary_reason reason) {
    result->status = (uint32_t)status;
    result->reason = (uint32_t)reason;
    return status;
}

void mb_zen_summary_limits_v1_default(mb_zen_summary_limits_v1 *limits) {
    if (!limits) return;
    *limits = (mb_zen_summary_limits_v1){
        (uint32_t)sizeof(*limits), MB_ZEN_SUMMARY_ABI_VERSION,
        MB_ZEN_SUMMARY_MAX_HEADER_BYTES, MB_ZEN_SUMMARY_MAX_NAMES,
        MB_ZEN_SUMMARY_MAX_NAME_BYTES, 0
    };
}

static mb_zen_summary_status parse_summary(const mb_zen_summary_view_v1 *view,
    const mb_zen_summary_profile_v1 *profile, const mb_zen_summary_limits_v1 *limits,
    parsed_summary *parsed, mb_zen_summary_reason *reason) {
    if (profile->summary_layout != MB_ZEN_LAYOUT_UE5_3_PLUS ||
        profile->verse_cells_present > 1 || profile->reserved) {
        *reason = MB_ZEN_SUMMARY_REASON_PROFILE;
        return MB_ZEN_SUMMARY_UNSUPPORTED;
    }
    if (view->captured_bytes > limits->max_header_bytes ||
        view->captured_bytes > MB_ZEN_SUMMARY_MAX_HEADER_BYTES) {
        *reason = MB_ZEN_SUMMARY_REASON_CAPTURE;
        return MB_ZEN_SUMMARY_RESOURCE_LIMIT;
    }
    if (view->captured_bytes < 4) {
        *reason = MB_ZEN_SUMMARY_REASON_CAPTURE;
        return MB_ZEN_SUMMARY_TRUNCATED;
    }
    const uint8_t *bytes = view->header_bytes;
    parsed->versioning_flag = read_u32(bytes);
    if (parsed->versioning_flag != 0) {
        *reason = MB_ZEN_SUMMARY_REASON_VERSIONING;
        return MB_ZEN_SUMMARY_UNSUPPORTED;
    }
    if (view->captured_bytes < 52) {
        *reason = MB_ZEN_SUMMARY_REASON_CAPTURE;
        return MB_ZEN_SUMMARY_TRUNCATED;
    }
    parsed->header_size = read_u32(bytes + 4);
    parsed->mapped_name_word = read_u32(bytes + 8);
    parsed->mapped_name_extra = read_u32(bytes + 12);
    parsed->package_flags = read_u32(bytes + 16);
    parsed->cooked_header_size = read_u32(bytes + 20);
    for (unsigned i = 0; i < 7; ++i) parsed->offsets[i] = read_i32(bytes + 24 + i * 4);

    if (!parsed->header_size || parsed->header_size > limits->max_header_bytes ||
        parsed->header_size > MB_ZEN_SUMMARY_MAX_HEADER_BYTES) {
        *reason = MB_ZEN_SUMMARY_REASON_HEADER_SIZE;
        return parsed->header_size > limits->max_header_bytes ||
            parsed->header_size > MB_ZEN_SUMMARY_MAX_HEADER_BYTES ?
            MB_ZEN_SUMMARY_RESOURCE_LIMIT : MB_ZEN_SUMMARY_MALFORMED;
    }
    if ((uint64_t)parsed->header_size > view->logical_package_bytes) {
        *reason = MB_ZEN_SUMMARY_REASON_HEADER_SIZE;
        return MB_ZEN_SUMMARY_OUT_OF_RANGE;
    }
    if (parsed->header_size > view->captured_bytes) {
        *reason = MB_ZEN_SUMMARY_REASON_CAPTURE;
        return MB_ZEN_SUMMARY_TRUNCATED;
    }
    const size_t minimum = 52u + (profile->verse_cells_present ? 8u : 0u) + 4u;
    if (parsed->header_size < minimum) {
        *reason = MB_ZEN_SUMMARY_REASON_HEADER_SIZE;
        return MB_ZEN_SUMMARY_MALFORMED;
    }
    for (unsigned i = 0; i < 7; ++i) {
        if (parsed->offsets[i] < 0 || (uint32_t)parsed->offsets[i] > parsed->header_size) {
            *reason = MB_ZEN_SUMMARY_REASON_OFFSET;
            return MB_ZEN_SUMMARY_MALFORMED;
        }
    }
    parsed->cells_present = profile->verse_cells_present;
    if (parsed->cells_present) {
        parsed->cell_offsets[0] = read_i32(bytes + 52);
        parsed->cell_offsets[1] = read_i32(bytes + 56);
        for (unsigned i = 0; i < 2; ++i) {
            if (parsed->cell_offsets[i] < 0 || (uint32_t)parsed->cell_offsets[i] > parsed->header_size) {
                *reason = MB_ZEN_SUMMARY_REASON_OFFSET;
                return MB_ZEN_SUMMARY_MALFORMED;
            }
        }
    }
    size_t cursor = minimum - 4u;
    const int32_t signed_count = read_i32(bytes + cursor);
    cursor += 4u;
    if (signed_count < 0) {
        *reason = MB_ZEN_SUMMARY_REASON_NAME_COUNT;
        return MB_ZEN_SUMMARY_MALFORMED;
    }
    if ((uint32_t)signed_count > limits->max_names ||
        (uint32_t)signed_count > MB_ZEN_SUMMARY_MAX_NAMES) {
        *reason = MB_ZEN_SUMMARY_REASON_NAME_COUNT;
        return MB_ZEN_SUMMARY_RESOURCE_LIMIT;
    }
    parsed->name_count = (uint32_t)signed_count;
    if (parsed->name_count > 0) {
        size_t hashes_bytes, headers_bytes, end;
        if (!add_size(cursor, 12u, &end) || end > parsed->header_size) {
            *reason = MB_ZEN_SUMMARY_REASON_NAME_MAP;
            return MB_ZEN_SUMMARY_MALFORMED;
        }
        parsed->declared_string_bytes = read_u32(bytes + cursor);
        parsed->hash_version = read_u64(bytes + cursor + 4);
        cursor = end;
        parsed->hashes_offset = cursor;
        if (!mul_size(parsed->name_count, 8u, &hashes_bytes) ||
            !add_size(cursor, hashes_bytes, &parsed->name_headers_offset) ||
            !mul_size(parsed->name_count, 2u, &headers_bytes) ||
            !add_size(parsed->name_headers_offset, headers_bytes, &parsed->strings_offset) ||
            parsed->strings_offset > parsed->header_size) {
            *reason = MB_ZEN_SUMMARY_REASON_NAME_MAP;
            return MB_ZEN_SUMMARY_MALFORMED;
        }
        cursor = parsed->strings_offset;
        for (uint32_t i = 0; i < parsed->name_count; ++i) {
            const uint8_t *header = bytes + parsed->name_headers_offset + (size_t)i * 2u;
            const uint32_t length = (((uint32_t)header[0] & 0x7fu) << 8) | header[1];
            size_t string_bytes = length;
            if (header[0] & 0x80u) {
                if (!mul_size(length, 2u, &string_bytes)) {
                    *reason = MB_ZEN_SUMMARY_REASON_NAME_MAP;
                    return MB_ZEN_SUMMARY_MALFORMED;
                }
            }
            if (!add_size(parsed->name_bytes, string_bytes, &parsed->name_bytes) ||
                parsed->name_bytes > limits->max_name_bytes ||
                parsed->name_bytes > MB_ZEN_SUMMARY_MAX_NAME_BYTES) {
                *reason = MB_ZEN_SUMMARY_REASON_NAME_MAP;
                return MB_ZEN_SUMMARY_RESOURCE_LIMIT;
            }
            if (!add_size(cursor, string_bytes, &cursor) || cursor > parsed->header_size) {
                *reason = MB_ZEN_SUMMARY_REASON_NAME_MAP;
                return MB_ZEN_SUMMARY_MALFORMED;
            }
        }
        parsed->name_end = cursor;
    } else {
        parsed->hashes_offset = cursor;
        parsed->name_headers_offset = cursor;
        parsed->strings_offset = cursor;
        parsed->name_end = cursor;
    }
    if (parsed->name_end > view->captured_bytes) {
        *reason = MB_ZEN_SUMMARY_REASON_CAPTURE;
        return MB_ZEN_SUMMARY_TRUNCATED;
    }
    const uint32_t mapped_type = parsed->mapped_name_word >> 30;
    const uint32_t mapped_index = parsed->mapped_name_word & UINT32_C(0x3fffffff);
    if (mapped_type > MB_ZEN_MAPPED_GLOBAL) {
        *reason = MB_ZEN_SUMMARY_REASON_NAME_TYPE;
        return MB_ZEN_SUMMARY_UNSUPPORTED;
    }
    if (mapped_type == MB_ZEN_MAPPED_PACKAGE && mapped_index >= parsed->name_count) {
        *reason = MB_ZEN_SUMMARY_REASON_NAME_INDEX;
        return MB_ZEN_SUMMARY_MALFORMED;
    }
    return MB_ZEN_SUMMARY_OK;
}

mb_zen_summary_status mb_zen_read_summary_names_v1(
    const mb_zen_summary_view_v1 *view,
    const mb_zen_summary_profile_v1 *profile,
    const mb_zen_summary_limits_v1 *given_limits,
    mb_zen_summary_result_v1 *result,
    mb_zen_name_v1 *names,
    uint32_t names_capacity,
    uint8_t *name_arena,
    uint32_t name_arena_capacity) {
    if (!view || !profile || !result ||
        !aligned_for(view, _Alignof(mb_zen_summary_view_v1)) ||
        !aligned_for(profile, _Alignof(mb_zen_summary_profile_v1)) ||
        !aligned_for(result, _Alignof(mb_zen_summary_result_v1)) ||
        (given_limits && !aligned_for(given_limits, _Alignof(mb_zen_summary_limits_v1))) ||
        (names_capacity && (!names || !aligned_for(names, _Alignof(mb_zen_name_v1)))) ||
        (name_arena_capacity && !name_arena) ||
        names_capacity > MB_ZEN_SUMMARY_MAX_NAMES ||
        name_arena_capacity > MB_ZEN_SUMMARY_MAX_NAME_BYTES ||
        view->struct_size != sizeof(*view) || view->abi_version != MB_ZEN_SUMMARY_ABI_VERSION ||
        profile->struct_size != sizeof(*profile) || profile->abi_version != MB_ZEN_SUMMARY_ABI_VERSION ||
        (given_limits && (given_limits->struct_size != sizeof(*given_limits) ||
            given_limits->abi_version != MB_ZEN_SUMMARY_ABI_VERSION)) ||
        result->struct_size != sizeof(*result) || result->abi_version != MB_ZEN_SUMMARY_ABI_VERSION) {
        return MB_ZEN_SUMMARY_BAD_ARGUMENT;
    }
    mb_zen_summary_limits_v1 defaults;
    mb_zen_summary_limits_v1_default(&defaults);
    const mb_zen_summary_limits_v1 *limits = given_limits ? given_limits : &defaults;
    if (limits->reserved || limits->max_header_bytes > MB_ZEN_SUMMARY_MAX_HEADER_BYTES ||
        limits->max_names > MB_ZEN_SUMMARY_MAX_NAMES ||
        limits->max_name_bytes > MB_ZEN_SUMMARY_MAX_NAME_BYTES ||
        (view->captured_bytes && !view->header_bytes) ||
        !all_outputs_disjoint(view, profile, given_limits, result, names,
            names_capacity, name_arena, name_arena_capacity)) {
        return MB_ZEN_SUMMARY_BAD_ARGUMENT;
    }

    clear_admitted_outputs(result, names, names_capacity, name_arena, name_arena_capacity);
    parsed_summary parsed;
    memset(&parsed, 0, sizeof(parsed));
    mb_zen_summary_reason reason = MB_ZEN_SUMMARY_REASON_NONE;
    mb_zen_summary_status status = parse_summary(view, profile, limits, &parsed, &reason);
    if (status != MB_ZEN_SUMMARY_OK) return fail(result, status, reason);
    if (parsed.name_count > names_capacity || parsed.name_bytes > name_arena_capacity) {
        return fail(result, MB_ZEN_SUMMARY_RESOURCE_LIMIT, MB_ZEN_SUMMARY_REASON_OUTPUT_CAPACITY);
    }

    const uint8_t *bytes = view->header_bytes;
    size_t string_cursor = parsed.strings_offset;
    size_t arena_cursor = 0;
    for (uint32_t i = 0; i < parsed.name_count; ++i) {
        const uint8_t *raw_header = bytes + parsed.name_headers_offset + (size_t)i * 2u;
        const uint32_t length = (((uint32_t)raw_header[0] & 0x7fu) << 8) | raw_header[1];
        const uint32_t encoding = (raw_header[0] & 0x80u) ? MB_ZEN_NAME_UTF16LE : MB_ZEN_NAME_UTF8;
        const uint32_t byte_length = encoding == MB_ZEN_NAME_UTF16LE ? length * 2u : length;
        names[i].hash_raw = read_u64(bytes + parsed.hashes_offset + (size_t)i * 8u);
        names[i].arena_offset = (uint32_t)arena_cursor;
        names[i].byte_length = byte_length;
        names[i].code_units = encoding == MB_ZEN_NAME_UTF16LE ? length : 0;
        names[i].encoding = encoding;
        if (byte_length) memcpy(name_arena + arena_cursor, bytes + string_cursor, byte_length);
        arena_cursor += byte_length;
        string_cursor += byte_length;
    }

    result->versioning_info_flag_raw = parsed.versioning_flag;
    result->header_size = parsed.header_size;
    result->mapped_name_type = parsed.mapped_name_word >> 30;
    result->mapped_name_index = parsed.mapped_name_word & UINT32_C(0x3fffffff);
    result->mapped_name_extra_index = parsed.mapped_name_extra;
    result->mapped_name_resolved = result->mapped_name_type == MB_ZEN_MAPPED_PACKAGE;
    result->package_flags_raw = parsed.package_flags;
    result->cooked_header_size = parsed.cooked_header_size;
    result->name_count = parsed.name_count;
    result->declared_name_string_bytes = parsed.declared_string_bytes;
    result->name_hash_version_raw = parsed.hash_version;
    result->name_bytes_used = (uint32_t)parsed.name_bytes;
    result->cell_offsets_present = parsed.cells_present;
    result->cell_import_map_offset = parsed.cell_offsets[0];
    result->cell_export_map_offset = parsed.cell_offsets[1];
    result->imported_public_export_hashes_offset = parsed.offsets[0];
    result->import_map_offset = parsed.offsets[1];
    result->export_map_offset = parsed.offsets[2];
    result->export_bundle_entries_offset = parsed.offsets[3];
    result->dependency_bundle_headers_offset = parsed.offsets[4];
    result->dependency_bundle_entries_offset = parsed.offsets[5];
    result->imported_package_names_offset = parsed.offsets[6];
    result->summary_complete = 1;
    result->name_map_complete = 1;
    result->status = MB_ZEN_SUMMARY_OK;
    result->reason = MB_ZEN_SUMMARY_REASON_NONE;
    return MB_ZEN_SUMMARY_OK;
}
