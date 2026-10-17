#ifdef MB_WIN_ADDRESS_WAIT_LAYOUT
#include "../mb_win_address_wait_abi.h"
const unsigned long long mb_address_wait_layout[]={sizeof(void*),sizeof(size_t),sizeof(int32_t),sizeof(uint32_t),sizeof(mb_win_address_wait),sizeof(mb_win_address_wait_pin),sizeof(mb_win_address_wait_session),offsetof(mb_win_address_wait_session,module),offsetof(mb_win_address_wait_session,active)};
#else
#define _POSIX_C_SOURCE 200809L
#include "../mb_win_address_wait_abi.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#undef assert
#define assert(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
#define WIN __attribute__((ms_abi))
typedef int32_t (WIN *wait_fn)(volatile void *,void *,size_t,uint32_t);
typedef void (WIN *wake_fn)(void *);
static mb_win_address_wait owner;
static mb_win_thread_domain domain;
static wait_fn wait_address;
static wake_fn wake_one,wake_all;
static atomic_uint hold_comparison,release_comparison,fail_clock,fail_destroy;
static _Thread_local atomic_uint *entered;
static void pause_ms(unsigned n){struct timespec t={n/1000,(long)(n%1000)*1000000};while(nanosleep(&t,&t))assert(errno==EINTR);}
static uint64_t monotonic_ns(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000+(uint64_t)t.tv_nsec;}
static void await(atomic_uint *flag){for(unsigned i=0;i<2000 && !atomic_load(flag);++i)pause_ms(1);assert(atomic_load(flag));}
/* Compile redirects only the service's native calls. The real condition wait
 * still atomically releases its actual mutex; the gate exposes the otherwise
 * tiny compare/queue -> native-sleep interval to a concurrent real waker. */
int mb_address_test_cond_wait(pthread_cond_t *condition,pthread_mutex_t *mutex){
 if(entered)atomic_store(entered,1);
 if(atomic_exchange(&hold_comparison,0))await(&release_comparison);
 return pthread_cond_wait(condition,mutex);
}
int mb_address_test_clock(clockid_t clock,struct timespec *out){if(atomic_exchange(&fail_clock,0)){errno=EIO;return -1;}return clock_gettime(clock,out);}
int mb_address_test_cond_destroy(pthread_cond_t *condition){if(atomic_exchange(&fail_destroy,0))return EBUSY;return pthread_cond_destroy(condition);}
static void *bind(mb_win_address_wait_session *s,const char *name){
 mb_pe_import i={0};i.library=(mb_pe_string){"API-MS-Win-Core-Synch-l1-2-0.dll",32};i.name=(mb_pe_string){name,strlen(name)};
 uint64_t address=0;assert(mb_win_address_wait_bind(s,&i,&address)==MB_PE_BIND_BOUND);return (void *)(uintptr_t)address;
}
struct thread_wait {uint32_t *address;atomic_uint queued,done;uint32_t last;unsigned native;};
static void *waiting(void *arg){
 struct thread_wait *w=arg;assert(mb_win_threads_enter(&domain,NULL)==MB_WIN_THREADS_OK);entered=&w->queued;
 uint32_t compare=0;*mb_win_threads_last_error(&domain)=0x98765432;
 assert((w->native?mb_win_address_wait_wait(&owner,w->address,&compare,4,UINT32_MAX):wait_address(w->address,&compare,4,UINT32_MAX))==1);
 w->last=*mb_win_threads_last_error(&domain);assert(w->last==0x98765432);atomic_store(&w->done,1);
 assert(mb_win_threads_leave(&domain,NULL)==MB_WIN_THREADS_OK);return NULL;
}
struct thread_wake {void *address;atomic_uint started;};
static void *waking(void *arg){struct thread_wake *w=arg;assert(mb_win_threads_enter(&domain,NULL)==MB_WIN_THREADS_OK);atomic_store(&w->started,1);wake_all(w->address);assert(mb_win_threads_leave(&domain,NULL)==MB_WIN_THREADS_OK);return NULL;}
static void *ping_wait(void *arg){
 uint32_t *value=arg;assert(mb_win_threads_enter(&domain,NULL)==MB_WIN_THREADS_OK);
 for(uint32_t i=1;i<=300;++i){uint32_t before=i-1;while(__atomic_load_n(value,__ATOMIC_ACQUIRE)==before)assert(wait_address(value,&before,4,2000));__atomic_store_n(value+1,i,__ATOMIC_RELEASE);wake_all(value+1);}
 assert(mb_win_threads_leave(&domain,NULL)==MB_WIN_THREADS_OK);return NULL;
}
int main(void){
 assert(mb_win_threads_open(&domain,NULL)==MB_WIN_THREADS_OK);
 assert(mb_win_threads_enter(&domain,NULL)==MB_WIN_THREADS_OK);
 assert(mb_win_address_wait_open(&owner,&domain,NULL)==MB_WIN_ADDRESS_WAIT_OK);
 mb_pe_module module={0};mb_win_address_wait_session session={0};
 assert(mb_win_address_wait_begin(&session,&owner,&module)==MB_WIN_ADDRESS_WAIT_OK);module.state=MB_PE_MODULE_ACTIVE;
 wait_address=(wait_fn)bind(&session,"WaitOnAddress");wake_one=(wake_fn)bind(&session,"WakeByAddressSingle");wake_all=(wake_fn)bind(&session,"WakeByAddressAll");
 uint32_t *last=mb_win_threads_last_error(&domain);*last=0xe1234567;*mb_win_threads_errno(&domain)=123;
 uint64_t value=UINT64_C(0xfedcba9876543210),compare=0;
 assert(wait_address(&value,&compare,8,0)==1 && *last==0xe1234567); /* red: real atomic comparison is missing */
#define CHECK_WIDTH(type) do {type scalar=0,old=0;assert(!wait_address(&scalar,&old,sizeof(scalar),0)&&*last==1460); \
 scalar=(type)(UINT64_C(1)<<(sizeof(scalar)*8-1));*last=0x87654321;assert(wait_address(&scalar,&old,sizeof(scalar),0)==1&&*last==0x87654321);}while(0)
 CHECK_WIDTH(uint8_t);CHECK_WIDTH(uint16_t);CHECK_WIDTH(uint32_t);CHECK_WIDTH(uint64_t);
#undef CHECK_WIDTH
 unsigned char unaligned_compare[9]={0};value=0;assert(!wait_address(&value,unaligned_compare+1,8,0)&&*last==1460);
 assert(!wait_address(&value,&compare,3,0)&&*last==87);
 assert(!wait_address(&value,&compare,(size_t)UINT64_C(0x100000004),0)&&*last==87);
 assert(!wait_address((char *)&value+1,&compare,4,0)&&*last==87);
 assert(!wait_address(NULL,&compare,4,0)&&*last==87);
 assert(!wait_address(&value,NULL,4,0)&&*last==87);
 assert(*mb_win_threads_errno(&domain)==123);
 uint64_t before=monotonic_ns();assert(!wait_address(&value,&compare,8,12)&&*last==1460);assert(monotonic_ns()-before>=UINT64_C(12000000));
 atomic_store(&fail_clock,1);assert(!wait_address(&value,&compare,8,10)&&*last==31);
 *last=0x1234;wake_one(NULL);wake_all((void *)(uintptr_t)UINT64_C(0x100000000));assert(*last==0x1234);
 puts("atomic widths, BOOL/SIZE_T ABI, zero/finite timeouts and error isolation passed");

 uint32_t address=0,other=0;struct thread_wait first={.address=&address},second={.address=&address},third={.address=&other};pthread_t a,b,c;
 assert(!pthread_create(&a,NULL,waiting,&first));await(&first.queued);
 assert(!pthread_create(&b,NULL,waiting,&second));await(&second.queued);
 assert(!pthread_create(&c,NULL,waiting,&third));await(&third.queued);
 assert(mb_win_address_wait_close(&owner,NULL)==MB_WIN_ADDRESS_WAIT_BUSY);
 wake_one(&address);await(&first.done);assert(!atomic_load(&second.done)&&!atomic_load(&third.done));
 assert(!pthread_join(a,NULL)); /* TRUE without value change: caller must recheck. */
 wake_all(&address);await(&second.done);assert(!atomic_load(&third.done));assert(!pthread_join(b,NULL));
 wake_all(&other);assert(!pthread_join(c,NULL));assert(address==0&&other==0);
 puts("FIFO single wake, exact address queues and all wake passed");

 struct thread_wait critical={.address=&address};struct thread_wake waker={.address=&address};
 atomic_store(&hold_comparison,1);assert(!pthread_create(&a,NULL,waiting,&critical));await(&critical.queued);
 __atomic_store_n(&address,1,__ATOMIC_RELEASE);assert(!pthread_create(&b,NULL,waking,&waker));await(&waker.started);
 atomic_store(&release_comparison,1);assert(!pthread_join(a,NULL)&&!pthread_join(b,NULL));
 assert(atomic_load(&critical.done));
 uint32_t ping[2]={0,0};assert(!pthread_create(&a,NULL,ping_wait,ping));
 for(uint32_t i=1;i<=300;++i){__atomic_store_n(ping,i,__ATOMIC_RELEASE);wake_all(ping);uint32_t before_value=i-1;while(__atomic_load_n(ping+1,__ATOMIC_ACQUIRE)==before_value)assert(wait_address(ping+1,&before_value,4,2000));}
 assert(!pthread_join(a,NULL));
 puts("compare-to-sleep handoff and 300 bidirectional atomic wake races passed");

 mb_win_address_wait_pin pin={0};assert(mb_win_address_wait_acquire(&owner,&pin)==MB_WIN_ADDRESS_WAIT_OK);
 mb_win_address_wait_pin copied=pin;assert(mb_win_address_wait_release_pin(&copied)==MB_WIN_ADDRESS_WAIT_BAD_STATE);
 mb_win_address_wait_session copy=session;assert(mb_win_address_wait_end(&copy)==MB_WIN_ADDRESS_WAIT_BAD_STATE);
 mb_pe_import miss={0};miss.library=(mb_pe_string){"kernel32.dll",12};miss.name=(mb_pe_string){"WaitOnAddress",13};uint64_t output=123;
 assert(mb_win_address_wait_bind(&session,&miss,&output)==MB_PE_BIND_UNRESOLVED && output==123);
 miss.library=(mb_pe_string){"api-ms-win-core-synch-l1-2-0.dll",32};miss.name=(mb_pe_string){"waitonaddress",13};
 assert(mb_win_address_wait_bind(&session,&miss,&output)==MB_PE_BIND_UNRESOLVED && output==123);
 miss.by_ordinal=1;assert(mb_win_address_wait_bind(&session,&miss,&output)==MB_PE_BIND_UNRESOLVED);
 assert(mb_win_address_wait_bind(&copy,&miss,&output)==MB_PE_BIND_ERROR && output==123);
 assert(mb_win_address_wait_end(&session)==MB_WIN_ADDRESS_WAIT_BUSY);module.state=MB_PE_MODULE_CLEANUP;
 assert(mb_win_address_wait_end(&session)==MB_WIN_ADDRESS_WAIT_BUSY);module.state=MB_PE_MODULE_EMPTY;
 assert(mb_win_address_wait_end(&session)==MB_WIN_ADDRESS_WAIT_OK);
 assert(mb_win_address_wait_close(&owner,NULL)==MB_WIN_ADDRESS_WAIT_BUSY);
 assert(mb_win_address_wait_release_pin(&pin)==MB_WIN_ADDRESS_WAIT_OK);
 struct thread_wait unpinned={.address=&other,.native=1};
 assert(!pthread_create(&a,NULL,waiting,&unpinned));await(&unpinned.queued);
 assert(mb_win_address_wait_close(&owner,NULL)==MB_WIN_ADDRESS_WAIT_BUSY); /* waiter alone retains owner */
 mb_win_address_wait_wake_all(&owner,&other);assert(!pthread_join(a,NULL));
 assert(mb_win_threads_leave(&domain,NULL)==MB_WIN_THREADS_OK);
 assert(mb_win_threads_close(&domain,NULL)==MB_WIN_THREADS_BUSY);
 atomic_store(&fail_destroy,1);int native=0;
 assert(mb_win_address_wait_close(&owner,&native)==MB_WIN_ADDRESS_WAIT_NATIVE_ERROR && native==EBUSY && owner.state);
 assert(mb_win_threads_close(&domain,NULL)==MB_WIN_THREADS_BUSY);
 assert(mb_win_address_wait_close(&owner,&native)==MB_WIN_ADDRESS_WAIT_OK && !owner.state);
 assert(mb_win_threads_close(&domain,NULL)==MB_WIN_THREADS_OK);
 puts("original session/pin ownership and retained native cleanup passed");
 return 0;
}
#endif
