/* Local developer runner for dependency_provider.c/dependency_consumer.c only. No console interfaces. */
#include "../pe_dependencies.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#if !defined(__x86_64__)
#error "Execute this harness as x86_64 (Rosetta is suitable on Apple Silicon)"
#endif
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)

struct memory {
    void *raw, *aligned;
    size_t raw_size, size;
    unsigned allocations, protections, flushes, releases, release_attempts;
    int fail_release, pin_violation;
    mb_pe_module *provider;
};
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
    ++m->release_attempts;
    if (m->provider && (m->provider->state != MB_PE_MODULE_ACTIVE || m->provider->lease_count != 1)) {
        m->pin_violation = 1;
        return -1;
    }
    if (m->fail_release) return -1;
    if (munmap(m->raw, m->raw_size)) return -1;
    m->raw = m->aligned = NULL; m->size = m->raw_size = 0; ++m->releases;
    return 0;
}

typedef unsigned long long (__attribute__((ms_abi)) *compute_fn)(unsigned long long);
struct fallback { mb_pe_module *provider; unsigned calls; int observed_pin; };
static mb_pe_bind_result missing_system(void *context, const mb_pe_import *import, uint64_t *value)
{
    struct fallback *f = context;
    (void)value;
    ++f->calls;
    if (import->by_ordinal || import->library.size != sizeof("KERNEL32.dll") - 1 ||
        memcmp(import->library.data, "KERNEL32.dll", sizeof("KERNEL32.dll") - 1) ||
        import->name.size != sizeof("UnimplementedFixtureApi") - 1 ||
        memcmp(import->name.data, "UnimplementedFixtureApi", sizeof("UnimplementedFixtureApi") - 1))
        return MB_PE_BIND_ERROR;
    f->observed_pin = f->provider->state == MB_PE_MODULE_ACTIVE && f->provider->lease_count == 1;
    return MB_PE_BIND_UNRESOLVED;
}
static unsigned char *read_file(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END)) { fclose(file); return NULL; }
    long length = ftell(file);
    if (length <= 0 || length >= 1048576 || fseek(file, 0, SEEK_SET)) { fclose(file); return NULL; }
    unsigned char *bytes = malloc((size_t)length);
    if (!bytes) { fclose(file); return NULL; }
    int ok = fread(bytes, 1, (size_t)length, file) == (size_t)length;
    if (fclose(file)) ok = 0;
    if (!ok) { free(bytes); return NULL; }
    *size = (size_t)length;
    return bytes;
}
int main(int argc, char **argv)
{
    CHECK(argc == 4);
    size_t provider_size = 0, consumer_size = 0, missing_size = 0;
    unsigned char *provider_file = read_file(argv[1], &provider_size);
    unsigned char *consumer_file = read_file(argv[2], &consumer_size);
    unsigned char *missing_file = read_file(argv[3], &missing_size);
    CHECK(provider_file && consumer_file && missing_file);
    long page = sysconf(_SC_PAGESIZE); CHECK(page > 0 && page <= 16384 && 16384 % page == 0);
    mb_pe_module provider = {0}; mb_pe_module_error error = {0};
    struct memory provider_memory = {0}, consumer_memory = {0}, missing_memory = {0};
    consumer_memory.provider = missing_memory.provider = &provider;
    mb_pe_memory_ops provider_ops = {&provider_memory, 16384, allocate_rw, protect_image, flush_image, release_image};
    mb_pe_memory_ops consumer_ops = {&consumer_memory, 16384, allocate_rw, protect_image, flush_image, release_image};
    mb_pe_memory_ops missing_ops = {&missing_memory, 16384, allocate_rw, protect_image, flush_image, release_image};
    CHECK(mb_pe_module_open(&provider, provider_file, provider_size, &provider_ops, NULL, NULL, &error) == MB_PE_MODULE_OK);
    CHECK(provider.report.relocations_applied == 1 && provider.lease_count == 0);
    const mb_pe_dependency registry[] = {{{"DependencyProvider.dll", sizeof("DependencyProvider.dll") - 1}, &provider}};
    struct fallback fallback = {&provider, 0, 0};
    mb_pe_link consumer = {0}; mb_pe_export_lease pins[2] = {{0}};
    mb_pe_module_status status = mb_pe_link_open(&consumer, consumer_file, consumer_size,
        &consumer_ops, NULL, registry, 1, pins, 2, missing_system, &fallback, &error);
    if (status != MB_PE_MODULE_OK) fprintf(stderr, "consumer open: %s cause=%d image=%s\n",
        mb_pe_module_status_name(status), error.cause, mb_pe_status_name(error.image.code));
    CHECK(status == MB_PE_MODULE_OK && consumer.module.state == MB_PE_MODULE_ACTIVE && consumer.owned);
    CHECK(consumer.module.report.imports_bound == 1 && consumer.module.report.relocations_applied == 1);
    CHECK(consumer.pin_count == 1 && provider.lease_count == 1 && fallback.calls == 0);
    CHECK(mb_pe_module_close(&provider, &error) == MB_PE_MODULE_BUSY && provider_memory.releases == 0);
    mb_pe_export_lease call = {0};
    CHECK(mb_pe_module_acquire_export(&consumer.module,
        (mb_pe_string){"ConsumerCompute", sizeof("ConsumerCompute") - 1}, &call, &error) == MB_PE_MODULE_OK);
    unsigned long long result = ((compute_fn)call.address)(11);
    CHECK(result == 89);
    CHECK(mb_pe_link_close(&consumer, &error) == MB_PE_MODULE_BUSY && consumer.pin_count == 1 && provider.lease_count == 1);
    CHECK(consumer_memory.release_attempts == 0 && mb_pe_module_close(&provider, &error) == MB_PE_MODULE_BUSY);
    CHECK(mb_pe_module_release(&call) == MB_PE_MODULE_OK);
    consumer_memory.fail_release = 1;
    CHECK(mb_pe_link_close(&consumer, &error) == MB_PE_MODULE_CLEANUP_REQUIRED);
    CHECK(consumer.module.state == MB_PE_MODULE_CLEANUP && consumer.owned && consumer.pin_count == 1 && provider.lease_count == 1);
    CHECK(!consumer_memory.pin_violation && consumer_memory.release_attempts == 1 && consumer_memory.releases == 0);
    CHECK(mb_pe_module_close(&provider, &error) == MB_PE_MODULE_BUSY);
    consumer_memory.fail_release = 0;
    CHECK(mb_pe_link_close(&consumer, &error) == MB_PE_MODULE_OK);
    CHECK(consumer.module.state == MB_PE_MODULE_EMPTY && !consumer.owned && consumer.pin_count == 0 && provider.lease_count == 0);
    CHECK(!pins[0].owner && consumer_memory.releases == 1 && !consumer_memory.pin_violation);

    /* This image first binds ProviderCompute, then fails on its KERNEL32 name.
     * The fallback and release callback observe that the provider is still pinned. */
    mb_pe_link missing = {0}; mb_pe_export_lease missing_pins[2] = {{0}};
    CHECK(mb_pe_link_open(&missing, missing_file, missing_size, &missing_ops, NULL,
        registry, 1, missing_pins, 2, missing_system, &fallback, &error) == MB_PE_MODULE_UNRESOLVED_IMPORTS);
    CHECK(fallback.calls == 1 && fallback.observed_pin && missing_memory.releases == 1 && !missing_memory.pin_violation);
    CHECK(missing.module.state == MB_PE_MODULE_EMPTY && !missing.owned && provider.lease_count == 0 && !missing_pins[0].owner);

    /* Failed cleanup after that rejected load must retain the same dependency
     * until a later successful consumer release, with the original cause intact. */
    missing_memory.fail_release = 1;
    fallback.observed_pin = 0;
    CHECK(mb_pe_link_open(&missing, missing_file, missing_size, &missing_ops, NULL,
        registry, 1, missing_pins, 2, missing_system, &fallback, &error) == MB_PE_MODULE_CLEANUP_REQUIRED);
    CHECK(error.cause == MB_PE_MODULE_UNRESOLVED_IMPORTS && fallback.calls == 2 && fallback.observed_pin);
    CHECK(missing.owned && missing.module.state == MB_PE_MODULE_CLEANUP && provider.lease_count == 1 && !missing_memory.pin_violation);
    CHECK(mb_pe_module_close(&provider, &error) == MB_PE_MODULE_BUSY);
    missing_memory.fail_release = 0;
    CHECK(mb_pe_link_close(&missing, &error) == MB_PE_MODULE_OK);
    CHECK(!missing.owned && missing.module.state == MB_PE_MODULE_EMPTY && provider.lease_count == 0 && !missing_pins[0].owner);
    CHECK(missing_memory.releases == 2 && !missing_memory.pin_violation);
    CHECK(mb_pe_module_close(&provider, &error) == MB_PE_MODULE_OK && provider_memory.releases == 1);
    free(provider_file); free(consumer_file); free(missing_file);
    printf("{\"owned_dependency_pe_executed\":true,\"consumer_result\":%llu,"
           "\"busy_pin_retained\":true,\"failed_release_pin_retained\":true,"
           "\"failed_load_pins_rolled_back\":true,\"failed_load_cleanup_pin_retained\":true,"
           "\"all_unloaded\":true,\"console_contact\":false}\n", result);
    return 0;
}
