/* Executes only freshly compiled owned fixture DLLs from lazy embedded bytes. */
#include "owned_runtime.h"
#include "../windows-services/mb_win_threads.h"
#include <errno.h>

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
static unsigned fail_leave;
static unsigned fail_mutex;
int mb_test_mutex_init(pthread_mutex_t *mutex,const pthread_mutexattr_t *attributes){
 if(fail_mutex){--fail_mutex;return EAGAIN;}return pthread_mutex_init(mutex,attributes);
}
/* Only owned_runtime.c's call site is renamed for this host fault control. */
mb_win_thread_status mb_test_threads_leave(mb_win_thread_domain *domain,int *error){
 if(fail_leave){--fail_leave;if(error)*error=EAGAIN;return MB_WIN_THREADS_NATIVE_ERROR;}
 return mb_win_threads_leave(domain,error);
}
static void *wrong_thread_cleanup(void *out){*(int *)out=mb_owned_dll_cleanup();return NULL;}
int main(int argc,char **argv){
 CHECK(argc==1 || argc==2);int leave_fault=argc==2 && argv[1][0]=='l';struct memory memory={0};
 mb_pe_memory_ops ops={&memory,16384,allocate_rw,protect_image,flush_image,release_image};
 if(argc==2 && argv[1][0]=='m'){
  fail_mutex=1;CHECK(mb_owned_dll_open(&ops)!=0);mb_owned_dll_report early;mb_owned_dll_result(&early);
  CHECK(early.failure_stage==1 && early.native_status==EAGAIN && !early.run_attempted && !early.dll_calls_started);
  CHECK(!mb_owned_dll_cleanup());mb_owned_dll_result(&early);CHECK(!early.cleanup_pending && early.native_status==EAGAIN);
  puts("First native mutex-init error retained after cleanup without DLL entry");return 0;
 }
 CHECK(mb_owned_dll_open(&ops)==0);fail_leave=(unsigned)leave_fault;
 CHECK(mb_owned_dll_run()==(leave_fault?-1:0));
 mb_owned_dll_report report={0};mb_owned_dll_result(&report);
 CHECK(report.completed && report.cleanup_pending && report.run_attempted && report.results[0]==86 && report.results[1]==93);
 CHECK(report.dll_calls_started==3 && report.dll_calls_returned==3);
 CHECK(report.consumer_imports==10 && report.helper_imports==6 && report.source_acquires[0]==1 && report.source_acquires[1]==1);
 CHECK(report.source_releases[0]==0 && report.source_releases[1]==1);
 CHECK(mb_owned_dll_run()!=0);
 if(leave_fault){
  CHECK(report.failure_stage==13);pthread_t thread;int status=0;
  CHECK(pthread_create(&thread,NULL,wrong_thread_cleanup,&status)==0 && pthread_join(thread,NULL)==0 && status!=0);
  mb_owned_dll_result(&report);CHECK(report.cleanup_pending && report.source_releases[0]==0 && report.failure_stage==13);
 } else if(argc==2){
  for(unsigned i=0;i<4;++i)if(memory.regions[i].base)memory.fail_release=memory.regions[i].base;
  CHECK(memory.fail_release && mb_owned_dll_cleanup()!=0);
  mb_owned_dll_result(&report);CHECK(report.cleanup_pending && report.failure_stage==14 && memory.failures==1);
 }
 CHECK(mb_owned_dll_cleanup()==0);mb_owned_dll_result(&report);
 CHECK(!report.cleanup_pending && report.source_releases[0]==1 && report.source_releases[1]==1 && memory.releases==2);
 CHECK(report.failure_stage==(leave_fault?13u:argc==2?14u:0u));
 CHECK(mb_owned_dll_cleanup()==0);
 puts(leave_fault?"Owned candidate retried thread leave only on owning thread, retaining original failure":
      argc==2?"Owned candidate retained failed cleanup and original failure; retry passed":
      "Owned candidate86/93,10+6 imports, lazy embedded sources and cleanup passed");return 0;
}
