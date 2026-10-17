#ifdef MB_WIN_SRW_LAYOUT
#include "../mb_win_srw_abi.h"
const unsigned long long mb_srw_layout[]={sizeof(void*),sizeof(size_t),sizeof(uint32_t),sizeof(uint8_t),sizeof(mb_win_srwlock),_Alignof(mb_win_srwlock),sizeof(mb_win_condition),_Alignof(mb_win_condition),sizeof(mb_win_srw),sizeof(mb_win_srw_pin),sizeof(mb_win_srw_config),sizeof(mb_win_srw_session),offsetof(mb_win_srw_session,module),offsetof(mb_win_srw_session,active)};
#else
#include "../mb_win_srw_abi.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#undef assert
#define assert(x) do { if(!(x)){fprintf(stderr,"CHECK failed at %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);} } while(0)
#define WIN __attribute__((ms_abi))
static mb_win_srw owner;
static mb_win_thread_domain domain;
static mb_win_srwlock lock;
static mb_win_condition cv;
typedef void(WIN *lock_fn)(mb_win_srwlock*);
typedef uint8_t(WIN *try_fn)(mb_win_srwlock*);
typedef void(WIN *cv_fn)(mb_win_condition*);
static lock_fn init_lock,acq_ex,acq_sh,rel_ex,rel_sh;
static try_fn try_ex,try_sh;
static cv_fn init_cv,wake,wake_all;
static int32_t(WIN *sleep_cv)(mb_win_condition*,mb_win_srwlock*,uint32_t,uint32_t);
static atomic_uint ready,go,done;
static unsigned permits;
static void pause_short(void){struct timespec t={0,1000000};nanosleep(&t,NULL);}
static void until(atomic_uint *p,unsigned n){for(unsigned i=0;i<5000&&atomic_load(p)!=n;++i)pause_short();assert(atomic_load(p)==n);}
static void *bind(mb_win_srw_session *s,const char *name){mb_pe_import i={0};i.library=(mb_pe_string){"KeRnEl32.DlL",12};i.name=(mb_pe_string){name,strlen(name)};uint64_t address=0;assert(mb_win_srw_bind(s,&i,&address)==MB_PE_BIND_BOUND);return (void*)(uintptr_t)address;}
static void enter(void){assert(mb_win_threads_enter(&domain,NULL)==MB_WIN_THREADS_OK);*mb_win_threads_last_error(&domain)=0xfedc0123;}
static void leave(void){assert(*mb_win_threads_last_error(&domain)==0xfedc0123);assert(mb_win_threads_leave(&domain,NULL)==MB_WIN_THREADS_OK);}
static void *reader(void *arg){(void)arg;enter();acq_sh(&lock);atomic_fetch_add(&ready,1);while(!atomic_load(&go))pause_short();rel_sh(&lock);leave();return NULL;}
static void *waiter(void *arg){uint32_t shared=(uint32_t)(uintptr_t)arg;enter();if(shared)acq_sh(&lock);else acq_ex(&lock);atomic_fetch_add(&ready,1);while(!permits)assert(sleep_cv(&cv,&lock,UINT32_MAX,shared));if(!shared)--permits;if(shared)rel_sh(&lock);else rel_ex(&lock);atomic_fetch_add(&done,1);leave();return NULL;}
int main(void){
    alarm(30);assert(mb_win_threads_open(&domain,NULL)==MB_WIN_THREADS_OK);enter();
    mb_win_srw_config config={&domain,16};assert(mb_win_srw_open(&owner,&config,NULL)==MB_WIN_SRW_OK);
    mb_pe_module module={0};mb_win_srw_session session={0};assert(mb_win_srw_begin(&session,&owner,&module)==MB_WIN_SRW_OK);module.state=MB_PE_MODULE_ACTIVE;
    init_lock=bind(&session,"InitializeSRWLock");acq_ex=bind(&session,"AcquireSRWLockExclusive");acq_sh=bind(&session,"AcquireSRWLockShared");rel_ex=bind(&session,"ReleaseSRWLockExclusive");rel_sh=bind(&session,"ReleaseSRWLockShared");try_ex=bind(&session,"TryAcquireSRWLockExclusive");try_sh=bind(&session,"TryAcquireSRWLockShared");init_cv=bind(&session,"InitializeConditionVariable");wake=bind(&session,"WakeConditionVariable");wake_all=bind(&session,"WakeAllConditionVariable");sleep_cv=bind(&session,"SleepConditionVariableSRW");
    assert(try_ex(&lock));assert(!try_sh(&lock));assert(!try_ex(&lock));assert(mb_win_srw_close(&owner,NULL)==MB_WIN_SRW_BUSY);rel_ex(&lock);assert(lock.opaque==0);assert(try_sh(&lock));assert(!try_ex(&lock));rel_sh(&lock);assert(lock.opaque==0);init_lock(&lock);init_cv(&cv);wake(&cv);wake_all(&cv);assert(cv.opaque==0);
    puts("PASS: static zero, typed eleven-function binder, try contention and idle retirement");
    pthread_t a,b,c;assert(!pthread_create(&a,NULL,reader,NULL));assert(!pthread_create(&b,NULL,reader,NULL));until(&ready,2);assert(!try_ex(&lock));assert(try_sh(&lock));rel_sh(&lock);atomic_store(&go,1);assert(!pthread_join(a,NULL));assert(!pthread_join(b,NULL));assert(lock.opaque==0);
    puts("PASS: simultaneous real pthread readers exclude writer");
    atomic_store(&ready,0);atomic_store(&done,0);assert(!pthread_create(&a,NULL,waiter,NULL));assert(!pthread_create(&b,NULL,waiter,NULL));assert(!pthread_create(&c,NULL,waiter,NULL));until(&ready,3);acq_ex(&lock);permits=1;wake(&cv);rel_ex(&lock);until(&done,1);acq_ex(&lock);permits=2;wake_all(&cv);rel_ex(&lock);assert(!pthread_join(a,NULL));assert(!pthread_join(b,NULL));assert(!pthread_join(c,NULL));assert(lock.opaque==0&&cv.opaque==0);
    puts("PASS: exclusive atomic release/wait/reacquire, wake one then all");
    atomic_store(&ready,0);atomic_store(&done,0);permits=0;assert(!pthread_create(&a,NULL,waiter,(void*)1));until(&ready,1);acq_ex(&lock);permits=1;wake_all(&cv);rel_ex(&lock);assert(!pthread_join(a,NULL));assert(lock.opaque==0&&cv.opaque==0);
    puts("PASS: shared-mode wait releases readers and reacquires shared");
    acq_ex(&lock);assert(!sleep_cv(&cv,&lock,0,2));assert(*mb_win_threads_last_error(&domain)==87);assert(!try_ex(&lock));assert(!sleep_cv(&cv,&lock,0,0));assert(*mb_win_threads_last_error(&domain)==1460);assert(!try_ex(&lock));struct timespec start,end;assert(!clock_gettime(CLOCK_MONOTONIC,&start));assert(!sleep_cv(&cv,&lock,15,0));assert(!clock_gettime(CLOCK_MONOTONIC,&end));assert((end.tv_sec-start.tv_sec)*1000000000LL+end.tv_nsec-start.tv_nsec>=15000000);assert(*mb_win_threads_last_error(&domain)==1460);assert(!try_sh(&lock));rel_ex(&lock);assert(lock.opaque==0&&cv.opaque==0);*mb_win_threads_last_error(&domain)=0xfedc0123;
    puts("PASS: zero/finite timeout, invalid flags, lock retained, Windows last error");
    mb_pe_import unknown={0};unknown.library=(mb_pe_string){"other.dll",9};unknown.name=(mb_pe_string){"InitializeSRWLock",17};uint64_t out=0x1234;assert(mb_win_srw_bind(&session,&unknown,&out)==MB_PE_BIND_UNRESOLVED&&out==0x1234);mb_win_srw_session copied=session;assert(mb_win_srw_end(&copied)==MB_WIN_SRW_BAD_STATE);assert(mb_win_srw_end(&session)==MB_WIN_SRW_BUSY);module.state=MB_PE_MODULE_EMPTY;assert(mb_win_srw_end(&session)==MB_WIN_SRW_OK);
    /* Idle public storage can disappear before owner close. */
    mb_win_srwlock *temporary=calloc(1,sizeof(*temporary));assert(temporary);mb_win_srw_acquire(&owner,temporary,0);assert(mb_win_srw_close(&owner,NULL)==MB_WIN_SRW_BUSY);mb_win_srw_release(&owner,temporary,0);free(temporary);
    assert(mb_win_srw_close(&owner,NULL)==MB_WIN_SRW_OK);
    config.capacity=1;assert(mb_win_srw_open(&owner,&config,NULL)==MB_WIN_SRW_OK);mb_win_srw_acquire(&owner,&lock,0);assert(!mb_win_srw_sleep(&owner,&cv,&lock,0,0));assert(*mb_win_threads_last_error(&domain)==8);assert(!mb_win_srw_try_acquire(&owner,&lock,0));mb_win_srw_release(&owner,&lock,0);assert(mb_win_srw_close(&owner,NULL)==MB_WIN_SRW_OK);*mb_win_threads_last_error(&domain)=0xfedc0123;leave();assert(mb_win_threads_close(&domain,NULL)==MB_WIN_THREADS_OK);puts("PASS: original session lifetime, unknown pair, forgotten idle object teardown");return 0;
}
#endif
