/* VENDORED ADAPTED COPY (origin: pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_init_once.c and mb_win_address_wait.c; those
 * original files are unchanged). */
#include "mb_win_mutex.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

_Static_assert(sizeof(uint64_t)==8&&sizeof(void*)==8,"mutex handle token width");
typedef struct waiter { struct waiter *next; pthread_t thread; int granted; uint32_t granted_code; } waiter;
typedef struct handle_entry { struct handle_entry *next; uint64_t token; struct sidecar *object; } handle_entry;
typedef struct sidecar {
    struct sidecar *next;
    uint64_t id,handles;
    size_t waiters;
    uint16_t *name; size_t name_units; int named;
    int owned,abandoned;
    pthread_t owner;
    waiter *queue_first,*queue_last;
    pthread_cond_t changed;
} sidecar;
typedef struct owned_link { struct owned_link *next; uint64_t object_id; } owned_link;
typedef struct thread_node { struct state *state; owned_link *owned; } thread_node;
typedef struct state {
    pthread_mutex_t mutex;
    mb_win_thread_domain *domain;
    mb_win_thread_pin domain_pin;
    mb_win_mutex_pin *pins;
    sidecar *objects;
    handle_entry *handles;
    size_t count,capacity;
    uint64_t next_token;
    pthread_key_t key; int key_live;
} state;
static void must(int status){if(status)abort();}
static state *get(mb_win_mutex *o){if(!o||!o->state)abort();return o->state;}
static void enter_lock(mb_win_mutex *o,state **out){state *s=get(o);(void)mb_win_threads_last_error(s->domain);must(pthread_mutex_lock(&s->mutex));*out=s;}
static uint32_t *errors(state *s){return mb_win_threads_last_error(s->domain);}
static int monotonic(uint64_t *now){
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;
    *now=(uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
    return 0;
}
static int deadline_from(uint32_t ms,uint64_t *deadline){
    uint64_t now;
    if(monotonic(&now))return -1;
    uint64_t duration=(uint64_t)ms*UINT64_C(1000000);
    if(UINT64_MAX-now<duration)return -1;
    *deadline=now+duration;
    return 0;
}
static int cond_deadline_wait(pthread_cond_t *cond,pthread_mutex_t *mutex,uint64_t deadline){
#if defined(__APPLE__)
    uint64_t now;
    if(monotonic(&now))return -1;
    if(now>=deadline)return ETIMEDOUT;
    uint64_t remaining=deadline-now;
    struct timespec relative={(time_t)(remaining/UINT64_C(1000000000)),(long)(remaining%UINT64_C(1000000000))};
    return pthread_cond_timedwait_relative_np(cond,mutex,&relative);
#else
    struct timespec absolute={(time_t)(deadline/UINT64_C(1000000000)),(long)(deadline%UINT64_C(1000000000))};
    return pthread_cond_timedwait(cond,mutex,&absolute);
#endif
}
static int init_cond(pthread_cond_t *cond){
    pthread_condattr_t attribute;
    int rc=pthread_condattr_init(&attribute);
#if !defined(__APPLE__)
    if(!rc)rc=pthread_condattr_setclock(&attribute,CLOCK_MONOTONIC);
#endif
    if(!rc)rc=pthread_cond_init(cond,&attribute);
    pthread_condattr_destroy(&attribute);
    return rc;
}
static handle_entry *find_handle(state *s,uint64_t token){
    for(handle_entry *e=s->handles;e;e=e->next)if(e->token==token)return e;
    return NULL;
}
static sidecar *find_object(state *s,uint64_t id){
    for(sidecar *n=s->objects;n;n=n->next)if(n->id==id)return n;
    return NULL;
}
/* Windows object-namespace name handling, adapter-scoped: ASCII-only
 * case folding (A-Z lowered; other code units verbatim), optional exact
 * folded "Local\" prefix stripped into the single private namespace, any
 * other backslash refused, MAX_PATH (260) unit bound. NULL or empty name
 * is unnamed for creation. */
static uint16_t fold_unit(uint16_t u){return (uint16_t)((u>=(uint16_t)'A'&&u<=(uint16_t)'Z')?(u+('a'-'A')):u);}
static uint32_t parse_name(const uint16_t *name,uint16_t **folded_out,size_t *units_out,int *unnamed){
    *folded_out=NULL;*units_out=0;
    if(!name||!name[0]){*unnamed=1;return 0;}
    *unnamed=0;
    size_t units=0;
    while(name[units]&&units<=(size_t)MB_WIN_MUTEX_MAX_NAME)++units;
    if(units>(size_t)MB_WIN_MUTEX_MAX_NAME)return MB_WIN_ERROR_FILENAME_EXCED_RANGE;
    uint16_t *folded=malloc(units*sizeof(*folded));
    if(!folded)return MB_WIN_ERROR_NOT_ENOUGH_MEMORY;
    for(size_t i=0;i<units;++i)folded[i]=fold_unit(name[i]);
    static const uint16_t prefix[6]={'l','o','c','a','l',(uint16_t)'\\'};
    size_t start=0;
    if(units>6)for(start=0;start<6;++start)if(folded[start]!=prefix[start])break;
    if(start==6)start=6;else start=0;
    for(size_t i=start;i<units;++i)if(folded[i]==(uint16_t)'\\'){free(folded);return MB_WIN_ERROR_INVALID_PARAMETER;}
    if(start){memmove(folded,folded+start,(units-start)*sizeof(*folded));units-=start;}
    if(!units){free(folded);*unnamed=1;return 0;}
    *folded_out=folded;*units_out=units;
    return 0;
}
static sidecar *lookup_named(state *s,const uint16_t *folded,size_t units){
    for(sidecar *n=s->objects;n;n=n->next)
        if(n->named&&n->name_units==units){
            size_t i=0;
            while(i<units&&n->name[i]==folded[i])++i;
            if(i==units)return n;
        }
    return NULL;
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
    w->next=NULL;
}
/* Ownership adoption registers the sidecar in the calling thread's private
 * owned-object registry so native thread exit can abandon it. */
static int adopt(state *s,sidecar *n){
    thread_node *node=pthread_getspecific(s->key);
    if(!node){
        node=calloc(1,sizeof(*node));
        if(!node)return 0;
        if(pthread_setspecific(s->key,node)){free(node);return 0;}
        node->state=s;
    }
    owned_link *link=malloc(sizeof(*link));
    if(!link)return 0;
    link->object_id=n->id;link->next=node->owned;node->owned=link;
    return 1;
}
static void unadopt(state *s,sidecar *n){
    thread_node *node=pthread_getspecific(s->key);
    if(!node)return;
    owned_link **p=&node->owned;
    while(*p&&(*p)->object_id!=n->id)p=&(*p)->next;
    if(*p){owned_link *dead=*p;*p=dead->next;free(dead);}
}
static void grant_first(state *s,sidecar *n,uint32_t code){
    (void)s;
    waiter *w=n->queue_first;
    if(!w)return;
    dequeue(n,w->thread);
    n->owned=1;n->abandoned=0;n->owner=w->thread;
    /* granted flag is separate from the code: WAIT_OBJECT_0 is zero. */
    w->granted=1;w->granted_code=code;
    must(pthread_cond_broadcast(&n->changed));
}
static void destroy_object(state *s,sidecar *n){
    if(n->queue_first||n->waiters)abort();
    sidecar **p=&s->objects;
    while(*p&&*p!=n)p=&(*p)->next;
    if(!*p)abort();
    *p=n->next;--s->count;
    free(n->name);
    must(pthread_cond_destroy(&n->changed));
    free(n);
}
static uint64_t new_handle(state *s,sidecar *n){
    if(s->next_token==UINT64_MAX)return 0;
    handle_entry *e=malloc(sizeof(*e));
    if(!e)return 0;
    e->token=++s->next_token;e->object=n;e->next=s->handles;s->handles=e;++n->handles;
    return e->token;
}
/* unused-tolerant: the red-stub control replaces owner open (the only
 * registrar) and must still compile. */
__attribute__((unused)) static void thread_exited(void *value){
    /* Runs on the exiting thread: every mutex still owned by it becomes
     * abandoned and the first queued waiter receives WAIT_ABANDONED plus
     * ownership. No thread-domain last-error use here: the domain may have
     * released this thread's registration first. */
    thread_node *node=value;
    if(!node)return;
    state *s=node->state;
    pthread_t self=pthread_self();
    must(pthread_mutex_lock(&s->mutex));
    owned_link *link=node->owned;
    while(link){
        owned_link *next=link->next;
        sidecar *n=find_object(s,link->object_id);
        if(n&&n->owned&&pthread_equal(n->owner,self)){
            n->owned=0;n->abandoned=1;
            grant_first(s,n,MB_WIN_WAIT_ABANDONED);
        }
        free(link);link=next;
    }
    free(node);
    must(pthread_mutex_unlock(&s->mutex));
}
mb_win_mutex_status mb_win_mutex_open(mb_win_mutex *o,const mb_win_mutex_config *c,int *error){(void)o;(void)c;(void)error;return MB_WIN_MUTEX_BAD_STATE;}
mb_win_mutex_status mb_win_mutex_close(mb_win_mutex *o,int *error){
    if(!o)return MB_WIN_MUTEX_INVALID_ARGUMENT;
    if(!o->state)return MB_WIN_MUTEX_BAD_STATE;
    state *s=o->state;
    must(pthread_mutex_lock(&s->mutex));
    if(s->pins){must(pthread_mutex_unlock(&s->mutex));return MB_WIN_MUTEX_BUSY;}
    if(s->handles){must(pthread_mutex_unlock(&s->mutex));return MB_WIN_MUTEX_BUSY;}
    for(sidecar *n=s->objects;n;n=n->next)
        if(n->waiters){must(pthread_mutex_unlock(&s->mutex));return MB_WIN_MUTEX_BUSY;}
    while(s->objects)destroy_object(s,s->objects);
    must(pthread_mutex_unlock(&s->mutex));
    int rc=pthread_key_delete(s->key);
    if(rc){if(error)*error=rc;return MB_WIN_MUTEX_NATIVE_ERROR;}
    s->key_live=0;
    rc=pthread_mutex_destroy(&s->mutex);
    if(rc){if(error)*error=rc;return MB_WIN_MUTEX_NATIVE_ERROR;}
    if(mb_win_threads_release_pin(&s->domain_pin)!=MB_WIN_THREADS_OK)abort();
    free(s);o->state=NULL;
    if(error)*error=0;
    return MB_WIN_MUTEX_OK;
}
mb_win_mutex_status mb_win_mutex_acquire_pin(mb_win_mutex *o,mb_win_mutex_pin *p){
    if(!o||!p)return MB_WIN_MUTEX_INVALID_ARGUMENT;
    if(!o->state||p->owner||p->next)return MB_WIN_MUTEX_BAD_STATE;
    state *s=o->state;
    must(pthread_mutex_lock(&s->mutex));
    for(mb_win_mutex_pin *n=s->pins;n;n=n->next)if(n==p){must(pthread_mutex_unlock(&s->mutex));return MB_WIN_MUTEX_BAD_STATE;}
    p->owner=o;p->next=s->pins;s->pins=p;
    must(pthread_mutex_unlock(&s->mutex));
    return MB_WIN_MUTEX_OK;
}
mb_win_mutex_status mb_win_mutex_release_pin(mb_win_mutex_pin *p){
    if(!p)return MB_WIN_MUTEX_INVALID_ARGUMENT;
    if(!p->owner||!p->owner->state)return MB_WIN_MUTEX_BAD_STATE;
    state *s=p->owner->state;
    must(pthread_mutex_lock(&s->mutex));
    mb_win_mutex_pin **at=&s->pins;
    while(*at&&*at!=p)at=&(*at)->next;
    if(!*at){must(pthread_mutex_unlock(&s->mutex));return MB_WIN_MUTEX_BAD_STATE;}
    *at=p->next;*p=(mb_win_mutex_pin){0};
    must(pthread_mutex_unlock(&s->mutex));
    return MB_WIN_MUTEX_OK;
}
mb_win_thread_domain *mb_win_mutex_domain(mb_win_mutex *o){return o&&o->state?((state*)o->state)->domain:NULL;}
uint64_t mb_win_mutex_create(mb_win_mutex *o,const uint16_t *name,uint32_t flags){
    state *s;
    enter_lock(o,&s);
    uint32_t *last_error=errors(s),saved=*last_error;
    if(flags&~(uint32_t)MB_WIN_MUTEX_INITIAL_OWNER){must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_INVALID_PARAMETER;return 0;}
    uint16_t *folded=NULL;size_t units=0;int unnamed=0;
    uint32_t name_error=parse_name(name,&folded,&units,&unnamed);
    if(name_error){must(pthread_mutex_unlock(&s->mutex));*last_error=name_error;return 0;}
    if(!unnamed){
        sidecar *existing=lookup_named(s,folded,units);
        if(existing){
            /* Microsoft: a named object that already exists returns a new
             * handle to the SAME object, succeeds, sets ERROR_ALREADY_EXISTS,
             * and ignores the initial-owner request. */
            free(folded);
            uint64_t token=new_handle(s,existing);
            if(!token){must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return 0;}
            must(pthread_mutex_unlock(&s->mutex));
            *last_error=MB_WIN_ERROR_ALREADY_EXISTS;
            return token;
        }
    }
    if(s->count==s->capacity||s->next_token==UINT64_MAX){free(folded);must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return 0;}
    sidecar *n=calloc(1,sizeof(*n));
    if(!n){free(folded);must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return 0;}
    int rc=init_cond(&n->changed);
    if(rc){free(n);free(folded);must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return 0;}
    n->id=++s->next_token;
    if(!unnamed){n->named=1;n->name=folded;n->name_units=units;}
    else free(folded);
    n->next=s->objects;s->objects=n;++s->count;
    uint64_t token=new_handle(s,n);
    if(!token){destroy_object(s,n);must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return 0;}
    if(flags&MB_WIN_MUTEX_INITIAL_OWNER){
        n->owned=1;n->owner=pthread_self();
        if(!adopt(s,n)){
            n->owned=0;
            handle_entry *e=find_handle(s,token);
            s->handles=e->next;free(e);--n->handles;
            destroy_object(s,n);
            must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return 0;
        }
    }
    must(pthread_mutex_unlock(&s->mutex));
    *last_error=saved;
    return token;
}
uint64_t mb_win_mutex_open_named(mb_win_mutex *o,const uint16_t *name){
    state *s;
    enter_lock(o,&s);
    uint32_t *last_error=errors(s),saved=*last_error;
    uint16_t *folded=NULL;size_t units=0;int unnamed=0;
    uint32_t name_error=parse_name(name,&folded,&units,&unnamed);
    if(name_error||unnamed){
        free(folded);
        must(pthread_mutex_unlock(&s->mutex));
        *last_error=name_error?name_error:MB_WIN_ERROR_INVALID_PARAMETER;
        return 0;
    }
    sidecar *n=lookup_named(s,folded,units);
    free(folded);
    if(!n){must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_FILE_NOT_FOUND;return 0;}
    uint64_t token=new_handle(s,n);
    if(!token){must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return 0;}
    must(pthread_mutex_unlock(&s->mutex));
    *last_error=saved;
    return token;
}
int32_t mb_win_mutex_release(mb_win_mutex *o,uint64_t handle){
    state *s;
    enter_lock(o,&s);
    uint32_t *last_error=errors(s),saved=*last_error;
    handle_entry *e=find_handle(s,handle);
    if(!e){must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_INVALID_HANDLE;return 0;}
    sidecar *n=e->object;
    if(!n->owned||!pthread_equal(n->owner,pthread_self())){must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_NOT_OWNER;return 0;}
    unadopt(s,n);
    n->owned=0;
    grant_first(s,n,MB_WIN_WAIT_OBJECT_0);
    must(pthread_mutex_unlock(&s->mutex));
    *last_error=saved;
    return 1;
}
uint32_t mb_win_mutex_wait(mb_win_mutex *o,uint64_t handle,uint32_t milliseconds){
    return mb_win_mutex_wait_ex(o,handle,milliseconds,0);
}
uint32_t mb_win_mutex_wait_ex(mb_win_mutex *o,uint64_t handle,uint32_t milliseconds,int32_t alertable){
    state *s;
    enter_lock(o,&s);
    uint32_t *last_error=errors(s),saved=*last_error;
    handle_entry *e=find_handle(s,handle);
    if(!e){must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_INVALID_HANDLE;return MB_WIN_WAIT_FAILED;}
    if(alertable){
        /* Alertable waits deliver queued APCs on Windows; no APC surface is
         * implemented by this family, so refuse instead of faking success. */
        must(pthread_mutex_unlock(&s->mutex));
        *last_error=MB_WIN_ERROR_NOT_SUPPORTED;
        return MB_WIN_WAIT_FAILED;
    }
    sidecar *n=e->object;
    if(n->owned&&pthread_equal(n->owner,pthread_self())){
        /* Windows mutexes are not recursive. The owner re-waiting can never
         * be satisfied (only the owner can release), so Windows deadlocks:
         * finite timeouts follow the real timeout path, an infinite timeout
         * is refused explicitly instead of hanging this process. */
        if(milliseconds==MB_WIN_WAIT_INFINITE){must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_POSSIBLE_DEADLOCK;return MB_WIN_WAIT_FAILED;}
        uint64_t deadline=0;
        if(deadline_from(milliseconds,&deadline)){must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_INVALID_PARAMETER;return MB_WIN_WAIT_FAILED;}
        for(;;){
            uint64_t now;
            if(monotonic(&now)){must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_INVALID_PARAMETER;return MB_WIN_WAIT_FAILED;}
            if(now>=deadline)break;
            int rc=cond_deadline_wait(&n->changed,&s->mutex,deadline);
            if(rc&&rc!=ETIMEDOUT){must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_INVALID_PARAMETER;return MB_WIN_WAIT_FAILED;}
        }
        must(pthread_mutex_unlock(&s->mutex));
        *last_error=saved;
        return MB_WIN_WAIT_TIMEOUT;
    }
    if(!n->owned){
        uint32_t code=n->abandoned?MB_WIN_WAIT_ABANDONED:MB_WIN_WAIT_OBJECT_0;
        n->owned=1;n->abandoned=0;n->owner=pthread_self();
        if(!adopt(s,n)){
            n->owned=0;
            grant_first(s,n,code);
            must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return MB_WIN_WAIT_FAILED;
        }
        must(pthread_mutex_unlock(&s->mutex));
        *last_error=saved;
        return code;
    }
    uint64_t deadline=0;
    if(milliseconds!=MB_WIN_WAIT_INFINITE&&deadline_from(milliseconds,&deadline)){must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_INVALID_PARAMETER;return MB_WIN_WAIT_FAILED;}
    waiter w={NULL,pthread_self(),0,0};
    enqueue(n,&w);++n->waiters;
    for(;;){
        if(w.granted)break;
        if(milliseconds==MB_WIN_WAIT_INFINITE){
            must(pthread_cond_wait(&n->changed,&s->mutex));
            continue;
        }
        uint64_t now;
        if(monotonic(&now)||now>=deadline){
            dequeue(n,w.thread);--n->waiters;
            must(pthread_mutex_unlock(&s->mutex));
            *last_error=saved;
            return MB_WIN_WAIT_TIMEOUT;
        }
        int rc=cond_deadline_wait(&n->changed,&s->mutex,deadline);
        if(rc&&rc!=ETIMEDOUT){
            dequeue(n,w.thread);--n->waiters;
            must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_INVALID_PARAMETER;return MB_WIN_WAIT_FAILED;
        }
    }
    --n->waiters;
    uint32_t code=w.granted_code;
    if(!adopt(s,n)){
        /* Registration failed: hand the granted ownership onward. */
        n->owned=0;
        if(code==MB_WIN_WAIT_ABANDONED)n->abandoned=1;
        grant_first(s,n,code);
        must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_NOT_ENOUGH_MEMORY;return MB_WIN_WAIT_FAILED;
    }
    must(pthread_mutex_unlock(&s->mutex));
    *last_error=saved;
    return code;
}
int32_t mb_win_mutex_close_handle(mb_win_mutex *o,uint64_t handle){
    state *s;
    enter_lock(o,&s);
    uint32_t *last_error=errors(s),saved=*last_error;
    handle_entry **at=&s->handles;
    while(*at&&(*at)->token!=handle)at=&(*at)->next;
    if(!*at){must(pthread_mutex_unlock(&s->mutex));*last_error=MB_WIN_ERROR_INVALID_HANDLE;return 0;}
    handle_entry *dead=*at;*at=dead->next;
    sidecar *n=dead->object;--n->handles;free(dead);
    /* Object destruction policy: the object lives while any handle or any
     * blocked waiter references it; destroying an owned object drops that
     * ownership, matching the object vanishing on Windows. */
    if(!n->handles&&!n->waiters)destroy_object(s,n);
    must(pthread_mutex_unlock(&s->mutex));
    *last_error=saved;
    return 1;
}
int32_t mb_win_mutex_is_handle(mb_win_mutex *o,uint64_t handle){
    if(!o||!o->state)return 0;
    state *s=o->state;
    must(pthread_mutex_lock(&s->mutex));
    int32_t found=find_handle(s,handle)!=NULL;
    must(pthread_mutex_unlock(&s->mutex));
    return found;
}
