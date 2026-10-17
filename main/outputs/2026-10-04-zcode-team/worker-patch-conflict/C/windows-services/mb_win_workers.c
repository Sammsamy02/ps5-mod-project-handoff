#define _POSIX_C_SOURCE 200809L
#include "mb_win_workers.h"
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>

typedef struct worker worker;
typedef struct worker_owner {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    pthread_key_t admission;
    mb_win_workers *public_owner;
    mb_win_worker_config config;
    mb_win_thread_pin domain_pin;
    mb_win_worker_pin *pins;
    worker *records;
    size_t count,pin_count,admitted;
    int closing,mutex_live,cond_live,key_live;
} worker_owner;
struct worker {
    worker_owner *owner;
    worker *next;
    pthread_t thread;
    void *start,*argument,*code_token;
    uintptr_t handle;
    uint32_t id,result;
    size_t waiters;
    int launching,created,ready,start_error,launch,done,joined,open,collecting;
};
static _Atomic uint32_t next_identity=1;
_Static_assert(sizeof(void*)==8 && sizeof(size_t)==8,"AMD64 worker pointers and SIZE_T");
static void lock(worker_owner *d){if(pthread_mutex_lock(&d->mutex))abort();}
static void unlock(worker_owner *d){if(pthread_mutex_unlock(&d->mutex))abort();}
static void signal_all(worker_owner *d){if(pthread_cond_broadcast(&d->changed))abort();}
static worker_owner *required(mb_win_workers *owner){if(!owner || !owner->state)abort();return owner->state;}
static uint32_t *last(worker_owner *d){return mb_win_threads_last_error(d->config.domain);}
static uint32_t depth(worker_owner *d){return (uint32_t)(uintptr_t)pthread_getspecific(d->admission);}
static mb_win_worker_status native_result(int *out,int rc){if(out)*out=rc;return rc?MB_WIN_WORKERS_NATIVE_ERROR:MB_WIN_WORKERS_OK;}
static worker *find(worker_owner *d,void *handle){for(worker *w=d->records;w;w=w->next)if(w->open && w->handle==(uintptr_t)handle)return w;return NULL;}
static uint32_t identity(void){uint32_t value=atomic_load(&next_identity);while(value && value<UINT32_MAX){if(atomic_compare_exchange_weak(&next_identity,&value,value+1))return value;}return 0;}
mb_win_thread_domain *mb_win_workers_domain(mb_win_workers *o){return o&&o->state?((worker_owner*)o->state)->config.domain:NULL;}
mb_win_worker_status mb_win_workers_open(mb_win_workers *o,const mb_win_worker_config *c,int *error){
    if(!o||!c||!c->domain||!c->acquire_code||!c->release_code||!c->capacity)return MB_WIN_WORKERS_INVALID_ARGUMENT;
    if(o->state)return MB_WIN_WORKERS_BAD_STATE;
    worker_owner *d=calloc(1,sizeof(*d));if(!d)return native_result(error,ENOMEM);
    d->config=*c;d->public_owner=o;
    if(mb_win_threads_acquire(c->domain,&d->domain_pin)!=MB_WIN_THREADS_OK){free(d);return MB_WIN_WORKERS_BAD_STATE;}
    int rc=pthread_mutex_init(&d->mutex,NULL);
    if(!rc){d->mutex_live=1;rc=pthread_cond_init(&d->changed,NULL);}
    if(!rc){d->cond_live=1;rc=pthread_key_create(&d->admission,NULL);}
    if(!rc)d->key_live=1;
    o->state=d;
    if(rc){d->closing=1;(void)mb_win_workers_close(o,NULL);return native_result(error,rc);}
    return native_result(error,0);
}
mb_win_worker_status mb_win_workers_acquire(mb_win_workers *o,mb_win_worker_pin *p){
    if(!o||!p)return MB_WIN_WORKERS_INVALID_ARGUMENT;
    if(!o->state)return MB_WIN_WORKERS_BAD_STATE;
    worker_owner *d=o->state;lock(d);
    if(d->closing||p->owner||p->next||d->pin_count==SIZE_MAX){unlock(d);return MB_WIN_WORKERS_BAD_STATE;}
    p->owner=o;p->next=d->pins;d->pins=p;++d->pin_count;unlock(d);return MB_WIN_WORKERS_OK;
}
mb_win_worker_status mb_win_workers_release_pin(mb_win_worker_pin *p){
    if(!p)return MB_WIN_WORKERS_INVALID_ARGUMENT;
    if(!p->owner||!p->owner->state)return MB_WIN_WORKERS_BAD_STATE;
    worker_owner *d=p->owner->state;lock(d);mb_win_worker_pin **at=&d->pins;
    while(*at && *at!=p)at=&(*at)->next;
    if(!*at||!d->pin_count){unlock(d);return MB_WIN_WORKERS_BAD_STATE;}
    *at=p->next;--d->pin_count;*p=(mb_win_worker_pin){0};unlock(d);return MB_WIN_WORKERS_OK;
}
mb_win_worker_status mb_win_workers_enter_call(mb_win_workers *o){
    worker_owner *d=required(o);(void)last(d);uint32_t n=depth(d);if(n==UINT32_MAX)return MB_WIN_WORKERS_BAD_STATE;
    lock(d);if(d->closing||(!n&&d->admitted==SIZE_MAX)){unlock(d);return MB_WIN_WORKERS_BAD_STATE;}
    int rc=pthread_setspecific(d->admission,(void*)(uintptr_t)(n+1));
    if(!rc&&!n)++d->admitted;unlock(d);return rc?MB_WIN_WORKERS_NATIVE_ERROR:MB_WIN_WORKERS_OK;
}
mb_win_worker_status mb_win_workers_leave_call(mb_win_workers *o){
    worker_owner *d=required(o);uint32_t n=depth(d);if(!n)return MB_WIN_WORKERS_BAD_STATE;
    lock(d);int rc=pthread_setspecific(d->admission,(void*)(uintptr_t)(n-1));
    if(!rc&&n==1){if(!d->admitted)abort();--d->admitted;}unlock(d);return rc?MB_WIN_WORKERS_NATIVE_ERROR:MB_WIN_WORKERS_OK;
}
uint32_t mb_win_workers_suspend_calls(mb_win_workers *o){worker_owner *d=required(o);uint32_t n=depth(d);if(pthread_setspecific(d->admission,NULL))abort();return n;}
void mb_win_workers_restore_calls(mb_win_workers *o,uint32_t n){worker_owner *d=required(o);if(depth(d)||pthread_setspecific(d->admission,(void*)(uintptr_t)n))abort();}
static void *run_worker(void *arg){
    worker *w=arg;worker_owner *d=w->owner;int native=0;
    int admitted=mb_win_threads_enter(d->config.domain,&native)==MB_WIN_THREADS_OK;
    int enabled=admitted && mb_win_workers_enter_call(d->public_owner)==MB_WIN_WORKERS_OK;
    lock(d);w->ready=1;w->start_error=enabled?0:(native?native:ENOMEM);signal_all(d);
    if(enabled)while(!w->launch)if(pthread_cond_wait(&d->changed,&d->mutex))abort();
    unlock(d);
    uint32_t result=0;
    if(enabled){result=mb_win_workers_invoke(w->start,w->argument);if(mb_win_workers_leave_call(d->public_owner)!=MB_WIN_WORKERS_OK)abort();}
    if(admitted)(void)mb_win_threads_leave(d->config.domain,&native); /* destructor owns a failed leave */
    lock(d);w->result=result;w->done=1;signal_all(d);unlock(d);return NULL;
}
mb_win_worker_status mb_win_workers_collect(mb_win_workers *o,int *error){
    if(!o)return MB_WIN_WORKERS_INVALID_ARGUMENT;if(!o->state)return MB_WIN_WORKERS_BAD_STATE;
    worker_owner *d=o->state;mb_win_worker_status result=MB_WIN_WORKERS_OK;
    lock(d);
    for(worker *w=d->records;w;){
        if(w->launching||w->collecting||!w->done){result=MB_WIN_WORKERS_BUSY;w=w->next;continue;}
        w->collecting=1;unlock(d);
        int rc=0;if(w->created&&!w->joined)rc=pthread_join(w->thread,NULL);
        int released=0;if(!rc&&w->code_token)released=d->config.release_code(d->config.context,w->code_token);
        lock(d);
        if(!rc)w->joined=1;
        if(!rc&&!released)w->code_token=NULL;
        w->collecting=0;
        if(rc){if(error)*error=rc;result=MB_WIN_WORKERS_NATIVE_ERROR;w=w->next;continue;}
        if(released){result=MB_WIN_WORKERS_CODE_ERROR;w=w->next;continue;}
        if(!w->open&&!w->waiters){worker **at=&d->records;while(*at!=w)at=&(*at)->next;*at=w->next;--d->count;worker *old=w;w=w->next;free(old);}else w=w->next;
    }
    unlock(d);return result;
}
mb_win_worker_status mb_win_workers_close(mb_win_workers *o,int *error){
    if(!o)return MB_WIN_WORKERS_INVALID_ARGUMENT;if(!o->state)return native_result(error,0);
    worker_owner *d=o->state;
    if(d->mutex_live){lock(d);if(d->count||d->pin_count||d->admitted){unlock(d);return MB_WIN_WORKERS_BUSY;}d->closing=1;unlock(d);}
    if(d->key_live){int rc=pthread_key_delete(d->admission);if(rc)return native_result(error,rc);d->key_live=0;}
    if(d->cond_live){int rc=pthread_cond_destroy(&d->changed);if(rc)return native_result(error,rc);d->cond_live=0;}
    if(d->mutex_live){int rc=pthread_mutex_destroy(&d->mutex);if(rc)return native_result(error,rc);d->mutex_live=0;}
    if(mb_win_threads_release_pin(&d->domain_pin)!=MB_WIN_THREADS_OK)return MB_WIN_WORKERS_BAD_STATE;
    free(d);o->state=NULL;return native_result(error,0);
}
void *mb_win_workers_create(mb_win_workers *o,void *attrs,size_t stack,void *start,void *arg,uint32_t flags,uint32_t *id){
    worker_owner *d=required(o);uint32_t *error=last(d),saved=*error;
    if(attrs||stack||flags){*error=50;return NULL;}if(!start){*error=87;return NULL;}if(!depth(d)){*error=170;return NULL;}
    worker *w=calloc(1,sizeof(*w));if(!w){*error=8;return NULL;}
    w->owner=d;w->start=start;w->argument=arg;w->launching=1;
    lock(d);if(d->closing||d->count>=d->config.capacity){unlock(d);free(w);*error=8;return NULL;}
    w->id=identity();if(!w->id){unlock(d);free(w);*error=8;return NULL;}w->handle=((uintptr_t)w->id<<2)|1;
    w->next=d->records;d->records=w;++d->count;unlock(d);
    int rc=d->config.acquire_code(d->config.context,start,&w->code_token);
    if(!rc&&!w->code_token)rc=EINVAL;
    if(!rc)rc=pthread_create(&w->thread,NULL,run_worker,w);
    lock(d);
    if(rc){w->done=1;w->launching=0;unlock(d);(void)mb_win_workers_collect(o,NULL);*error=8;return NULL;}
    w->created=1;
    while(!w->ready)if(pthread_cond_wait(&d->changed,&d->mutex))abort();
    if(w->start_error){w->launching=0;unlock(d);(void)mb_win_workers_collect(o,NULL);*error=8;return NULL;}
    w->open=1;w->launch=1;w->launching=0;uintptr_t handle=w->handle;uint32_t tid=w->id;signal_all(d);unlock(d);
    if(id)*id=tid;*error=saved;return (void*)handle;
}
static int monotonic(uint64_t *value){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return errno?errno:EIO;if(t.tv_sec<0||t.tv_nsec<0||t.tv_nsec>=1000000000L||(uint64_t)t.tv_sec>UINT64_MAX/1000000000)return EOVERFLOW;uint64_t s=(uint64_t)t.tv_sec*1000000000;if(UINT64_MAX-s<(uint64_t)t.tv_nsec)return EOVERFLOW;*value=s+(uint64_t)t.tv_nsec;return 0;}
static int pause_ns(uint64_t ns){struct timespec t={(time_t)(ns/1000000000),(long)(ns%1000000000)},remaining;while(nanosleep(&t,&remaining)){if(errno!=EINTR)return errno?errno:EIO;t=remaining;}return 0;}
uint32_t mb_win_workers_wait(mb_win_workers *o,void *handle,uint32_t ms){
    worker_owner *d=required(o);uint32_t *error=last(d),saved=*error,result=MB_WIN_WAIT_OBJECT_0;uint64_t start=0;
    lock(d);worker *w=find(d,handle);if(!w){unlock(d);*error=6;return MB_WIN_WAIT_FAILED;}++w->waiters;
    if(!w->done&&ms&&ms!=MB_WIN_INFINITE&&monotonic(&start)){result=MB_WIN_WAIT_FAILED;}
    while(result==0&&!w->done){
        if(!ms){result=MB_WIN_WAIT_TIMEOUT;break;}
        if(ms==MB_WIN_INFINITE){if(pthread_cond_wait(&d->changed,&d->mutex)){result=MB_WIN_WAIT_FAILED;break;}continue;}
        uint64_t now;if(monotonic(&now)||now<start){result=MB_WIN_WAIT_FAILED;break;}
        uint64_t duration=(uint64_t)ms*1000000,elapsed=now-start;
        if(elapsed>=duration){result=MB_WIN_WAIT_TIMEOUT;break;}
        uint64_t delay=duration-elapsed;if(delay>1000000)delay=1000000;
        unlock(d);int rc=pause_ns(delay);lock(d);if(rc){result=MB_WIN_WAIT_FAILED;break;}
    }
    --w->waiters;unlock(d);*error=result==MB_WIN_WAIT_FAILED?31:saved;return result;
}
int32_t mb_win_workers_exit_code(mb_win_workers *o,void *handle,uint32_t *code){
    worker_owner *d=required(o);uint32_t *error=last(d);if(!code){*error=87;return 0;}
    lock(d);worker *w=find(d,handle);if(!w){unlock(d);*error=6;return 0;}uint32_t value=w->done?w->result:MB_WIN_STILL_ACTIVE;unlock(d);*code=value;return 1;
}
int32_t mb_win_workers_close_handle(mb_win_workers *o,void *handle){
    worker_owner *d=required(o);uint32_t *error=last(d);lock(d);worker *w=find(d,handle);
    if(!w){unlock(d);*error=6;return 0;}w->open=0;unlock(d);return 1;
}
void mb_win_workers_sleep(mb_win_workers *o,uint32_t ms){
    worker_owner *d=required(o);uint32_t *error=last(d),saved=*error;
    if(!ms){if(sched_yield())abort();}
    else if(ms==MB_WIN_INFINITE){for(;;)if(pause_ns(UINT64_C(3600000000000)))abort();}
    else if(pause_ns((uint64_t)ms*1000000))abort();
    *error=saved;
}
