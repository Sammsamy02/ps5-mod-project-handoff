#include "summary.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; printf("FAIL line=%u %s\n", (unsigned)__LINE__, #x); } } while (0)
#define REQUIRE(x) do { CHECK(x); if (!(x)) return; } while (0)

static uint8_t fixture[256];
static mb_zen_summary_view_v1 view;
static mb_zen_summary_profile_v1 profile;
static mb_zen_summary_limits_v1 limits;
static mb_zen_summary_result_v1 result;
static mb_zen_name_v1 names[4];
static uint8_t arena[32];

static void put_le(uint8_t *p, uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) {
        p[i] = (uint8_t)value;
        value >>= 8;
    }
}

static void base(unsigned with_cells) {
    memset(fixture, 0, sizeof(fixture));
    put_le(fixture + 0, 0, 4); /* bHasVersioningInfo */
    put_le(fixture + 4, 128, 4); /* HeaderSize */
    put_le(fixture + 8, 1, 4); /* local Package NameIndex */
    put_le(fixture + 12, 7, 4); /* FMappedName ExtraIndex */
    put_le(fixture + 16, UINT32_C(0x80002200), 4); /* raw package flags */
    put_le(fixture + 20, 4096, 4); /* CookedHeaderSize may exceed HeaderSize */
    const int32_t offsets[7] = {112, 112, 116, 120, 124, 124, 128};
    for (unsigned i = 0; i < 7; ++i) put_le(fixture + 24 + 4 * i, (uint32_t)offsets[i], 4);

    size_t p = 52;
    if (with_cells) {
        put_le(fixture + p, 100, 4);
        put_le(fixture + p + 4, 101, 4);
        p += 8;
    }
    put_le(fixture + p, 2, 4); p += 4; /* signed name count */
    put_le(fixture + p, 200, 4); p += 4; /* retained, not trusted as a verified sum */
    put_le(fixture + p, UINT64_C(0x0102030405060708), 8); p += 8;
    put_le(fixture + p, UINT64_C(0x1122334455667788), 8); p += 8;
    put_le(fixture + p, UINT64_C(0x8877665544332211), 8); p += 8;
    fixture[p++] = 0; fixture[p++] = 2; /* UTF-8 byte length 2 */
    fixture[p++] = 0x80; fixture[p++] = 1; /* UTF-16 code-unit length 1 */
    fixture[p++] = 'A'; fixture[p++] = 'B';
    fixture[p++] = 'C'; fixture[p++] = 0;

    view = (mb_zen_summary_view_v1){sizeof(view), MB_ZEN_SUMMARY_ABI_VERSION,
        fixture, sizeof(fixture), sizeof(fixture)};
    profile = (mb_zen_summary_profile_v1){sizeof(profile), MB_ZEN_SUMMARY_ABI_VERSION,
        MB_ZEN_LAYOUT_UE5_3_PLUS, with_cells, 0};
    mb_zen_summary_limits_v1_default(&limits);
    memset(&result, 0xa5, sizeof(result));
    result.struct_size = sizeof(result);
    result.abi_version = MB_ZEN_SUMMARY_ABI_VERSION;
    memset(names, 0xa5, sizeof(names));
    memset(arena, 0xa5, sizeof(arena));
}

static void success_checks(void) {
    base(1);
    CHECK(mb_zen_read_summary_names_v1(&view, &profile, &limits, &result,
        names, 4, arena, sizeof(arena)) == MB_ZEN_SUMMARY_OK);
    CHECK(result.status == MB_ZEN_SUMMARY_OK && result.summary_complete && result.name_map_complete);
    CHECK(result.header_size == 128 && result.cooked_header_size == 4096);
    CHECK(result.package_flags_raw == UINT32_C(0x80002200));
    CHECK(result.mapped_name_type == MB_ZEN_MAPPED_PACKAGE && result.mapped_name_index == 1 && result.mapped_name_extra_index == 7);
    CHECK(result.mapped_name_resolved && result.name_count == 2 && result.declared_name_string_bytes == 200);
    CHECK(result.name_hash_version_raw == UINT64_C(0x0102030405060708));
    CHECK(result.cell_offsets_present && result.cell_import_map_offset == 100 && result.cell_export_map_offset == 101);
    CHECK(result.imported_public_export_hashes_offset == 112 && result.import_map_offset == 112);
    CHECK(result.export_map_offset == 116 && result.export_bundle_entries_offset == 120);
    CHECK(result.dependency_bundle_headers_offset == 124 && result.dependency_bundle_entries_offset == 124);
    CHECK(result.imported_package_names_offset == 128 && result.name_bytes_used == 4);
    CHECK(names[0].hash_raw == UINT64_C(0x1122334455667788));
    CHECK(names[0].encoding == MB_ZEN_NAME_UTF8 && names[0].byte_length == 2 && names[0].code_units == 0);
    CHECK(names[0].arena_offset == 0 && memcmp(arena, "AB", 2) == 0);
    CHECK(names[1].hash_raw == UINT64_C(0x8877665544332211));
    CHECK(names[1].encoding == MB_ZEN_NAME_UTF16LE && names[1].byte_length == 2 && names[1].code_units == 1);
    CHECK(names[1].arena_offset == 2 && arena[2] == 'C' && arena[3] == 0);

    base(0);
    CHECK(mb_zen_read_summary_names_v1(&view, &profile, &limits, &result,
        names, 4, arena, sizeof(arena)) == MB_ZEN_SUMMARY_OK);
    CHECK(!result.cell_offsets_present && result.name_count == 2 && result.mapped_name_resolved);

    base(1); put_le(fixture + 8, UINT32_C(0x40000000), 4);
    CHECK(mb_zen_read_summary_names_v1(&view, &profile, &limits, &result,
        names, 4, arena, sizeof(arena)) == MB_ZEN_SUMMARY_OK);
    CHECK(result.mapped_name_type == MB_ZEN_MAPPED_CONTAINER && !result.mapped_name_resolved);
}

static int all_zero(const void *ptr, size_t bytes) {
    const uint8_t *p = (const uint8_t *)ptr;
    for (size_t i = 0; i < bytes; ++i) if (p[i]) return 0;
    return 1;
}

static void rejected(mb_zen_summary_status expected) {
    const mb_zen_summary_status actual = mb_zen_read_summary_names_v1(&view, &profile,
        &limits, &result, names, 4, arena, sizeof(arena));
    if (actual != expected) printf("REJECT expected=%u actual=%u reason=%u\n", (unsigned)expected,
        (unsigned)actual, result.reason);
    CHECK(actual == expected && result.status == (uint32_t)expected);
    CHECK(!result.summary_complete && !result.name_map_complete && !result.name_count && !result.name_bytes_used);
    CHECK(all_zero(names, sizeof(names)) && all_zero(arena, sizeof(arena)));
}

static void failure_checks(void) {
    base(1); profile.summary_layout = 99; rejected(MB_ZEN_SUMMARY_UNSUPPORTED);
    base(1); put_le(fixture, 1, 4); rejected(MB_ZEN_SUMMARY_UNSUPPORTED);
    base(1); view.captured_bytes = 51; rejected(MB_ZEN_SUMMARY_TRUNCATED);
    base(1); view.captured_bytes = 128; put_le(fixture + 4, 129, 4); rejected(MB_ZEN_SUMMARY_TRUNCATED);
    base(1); put_le(fixture + 4, 99, 4); put_le(fixture + 24, 99, 4);
    put_le(fixture + 28, 99, 4); put_le(fixture + 32, 99, 4); put_le(fixture + 36, 99, 4);
    put_le(fixture + 40, 99, 4); put_le(fixture + 44, 99, 4); put_le(fixture + 48, 99, 4);
    put_le(fixture + 52, 99, 4); put_le(fixture + 56, 99, 4);
    rejected(MB_ZEN_SUMMARY_MALFORMED);
    base(1); put_le(fixture + 4, 59, 4); rejected(MB_ZEN_SUMMARY_MALFORMED);
    base(1); put_le(fixture + 24, UINT32_C(0xffffffff), 4); rejected(MB_ZEN_SUMMARY_MALFORMED);
    base(1); put_le(fixture + 60, UINT32_C(0xffffffff), 4); rejected(MB_ZEN_SUMMARY_MALFORMED);
    base(1); put_le(fixture + 60, 4097, 4); rejected(MB_ZEN_SUMMARY_RESOURCE_LIMIT);
    base(1); put_le(fixture + 8, 2, 4); rejected(MB_ZEN_SUMMARY_MALFORMED);
    base(1); put_le(fixture + 8, UINT32_C(0xc0000000) | 1, 4); rejected(MB_ZEN_SUMMARY_UNSUPPORTED);
    base(1); limits.max_name_bytes = 3; rejected(MB_ZEN_SUMMARY_RESOURCE_LIMIT);
    base(1); limits.max_header_bytes = 127; rejected(MB_ZEN_SUMMARY_RESOURCE_LIMIT);
    base(1);
    CHECK(mb_zen_read_summary_names_v1(&view, &profile, &limits, &result,
        names, 1, arena, sizeof(arena)) == MB_ZEN_SUMMARY_RESOURCE_LIMIT);
    CHECK(!result.summary_complete && !result.name_map_complete && !result.name_count);
    CHECK(all_zero(names, sizeof(names[0])) && all_zero(arena, sizeof(arena)));
    base(1); view.logical_package_bytes = 127; rejected(MB_ZEN_SUMMARY_OUT_OF_RANGE);

    union { max_align_t alignment; uint8_t bytes[sizeof(mb_zen_summary_result_v1)]; } alias;
    memset(alias.bytes, 0x5a, sizeof(alias.bytes));
    put_le(alias.bytes, sizeof(mb_zen_summary_result_v1), 4);
    put_le(alias.bytes + 4, MB_ZEN_SUMMARY_ABI_VERSION, 4);
    view.header_bytes = alias.bytes;
    view.captured_bytes = sizeof(alias.bytes);
    uint8_t alias_before[sizeof(alias.bytes)]; memcpy(alias_before, alias.bytes, sizeof(alias.bytes));
    const mb_zen_summary_status alias_status = mb_zen_read_summary_names_v1(&view,
        &profile, &limits, (mb_zen_summary_result_v1 *)(void *)alias.bytes,
        names, 4, arena, sizeof(arena));
    CHECK(alias_status == MB_ZEN_SUMMARY_BAD_ARGUMENT && memcmp(alias.bytes, alias_before, sizeof(alias.bytes)) == 0);
}

static int actual_mode(const char *path, const char *logical_bytes_text) {
    char *end = NULL;
    const uint64_t logical_bytes = strtoull(logical_bytes_text, &end, 10);
    if (!end || end == logical_bytes_text || *end || logical_bytes_text[0] == '-') return 6;
    uint8_t bytes[MB_ZEN_SUMMARY_MAX_HEADER_BYTES];
    FILE *file = fopen(path, "rb");
    if (!file) return 2;
    const size_t count = fread(bytes, 1, sizeof(bytes), file);
    const int extra = fgetc(file);
    const int read_error = ferror(file);
    fclose(file);
    if (read_error || extra != EOF || !count) return 3;
    view = (mb_zen_summary_view_v1){sizeof(view), MB_ZEN_SUMMARY_ABI_VERSION,
        bytes, count, logical_bytes};
    profile = (mb_zen_summary_profile_v1){sizeof(profile), MB_ZEN_SUMMARY_ABI_VERSION,
        MB_ZEN_LAYOUT_UE5_3_PLUS, 1, 0};
    mb_zen_summary_limits_v1_default(&limits);
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    result.abi_version = MB_ZEN_SUMMARY_ABI_VERSION;
    mb_zen_name_v1 actual_names[MB_ZEN_SUMMARY_MAX_NAMES];
    uint8_t actual_arena[MB_ZEN_SUMMARY_MAX_NAME_BYTES];
    const mb_zen_summary_status status = mb_zen_read_summary_names_v1(&view, &profile, &limits, &result,
        actual_names, MB_ZEN_SUMMARY_MAX_NAMES, actual_arena, sizeof(actual_arena));
    if (status != MB_ZEN_SUMMARY_OK) {
        fprintf(stderr, "actual parse status=%u reason=%u\n", (unsigned)status, result.reason);
        return 4;
    }
    if (!result.mapped_name_resolved || result.mapped_name_index >= result.name_count) return 5;
    const mb_zen_name_v1 package_name = actual_names[result.mapped_name_index];
    printf("{\"header_size\":%u,\"mapped_name_index\":%u,\"mapped_name_type\":%u,"
        "\"mapped_name_extra_index\":%u,\"package_flags_raw\":%u,\"cooked_header_size\":%u,"
        "\"name_count\":%u,\"declared_name_string_bytes\":%u,\"name_hash_version_raw\":\"%" PRIu64 "\","
        "\"name_bytes_used\":%u,\"cell_import_map_offset\":%u,\"cell_export_map_offset\":%u,"
        "\"summary_offsets\":[%d,%d,%d,%d,%d,%d,%d],\"package_name_encoding\":%u,"
        "\"package_name_byte_length\":%u,\"package_name_hex\":\"",
        result.header_size, result.mapped_name_index, result.mapped_name_type,
        result.mapped_name_extra_index, result.package_flags_raw, result.cooked_header_size,
        result.name_count, result.declared_name_string_bytes, result.name_hash_version_raw,
        result.name_bytes_used, result.cell_import_map_offset, result.cell_export_map_offset,
        result.imported_public_export_hashes_offset, result.import_map_offset,
        result.export_map_offset, result.export_bundle_entries_offset,
        result.dependency_bundle_headers_offset, result.dependency_bundle_entries_offset,
        result.imported_package_names_offset, package_name.encoding, package_name.byte_length);
    for (uint32_t i = 0; i < package_name.byte_length; ++i) printf("%02x", actual_arena[package_name.arena_offset + i]);
    puts("\"}");
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "--actual") == 0) return actual_mode(argv[2], argv[3]);
    if (argc != 1) return 64;
    success_checks();
    failure_checks();
    printf("zen-summary groups=2 checks=%u failures=%u\n", checks, failures);
    return failures ? 1 : 0;
}
