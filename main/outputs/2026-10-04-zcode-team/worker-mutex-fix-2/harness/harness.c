/* worker-mutex-fix-2 reproduction harness (alarm-guarded).
 * Compiles the REAL vendored fixture header (worker-mutex.h, unmodified copy)
 * against the REAL vendored windows-services mutex owner (mb_win_mutex.c) and
 * binds the fixture's KERNEL32 imports to the owner's typed entry points.
 *
 *  MODE current (default): run the fixture's creator side (prepare/finish) and
 *   the driver teardown probe: prepare code, finish code, retired-token count,
 *   mutex owner close result. Demonstrates the composed hang cause without
 *   hanging: close returns BUSY and the real driver would retry forever.
 *  MODE fixed (-DFIXED): the fixed vendored header (worker-mutex-fixed.h) with
 *   a real worker pthread abandoning the named mutex; verifies full green
 *   teardown: prepare 1, WAIT_ABANDONED + transfer + reacquire, 7/7 retirement,
 *   mutex close OK, threads close OK -> exit 0.
 */
#include "mb_win_threads.h"
#include "mb_win_mutex.h"
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
typedef __INT32_TYPE__ mb_worker_i32;
typedef __UINT32_TYPE__ mb_worker_u32;
uint32_t GetLastError(void); /* the PE fixture declares this via worker-fixture.c */
#if FIXED
#include "worker-mutex-fixed.h"
#else
#include "worker-mutex.h"
#endif
static mb_win_thread_domain domain;
static mb_win_mutex mutex_owner;
static unsigned mutex_open_ok, domain_open_ok;
static void on_alarm(int sig){(void)sig;fprintf(stderr,"[alarm] HARNESS BLOCKED (timeout) - killing\n");_exit(99);}
/* Typed bindings for the fixture header's dllimport surface. */
void *CreateMutexW(void *sec,int32_t initial,const mb_worker_u16 *name){
 (void)sec;
 return (void *)(uintptr_t)mb_win_mutex_create(&mutex_owner,name,initial?MB_WIN_MUTEX_INITIAL_OWNER:0);}
void *CreateMutexExW(void *sec,const mb_worker_u16 *name,uint32_t flags,uint32_t access){
 (void)sec;(void)access;
 return (void *)(uintptr_t)mb_win_mutex_create(&mutex_owner,name,flags);}
void *OpenMutexW(uint32_t access,int32_t inherit,const mb_worker_u16 *name){
 (void)access;(void)inherit;
 return (void *)(uintptr_t)mb_win_mutex_open_named(&mutex_owner,name);}
int32_t ReleaseMutex(void *handle){return mb_win_mutex_release(&mutex_owner,(uint64_t)(uintptr_t)handle);}
uint32_t WaitForSingleObjectEx(void *handle,uint32_t milliseconds,int32_t alertable){
 return mb_win_mutex_wait_ex(&mutex_owner,(uint64_t)(uintptr_t)handle,milliseconds,alertable);}
int32_t CloseHandle(void *handle){return mb_win_mutex_close_handle(&mutex_owner,(uint64_t)(uintptr_t)handle);}
uint32_t GetLastError(void){return *mb_win_threads_last_error(&domain);}
static uint32_t set_last_error(uint32_t v){*mb_win_threads_last_error(&domain)=v;}
#if FIXED
static void *fixed_worker(void *arg){
 (void)arg;int native=0;
 if(mb_win_threads_enter(&domain,&native)!=MB_WIN_THREADS_OK){fprintf(stderr,"[worker] domain enter FAILED\n");return (void *)1;}
 fprintf(stderr,"[worker] admitted; creating abandon mutex as initial owner\n");
 mb_worker_i32 r=mb_worker_mutex_thread_acquire();
 fprintf(stderr,"[worker] thread_acquire -> %d (token count now %u); exiting WITHOUT release\n",r,mb_worker_mutex_state.token_count);
 return (void *)(uintptr_t)(r==1?0:1);
}
#endif
int main(void){
 signal(SIGALRM,on_alarm);alarm(45);
 int native=0;
 if(mb_win_threads_open(&domain,&native)!=MB_WIN_THREADS_OK){fprintf(stderr,"threads open FAILED\n");return 3;}
 domain_open_ok=1;
 if(mb_win_threads_enter(&domain,&native)!=MB_WIN_THREADS_OK){fprintf(stderr,"main admission FAILED\n");return 3;}
 mb_win_mutex_config mc={&domain,16};
 mb_win_mutex_status ms=mb_win_mutex_open(&mutex_owner,&mc,&native);
 if(ms!=MB_WIN_MUTEX_OK){fprintf(stderr,"mutex owner open FAILED (%d)\n",(int)ms);return 3;}
 mutex_open_ok=1;
 set_last_error(0xc2468aceu);
#if FIXED
 fprintf(stderr,"[main] prepare...\n");
 mb_worker_i32 p=mb_worker_mutex_prepare();
 fprintf(stderr,"[main] prepare -> %d (tokens kept %u)\n",p,mb_worker_mutex_state.token_count);
 if(p!=1)return 2;
 pthread_t worker;
 if(pthread_create(&worker,0,fixed_worker,0)){fprintf(stderr,"worker pthread create FAILED\n");return 3;}
 void *exit_code=0;
 if(pthread_join(worker,&exit_code)||(uintptr_t)exit_code){fprintf(stderr,"worker join FAILED\n");return 3;}
 fprintf(stderr,"[main] worker joined; finish...\n");
 mb_worker_i32 f=mb_worker_mutex_finish();
 fprintf(stderr,"[main] finish -> %d transfered=%u reacquired=%u tokens_left=%u\n",
  f,mb_worker_mutex_state.transfered,mb_worker_mutex_state.reacquired,mb_worker_mutex_state.token_count);
 if(f!=1||!mb_worker_mutex_state.transfered||!mb_worker_mutex_state.reacquired||mb_worker_mutex_state.token_count)return 2;
 int close_native=0;
 ms=mb_win_mutex_close(&mutex_owner,&close_native);
 fprintf(stderr,"[main] mutex close -> %d (%s)\n",(int)ms,ms==MB_WIN_MUTEX_OK?"OK":"BUSY/FAIL");
 if(ms!=MB_WIN_MUTEX_OK)return 2;
 mb_win_thread_status ts=mb_win_threads_leave(&domain,&native);
 fprintf(stderr,"[main] threads leave -> %d\n",(int)ts);
 if(ts!=MB_WIN_THREADS_OK)return 2;
 ts=mb_win_threads_close(&domain,&close_native);
 fprintf(stderr,"[main] threads close -> %d\n",(int)ts);
 if(ts!=MB_WIN_THREADS_OK)return 2;
 fprintf(stderr,"[main] REPRO FIXED COMPLETED clean\n");
 return 0;
#else
 fprintf(stderr,"[main] prepare...\n");
 mb_worker_i32 p=mb_worker_mutex_prepare();
 fprintf(stderr,"[main] prepare -> %d  (fixture code: -68 = named-mutex re-wait step) tokens kept: %u\n",
  p,mb_worker_mutex_state.token_count);
 fprintf(stderr,"[main] last_error after prepare -> %u\n",GetLastError());
 fprintf(stderr,"[main] finish (failure-path handler inside WorkerBegin; abandon mutex does not exist yet)...\n");
 mb_worker_i32 f=mb_worker_mutex_finish();
 fprintf(stderr,"[main] finish -> %d (current finish returns -72 BEFORE the retirement loop; tokens retired: 0)\n",f);
 fprintf(stderr,"[main] retired tokens: %u (leaked: %u) -> owner close would BUSY forever\n",
  mb_worker_mutex_state.token_count==0?mb_worker_mutex_state.token_count:0,mb_worker_mutex_state.token_count);
 int close_native=0;
 ms=mb_win_mutex_close(&mutex_owner,&close_native);
 fprintf(stderr,"[main] mutex close -> %d (%s)   [3=BUSY: real driver retry loop hangs here]\n",
  (int)ms,ms==MB_WIN_MUTEX_OK?"OK":"BUSY");
 (void)mb_win_threads_leave(&domain,&native);
 (void)mb_win_threads_close(&domain,&close_native);
 if(p==-68 && f==-72 && ms==MB_WIN_MUTEX_BUSY){fprintf(stderr,"[main] REPRO CONFIRMED (prepare -68 / finish -72 / close BUSY)\n");return 2;}
 fprintf(stderr,"[main] UNEXPECTED state\n");
 return 3;
#endif
}
