/* Executes only the two owned fixture DLLs, using the real file snapshot source. */
#include "../pe_file_source.h"
#include "../../windows-services/mb_win_loader_bridge.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#if !defined(__x86_64__)
#error "Execute as x86_64"
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
struct region {void *raw,*base;size_t raw_size,size;};
struct memory {struct region regions[4];void *fail_release;unsigned releases,failures;};
static struct region *find_region(struct memory *m,void *base){for(unsigned i=0;i<4;++i)if(m->regions[i].base==base)return &m->regions[i];return NULL;}
static void *allocate_rw(void *opaque,size_t size,size_t alignment){
 struct memory *m=opaque;struct region *r=NULL;for(unsigned i=0;i<4;++i)if(!m->regions[i].raw){r=&m->regions[i];break;}
 if(!r || size>SIZE_MAX-alignment)return NULL;size_t total=size+alignment;
 void *raw=mmap(NULL,total,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);if(raw==MAP_FAILED)return NULL;
 r->raw=raw;r->raw_size=total;r->size=size;r->base=(void *)(((uintptr_t)raw+alignment-1)&~(uintptr_t)(alignment-1));return r->base;
}
static int protect_image(void *opaque,void *base,size_t offset,size_t length,unsigned rights){
 struct region *r=find_region(opaque,base);if(!r || offset>r->size || length>r->size-offset || (rights&~7u) || (rights&6u)==6u)return -1;
 int prot=(rights&1?PROT_READ:0)|(rights&2?PROT_WRITE:0)|(rights&4?PROT_EXEC:0);return mprotect((char *)base+offset,length,prot);
}
static int flush_image(void *opaque,void *base,size_t length){struct region *r=find_region(opaque,base);if(!r || length!=r->size)return -1;__builtin___clear_cache((char *)base,(char *)base+length);return 0;}
static int release_image(void *opaque,void *base,size_t length){
 struct memory *m=opaque;struct region *r=find_region(m,base);if(!r || length!=r->size)return -1;
 if(m->fail_release==base){m->fail_release=NULL;++m->failures;return -1;}
 if(munmap(r->raw,r->raw_size))return -1;*r=(struct region){0};++m->releases;return 0;
}
struct service {
 mb_win_heap_bridge_session heap;mb_win_tls_bridge_session tls;mb_win_loader_bridge_session loader;
 mb_win_runtime_bind_context runtime;mb_win_loader_runtime_context complete;unsigned active;
};
struct environment {
 mb_pe_loader *owner;mb_pe_loader_catalog_entry *catalog;mb_win_thread_domain *domain;mb_win_heap *heap;
 pthread_mutex_t mutex;struct service services[2];void *source_tokens[2];
 unsigned acquired[2],released[2],begun[2],ended[2];
};
static void loader_lock(void *opaque){if(pthread_mutex_lock(&((struct environment *)opaque)->mutex))abort();}
static void loader_unlock(void *opaque){if(pthread_mutex_unlock(&((struct environment *)opaque)->mutex))abort();}
static int source_acquire(void *opaque,const mb_pe_loader_catalog_entry *entry,const void **file,size_t *size,void **token){
 struct environment *e=opaque;size_t i=(size_t)(entry-e->catalog);if(i>=2)return -1;
 int result=mb_pe_file_source_acquire(NULL,entry,file,size,token);
 if(!result){++e->acquired[i];e->source_tokens[i]=*token;*mb_win_threads_last_error(e->domain)=UINT32_C(0xdead3333);}return result;
}
static int source_release(void *opaque,void *token){
 struct environment *e=opaque;size_t i;for(i=0;i<2;++i)if(e->source_tokens[i]==token)break;if(i==2)return -1;
 int result=mb_pe_file_source_release(NULL,token);if(!result){++e->released[i];e->source_tokens[i]=NULL;*mb_win_threads_last_error(e->domain)=UINT32_C(0xdead4444);}return result;
}
static int service_end(void *opaque,void *token){
 struct environment *e=opaque;struct service *s=token;size_t i=(size_t)(s-e->services);if(i>=2 || !s->active)return -1;
 if(s->loader.active && mb_win_loader_bridge_end(&s->loader)!=MB_WIN_LOADER_BRIDGE_OK)return -1;
 if(s->tls.active && mb_win_tls_bridge_end(&s->tls)!=MB_WIN_TLS_BRIDGE_OK)return -1;
 if(s->heap.active && mb_win_heap_bridge_end(&s->heap)!=MB_WIN_HEAP_BRIDGE_OK)return -1;
 s->active=0;++e->ended[i];return 0;
}
static int service_begin(void *opaque,const mb_pe_loader_catalog_entry *entry,mb_pe_module *module,
                         mb_pe_binder *binder,void **context,void **token){
 struct environment *e=opaque;size_t i=(size_t)(entry-e->catalog);if(i>=2)return -1;struct service *s=&e->services[i];
 if(s->active)return -1;
 mb_win_heap_bridge_config heap={e->heap,e->domain,mb_win_threads_lock,mb_win_threads_unlock};
 mb_win_loader_bridge_config loader={e->owner,e->domain,e,loader_lock,loader_unlock};
 if(mb_win_heap_bridge_begin(&s->heap,&heap,module)!=MB_WIN_HEAP_BRIDGE_OK)return -1;
 if(mb_win_tls_bridge_begin(&s->tls,e->domain,module)!=MB_WIN_TLS_BRIDGE_OK){if(mb_win_heap_bridge_end(&s->heap)!=MB_WIN_HEAP_BRIDGE_OK)abort();return -1;}
 if(mb_win_loader_bridge_begin(&s->loader,&loader,module)!=MB_WIN_LOADER_BRIDGE_OK){
  if(mb_win_tls_bridge_end(&s->tls)!=MB_WIN_TLS_BRIDGE_OK || mb_win_heap_bridge_end(&s->heap)!=MB_WIN_HEAP_BRIDGE_OK)abort();return -1;
 }
 s->runtime=(mb_win_runtime_bind_context){&s->tls,&s->heap};s->complete=(mb_win_loader_runtime_context){&s->loader,&s->runtime};
 s->active=1;++e->begun[i];*binder=mb_win_loader_runtime_bind;*context=&s->complete;*token=s;return 0;
}
typedef int (__attribute__((ms_abi)) *exercise_fn)(uint64_t *);
typedef int (__attribute__((ms_abi)) *unload_fn)(void *);
typedef void *(__attribute__((ms_abi)) *lookup_fn)(void);
int main(int argc,char **argv){
 CHECK(argc==3);long page=sysconf(_SC_PAGESIZE);CHECK(page>0 && page<=16384 && 16384%page==0);
 mb_win_thread_domain domain={0};int native=0;CHECK(mb_win_threads_open(&domain,&native)==MB_WIN_THREADS_OK);
 CHECK(mb_win_threads_enter(&domain,&native)==MB_WIN_THREADS_OK);
 mb_win_heap heap={0};mb_win_heap_ops heap_ops={&domain,mb_win_threads_allocate,mb_win_threads_release,mb_win_threads_errno};
 CHECK(mb_win_heap_open(&heap,&heap_ops)==MB_WIN_HEAP_OK);
 mb_pe_file_source_profile profiles[2]={{argv[1],1048576},{argv[2],1048576}};
 mb_pe_export_lease imports[2][16]={0},procedures[2][8]={0};
 mb_pe_loader_catalog_entry catalog[2]={
  {{"owned_consumer.dll",18},&profiles[0],NULL,imports[0],16,procedures[0],8},
  {{"owned_helper.dll",16},&profiles[1],NULL,imports[1],16,procedures[1],8}};
 mb_pe_loader_record records[2]={0};mb_pe_loader owner={0};mb_pe_loader_error error={0};struct memory memory={0};
 struct environment environment={.owner=&owner,.catalog=catalog,.domain=&domain,.heap=&heap};
 CHECK(pthread_mutex_init(&environment.mutex,NULL)==0);
 mb_pe_loader_config config={.catalog=catalog,.catalog_count=2,.records=records,
  .memory={&memory,16384,allocate_rw,protect_image,flush_image,release_image},
  .source={&environment,source_acquire,source_release},.services={&environment,service_begin,service_end}};
 CHECK(mb_pe_loader_open(&owner,&config,&error)==MB_PE_LOADER_OK);
 void *consumer=NULL,*exercise=NULL,*unload=NULL,*lookup=NULL;
 loader_lock(&environment);
 mb_pe_loader_status status=mb_pe_loader_load(&owner,(mb_pe_string){"owned_consumer",14},&consumer,&error);
 loader_unlock(&environment);
 if(status!=MB_PE_LOADER_OK)fprintf(stderr,"consumer load status=%d cause=%d module=%d image=%d callback=%d\n",status,error.cause,error.module.code,error.module.image.code,error.callback_error);
 CHECK(status==MB_PE_LOADER_OK && consumer==records[0].link.module.image);
 CHECK(environment.acquired[0]==1 && environment.acquired[1]==0 && records[1].state==MB_PE_LOADER_EMPTY);
 CHECK(records[0].link.module.report.imports_bound==10 && owner.pin_count==1);
 loader_lock(&environment);
 CHECK(mb_pe_loader_proc_name(&owner,consumer,(mb_pe_string){"LoaderExercise",14},&exercise,&error)==MB_PE_LOADER_OK);
 CHECK(mb_pe_loader_proc_name(&owner,consumer,(mb_pe_string){"LoaderUnload",12},&unload,&error)==MB_PE_LOADER_OK);
 CHECK(mb_pe_loader_proc_name(&owner,consumer,(mb_pe_string){"LoaderLookup",12},&lookup,&error)==MB_PE_LOADER_OK);
 loader_unlock(&environment);
 uint64_t results[4]={0};int result=((exercise_fn)exercise)(results);
 if(result)fprintf(stderr,"LoaderExercise returned %d; last_error=%u\n",result,*mb_win_threads_last_error(&domain));
 CHECK(result==0 && results[2]==86 && results[3]==93);
 void *helper=(void *)(uintptr_t)results[0];
 CHECK(helper==records[1].link.module.image && records[1].explicit_refs==1 && records[1].procedure_count==1);
 CHECK(records[1].link.module.report.imports_bound==6 && heap.live_count==0 && heap.live_bytes==0);
 CHECK(environment.acquired[1]==1 && environment.released[1]==0 && owner.pin_count==2 && heap.pin_count==2);
 CHECK(environment.begun[0]==1 && environment.begun[1]==1);
 memory.fail_release=helper;
 CHECK(((unload_fn)unload)(helper)==0 && *mb_win_threads_last_error(&domain)==170);
 CHECK(records[1].state==MB_PE_LOADER_CLEANUP && environment.released[1]==0 && environment.ended[1]==0);
 CHECK(environment.services[1].loader.active && environment.services[1].tls.active && environment.services[1].heap.active);
 CHECK(owner.pin_count==2 && heap.pin_count==2 && memory.failures==1);
 CHECK(((unload_fn)unload)(helper)!=0);
 CHECK(records[1].state==MB_PE_LOADER_EMPTY && environment.released[1]==1 && environment.ended[1]==1 && owner.pin_count==1);
 CHECK(((lookup_fn)lookup)()==NULL && *mb_win_threads_last_error(&domain)==126);
 loader_lock(&environment);status=mb_pe_loader_free(&owner,consumer,&error);loader_unlock(&environment);
 CHECK(status==MB_PE_LOADER_OK && records[0].state==MB_PE_LOADER_EMPTY && owner.pin_count==0);
 CHECK(environment.released[0]==1 && environment.ended[0]==1 && memory.releases==2);
 CHECK(mb_pe_loader_close(&owner,&error)==MB_PE_LOADER_OK && mb_win_heap_close(&heap)==MB_WIN_HEAP_OK);
 CHECK(mb_win_threads_leave(&domain,&native)==MB_WIN_THREADS_OK && mb_win_threads_close(&domain,&native)==MB_WIN_THREADS_OK);
 CHECK(pthread_mutex_destroy(&environment.mutex)==0);
 puts("{\"owned_consumer_executed\":true,\"loader_imports_called\":8,\"helper_service_imports_called\":6,\"helper_acquired_from_disk\":true,\"named_and_ordinal_results\":[86,93],\"refcount_and_handle_semantics\":true,\"cleanup_retained_services\":true,\"all_unloaded\":true,\"console_contact\":false}");return 0;
}
