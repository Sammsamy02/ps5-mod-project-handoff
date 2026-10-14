/* worker-mutex-deadlock minimal host reproducer.
 * Replicates the composed sdk_driver call sequence natively against the real
 * windows-services owners: open owners -> WorkerBegin (sync+initonce+mutex
 * prepare, CreateThread, ready handshake) -> worker runs mutex thread_acquire
 * then sync gate -> WorkerCollect (INFINITE thread wait, exit code, CloseHandle,
 * initonce+mutex finish) -> workers collect (join) -> driver cleanup order.
 * Every stage prints; alarm() bounds the whole process. */
#include "mb_win_threads.h"
#include "mb_win_workers.h"
#include "mb_win_critical.h"
#include "mb_win_address_wait.h"
#include "mb_win_srw.h"
#include "mb_win_init_once.h"
#include "mb_win_mutex.h"
#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define WIN64 __attribute__((ms_abi))
#define EXPECTED_EXIT 0x81234567u
#define W_ERR_LAST 0xe17395abu
#define W_ERR_CREATOR 0xc2468aceu
static mb_win_thread_domain domain;
static mb_win_workers workers;
static mb_win_critical critical;
static mb_win_address_wait address_wait;
static mb_win_srw srw;
static mb_win_init_once init_once;
static mb_win_mutex mutex_owner;
static const uint16_t named[]={'M','b','W','o','r','k','e','r','.','N','a','m','e','d',0};
static const uint16_t named_folded[]={'m','b','w','o','r','k','e','r','.','n','a','m','e','d',0};
static const uint16_t abandon[]={'M','b','W','o','r','k','e','r','.','A','b','a','n','d','o','n',0};
static const uint16_t abandon_folded[]={'m','b','w','o','r','k','e','r','.','a','b','a','n','d','o','n',0};
static uint64_t tokens[16];static unsigned token_count;
static void keep(uint64_t t){if(t&&token_count<16)tokens[token_count++]=t;}
void mb_repro_keep_worker_token(uint64_t t){keep(t);}
static const uint16_t backslash[]={'B','a','d','\\','N','a','m','e',0};
static void stage(const char *what){fprintf(stderr,"[main] %s\n",what);fflush(stderr);}
static void die(const char *what){fprintf(stderr,"[main] FAILED: %s\n",what);exit(2);}
static void on_alarm(int sig){(void)sig;fprintf(stderr,"[alarm] REPRO BLOCKED (timeout)\n");_exit(99);}
static int code_acquire(void *c,void *a,void **t){(void)c;(void)a;*t=malloc(8);return *t?0:-1;}
static int code_release(void *c,void *t){(void)c;free(t);return 0;}
/* identical trampoline to mb_win_workers_abi.c (that unit needs the PE binder
 * headers; the reproduction only needs the typed Windows call route itself) */
uint32_t mb_win_workers_invoke(void *start,void *arg){typedef uint32_t(WIN64 *fn)(void*);return ((fn)start)(arg);}
struct wstate {
    _Alignas(8) unsigned char cs[40];
    mb_win_srwlock srw; mb_win_condition cv;
    mb_win_once once_a, once_b;
    _Atomic uint32_t gate, ready;
    atomic_uint entered, done, failure;
    uint32_t worker_error;
};
static struct wstate S;
static void *thread_handle;
static uint32_t last_error(void){return *mb_win_threads_last_error(&domain);}
static void set_last_error(uint32_t v){*mb_win_threads_last_error(&domain)=v;}
static uint32_t WIN64 worker_start(void *arg){
    struct wstate *s=arg;
    fprintf(stderr,"[worker] start\n");
    set_last_error(W_ERR_LAST);
    int32_t pending=0; void *ctx=0;
    if(!mb_win_init_once_begin(&init_once,&s->once_b,0,&pending,&ctx)||pending!=1)s->failure=1;
    fprintf(stderr,"[worker] initonce begin done\n");
    uint64_t m=mb_win_mutex_create(&mutex_owner,abandon,MB_WIN_MUTEX_INITIAL_OWNER);extern void mb_repro_keep_worker_token(uint64_t);mb_repro_keep_worker_token(m);
    if(!m)s->failure=1;
    fprintf(stderr,"[worker] mutex adopted (handle %llu), entered=1\n",(unsigned long long)m);
    atomic_store(&s->entered,1);
    mb_win_workers_sleep(&workers,0);mb_win_workers_sleep(&workers,1);
    /* mb_worker_sync_started equivalent: critical must be creator-held. */
    int32_t held=0;mb_win_critical_try_enter(&critical,s->cs,&held);
    if(held)mb_win_critical_leave(&critical,s->cs);
    mb_win_srw_acquire(&srw,&s->srw,0);s->ready=held?2:1;
    mb_win_srw_wake(&srw,&s->cv,1);mb_win_srw_release(&srw,&s->srw,0);
    fprintf(stderr,"[worker] ready=%u, waiting on gate\n",(unsigned)s->ready);
    uint32_t zero=0;
    while(!atomic_load(&s->gate))
        if(!mb_win_address_wait_wait(&address_wait,&s->gate,&zero,4,0xffffffffu)){s->failure=1;break;}
    fprintf(stderr,"[worker] gate open, critical enter\n");
    mb_win_critical_enter(&critical,s->cs);
    s->worker_error=last_error();
    mb_win_critical_leave(&critical,s->cs);
    fprintf(stderr,"[worker] worker_error=%08x\n",(unsigned)s->worker_error);
    if(!mb_win_init_once_complete(&init_once,&s->once_b,0,(void*)0x51d2beef5a5a0002ULL))s->failure=1;
    int32_t p2=0;void *c2=0;
    if(!mb_win_init_once_begin(&init_once,&s->once_b,1u,&p2,&c2)||p2!=0||c2!=(void*)0x51d2beef5a5a0002ULL)s->failure=1;
    atomic_store(&s->done,1);
    fprintf(stderr,"[worker] returning %08x (thread exit + TSD destructors follow)\n",
            s->failure?0xf0010002u:EXPECTED_EXIT);
    return s->failure?0xf0010002u:EXPECTED_EXIT;
}
static void mutex_prepare(void){
    uint64_t h=mb_win_mutex_create(&mutex_owner,NULL,0);keep(h);
    if(!h)die("prepare create unnamed");
    if(mb_win_mutex_wait_ex(&mutex_owner,h,1,0)!=0)die("prepare finite acquire");
    if(mb_win_mutex_release(&mutex_owner,h)!=1)die("prepare release");
    if(mb_win_mutex_wait_ex(&mutex_owner,h,0,1)!=0xffffffffu||last_error()!=50)die("prepare alertable");
    {uint64_t t=mb_win_mutex_create(&mutex_owner,named,0);if(!t)die("prepare named");keep(t);}
    {uint64_t t=mb_win_mutex_open_named(&mutex_owner,named_folded);if(!t)die("prepare open folded");keep(t);}
    uint64_t exists=mb_win_mutex_create(&mutex_owner,named,MB_WIN_MUTEX_INITIAL_OWNER);keep(exists);
    if(!exists||last_error()!=183)die("prepare already exists");
    if(mb_win_mutex_release(&mutex_owner,exists)!=0||last_error()!=288)die("prepare not owner");
    {uint64_t t=mb_win_mutex_open_named(&mutex_owner,named_folded);if(!t)die("prepare open wait 2");keep(t);if(mb_win_mutex_wait_ex(&mutex_owner,t,0,0)!=0)die("prepare open wait");}
    /* keep the same handle discipline as the fixture: tokens are never closed */
    if(mb_win_mutex_create(&mutex_owner,backslash,0)!=0||last_error()!=87)die("prepare backslash");
}
int main(void){
    alarm(40);signal(SIGALRM,on_alarm);setbuf(stderr,NULL);
    int native=0;
    stage("threads open+enter");
    if(mb_win_threads_open(&domain,&native))die("threads open");
    if(mb_win_threads_enter(&domain,&native))die("threads enter");
    stage("owners open (workers, critical, address_wait, srw, initonce, mutex)");
    mb_win_worker_config wc={&domain,NULL,code_acquire,code_release,8};
    if(mb_win_workers_open(&workers,&wc,&native))die("workers open");
    if(mb_win_critical_open(&critical,&domain,16,&native))die("critical open");
    if(mb_win_address_wait_open(&address_wait,&domain,&native))die("address_wait open");
    mb_win_srw_config sc={&domain,16};
    if(mb_win_srw_open(&srw,&sc,&native))die("srw open");
    mb_win_init_once_config ic={&domain,16};
    if(mb_win_init_once_open(&init_once,&ic,&native))die("init_once open");
    mb_win_mutex_config mc={&domain,16};
    if(mb_win_mutex_open(&mutex_owner,&mc,&native))die("mutex open");
    /* WorkerBegin */
    stage("sync prepare: initialize critical + hold it");
    memset(&S,0,sizeof S);
    if(mb_win_critical_initialize(&critical,S.cs,11))die("critical initialize");
    if(mb_win_critical_enter(&critical,S.cs))die("critical enter");
    mb_win_srw_initialize(&srw,&S.srw);mb_win_srw_condition_initialize(&srw,&S.cv);
    stage("initonce prepare");
    {int32_t p=0;void *c=0;
     if(!mb_win_init_once_begin(&init_once,&S.once_a,0,&p,&c)||p!=1)die("once begin");
     if(!mb_win_init_once_complete(&init_once,&S.once_a,0,(void*)0x51d1beef5a5a0001ULL))die("once complete");
     if(!mb_win_init_once_begin(&init_once,&S.once_a,1u,&p,&c)||p||c!=(void*)0x51d1beef5a5a0001ULL)die("once check");
     if(mb_win_init_once_complete(&init_once,&S.once_a,0,(void*)0x51d1beef5a5a0001ULL))die("once duplicate");
     if(mb_win_init_once_begin(&init_once,&S.once_b,1u,&p,&c)||last_error()!=997)die("once never begun");
     S.once_a=(mb_win_once){0};}
    stage("mutex prepare (fixture worker-mutex.h creator side)");
    set_last_error(W_ERR_CREATOR);
    mutex_prepare();
    stage("CreateThread (enter_call, launch, ready handshake)");
    if(mb_win_workers_enter_call(&workers))die("enter call");
    uint32_t tid=0;
    thread_handle=mb_win_workers_create(&workers,NULL,0,worker_start,&S,0,&tid);
    if(!thread_handle)die("create thread");
    /* mb_worker_sync_ready equivalent: bounded cv wait for ready */
    mb_win_srw_acquire(&srw,&S.srw,0);
    unsigned attempts=0;
    while(!S.ready){
        mb_win_srw_sleep(&srw,&S.cv,&S.srw,100,0);
        if(!S.ready&&(mb_win_workers_wait(&workers,thread_handle,0)!=258||++attempts==20))
            {mb_win_srw_release(&srw,&S.srw,0);die("ready handshake");}
    }
    mb_win_srw_release(&srw,&S.srw,0);
    mb_win_critical_leave(&critical,S.cs);
    fprintf(stderr,"[main] ready handshake done (attempts=%u)\n",attempts);
    if(mb_win_workers_wait(&workers,thread_handle,0)!=258)die("wait0 258");
    uint32_t code=0;
    if(!mb_win_workers_exit_code(&workers,thread_handle,&code)||code!=259)die("exit 259");
    mb_win_workers_sleep(&workers,0);
    stage("WorkerCollect: unblock gate");
    atomic_store(&S.gate,1);
    mb_win_address_wait_wake_single(&address_wait,&S.gate);
    mb_win_address_wait_wake_all(&address_wait,&S.gate);
    set_last_error(W_ERR_CREATOR);
    stage("WaitForSingleObject(thread, INFINITE)");
    if(mb_win_workers_wait(&workers,thread_handle,0xffffffffu)!=0)die("wait infinite");
    stage("thread signaled; exit code + CloseHandle");
    if(!mb_win_workers_exit_code(&workers,thread_handle,&code)||code!=EXPECTED_EXIT)die("exit code");
    fprintf(stderr,"[main] exit=%08x worker_error=%08x wait0=%u\n",(unsigned)code,
            (unsigned)S.worker_error,(unsigned)mb_win_workers_wait(&workers,thread_handle,0));
    if(!mb_win_workers_close_handle(&workers,thread_handle))die("close handle");
    stage("sync finish + initonce finish");
    mb_win_critical_delete(&critical,S.cs);
    {int32_t p=0;void *c=0;
     if(!mb_win_init_once_begin(&init_once,&S.once_b,1u,&p,&c)||p||c!=(void*)0x51d2beef5a5a0002ULL)die("initonce finish");}
    stage("mutex finish: open abandon + 5s wait expecting WAIT_ABANDONED");
    uint64_t ao=mb_win_mutex_open_named(&mutex_owner,abandon_folded);keep(ao);
    if(!ao)die("abandon open");
    uint32_t wait=mb_win_mutex_wait_ex(&mutex_owner,ao,5000,0);
    fprintf(stderr,"[main] abandon wait -> %08x (expect 80)\n",(unsigned)wait);
    if(wait!=0x80u)die("abandoned");
    if(mb_win_mutex_release(&mutex_owner,ao)!=1)die("transfer release");
    if(mb_win_mutex_wait_ex(&mutex_owner,ao,0,0)!=0)die("reacquire");
    if(mb_win_mutex_release(&mutex_owner,ao)!=1)die("reacquire release");
    stage("leave_call + workers collect (pthread_join)");
    if(mb_win_workers_leave_call(&workers))die("leave call");
    if(mb_win_workers_collect(&workers,&native))die("workers collect");
    stage("join complete; driver cleanup order");
    if(mb_win_workers_close(&workers,&native))die("workers close");
    if(mb_win_srw_close(&srw,&native))die("srw close");
    if(mb_win_address_wait_close(&address_wait,&native))die("address_wait close");
    if(mb_win_critical_close(&critical,&native))die("critical close");
    if(mb_win_init_once_close(&init_once,&native))die("init_once close");
    unsigned retired=0;for(unsigned i=0;i<token_count;++i)if(mb_win_mutex_close_handle(&mutex_owner,tokens[i])==1)++retired;
    fprintf(stderr,"[main] retired %u/%u mutex handle tokens (fixed fixture composition)\n",retired,token_count);
    int mc_rc=mb_win_mutex_close(&mutex_owner,&native);
    fprintf(stderr,"[main] mutex close -> %d (%s): leaked fixture handle tokens make it %s\n",
            mc_rc,mc_rc?"BUSY":"OK",mc_rc?"unretirable":"retired");
    stage("threads leave+close");
    if(mb_win_threads_leave(&domain,&native))die("threads leave");
    if(mb_win_threads_close(&domain,&native))die("threads close");
    fprintf(stderr,"[main] REPRO COMPLETED clean%s\n",mc_rc?" (except mutex owner BUSY)":"");
    return 0;
}
