/* VENDORED ADAPTED COPY (origin: pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_srw.c; that original file is unchanged). */
#include "mb_win_init_once.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

_Static_assert(sizeof(mb_win_once)==8&&_Alignof(mb_win_once)==8,"Windows INIT_ONCE storage");
#define PHASE_EXCLUSIVE 1u
#define PHASE_ASYNC 2u
#define PHASE_COMPLETE 3u
typedef struct right { struct right *next; pthread_t thread; } right;
typedef struct waiter { struct waiter *next; pthread_t thread; unsigned async; } waiter;
typedef struct sidecar {
    struct sidecar *next;
    uint64_t *address,token;
    unsigned phase;
    waiter *queue_first,*queue_last;
    pthread_t initializer;
    right *rights;
    void *context;
    pthread_cond_t changed;
} sidecar;
typedef struct state {
    pthread_mutex_t mutex;
    mb_win_thread_domain *domain;
    mb_win_thread_pin domain_pin;
    mb_win_init_once_pin *pins;
    sidecar *objects;
    size_t count,capacity;
    uint64_t next_token;
} state;
static void must(int status){if(status)abort();}
static state *get(mb_win_init_once *o){if(!o||!o->state)abort();return o->state;}
static state *enter(mb_win_init_once *o){state *s=get(o);(void)mb_win_threads_last_error(s->domain);must(pthread_mutex_lock(&s->mutex));return s;}
static void unlock(state *s){must(pthread_mutex_unlock(&s->mutex));}
static void pointer(const void *p){if(!p||((uintptr_t)p&7))abort();}
/* Lookup never follows a caller-supplied token as a pointer. Public storage is
 * read/written only while a current valid call owns this object's lifetime. */
static sidecar *lookup(state *s,uint64_t *p,int create,int *native_error){
    pointer(p);
    for(sidecar *n=s->objects;n;n=n->next)if(n->address==p){if(*p!=n->token)abort();return n;}
    if(*p)abort();
    if(!create)return NULL;
    if(s->count==s->capacity||s->next_token==UINT64_MAX)return NULL;
    sidecar *n=calloc(1,sizeof(*n));if(!n)return NULL;
    int e=pthread_cond_init(&n->changed,NULL);if(e){if(native_error)*native_error=e;free(n);return NULL;}
    n->address=p;n->token=++s->next_token;n->next=s->objects;s->objects=n;++s->count;*p=n->token;return n;
}
static void free_rights(sidecar *n){while(n->rights){right *r=n->rights;n->rights=r->next;free(r);}}
/* Call only with the state mutex held and with no waiters/rights left. */
static void destroy_active(state *s,sidecar *n,int zero_storage){
    if(n->queue_first||n->rights)abort();
    sidecar **p=&s->objects;while(*p&&*p!=n)p=&(*p)->next;if(!*p)abort();
    must(pthread_cond_destroy(&n->changed));
    if(zero_storage)*n->address=0;
    *p=n->next;--s->count;free(n);
}
static int holds_right(sidecar *n){
    if(n->phase==PHASE_EXCLUSIVE)return pthread_equal(n->initializer,pthread_self());
    if(n->phase==PHASE_ASYNC)for(right *r=n->rights;r;r=r->next)if(pthread_equal(r->thread,pthread_self()))return 1;
    return 0;
}
static int push_right(sidecar *n){right *r=malloc(sizeof(*r));if(!r)return 0;r->thread=pthread_self();r->next=n->rights;n->rights=r;return 1;}
static void pop_right(sidecar *n){
    right **p=&n->rights;while(*p&&!pthread_equal((*p)->thread,pthread_self()))p=&(*p)->next;
    if(!*p)abort();right *r=*p;*p=r->next;free(r);
}
static void enqueue(sidecar *n,waiter *w){
    if(n->queue_last)n->queue_last->next=w;else n->queue_first=w;
    n->queue_last=w;
}
static void dequeue(sidecar *n,pthread_t self){
    waiter **p=&n->queue_first,*previous=NULL;
    while(*p&&!pthread_equal((*p)->thread,self)){previous=*p;p=&(*p)->next;}
    if(!*p)abort();
    waiter *w=*p;*p=w->next;if(n->queue_last==w)n->queue_last=previous;
}
mb_win_init_once_status mb_win_init_once_open(mb_win_init_once *o,const mb_win_init_once_config *c,int *error){(void)o;(void)c;(void)error;return MB_WIN_INIT_ONCE_BAD_STATE;}
mb_win_init_once_status mb_win_init_once_close(mb_win_init_once *o,int *error){
    if(!o)return MB_WIN_INIT_ONCE_INVALID_ARGUMENT;if(!o->state)return MB_WIN_INIT_ONCE_BAD_STATE;
    state *s=o->state;must(pthread_mutex_lock(&s->mutex));
    if(s->pins){unlock(s);return MB_WIN_INIT_ONCE_BUSY;}
    for(sidecar *n=s->objects;n;n=n->next)
        if(n->phase!=PHASE_COMPLETE||n->queue_first||n->rights){unlock(s);return MB_WIN_INIT_ONCE_BUSY;}
    /* Completed objects keep no active rights or waiters; release their
     * retained contexts here. Public storage is left untouched (it holds a
     * stale token the caller must zero before reuse, as on Windows). */
    while(s->objects){
        sidecar *n=s->objects;s->objects=n->next;
        if(n->queue_first)abort();
        free_rights(n);must(pthread_cond_destroy(&n->changed));--s->count;free(n);
    }
    unlock(s);
    int e=pthread_mutex_destroy(&s->mutex);if(e){if(error)*error=e;return MB_WIN_INIT_ONCE_NATIVE_ERROR;}
    if(mb_win_threads_release_pin(&s->domain_pin)!=MB_WIN_THREADS_OK)abort();free(s);o->state=NULL;if(error)*error=0;return MB_WIN_INIT_ONCE_OK;
}
mb_win_init_once_status mb_win_init_once_acquire_pin(mb_win_init_once *o,mb_win_init_once_pin *p){
    if(!o||!p)return MB_WIN_INIT_ONCE_INVALID_ARGUMENT;if(!o->state||p->owner||p->next)return MB_WIN_INIT_ONCE_BAD_STATE;
    state *s=o->state;must(pthread_mutex_lock(&s->mutex));for(mb_win_init_once_pin *n=s->pins;n;n=n->next)if(n==p){unlock(s);return MB_WIN_INIT_ONCE_BAD_STATE;}
    p->owner=o;p->next=s->pins;s->pins=p;unlock(s);return MB_WIN_INIT_ONCE_OK;
}
mb_win_init_once_status mb_win_init_once_release_pin(mb_win_init_once_pin *p){
    if(!p)return MB_WIN_INIT_ONCE_INVALID_ARGUMENT;if(!p->owner||!p->owner->state)return MB_WIN_INIT_ONCE_BAD_STATE;
    state *s=p->owner->state;must(pthread_mutex_lock(&s->mutex));mb_win_init_once_pin **at=&s->pins;while(*at&&*at!=p)at=&(*at)->next;
    if(!*at){unlock(s);return MB_WIN_INIT_ONCE_BAD_STATE;}*at=p->next;*p=(mb_win_init_once_pin){0};unlock(s);return MB_WIN_INIT_ONCE_OK;
}
mb_win_thread_domain *mb_win_init_once_domain(mb_win_init_once *o){return o&&o->state?((state*)o->state)->domain:NULL;}
int32_t mb_win_init_once_begin(mb_win_init_once *o,mb_win_once *p,uint32_t flags,int32_t *pending,void **context){
    state *s=enter(o);
    uint32_t *last_error=mb_win_threads_last_error(s->domain);
    uint32_t saved=*last_error;
    if(flags&~(MB_WIN_INIT_ONCE_CHECK_ONLY|MB_WIN_INIT_ONCE_ASYNC)){unlock(s);*last_error=MB_WIN_ERROR_INVALID_PARAMETER;return 0;}
    sidecar *n=lookup(s,p?&p->opaque:NULL,!(flags&MB_WIN_INIT_ONCE_CHECK_ONLY),NULL);
    if(flags&MB_WIN_INIT_ONCE_CHECK_ONLY){
        /* Microsoft: check-only succeeds only when initialization completed;
         * otherwise the call fails and last error is ERROR_IO_PENDING. */
        if(n&&n->phase==PHASE_COMPLETE){
            if(pending)*pending=0;if(context)*context=n->context;
            unlock(s);*last_error=saved;return 1;
        }
        unlock(s);*last_error=MB_WIN_ERROR_IO_PENDING;return 0;
    }
    if(!n){unlock(s);*last_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return 0;}
    if(n->phase==PHASE_COMPLETE){
        if(pending)*pending=0;if(context)*context=n->context;
        unlock(s);*last_error=saved;return 1;
    }
    if(n->phase==0){
        /* lookup() just created this sidecar: grant the initial right. No
         * other sidecar in the list is ever left in phase 0. */
        if(flags&MB_WIN_INIT_ONCE_ASYNC){
            n->phase=PHASE_ASYNC;
            if(!push_right(n)){destroy_active(s,n,1);unlock(s);*last_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return 0;}
        }else{
            n->phase=PHASE_EXCLUSIVE;
            n->initializer=pthread_self();
        }
        if(pending)*pending=1;
        unlock(s);*last_error=saved;return 1;
    }
    if(n->phase==PHASE_ASYNC&&(flags&MB_WIN_INIT_ONCE_ASYNC)){
        /* Every asynchronous asker runs initialization concurrently. */
        if(!push_right(n)){unlock(s);*last_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return 0;}
        if(pending)*pending=1;
        unlock(s);*last_error=saved;return 1;
    }
    if(holds_right(n))abort(); /* a thread already holding rights cannot begin again */
    waiter w={NULL,pthread_self(),(flags&MB_WIN_INIT_ONCE_ASYNC)?1u:0u};
    enqueue(n,&w);
    for(;;){
        if(n->phase==PHASE_COMPLETE)break;
        if(n->phase==PHASE_EXCLUSIVE&&pthread_equal(n->initializer,w.thread))break; /* failure handoff */
        must(pthread_cond_wait(&n->changed,&s->mutex));
    }
    dequeue(n,w.thread);
    if(n->phase==PHASE_COMPLETE){
        if(pending)*pending=0;if(context)*context=n->context;
        unlock(s);*last_error=saved;return 1;
    }
    /* Handed the exclusive initialization right after another thread failed. */
    if(pending)*pending=1;
    unlock(s);*last_error=saved;return 1;
}
int32_t mb_win_init_once_complete(mb_win_init_once *o,mb_win_once *p,uint32_t flags,void *context){
    state *s=enter(o);
    uint32_t *last_error=mb_win_threads_last_error(s->domain);
    uint32_t saved=*last_error;
    uint32_t known=MB_WIN_INIT_ONCE_ASYNC|MB_WIN_INIT_ONCE_INIT_FAILED;
    if(flags&~known||(flags&known)==known){unlock(s);*last_error=MB_WIN_ERROR_INVALID_PARAMETER;return 0;}
    sidecar *n=lookup(s,p?&p->opaque:NULL,0,NULL);
    if(!n){unlock(s);*last_error=MB_WIN_ERROR_GEN_FAILURE;return 0;}
    if(n->phase==PHASE_COMPLETE){
        /* Lost an asynchronous race or duplicated completion; consume any
         * leftover asynchronous right of this thread, then fail. */
        for(right **r=&n->rights;*r;r=&(*r)->next)
            if(pthread_equal((*r)->thread,pthread_self())){right *dead=*r;*r=dead->next;free(dead);break;}
        unlock(s);*last_error=MB_WIN_ERROR_ALREADY_EXISTS;return 0;
    }
    if(!holds_right(n)){unlock(s);*last_error=MB_WIN_ERROR_GEN_FAILURE;return 0;}
    if(flags&MB_WIN_INIT_ONCE_INIT_FAILED){
        /* Microsoft: INIT_FAILED takes no context and never combines with
         * ASYNC (rejected above). Hand the right to the first queued waiter
         * or reset the object to never-begun for a later retry. */
        waiter *w=n->queue_first;
        if(w){
            n->phase=PHASE_EXCLUSIVE;
            n->initializer=w->thread;
            must(pthread_cond_broadcast(&n->changed));
            unlock(s);*last_error=saved;return 1;
        }
        destroy_active(s,n,1);
        unlock(s);*last_error=saved;return 1;
    }
    if(n->phase==PHASE_ASYNC)pop_right(n);
    n->phase=PHASE_COMPLETE;
    n->context=context;
    must(pthread_cond_broadcast(&n->changed));
    unlock(s);*last_error=saved;return 1;
}
