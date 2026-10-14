#include "pak_writer.h"
#include <stdio.h>
#include <string.h>

/* These fixtures catch missing serialization, lax names/aliases, premature
 * publication and cap/work off-by-one errors. Payload expectations are literal
 * input bytes, independently extracted/hashed by the Python/repak checks. */
static unsigned checks, failures, groups;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; \
    printf("FAIL line=%u %s\n", (unsigned)__LINE__, #x); } } while (0)

static mb_pak_writer_workspace_v1 workspace;
static mb_pak_writer_result_v1 result;
static mb_pak_writer_limits_v1 limits;
static mb_pak_writer_profile_v1 profile;
static mb_pak_writer_file_v1 files[MB_PAK_WRITER_MAX_FILES + 1];
static uint8_t image[MB_PAK_WRITER_MAX_ARCHIVE_BYTES + 16];
static uint8_t saved[16384];
static uint8_t large[MB_PAK_WRITER_MAX_PAYLOAD_BYTES + 1];
static const uint8_t text[] = "mode=original\r\n";
static const uint8_t icon[] = {0, 1, 2, 255, 128, 0, 42, 17, 99};
static const uint8_t readme[] = "bounded synthetic bytes\n";

static int filled(const void *p, size_t n, uint8_t value) {
    const uint8_t *b = p;
    for (size_t i = 0; i < n; ++i) if (b[i] != value) return 0;
    return 1;
}

static mb_pak_writer_file_v1 file(const char *path, const uint8_t *payload, size_t bytes) {
    mb_pak_writer_file_v1 f = {sizeof(f), 1, (const uint8_t *)path,
        (uint32_t)strlen(path), 0, payload, bytes};
    return f;
}

static void setup(void) {
    memset(&workspace, 0x5a, sizeof(workspace));
    memset(&result, 0x5a, sizeof(result));
    memset(image, 0xa5, sizeof(image));
    mb_pak_writer_limits_v1_default(&limits);
    profile = (mb_pak_writer_profile_v1){sizeof(profile), 1,
        (const uint8_t *)"../../../", 9, 0, UINT64_C(0xf375bbbd)};
    files[0] = file("Example/Config/base.ini", text, sizeof(text)-1);
    files[1] = file("readme.txt", readme, sizeof(readme)-1);
    files[2] = file("Engine/Content/UI/Icon.bin", icon, sizeof(icon));
    files[3] = file("empty.bin", NULL, 0);
}

static mb_pak_writer_status run(uint32_t count, size_t capacity) {
    return mb_pak_write_v11_none(&profile, files, count, &limits, &workspace,
                                 image, capacity, &result);
}

static void rejected(mb_pak_writer_status expected, uint32_t count) {
    CHECK(run(count, MB_PAK_WRITER_MAX_ARCHIVE_BYTES) == expected);
    CHECK(result.status == expected && !result.archive_complete && !result.archive_bytes);
    CHECK(!result.file_count && !result.directory_count);
    CHECK(filled(result.primary_sha1, 20, 0) && filled(result.path_index_sha1, 20, 0) &&
          filled(result.directory_sha1, 20, 0));
    CHECK(filled(image, sizeof(image), 0xa5));
    CHECK(filled(&workspace, sizeof(workspace), 0));
}

static int save_archive(const char *directory, const char *name) {
    char path[4096];
    const int n = snprintf(path, sizeof(path), "%s/%s", directory, name);
    if (n < 0 || (size_t)n >= sizeof(path)) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    const int written = fwrite(image, 1, (size_t)result.archive_bytes, f) == result.archive_bytes;
    return fclose(f) == 0 && written;
}

static void deterministic(const char *directory) {
    ++groups; setup();
    CHECK(run(4, MB_PAK_WRITER_MAX_ARCHIVE_BYTES) == MB_PAK_WRITE_OK);
    CHECK(result.archive_complete == 1 && result.file_count == 4 && result.directory_count == 6);
    CHECK(result.archive_bytes == result.required_bytes && result.archive_bytes < sizeof(saved));
    CHECK(result.work_used && result.work_used <= limits.max_work);
    CHECK(filled(&workspace, sizeof(workspace), 0));
    CHECK(filled(image + result.archive_bytes, sizeof(image) - (size_t)result.archive_bytes, 0xa5));
    if (result.archive_complete && result.archive_bytes < sizeof(saved)) {
        const size_t bytes = (size_t)result.archive_bytes;
        memcpy(saved, image, bytes);
        CHECK(save_archive(directory, "prepared.pak"));
        const mb_pak_writer_file_v1 tmp = files[0]; files[0] = files[2]; files[2] = tmp;
        CHECK(run(4, MB_PAK_WRITER_MAX_ARCHIVE_BYTES) == MB_PAK_WRITE_OK);
        CHECK(result.archive_bytes == bytes && memcmp(saved, image, bytes) == 0);
        profile.path_hash_seed = UINT64_C(0xffffffffffffffff);
        profile.mount = (const uint8_t *)"../Shared/"; profile.mount_bytes = 10;
        CHECK(run(4, MB_PAK_WRITER_MAX_ARCHIVE_BYTES) == MB_PAK_WRITE_OK);
        CHECK(result.archive_bytes != 0 && memcmp(saved, image, bytes) != 0);
        CHECK(save_archive(directory, "custom.pak"));
    }
    setup();
    CHECK(run(0, MB_PAK_WRITER_MAX_ARCHIVE_BYTES) == MB_PAK_WRITE_OK);
    CHECK(result.archive_complete && !result.file_count && !result.directory_count);
    if (result.archive_complete) CHECK(save_archive(directory, "empty.pak"));
}

static void capacities(void) {
    ++groups; setup();
    CHECK(mb_pak_write_v11_none(&profile, files, 4, &limits, &workspace, NULL, 0, &result)
          == MB_PAK_WRITE_BUFFER_TOO_SMALL);
    CHECK(!result.archive_complete && !result.archive_bytes && result.required_bytes > 221);
    const size_t required = (size_t)result.required_bytes;
    const uint64_t query_work = result.work_used;
    CHECK(filled(&workspace, sizeof(workspace), 0));
    if (required < sizeof(image) && required) {
        CHECK(run(4, required - 1) == MB_PAK_WRITE_BUFFER_TOO_SMALL);
        CHECK(result.required_bytes == required && result.work_used == query_work);
        CHECK(filled(image, sizeof(image), 0xa5));
        CHECK(run(4, required) == MB_PAK_WRITE_OK && result.archive_bytes == required);
        CHECK(filled(image + required, sizeof(image) - required, 0xa5));
        const uint64_t exact_work = result.work_used;
        setup(); limits.max_work = exact_work;
        CHECK(run(4, required) == MB_PAK_WRITE_OK);
        setup(); limits.max_work = exact_work - 1;
        rejected(MB_PAK_WRITE_RESOURCE_LIMIT, 4);
        setup(); limits.max_archive_bytes = required;
        CHECK(run(4, required) == MB_PAK_WRITE_OK);
        setup(); limits.max_archive_bytes = required - 1;
        rejected(MB_PAK_WRITE_RESOURCE_LIMIT, 4);
    }
}

static void paths(void) {
    ++groups;
    const char *bad[] = {"", "/root", "../a", "a/../b", "a/./b", "a//b", "a/", "a\\b",
        "C:a", "a?b", "a*b", "a|b", "a<b", "a\"b", " a", "a ", "a/ b", "a/b "};
    for (size_t i = 0; i < sizeof(bad)/sizeof(*bad); ++i) {
        setup(); files[3] = file(bad[i], icon, sizeof(icon));
        rejected(MB_PAK_WRITE_INVALID_PATH, 4);
    }
    setup(); files[3] = file("a\xc3\xa9.bin", icon, sizeof(icon));
    rejected(MB_PAK_WRITE_UNSUPPORTED_NAME, 4);
    setup(); files[3] = file("a\n.bin", icon, sizeof(icon));
    rejected(MB_PAK_WRITE_INVALID_PATH, 4);
    static const uint8_t nul_path[] = {'a', 0, 'b'};
    setup(); files[3].path = nul_path; files[3].path_bytes = 3;
    rejected(MB_PAK_WRITE_INVALID_PATH, 4);
    setup(); files[3] = files[0]; rejected(MB_PAK_WRITE_PATH_CONFLICT, 4);
    setup(); files[3] = file("EXAMPLE/config/BASE.INI", icon, sizeof(icon));
    rejected(MB_PAK_WRITE_PATH_CONFLICT, 4);
    /* A lexical neighbor between ancestor and descendant defeats an adjacent
     * prefix-only implementation. */
    setup(); files[0] = file("a", text, sizeof(text)-1);
    files[1] = file("a!.txt", readme, sizeof(readme)-1);
    files[2] = file("a/b", icon, sizeof(icon));
    rejected(MB_PAK_WRITE_PATH_CONFLICT, 3);
    setup(); files[0] = file("A/one", text, sizeof(text)-1);
    files[1] = file("a/two", icon, sizeof(icon));
    rejected(MB_PAK_WRITE_PATH_CONFLICT, 2);
    setup(); profile.mount = (const uint8_t *)"relative"; profile.mount_bytes = 8;
    rejected(MB_PAK_WRITE_INVALID_PATH, 4);
    setup(); profile.mount = (const uint8_t *)"a/../"; profile.mount_bytes = 5;
    rejected(MB_PAK_WRITE_INVALID_PATH, 4);
    setup(); profile.mount = (const uint8_t *)"/"; profile.mount_bytes = 1;
    CHECK(run(4, MB_PAK_WRITER_MAX_ARCHIVE_BYTES) == MB_PAK_WRITE_OK);
}

static void limits_cases(void) {
    ++groups;
    setup(); limits.max_files = 3; rejected(MB_PAK_WRITE_RESOURCE_LIMIT, 4);
    setup(); limits.max_directories = 5; rejected(MB_PAK_WRITE_RESOURCE_LIMIT, 4);
    setup(); limits.max_name_bytes = 25; rejected(MB_PAK_WRITE_RESOURCE_LIMIT, 4);
    setup(); limits.max_path_bytes = 8; rejected(MB_PAK_WRITE_RESOURCE_LIMIT, 4);
    setup(); limits.max_payload_bytes = 8; rejected(MB_PAK_WRITE_RESOURCE_LIMIT, 4);
    setup(); limits.max_work = 0; rejected(MB_PAK_WRITE_RESOURCE_LIMIT, 4);
    setup(); files[0] = file("large.bin", large, MB_PAK_WRITER_MAX_PAYLOAD_BYTES);
    CHECK(run(1, MB_PAK_WRITER_MAX_ARCHIVE_BYTES) == MB_PAK_WRITE_OK);
    CHECK(result.archive_bytes > MB_PAK_WRITER_MAX_PAYLOAD_BYTES && result.archive_complete);
    setup(); files[0] = file("large.bin", large, sizeof(large));
    rejected(MB_PAK_WRITE_RESOURCE_LIMIT, 1);
    char component[257]; memset(component, 'a', 256); component[256] = 0;
    setup(); files[0] = file(component, icon, sizeof(icon));
    rejected(MB_PAK_WRITE_INVALID_PATH, 1);
    /* Three maximum payloads fit; four plus metadata exceed complete-PAK cap. */
    setup();
    for (unsigned i = 0; i < 4; ++i) { files[i].payload = large;
        files[i].payload_bytes = MB_PAK_WRITER_MAX_PAYLOAD_BYTES; }
    rejected(MB_PAK_WRITE_RESOURCE_LIMIT, 4);
}

static void preflight(void) {
    ++groups; setup();
    CHECK(mb_pak_write_v11_none(&profile, files, 4, &limits, &workspace,
          (uint8_t *)&workspace, 100, &result) == MB_PAK_WRITE_BAD_ARGUMENT);
    CHECK(filled(&workspace, sizeof(workspace), 0x5a) && filled(&result, sizeof(result), 0x5a));
    setup(); files[3].payload = image + 100; files[3].payload_bytes = 2;
    CHECK(run(4, 4096) == MB_PAK_WRITE_BAD_ARGUMENT);
    CHECK(filled(image, sizeof(image), 0xa5) && filled(&result, sizeof(result), 0x5a));
    setup(); files[3].path = image; files[3].path_bytes = 1;
    CHECK(run(4, 4096) == MB_PAK_WRITE_BAD_ARGUMENT);
    CHECK(filled(image, sizeof(image), 0xa5) && filled(&result, sizeof(result), 0x5a));
    setup(); profile.abi_version = 2;
    CHECK(run(4, 4096) == MB_PAK_WRITE_BAD_ARGUMENT);
    CHECK(filled(&workspace, sizeof(workspace), 0x5a) && filled(&result, sizeof(result), 0x5a));
    setup(); limits.max_work = MB_PAK_WRITER_MAX_WORK + 1;
    CHECK(run(4, 4096) == MB_PAK_WRITE_BAD_ARGUMENT);
    CHECK(filled(&result, sizeof(result), 0x5a));
    setup(); files[3].reserved = 1;
    CHECK(run(4, 4096) == MB_PAK_WRITE_BAD_ARGUMENT);
    CHECK(filled(&result, sizeof(result), 0x5a));
    setup();
    CHECK(mb_pak_write_v11_none(&profile, files, 4, &limits,
          (mb_pak_writer_workspace_v1 *)((uint8_t *)&workspace + 1), image, 4096, &result)
          == MB_PAK_WRITE_BAD_ARGUMENT);
    CHECK(filled(&result, sizeof(result), 0x5a));
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "one existing output directory required\n"); return 2; }
    deterministic(argv[1]); capacities(); paths(); limits_cases(); preflight();
    printf("pak-writer groups=%u checks=%u failures=%u synthetic_only=true runtime_supported=false\n",
           groups, checks, failures);
    return failures ? 1 : 0;
}
