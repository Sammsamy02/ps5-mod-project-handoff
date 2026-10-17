#include "package_imports.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static unsigned failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; fprintf(stderr, "FAIL line=%u %s\n", (unsigned)__LINE__, #x); } } while (0)

enum { SYNTHETIC_HEADER_BYTES = 165, SYNTHETIC_TABLE_OFFSET = 96,
       NAME_CAPACITY = 8, ARENA_CAPACITY = 64, BINDING_CAPACITY = 8 };
static _Alignas(8) uint8_t package_bytes[256];
static uint8_t saved_bytes[256];
static mb_zen_summary_view_v1 view;
static mb_zen_summary_profile_v1 summary_profile;
static mb_zen_summary_limits_v1 summary_limits;
static mb_zen_summary_result_v1 summary;
static mb_zen_import_limits_v1 import_limits;
static mb_zen_import_result_v1 import_map;
static mb_zen_import_ref_v1 references[8];
static mb_zen_package_imports_limits_v1 limits;
static mb_zen_package_imports_result_v1 result;
static mb_zen_imported_package_name_v1 names[NAME_CAPACITY];
static uint8_t name_arena[ARENA_CAPACITY];
static mb_zen_package_import_binding_v1 bindings[BINDING_CAPACITY];

static void put_le(uint8_t *p, uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) { p[i] = (uint8_t)value; value >>= 8; }
}
static uint64_t package_import(uint32_t name_index, uint32_t hash_index) {
    return (UINT64_C(2) << 62) | ((uint64_t)name_index << 32) | hash_index;
}
static int all_zero(const void *ptr, size_t bytes) {
    const uint8_t *p = ptr; while (bytes--) if (*p++) return 0; return 1;
}
static void reset_outputs(void) {
    memset(&result, 0xa5, sizeof(result));
    result.struct_size = sizeof(result); result.abi_version = MB_ZEN_PACKAGE_IMPORTS_ABI_VERSION;
    memset(names, 0xa5, sizeof(names)); memset(name_arena, 0xa5, sizeof(name_arena));
    memset(bindings, 0xa5, sizeof(bindings));
}
static void base(void) {
    memset(package_bytes, 0, sizeof(package_bytes));
    put_le(package_bytes + 4, SYNTHETIC_HEADER_BYTES, 4);
    put_le(package_bytes + 8, UINT32_C(0x80000000), 4); /* External Global mapped name. */
    put_le(package_bytes + 20, SYNTHETIC_HEADER_BYTES, 4);
    put_le(package_bytes + 24, 56, 4); put_le(package_bytes + 28, 72, 4);
    put_le(package_bytes + 32, 96, 4); put_le(package_bytes + 36, 96, 4);
    put_le(package_bytes + 40, 96, 4); put_le(package_bytes + 44, 96, 4);
    put_le(package_bytes + 48, SYNTHETIC_TABLE_OFFSET, 4);
    put_le(package_bytes + 52, 0, 4); /* Local summary name table is empty. */
    put_le(package_bytes + 56, UINT64_C(0x1111222233334444), 8);
    put_le(package_bytes + 64, UINT64_C(0xaaaabbbbccccdddd), 8);
    put_le(package_bytes + 72, package_import(1, 0), 8);
    put_le(package_bytes + 80, package_import(2, 1), 8);
    put_le(package_bytes + 88, UINT64_C(42), 8); /* One opaque export reference. */

    uint8_t *t = package_bytes + SYNTHETIC_TABLE_OFFSET;
    put_le(t, 3, 4); put_le(t + 4, 11, 4);
    put_le(t + 8, MB_ZEN_PACKAGE_NAME_HASH_VERSION, 8);
    put_le(t + 16, UINT64_C(0x0102030405060708), 8);
    put_le(t + 24, UINT64_C(0x1112131415161718), 8);
    put_le(t + 32, UINT64_C(0x2122232425262728), 8);
    t[40] = 0x00; t[41] = 0x03; /* UTF-8, three bytes. */
    t[42] = 0x80; t[43] = 0x01; /* UTF-16LE, one code unit. */
    t[44] = 0x00; t[45] = 0x06; /* UTF-8, six bytes. */
    memcpy(t + 46, "Pkg", 3); t[49] = 0x2b; t[50] = 0x73;
    memcpy(t + 51, "Legacy", 6);
    put_le(t + 57, 0, 4); put_le(t + 61, 1, 4);
    put_le(t + 65, UINT32_C(0x80000000), 4); /* Preserve signed INT32_MIN. */

    view = (mb_zen_summary_view_v1){sizeof(view), MB_ZEN_SUMMARY_ABI_VERSION,
        package_bytes, SYNTHETIC_HEADER_BYTES, SYNTHETIC_HEADER_BYTES};
    summary_profile = (mb_zen_summary_profile_v1){sizeof(summary_profile),
        MB_ZEN_SUMMARY_ABI_VERSION, MB_ZEN_LAYOUT_UE5_3_PLUS, 0, 0};
    mb_zen_summary_limits_v1_default(&summary_limits);
    memset(&summary, 0, sizeof(summary)); summary.struct_size = sizeof(summary);
    summary.abi_version = MB_ZEN_SUMMARY_ABI_VERSION;
    mb_zen_import_limits_v1_default(&import_limits);
    memset(&import_map, 0, sizeof(import_map)); import_map.struct_size = sizeof(import_map);
    import_map.abi_version = MB_ZEN_IMPORT_MAP_ABI_VERSION;
    memset(references, 0xa5, sizeof(references));
    mb_zen_package_imports_limits_v1_default(&limits);
    reset_outputs();
}
static int prepare_inputs(void) {
    if (mb_zen_read_summary_names_v1(&view, &summary_profile, &summary_limits,
            &summary, NULL, 0, NULL, 0) != MB_ZEN_SUMMARY_OK) return 0;
    if (mb_zen_read_import_map_v1(&view, &summary, &import_limits,
            &import_map, references, 8) != MB_ZEN_IMPORT_MAP_OK) return 0;
    return 1;
}
static mb_zen_package_imports_status read_package_names(void) {
    return mb_zen_read_package_imports_v1(&view, &summary, &import_map,
        references, 8, &limits, &result, names, NAME_CAPACITY, name_arena,
        ARENA_CAPACITY, bindings, BINDING_CAPACITY);
}
static void admitted_failure(mb_zen_package_imports_status expected) {
    reset_outputs();
    CHECK(read_package_names() == expected);
    CHECK(result.status == (uint32_t)expected && result.complete == 0);
    CHECK(result.name_count == 0 && result.package_binding_count == 0 && result.table_bytes == 0);
    CHECK(all_zero(names, sizeof(names)) && all_zero(name_arena, sizeof(name_arena)));
    CHECK(all_zero(bindings, sizeof(bindings)));
}
static void test_complete_table_and_bindings(void) {
    base(); CHECK(prepare_inputs());
    CHECK(read_package_names() == MB_ZEN_PACKAGE_IMPORTS_OK);
    CHECK(result.complete == 1 && result.table_offset == 96 && result.table_bytes == 69);
    CHECK(result.name_count == 3 && result.name_bytes_used == 11 && result.declared_string_bytes_raw == 11);
    CHECK(result.name_hash_version_raw == MB_ZEN_PACKAGE_NAME_HASH_VERSION && result.package_binding_count == 2);
    CHECK(names[0].hash_raw == UINT64_C(0x0102030405060708) && names[0].encoding == MB_ZEN_NAME_UTF8);
    CHECK(names[0].byte_length == 3 && names[0].code_units == 0 && names[0].number == 0);
    CHECK(memcmp(name_arena + names[0].arena_offset, "Pkg", 3) == 0);
    CHECK(names[1].encoding == MB_ZEN_NAME_UTF16LE && names[1].byte_length == 2);
    CHECK(names[1].code_units == 1 && names[1].number == 1);
    CHECK(name_arena[names[1].arena_offset] == 0x2b && name_arena[names[1].arena_offset + 1] == 0x73);
    CHECK(names[2].encoding == MB_ZEN_NAME_UTF8 && names[2].byte_length == 6);
    CHECK(names[2].number == INT32_MIN && memcmp(name_arena + names[2].arena_offset, "Legacy", 6) == 0);
    CHECK(bindings[0].import_reference_index == 0 && bindings[0].imported_package_index == 1);
    CHECK(bindings[0].public_export_hash_index == 0 && bindings[0].object_index_raw == package_import(1, 0));
    CHECK(bindings[0].public_export_hash == UINT64_C(0x1111222233334444));
    CHECK(bindings[1].import_reference_index == 1 && bindings[1].imported_package_index == 2);
    CHECK(bindings[1].public_export_hash_index == 1 && bindings[1].object_index_raw == package_import(2, 1));
    CHECK(bindings[1].public_export_hash == UINT64_C(0xaaaabbbbccccdddd));
}
static void test_declared_string_bytes_remain_raw(void) {
    base(); put_le(package_bytes + 100, 999, 4); CHECK(prepare_inputs());
    CHECK(read_package_names() == MB_ZEN_PACKAGE_IMPORTS_OK);
    CHECK(result.declared_string_bytes_raw == 999 && result.name_bytes_used == 11 && result.complete == 1);
}
static void test_invalid_package_name_index_clears_all(void) {
    base(); put_le(package_bytes + 72, package_import(3, 0), 8); CHECK(prepare_inputs());
    admitted_failure(MB_ZEN_PACKAGE_IMPORTS_MALFORMED);
    CHECK(result.reason == MB_ZEN_PACKAGE_IMPORTS_REASON_PACKAGE_INDEX);
}
static void test_invalid_public_hash_index_clears_all(void) {
    base(); CHECK(prepare_inputs());
    put_le(package_bytes + 72, package_import(1, 2), 8);
    references[0].raw = package_import(1, 2); references[0].value = ((uint64_t)1 << 32) | 2;
    references[0].kind = MB_ZEN_IMPORT_PACKAGE; references[0].imported_package_index = 1;
    references[0].public_export_hash_index = 2;
    admitted_failure(MB_ZEN_PACKAGE_IMPORTS_MALFORMED);
    CHECK(result.reason == MB_ZEN_PACKAGE_IMPORTS_REASON_PUBLIC_HASH_INDEX);
}
static void test_short_number_array_inside_header_clears_all(void) {
    base(); put_le(package_bytes + 4, SYNTHETIC_HEADER_BYTES - 1, 4);
    view.captured_bytes = SYNTHETIC_HEADER_BYTES - 1; view.logical_package_bytes = SYNTHETIC_HEADER_BYTES;
    CHECK(prepare_inputs()); admitted_failure(MB_ZEN_PACKAGE_IMPORTS_MALFORMED);
    CHECK(result.reason == MB_ZEN_PACKAGE_IMPORTS_REASON_NUMBER_ARRAY);
}
static void test_capture_truncation_clears_all(void) {
    base(); CHECK(prepare_inputs()); view.captured_bytes = SYNTHETIC_HEADER_BYTES - 1;
    admitted_failure(MB_ZEN_PACKAGE_IMPORTS_TRUNCATED);
    CHECK(result.reason == MB_ZEN_PACKAGE_IMPORTS_REASON_CAPTURE);
}
static void test_unexpected_hash_version_is_unsupported(void) {
    base(); put_le(package_bytes + 104, 7, 8); CHECK(prepare_inputs());
    admitted_failure(MB_ZEN_PACKAGE_IMPORTS_UNSUPPORTED);
    CHECK(result.reason == MB_ZEN_PACKAGE_IMPORTS_REASON_HASH_VERSION);
}
static void test_name_cap_clears_all(void) {
    base(); CHECK(prepare_inputs()); limits.max_names = 2;
    admitted_failure(MB_ZEN_PACKAGE_IMPORTS_RESOURCE_LIMIT);
    CHECK(result.reason == MB_ZEN_PACKAGE_IMPORTS_REASON_NAME_COUNT);
}
static void test_changed_reference_record_is_rejected(void) {
    base(); CHECK(prepare_inputs()); references[0].public_export_hash_index = 1;
    admitted_failure(MB_ZEN_PACKAGE_IMPORTS_BAD_ARGUMENT);
    CHECK(result.reason == MB_ZEN_PACKAGE_IMPORTS_REASON_IMPORT_MAP);
}
static void test_zero_name_table_is_complete_with_no_imports(void) {
    base(); memset(package_bytes + 56, 0, 40);
    put_le(package_bytes + 4, 100, 4); put_le(package_bytes + 28, 56, 4);
    put_le(package_bytes + 32, 56, 4); put_le(package_bytes + 36, 56, 4);
    put_le(package_bytes + 40, 56, 4); put_le(package_bytes + 44, 56, 4);
    put_le(package_bytes + 48, 96, 4); put_le(package_bytes + 96, 0, 4);
    view.captured_bytes = 100; view.logical_package_bytes = 100;
    CHECK(prepare_inputs()); CHECK(import_map.reference_count == 0);
    CHECK(read_package_names() == MB_ZEN_PACKAGE_IMPORTS_OK);
    CHECK(result.complete == 1 && result.table_offset == 96 && result.table_bytes == 4);
    CHECK(result.name_count == 0 && result.package_binding_count == 0 && result.name_bytes_used == 0);
}
static void test_missing_table_is_unsupported(void) {
    base(); put_le(package_bytes + 48, 0, 4); CHECK(prepare_inputs());
    admitted_failure(MB_ZEN_PACKAGE_IMPORTS_UNSUPPORTED);
    CHECK(result.reason == MB_ZEN_PACKAGE_IMPORTS_REASON_OFFSET);
}
static void test_name_tail_cannot_overlap_preceding_hash_range(void) {
    base();
    put_le(package_bytes + 4, 72, 4); put_le(package_bytes + 20, 72, 4);
    put_le(package_bytes + 28, 72, 4); put_le(package_bytes + 32, 72, 4);
    put_le(package_bytes + 36, 72, 4); put_le(package_bytes + 40, 72, 4);
    put_le(package_bytes + 44, 72, 4); put_le(package_bytes + 48, 68, 4);
    put_le(package_bytes + 68, 0, 4); /* Empty table ends at HeaderSize but sits in the hash span. */
    view.captured_bytes = 72; view.logical_package_bytes = 72;
    CHECK(prepare_inputs()); CHECK(import_map.reference_count == 0);
    admitted_failure(MB_ZEN_PACKAGE_IMPORTS_MALFORMED);
    CHECK(result.reason == MB_ZEN_PACKAGE_IMPORTS_REASON_OFFSET);
}
static void test_zero_optional_section_offsets_do_not_reject_name_tail(void) {
    base();
    put_le(package_bytes + 36, 0, 4);
    put_le(package_bytes + 40, 0, 4);
    put_le(package_bytes + 44, 0, 4);
    CHECK(prepare_inputs());
    CHECK(read_package_names() == MB_ZEN_PACKAGE_IMPORTS_OK);
    CHECK(result.complete == 1 && result.table_offset == SYNTHETIC_TABLE_OFFSET);
}
static void test_known_alias_preflight_leaves_everything_untouched(void) {
    base(); CHECK(prepare_inputs()); reset_outputs(); memcpy(saved_bytes, package_bytes, sizeof(saved_bytes));
    mb_zen_package_imports_result_v1 saved_result = result;
    mb_zen_imported_package_name_v1 saved_names[NAME_CAPACITY]; memcpy(saved_names, names, sizeof(names));
    mb_zen_package_import_binding_v1 saved_bindings[BINDING_CAPACITY]; memcpy(saved_bindings, bindings, sizeof(bindings));
    uint8_t *alias = package_bytes + 160;
    CHECK(mb_zen_read_package_imports_v1(&view, &summary, &import_map, references, 8,
        &limits, &result, names, NAME_CAPACITY, alias, 16, bindings, BINDING_CAPACITY) ==
        MB_ZEN_PACKAGE_IMPORTS_BAD_ARGUMENT);
    CHECK(memcmp(package_bytes, saved_bytes, sizeof(package_bytes)) == 0);
    CHECK(memcmp(&result, &saved_result, sizeof(result)) == 0);
    CHECK(memcmp(names, saved_names, sizeof(names)) == 0);
    CHECK(memcmp(bindings, saved_bindings, sizeof(bindings)) == 0);
}

static void bytes_hex(const uint8_t *bytes, uint32_t count, char *out) {
    static const char hex[] = "0123456789abcdef";
    for (uint32_t i = 0; i < count; ++i) { out[i * 2] = hex[bytes[i] >> 4]; out[i * 2 + 1] = hex[bytes[i] & 15]; }
    out[count * 2] = '\0';
}
static int actual_mode(const char *path, const char *logical_text) {
    char *end = NULL; unsigned long long logical = strtoull(logical_text, &end, 10);
    if (!end || end == logical_text || *end || logical_text[0] == '-') return 2;
    static uint8_t header[MB_ZEN_SUMMARY_MAX_HEADER_BYTES];
    FILE *file = fopen(path, "rb"); if (!file) return 3;
    size_t captured = fread(header, 1, sizeof(header), file); int extra = fgetc(file), bad = ferror(file); fclose(file);
    if (bad || extra != EOF || !captured) return 4;
    mb_zen_summary_view_v1 actual_view = {sizeof(actual_view), MB_ZEN_SUMMARY_ABI_VERSION,
        header, captured, (uint64_t)logical};
    mb_zen_summary_profile_v1 profile = {sizeof(profile), MB_ZEN_SUMMARY_ABI_VERSION,
        MB_ZEN_LAYOUT_UE5_3_PLUS, 1, 0};
    mb_zen_summary_limits_v1 sl; mb_zen_summary_limits_v1_default(&sl);
    static mb_zen_summary_result_v1 sr; memset(&sr, 0, sizeof(sr));
    sr.struct_size = sizeof(sr); sr.abi_version = MB_ZEN_SUMMARY_ABI_VERSION;
    static mb_zen_name_v1 summary_names[MB_ZEN_SUMMARY_MAX_NAMES];
    static uint8_t summary_arena[MB_ZEN_SUMMARY_MAX_NAME_BYTES];
    if (mb_zen_read_summary_names_v1(&actual_view, &profile, &sl, &sr,
        summary_names, MB_ZEN_SUMMARY_MAX_NAMES, summary_arena, sizeof(summary_arena)) != MB_ZEN_SUMMARY_OK) return 5;
    mb_zen_import_limits_v1 il; mb_zen_import_limits_v1_default(&il);
    static mb_zen_import_result_v1 ir; memset(&ir, 0, sizeof(ir));
    ir.struct_size = sizeof(ir); ir.abi_version = MB_ZEN_IMPORT_MAP_ABI_VERSION;
    static mb_zen_import_ref_v1 refs[MB_ZEN_IMPORT_MAP_MAX_ENTRIES];
    if (mb_zen_read_import_map_v1(&actual_view, &sr, &il, &ir, refs, MB_ZEN_IMPORT_MAP_MAX_ENTRIES) != MB_ZEN_IMPORT_MAP_OK) return 6;
    mb_zen_package_imports_limits_v1 pil; mb_zen_package_imports_limits_v1_default(&pil);
    static mb_zen_package_imports_result_v1 pr; memset(&pr, 0, sizeof(pr));
    pr.struct_size = sizeof(pr); pr.abi_version = MB_ZEN_PACKAGE_IMPORTS_ABI_VERSION;
    static mb_zen_imported_package_name_v1 imported_names[MB_ZEN_PACKAGE_IMPORTS_MAX_NAMES];
    static uint8_t imported_arena[MB_ZEN_PACKAGE_IMPORTS_MAX_NAME_BYTES];
    static mb_zen_package_import_binding_v1 imported_bindings[MB_ZEN_PACKAGE_IMPORTS_MAX_BINDINGS];
    if (mb_zen_read_package_imports_v1(&actual_view, &sr, &ir, refs, MB_ZEN_IMPORT_MAP_MAX_ENTRIES,
        &pil, &pr, imported_names, MB_ZEN_PACKAGE_IMPORTS_MAX_NAMES, imported_arena,
        sizeof(imported_arena), imported_bindings, MB_ZEN_PACKAGE_IMPORTS_MAX_BINDINGS) != MB_ZEN_PACKAGE_IMPORTS_OK) {
        fprintf(stderr, "package-imports status=%u reason=%u\n", pr.status, pr.reason); return 7;
    }
    printf("{\"header_bytes\":%u,\"table_offset\":%u,\"table_bytes\":%u,\"name_count\":%u,"
        "\"name_bytes_used\":%u,\"declared_string_bytes_raw\":%u,\"name_hash_version_raw\":\"%016" PRIx64 "\",\"names\":[",
        sr.header_size, pr.table_offset, pr.table_bytes, pr.name_count, pr.name_bytes_used,
        pr.declared_string_bytes_raw, pr.name_hash_version_raw);
    for (uint32_t i = 0; i < pr.name_count; ++i) {
        char hex[MB_ZEN_PACKAGE_IMPORTS_MAX_NAME_BYTES * 2u + 1u];
        bytes_hex(imported_arena + imported_names[i].arena_offset, imported_names[i].byte_length, hex);
        printf("%s{\"hash\":\"%016" PRIx64 "\",\"encoding\":%u,\"bytes\":%u,\"units\":%u,\"number\":%" PRId32 ",\"raw_hex\":\"%s\"}",
            i ? "," : "", imported_names[i].hash_raw, imported_names[i].encoding,
            imported_names[i].byte_length, imported_names[i].code_units, imported_names[i].number, hex);
    }
    printf("],\"binding_count\":%u,\"bindings\":[", pr.package_binding_count);
    for (uint32_t i = 0; i < pr.package_binding_count; ++i) {
        const mb_zen_package_import_binding_v1 *b = &imported_bindings[i];
        printf("%s{\"import_index\":%u,\"name_index\":%u,\"hash_index\":%u,\"object_index\":\"%016" PRIx64 "\",\"public_hash\":\"%016" PRIx64 "\"}",
            i ? "," : "", b->import_reference_index, b->imported_package_index,
            b->public_export_hash_index, b->object_index_raw, b->public_export_hash);
    }
    puts("]}"); return 0;
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "--actual") == 0) return actual_mode(argv[2], argv[3]);
    if (argc != 1) return 64;
    test_complete_table_and_bindings();
    test_declared_string_bytes_remain_raw();
    test_invalid_package_name_index_clears_all();
    test_invalid_public_hash_index_clears_all();
    test_short_number_array_inside_header_clears_all();
    test_capture_truncation_clears_all();
    test_unexpected_hash_version_is_unsupported();
    test_name_cap_clears_all();
    test_changed_reference_record_is_rejected();
    test_zero_name_table_is_complete_with_no_imports();
    test_missing_table_is_unsupported();
    test_name_tail_cannot_overlap_preceding_hash_range();
    test_zero_optional_section_offsets_do_not_reject_name_tail();
    test_known_alias_preflight_leaves_everything_untouched();
    printf("zen-package-imports checks=%u failures=%u\n", checks, failures);
    return failures ? 1 : 0;
}
