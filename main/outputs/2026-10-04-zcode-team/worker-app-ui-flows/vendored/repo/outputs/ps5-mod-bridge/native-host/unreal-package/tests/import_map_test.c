#include "import_map.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; fprintf(stderr, "FAIL line=%u %s\n", (unsigned)__LINE__, #x); } } while (0)
static uint8_t package_bytes[256], saved_bytes[256];
static mb_zen_summary_view_v1 view;
static mb_zen_summary_result_v1 summary;
static mb_zen_import_limits_v1 limits;
static mb_zen_import_result_v1 result;
static mb_zen_import_ref_v1 refs[8];

static void put_le(uint8_t *p, uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) { p[i] = (uint8_t)value; value >>= 8; }
}
static void base(void) {
    const uint64_t package_import = (UINT64_C(2) << 62) | (UINT64_C(2) << 32) | UINT64_C(1);
    memset(package_bytes, 0, sizeof(package_bytes));
    put_le(package_bytes + 4, 192, 4);
    put_le(package_bytes + 24, 112, 4);
    put_le(package_bytes + 28, 128, 4);
    put_le(package_bytes + 32, 160, 4);
    put_le(package_bytes + 112, UINT64_C(0x1111222233334444), 8);
    put_le(package_bytes + 120, UINT64_C(0xaaaabbbbccccdddd), 8);
    put_le(package_bytes + 128, UINT64_C(3), 8);
    put_le(package_bytes + 136, (UINT64_C(1) << 62) | UINT64_C(0x0123456789), 8);
    put_le(package_bytes + 144, package_import, 8);
    put_le(package_bytes + 152, UINT64_MAX, 8);
    view = (mb_zen_summary_view_v1){sizeof(view), MB_ZEN_SUMMARY_ABI_VERSION,
        package_bytes, 192, 192};
    memset(&summary, 0, sizeof(summary));
    summary.struct_size = sizeof(summary); summary.abi_version = MB_ZEN_SUMMARY_ABI_VERSION;
    summary.status = MB_ZEN_SUMMARY_OK; summary.summary_complete = 1; summary.name_map_complete = 1;
    summary.header_size = 192; summary.imported_public_export_hashes_offset = 112;
    summary.import_map_offset = 128; summary.export_map_offset = 160;
    memset(&limits, 0, sizeof(limits)); limits.struct_size = sizeof(limits); limits.abi_version = 1;
    limits.max_imports = MB_ZEN_IMPORT_MAP_MAX_ENTRIES;
    memset(&result, 0xa5, sizeof(result)); result.struct_size = sizeof(result); result.abi_version = 1;
    memset(refs, 0xa5, sizeof(refs));
}
static int all_zero(const void *ptr, size_t bytes) {
    const uint8_t *p = ptr; while (bytes--) if (*p++) return 0; return 1;
}
static void rejected(mb_zen_import_map_status expected) {
    const mb_zen_import_map_status actual = mb_zen_read_import_map_v1(
        &view, &summary, &limits, &result, refs, 8);
    CHECK(actual == expected && result.status == (uint32_t)expected);
    CHECK(!result.complete && !result.reference_count && !result.import_map_bytes);
    CHECK(all_zero(refs, sizeof(refs)));
}
static void positives(void) {
    base();
    CHECK(mb_zen_read_import_map_v1(&view, &summary, &limits, &result, refs, 8) == MB_ZEN_IMPORT_MAP_OK);
    CHECK(result.complete && result.reference_count == 4 && result.import_map_offset == 128 && result.import_map_bytes == 32);
    CHECK(result.public_hash_offset == 112 && result.public_hash_bytes == 16 && result.public_hash_count == 2);
    CHECK(result.kind_counts[MB_ZEN_IMPORT_EXPORT] == 1 && result.kind_counts[MB_ZEN_IMPORT_SCRIPT] == 1);
    CHECK(result.kind_counts[MB_ZEN_IMPORT_PACKAGE] == 1 && result.kind_counts[MB_ZEN_IMPORT_NULL] == 1);
    CHECK(refs[0].raw == 3 && refs[0].kind == MB_ZEN_IMPORT_EXPORT && refs[0].value == 3 && refs[0].export_index == 3);
    CHECK(refs[1].kind == MB_ZEN_IMPORT_SCRIPT && refs[1].value == UINT64_C(0x0123456789));
    CHECK(refs[2].kind == MB_ZEN_IMPORT_PACKAGE && refs[2].value == ((UINT64_C(2) << 32) | 1));
    CHECK(refs[2].imported_package_index == 2 && refs[2].public_export_hash_index == 1);
    CHECK(refs[3].raw == UINT64_MAX && refs[3].kind == MB_ZEN_IMPORT_NULL && refs[3].is_null == 1);

    /* CUE4Parse classifies the top-bit type as Null but recognizes only the
     * all-ones sentinel as IsNull; preserve both facts independently. */
    base(); put_le(package_bytes + 152, UINT64_C(3) << 62, 8);
    CHECK(mb_zen_read_import_map_v1(&view, &summary, &limits, &result, refs, 8) == MB_ZEN_IMPORT_MAP_OK);
    CHECK(refs[3].kind == MB_ZEN_IMPORT_NULL && refs[3].is_null == 0 && refs[3].raw != UINT64_MAX);
}
static void negatives(void) {
    base(); view.captured_bytes = 159; rejected(MB_ZEN_IMPORT_MAP_TRUNCATED);
    base(); summary.export_map_offset = 161; rejected(MB_ZEN_IMPORT_MAP_MALFORMED);
    base(); summary.import_map_offset = 168; rejected(MB_ZEN_IMPORT_MAP_MALFORMED);
    base(); put_le(package_bytes + 144, (UINT64_C(2) << 62) | (UINT64_C(2) << 32) | UINT64_C(2), 8);
    rejected(MB_ZEN_IMPORT_MAP_MALFORMED);
    base(); limits.max_imports = 3; rejected(MB_ZEN_IMPORT_MAP_RESOURCE_LIMIT);
    base(); summary.summary_complete = 0; rejected(MB_ZEN_IMPORT_MAP_BAD_ARGUMENT);
    base(); memcpy(saved_bytes, package_bytes, sizeof(saved_bytes));
    CHECK(mb_zen_read_import_map_v1(&view, &summary, &limits, &result,
        (mb_zen_import_ref_v1 *)(void *)(package_bytes + 128), 4) == MB_ZEN_IMPORT_MAP_BAD_ARGUMENT);
    CHECK(memcmp(saved_bytes, package_bytes, sizeof(saved_bytes)) == 0);
}

static int actual_mode(const char *path, const char *logical_bytes_text) {
    char *end = NULL;
    const uint64_t logical_bytes = strtoull(logical_bytes_text, &end, 10);
    if (!end || end == logical_bytes_text || *end || logical_bytes_text[0] == '-') return 6;

    uint8_t header[MB_ZEN_SUMMARY_MAX_HEADER_BYTES];
    FILE *file = fopen(path, "rb");
    if (!file) return 2;
    const size_t captured = fread(header, 1, sizeof(header), file);
    const int extra = fgetc(file);
    const int read_error = ferror(file);
    fclose(file);
    if (read_error || extra != EOF || !captured) return 3;

    mb_zen_summary_view_v1 actual_view = {sizeof(actual_view), MB_ZEN_SUMMARY_ABI_VERSION,
        header, captured, logical_bytes};
    mb_zen_summary_profile_v1 profile = {sizeof(profile), MB_ZEN_SUMMARY_ABI_VERSION,
        MB_ZEN_LAYOUT_UE5_3_PLUS, 1, 0};
    mb_zen_summary_limits_v1 summary_limits;
    mb_zen_summary_limits_v1_default(&summary_limits);
    mb_zen_summary_result_v1 summary_result = {0};
    summary_result.struct_size = sizeof(summary_result);
    summary_result.abi_version = MB_ZEN_SUMMARY_ABI_VERSION;
    mb_zen_name_v1 names[MB_ZEN_SUMMARY_MAX_NAMES];
    uint8_t name_arena[MB_ZEN_SUMMARY_MAX_NAME_BYTES];
    const mb_zen_summary_status summary_status = mb_zen_read_summary_names_v1(
        &actual_view, &profile, &summary_limits, &summary_result,
        names, MB_ZEN_SUMMARY_MAX_NAMES, name_arena, sizeof(name_arena));
    if (summary_status != MB_ZEN_SUMMARY_OK) {
        fprintf(stderr, "summary status=%u reason=%u\n", (unsigned)summary_status,
            summary_result.reason);
        return 4;
    }

    mb_zen_import_limits_v1 import_limits;
    mb_zen_import_limits_v1_default(&import_limits);
    mb_zen_import_result_v1 import_result = {0};
    import_result.struct_size = sizeof(import_result);
    import_result.abi_version = MB_ZEN_IMPORT_MAP_ABI_VERSION;
    mb_zen_import_ref_v1 references[MB_ZEN_IMPORT_MAP_MAX_ENTRIES];
    const mb_zen_import_map_status import_status = mb_zen_read_import_map_v1(
        &actual_view, &summary_result, &import_limits, &import_result,
        references, MB_ZEN_IMPORT_MAP_MAX_ENTRIES);
    if (import_status != MB_ZEN_IMPORT_MAP_OK) {
        fprintf(stderr, "import map status=%u reason=%u\n", (unsigned)import_status,
            import_result.reason);
        return 5;
    }

    printf("{\"header_bytes\":%u,\"import_map_offset\":%u,\"import_map_bytes\":%u,"
        "\"reference_count\":%u,\"public_hash_count\":%u,\"kind_counts\":[%u,%u,%u,%u],\"raw_refs\":[",
        summary_result.header_size, import_result.import_map_offset,
        import_result.import_map_bytes, import_result.reference_count,
        import_result.public_hash_count, import_result.kind_counts[0],
        import_result.kind_counts[1], import_result.kind_counts[2], import_result.kind_counts[3]);
    for (uint32_t i = 0; i < import_result.reference_count; ++i) {
        if (i) putchar(',');
        printf("\"%016" PRIx64 "\"", references[i].raw);
    }
    puts("]}");
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "--actual") == 0)
        return actual_mode(argv[2], argv[3]);
    if (argc != 1) return 64;
    positives(); negatives();
    printf("zen-import-map checks=%u failures=%u\n", checks, failures);
    return failures ? 1 : 0;
}
