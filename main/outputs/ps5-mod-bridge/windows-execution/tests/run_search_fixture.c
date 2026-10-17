/* Local developer runner for search_fixture.c only. No console interfaces. */
#include "../pe_module.h"
#include "../../windows-services/mb_win_crt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#if !defined(__x86_64__)
#error "Execute this harness as x86_64 (Rosetta is suitable on Apple Silicon)"
#endif
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)

struct memory { void *raw, *aligned; size_t raw_size, size; unsigned allocations, protections, flushes, releases; };
static void *allocate_rw(void *context, size_t size, size_t alignment)
{
    struct memory *m = context;
    if (m->raw || size > SIZE_MAX - alignment) return NULL;
    size_t total = size + alignment;
    void *raw = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (raw == MAP_FAILED) return NULL;
    uintptr_t address = ((uintptr_t)raw + alignment - 1) & ~(uintptr_t)(alignment - 1);
    m->raw = raw; m->raw_size = total; m->aligned = (void *)address; m->size = size;
    ++m->allocations;
    return m->aligned;
}
static int protect_image(void *context, void *base, size_t offset, size_t length, unsigned rights)
{
    struct memory *m = context;
    if (base != m->aligned || offset > m->size || length > m->size - offset ||
        (rights & ~7u) || ((rights & 6u) == 6u)) return -1;
    int prot = (rights & 1u ? PROT_READ : 0) | (rights & 2u ? PROT_WRITE : 0) | (rights & 4u ? PROT_EXEC : 0);
    if (mprotect((char *)base + offset, length, prot)) { perror("mprotect"); return -1; }
    ++m->protections;
    return 0;
}
static int flush_image(void *context, void *base, size_t length)
{
    struct memory *m = context;
    if (base != m->aligned || length > m->size) return -1;
    __builtin___clear_cache((char *)base, (char *)base + length);
    ++m->flushes;
    return 0;
}
static int release_image(void *context, void *base, size_t length)
{
    struct memory *m = context;
    if (base != m->aligned || length != m->size || !m->raw) return -1;
    if (munmap(m->raw, m->raw_size)) return -1;
    m->raw = m->aligned = NULL; m->size = m->raw_size = 0; ++m->releases;
    return 0;
}
typedef int (MB_WIN_X64_ABI *search_fn)(const char *, const unsigned char *,
                                      void **, unsigned long long);
int main(int argc, char **argv)
{
    CHECK(argc == 2);
    FILE *file = fopen(argv[1], "rb"); CHECK(file != NULL);
    CHECK(fseek(file, 0, SEEK_END) == 0); long size = ftell(file);
    CHECK(size > 0 && size < 1048576 && fseek(file, 0, SEEK_SET) == 0);
    unsigned char *bytes = malloc((size_t)size); CHECK(bytes != NULL);
    CHECK(fread(bytes, 1, (size_t)size, file) == (size_t)size && fclose(file) == 0);
    long page = sysconf(_SC_PAGESIZE); CHECK(page > 0 && page <= 16384 && 16384 % page == 0);
    struct memory memory = {0}, guard = {0};
    mb_pe_memory_ops ops = {&memory, 16384, allocate_rw, protect_image, flush_image, release_image};
    mb_pe_module module = {0}; mb_pe_module_error error = {0};
    mb_pe_module_status status = mb_pe_module_open(&module, bytes, (size_t)size, &ops, mb_win_crt_bind, NULL, &error);
    if (status != MB_PE_MODULE_OK) fprintf(stderr, "open: %s, cause=%d detail=%u image=%s rva=%u\n",
        mb_pe_module_status_name(status), error.cause, error.detail, mb_pe_status_name(error.image.code), error.image.rva);
    CHECK(status == MB_PE_MODULE_OK && module.state == MB_PE_MODULE_ACTIVE);
    CHECK(module.report.imports_bound == 4 && module.report.imports_unresolved == 0 && module.report.relocations_applied == 1);
    CHECK(memory.allocations == 1 && memory.flushes == 1 && memory.protections > 0);
    mb_pe_export_lease lease = {0};
    CHECK(mb_pe_module_acquire_export(&module, (mb_pe_string){"SearchExercise", 14}, &lease, &error) == MB_PE_MODULE_OK);
    CHECK(mb_pe_module_close(&module, &error) == MB_PE_MODULE_BUSY && memory.releases == 0);

    unsigned char *guard_base = allocate_rw(&guard, 32768, 16384); CHECK(guard_base != NULL);
    unsigned char *last_byte = guard_base + 16384 - 1;
    *last_byte = 0x80;
    CHECK(mprotect(guard_base + 16384, 16384, PROT_NONE) == 0);
    CHECK(mprotect(guard_base, 16384, PROT_READ) == 0);
    const char input[] = {'a', (char)0x80, 'b', 'a', (char)0xff, 0, 'z', (char)0x80};
    void *found[6] = {0};
    int result = ((search_fn)lease.address)(input, last_byte, found, 6);
    if (result) fprintf(stderr, "SearchExercise returned %d\n", result);
    CHECK(result == 0);
    const unsigned offsets[] = {1, 0, 3, 1, 5, 0};
    for (unsigned i = 0; i < 6; ++i) CHECK(found[i] == input + offsets[i]);
    CHECK(release_image(&guard, guard_base, 32768) == 0 && guard.releases == 1);
    CHECK(mb_pe_module_release(&lease) == MB_PE_MODULE_OK && lease.address == NULL);
    CHECK(mb_pe_module_close(&module, &error) == MB_PE_MODULE_OK);
    CHECK(memory.releases == 1 && memory.raw == NULL && module.state == MB_PE_MODULE_EMPTY);
    free(bytes);
    printf("{\"owned_pe_executed\":true,\"search_imports_called\":4,"
           "\"original_pointer_offsets\":[%td,%td,%td,%td,%td,%td],"
           "\"guarded_bounds_passed\":true,\"unloaded\":true,\"console_contact\":false}\n",
           (const char *)found[0] - input, (const char *)found[1] - input,
           (const char *)found[2] - input, (const char *)found[3] - input,
           (const char *)found[4] - input, (const char *)found[5] - input);
    return 0;
}
