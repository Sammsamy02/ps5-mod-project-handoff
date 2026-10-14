#include "module_metadata.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct fixture {
    size_t count;
    uint32_t handles[MB_MODULE_METADATA_LIMIT];
    mb_prospero_module_info info[2];
    int list_error, info_error, info_error_at;
    unsigned list_calls, info_calls;
    int omit_count;
} fixture;

static mb_module_metadata_snapshot snapshot;
static fixture native_fixture;
static unsigned cases;

static void expect_zero(const void *memory, size_t size) {
    const unsigned char *p = memory;
    for (size_t i = 0; i < size; ++i) assert(p[i] == 0);
}

static fixture make_fixture(void) {
    fixture f = {0};
    f.count = 2;
    f.info_error_at = -1;
    for (size_t i = 0; i < MB_MODULE_METADATA_LIMIT; ++i) f.handles[i] = (uint32_t)i;
    for (unsigned i = 0; i < 2; ++i) {
        f.info[i].size = 352;
        memcpy(f.info[i].name, i ? "library.prx" : "application.elf", i ? 12 : 16);
        f.info[i].segmentCount = 1;
        f.info[i].segmentInfo[0].address = (void *)(uintptr_t)(0x1000 + i * 0x2000);
        f.info[i].segmentInfo[0].size = 0x800;
        f.info[i].segmentInfo[0].prot = -123; /* Preserve raw bits, no permission claim. */
        f.info[i].fingerprint[0] = (uint8_t)(0xA0 + i);
    }
    return f;
}

static int list(void *context, uint32_t *handles, size_t capacity, size_t *count) {
    fixture *f = context;
    ++f->list_calls;
    assert(capacity == MB_MODULE_METADATA_LIMIT);
    if (!f->omit_count) *count = f->count;
    size_t n = f->count < capacity ? f->count : capacity;
    memcpy(handles, f->handles, n * sizeof(*handles));
    return f->list_error;
}

static int info(void *context, uint32_t handle, mb_prospero_module_info *out) {
    fixture *f = context;
    ++f->info_calls;
    assert(out->size == 352);
    expect_zero(out->name, sizeof(*out) - offsetof(mb_prospero_module_info, name));
    *out = f->info[handle == 0 ? 0 : 1];
    return (int)handle == f->info_error_at ? f->info_error : 0;
}

static mb_module_metadata_provider provider(fixture *f) {
    mb_module_metadata_provider p = {f, list, info};
    return p;
}

/* Link-local stand-ins exercise the production native forwarding TU; they do
 * not resolve or call any target service. No target executable is produced. */
int sceKernelGetModuleInfo(uint32_t handle, mb_prospero_module_info *out) {
    return info(&native_fixture, handle, out);
}
int sceKernelGetModuleList(uint32_t *handles, size_t size, size_t *count) {
    (void)handles; (void)size; (void)count;
    ++native_fixture.list_calls;
    assert(!"native list must stay unavailable while size units are unverified");
    return -1;
}

static mb_module_metadata_result capture(fixture *f) {
    mb_module_metadata_provider p = provider(f);
    memset(&snapshot, 0xD5, sizeof(snapshot));
    return mb_module_metadata_capture(&p, &snapshot);
}

static void failure(fixture *f, mb_module_metadata_status status, size_t index) {
    mb_module_metadata_result r = capture(f);
    assert(r.status == status && r.module_index == index);
    expect_zero(&snapshot, sizeof(snapshot));
    ++cases;
}

int main(void) {
    fixture f = make_fixture();
    mb_module_metadata_result r = capture(&f);
    assert(r.status == MB_MODULE_METADATA_OK && snapshot.count == 2);
    assert(snapshot.records[0].handle == 0 && snapshot.records[1].handle == 1);
    assert(strcmp(snapshot.records[0].name, "application.elf") == 0);
    assert(snapshot.records[0].segments[0].reported_address == 0x1000);
    assert(snapshot.records[0].segments[0].reported_size == 0x800);
    assert(snapshot.records[0].segments[0].reported_protection == -123);
    assert(snapshot.records[1].fingerprint[0] == 0xA1);
    memset(&f, 0, sizeof(f));
    assert(strcmp(snapshot.records[1].name, "library.prx") == 0);
    expect_zero(&snapshot.records[2], sizeof(snapshot.records) - 2 * sizeof(snapshot.records[0]));
    expect_zero(&snapshot.records[0].segments[1], 3 * sizeof(snapshot.records[0].segments[0]));
    ++cases;

    f = make_fixture(); f.count = 0;
    assert(capture(&f).status == MB_MODULE_METADATA_OK && snapshot.count == 0 && f.info_calls == 0); ++cases;
    f = make_fixture(); f.count = MB_MODULE_METADATA_LIMIT;
    assert(capture(&f).status == MB_MODULE_METADATA_OK && snapshot.count == 256); ++cases;
    f = make_fixture(); f.count = MB_MODULE_METADATA_LIMIT + 1;
    failure(&f, MB_MODULE_METADATA_COUNT_OUT_OF_RANGE, SIZE_MAX); assert(f.info_calls == 0);
    f = make_fixture(); f.omit_count = 1;
    failure(&f, MB_MODULE_METADATA_COUNT_OUT_OF_RANGE, SIZE_MAX);
    f = make_fixture(); f.handles[1] = 0;
    failure(&f, MB_MODULE_METADATA_DUPLICATE_HANDLE, 1); assert(f.info_calls == 0);
    f = make_fixture(); f.list_error = -77;
    r = capture(&f); assert(r.status == MB_MODULE_METADATA_LIST_FAILED && r.provider_code == -77);
    expect_zero(&snapshot, sizeof(snapshot)); assert(f.info_calls == 0); ++cases;
    f = make_fixture(); f.info_error = 91; f.info_error_at = 1;
    r = capture(&f); assert(r.status == MB_MODULE_METADATA_INFO_FAILED && r.provider_code == 91 && r.module_index == 1 && r.handle == 1);
    expect_zero(&snapshot, sizeof(snapshot)); ++cases;
    f = make_fixture(); f.info[1].size = 351;
    failure(&f, MB_MODULE_METADATA_INFO_SIZE_MISMATCH, 1);
    f = make_fixture(); f.info[1].size = 353;
    failure(&f, MB_MODULE_METADATA_INFO_SIZE_MISMATCH, 1);
    f = make_fixture(); memset(f.info[1].name, 'x', 256);
    failure(&f, MB_MODULE_METADATA_INVALID_NAME, 1);
    f = make_fixture(); f.info[1].name[0] = 0;
    failure(&f, MB_MODULE_METADATA_INVALID_NAME, 1);
    f = make_fixture(); memset(f.info[0].name, 0xA2, 255); f.info[0].name[255] = 0;
    assert(capture(&f).status == MB_MODULE_METADATA_OK && (unsigned char)snapshot.records[0].name[254] == 0xA2); ++cases;
    f = make_fixture(); f.info[1].segmentCount = 5;
    failure(&f, MB_MODULE_METADATA_SEGMENT_COUNT_OUT_OF_RANGE, 1);
    f = make_fixture(); f.info[1].segmentCount = 0;
    failure(&f, MB_MODULE_METADATA_SEGMENT_COUNT_OUT_OF_RANGE, 1);
    f = make_fixture(); f.info[1].segmentInfo[0].address = NULL;
    failure(&f, MB_MODULE_METADATA_INVALID_SEGMENT, 1);
    f = make_fixture(); f.info[1].segmentInfo[0].size = 0;
    failure(&f, MB_MODULE_METADATA_INVALID_SEGMENT, 1);
    f = make_fixture(); f.info[1].segmentInfo[0].address = (void *)(uintptr_t)(UINT64_MAX - 0x100);
    r = capture(&f); assert(r.status == MB_MODULE_METADATA_ADDRESS_OVERFLOW && r.segment_index == 0);
    expect_zero(&snapshot, sizeof(snapshot)); ++cases;
    f = make_fixture(); f.info[0].segmentCount = 4;
    for (unsigned i = 1; i < 4; ++i) f.info[0].segmentInfo[i] = f.info[0].segmentInfo[0];
    f.info[0].segmentInfo[3].address = (void *)(uintptr_t)(UINT64_MAX - 1);
    f.info[0].segmentInfo[3].size = 1;
    assert(capture(&f).status == MB_MODULE_METADATA_OK && snapshot.records[0].segment_count == 4);
    assert(snapshot.records[0].segments[3].reported_address == UINT64_MAX - 1); ++cases;

    mb_module_metadata_record one;
    f = make_fixture(); mb_module_metadata_provider p = provider(&f); p.get_list = NULL;
    assert(mb_module_metadata_read(&p, 1, &one).status == MB_MODULE_METADATA_OK && one.handle == 1); ++cases;
    memset(&one, 1, sizeof(one)); p.get_info = NULL;
    assert(mb_module_metadata_read(&p, 1, &one).status == MB_MODULE_METADATA_INVALID_ARGUMENT);
    expect_zero(&one, sizeof(one)); ++cases;
    memset(&snapshot, 1, sizeof(snapshot));
    assert(mb_module_metadata_capture(NULL, &snapshot).status == MB_MODULE_METADATA_INVALID_ARGUMENT);
    expect_zero(&snapshot, sizeof(snapshot)); ++cases;
    p = provider(&f);
    assert(mb_module_metadata_capture(&p, NULL).status == MB_MODULE_METADATA_INVALID_ARGUMENT);
    assert(mb_module_metadata_read(&p, 1, NULL).status == MB_MODULE_METADATA_INVALID_ARGUMENT); ++cases;

    native_fixture = make_fixture();
    assert(mb_module_metadata_native_read(1, &one).status == MB_MODULE_METADATA_OK);
    assert(one.handle == 1 && strcmp(one.name, "library.prx") == 0 && native_fixture.info_calls == 1); ++cases;
    native_fixture.info[1].size = 0;
    assert(mb_module_metadata_native_read(1, &one).status == MB_MODULE_METADATA_INFO_SIZE_MISMATCH);
    expect_zero(&one, sizeof(one)); ++cases;
    native_fixture.info_error_at = 1; native_fixture.info_error = -42;
    r = mb_module_metadata_native_read(1, &one);
    assert(r.status == MB_MODULE_METADATA_INFO_FAILED && r.provider_code == -42);
    expect_zero(&one, sizeof(one)); ++cases;
    memset(&snapshot, 1, sizeof(snapshot));
    unsigned calls = native_fixture.info_calls;
    assert(mb_module_metadata_native_capture(&snapshot).status == MB_MODULE_METADATA_LIST_ABI_UNVERIFIED);
    expect_zero(&snapshot, sizeof(snapshot));
    assert(native_fixture.list_calls == 0 && native_fixture.info_calls == calls); ++cases;
    assert(mb_module_metadata_native_capture(NULL).status == MB_MODULE_METADATA_INVALID_ARGUMENT); ++cases;
    printf("PASS %u owned-provider cases; no native service execution\n", cases);
    return 0;
}
