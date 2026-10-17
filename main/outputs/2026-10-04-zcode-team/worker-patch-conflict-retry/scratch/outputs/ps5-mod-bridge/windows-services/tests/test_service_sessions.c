#include "../mb_win_tls_bridge.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef void *(__attribute__((ms_abi)) *malloc_fn)(size_t);
typedef void (__attribute__((ms_abi)) *free_fn)(void *);
typedef void (__attribute__((ms_abi)) *set_error_fn)(uint32_t);
typedef uint32_t (__attribute__((ms_abi)) *get_error_fn)(void);
typedef int32_t (__attribute__((ms_abi)) *set_value_fn)(uint32_t,void *);
typedef void *(__attribute__((ms_abi)) *get_value_fn)(uint32_t);
struct work {mb_win_thread_domain *domain;uint32_t index;malloc_fn allocate;free_fn release;
 set_error_fn set_error;get_error_fn get_error;set_value_fn set_value;get_value_fn get_value;
 _Atomic unsigned calls;_Atomic int stop;int failure,marker;};
static void *worker(void *opaque) {
 struct work *w=opaque;int native=0;
 if(mb_win_threads_enter(w->domain,&native)!=MB_WIN_THREADS_OK)abort();
 while(!atomic_load_explicit(&w->stop,memory_order_acquire)) {
  unsigned char *p=w->allocate(17);if(!p)abort();p[0]=31;p[16]=63;
  w->set_error(UINT32_C(0xf1234567));
  if(!w->set_value(w->index,&w->marker) || w->get_error()!=UINT32_C(0xf1234567) ||
     w->get_value(w->index)!=&w->marker || w->get_error()!=0 || p[0]!=31 || p[16]!=63)w->failure=1;
  w->release(p);atomic_fetch_add_explicit(&w->calls,1,memory_order_release);
 }
 if(mb_win_threads_leave(w->domain,&native)!=MB_WIN_THREADS_OK)abort();return NULL;
}
static uint64_t bind(mb_win_runtime_bind_context *context,const char *library,size_t n,const char *name,size_t m) {
 mb_pe_import import={{library,n},{name,m},0,0,0,0};uint64_t value=0;
 if(mb_win_runtime_bind(context,&import,&value)!=MB_PE_BIND_BOUND)abort();return value;
}
int main(void) {
 mb_win_thread_domain domain={0};int native=0;
 CHECK(mb_win_threads_open(&domain,&native)==MB_WIN_THREADS_OK && mb_win_threads_enter(&domain,&native)==MB_WIN_THREADS_OK);
 mb_win_heap heap={0};mb_win_heap_ops ops={&domain,mb_win_threads_allocate,mb_win_threads_release,mb_win_threads_errno};
 CHECK(mb_win_heap_open(&heap,&ops)==MB_WIN_HEAP_OK);
 mb_win_heap_bridge_config config={&heap,&domain,mb_win_threads_lock,mb_win_threads_unlock};
 mb_pe_module primary={0},additional={0};mb_win_heap_bridge_session h={0};mb_win_tls_bridge_session t={0};
 CHECK(mb_win_heap_bridge_begin(&h,&config,&primary)==MB_WIN_HEAP_BRIDGE_OK);
 CHECK(mb_win_tls_bridge_begin(&t,&domain,&primary)==MB_WIN_TLS_BRIDGE_OK);
 primary.state=MB_PE_MODULE_ACTIVE;mb_win_runtime_bind_context context={&t,&h};
 struct work w={.domain=&domain};w.index=mb_win_threads_tls_alloc(&domain);CHECK(w.index!=UINT32_MAX);
 w.allocate=(malloc_fn)(uintptr_t)bind(&context,"api-ms-win-crt-heap-l1-1-0.dll",30,"malloc",6);
 w.release=(free_fn)(uintptr_t)bind(&context,"api-ms-win-crt-heap-l1-1-0.dll",30,"free",4);
 w.set_error=(set_error_fn)(uintptr_t)bind(&context,"KERNEL32.dll",12,"SetLastError",12);
 w.get_error=(get_error_fn)(uintptr_t)bind(&context,"KERNEL32.dll",12,"GetLastError",12);
 w.set_value=(set_value_fn)(uintptr_t)bind(&context,"KERNEL32.dll",12,"TlsSetValue",11);
 w.get_value=(get_value_fn)(uintptr_t)bind(&context,"KERNEL32.dll",12,"TlsGetValue",11);
 pthread_t thread;CHECK(pthread_create(&thread,NULL,worker,&w)==0);
 while(!atomic_load_explicit(&w.calls,memory_order_acquire))sched_yield();
 for(unsigned i=0;i<1000;++i) {
  mb_win_heap_bridge_session extra_heap={0};mb_win_tls_bridge_session extra_tls={0};
  CHECK(mb_win_heap_bridge_begin(&extra_heap,&config,&additional)==MB_WIN_HEAP_BRIDGE_OK);
  CHECK(mb_win_tls_bridge_begin(&extra_tls,&domain,&additional)==MB_WIN_TLS_BRIDGE_OK);
  CHECK(mb_win_tls_bridge_end(&extra_tls)==MB_WIN_TLS_BRIDGE_OK);
  CHECK(mb_win_heap_bridge_end(&extra_heap)==MB_WIN_HEAP_BRIDGE_OK);
 }
 atomic_store_explicit(&w.stop,1,memory_order_release);CHECK(pthread_join(thread,NULL)==0);
 CHECK(!w.failure && atomic_load(&w.calls)>0);
 primary.state=MB_PE_MODULE_EMPTY;
 CHECK(mb_win_tls_bridge_end(&t)==MB_WIN_TLS_BRIDGE_OK && mb_win_heap_bridge_end(&h)==MB_WIN_HEAP_BRIDGE_OK);
 CHECK(mb_win_heap_close(&heap)==MB_WIN_HEAP_OK);
 CHECK(mb_win_threads_tls_free(&domain,w.index)!=0);
 CHECK(mb_win_threads_leave(&domain,&native)==MB_WIN_THREADS_OK && mb_win_threads_close(&domain,&native)==MB_WIN_THREADS_OK);
 puts("Concurrent existing heap/TLS calls survived 1000 added-module session lifetimes");return 0;
}
