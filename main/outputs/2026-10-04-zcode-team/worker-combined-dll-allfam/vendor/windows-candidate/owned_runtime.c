#include "owned_runtime.h"
#include "call_exports.h"
#include "owned_fixtures.h"
#include "owned_profile.h"
#include "../windows-services/mb_win_entry.h"
#include "../windows-services/mb_win_loader_bridge.h"
#if MB_OWNED_CLOCK
#include "../windows-services/mb_win_clock.h"
#endif
#if MB_OWNED_WORKERS
#include "../windows-services/mb_win_workers_abi.h"
#include "call_workers.h"
#endif
#if MB_OWNED_SYNC
#include "../windows-services/mb_win_critical_abi.h"
#include "../windows-services/mb_win_address_wait_abi.h"
#include "../windows-services/mb_win_srw_abi.h"
#endif
#if MB_OWNED_INIT_ONCE
#include "../windows-services/mb_win_init_once_abi.h"
#endif
#if MB_OWNED_MUTEX
#include "../windows-services/mb_win_mutex_abi.h"
#endif
#if MB_OWNED_FILEIO
#include "../windows-services/mb_win_fileio_abi.h"
#ifndef MB_OWNED_FILEIO_ROOT
#error "owned_profile.h must define MB_OWNED_FILEIO_ROOT"
#endif
#ifdef MB_OWNED_FILEIO_ROOT_OVERRIDE
#define MB_OWNED_FILEIO_ROOT_EFFECTIVE MB_OWNED_FILEIO_ROOT_OVERRIDE
#else
#define MB_OWNED_FILEIO_ROOT_EFFECTIVE MB_OWNED_FILEIO_ROOT
#endif
#endif
#if MB_OWNED_CRT_STARTUP
#include "../windows-services/mb_win_crt_startup_abi.h"
#endif
#if MB_OWNED_MODULE_ENUM
#include "../windows-services/mb_win_module_enum_abi.h"
#endif
#if MB_OWNED_VM
#include "../windows-services/mb_win_vm_abi.h"
#endif
#include <pthread.h>
#include <stdlib.h>
/* +10 fileio and +17 combined-family imports fit every tier: the full entry
 * stack 16+5+21+2+5+10+7+5+5=76 keeps headroom under the 96 sync cap. */
#define IMPORT_CAPACITY (MB_OWNED_SYNC?(MB_OWNED_FILEIO?96:64):MB_OWNED_WORKERS?(MB_OWNED_FILEIO?40:26):MB_OWNED_CLOCK?18:16)
#define ENTRY __attribute__((force_align_arg_pointer))
struct source {const void *bytes;size_t size;unsigned index,active;};
struct service {mb_win_heap_bridge_session heap;mb_win_tls_bridge_session tls;mb_win_loader_bridge_session loader;
#if MB_OWNED_CLOCK
 mb_win_clock_session clock;
#endif
#if MB_OWNED_WORKERS
 mb_win_worker_session worker;
#endif
#if MB_OWNED_SYNC
 mb_win_critical_session critical;
 mb_win_address_wait_session address_wait;
 mb_win_srw_session srw;
#endif
#if MB_OWNED_INIT_ONCE
 mb_win_init_once_session init_once;
#endif
#if MB_OWNED_MUTEX
 mb_win_mutex_session mutex;
#endif
#if MB_OWNED_FILEIO
 mb_win_fileio_session fileio;
#endif
#if MB_OWNED_CRT_STARTUP
 mb_win_crt_startup_session crt_startup;
#endif
#if MB_OWNED_MODULE_ENUM
 mb_win_module_enum_session module_enum;
#endif
#if MB_OWNED_VM
 mb_win_vm_abi_session vm;
#endif
 mb_win_runtime_bind_context runtime;mb_win_loader_runtime_context complete;unsigned active;};
static struct {
 mb_win_thread_domain domain;mb_win_heap heap;mb_pe_loader loader;
#if MB_OWNED_WORKERS
 mb_win_workers workers;unsigned workers_live,worker_call_live;
#endif
#if MB_OWNED_SYNC
 mb_win_critical critical;mb_win_address_wait address_wait;mb_win_srw srw;
 unsigned critical_live,address_wait_live,srw_live;
#endif
#if MB_OWNED_INIT_ONCE
 mb_win_init_once init_once;unsigned init_once_live;
#endif
#if MB_OWNED_MUTEX
 mb_win_mutex mutex_owner;unsigned mutex_owner_live;
#endif
#if MB_OWNED_FILEIO
 mb_win_fileio fileio_owner;unsigned fileio_owner_live;
#endif
#if MB_OWNED_CRT_STARTUP
 /* No native owner object: the CRT startup state is internal to
  * mb_win_crt_startup.c; sessions only pin the bound module. */
#endif
#if MB_OWNED_MODULE_ENUM
 mb_win_module_enum module_enum_owner;
 unsigned module_enum_owner_live,module_enum_probe_registered;
#endif
#if MB_OWNED_VM
 mb_win_vm vm_owner;unsigned vm_owner_live;
#endif
 pthread_mutex_t mutex;pthread_t entered_thread;unsigned entered,mutex_live,domain_live,heap_live,loader_live;
 mb_pe_loader_record records[2];mb_pe_export_lease imports[2][IMPORT_CAPACITY],procedures[2][8];
 mb_pe_loader_catalog_entry catalog[2];struct source sources[2];struct service services[2];
 mb_pe_entry_ops entries[2];
 mb_owned_dll_report report;
} owned;
static void lock(void *unused){(void)unused;if(pthread_mutex_lock(&owned.mutex))abort();}
static void unlock(void *unused){(void)unused;if(pthread_mutex_unlock(&owned.mutex))abort();}
#if MB_OWNED_MODULE_ENUM
/* Static probe record backing the module-enum fixture hook: one export whose
 * address is the owner itself, a deterministic base/entry point borrowed from
 * owned storage, and image size 4096 (fixture asserts all three). */
static const mb_win_module_export mb_module_enum_probe_exports[1]={
 {"MbProbe",(void *)&owned.module_enum_owner}};
#endif
#if MB_OWNED_VM
/* Virtual-memory backend: the SDK libc allocator on the same native route the
 * owned runtime itself uses; no host-enforced page protection (the family's
 * documented emulation contract). */
static void *mb_vm_allocate(void *context,size_t bytes){(void)context;return malloc(bytes);}
static void mb_vm_release(void *context,void *page,size_t bytes){(void)context;(void)bytes;free(page);}
#endif
#if MB_OWNED_WORKERS && MB_OWNED_MUTEX
/* Combined CloseHandle composition (worker-mutex-deadlock fix): the workers
 * family owns thread handles; any other live token is offered to the mutex
 * owner before failing. Without this, fixture mutex tokens can never retire
 * and the driver's cleanup retry loop blocks forever (worker-mutex-deadlock). */
__attribute__((ms_abi)) static int32_t combined_close_handle(void *handle){
 if(mb_win_workers_close_handle(&owned.workers,handle)==1)return 1;
 return mb_win_mutex_close_handle(&owned.mutex_owner,(uint64_t)(uintptr_t)handle);
}
#endif
#if MB_OWNED_WORKERS
static int acquire_worker_code(void *unused,void *address,void **token){
 (void)unused;mb_pe_export_lease *lease=calloc(1,sizeof(*lease));if(!lease)return -1;
 mb_pe_module_status status=MB_PE_MODULE_NOT_FOUND;mb_pe_module_error error={0};
 lock(NULL);
 for(unsigned i=0;i<2;++i){
  mb_pe_module *module=&owned.records[i].link.module;
  if(module->state==MB_PE_MODULE_ACTIVE && mb_pe_module_acquire_code(module,address,lease,&error)==MB_PE_MODULE_OK){status=MB_PE_MODULE_OK;break;}
 }
 unlock(NULL);
 if(status!=MB_PE_MODULE_OK){free(lease);return -1;}*token=lease;return 0;
}
static int release_worker_code(void *unused,void *token){
 (void)unused;mb_pe_export_lease *lease=token;
 lock(NULL);mb_pe_module_status status=mb_pe_module_release(lease);unlock(NULL);
 if(status!=MB_PE_MODULE_OK)return -1;free(lease);return 0;
}
static int worker_call_begin(void){
 if(owned.worker_call_live || mb_win_workers_enter_call(&owned.workers)!=MB_WIN_WORKERS_OK)return -1;
 owned.worker_call_live=1;return 0;
}
static int worker_call_end(void){
 if(!owned.worker_call_live)return 0;
 if(mb_win_workers_leave_call(&owned.workers)!=MB_WIN_WORKERS_OK)return -1;
 owned.worker_call_live=0;return 0;
}
#endif
static int fail(unsigned stage,unsigned status){
 if(!owned.report.failure_stage){owned.report.failure_stage=stage;owned.report.native_status=status;
  if(owned.domain_live && mb_win_threads_errno(&owned.domain))owned.report.windows_error=*mb_win_threads_last_error(&owned.domain);}
 return -1;
}
static int fail_loader(unsigned stage,mb_pe_loader_status status,const mb_pe_loader_error *error){
 if(!owned.report.failure_stage)owned.report.first_module_cause=error->module.cause;
 return fail(stage,(unsigned)status);
}
#if MB_OWNED_ENTRY
static int32_t entry_invoke(void *context,void *address,void *image,uint32_t reason,void *reserved){
 struct source *source=context;unsigned i=source->index;mb_pe_module *module=&owned.records[i].link.module;
 if(i>=2 || reason>1 || reserved || !owned.entered || !pthread_equal(pthread_self(),owned.entered_thread) ||
    !mb_win_threads_errno(&owned.domain) || !owned.services[i].active || image!=module->image ||
    module->state!=(reason?MB_PE_MODULE_INITIALIZING:MB_PE_MODULE_STOPPING)){
  ++owned.report.entry_contract_errors;(void)fail(20,1);return 0;
 }
#if MB_OWNED_CLOCK
 if(!owned.services[i].clock.active){++owned.report.entry_contract_errors;(void)fail(20,2);return 0;}
#endif
 if(reason)++owned.report.entry_attach_calls[i];else ++owned.report.entry_detach_calls[i];
#if MB_OWNED_WORKERS
 uint32_t admission=mb_win_workers_suspend_calls(&owned.workers);
#endif
 int32_t result=mb_win_call_entry(NULL,address,image,reason,reserved);
#if MB_OWNED_WORKERS
 mb_win_workers_restore_calls(&owned.workers,admission);
#endif
 if(reason)owned.report.entry_attach_result[i]=result;
 else {owned.report.entry_detach_result[i]=result;if(result){++owned.report.entry_contract_errors;(void)fail(20,(unsigned)result);}}
 return result;
}
#endif
static int acquire(void *unused,const mb_pe_loader_catalog_entry *entry,const void **file,size_t *size,void **token){
 (void)unused;struct source *s=entry->source_context;if(s->active)return -1;
 s->active=1;*file=s->bytes;*size=s->size;*token=s;++owned.report.source_acquires[s->index];return 0;
}
static int release(void *unused,void *token){
 (void)unused;struct source *s=token;if(!s->active)return -1;s->active=0;++owned.report.source_releases[s->index];return 0;
}
#if MB_OWNED_CLOCK || MB_OWNED_WORKERS
static mb_pe_bind_result service_bind(void *context,const mb_pe_import *import,uint64_t *address){
 struct service *s=context;mb_pe_bind_result result=MB_PE_BIND_UNRESOLVED;
#if MB_OWNED_SYNC
 result=mb_win_critical_bind(&s->critical,import,address);if(result!=MB_PE_BIND_UNRESOLVED)return result;
 result=mb_win_address_wait_bind(&s->address_wait,import,address);if(result!=MB_PE_BIND_UNRESOLVED)return result;
 result=mb_win_srw_bind(&s->srw,import,address);if(result!=MB_PE_BIND_UNRESOLVED)return result;
#endif
#if MB_OWNED_INIT_ONCE
 result=mb_win_init_once_bind(&s->init_once,import,address);if(result!=MB_PE_BIND_UNRESOLVED)return result;
#endif
#if MB_OWNED_WORKERS && MB_OWNED_MUTEX
 if(import->name.size==11 && !import->by_ordinal && import->name.data){
  static const char close_name[11]={'C','l','o','s','e','H','a','n','d','l','e'};
  unsigned equal=1;for(unsigned i=0;i<11;++i)if((char)import->name.data[i]!=close_name[i])equal=0;
  if(equal){*address=(uint64_t)(uintptr_t)&combined_close_handle;return MB_PE_BIND_BOUND;}
 }
#endif
#if MB_OWNED_WORKERS
 result=mb_win_workers_bind(&s->worker,import,address);
 if(result!=MB_PE_BIND_UNRESOLVED)return result;
#endif
#if MB_OWNED_MUTEX
 /* Workers claim the shared WaitForSingleObject for thread handles first; the
  * mutex family's Ex route and unique names bind here. A combined is_handle
  * dispatch remains a documented future composition, not guessed here. */
 result=mb_win_mutex_bind(&s->mutex,import,address);if(result!=MB_PE_BIND_UNRESOLVED)return result;
#endif
#if MB_OWNED_FILEIO
 /* The file I/O family's twenty KERNEL32 names intersect no other family
  * (verified: no wait, close or error-name overlap), so this position in
  * the shared-name bind chain cannot change any existing resolution. */
 result=mb_win_fileio_bind(&s->fileio,import,address);if(result!=MB_PE_BIND_UNRESOLVED)return result;
#endif
#if MB_OWNED_CLOCK
 result=mb_win_clock_bind(&s->clock,import,address);if(result!=MB_PE_BIND_UNRESOLVED)return result;
#endif
 result=mb_win_loader_runtime_bind(&s->complete,import,address);if(result!=MB_PE_BIND_UNRESOLVED)return result;
#if MB_OWNED_CRT_STARTUP || MB_OWNED_MODULE_ENUM || MB_OWNED_VM
 /* The combined families bind after the loader bridge: the census KERNEL32
  * module/loader names the retained-image machinery relies on (GetModuleHandle,
  * GetProcAddress, LoadLibrary, FreeLibrary) keep their pe_loader-backed
  * resolutions unchanged, and the new families win only the names no earlier
  * family claims (K32*, GetModuleFileName/HandleEx, Virtual*, crt-runtime). */
#endif
#if MB_OWNED_CRT_STARTUP
 /* Matches only api-ms-win-crt-runtime-l1-1-0.dll, ucrtbase.dll and
  * vcruntime140.dll: no KERNEL32 name overlap at all. */
 result=mb_win_crt_startup_bind(&s->crt_startup,import,address);if(result!=MB_PE_BIND_UNRESOLVED)return result;
#endif
#if MB_OWNED_MODULE_ENUM
 /* Of the fourteen census KERNEL32 names, five are unclaimed elsewhere:
  * GetModuleHandleExW, GetModuleFileNameA/W and the three K32 PSAPI names. */
 result=mb_win_module_enum_bind(&s->module_enum,import,address);if(result!=MB_PE_BIND_UNRESOLVED)return result;
#endif
#if MB_OWNED_VM
 /* Five census KERNEL32 names, unclaimed elsewhere. */
 result=mb_win_vm_abi_bind(&s->vm,import,address);if(result!=MB_PE_BIND_UNRESOLVED)return result;
#endif
 return result;
}
#endif
static int service_end(void *unused,void *token){
 (void)unused;struct service *s=token;
#if MB_OWNED_VM
 if(s->vm.active && mb_win_vm_abi_end_session(&s->vm)!=MB_WIN_VM_ABI_OK)return -1;
#endif
#if MB_OWNED_MODULE_ENUM
 if(s->module_enum.active && mb_win_module_enum_abi_end(&s->module_enum)!=MB_WIN_MODULE_ENUM_OK)return -1;
#endif
#if MB_OWNED_CRT_STARTUP
 if(s->crt_startup.active && mb_win_crt_startup_end_session(&s->crt_startup)!=MB_WIN_CRT_STARTUP_OK)return -1;
#endif
#if MB_OWNED_FILEIO
 if(s->fileio.active && mb_win_fileio_end_session(&s->fileio)!=MB_WIN_FILEIO_OK)return -1;
#endif
#if MB_OWNED_MUTEX
 if(s->mutex.active && mb_win_mutex_end_session(&s->mutex)!=MB_WIN_MUTEX_OK)return -1;
#endif
#if MB_OWNED_INIT_ONCE
 if(s->init_once.active && mb_win_init_once_end_session(&s->init_once)!=MB_WIN_INIT_ONCE_OK)return -1;
#endif
#if MB_OWNED_SYNC
 if(s->srw.active && mb_win_srw_end(&s->srw)!=MB_WIN_SRW_OK)return -1;
 if(s->address_wait.active && mb_win_address_wait_end(&s->address_wait)!=MB_WIN_ADDRESS_WAIT_OK)return -1;
 if(s->critical.active && mb_win_critical_end(&s->critical)!=MB_WIN_CRITICAL_OK)return -1;
#endif
#if MB_OWNED_WORKERS
 if(s->worker.active && mb_win_workers_end(&s->worker)!=MB_WIN_WORKERS_OK)return -1;
#endif
#if MB_OWNED_CLOCK
 if(s->clock.active){
  if(mb_win_clock_end(&s->clock)!=MB_WIN_CLOCK_OK)return -1;
  ++owned.report.clock_session_ends[s-owned.services];
 }
#endif
 if(s->loader.active && mb_win_loader_bridge_end(&s->loader)!=MB_WIN_LOADER_BRIDGE_OK)return -1;
 if(s->tls.active && mb_win_tls_bridge_end(&s->tls)!=MB_WIN_TLS_BRIDGE_OK)return -1;
 if(s->heap.active && mb_win_heap_bridge_end(&s->heap)!=MB_WIN_HEAP_BRIDGE_OK)return -1;
 s->active=0;return 0;
}
static int service_begin(void *unused,const mb_pe_loader_catalog_entry *entry,mb_pe_module *module,
 mb_pe_binder *binder,void **context,void **token){
 (void)unused;size_t i=(size_t)(entry-owned.catalog);if(i>=2)return -1;struct service *s=&owned.services[i];
 mb_win_heap_bridge_config heap={&owned.heap,&owned.domain,mb_win_threads_lock,mb_win_threads_unlock};
 mb_win_loader_bridge_config loader={&owned.loader,&owned.domain,NULL,lock,unlock};
 if(mb_win_heap_bridge_begin(&s->heap,&heap,module)!=MB_WIN_HEAP_BRIDGE_OK)return -1;
 if(mb_win_tls_bridge_begin(&s->tls,&owned.domain,module)!=MB_WIN_TLS_BRIDGE_OK){if(service_end(NULL,s))abort();return -1;}
 if(mb_win_loader_bridge_begin(&s->loader,&loader,module)!=MB_WIN_LOADER_BRIDGE_OK){if(service_end(NULL,s))abort();return -1;}
 s->runtime=(mb_win_runtime_bind_context){&s->tls,&s->heap};s->complete=(mb_win_loader_runtime_context){&s->loader,&s->runtime};
#if MB_OWNED_CLOCK
 if(mb_win_clock_begin(&s->clock,&owned.domain,module)!=MB_WIN_CLOCK_OK){if(service_end(NULL,s))abort();return -1;}
 ++owned.report.clock_session_begins[i];
#endif
#if MB_OWNED_WORKERS
 if(mb_win_workers_begin(&s->worker,&owned.workers,module)!=MB_WIN_WORKERS_OK){if(service_end(NULL,s))abort();return -1;}
#endif
#if MB_OWNED_SYNC
 if(mb_win_critical_begin(&s->critical,&owned.critical,module)!=MB_WIN_CRITICAL_OK){if(service_end(NULL,s))abort();return -1;}
 if(mb_win_address_wait_begin(&s->address_wait,&owned.address_wait,module)!=MB_WIN_ADDRESS_WAIT_OK){if(service_end(NULL,s))abort();return -1;}
 if(mb_win_srw_begin(&s->srw,&owned.srw,module)!=MB_WIN_SRW_OK){if(service_end(NULL,s))abort();return -1;}
#endif
#if MB_OWNED_INIT_ONCE
 if(mb_win_init_once_begin_session(&s->init_once,&owned.init_once,module)!=MB_WIN_INIT_ONCE_OK){if(service_end(NULL,s))abort();return -1;}
#endif
#if MB_OWNED_MUTEX
 if(mb_win_mutex_begin_session(&s->mutex,&owned.mutex_owner,module)!=MB_WIN_MUTEX_OK){if(service_end(NULL,s))abort();return -1;}
#endif
#if MB_OWNED_FILEIO
 if(mb_win_fileio_begin_session(&s->fileio,&owned.fileio_owner,module)!=MB_WIN_FILEIO_OK){if(service_end(NULL,s))abort();return -1;}
#endif
#if MB_OWNED_CRT_STARTUP
 /* Both fixture records may hold CRT startup sessions (independent modules). */
 if(mb_win_crt_startup_begin_session(&s->crt_startup,module)!=MB_WIN_CRT_STARTUP_OK){if(service_end(NULL,s))abort();return -1;}
#endif
#if MB_OWNED_MODULE_ENUM || MB_OWNED_VM
 if(i==0){
#endif
#if MB_OWNED_MODULE_ENUM
  /* The module-enum owner is single-pin: only the consumer record holds the
   * session (the helper imports none of these families). The static probe
   * record registers once, while the owner is active. */
  if(mb_win_module_enum_abi_begin(&s->module_enum,&owned.module_enum_owner,module)!=MB_WIN_MODULE_ENUM_OK){if(service_end(NULL,s))abort();return -1;}
  owned.module_enum_owner_live=1;
  if(!owned.module_enum_probe_registered){
   mb_win_module_record probe={0};
   {const char name[]="mbmodenum.dll";for(unsigned c=0;c<sizeof(name);++c){probe.name[c]=name[c];probe.file_name[c]=name[c];}}
   probe.base=(void *)&owned.module_enum_owner;probe.entry_point=(void *)&mb_owned_dll_result;
   probe.image_size=4096;probe.exports=mb_module_enum_probe_exports;probe.export_count=1;
   mb_win_module_enum_status ps=mb_win_module_enum_register(&owned.module_enum_owner,&probe);
   if(ps!=MB_WIN_MODULE_ENUM_OK){if(service_end(NULL,s))abort();return -1;}
   owned.module_enum_probe_registered=1;
  }
#endif
#if MB_OWNED_VM
  if(mb_win_vm_abi_begin_session(&s->vm,&owned.vm_owner,module)!=MB_WIN_VM_ABI_OK){if(service_end(NULL,s))abort();return -1;}
#endif
#if MB_OWNED_MODULE_ENUM || MB_OWNED_VM
 }
#endif
#if MB_OWNED_CLOCK || MB_OWNED_WORKERS
 *binder=service_bind;*context=s;
#else
 *binder=mb_win_loader_runtime_bind;*context=&s->complete;
#endif
 s->active=1;*token=s;return 0;
}
ENTRY int mb_owned_dll_open(const mb_pe_memory_ops *memory){
 if(!memory || owned.report.started)return -1;owned.report.started=1;owned.report.cleanup_pending=1;
 owned.report.stage=MB_OWNED_STAGE_INITIALIZING;
 owned.report.entry_enabled=MB_OWNED_ENTRY;owned.report.clock_enabled=MB_OWNED_CLOCK;
 owned.report.worker_enabled=MB_OWNED_WORKERS;
 owned.report.sync_enabled=MB_OWNED_SYNC;
 owned.report.init_once_enabled=MB_OWNED_INIT_ONCE;
 owned.report.mutex_enabled=MB_OWNED_MUTEX;
 owned.report.fileio_enabled=MB_OWNED_FILEIO;
 owned.report.crt_startup_enabled=MB_OWNED_CRT_STARTUP;
 owned.report.module_enum_enabled=MB_OWNED_MODULE_ENUM;
 owned.report.vm_enabled=MB_OWNED_VM;
 int native=0;
 int mutex_status=pthread_mutex_init(&owned.mutex,NULL);if(mutex_status)return fail(1,(unsigned)mutex_status);owned.mutex_live=1;
 mb_win_thread_status ts=mb_win_threads_open(&owned.domain,&native);
 if(owned.domain.state)owned.domain_live=1;if(ts!=MB_WIN_THREADS_OK)return fail(2,(unsigned)native);
 mb_win_heap_ops hops={&owned.domain,mb_win_threads_allocate,mb_win_threads_release,mb_win_threads_errno};
 mb_win_heap_status hs=mb_win_heap_open(&owned.heap,&hops);if(hs!=MB_WIN_HEAP_OK)return fail(3,hs);owned.heap_live=1;
#if MB_OWNED_WORKERS
 mb_win_worker_config workers={&owned.domain,NULL,acquire_worker_code,release_worker_code,8};
 mb_win_worker_status ws=mb_win_workers_open(&owned.workers,&workers,&native);
 if(owned.workers.state)owned.workers_live=1;if(ws!=MB_WIN_WORKERS_OK)return fail(21,(unsigned)(ws==MB_WIN_WORKERS_NATIVE_ERROR&&native?native:(int)ws));
#endif
#if MB_OWNED_SYNC
 mb_win_critical_status cs=mb_win_critical_open(&owned.critical,&owned.domain,16,&native);
 if(owned.critical.state)owned.critical_live=1;if(cs!=MB_WIN_CRITICAL_OK)return fail(31,(unsigned)(cs==MB_WIN_CRITICAL_NATIVE_ERROR&&native?native:(int)cs));
 mb_win_address_wait_status aw=mb_win_address_wait_open(&owned.address_wait,&owned.domain,&native);
 if(owned.address_wait.state)owned.address_wait_live=1;if(aw!=MB_WIN_ADDRESS_WAIT_OK)return fail(32,(unsigned)(aw==MB_WIN_ADDRESS_WAIT_NATIVE_ERROR&&native?native:(int)aw));
 mb_win_srw_config sc={&owned.domain,16};mb_win_srw_status sr=mb_win_srw_open(&owned.srw,&sc,&native);
 if(owned.srw.state)owned.srw_live=1;if(sr!=MB_WIN_SRW_OK)return fail(33,(unsigned)(sr==MB_WIN_SRW_NATIVE_ERROR&&native?native:(int)sr));
#endif
#if MB_OWNED_INIT_ONCE
 mb_win_init_once_config ioc={&owned.domain,16};mb_win_init_once_status io=mb_win_init_once_open(&owned.init_once,&ioc,&native);
 if(owned.init_once.state)owned.init_once_live=1;if(io!=MB_WIN_INIT_ONCE_OK)return fail(37,(unsigned)(io==MB_WIN_INIT_ONCE_NATIVE_ERROR&&native?native:(int)io));
#endif
#if MB_OWNED_MUTEX
 mb_win_mutex_config mc={&owned.domain,16};mb_win_mutex_status ms=mb_win_mutex_open(&owned.mutex_owner,&mc,&native);
 if(owned.mutex_owner.state)owned.mutex_owner_live=1;if(ms!=MB_WIN_MUTEX_OK)return fail(39,(unsigned)(ms==MB_WIN_MUTEX_NATIVE_ERROR&&native?native:(int)ms));
#endif
#if MB_OWNED_FILEIO
 /* Stage 41 (owner open) and 42 (owner close) are unique: the tree used 1-17,
  * 19-29 and 31-40 before this family. The root is the payload's dedicated
  * sandbox namespace (build passes a per-build host directory override for
  * host driver controls only; the native ELF keeps the production root). */
 mb_win_fileio_config fc={&owned.domain,MB_OWNED_FILEIO_ROOT_EFFECTIVE,16};mb_win_fileio_status fs=mb_win_fileio_open(&owned.fileio_owner,&fc,&native);
 if(owned.fileio_owner.state)owned.fileio_owner_live=1;if(fs!=MB_WIN_FILEIO_OK)return fail(41,(unsigned)(fs==MB_WIN_FILEIO_NATIVE_ERROR&&native?native:(int)fs));
#endif
#if MB_OWNED_VM
 /* Stage 53 (owner open) and 54 (owner close) are unique in this tree. The
  * backend rides the SDK libc allocator; the fixture never leaves a region
  * live, so the owner must close clean at cleanup. */
 mb_win_vm_ops vm_ops={0,mb_vm_allocate,mb_vm_release,0,0};
 mb_win_vm_status vs=mb_win_vm_open(&owned.vm_owner,&vm_ops,0);
 if(vs!=MB_WIN_VM_OK)return fail(53,(unsigned)vs);owned.vm_owner_live=1;
#endif
 owned.sources[0]=(struct source){mb_owned_consumer_bytes,sizeof(mb_owned_consumer_bytes),0,0};
 owned.sources[1]=(struct source){mb_owned_helper_bytes,sizeof(mb_owned_helper_bytes),1,0};
 owned.catalog[0]=(mb_pe_loader_catalog_entry){{"owned_consumer.dll",18},&owned.sources[0],NULL,owned.imports[0],IMPORT_CAPACITY,owned.procedures[0],8};
 owned.catalog[1]=(mb_pe_loader_catalog_entry){{"owned_helper.dll",16},&owned.sources[1],NULL,owned.imports[1],IMPORT_CAPACITY,owned.procedures[1],8};
#if MB_OWNED_ENTRY
 for(unsigned i=0;i<2;++i){owned.entries[i]=(mb_pe_entry_ops){&owned.sources[i],entry_invoke};owned.catalog[i].entry=&owned.entries[i];}
#endif
 mb_pe_loader_config cfg={.catalog=owned.catalog,.catalog_count=2,.records=owned.records,.memory=*memory,
  .source={NULL,acquire,release},.services={NULL,service_begin,service_end}};
 mb_pe_loader_error error={0};mb_pe_loader_status status=mb_pe_loader_open(&owned.loader,&cfg,&error);
 if(status!=MB_PE_LOADER_OK)return fail_loader(4,status,&error);owned.loader_live=1;owned.report.stage=MB_OWNED_STAGE_READY;return 0;
}
static int run_body(void){
 void *consumer=NULL,*exercise=NULL,*unload=NULL,*lookup=NULL;mb_pe_loader_error error={0};mb_pe_loader_status status;
 owned.report.stage=MB_OWNED_STAGE_LOAD_CONSUMER;
 lock(NULL);status=mb_pe_loader_load(&owned.loader,(mb_pe_string){"owned_consumer",14},&consumer,&error);unlock(NULL);
 if(status!=MB_PE_LOADER_OK)return fail_loader(5,status,&error);
 owned.report.consumer_imports=owned.records[0].link.module.report.imports_bound;
 if(owned.records[1].state!=MB_PE_LOADER_EMPTY || owned.report.source_acquires[1])return fail(6,0);
 owned.report.stage=MB_OWNED_STAGE_RESOLVE_EXPORTS;lock(NULL);
 status=mb_pe_loader_proc_name(&owned.loader,consumer,(mb_pe_string){"LoaderExercise",14},&exercise,&error);
 if(status==MB_PE_LOADER_OK)status=mb_pe_loader_proc_name(&owned.loader,consumer,(mb_pe_string){"LoaderUnload",12},&unload,&error);
 if(status==MB_PE_LOADER_OK)status=mb_pe_loader_proc_name(&owned.loader,consumer,(mb_pe_string){"LoaderLookup",12},&lookup,&error);
 unlock(NULL);if(status!=MB_PE_LOADER_OK)return fail_loader(7,status,&error);
 uint64_t values[4]={0};owned.report.stage=MB_OWNED_STAGE_EXERCISE;++owned.report.dll_calls_started;
#if MB_OWNED_WORKERS
 if(worker_call_begin())return fail(22,1);
#endif
 int result=mb_owned_call_exercise(exercise,values);++owned.report.dll_calls_returned;
#if MB_OWNED_WORKERS
 if(worker_call_end())return fail(22,2);
#endif
 owned.report.results[0]=values[2];owned.report.results[1]=values[3];
 owned.report.consumer_imports=owned.records[0].link.module.report.imports_bound;
 owned.report.helper_imports=owned.records[1].link.module.report.imports_bound;
 if(result)return fail(8,(unsigned)result);
 if(values[2]!=86 || values[3]!=93 || (void *)(uintptr_t)values[0]!=owned.records[1].link.module.image ||
    owned.records[1].explicit_refs!=1 || owned.records[1].procedure_count!=1 || owned.heap.live_count!=(MB_OWNED_ENTRY?2u:0u) ||
    owned.report.consumer_imports!=(MB_OWNED_CLOCK?18u:MB_OWNED_ENTRY?16u:10u)+(MB_OWNED_WORKERS?5u:0u)+(MB_OWNED_SYNC?21u:0u)+(MB_OWNED_INIT_ONCE?2u:0u)+(MB_OWNED_MUTEX?5u:0u)+(MB_OWNED_FILEIO?10u:0u)+(MB_OWNED_CRT_STARTUP?7u:0u)+((MB_OWNED_ENTRY&&MB_OWNED_MODULE_ENUM)?5u:0u)+((MB_OWNED_ENTRY&&MB_OWNED_VM)?5u:0u) || owned.report.helper_imports!=(MB_OWNED_CLOCK?10u:MB_OWNED_ENTRY?8u:6u))return fail(9,0);
#if MB_OWNED_WORKERS
 void *begin=NULL,*collect=NULL;lock(NULL);
 status=mb_pe_loader_proc_name(&owned.loader,consumer,(mb_pe_string){"WorkerBegin",11},&begin,&error);
 if(status==MB_PE_LOADER_OK)status=mb_pe_loader_proc_name(&owned.loader,consumer,(mb_pe_string){"WorkerCollect",13},&collect,&error);
 unlock(NULL);if(status!=MB_PE_LOADER_OK)return fail_loader(23,status,&error);
 if(worker_call_begin())return fail(24,1);++owned.report.worker_calls_started;
 result=mb_owned_call_worker(begin,owned.report.worker_begin);++owned.report.worker_calls_returned;
 if(result!=1){(void)fail(24,(unsigned)result);(void)worker_call_end();return -1;}
 /* A successful Begin leaves a gated asynchronous obligation. Keep creation
  * admission until Collect has opened the gate and retired the handle, so a
  * failed leave/second enter cannot strand that obligation. */
 ++owned.report.worker_calls_started;
 result=mb_owned_call_worker(collect,owned.report.worker_result);++owned.report.worker_calls_returned;
 if(result!=1){(void)fail(25,(unsigned)result);(void)worker_call_end();return -1;}
 if(worker_call_end())return fail(25,MB_WIN_WORKERS_NATIVE_ERROR);
 int worker_native=0;mb_win_worker_status ws=mb_win_workers_collect(&owned.workers,&worker_native);
 if(ws!=MB_WIN_WORKERS_OK)return fail(26,(unsigned)(ws==MB_WIN_WORKERS_NATIVE_ERROR&&worker_native?worker_native:(int)ws));
#endif
#if MB_OWNED_FILEIO
 /* Stages 43 (export resolve), 44 (first call window), 45 (native file-handle
  * retirement), 46 (second call window) are unique in this tree. The two
  * calls ride the existing worker admission route, so dll_calls_started /
  * dll_calls_returned (exercise/unload/lookup) stay byte-identical with
  * --fileio off and gain nothing else when it is on. */
 void *fileio_export=NULL;lock(NULL);
 status=mb_pe_loader_proc_name(&owned.loader,consumer,(mb_pe_string){"WorkerFileIo",12},&fileio_export,&error);
 unlock(NULL);if(status!=MB_PE_LOADER_OK)return fail_loader(43,status,&error);
 uint64_t fileio_out[4]={0};
 if(worker_call_begin())return fail(44,1);++owned.report.worker_calls_started;
 result=mb_owned_call_worker(fileio_export,fileio_out);++owned.report.worker_calls_returned;
 /* The fixture cannot close file handles itself (the workers family binds the
  * CloseHandle import for thread handles); it returns both open handles and
  * this runner retires them through the documented delegation service. */
 if(result!=1 || fileio_out[2]!=1 || fileio_out[3]!=UINT64_C(0x46494f31)){(void)worker_call_end();return fail(44,(unsigned)(result?result:1));}
 if(!mb_win_fileio_close_handle(&owned.fileio_owner,(void *)(uintptr_t)fileio_out[0]) ||
    !mb_win_fileio_close_handle(&owned.fileio_owner,(void *)(uintptr_t)fileio_out[1])){(void)worker_call_end();return fail(45,6);}
 if(worker_call_end())return fail(44,2);
 if(worker_call_begin())return fail(46,1);++owned.report.worker_calls_started;
 result=mb_owned_call_worker(fileio_export,fileio_out);++owned.report.worker_calls_returned;
 if(result!=1 || fileio_out[0]!=1 || fileio_out[3]!=UINT64_C(0x46494f32)){(void)worker_call_end();return fail(46,(unsigned)(result?result:1));}
 if(worker_call_end())return fail(46,2);
#endif
 owned.report.stage=MB_OWNED_STAGE_UNLOAD;++owned.report.dll_calls_started;
 result=mb_owned_call_unload(unload,(void *)(uintptr_t)values[0]);++owned.report.dll_calls_returned;
 if(!result)return fail(10,0);
 owned.report.stage=MB_OWNED_STAGE_LOOKUP;++owned.report.dll_calls_started;
 void *found=mb_owned_call_lookup(lookup);++owned.report.dll_calls_returned;
 if(found || owned.report.source_releases[1]!=1)return fail(11,0);
 owned.report.completed=1;return 0;
}
ENTRY int mb_owned_dll_run(void){
 if(!owned.loader_live || owned.report.failure_stage || owned.report.run_attempted)return -1;
 owned.report.run_attempted=1;int native=0;
 owned.report.stage=MB_OWNED_STAGE_THREAD_ENTER;
 if(mb_win_threads_enter(&owned.domain,&native)!=MB_WIN_THREADS_OK)return fail(12,(unsigned)native);
 owned.entered=1;owned.entered_thread=pthread_self();
 int result=run_body();
#if !MB_OWNED_ENTRY
 if(mb_win_threads_leave(&owned.domain,&native)!=MB_WIN_THREADS_OK)return fail(13,(unsigned)native);
 owned.entered=0;
#endif
 if(!result)owned.report.stage=MB_OWNED_STAGE_RUN_RETURNED;return result;
}
ENTRY int mb_owned_dll_cleanup(void){
 if(!owned.report.started || !owned.report.cleanup_pending)return 0;
 owned.report.stage=MB_OWNED_STAGE_CLEANUP;
 if(owned.entered){
  if(!pthread_equal(pthread_self(),owned.entered_thread))return fail(19,0);
#if !MB_OWNED_ENTRY
  int native=0;if(mb_win_threads_leave(&owned.domain,&native)!=MB_WIN_THREADS_OK)return fail(13,(unsigned)native);
  owned.entered=0;
#endif
 }
 mb_pe_loader_error error={0};mb_pe_loader_status status=MB_PE_LOADER_OK;
#if MB_OWNED_WORKERS
 if(owned.worker_call_live && worker_call_end())return fail(29,MB_WIN_WORKERS_NATIVE_ERROR);
 if(owned.workers_live){int native=0;mb_win_worker_status ws=mb_win_workers_collect(&owned.workers,&native);
  if(ws!=MB_WIN_WORKERS_OK)return fail(27,(unsigned)(ws==MB_WIN_WORKERS_NATIVE_ERROR&&native?native:(int)ws));}
#endif
 if(owned.loader_live){
  lock(NULL);
  /* Exports never recur here. The entry profile may invoke detach once as
   * references retire; failed release retains that completed detach phase. */
  for(size_t i=0;i<2 && status==MB_PE_LOADER_OK;++i){
   while(owned.records[i].explicit_refs && status==MB_PE_LOADER_OK)
    status=mb_pe_loader_free(&owned.loader,owned.records[i].handle,&error);
  }
  if(status==MB_PE_LOADER_OK)status=mb_pe_loader_close(&owned.loader,&error);
  unlock(NULL);if(status!=MB_PE_LOADER_OK)return fail_loader(14,status,&error);owned.loader_live=0;
 }
#if MB_OWNED_WORKERS
 if(owned.workers_live){int native=0;mb_win_worker_status ws=mb_win_workers_close(&owned.workers,&native);
  if(ws!=MB_WIN_WORKERS_OK)return fail(28,(unsigned)(ws==MB_WIN_WORKERS_NATIVE_ERROR&&native?native:(int)ws));owned.workers_live=0;}
#endif
#if MB_OWNED_SYNC
 if(owned.srw_live){int native=0;mb_win_srw_status sr=mb_win_srw_close(&owned.srw,&native);
  if(sr!=MB_WIN_SRW_OK)return fail(34,(unsigned)(sr==MB_WIN_SRW_NATIVE_ERROR&&native?native:(int)sr));owned.srw_live=0;}
 if(owned.address_wait_live){int native=0;mb_win_address_wait_status aw=mb_win_address_wait_close(&owned.address_wait,&native);
  if(aw!=MB_WIN_ADDRESS_WAIT_OK)return fail(35,(unsigned)(aw==MB_WIN_ADDRESS_WAIT_NATIVE_ERROR&&native?native:(int)aw));owned.address_wait_live=0;}
 if(owned.critical_live){int native=0;mb_win_critical_status cs=mb_win_critical_close(&owned.critical,&native);
  if(cs!=MB_WIN_CRITICAL_OK)return fail(36,(unsigned)(cs==MB_WIN_CRITICAL_NATIVE_ERROR&&native?native:(int)cs));owned.critical_live=0;}
#endif
#if MB_OWNED_INIT_ONCE
 if(owned.init_once_live){int native=0;mb_win_init_once_status io=mb_win_init_once_close(&owned.init_once,&native);
  if(io!=MB_WIN_INIT_ONCE_OK)return fail(38,(unsigned)(io==MB_WIN_INIT_ONCE_NATIVE_ERROR&&native?native:(int)io));owned.init_once_live=0;}
#endif
#if MB_OWNED_VM
 if(owned.vm_owner_live){mb_win_vm_status vs=mb_win_vm_close(&owned.vm_owner);
  if(vs!=MB_WIN_VM_OK)return fail(54,(unsigned)vs);owned.vm_owner_live=0;}
#endif
#if MB_OWNED_MODULE_ENUM
 /* The module-enum owner has no close: static records persist by contract;
  * only this process-local flag retires. */
 owned.module_enum_owner_live=0;
#endif
#if MB_OWNED_FILEIO
 if(owned.fileio_owner_live){int native=0;mb_win_fileio_status fs=mb_win_fileio_close(&owned.fileio_owner,&native);
  if(fs!=MB_WIN_FILEIO_OK)return fail(42,(unsigned)(fs==MB_WIN_FILEIO_NATIVE_ERROR&&native?native:(int)fs));owned.fileio_owner_live=0;}
#endif
#if MB_OWNED_MUTEX
 if(owned.mutex_owner_live){int native=0;mb_win_mutex_status ms=mb_win_mutex_close(&owned.mutex_owner,&native);
  if(ms!=MB_WIN_MUTEX_OK)return fail(40,(unsigned)(ms==MB_WIN_MUTEX_NATIVE_ERROR&&native?native:(int)ms));owned.mutex_owner_live=0;}
#endif
#if MB_OWNED_ENTRY
 /* Every possible DllMain detach has retired before thread-local services do.
  * Failed loader cleanup retains admission and retries only on this thread. */
 if(owned.entered){int native=0;if(mb_win_threads_leave(&owned.domain,&native)!=MB_WIN_THREADS_OK)return fail(13,(unsigned)native);owned.entered=0;}
#endif
 if(owned.heap_live){mb_win_heap_status hs=mb_win_heap_close(&owned.heap);if(hs!=MB_WIN_HEAP_OK)return fail(15,hs);owned.heap_live=0;}
 if(owned.domain_live){int native=0;mb_win_thread_status ts=mb_win_threads_close(&owned.domain,&native);
  if(ts!=MB_WIN_THREADS_OK)return fail(16,(unsigned)native);owned.domain_live=0;}
 if(owned.mutex_live){int rc=pthread_mutex_destroy(&owned.mutex);if(rc)return fail(17,(unsigned)rc);owned.mutex_live=0;}
 owned.report.cleanup_pending=0;owned.report.stage=MB_OWNED_STAGE_RETIRED;return 0;
}
ENTRY void mb_owned_dll_result(mb_owned_dll_report *out){if(out)*out=owned.report;}
