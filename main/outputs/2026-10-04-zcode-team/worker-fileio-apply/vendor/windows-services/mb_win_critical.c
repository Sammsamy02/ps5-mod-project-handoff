#include "mb_win_critical.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if !defined(__x86_64__)
#error "Critical section service targets AMD64"
#endif
_Static_assert(sizeof(mb_win_critical_storage)==40 && _Alignof(mb_win_critical_storage)==8,"Windows AMD64 CRITICAL_SECTION storage");
typedef struct critical_sidecar {
    void *object;
    pthread_t holder;
    uint32_t recursion,spin;
    size_t users,waiters;
} critical_sidecar;
typedef struct critical_owner {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    mb_win_thread_domain *domain;
    mb_win_thread_pin domain_pin;
    mb_win_critical_pin *pins;
    size_t capacity,pin_count,calls;
    int multiple_processors,closing,mutex_live,cond_live;
    critical_sidecar slots[];
} critical_owner;
static void lock(critical_owner *d){if(pthread_mutex_lock(&d->mutex))abort();}
static void unlock(critical_owner *d){if(pthread_mutex_unlock(&d->mutex))abort();}
static mb_win_critical_status native_result(int *out,int rc){if(out)*out=rc;return rc?MB_WIN_CRITICAL_NATIVE_ERROR:MB_WIN_CRITICAL_OK;}
static int valid_object(void *p){return p && !((uintptr_t)p&7) && (uintptr_t)p<=UINTPTR_MAX-40;}
static critical_sidecar *find(critical_owner *d,void *p){for(size_t i=0;i<d->capacity;++i)if(d->slots[i].object==p)return &d->slots[i];return NULL;}
static mb_win_critical_status acquire_state(mb_win_critical *o,void *p,critical_owner **out,critical_sidecar **slot){
    if(!o||!valid_object(p))return MB_WIN_CRITICAL_INVALID_ARGUMENT;
    critical_owner *d=o->state;if(!d||d->closing)return MB_WIN_CRITICAL_BAD_STATE;
    lock(d);critical_sidecar *s=find(d,p);
    if(!s){unlock(d);return MB_WIN_CRITICAL_BAD_STATE;}
    *out=d;*slot=s;return MB_WIN_CRITICAL_OK;
}
mb_win_thread_domain *mb_win_critical_domain(mb_win_critical *o){return o&&o->state?((critical_owner*)o->state)->domain:NULL;}
mb_win_critical_status mb_win_critical_open(mb_win_critical *o,mb_win_thread_domain *domain,size_t capacity,int *error){
    if(!o||!domain||!capacity||capacity>(SIZE_MAX-sizeof(critical_owner))/sizeof(critical_sidecar))return MB_WIN_CRITICAL_INVALID_ARGUMENT;
    if(o->state)return MB_WIN_CRITICAL_BAD_STATE;
    errno=0;long processors=sysconf(_SC_NPROCESSORS_ONLN);if(processors<=0)return native_result(error,errno?errno:EINVAL);
    critical_owner *d=calloc(1,sizeof(*d)+capacity*sizeof(critical_sidecar));if(!d)return native_result(error,ENOMEM);
    d->capacity=capacity;d->domain=domain;d->multiple_processors=processors>1;
    if(mb_win_threads_acquire(domain,&d->domain_pin)!=MB_WIN_THREADS_OK){free(d);return MB_WIN_CRITICAL_BAD_STATE;}
    int rc=pthread_mutex_init(&d->mutex,NULL);
    if(!rc){d->mutex_live=1;rc=pthread_cond_init(&d->changed,NULL);}
    if(!rc)d->cond_live=1;
    o->state=d;
    if(rc){d->closing=1;(void)mb_win_critical_close(o,NULL);return native_result(error,rc);}
    return native_result(error,0);
}
mb_win_critical_status mb_win_critical_close(mb_win_critical *o,int *error){
    if(!o)return MB_WIN_CRITICAL_INVALID_ARGUMENT;if(!o->state)return native_result(error,0);
    critical_owner *d=o->state;
    if(d->mutex_live){
        lock(d);if(d->calls||d->pin_count){unlock(d);return MB_WIN_CRITICAL_BUSY;}
        for(size_t i=0;i<d->capacity;++i)if(d->slots[i].recursion||d->slots[i].users||d->slots[i].waiters){unlock(d);return MB_WIN_CRITICAL_BUSY;}
        d->closing=1;unlock(d);
    }
    if(d->cond_live){int rc=pthread_cond_destroy(&d->changed);if(rc)return native_result(error,rc);d->cond_live=0;}
    if(d->mutex_live){int rc=pthread_mutex_destroy(&d->mutex);if(rc)return native_result(error,rc);d->mutex_live=0;}
    if(mb_win_threads_release_pin(&d->domain_pin)!=MB_WIN_THREADS_OK)return MB_WIN_CRITICAL_BAD_STATE;
    /* Windows objects might already be unmapped; only free private sidecars. */
    free(d);o->state=NULL;return native_result(error,0);
}
mb_win_critical_status mb_win_critical_acquire(mb_win_critical *o,mb_win_critical_pin *p){
    if(!o||!p)return MB_WIN_CRITICAL_INVALID_ARGUMENT;critical_owner *d=o->state;
    if(!d||d->closing)return MB_WIN_CRITICAL_BAD_STATE;
    lock(d);if(p->owner||p->next||d->pin_count==SIZE_MAX){unlock(d);return MB_WIN_CRITICAL_BAD_STATE;}
    p->owner=o;p->next=d->pins;d->pins=p;++d->pin_count;unlock(d);return MB_WIN_CRITICAL_OK;
}
mb_win_critical_status mb_win_critical_release_pin(mb_win_critical_pin *p){
    if(!p)return MB_WIN_CRITICAL_INVALID_ARGUMENT;if(!p->owner||!p->owner->state)return MB_WIN_CRITICAL_BAD_STATE;
    critical_owner *d=p->owner->state;lock(d);mb_win_critical_pin **at=&d->pins;while(*at&&*at!=p)at=&(*at)->next;
    if(!*at||!d->pin_count){unlock(d);return MB_WIN_CRITICAL_BAD_STATE;}
    *at=p->next;--d->pin_count;*p=(mb_win_critical_pin){0};unlock(d);return MB_WIN_CRITICAL_OK;
}
mb_win_critical_status mb_win_critical_initialize(mb_win_critical *o,void *p,uint32_t spin){
    if(!o||!valid_object(p))return MB_WIN_CRITICAL_INVALID_ARGUMENT;critical_owner *d=o->state;if(!d||d->closing)return MB_WIN_CRITICAL_BAD_STATE;
    lock(d);critical_sidecar *empty=NULL;
    for(size_t i=0;i<d->capacity;++i){critical_sidecar *s=&d->slots[i];
        if(s->object==p){unlock(d);return MB_WIN_CRITICAL_BAD_STATE;}
        if(!s->object){if(!empty)empty=s;continue;}
        uintptr_t a=(uintptr_t)p,b=(uintptr_t)s->object;
        if(a<b?b-a<40:a-b<40){unlock(d);return MB_WIN_CRITICAL_INVALID_ARGUMENT;}
    }
    if(!empty){unlock(d);return MB_WIN_CRITICAL_LIMIT;}
    memset(p,0,40);empty->object=p;empty->spin=d->multiple_processors?spin:0;unlock(d);return MB_WIN_CRITICAL_OK;
}
static mb_win_critical_status claim(critical_sidecar *s,pthread_t self){
    if(s->recursion==UINT32_MAX)return MB_WIN_CRITICAL_LIMIT;
    if(!s->recursion)s->holder=self;
    ++s->recursion;return MB_WIN_CRITICAL_OK;
}
mb_win_critical_status mb_win_critical_enter(mb_win_critical *o,void *p){
    critical_owner *d;critical_sidecar *s;mb_win_critical_status status=acquire_state(o,p,&d,&s);if(status)return status;
    if(d->calls==SIZE_MAX||s->users==SIZE_MAX){unlock(d);return MB_WIN_CRITICAL_LIMIT;}
    ++d->calls;++s->users;pthread_t self=pthread_self();uint32_t remaining=s->spin;
    while(s->recursion&&!pthread_equal(s->holder,self)){
        if(remaining){--remaining;unlock(d);__builtin_ia32_pause();lock(d);continue;}
        if(s->waiters==SIZE_MAX){status=MB_WIN_CRITICAL_LIMIT;break;}
        ++s->waiters;int rc=pthread_cond_wait(&d->changed,&d->mutex);--s->waiters;
        if(rc){status=MB_WIN_CRITICAL_NATIVE_ERROR;break;}
    }
    if(!status)status=claim(s,self);
    --s->users;--d->calls;unlock(d);return status;
}
mb_win_critical_status mb_win_critical_try_enter(mb_win_critical *o,void *p,int32_t *entered){
    if(!entered)return MB_WIN_CRITICAL_INVALID_ARGUMENT;critical_owner *d;critical_sidecar *s;
    mb_win_critical_status status=acquire_state(o,p,&d,&s);if(status)return status;
    pthread_t self=pthread_self();int32_t got=0;
    if(!s->recursion||pthread_equal(s->holder,self)){status=claim(s,self);got=status?0:1;}
    unlock(d);if(!status)*entered=got;return status;
}
mb_win_critical_status mb_win_critical_leave(mb_win_critical *o,void *p){
    critical_owner *d;critical_sidecar *s;mb_win_critical_status status=acquire_state(o,p,&d,&s);if(status)return status;
    if(!s->recursion||!pthread_equal(s->holder,pthread_self())){unlock(d);return MB_WIN_CRITICAL_BAD_STATE;}
    --s->recursion;if(!s->recursion&&pthread_cond_broadcast(&d->changed))abort();unlock(d);return MB_WIN_CRITICAL_OK;
}
mb_win_critical_status mb_win_critical_delete(mb_win_critical *o,void *p){
    critical_owner *d;critical_sidecar *s;mb_win_critical_status status=acquire_state(o,p,&d,&s);if(status)return status;
    if(s->recursion||s->users||s->waiters){unlock(d);return MB_WIN_CRITICAL_BUSY;}
    memset(s,0,sizeof(*s));unlock(d);return MB_WIN_CRITICAL_OK;
}
mb_win_critical_status mb_win_critical_set_spin(mb_win_critical *o,void *p,uint32_t spin,uint32_t *previous){
    if(!previous)return MB_WIN_CRITICAL_INVALID_ARGUMENT;critical_owner *d;critical_sidecar *s;
    mb_win_critical_status status=acquire_state(o,p,&d,&s);if(status)return status;
    uint32_t old=s->spin;s->spin=d->multiple_processors?spin:0;unlock(d);*previous=old;return MB_WIN_CRITICAL_OK;
}
