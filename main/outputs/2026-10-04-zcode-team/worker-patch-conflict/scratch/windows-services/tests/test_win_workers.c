#ifdef MB_WIN_WORKERS_LAYOUT
#include "../mb_win_workers_abi.h"
const unsigned long long mb_worker_layout[]={sizeof(void*),sizeof(size_t),sizeof(uint32_t),sizeof(mb_win_workers),sizeof(mb_win_worker_pin),sizeof(mb_win_worker_config),sizeof(mb_win_worker_session),offsetof(mb_win_worker_session,module),offsetof(mb_win_worker_session,active)};
#else
#include "../mb_win_workers_abi.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define WIN __attribute__((ms_abi))
static mb_win_workers owner;
static mb_win_thread_domain domain;
static atomic_uint gate,entered,acquired,released;
static int deny_acquire,deny_release;
#ifdef MB_WIN_WORKERS_FAULTS
static atomic_int fail_create,fail_enter;
int worker_test_create(pthread_t *thread,const pthread_attr_t *attrs,void *(*start)(void*),void *arg) {
    if(atomic_exchange(&fail_create,0))return EAGAIN;
    return pthread_create(thread,attrs,start,arg);
}
mb_win_thread_status worker_test_enter(mb_win_thread_domain *d,int *error) {
    if(atomic_exchange(&fail_enter,0)){if(error)*error=ENOMEM;return MB_WIN_THREADS_NATIVE_ERROR;}
    return mb_win_threads_enter(d,error);
}
#endif
static uint32_t expected=UINT32_C(0x81234567);
static WIN uint32_t worker(void *arg) {
    assert(arg==&expected);
    assert(mb_win_threads_errno(&domain));
    assert(*mb_win_threads_last_error(&domain)==0);
    *mb_win_threads_last_error(&domain)=0xaabbccdd;
    mb_win_workers_sleep(&owner,0);
    atomic_store(&entered,1);
    while(!atomic_load(&gate)) mb_win_workers_sleep(&owner,1);
    assert(*mb_win_threads_last_error(&domain)==0xaabbccdd);
    return *(uint32_t*)arg;
}
static int acquire(void *ctx,void *start,void **token) {
    assert(ctx==&domain && start==(void*)&worker);
    if(deny_acquire)return 1;
    *token=malloc(1);assert(*token);atomic_fetch_add(&acquired,1);return 0;
}
static int release(void *ctx,void *token) {
    assert(ctx==&domain && token);
    if(deny_release)return 1;
    free(token);atomic_fetch_add(&released,1);return 0;
}
static void *bind(mb_win_worker_session *s,const char *name) {
    mb_pe_import i={0};i.library=(mb_pe_string){"KeRnEl32.DlL",12};i.name=(mb_pe_string){name,strlen(name)};
    uint64_t address=0;assert(mb_win_workers_bind(s,&i,&address)==MB_PE_BIND_BOUND);return (void*)(uintptr_t)address;
}
static void drained(void) {
    for(unsigned i=0;i<2000;++i){
        mb_win_worker_status s=mb_win_workers_collect(&owner,NULL);
        if(s==MB_WIN_WORKERS_OK)return;
        assert(s==MB_WIN_WORKERS_BUSY);mb_win_workers_sleep(&owner,1);
    }
    assert(!"worker did not finish");
}
int main(void) {
    assert(mb_win_threads_open(&domain,NULL)==MB_WIN_THREADS_OK);
    assert(mb_win_threads_enter(&domain,NULL)==MB_WIN_THREADS_OK);
    mb_win_worker_config config={&domain,&domain,acquire,release,4};
    assert(mb_win_workers_open(&owner,&config,NULL)==MB_WIN_WORKERS_OK);
    mb_pe_module module={0};mb_win_worker_session session={0};
    assert(mb_win_workers_begin(&session,&owner,&module)==MB_WIN_WORKERS_OK);
    assert(mb_win_workers_close(&owner,NULL)==MB_WIN_WORKERS_BUSY);
    module.state=MB_PE_MODULE_ACTIVE;
    typedef void *(WIN *create_fn)(void*,size_t,void*,void*,uint32_t,uint32_t*);
    typedef uint32_t (WIN *wait_fn)(void*,uint32_t);
    typedef int32_t (WIN *exit_fn)(void*,uint32_t*);
    typedef int32_t (WIN *close_fn)(void*);
    typedef void (WIN *sleep_fn)(uint32_t);
    create_fn create=(create_fn)bind(&session,"CreateThread");
    wait_fn wait=(wait_fn)bind(&session,"WaitForSingleObject");
    exit_fn getexit=(exit_fn)bind(&session,"GetExitCodeThread");
    close_fn closehandle=(close_fn)bind(&session,"CloseHandle");
    sleep_fn sleep=(sleep_fn)bind(&session,"Sleep");
    uint32_t tid=77,result=0xa5;uint32_t *last=mb_win_threads_last_error(&domain);
    assert(!create(NULL,0,(void*)&worker,&expected,0,&tid) && tid==77 && *last==170);
    assert(mb_win_workers_enter_call(&owner)==MB_WIN_WORKERS_OK);
    assert(!create(NULL,0,(void*)&worker,&expected,UINT32_C(0x10000),&tid) && tid==77 && *last==50);
    assert(!create(&result,0,(void*)&worker,&expected,0,&tid) && *last==50);
    assert(!create(NULL,123,(void*)&worker,&expected,0,&tid) && *last==50);
    deny_acquire=1;assert(!create(NULL,0,(void*)&worker,&expected,0,&tid));deny_acquire=0;
    uint32_t depth=mb_win_workers_suspend_calls(&owner);assert(depth==1);
    assert(!create(NULL,0,(void*)&worker,&expected,0,&tid) && *last==170);
    mb_win_workers_restore_calls(&owner,depth);
    *last=0x12345678;
    void *handle=create(NULL,0,(void*)&worker,&expected,0,&tid);
    assert(handle && tid && tid!=77 && *last==0x12345678);
    assert(wait(handle,0)==MB_WIN_WAIT_TIMEOUT);
    assert(wait(handle,8)==MB_WIN_WAIT_TIMEOUT && *last==0x12345678);
    assert(getexit(handle,&result) && result==MB_WIN_STILL_ACTIVE);
    assert(atomic_load(&acquired)==1 && atomic_load(&released)==0);
    assert(mb_win_workers_collect(&owner,NULL)==MB_WIN_WORKERS_BUSY);
    atomic_store(&gate,1);
    assert(wait(handle,MB_WIN_INFINITE)==0);
    assert(wait(handle,0)==0 && getexit(handle,&result) && result==expected);
    assert(*last==0x12345678);
    deny_release=1;
    assert(mb_win_workers_collect(&owner,NULL)==MB_WIN_WORKERS_CODE_ERROR);
    assert(atomic_load(&released)==0);
    assert(closehandle(handle));
    assert(wait(handle,0)==MB_WIN_WAIT_FAILED && *last==6);
    assert(!closehandle(handle) && *last==6);
    deny_release=0;drained();assert(atomic_load(&released)==1);
    puts("worker ABI, result, waits and retained release failure passed");
    atomic_store(&gate,0);atomic_store(&entered,0);
    void *second=create(NULL,0,(void*)&worker,&expected,0,&tid);assert(second && second!=handle);
    assert(closehandle(second));
    assert(mb_win_workers_collect(&owner,NULL)==MB_WIN_WORKERS_BUSY);
    assert(atomic_load(&released)==1);
    atomic_store(&gate,1);drained();assert(atomic_load(&released)==2);
    assert(wait(second,0)==MB_WIN_WAIT_FAILED);
    puts("closed running handle retains worker/code lifetime passed");
    mb_pe_import wrong={0};wrong.library=(mb_pe_string){"kernelbase.dll",14};wrong.name=(mb_pe_string){"Sleep",5};uint64_t address=123;
    assert(mb_win_workers_bind(&session,&wrong,&address)==MB_PE_BIND_UNRESOLVED && address==123);
    wrong.library=(mb_pe_string){"kernel32.dll",12};wrong.name=(mb_pe_string){"sleep",5};
    assert(mb_win_workers_bind(&session,&wrong,&address)==MB_PE_BIND_UNRESOLVED && address==123);
    wrong.by_ordinal=1;assert(mb_win_workers_bind(&session,&wrong,&address)==MB_PE_BIND_UNRESOLVED);
    *last=99;sleep(0);sleep(1);assert(*last==99);
    assert(mb_win_workers_leave_call(&owner)==MB_WIN_WORKERS_OK);
    assert(mb_win_workers_end(&session)==MB_WIN_WORKERS_BUSY);module.state=MB_PE_MODULE_EMPTY;
    assert(mb_win_workers_end(&session)==MB_WIN_WORKERS_OK);
    assert(mb_win_workers_close(&owner,NULL)==MB_WIN_WORKERS_OK);
    assert(mb_win_threads_leave(&domain,NULL)==MB_WIN_THREADS_OK);
    assert(mb_win_threads_close(&domain,NULL)==MB_WIN_THREADS_OK);
    puts("worker binding, admission and clean owner retirement passed");
#ifdef MB_WIN_WORKERS_FAULTS
    assert(mb_win_threads_open(&domain,NULL)==MB_WIN_THREADS_OK);
    assert(mb_win_threads_enter(&domain,NULL)==MB_WIN_THREADS_OK);
    assert(mb_win_workers_open(&owner,&config,NULL)==MB_WIN_WORKERS_OK);
    assert(mb_win_workers_enter_call(&owner)==MB_WIN_WORKERS_OK);
    unsigned before=atomic_load(&acquired),was_released=atomic_load(&released);
    atomic_store(&fail_create,1);tid=456;
    assert(!mb_win_workers_create(&owner,NULL,0,(void*)&worker,&expected,0,&tid));
    assert(tid==456 && atomic_load(&acquired)==before+1 && atomic_load(&released)==was_released+1);
    atomic_store(&fail_enter,1);
    assert(!mb_win_workers_create(&owner,NULL,0,(void*)&worker,&expected,0,&tid));
    drained();assert(atomic_load(&acquired)==before+2 && atomic_load(&released)==was_released+2);
    deny_release=1;atomic_store(&fail_create,1);
    assert(!mb_win_workers_create(&owner,NULL,0,(void*)&worker,&expected,0,&tid));
    assert(mb_win_workers_collect(&owner,NULL)==MB_WIN_WORKERS_CODE_ERROR);
    assert(atomic_load(&released)==was_released+2);
    deny_release=0;drained();assert(atomic_load(&released)==was_released+3);
    assert(mb_win_workers_leave_call(&owner)==MB_WIN_WORKERS_OK);
    assert(mb_win_workers_close(&owner,NULL)==MB_WIN_WORKERS_OK);
    assert(mb_win_threads_leave(&domain,NULL)==MB_WIN_THREADS_OK);
    assert(mb_win_threads_close(&domain,NULL)==MB_WIN_THREADS_OK);
    puts("pthread create/admission failure and retained rollback passed");
#endif
    return 0;
}

#endif
