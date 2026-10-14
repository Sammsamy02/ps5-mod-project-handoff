#include "mb_win_srw.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

_Static_assert(sizeof(mb_win_srwlock)==8&&_Alignof(mb_win_srwlock)==8,"Windows SRW storage");
_Static_assert(sizeof(mb_win_condition)==8&&_Alignof(mb_win_condition)==8,"Windows CV storage");
typedef struct waiter { struct waiter *next; pthread_cond_t condition; unsigned notified; } waiter;
typedef struct sidecar {
    struct sidecar *next;
    uint64_t *address,token;
    unsigned kind,writer;
    size_t readers,users,waiting_writers;
    pthread_t writer_thread;
    pthread_cond_t changed;
    waiter *first,*last;
} sidecar;
typedef struct state {
    pthread_mutex_t mutex;
    mb_win_thread_domain *domain;
    mb_win_thread_pin domain_pin;
    mb_win_srw_pin *pins;
    sidecar *objects;
    size_t count,capacity;
    uint64_t next_token;
} state;
static void must(int status){if(status)abort();}
static state *get(mb_win_srw *o){if(!o||!o->state)abort();return o->state;}
static state *enter(mb_win_srw *o){state *s=get(o);(void)mb_win_threads_last_error(s->domain);must(pthread_mutex_lock(&s->mutex));return s;}
static void unlock(state *s){must(pthread_mutex_unlock(&s->mutex));}
static void pointer(const void *p){if(!p||((uintptr_t)p&7))abort();}
static int condition_init(pthread_cond_t *c){
    pthread_condattr_t attr;int e=pthread_condattr_init(&attr);if(e)return e;
#if !defined(__APPLE__)
    e=pthread_condattr_setclock(&attr,CLOCK_MONOTONIC);
#endif
    if(!e)e=pthread_cond_init(c,&attr);
    must(pthread_condattr_destroy(&attr));return e;
}
/* Lookup never follows a caller-supplied token as a pointer. Public storage is
 * read/written only while a current valid call owns this object's lifetime. */
static sidecar *lookup(state *s,uint64_t *p,unsigned kind,int create,int *native_error){
    pointer(p);
    for(sidecar *n=s->objects;n;n=n->next)if(n->address==p){if(n->kind!=kind||*p!=n->token)abort();return n;}
    if(*p)abort();
    if(!create)return NULL;
    if(s->count==s->capacity||s->next_token==UINT64_MAX)return NULL;
    sidecar *n=calloc(1,sizeof(*n));if(!n)return NULL;
    int e=condition_init(&n->changed);if(e){if(native_error)*native_error=e;free(n);return NULL;}
    n->address=p;n->kind=kind;n->token=++s->next_token;n->next=s->objects;s->objects=n;++s->count;*p=n->token;return n;
}
static void retire(state *s,sidecar *n){
    if(n->users||n->readers||n->writer||n->first||n->waiting_writers)return;
    sidecar **p=&s->objects;while(*p&&*p!=n)p=&(*p)->next;if(!*p)abort();
    must(pthread_cond_destroy(&n->changed));*n->address=0;*p=n->next;--s->count;free(n);
}
static void take(state *s,sidecar *n,unsigned shared){
    if(n->users==SIZE_MAX||n->readers==SIZE_MAX||n->waiting_writers==SIZE_MAX)abort();
    ++n->users;if(!shared)++n->waiting_writers;
    while(n->writer||(!shared&&n->readers)||(shared&&n->waiting_writers))must(pthread_cond_wait(&n->changed,&s->mutex));
    if(shared)++n->readers;else{--n->waiting_writers;n->writer=1;n->writer_thread=pthread_self();}
    --n->users;
}
static void drop(sidecar *n,unsigned shared){
    if(shared){if(!n->readers||n->writer)abort();--n->readers;}
    else{if(!n->writer||!pthread_equal(n->writer_thread,pthread_self()))abort();n->writer=0;}
    must(pthread_cond_broadcast(&n->changed));
}
mb_win_srw_status mb_win_srw_open(mb_win_srw *o,const mb_win_srw_config *c,int *error){
    if(!o||!c||!c->domain||!c->capacity)return MB_WIN_SRW_INVALID_ARGUMENT;
    if(o->state)return MB_WIN_SRW_BAD_STATE;
    state *s=calloc(1,sizeof(*s));if(!s){if(error)*error=ENOMEM;return MB_WIN_SRW_NATIVE_ERROR;}
    int e=pthread_mutex_init(&s->mutex,NULL);if(e){free(s);if(error)*error=e;return MB_WIN_SRW_NATIVE_ERROR;}
    if(mb_win_threads_acquire(c->domain,&s->domain_pin)!=MB_WIN_THREADS_OK){must(pthread_mutex_destroy(&s->mutex));free(s);return MB_WIN_SRW_BAD_STATE;}
    s->domain=c->domain;s->capacity=c->capacity;o->state=s;if(error)*error=0;return MB_WIN_SRW_OK;
}
mb_win_srw_status mb_win_srw_close(mb_win_srw *o,int *error){
    if(!o)return MB_WIN_SRW_INVALID_ARGUMENT;if(!o->state)return MB_WIN_SRW_BAD_STATE;
    state *s=o->state;must(pthread_mutex_lock(&s->mutex));int busy=s->pins||s->objects;unlock(s);if(busy)return MB_WIN_SRW_BUSY;
    int e=pthread_mutex_destroy(&s->mutex);if(e){if(error)*error=e;return MB_WIN_SRW_NATIVE_ERROR;}
    if(mb_win_threads_release_pin(&s->domain_pin)!=MB_WIN_THREADS_OK)abort();free(s);o->state=NULL;if(error)*error=0;return MB_WIN_SRW_OK;
}
mb_win_srw_status mb_win_srw_acquire_pin(mb_win_srw *o,mb_win_srw_pin *p){
    if(!o||!p)return MB_WIN_SRW_INVALID_ARGUMENT;if(!o->state||p->owner||p->next)return MB_WIN_SRW_BAD_STATE;
    state *s=o->state;must(pthread_mutex_lock(&s->mutex));for(mb_win_srw_pin *n=s->pins;n;n=n->next)if(n==p){unlock(s);return MB_WIN_SRW_BAD_STATE;}
    p->owner=o;p->next=s->pins;s->pins=p;unlock(s);return MB_WIN_SRW_OK;
}
mb_win_srw_status mb_win_srw_release_pin(mb_win_srw_pin *p){
    if(!p)return MB_WIN_SRW_INVALID_ARGUMENT;if(!p->owner||!p->owner->state)return MB_WIN_SRW_BAD_STATE;
    state *s=p->owner->state;must(pthread_mutex_lock(&s->mutex));mb_win_srw_pin **at=&s->pins;while(*at&&*at!=p)at=&(*at)->next;
    if(!*at){unlock(s);return MB_WIN_SRW_BAD_STATE;}*at=p->next;*p=(mb_win_srw_pin){0};unlock(s);return MB_WIN_SRW_OK;
}
mb_win_thread_domain *mb_win_srw_domain(mb_win_srw *o){return o&&o->state?((state*)o->state)->domain:NULL;}
static void initialize(mb_win_srw *o,uint64_t *p){state *s=enter(o);pointer(p);for(sidecar *n=s->objects;n;n=n->next)if(n->address==p)abort();*p=0;unlock(s);}
void mb_win_srw_initialize(mb_win_srw *o,mb_win_srwlock *p){initialize(o,p?&p->opaque:NULL);}
void mb_win_srw_condition_initialize(mb_win_srw *o,mb_win_condition *p){initialize(o,p?&p->opaque:NULL);}
void mb_win_srw_acquire(mb_win_srw *o,mb_win_srwlock *p,uint32_t shared){
    state *s=enter(o);if(shared>1)abort();sidecar *n=lookup(s,p?&p->opaque:NULL,1,1,NULL);if(!n)abort();take(s,n,shared);unlock(s);
}
uint8_t mb_win_srw_try_acquire(mb_win_srw *o,mb_win_srwlock *p,uint32_t shared){
    state *s=enter(o);if(shared>1)abort();sidecar *n=lookup(s,p?&p->opaque:NULL,1,1,NULL);if(!n)abort();
    uint8_t ok=(uint8_t)(!n->writer&&(shared?!n->waiting_writers:!n->readers));
    if(ok){if(n->readers==SIZE_MAX)abort();if(shared)++n->readers;else{n->writer=1;n->writer_thread=pthread_self();}}
    retire(s,n);unlock(s);return ok;
}
void mb_win_srw_release(mb_win_srw *o,mb_win_srwlock *p,uint32_t shared){
    state *s=enter(o);if(shared>1)abort();sidecar *n=lookup(s,p?&p->opaque:NULL,1,0,NULL);if(!n)abort();drop(n,shared);retire(s,n);unlock(s);
}
static int monotonic(struct timespec *t){int e=clock_gettime(CLOCK_MONOTONIC,t);return e?errno:0;}
static int before(const struct timespec *a,const struct timespec *b){return a->tv_sec<b->tv_sec||(a->tv_sec==b->tv_sec&&a->tv_nsec<b->tv_nsec);}
static int timed_wait(state *s,waiter *w,const struct timespec *deadline){
#if defined(__APPLE__)
    struct timespec now,relative;int e=monotonic(&now);if(e)return e;
    if(!before(&now,deadline))return ETIMEDOUT;
    relative.tv_sec=deadline->tv_sec-now.tv_sec;relative.tv_nsec=deadline->tv_nsec-now.tv_nsec;
    if(relative.tv_nsec<0){--relative.tv_sec;relative.tv_nsec+=1000000000;}
    return pthread_cond_timedwait_relative_np(&w->condition,&s->mutex,&relative);
#else
    return pthread_cond_timedwait(&w->condition,&s->mutex,deadline);
#endif
}
int32_t mb_win_srw_sleep(mb_win_srw *o,mb_win_condition *cv,mb_win_srwlock *p,uint32_t ms,uint32_t flags){
    state *s=enter(o);uint32_t *last_error=mb_win_threads_last_error(s->domain),saved=*last_error;
    if(flags>1){unlock(s);*last_error=87;return 0;}
    sidecar *lock=lookup(s,p?&p->opaque:NULL,1,0,NULL);
    /* Adapter retirement recovery: this owner retires an idle SRW sidecar at
     * the final release (the caller's word doubles as the registry token), so
     * a peer thread whose complete acquire->release cycle fits between two
     * SleepConditionVariableSRW iterations of a looping caller destroys and
     * may recreate that sidecar mid-protocol. In a correct protocol the caller
     * provably still holds the mode its previous sleep re-acquired (a peer can
     * only take the mode while this caller is inside a sleep), so a missing or
     * foreign-owned sidecar at entry is exactly that desync, never a genuinely
     * unowned sleep: re-register and re-adopt instead of failing stop. */
    int readopted=0;
    if(!lock){
        lock=lookup(s,p?&p->opaque:NULL,1,1,NULL);
        if(!lock){unlock(s);*last_error=8;return 0;}
        readopted=1;
    }else if(flags){if(!lock->readers||lock->writer)abort();}
    else if(!lock->writer||!pthread_equal(lock->writer_thread,pthread_self())){lock=lookup(s,p?&p->opaque:NULL,1,1,NULL);if(!lock){unlock(s);*last_error=8;return 0;}readopted=1;}
    if(readopted){if(lock->writer||lock->readers){unlock(s);*last_error=31;return 0;}if(flags)++lock->readers;else{lock->writer=1;lock->writer_thread=pthread_self();}}
    struct timespec deadline={0};int e=0;
    if(ms!=UINT32_MAX){e=monotonic(&deadline);if(!e&&(deadline.tv_sec<0||deadline.tv_sec>INT64_MAX-INT64_C(4294968)||deadline.tv_nsec<0||deadline.tv_nsec>=1000000000))e=EOVERFLOW;
        if(!e){deadline.tv_sec+=ms/1000;deadline.tv_nsec+=(long)(ms%1000)*1000000;if(deadline.tv_nsec>=1000000000){++deadline.tv_sec;deadline.tv_nsec-=1000000000;}}}
    if(e){unlock(s);*last_error=31;return 0;}
    sidecar *condition=lookup(s,cv?&cv->opaque:NULL,2,1,&e);if(!condition){unlock(s);*last_error=e?31:8;return 0;}
    waiter w={0};e=condition_init(&w.condition);if(e){retire(s,condition);unlock(s);*last_error=31;return 0;}
    if(condition->last)condition->last->next=&w;else condition->first=&w;condition->last=&w;
    if(condition->users==SIZE_MAX||lock->users==SIZE_MAX)abort();
    ++condition->users;++lock->users;drop(lock,flags);
    while(!w.notified){
        e=ms==UINT32_MAX?pthread_cond_wait(&w.condition,&s->mutex):timed_wait(s,&w,&deadline);
        if(e==ETIMEDOUT){struct timespec now;int clock_error=monotonic(&now);if(clock_error){e=clock_error;break;}if(!before(&now,&deadline))break;e=0;}
        if(e)break;
    }
    int32_t result=w.notified?1:0;uint32_t failure=e==ETIMEDOUT?1460:31;
    waiter **at=&condition->first,*previous=NULL;while(*at&&*at!=&w){previous=*at;at=&(*at)->next;}if(!*at)abort();*at=w.next;if(condition->last==&w)condition->last=previous;
    --condition->users;must(pthread_cond_destroy(&w.condition));retire(s,condition);
    /* The extra user pins this lock while the caller owns no logical mode. */
    take(s,lock,flags);--lock->users;unlock(s);*last_error=result?saved:failure;return result;
}
void mb_win_srw_wake(mb_win_srw *o,mb_win_condition *cv,uint32_t all){
    state *s=enter(o);if(all>1)abort();sidecar *n=lookup(s,cv?&cv->opaque:NULL,2,0,NULL);
    if(n)for(waiter *w=n->first;w;w=w->next)if(!w->notified){w->notified=1;must(pthread_cond_signal(&w->condition));if(!all)break;}
    unlock(s);
}
