/* Pure shared-cap regression: names and layout are synthetic format data. */
#include "package_imports.h"
#include <stdio.h>
#include <string.h>

enum { CAP_BYTES = 262144, NAME_BYTES = 90000, NAME_COUNT = 3,
       HASH_OFFSET = 90112, IMPORT_OFFSET = 90128, EXPORT_OFFSET = 90152,
       TAIL_OFFSET = 90224, HEADER_BYTES = 180282 };
static uint8_t bytes[CAP_BYTES + 1], saved[CAP_BYTES + 1];
static uint8_t local_arena[NAME_BYTES], imported_arena[NAME_BYTES];
static mb_zen_name_v1 local_names[NAME_COUNT];
static mb_zen_imported_package_name_v1 imported_names[NAME_COUNT];
static mb_zen_import_ref_v1 refs[3];
static mb_zen_package_import_binding_v1 bindings[3];
static mb_zen_summary_view_v1 view;
static mb_zen_summary_profile_v1 profile;
static mb_zen_summary_limits_v1 sl;
static mb_zen_import_limits_v1 il;
static mb_zen_package_imports_limits_v1 pl;
static mb_zen_summary_result_v1 sr;
static mb_zen_import_result_v1 ir;
static mb_zen_package_imports_result_v1 pr;
static unsigned checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; fprintf(stderr, "FAIL line=%u %s\n", (unsigned)__LINE__, #x); } } while (0)

static void put(uint8_t *p, uint64_t x, unsigned n) {
    for (unsigned i = 0; i < n; ++i) { p[i] = (uint8_t)x; x >>= 8; }
}
static int zero(const void *ptr, size_t n) {
    const uint8_t *p = ptr; while (n--) if (*p++) return 0; return 1;
}
static int poisoned(const void *ptr, size_t n) {
    const uint8_t *p = ptr; while (n--) if (*p++ != 0xa5) return 0; return 1;
}
static void reset(void) {
    memset(&sr, 0xa5, sizeof(sr)); sr.struct_size = sizeof(sr); sr.abi_version = 1;
    memset(&ir, 0xa5, sizeof(ir)); ir.struct_size = sizeof(ir); ir.abi_version = 1;
    memset(&pr, 0xa5, sizeof(pr)); pr.struct_size = sizeof(pr); pr.abi_version = 1;
    memset(local_names, 0xa5, sizeof(local_names)); memset(local_arena, 0xa5, sizeof(local_arena));
    memset(imported_names, 0xa5, sizeof(imported_names)); memset(imported_arena, 0xa5, sizeof(imported_arena));
    memset(refs, 0xa5, sizeof(refs)); memset(bindings, 0xa5, sizeof(bindings));
}
static size_t batch(size_t start, int numbers) {
    size_t p = start;
    put(bytes + p, NAME_COUNT, 4); p += 4;
    put(bytes + p, NAME_BYTES, 4); p += 4;
    put(bytes + p, MB_ZEN_PACKAGE_NAME_HASH_VERSION, 8); p += 8;
    for (unsigned i = 0; i < NAME_COUNT; ++i) { put(bytes + p, 100 + i, 8); p += 8; }
    for (unsigned i = 0; i < NAME_COUNT; ++i) { bytes[p++] = 0x75; bytes[p++] = 0x30; }
    for (unsigned i = 0; i < NAME_COUNT; ++i) { memset(bytes + p, 'A' + (int)i, 30000); p += 30000; }
    if (numbers) for (unsigned i = 0; i < NAME_COUNT; ++i) { put(bytes + p, i, 4); p += 4; }
    return p;
}
static void base(void) {
    memset(bytes, 0, sizeof(bytes));
    put(bytes + 4, HEADER_BYTES, 4);
    const unsigned offsets[7] = {HASH_OFFSET, IMPORT_OFFSET, EXPORT_OFFSET,
        TAIL_OFFSET, TAIL_OFFSET, TAIL_OFFSET, TAIL_OFFSET};
    for (unsigned i = 0; i < 7; ++i) put(bytes + 24 + 4*i, offsets[i], 4);
    put(bytes + 52, TAIL_OFFSET, 4); put(bytes + 56, TAIL_OFFSET, 4);
    CHECK(batch(60, 0) == 90106); CHECK(batch(TAIL_OFFSET, 1) == HEADER_BYTES);
    put(bytes + HASH_OFFSET, 101, 8); put(bytes + HASH_OFFSET + 8, 103, 8);
    put(bytes + IMPORT_OFFSET, UINT64_C(2) << 62, 8);
    put(bytes + IMPORT_OFFSET + 8, (UINT64_C(2) << 62) | (UINT64_C(2) << 32) | 1, 8);
    put(bytes + IMPORT_OFFSET + 16, UINT64_C(1) << 62, 8);
    view = (mb_zen_summary_view_v1){sizeof(view), 1, bytes, HEADER_BYTES, HEADER_BYTES};
    profile = (mb_zen_summary_profile_v1){sizeof(profile), 1, MB_ZEN_LAYOUT_UE5_3_PLUS, 1, 0};
    mb_zen_summary_limits_v1_default(&sl); mb_zen_import_limits_v1_default(&il);
    mb_zen_package_imports_limits_v1_default(&pl); reset();
}
static mb_zen_summary_status summary(void) {
    return mb_zen_read_summary_names_v1(&view, &profile, &sl, &sr,
        local_names, NAME_COUNT, local_arena, sizeof(local_arena));
}
static mb_zen_import_map_status imports(void) {
    return mb_zen_read_import_map_v1(&view, &sr, &il, &ir, refs, 3);
}
static mb_zen_package_imports_status names(uint32_t capacity) {
    return mb_zen_read_package_imports_v1(&view, &sr, &ir, refs, 3, &pl, &pr,
        imported_names, NAME_COUNT, imported_arena, capacity, bindings, 3);
}
static void summary_cleared(void) {
    CHECK(!sr.summary_complete && !sr.name_map_complete && !sr.name_count && !sr.name_bytes_used);
    CHECK(zero(local_names, sizeof(local_names)) && zero(local_arena, sizeof(local_arena)));
}
static int prepared(void) {
    mb_zen_summary_status s = summary(); CHECK(s == MB_ZEN_SUMMARY_OK);
    if (s) return 0;
    mb_zen_import_map_status i = imports(); CHECK(i == MB_ZEN_IMPORT_MAP_OK);
    return i == MB_ZEN_IMPORT_MAP_OK;
}
static void successes(void) {
    base(); CHECK(sl.max_header_bytes == CAP_BYTES && sl.max_name_bytes == CAP_BYTES);
    CHECK(il.max_imports == 8192 && pl.max_bindings == 8192 && sl.max_names == 4096 && pl.max_names == 4096);
    if (!prepared()) return;
    CHECK(sr.header_size == HEADER_BYTES && sr.name_bytes_used == NAME_BYTES && sr.name_count == NAME_COUNT);
    CHECK(local_arena[0] == 'A' && local_arena[NAME_BYTES - 1] == 'C');
    CHECK(ir.complete && ir.reference_count == 3 && ir.import_map_offset == IMPORT_OFFSET);
    CHECK(names(NAME_BYTES) == MB_ZEN_PACKAGE_IMPORTS_OK);
    CHECK(pr.complete && pr.name_bytes_used == NAME_BYTES && pr.package_binding_count == 2);
    CHECK(bindings[1].imported_package_index == 2 && bindings[1].public_export_hash == 103);
    CHECK(imported_arena[0] == 'A' && imported_arena[NAME_BYTES - 1] == 'C');
    base(); sl.max_header_bytes = HEADER_BYTES; sl.max_name_bytes = NAME_BYTES; sl.max_names = NAME_COUNT;
    CHECK(summary() == MB_ZEN_SUMMARY_OK);
    base(); view.captured_bytes = CAP_BYTES; view.logical_package_bytes = CAP_BYTES;
    put(bytes + 4, CAP_BYTES, 4); CHECK(summary() == MB_ZEN_SUMMARY_OK);
}
static void failures_and_alias(void) {
    base(); sl.max_header_bytes = HEADER_BYTES - 1;
    CHECK(summary() == MB_ZEN_SUMMARY_RESOURCE_LIMIT); summary_cleared();
    base(); sl.max_name_bytes = NAME_BYTES - 1;
    CHECK(summary() == MB_ZEN_SUMMARY_RESOURCE_LIMIT); summary_cleared();
    base(); view.captured_bytes = CAP_BYTES + 1; view.logical_package_bytes = CAP_BYTES + 1;
    CHECK(summary() == MB_ZEN_SUMMARY_RESOURCE_LIMIT); summary_cleared();
    base(); put(bytes + 4, CAP_BYTES + 1, 4); view.logical_package_bytes = CAP_BYTES + 1;
    CHECK(summary() == MB_ZEN_SUMMARY_RESOURCE_LIMIT); summary_cleared();
    base(); sl.max_header_bytes = CAP_BYTES + 1;
    mb_zen_summary_result_v1 before = sr;
    CHECK(summary() == MB_ZEN_SUMMARY_BAD_ARGUMENT);
    CHECK(memcmp(&before, &sr, sizeof(sr)) == 0 && poisoned(local_names, sizeof(local_names)) && poisoned(local_arena, sizeof(local_arena)));
    base(); sl.max_name_bytes = CAP_BYTES + 1; before = sr;
    CHECK(summary() == MB_ZEN_SUMMARY_BAD_ARGUMENT);
    CHECK(memcmp(&before, &sr, sizeof(sr)) == 0 && poisoned(local_names, sizeof(local_names)) && poisoned(local_arena, sizeof(local_arena)));
    base(); if (!prepared()) return;
    view.captured_bytes = CAP_BYTES + 1;
    CHECK(imports() == MB_ZEN_IMPORT_MAP_RESOURCE_LIMIT);
    CHECK(!ir.complete && !ir.reference_count && zero(refs, sizeof(refs)));
    base(); if (!prepared()) return;
    pl.max_name_bytes = NAME_BYTES - 1;
    CHECK(names(NAME_BYTES) == MB_ZEN_PACKAGE_IMPORTS_RESOURCE_LIMIT);
    CHECK(!pr.complete && !pr.name_count && !pr.package_binding_count);
    CHECK(zero(imported_names, sizeof(imported_names)) && zero(imported_arena, sizeof(imported_arena)) && zero(bindings, sizeof(bindings)));
    base(); if (!prepared()) return;
    CHECK(names(NAME_BYTES - 1) == MB_ZEN_PACKAGE_IMPORTS_RESOURCE_LIMIT);
    CHECK(!pr.complete && zero(imported_names, sizeof(imported_names)) && zero(imported_arena, NAME_BYTES - 1) && zero(bindings, sizeof(bindings)));
    base(); if (!prepared()) return;
    view.captured_bytes = CAP_BYTES + 1;
    CHECK(names(NAME_BYTES) == MB_ZEN_PACKAGE_IMPORTS_RESOURCE_LIMIT);
    CHECK(!pr.complete && zero(imported_arena, sizeof(imported_arena)) && zero(bindings, sizeof(bindings)));
    base(); memcpy(saved, bytes, sizeof(bytes));
    before = sr;
    CHECK(mb_zen_read_summary_names_v1(&view, &profile, &sl, &sr, local_names,
        NAME_COUNT, bytes + 120000, NAME_BYTES) == MB_ZEN_SUMMARY_BAD_ARGUMENT);
    CHECK(memcmp(saved, bytes, sizeof(bytes)) == 0 && memcmp(&before, &sr, sizeof(sr)) == 0);
}
int main(void) {
    successes(); failures_and_alias();
    printf("zen-header-caps checks=%u failures=%u\n", checks, failures);
    return failures ? 1 : 0;
}
