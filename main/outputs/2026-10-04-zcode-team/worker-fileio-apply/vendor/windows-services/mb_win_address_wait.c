#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#include "mb_win_address_wait.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#if !defined(__x86_64__)
#error "Address-wait scalar contract requires AMD64"
#endif
typedef struct waiter {
    struct waiter *next;
    volatile void *address;
    unsigned signaled;
} waiter;
typedef struct address_owner {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_condattr_t attribute;
    mb_win_thread_domain *domain;
    mb_win_thread_pin domain_pin;
    mb_win_address_wait_pin *pins;
    waiter *first,*last;
    size_t wait_count,pin_count;
    unsigned mutex_live,condition_live,attribute_live,closing;
} address_owner;
static void lock(address_owner *d){if(pthread_mutex_lock(&d->mutex))abort();}
static void unlock(address_owner *d){if(pthread_mutex_unlock(&d->mutex))abort();}
static address_owner *required(mb_win_address_wait *o){if(!o || !o->state)abort();address_owner *d=o->state;if(d->closing)abort();return d;}
static mb_win_address_wait_status native_result(int *out,int rc){if(out)*out=rc;return rc?MB_WIN_ADDRESS_WAIT_NATIVE_ERROR:MB_WIN_ADDRESS_WAIT_OK;}
mb_win_address_wait_status mb_win_address_wait_open(mb_win_address_wait *o,mb_win_thread_domain *domain,int *error){
 if(!o || !domain)return MB_WIN_ADDRESS_WAIT_INVALID_ARGUMENT;
 if(o->state)return MB_WIN_ADDRESS_WAIT_BAD_STATE;
 address_owner *d=calloc(1,sizeof(*d));if(!d)return native_result(error,ENOMEM);
 d->domain=domain;
 if(mb_win_threads_acquire(domain,&d->domain_pin)!=MB_WIN_THREADS_OK){free(d);return MB_WIN_ADDRESS_WAIT_OWNER_ERROR;}
 int rc=pthread_mutex_init(&d->mutex,NULL);
 if(!rc){d->mutex_live=1;rc=pthread_condattr_init(&d->attribute);}
 if(!rc){d->attribute_live=1;
#if !defined(__APPLE__)
  rc=pthread_condattr_setclock(&d->attribute,CLOCK_MONOTONIC);
#endif
 }
 if(!rc)rc=pthread_cond_init(&d->condition,&d->attribute);
 if(!rc){d->condition_live=1;rc=pthread_condattr_destroy(&d->attribute);if(!rc)d->attribute_live=0;}
 o->state=d;
 if(rc){d->closing=1;(void)mb_win_address_wait_close(o,NULL);return native_result(error,rc);}
 return native_result(error,0);
}
mb_win_address_wait_status mb_win_address_wait_close(mb_win_address_wait *o,int *error){
 if(!o)return MB_WIN_ADDRESS_WAIT_INVALID_ARGUMENT;if(!o->state)return native_result(error,0);
 address_owner *d=o->state;
 if(d->mutex_live){lock(d);if(d->wait_count || d->pin_count){unlock(d);return MB_WIN_ADDRESS_WAIT_BUSY;}d->closing=1;unlock(d);}
 if(d->condition_live){int rc=pthread_cond_destroy(&d->condition);if(rc)return native_result(error,rc);d->condition_live=0;}
 if(d->attribute_live){int rc=pthread_condattr_destroy(&d->attribute);if(rc)return native_result(error,rc);d->attribute_live=0;}
 if(d->mutex_live){int rc=pthread_mutex_destroy(&d->mutex);if(rc)return native_result(error,rc);d->mutex_live=0;}
 if(mb_win_threads_release_pin(&d->domain_pin)!=MB_WIN_THREADS_OK)return MB_WIN_ADDRESS_WAIT_OWNER_ERROR;
 free(d);o->state=NULL;return native_result(error,0);
}
mb_win_address_wait_status mb_win_address_wait_acquire(mb_win_address_wait *o,mb_win_address_wait_pin *p){
 if(!o || !p)return MB_WIN_ADDRESS_WAIT_INVALID_ARGUMENT;if(!o->state)return MB_WIN_ADDRESS_WAIT_BAD_STATE;
 address_owner *d=o->state;if(d->closing)return MB_WIN_ADDRESS_WAIT_BAD_STATE;
 lock(d);if(p->owner || p->next || d->pin_count==SIZE_MAX){unlock(d);return MB_WIN_ADDRESS_WAIT_BAD_STATE;}
 p->owner=o;p->next=d->pins;d->pins=p;++d->pin_count;unlock(d);return MB_WIN_ADDRESS_WAIT_OK;
}
mb_win_address_wait_status mb_win_address_wait_release_pin(mb_win_address_wait_pin *p){
 if(!p)return MB_WIN_ADDRESS_WAIT_INVALID_ARGUMENT;if(!p->owner || !p->owner->state)return MB_WIN_ADDRESS_WAIT_BAD_STATE;
 address_owner *d=p->owner->state;lock(d);mb_win_address_wait_pin **at=&d->pins;
 while(*at && *at!=p)at=&(*at)->next;
 if(!*at || !d->pin_count){unlock(d);return MB_WIN_ADDRESS_WAIT_BAD_STATE;}
 *at=p->next;--d->pin_count;*p=(mb_win_address_wait_pin){0};unlock(d);return MB_WIN_ADDRESS_WAIT_OK;
}
static uint64_t atomic_value(volatile void *p,size_t size){
 switch(size){
 case 1:return __atomic_load_n((volatile uint8_t *)p,__ATOMIC_ACQUIRE);
 case 2:return __atomic_load_n((volatile uint16_t *)p,__ATOMIC_ACQUIRE);
 case 4:return __atomic_load_n((volatile uint32_t *)p,__ATOMIC_ACQUIRE);
 default:return __atomic_load_n((volatile uint64_t *)p,__ATOMIC_ACQUIRE);
 }
}
static int monotonic(uint64_t *value){
 struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;
 if(t.tv_sec<0 || t.tv_nsec<0 || t.tv_nsec>=1000000000L || (uint64_t)t.tv_sec>UINT64_MAX/UINT64_C(1000000000))return -1;
 uint64_t seconds=(uint64_t)t.tv_sec*UINT64_C(1000000000);if(UINT64_MAX-seconds<(uint64_t)t.tv_nsec)return -1;
 *value=seconds+(uint64_t)t.tv_nsec;return 0;
}
static void remove_waiter(address_owner *d,waiter *w){
 waiter **at=&d->first,*previous=NULL;while(*at!=w){previous=*at;at=&(*at)->next;}
 *at=w->next;if(d->last==w)d->last=previous;--d->wait_count;
}
int32_t mb_win_address_wait_wait(mb_win_address_wait *o,volatile void *address,const void *compare,size_t size,uint32_t millis){
 address_owner *d=required(o);uint32_t *error=mb_win_threads_last_error(d->domain),saved=*error;
 if((size!=1 && size!=2 && size!=4 && size!=8) || !address || !compare || ((uintptr_t)address&(size-1))){*error=87;return 0;}
 uint64_t expected=0;const unsigned char *bytes=compare;
 for(size_t i=0;i<size;++i)expected|=(uint64_t)bytes[i]<<(i*8);
 lock(d);
 if(atomic_value(address,size)!=expected){unlock(d);*error=saved;return 1;}
 if(!millis){unlock(d);*error=MB_WIN_ADDRESS_WAIT_TIMEOUT;return 0;}
 uint64_t deadline=0,now=0;
 if(millis!=MB_WIN_ADDRESS_WAIT_INFINITE){
  uint64_t duration=(uint64_t)millis*UINT64_C(1000000);
  if(monotonic(&now) || UINT64_MAX-now<duration){unlock(d);*error=31;return 0;}deadline=now+duration;
 }
 if(d->wait_count==SIZE_MAX){unlock(d);*error=8;return 0;}
 waiter w={.address=address};if(d->last)d->last->next=&w;else d->first=&w;d->last=&w;++d->wait_count;
 uint32_t failure=0;
 while(!w.signaled){
  int rc;
  if(millis==MB_WIN_ADDRESS_WAIT_INFINITE)rc=pthread_cond_wait(&d->condition,&d->mutex);
  else {
   if(monotonic(&now)){failure=31;break;}
   if(now>=deadline){failure=MB_WIN_ADDRESS_WAIT_TIMEOUT;break;}
#if defined(__APPLE__)
   uint64_t remaining=deadline-now;
   struct timespec time={(time_t)(remaining/UINT64_C(1000000000)),(long)(remaining%UINT64_C(1000000000))};
   rc=pthread_cond_timedwait_relative_np(&d->condition,&d->mutex,&time);
#else
   struct timespec time={(time_t)(deadline/UINT64_C(1000000000)),(long)(deadline%UINT64_C(1000000000))};
   rc=pthread_cond_timedwait(&d->condition,&d->mutex,&time);
#endif
  }
  if(rc && rc!=ETIMEDOUT){failure=31;break;}
 }
 remove_waiter(d,&w);unlock(d);*error=failure?failure:saved;return failure?0:1;
}
static void wake(mb_win_address_wait *o,void *address,int all){
 address_owner *d=required(o);uint32_t *error=mb_win_threads_last_error(d->domain),saved=*error;
 lock(d);unsigned signaled=0;
 for(waiter *w=d->first;w;w=w->next)if(w->address==address && !w->signaled){w->signaled=1;signaled=1;if(!all)break;}
 /* A shared condition cannot use signal: it might choose another address.
  * Only the explicitly marked record(s) return after this broadcast. */
 if(signaled && pthread_cond_broadcast(&d->condition))abort();
 unlock(d);*error=saved;
}
void mb_win_address_wait_wake_single(mb_win_address_wait *o,void *address){wake(o,address,0);}
void mb_win_address_wait_wake_all(mb_win_address_wait *o,void *address){wake(o,address,1);}
