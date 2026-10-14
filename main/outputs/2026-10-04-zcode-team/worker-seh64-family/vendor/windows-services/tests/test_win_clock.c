#include "../mb_win_clock.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#c);exit(1);}}while(0)
typedef int32_t (__attribute__((ms_abi))*query_fn)(int64_t *);
#if defined(MB_WIN_CLOCK_TEST)
static struct timespec supplied;
static int clock_failure;
static unsigned native_calls;
static uint32_t *error_side_effect;
int mb_win_clock_test_gettime(clockid_t clock,struct timespec *out){
 CHECK(clock==CLOCK_MONOTONIC);++native_calls;if(error_side_effect)*error_side_effect=12345;if(clock_failure){errno=EIO;return -1;}*out=supplied;return 0;
}
static void sample(int64_t sec,long ns){supplied.tv_sec=sec;supplied.tv_nsec=ns;clock_failure=0;}
static void conversion(void){
 int64_t out=-91;CHECK(mb_win_clock_read_counter(NULL)==MB_WIN_CLOCK_INVALID_ARGUMENT);
 sample(0,0);CHECK(mb_win_clock_read_counter(&out)==MB_WIN_CLOCK_OK&&out==0);
 sample(12,345678901);CHECK(mb_win_clock_read_counter(&out)==MB_WIN_CLOCK_OK&&out==INT64_C(12345678901));
 CHECK(mb_win_clock_read_frequency(&out)==MB_WIN_CLOCK_OK&&out==MB_WIN_CLOCK_FREQUENCY);
 sample(INT64_MAX/1000000000,INT64_MAX%1000000000);CHECK(mb_win_clock_read_counter(&out)==MB_WIN_CLOCK_OK&&out==INT64_MAX);
 out=-91;sample(INT64_MAX/1000000000,INT64_MAX%1000000000+1);CHECK(mb_win_clock_read_counter(&out)==MB_WIN_CLOCK_RANGE_ERROR&&out==-91);
 sample(INT64_MAX,0);CHECK(mb_win_clock_read_counter(&out)==MB_WIN_CLOCK_RANGE_ERROR&&out==-91);
 sample(-1,1);CHECK(mb_win_clock_read_counter(&out)==MB_WIN_CLOCK_RANGE_ERROR&&out==-91);
 sample(1,-1);CHECK(mb_win_clock_read_counter(&out)==MB_WIN_CLOCK_RANGE_ERROR&&out==-91);
 sample(1,1000000000);CHECK(mb_win_clock_read_counter(&out)==MB_WIN_CLOCK_RANGE_ERROR&&out==-91);
 sample(1,1);clock_failure=1;CHECK(mb_win_clock_read_counter(&out)==MB_WIN_CLOCK_NATIVE_ERROR&&out==-91);
 CHECK(mb_win_clock_read_frequency(&out)==MB_WIN_CLOCK_NATIVE_ERROR&&out==-91);sample(2,3);
}
#endif
static mb_win_thread_domain domain;
static mb_win_clock_session session;
static mb_pe_module module;
static query_fn counter,frequency;
static mb_pe_import item(const char *library,const char *name){mb_pe_import i={0};i.library=(mb_pe_string){library,strlen(library)};i.name=(mb_pe_string){name,strlen(name)};return i;}
static void setup(void){
 int error=0;CHECK(mb_win_threads_open(&domain,&error)==MB_WIN_THREADS_OK);CHECK(mb_win_clock_begin(&session,&domain,&module)==MB_WIN_CLOCK_OK);
 CHECK(mb_win_threads_close(&domain,&error)==MB_WIN_THREADS_BUSY);CHECK(mb_win_threads_enter(&domain,&error)==MB_WIN_THREADS_OK);module.state=MB_PE_MODULE_LOADING;
 uint64_t address=0;mb_pe_import i=item("kErNeL32.dLl","QueryPerformanceCounter");CHECK(mb_win_clock_bind(&session,&i,&address)==MB_PE_BIND_BOUND);counter=(query_fn)(uintptr_t)address;
 i=item("KERNEL32.dll","QueryPerformanceFrequency");CHECK(mb_win_clock_bind(&session,&i,&address)==MB_PE_BIND_BOUND);frequency=(query_fn)(uintptr_t)address;
}
static void finish(void){int error=0;module.state=MB_PE_MODULE_EMPTY;CHECK(mb_win_clock_end(&session)==MB_WIN_CLOCK_OK);CHECK(mb_win_threads_leave(&domain,&error)==MB_WIN_THREADS_OK);CHECK(mb_win_threads_close(&domain,&error)==MB_WIN_THREADS_OK);}
static void binding_and_lifetime(void){
 setup();uint64_t address=0x55;const char *unsupported[]={"queryperformancecounter","GetSystemTimeAsFileTime","GetCurrentThreadId","GetCurrentProcessId"};
 for(size_t n=0;n<sizeof(unsupported)/sizeof(*unsupported);++n){mb_pe_import i=item("KERNEL32.dll",unsupported[n]);CHECK(mb_win_clock_bind(&session,&i,&address)==MB_PE_BIND_UNRESOLVED&&address==0x55);}
 mb_pe_import i=item("KERNELBASE.dll","QueryPerformanceCounter");CHECK(mb_win_clock_bind(&session,&i,&address)==MB_PE_BIND_UNRESOLVED&&address==0x55);
 i=item("KERNEL32.dll","QueryPerformanceCounter");i.by_ordinal=1;i.ordinal=7;CHECK(mb_win_clock_bind(&session,&i,&address)==MB_PE_BIND_UNRESOLVED&&address==0x55);i.by_ordinal=0;
 mb_win_clock_session copied=session;CHECK(mb_win_clock_bind(&copied,&i,&address)==MB_PE_BIND_ERROR&&mb_win_clock_end(&copied)==MB_WIN_CLOCK_BAD_STATE);
 CHECK(mb_win_clock_end(&session)==MB_WIN_CLOCK_BUSY);module.state=MB_PE_MODULE_CLEANUP;CHECK(mb_win_clock_end(&session)==MB_WIN_CLOCK_BUSY&&session.pin.owner==&domain&&mb_win_clock_bind(&session,&i,&address)==MB_PE_BIND_ERROR);
 mb_win_thread_domain other={0};mb_pe_module another={0};mb_win_clock_session next={0};int error=0;CHECK(mb_win_threads_open(&other,&error)==MB_WIN_THREADS_OK);CHECK(mb_win_clock_begin(&next,&other,&another)==MB_WIN_CLOCK_BUSY);CHECK(mb_win_threads_close(&other,&error)==MB_WIN_THREADS_OK);
 CHECK(mb_win_clock_begin(&next,&domain,&another)==MB_WIN_CLOCK_OK);CHECK(mb_win_clock_end(&next)==MB_WIN_CLOCK_OK);finish();
}
static void actual_calls(void){
 setup();module.state=MB_PE_MODULE_ACTIVE;*mb_win_threads_last_error(&domain)=UINT32_C(0xf1234567);*mb_win_threads_errno(&domain)=82;
#if defined(MB_WIN_CLOCK_TEST)
 error_side_effect=mb_win_threads_last_error(&domain);
#endif
 int64_t first=-1,second=-1,rate=-1;
 CHECK(counter(&first)==1&&counter(&second)==1&&first>=0&&second>=first);
 CHECK(frequency(&rate)==1&&rate==MB_WIN_CLOCK_FREQUENCY&&*mb_win_threads_last_error(&domain)==UINT32_C(0xf1234567)&&*mb_win_threads_errno(&domain)==82);
#if defined(MB_WIN_CLOCK_TEST)
 CHECK(first==INT64_C(2000000003));clock_failure=1;second=77;
 CHECK(counter(&second)==0&&second==77&&*mb_win_threads_last_error(&domain)==MB_WIN_CLOCK_ERROR_NATIVE&&*mb_win_threads_errno(&domain)==82);
 rate=89;CHECK(frequency(&rate)==0&&rate==89&&*mb_win_threads_last_error(&domain)==MB_WIN_CLOCK_ERROR_NATIVE);
 sample(INT64_MAX,0);CHECK(counter(&second)==0&&second==77&&*mb_win_threads_last_error(&domain)==MB_WIN_CLOCK_ERROR_RANGE);
 sample(3,4);CHECK(counter(&second)==1&&second==INT64_C(3000000004)&&*mb_win_threads_last_error(&domain)==MB_WIN_CLOCK_ERROR_RANGE);
#endif
 CHECK(counter(NULL)==0&&*mb_win_threads_last_error(&domain)==MB_WIN_ERROR_INVALID_PARAMETER);
#if defined(MB_WIN_CLOCK_TEST)
 error_side_effect=NULL;
#endif
 finish();
}
static void *worker(void *arg){
 (void)arg;int error=0;CHECK(mb_win_threads_enter(&domain,&error)==MB_WIN_THREADS_OK);*mb_win_threads_last_error(&domain)=901;*mb_win_threads_errno(&domain)=902;int64_t out=17;
#if defined(MB_WIN_CLOCK_TEST)
 CHECK(counter(&out)==0&&out==17&&*mb_win_threads_last_error(&domain)==MB_WIN_CLOCK_ERROR_NATIVE);
#else
 CHECK(counter(&out)==1&&out>=0&&*mb_win_threads_last_error(&domain)==901);
#endif
 CHECK(*mb_win_threads_errno(&domain)==902);CHECK(mb_win_threads_leave(&domain,&error)==MB_WIN_THREADS_OK);return NULL;
}
static void thread_isolation(void){
 setup();*mb_win_threads_last_error(&domain)=701;*mb_win_threads_errno(&domain)=702;
#if defined(MB_WIN_CLOCK_TEST)
 clock_failure=1;
#endif
 pthread_t thread;CHECK(!pthread_create(&thread,NULL,worker,NULL));CHECK(!pthread_join(thread,NULL));
 CHECK(*mb_win_threads_last_error(&domain)==701&&*mb_win_threads_errno(&domain)==702);finish();
}
int main(void){
#if defined(MB_WIN_CLOCK_TEST)
 conversion();
#endif
 binding_and_lifetime();actual_calls();thread_isolation();
#if defined(MB_WIN_CLOCK_TEST)
 CHECK(native_calls>10);puts("Clock: checked conversions, native failures, exact binder/lifetimes and two-thread Win64 calls passed");
#else
 puts("Clock: real host CLOCK_MONOTONIC, exact binder/lifetimes and two-thread Win64 calls passed");
#endif
 return 0;
}
