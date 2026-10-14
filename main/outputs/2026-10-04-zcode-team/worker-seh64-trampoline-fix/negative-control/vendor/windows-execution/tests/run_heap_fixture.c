/* Local developer runner for heap_fixture.c only. No console interfaces. */
#include "../pe_module.h"
#include "../../windows-services/mb_win_heap_bridge.h"
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
typedef int (__attribute__((ms_abi)) *exercise_fn)(unsigned,void **);
typedef int32_t *(__attribute__((ms_abi)) *errno_fn)(void);
struct heap_backend {mb_win_thread_domain *domain;unsigned fail_next,allocations,releases;};
static void *heap_allocate(void *context,size_t size) {
    struct heap_backend *b=context;
    if (b->fail_next) {--b->fail_next;return NULL;}
    ++b->allocations;return mb_win_threads_allocate(b->domain,size);
}
static void heap_release(void *context,void *pointer) {
    struct heap_backend *b=context;++b->releases;mb_win_threads_release(b->domain,pointer);
}
static int32_t *heap_slot(void *context) {return mb_win_threads_errno(((struct heap_backend *)context)->domain);}
struct worker {mb_win_thread_domain *domain;errno_fn get;uintptr_t address;int result;};
static void *worker_main(void *context) {
    struct worker *w=context;int native=0;
    if (mb_win_threads_enter(w->domain,&native)!=MB_WIN_THREADS_OK) {w->result=1;return NULL;}
    int32_t *slot=w->get();
    if (!slot || slot!=mb_win_threads_errno(w->domain) || *slot!=0) w->result=2;
    else {w->address=(uintptr_t)slot;*slot=2468;if (*w->get()!=2468) w->result=3;}
    if (mb_win_threads_leave(w->domain,&native)!=MB_WIN_THREADS_OK) w->result=4;
    return NULL;
}
int main(int argc,char **argv) {
    CHECK(argc==2);
    FILE *file=fopen(argv[1],"rb");CHECK(file);
    CHECK(fseek(file,0,SEEK_END)==0);long size=ftell(file);
    CHECK(size>0 && size<1048576 && fseek(file,0,SEEK_SET)==0);
    unsigned char *bytes=malloc((size_t)size);CHECK(bytes);
    CHECK(fread(bytes,1,(size_t)size,file)==(size_t)size && fclose(file)==0);
    long page=sysconf(_SC_PAGESIZE);CHECK(page>0 && page<=16384 && 16384%page==0);
    mb_win_thread_domain domain={0};int native=0;
    CHECK(mb_win_threads_open(&domain,&native)==MB_WIN_THREADS_OK);
    CHECK(mb_win_threads_enter(&domain,&native)==MB_WIN_THREADS_OK);
    struct heap_backend backend={&domain,0,0,0};
    mb_win_heap_ops heap_ops={&backend,heap_allocate,heap_release,heap_slot};
    mb_win_heap heap={0};CHECK(mb_win_heap_open(&heap,&heap_ops)==MB_WIN_HEAP_OK);
    struct memory memory={0};
    mb_pe_memory_ops ops={&memory,16384,allocate_rw,protect_image,flush_image,release_image};
    mb_pe_module module={0};mb_pe_module_error error={0};
    mb_win_heap_bridge_config config={&heap,&domain,mb_win_threads_lock,mb_win_threads_unlock};
    mb_win_heap_bridge_session session={0};
    CHECK(mb_win_heap_bridge_begin(&session,&config,&module)==MB_WIN_HEAP_BRIDGE_OK);
    CHECK(heap.pin_count==1 && mb_win_heap_close(&heap)==MB_WIN_HEAP_BUSY);
    mb_pe_module_status status=mb_pe_module_open(&module,bytes,(size_t)size,&ops,mb_win_process_bind,&session,&error);
    if(status!=MB_PE_MODULE_OK) fprintf(stderr,"open: %s cause=%d image=%s detail=%u\n",mb_pe_module_status_name(status),error.cause,mb_pe_status_name(error.image.code),error.detail);
    CHECK(status==MB_PE_MODULE_OK && module.report.imports_bound==6 && module.report.relocations_applied==1);
    CHECK(mb_win_heap_bridge_end(&session)==MB_WIN_HEAP_BRIDGE_BUSY && heap.pin_count==1);
    mb_pe_export_lease exercise={0},get_errno={0};
    CHECK(mb_pe_module_acquire_export(&module,(mb_pe_string){"HeapExercise",12},&exercise,&error)==MB_PE_MODULE_OK);
    CHECK(mb_pe_module_acquire_export(&module,(mb_pe_string){"HeapErrno",9},&get_errno,&error)==MB_PE_MODULE_OK);
    void *retained=NULL;
    int result=((exercise_fn)exercise.address)(0,&retained);
    if(result) fprintf(stderr,"HeapExercise(0): %d\n",result);
    CHECK(result==0 && retained && ((uintptr_t)retained&15)==0 && heap.live_count==1 && heap.live_bytes==7);
    int32_t *windows_errno=((errno_fn)get_errno.address)();
    CHECK(windows_errno==mb_win_threads_errno(&domain) && *windows_errno==1777);
    for(unsigned i=0;i<7;++i) CHECK(((unsigned char *)retained)[i]==i+1);
    errno=733;backend.fail_next=1;
    unsigned allocations=backend.allocations;
    result=((exercise_fn)exercise.address)(1,&retained);
    if(result) fprintf(stderr,"HeapExercise(1): %d\n",result);
    CHECK(result==0 && backend.fail_next==0 && backend.allocations==allocations && errno==733);
    CHECK(*windows_errno==1999 && heap.live_count==1 && heap.live_bytes==7);
    struct worker worker={&domain,(errno_fn)get_errno.address,0,0};pthread_t thread;
    CHECK(pthread_create(&thread,NULL,worker_main,&worker)==0 && pthread_join(thread,NULL)==0);
    CHECK(worker.result==0 && worker.address!=(uintptr_t)windows_errno && *windows_errno==1999);
    CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_BUSY);
    CHECK(mb_pe_module_release(&exercise)==MB_PE_MODULE_OK && mb_pe_module_release(&get_errno)==MB_PE_MODULE_OK);
    memory.fail_release=1;
    CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_CLEANUP_REQUIRED && heap.pin_count==1);
    CHECK(mb_win_heap_bridge_end(&session)==MB_WIN_HEAP_BRIDGE_BUSY && session.active);
    CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK && module.state==MB_PE_MODULE_EMPTY);
    CHECK(mb_win_heap_bridge_end(&session)==MB_WIN_HEAP_BRIDGE_BUSY && heap.pin_count==1);
    mb_win_threads_lock(&domain);mb_win_heap_free(&heap,retained);mb_win_threads_unlock(&domain);
    CHECK(backend.allocations==backend.releases && heap.live_count==0 && heap.live_bytes==0);
    CHECK(mb_win_heap_bridge_end(&session)==MB_WIN_HEAP_BRIDGE_OK && !heap.pin_count);
    CHECK(mb_win_heap_close(&heap)==MB_WIN_HEAP_OK && memory.releases==1);
    CHECK(mb_win_threads_leave(&domain,&native)==MB_WIN_THREADS_OK);
    CHECK(mb_win_threads_close(&domain,&native)==MB_WIN_THREADS_OK);
    free(bytes);
    puts("{\"owned_pe_executed\":true,\"heap_errno_imports_called\":5,\"composed_crt_imports_called\":1,\"realloc_failure_preserved\":true,\"thread_errno_isolated\":true,\"session_lifetime_passed\":true,\"unloaded\":true,\"console_contact\":false}");
    return 0;
}
