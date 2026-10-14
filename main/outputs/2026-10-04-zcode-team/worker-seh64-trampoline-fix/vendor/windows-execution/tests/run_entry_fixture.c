/* Execute only entry_fixture.c, built by verify_entry_execution.py, locally. */
#include "../pe_module.h"
#include "../../windows-services/mb_win_entry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#if !defined(__x86_64__)
#error "This owned-fixture harness requires x86_64"
#endif
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
struct trace { uint32_t attach, detach, sequence, bad_arguments; uint64_t image_base; };
_Static_assert(sizeof(struct trace) == 24, "fixture trace layout");
struct memory {
    void *raw, *aligned;
    size_t raw_size, size;
    uintptr_t allocated_base;
    uint32_t trace_rva;
    unsigned release_attempts, releases, fail_release_once;
    struct trace snapshots[2];
};
struct trace_lookup { uint32_t rva; unsigned found; };
static int find_trace(void *context, const mb_pe_export *item)
{
    struct trace_lookup *t = context;
    if (item->name.size == 10 && !memcmp(item->name.data, "EntryTrace", 10)) {
        if (item->forwarder.size || t->found) return -1;
        t->rva = item->rva; ++t->found;
    }
    return 0;
}
static void *allocate_rw(void *context, size_t size, size_t alignment)
{
    struct memory *m = context;
    if (m->raw || size > SIZE_MAX - alignment) return NULL;
    size_t total = size + alignment;
    void *raw = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (raw == MAP_FAILED) return NULL;
    uintptr_t address = ((uintptr_t)raw + alignment - 1) & ~(uintptr_t)(alignment - 1);
    m->raw = raw; m->raw_size = total; m->aligned = (void *)address;
    m->size = size; m->allocated_base = address;
    return m->aligned;
}
static int protect_image(void *context, void *base, size_t offset, size_t length, unsigned rights)
{
    struct memory *m = context;
    if (base != m->aligned || offset > m->size || length > m->size - offset ||
        (rights & ~7u) || ((rights & 6u) == 6u)) return -1;
    int prot = (rights & 1u ? PROT_READ : 0) | (rights & 2u ? PROT_WRITE : 0) |
               (rights & 4u ? PROT_EXEC : 0);
    return mprotect((char *)base + offset, length, prot);
}
static int flush_image(void *context, void *base, size_t length)
{
    struct memory *m = context;
    if (base != m->aligned || length != m->size) return -1;
    __builtin___clear_cache((char *)base, (char *)base + length);
    return 0;
}
static int release_image(void *context, void *base, size_t length)
{
    struct memory *m = context;
    if (base != m->aligned || length != m->size || !m->raw || m->release_attempts >= 2 ||
        m->trace_rva > length || sizeof(struct trace) > length - m->trace_rva) return -1;
    /* No image code may run now. Make our own mapping readable even if an
     * earlier protection operation failed; remove execute permission first. */
    if (mprotect(base, length, PROT_READ | PROT_WRITE)) return -1;
    memcpy(&m->snapshots[m->release_attempts], (char *)base + m->trace_rva, sizeof(struct trace));
    ++m->release_attempts;
    if (m->fail_release_once) { m->fail_release_once = 0; return -1; }
    if (munmap(m->raw, m->raw_size)) return -1;
    m->raw = m->aligned = NULL; m->size = m->raw_size = 0; ++m->releases;
    return 0;
}
typedef uint32_t (__attribute__((ms_abi)) *status_fn)(void);

static int exercise(const void *bytes, size_t size, int rejection, unsigned retry)
{
    mb_pe_info info; mb_pe_error pe_error;
    struct trace_lookup lookup = {0};
    CHECK(mb_pe_inspect(bytes, size, &info, &pe_error) == MB_PE_OK);
    CHECK(info.entry_rva != 0 && info.import_count == 0 && info.relocation_count == 1);
    CHECK(mb_pe_visit_exports(bytes, size, find_trace, &lookup, &pe_error) == MB_PE_OK);
    CHECK(lookup.found == 1 && lookup.rva <= info.image_size && sizeof(struct trace) <= info.image_size - lookup.rva);
    struct memory memory = {0}; memory.trace_rva = lookup.rva; memory.fail_release_once = retry;
    mb_pe_memory_ops ops = {&memory, 16384, allocate_rw, protect_image, flush_image, release_image};
    mb_pe_entry_ops entry = {NULL, mb_win_call_entry};
    mb_pe_module module = {0}; mb_pe_module_error error = {0};
    mb_pe_module_status status = mb_pe_module_open_with_entry(&module, bytes, size, &ops, &entry, NULL, NULL, &error);
    mb_pe_export_lease lease = {0};
    if (rejection) {
        CHECK(status == (retry ? MB_PE_MODULE_CLEANUP_REQUIRED : MB_PE_MODULE_ENTRY_REJECTED));
        CHECK(error.cause == MB_PE_MODULE_ENTRY_REJECTED);
        CHECK(module.state == (retry ? MB_PE_MODULE_CLEANUP : MB_PE_MODULE_EMPTY));
        CHECK(mb_pe_module_acquire_export(&module, (mb_pe_string){"Status", 6}, &lease, &error) == MB_PE_MODULE_BAD_STATE);
    } else {
        if (status != MB_PE_MODULE_OK) fprintf(stderr, "open=%s cause=%d image=%s detail=%u\n",
            mb_pe_module_status_name(status), error.cause, mb_pe_status_name(error.image.code), error.detail);
        CHECK(status == MB_PE_MODULE_OK && module.state == MB_PE_MODULE_ACTIVE);
        CHECK(mb_pe_module_acquire_export(&module, (mb_pe_string){"Status", 6}, &lease, &error) == MB_PE_MODULE_OK);
        CHECK(mb_pe_module_close(&module, &error) == MB_PE_MODULE_BUSY && memory.release_attempts == 0);
        CHECK(((status_fn)lease.address)() == 100); /* Attach succeeded; no premature detach. */
        CHECK(mb_pe_module_release(&lease) == MB_PE_MODULE_OK);
        status = mb_pe_module_close(&module, &error);
        CHECK(status == (retry ? MB_PE_MODULE_CLEANUP_REQUIRED : MB_PE_MODULE_OK));
    }
    if (retry) {
        CHECK(memory.raw != NULL && memory.releases == 0 && memory.release_attempts == 1);
        CHECK(module.state == MB_PE_MODULE_CLEANUP && module.entry_attached == 0);
        CHECK(module.failure.cause == (rejection ? MB_PE_MODULE_ENTRY_REJECTED : MB_PE_MODULE_MEMORY_ERROR));
        CHECK(mb_pe_module_close(&module, &error) == MB_PE_MODULE_OK);
    }
    CHECK(module.state == MB_PE_MODULE_EMPTY && memory.raw == NULL && memory.releases == 1);
    CHECK(memory.release_attempts == 1 + retry);
    for (unsigned i = 0; i < memory.release_attempts; ++i) {
        const struct trace *t = &memory.snapshots[i];
        CHECK(t->attach == 1 && t->detach == 1 && t->sequence == 12 && t->bad_arguments == 0);
        CHECK(t->image_base == memory.allocated_base);
    }
    CHECK(mb_pe_module_close(&module, &error) == MB_PE_MODULE_OK && memory.release_attempts == 1 + retry);
    return 0;
}
int main(int argc, char **argv)
{
    CHECK(argc == 3);
    long page = sysconf(_SC_PAGESIZE); CHECK(page > 0 && page <= 16384 && 16384 % page == 0);
    for (int reject = 0; reject < 2; ++reject) {
        FILE *f = fopen(argv[reject + 1], "rb"); CHECK(f != NULL);
        CHECK(fseek(f, 0, SEEK_END) == 0); long size = ftell(f);
        CHECK(size > 0 && size < 1048576 && fseek(f, 0, SEEK_SET) == 0);
        void *bytes = malloc((size_t)size); CHECK(bytes != NULL);
        CHECK(fread(bytes, 1, (size_t)size, f) == (size_t)size && fclose(f) == 0);
        CHECK(exercise(bytes, (size_t)size, reject, 0) == 0);
        CHECK(exercise(bytes, (size_t)size, reject, 1) == 0);
        free(bytes);
    }
    puts("{\"owned_entry_dlls_executed\":2,\"lifecycle_cases\":4,\"attach_rejection_detached\":true,\"cleanup_retry_detached_once\":true,\"busy_close_preserved_entry\":true,\"console_contact\":false}");
    return 0;
}
