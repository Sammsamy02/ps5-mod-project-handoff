/* Local developer runner for tls_fixture.c only. No console interfaces. */
#include "../pe_module.h"
#include "../../windows-services/mb_win_tls_bridge.h"
#include "../../windows-services/mb_win_threads.h"
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#if !defined(__x86_64__)
#error "Execute this harness as x86_64 (Rosetta is suitable on Apple Silicon)"
#endif
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)

struct memory { void *raw, *aligned; size_t raw_size, size; unsigned allocations, protections, flushes, releases, fail_release; };
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
    if (m->fail_release) { --m->fail_release; return -1; }
    if (munmap(m->raw, m->raw_size)) return -1;
    m->raw = m->aligned = NULL; m->size = m->raw_size = 0; ++m->releases;
    return 0;
}
typedef int (__attribute__((ms_abi)) *exercise_fn)(uint32_t,uint32_t *,void *,void **);
struct worker {
    mb_win_thread_domain *domain;exercise_fn exercise;uint32_t *index;
    pthread_mutex_t mutex;pthread_cond_t condition;unsigned phase;
    int result,payload;void *observed;
};
static void *worker_main(void *context) {
    struct worker *w=context;int native=0,entered=0;
    if(mb_win_threads_enter(w->domain,&native)!=MB_WIN_THREADS_OK) w->result=100;
    else {entered=1;errno=844;w->result=w->exercise(1,w->index,&w->payload,&w->observed);if(errno!=844)w->result=101;}
    if(pthread_mutex_lock(&w->mutex))abort();w->phase=1;
    if(pthread_cond_signal(&w->condition))abort();
    while(w->phase<2)if(pthread_cond_wait(&w->condition,&w->mutex))abort();
    if(pthread_mutex_unlock(&w->mutex))abort();
    if(!w->result) {w->result=w->exercise(3,w->index,&w->payload,&w->observed);if(errno!=844)w->result=102;}
    if(entered && mb_win_threads_leave(w->domain,&native)!=MB_WIN_THREADS_OK)w->result=103;
    return NULL;
}
int main(int argc,char **argv) {
    CHECK(argc==2);FILE *file=fopen(argv[1],"rb");CHECK(file);
    CHECK(fseek(file,0,SEEK_END)==0);long size=ftell(file);
    CHECK(size>0 && size<1048576 && fseek(file,0,SEEK_SET)==0);
    unsigned char *bytes=malloc((size_t)size);CHECK(bytes);
    CHECK(fread(bytes,1,(size_t)size,file)==(size_t)size && fclose(file)==0);
    long page=sysconf(_SC_PAGESIZE);CHECK(page>0 && page<=16384 && 16384%page==0);
    mb_win_thread_domain domain={0};int native=0;
    CHECK(mb_win_threads_open(&domain,&native)==MB_WIN_THREADS_OK);
    CHECK(mb_win_threads_enter(&domain,&native)==MB_WIN_THREADS_OK);
    mb_win_heap heap={0};mb_win_heap_ops hops={&domain,mb_win_threads_allocate,mb_win_threads_release,mb_win_threads_errno};
    CHECK(mb_win_heap_open(&heap,&hops)==MB_WIN_HEAP_OK);
    struct memory memory={0};mb_pe_memory_ops ops={&memory,16384,allocate_rw,protect_image,flush_image,release_image};
    mb_pe_module module={0};mb_pe_module_error error={0};
    mb_win_heap_bridge_config config={&heap,&domain,mb_win_threads_lock,mb_win_threads_unlock};
    mb_win_heap_bridge_session process={0};mb_win_tls_bridge_session tls={0};
    CHECK(mb_win_heap_bridge_begin(&process,&config,&module)==MB_WIN_HEAP_BRIDGE_OK);
    CHECK(mb_win_tls_bridge_begin(&tls,&domain,&module)==MB_WIN_TLS_BRIDGE_OK);
    mb_win_runtime_bind_context binding={&tls,&process};
    mb_pe_module_status status=mb_pe_module_open(&module,bytes,(size_t)size,&ops,mb_win_runtime_bind,&binding,&error);
    if(status!=MB_PE_MODULE_OK)fprintf(stderr,"open: %s cause=%d image=%s detail=%u\n",mb_pe_module_status_name(status),error.cause,mb_pe_status_name(error.image.code),error.detail);
    CHECK(status==MB_PE_MODULE_OK && module.report.imports_bound==8 && module.report.relocations_applied==1);
    CHECK(mb_win_tls_bridge_end(&tls)==MB_WIN_TLS_BRIDGE_BUSY);
    mb_pe_export_lease lease={0};
    CHECK(mb_pe_module_acquire_export(&module,(mb_pe_string){"TlsExercise",11},&lease,&error)==MB_PE_MODULE_OK);
    exercise_fn exercise=(exercise_fn)lease.address;
    uint32_t index=UINT32_MAX;int payload=12345;void *observed=NULL;errno=733;
    int result=exercise(0,&index,&payload,&observed);
    if(result)fprintf(stderr,"TlsExercise(0): %d\n",result);
    CHECK(result==0 && index!=UINT32_MAX && observed==&payload && errno==733);
    struct worker worker={.domain=&domain,.exercise=exercise,.index=&index,.payload=54321};pthread_t thread;
    CHECK(pthread_mutex_init(&worker.mutex,NULL)==0 && pthread_cond_init(&worker.condition,NULL)==0);
    CHECK(pthread_create(&thread,NULL,worker_main,&worker)==0);
    CHECK(pthread_mutex_lock(&worker.mutex)==0);
    while(worker.phase<1)CHECK(pthread_cond_wait(&worker.condition,&worker.mutex)==0);
    CHECK(worker.result==0 && worker.observed==&worker.payload);
    CHECK(pthread_mutex_unlock(&worker.mutex)==0);
    result=exercise(2,&index,&payload,&observed);
    if(result)fprintf(stderr,"TlsExercise(2): %d\n",result);
    CHECK(result==0 && observed==NULL && payload==12345 && worker.payload==54321 && errno==733);
    CHECK(pthread_mutex_lock(&worker.mutex)==0);worker.phase=2;
    CHECK(pthread_cond_signal(&worker.condition)==0 && pthread_mutex_unlock(&worker.mutex)==0);
    CHECK(pthread_join(thread,NULL)==0 && worker.result==0 && worker.observed==NULL && worker.payload==54321);
    CHECK(pthread_cond_destroy(&worker.condition)==0 && pthread_mutex_destroy(&worker.mutex)==0);
    result=exercise(4,&index,&payload,&observed);
    if(result)fprintf(stderr,"TlsExercise(4): %d\n",result);
    CHECK(result==0 && payload==12345 && errno==733 && *mb_win_threads_errno(&domain)==1717);
    CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_BUSY);
    CHECK(mb_pe_module_release(&lease)==MB_PE_MODULE_OK);
    memory.fail_release=1;
    CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_CLEANUP_REQUIRED);
    CHECK(mb_win_tls_bridge_end(&tls)==MB_WIN_TLS_BRIDGE_BUSY && tls.active);
    CHECK(mb_win_heap_bridge_end(&process)==MB_WIN_HEAP_BRIDGE_BUSY);
    CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK && memory.releases==1);
    CHECK(mb_win_threads_leave(&domain,&native)==MB_WIN_THREADS_OK);
    CHECK(mb_win_threads_close(&domain,&native)==MB_WIN_THREADS_BUSY);
    CHECK(mb_win_heap_bridge_end(&process)==MB_WIN_HEAP_BRIDGE_OK);
    CHECK(mb_win_heap_close(&heap)==MB_WIN_HEAP_OK);
    CHECK(mb_win_threads_close(&domain,&native)==MB_WIN_THREADS_BUSY);
    CHECK(mb_win_tls_bridge_end(&tls)==MB_WIN_TLS_BRIDGE_OK);
    CHECK(mb_win_threads_close(&domain,&native)==MB_WIN_THREADS_OK);
    free(bytes);
    puts("{\"owned_pe_executed\":true,\"tls_imports_called\":6,\"composed_imports_called\":2,\"thread_values_isolated\":true,\"reuse_clears_existing_thread\":true,\"last_error_effects_passed\":true,\"cleanup_pin_lifetime_passed\":true,\"unloaded\":true,\"console_contact\":false}");return 0;
}
