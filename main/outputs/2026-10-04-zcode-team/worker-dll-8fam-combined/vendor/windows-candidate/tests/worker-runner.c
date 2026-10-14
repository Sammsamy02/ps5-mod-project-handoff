/* Actual x86_64 execution of self-owned worker PE; no console interfaces. */
#include "../../windows-execution/pe_module.h"
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
#include "../../windows-services/mb_win_workers_abi.h"
#include "../call_exports.h"
#include "worker-state.h"

struct code_context {
 mb_pe_module *module;pthread_mutex_t mutex;
 unsigned acquired,released,fail_release;void *start,*begin,*collect;
};
static int acquire_worker_code(void *opaque,void *start,void **token) {
 struct code_context *c=opaque;mb_pe_export_lease *lease=calloc(1,sizeof(*lease));
 if(!lease)return 1;
 if(pthread_mutex_lock(&c->mutex))abort();
 mb_pe_module_error error={0};mb_pe_module_status status=mb_pe_module_acquire_code(c->module,start,lease,&error);
 if(status==MB_PE_MODULE_OK){++c->acquired;c->start=start;*token=lease;}
 if(pthread_mutex_unlock(&c->mutex))abort();
 if(status!=MB_PE_MODULE_OK){free(lease);return 1;}return 0;
}
static int release_worker_code(void *opaque,void *token) {
 struct code_context *c=opaque;mb_pe_export_lease *lease=token;
 if(pthread_mutex_lock(&c->mutex))abort();
 if(c->fail_release){--c->fail_release;if(pthread_mutex_unlock(&c->mutex))abort();return 1;}
 mb_pe_module_status status=mb_pe_module_release(lease);
 if(status==MB_PE_MODULE_OK)++c->released;
 if(pthread_mutex_unlock(&c->mutex))abort();
 if(status==MB_PE_MODULE_OK)free(lease);
 return status==MB_PE_MODULE_OK?0:1;
}
struct binding {mb_win_worker_session *worker;mb_win_tls_bridge_session *tls;};
static mb_pe_bind_result bind_import(void *opaque,const mb_pe_import *import,uint64_t *out) {
 struct binding *b=opaque;mb_pe_bind_result result=mb_win_workers_bind(b->worker,import,out);
 return result==MB_PE_BIND_UNRESOLVED?mb_win_tls_bridge_bind(b->tls,import,out):result;
}
int main(int argc,char **argv) {
 CHECK(argc==3);int red=!strcmp(argv[2],"red-no-admission"),early=!strcmp(argv[2],"close-early");
 FILE *file=fopen(argv[1],"rb");CHECK(file);CHECK(fseek(file,0,SEEK_END)==0);long size=ftell(file);CHECK(size>0&&size<1048576&&fseek(file,0,SEEK_SET)==0);
 unsigned char *bytes=malloc((size_t)size);CHECK(bytes);CHECK(fread(bytes,1,(size_t)size,file)==(size_t)size&&fclose(file)==0);
 long page=sysconf(_SC_PAGESIZE);CHECK(page>0&&page<=16384&&16384%page==0);
 mb_win_thread_domain domain={0};int native=0;CHECK(mb_win_threads_open(&domain,&native)==MB_WIN_THREADS_OK);CHECK(mb_win_threads_enter(&domain,&native)==MB_WIN_THREADS_OK);
 mb_pe_module module={0};mb_pe_module_error error={0};struct memory memory={0};
 mb_pe_memory_ops memory_ops={&memory,16384,allocate_rw,protect_image,flush_image,release_image};
 struct code_context code={.module=&module};CHECK(pthread_mutex_init(&code.mutex,NULL)==0);
 mb_win_workers workers={0};mb_win_worker_config config={&domain,&code,acquire_worker_code,release_worker_code,8};
 CHECK(mb_win_workers_open(&workers,&config,&native)==MB_WIN_WORKERS_OK);
 mb_win_worker_session worker={0};mb_win_tls_bridge_session tls={0};
 CHECK(mb_win_workers_begin(&worker,&workers,&module)==MB_WIN_WORKERS_OK);CHECK(mb_win_tls_bridge_begin(&tls,&domain,&module)==MB_WIN_TLS_BRIDGE_OK);
 struct binding binding={&worker,&tls};CHECK(mb_pe_module_open(&module,bytes,(size_t)size,&memory_ops,bind_import,&binding,&error)==MB_PE_MODULE_OK);CHECK(module.report.imports_bound==7);
 mb_pe_export_lease begin={0},collect={0};CHECK(mb_pe_module_acquire_export(&module,(mb_pe_string){"WorkerBegin",11},&begin,&error)==MB_PE_MODULE_OK);code.begin=begin.address;
 CHECK(mb_pe_module_acquire_export(&module,(mb_pe_string){"WorkerCollect",13},&collect,&error)==MB_PE_MODULE_OK);code.collect=collect.address;CHECK(mb_pe_module_release(&collect)==MB_PE_MODULE_OK);
 uint64_t started[4]={0},result[4]={0};
 if(!red)CHECK(mb_win_workers_enter_call(&workers)==MB_WIN_WORKERS_OK);
 int32_t begin_result=mb_owned_call_exercise(begin.address,started);
 if(!red)CHECK(mb_win_workers_leave_call(&workers)==MB_WIN_WORKERS_OK);
 CHECK(mb_pe_module_release(&begin)==MB_PE_MODULE_OK);
 if(red){
  CHECK(begin_result==-2&&started[0]==0&&code.acquired==0);CHECK(mb_win_workers_collect(&workers,&native)==MB_WIN_WORKERS_OK);
  CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK);CHECK(mb_win_workers_end(&worker)==MB_WIN_WORKERS_OK);CHECK(mb_win_tls_bridge_end(&tls)==MB_WIN_TLS_BRIDGE_OK);CHECK(mb_win_workers_close(&workers,&native)==MB_WIN_WORKERS_OK);CHECK(mb_win_threads_leave(&domain,&native)==MB_WIN_THREADS_OK);CHECK(mb_win_threads_close(&domain,&native)==MB_WIN_THREADS_OK);CHECK(pthread_mutex_destroy(&code.mutex)==0);free(bytes);
  puts("{\"expected_red\":true,\"owned_pe_export_executed\":true,\"begin_result\":-2,\"worker_started\":false,\"creation_admission_required\":true}");return 1;
 }
 CHECK(begin_result==1&&started[0]&&started[1]&&started[2]==258&&started[3]==259);
 CHECK(code.acquired==1&&code.released==0&&code.start!=code.begin&&code.start!=code.collect&&module.lease_count==1);
 if(early){uint32_t ignored=0;CHECK(mb_win_workers_exit_code(&workers,(void *)(uintptr_t)started[0],&ignored)==0);CHECK(*mb_win_threads_last_error(&domain)==6);}
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_BUSY);CHECK(mb_win_workers_close(&workers,&native)==MB_WIN_WORKERS_BUSY);
 CHECK(mb_win_workers_end(&worker)==MB_WIN_WORKERS_BUSY);
 CHECK(mb_pe_module_acquire_export(&module,(mb_pe_string){"WorkerCollect",13},&collect,&error)==MB_PE_MODULE_OK);
 CHECK(mb_win_workers_enter_call(&workers)==MB_WIN_WORKERS_OK);int32_t collect_result=mb_owned_call_exercise(collect.address,result);CHECK(mb_win_workers_leave_call(&workers)==MB_WIN_WORKERS_OK);CHECK(mb_pe_module_release(&collect)==MB_PE_MODULE_OK);
 CHECK(collect_result==1&&result[0]==(early?0:MB_WORKER_EXPECTED_EXIT)&&result[1]==MB_WORKER_PARAMETER_COOKIE&&result[2]==MB_WORKER_LAST_ERROR&&result[3]==(early?UINT32_MAX:0));
 CHECK(module.lease_count==1&&code.released==0);code.fail_release=1;
 mb_win_worker_status retirement=MB_WIN_WORKERS_OK;
 for(unsigned retry=0;retry<1000;++retry){retirement=mb_win_workers_collect(&workers,&native);if(retirement==MB_WIN_WORKERS_CODE_ERROR)break;CHECK(retirement==MB_WIN_WORKERS_OK||retirement==MB_WIN_WORKERS_BUSY);usleep(1000);}
 CHECK(retirement==MB_WIN_WORKERS_CODE_ERROR);CHECK(module.lease_count==1&&code.released==0);
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_BUSY);
 CHECK(mb_win_workers_collect(&workers,&native)==MB_WIN_WORKERS_OK);CHECK(code.released==1&&module.lease_count==0);
 memory.fail_release=1;CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_CLEANUP_REQUIRED);CHECK(mb_win_workers_end(&worker)==MB_WIN_WORKERS_BUSY);
 CHECK(mb_pe_module_close(&module,&error)==MB_PE_MODULE_OK);CHECK(mb_win_workers_end(&worker)==MB_WIN_WORKERS_OK);CHECK(mb_win_tls_bridge_end(&tls)==MB_WIN_TLS_BRIDGE_OK);
 CHECK(mb_win_workers_close(&workers,&native)==MB_WIN_WORKERS_OK);CHECK(mb_win_threads_leave(&domain,&native)==MB_WIN_THREADS_OK);CHECK(mb_win_threads_close(&domain,&native)==MB_WIN_THREADS_OK);CHECK(pthread_mutex_destroy(&code.mutex)==0);
 CHECK(memory.releases==1&&code.acquired==1&&code.released==1);free(bytes);
 if(early){puts("{\"worker_started\":true,\"worker_returned\":true,\"handle_closed_while_running\":true,\"closed_handle_rejected\":true,\"code_pin_survived_handle_close\":true,\"code_lease_retired\":true,\"target_execution\":false}");return 0;}
 puts("{\"worker_started\":true,\"worker_returned\":true,\"internal_pe_start\":true,\"nonexported_code_lease\":true,\"module_busy_while_running\":true,\"high_bit_exit\":2166572391,\"timeout_and_repeated_wait\":true,\"last_error_isolation\":true,\"code_release_retry\":true,\"code_lease_retired\":true,\"memory_release_retry\":true,\"target_execution\":false}");return 0;
}
