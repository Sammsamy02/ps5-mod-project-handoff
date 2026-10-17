#ifdef MB_WIN_CRITICAL_LAYOUT
#include "../mb_win_critical_abi.h"
const unsigned long long mb_critical_layout[]={sizeof(void*),sizeof(uint32_t),sizeof(mb_win_critical_storage),_Alignof(mb_win_critical_storage),sizeof(mb_win_critical),sizeof(mb_win_critical_pin),sizeof(mb_win_critical_session),offsetof(mb_win_critical_session,module)};
#else
#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "../mb_win_critical_abi.h"
#include <pthread.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define WIN __attribute__((ms_abi))
typedef void(WIN *one_fn)(void*);
typedef int32_t(WIN *try_fn)(void*);
typedef int32_t(WIN *init_fn)(void*,uint32_t);
typedef uint32_t(WIN *spin_fn)(void*,uint32_t);
#ifdef MB_WIN_CRITICAL_FAULTS
static int fake_processors,fail_destroy;
long critical_test_sysconf(int name){if(fake_processors<0){errno=EIO;return -1;}return fake_processors?fake_processors:sysconf(name);}
int critical_test_destroy(pthread_cond_t *condition){if(fail_destroy){fail_destroy=0;return EBUSY;}return pthread_cond_destroy(condition);}
#endif
static mb_win_critical owner;static mb_win_thread_domain domain;
static struct {uint64_t before;mb_win_critical_storage cs;uint64_t after;} guarded;
static one_fn enter,leave;static try_fn attempt;static atomic_uint stage;static unsigned counter;
static void *resolve(mb_win_critical_session *s,const char *name){
 mb_pe_import i={0};i.library=(mb_pe_string){"KeRnEl32.DlL",12};i.name=(mb_pe_string){name,strlen(name)};uint64_t address=0;
 CHECK(mb_win_critical_bind(s,&i,&address)==MB_PE_BIND_BOUND);return (void*)(uintptr_t)address;
}
static void *contender(void *arg){
 CHECK(mb_win_threads_enter(&domain,NULL)==MB_WIN_THREADS_OK);uint32_t *error=mb_win_threads_last_error(&domain);*error=0xaabbccdd;
 if(arg){CHECK(!attempt(&guarded.cs));CHECK(*error==0xaabbccdd);CHECK(mb_win_critical_leave(&owner,&guarded.cs)==MB_WIN_CRITICAL_BAD_STATE);atomic_store(&stage,1);enter(&guarded.cs);++counter;atomic_store(&stage,2);leave(&guarded.cs);}
 else for(unsigned i=0;i<4000;++i){enter(&guarded.cs);++counter;leave(&guarded.cs);}
 CHECK(*error==0xaabbccdd);CHECK(mb_win_threads_leave(&domain,NULL)==MB_WIN_THREADS_OK);return NULL;
}
int main(void){
 CHECK(sizeof(mb_win_critical_storage)==40 && _Alignof(mb_win_critical_storage)==8);
 CHECK(mb_win_threads_open(&domain,NULL)==MB_WIN_THREADS_OK);CHECK(mb_win_threads_enter(&domain,NULL)==MB_WIN_THREADS_OK);
 CHECK(mb_win_critical_open(&owner,&domain,2,NULL)==MB_WIN_CRITICAL_OK);
 mb_pe_module module={0};mb_win_critical_session session={0};CHECK(mb_win_critical_begin(&session,&owner,&module)==MB_WIN_CRITICAL_OK);module.state=MB_PE_MODULE_ACTIVE;
 one_fn initialize=(one_fn)resolve(&session,"InitializeCriticalSection"),destroy=(one_fn)resolve(&session,"DeleteCriticalSection");
 enter=(one_fn)resolve(&session,"EnterCriticalSection");leave=(one_fn)resolve(&session,"LeaveCriticalSection");attempt=(try_fn)resolve(&session,"TryEnterCriticalSection");
 init_fn init_spin=(init_fn)resolve(&session,"InitializeCriticalSectionAndSpinCount");spin_fn set_spin=(spin_fn)resolve(&session,"SetCriticalSectionSpinCount");
 guarded.before=UINT64_C(0x1122334455667788);guarded.after=UINT64_C(0x8877665544332211);memset(&guarded.cs,0xa5,40);
 uint32_t *error=mb_win_threads_last_error(&domain);*error=0x12345678;
 CHECK(init_spin(&guarded.cs,32));CHECK(*error==0x12345678);CHECK(guarded.before==UINT64_C(0x1122334455667788)&&guarded.after==UINT64_C(0x8877665544332211));
 CHECK(set_spin(&guarded.cs,64)==(sysconf(_SC_NPROCESSORS_ONLN)>1?32:0));CHECK(set_spin(&guarded.cs,0)==(sysconf(_SC_NPROCESSORS_ONLN)>1?64:0));
 CHECK(set_spin(&guarded.cs,32)==0);enter(&guarded.cs);CHECK(attempt(&guarded.cs));pthread_t t;CHECK(!pthread_create(&t,NULL,contender,&owner));
 while(!atomic_load(&stage))sched_yield();
 CHECK(mb_win_critical_delete(&owner,&guarded.cs)==MB_WIN_CRITICAL_BUSY);CHECK(mb_win_critical_close(&owner,NULL)==MB_WIN_CRITICAL_BUSY);
 leave(&guarded.cs);CHECK(atomic_load(&stage)==1);leave(&guarded.cs);CHECK(!pthread_join(t,NULL));CHECK(counter==1&&atomic_load(&stage)==2);
 CHECK(*error==0x12345678);puts("critical Win64 widths, recursion, TryEnter, contention and LastError passed");
 pthread_t a,b;CHECK(!pthread_create(&a,NULL,contender,NULL));CHECK(!pthread_create(&b,NULL,contender,NULL));CHECK(!pthread_join(a,NULL)&&!pthread_join(b,NULL));CHECK(counter==8001);
 puts("critical real pthread mutual exclusion passed");
 mb_win_critical_storage second,third;memset(&third,0xa5,sizeof(third));initialize(&second);CHECK(!init_spin(&third,0)&&*error==8);for(size_t i=0;i<sizeof(third);++i)CHECK(((unsigned char*)&third)[i]==0xa5);
 CHECK(mb_win_critical_initialize(&owner,&guarded.cs,0)==MB_WIN_CRITICAL_BAD_STATE);destroy(&second);initialize(&second);destroy(&second);destroy(&guarded.cs);
 CHECK(guarded.before==UINT64_C(0x1122334455667788)&&guarded.after==UINT64_C(0x8877665544332211));
 int32_t sentinel=99;CHECK(mb_win_critical_try_enter(&owner,&guarded.cs,&sentinel)==MB_WIN_CRITICAL_BAD_STATE&&sentinel==99);
 CHECK(mb_win_critical_initialize(&owner,(char*)&third+1,0)==MB_WIN_CRITICAL_INVALID_ARGUMENT);
 mb_pe_import wrong={0};wrong.library=(mb_pe_string){"kernel32.dll",12};wrong.name=(mb_pe_string){"entercriticalsection",20};uint64_t address=123;CHECK(mb_win_critical_bind(&session,&wrong,&address)==MB_PE_BIND_UNRESOLVED&&address==123);
 wrong.name=(mb_pe_string){"EnterCriticalSection",20};wrong.by_ordinal=1;CHECK(mb_win_critical_bind(&session,&wrong,&address)==MB_PE_BIND_UNRESOLVED);
 CHECK(mb_win_critical_end(&session)==MB_WIN_CRITICAL_BUSY);module.state=MB_PE_MODULE_EMPTY;CHECK(mb_win_critical_end(&session)==MB_WIN_CRITICAL_OK);
 /* Teardown intentionally does not dereference retired idle Windows storage. */
 mb_win_critical_storage *retired=malloc(sizeof(*retired));CHECK(retired);CHECK(mb_win_critical_initialize(&owner,retired,0)==MB_WIN_CRITICAL_OK);free(retired);
 CHECK(mb_win_critical_close(&owner,NULL)==MB_WIN_CRITICAL_OK);CHECK(!owner.state);
 CHECK(mb_win_threads_leave(&domain,NULL)==MB_WIN_THREADS_OK);CHECK(mb_win_threads_close(&domain,NULL)==MB_WIN_THREADS_OK);
 puts("critical capacity, canaries, reinit, binding and idle retirement passed");
#ifdef MB_WIN_CRITICAL_FAULTS
 CHECK(mb_win_threads_open(&domain,NULL)==MB_WIN_THREADS_OK);CHECK(mb_win_threads_enter(&domain,NULL)==MB_WIN_THREADS_OK);
 int native=0;fake_processors=-1;CHECK(mb_win_critical_open(&owner,&domain,1,&native)==MB_WIN_CRITICAL_NATIVE_ERROR&&native==EIO&&!owner.state);
 fake_processors=1;CHECK(mb_win_critical_open(&owner,&domain,1,&native)==MB_WIN_CRITICAL_OK);
 uint32_t previous=9;CHECK(mb_win_critical_initialize(&owner,&third,123)==MB_WIN_CRITICAL_OK);CHECK(mb_win_critical_set_spin(&owner,&third,456,&previous)==MB_WIN_CRITICAL_OK&&previous==0);
 fail_destroy=1;CHECK(mb_win_critical_close(&owner,&native)==MB_WIN_CRITICAL_NATIVE_ERROR&&native==EBUSY&&owner.state);
 CHECK(mb_win_threads_leave(&domain,NULL)==MB_WIN_THREADS_OK);CHECK(mb_win_threads_close(&domain,NULL)==MB_WIN_THREADS_BUSY);
 CHECK(mb_win_critical_close(&owner,&native)==MB_WIN_CRITICAL_OK);CHECK(mb_win_threads_close(&domain,NULL)==MB_WIN_THREADS_OK);
 puts("critical native CPU failure, single CPU spin and cleanup retry passed");
#endif
 return 0;
}
#endif
