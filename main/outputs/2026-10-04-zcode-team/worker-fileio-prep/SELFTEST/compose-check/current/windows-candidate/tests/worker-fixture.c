/* Owned PE fixture: internal Windows callback is real machine code in this DLL.
 * No DllMain invocation, CRT, static TLS, exception/unwind or host callback seam.
 * Suitable for standalone /noentry and inclusion in the opt-in entry consumer. */
#include "worker-state.h"
#ifndef MB_WORKER_CLOSE_EARLY
#define MB_WORKER_CLOSE_EARLY 0
#endif
#ifndef MB_OWNED_REJECT_WORKER_START
#define MB_OWNED_REJECT_WORKER_START 0
#endif
#ifndef MB_OWNED_INIT_ONCE
#define MB_OWNED_INIT_ONCE 0
#endif
#ifndef MB_OWNED_MUTEX
#define MB_OWNED_MUTEX 0
#endif
#ifndef MB_OWNED_FILEIO
#define MB_OWNED_FILEIO 0
#endif
typedef __UINT32_TYPE__ mb_worker_u32;
typedef __UINT64_TYPE__ mb_worker_u64;
typedef __INT32_TYPE__ mb_worker_i32;
typedef __SIZE_TYPE__ mb_worker_size;
typedef __UINTPTR_TYPE__ mb_worker_ptr;
__declspec(dllimport) void *CreateThread(void *,mb_worker_size,mb_worker_u32 (*)(void *),void *,mb_worker_u32,mb_worker_u32 *);
__declspec(dllimport) mb_worker_u32 WaitForSingleObject(void *,mb_worker_u32);
__declspec(dllimport) mb_worker_i32 GetExitCodeThread(void *,mb_worker_u32 *);
__declspec(dllimport) mb_worker_i32 CloseHandle(void *);
__declspec(dllimport) void Sleep(mb_worker_u32);
__declspec(dllimport) mb_worker_u32 GetLastError(void);
__declspec(dllimport) void SetLastError(mb_worker_u32);
_Static_assert(sizeof(void *)==8 && sizeof(mb_worker_size)==8 && sizeof(mb_worker_u32)==4,"Windows64 fixture ABI");
#if MB_OWNED_SYNC
#include "worker-sync.h"
#endif
#if MB_OWNED_INIT_ONCE
#include "worker-initonce.h"
#endif
#if MB_OWNED_MUTEX
#include "worker-mutex.h"
#endif
#if MB_OWNED_FILEIO
#include "worker-fileio.h"
#endif
struct mb_worker_fixture_state {
 void *handle;
 mb_worker_u64 cookie;
 mb_worker_u64 observed_cookie;
 mb_worker_u32 gate,entered,done,failure,worker_error,begun;
};
static struct mb_worker_fixture_state mb_worker_state;
static struct mb_worker_fixture_state *volatile mb_worker_relocated_state=&mb_worker_state;
static mb_worker_u32 mb_worker_internal_start(void *arg) {
 struct mb_worker_fixture_state *s=(struct mb_worker_fixture_state *)arg;
 if(s!=&mb_worker_state || s->cookie!=MB_WORKER_PARAMETER_COOKIE)return 0xf0010001U;
 SetLastError(MB_WORKER_LAST_ERROR);
#if MB_OWNED_SYNC
 if(mb_worker_sync_started()!=1)s->failure=1;
#endif
#if MB_OWNED_INIT_ONCE
 if(mb_worker_initonce_thread_begin()!=1)s->failure=1;
#endif
#if MB_OWNED_MUTEX
 if(mb_worker_mutex_thread_acquire()!=1)s->failure=1;
#endif
 __atomic_store_n(&s->entered,1,__ATOMIC_RELEASE);
 Sleep(0);Sleep(1);
#if MB_OWNED_SYNC
 mb_worker_u32 expected=0;
 while(!__atomic_load_n(&s->gate,__ATOMIC_ACQUIRE))if(!WaitOnAddress(&s->gate,&expected,4,0xffffffffU)){s->failure=1;break;}
 EnterCriticalSection(mb_worker_sync.critical);
#else
 while(!__atomic_load_n(&s->gate,__ATOMIC_ACQUIRE))Sleep(0);
#endif
 s->observed_cookie=s->cookie;
 s->worker_error=GetLastError();
 if(s->worker_error!=MB_WORKER_LAST_ERROR)s->failure=1;
#if MB_OWNED_SYNC
 LeaveCriticalSection(mb_worker_sync.critical);
#endif
#if MB_OWNED_INIT_ONCE
 if(mb_worker_initonce_thread_complete()!=1)s->failure=1;
#endif
 __atomic_store_n(&s->done,1,__ATOMIC_RELEASE);
 return s->failure?0xf0010002U:MB_WORKER_EXPECTED_EXIT;
}
/* Force a DIR64 pointer relocation and retain a non-exported start function. */
static mb_worker_u32 (*volatile mb_worker_relocated_start)(void *)=mb_worker_internal_start;
/* Every failure after CreateThread opens the gate and attempts handle closure.
 * Failed closure keeps the exact handle in out[0] for native cleanup retry. */
static mb_worker_i32 mb_worker_fixture_fail(struct mb_worker_fixture_state *s,mb_worker_u64 *out,mb_worker_i32 failure) {
#if MB_OWNED_SYNC
 mb_worker_sync_unblock(&s->gate);
#else
 __atomic_store_n(&s->gate,1,__ATOMIC_RELEASE);
#endif
 if(s->handle && CloseHandle(s->handle)){s->handle=(void *)0;s->begun=0;}
 out[0]=(mb_worker_u64)(mb_worker_ptr)s->handle;
 return failure;
}
__declspec(dllexport) mb_worker_i32 WorkerBegin(mb_worker_u64 *out) {
 struct mb_worker_fixture_state *s=mb_worker_relocated_state;
 if(!out || s->begun)return -1;
 out[0]=out[1]=out[2]=out[3]=0;
 s->handle=(void *)0;s->cookie=MB_WORKER_PARAMETER_COOKIE;s->observed_cookie=0;
 s->worker_error=0;s->failure=0;s->begun=1;
 __atomic_store_n(&s->gate,0,__ATOMIC_RELEASE);__atomic_store_n(&s->entered,0,__ATOMIC_RELEASE);__atomic_store_n(&s->done,0,__ATOMIC_RELEASE);
#if MB_OWNED_SYNC
 mb_worker_i32 prepared=mb_worker_sync_prepare(&s->gate);
 if(prepared!=1){mb_worker_sync_unblock(&s->gate);(void)mb_worker_sync_finish();s->begun=0;return prepared;}
#endif
#if MB_OWNED_INIT_ONCE
 mb_worker_i32 once_prepared=mb_worker_initonce_prepare();
 if(once_prepared!=1){mb_worker_sync_unblock(&s->gate);(void)mb_worker_sync_finish();(void)mb_worker_initonce_finish();s->begun=0;return once_prepared;}
#endif
#if MB_OWNED_MUTEX
 mb_worker_i32 mutex_prepared=mb_worker_mutex_prepare();
 if(mutex_prepared!=1){mb_worker_sync_unblock(&s->gate);(void)mb_worker_sync_finish();(void)mb_worker_initonce_finish();(void)mb_worker_mutex_finish();s->begun=0;return mutex_prepared;}
#endif
 struct {mb_worker_u32 left,value,right;} id={0x51a1beefU,0,0x51a2beefU};
 SetLastError(MB_WORKER_CREATOR_ERROR);
 s->handle=CreateThread((void *)0,0,mb_worker_relocated_start,MB_OWNED_REJECT_WORKER_START?(void *)0:s,0,&id.value);
 if(!s->handle){
#if MB_OWNED_SYNC
  mb_worker_sync_unblock(&s->gate);(void)mb_worker_sync_finish();
#endif
  s->begun=0;return -2;
 }
 out[0]=(mb_worker_u64)(mb_worker_ptr)s->handle;out[1]=id.value;
 if(id.left!=0x51a1beefU||id.right!=0x51a2beefU||id.value==0)return mb_worker_fixture_fail(s,out,-3);
#if MB_OWNED_SYNC
 if(mb_worker_sync_ready(s->handle)!=1)return mb_worker_fixture_fail(s,out,-40);
#endif
 out[2]=WaitForSingleObject(s->handle,0);if(out[2]!=258)return mb_worker_fixture_fail(s,out,-4);
 struct {mb_worker_u32 left,value,right;} exit={0x51b1beefU,0,0x51b2beefU};
 if(!GetExitCodeThread(s->handle,&exit.value))return mb_worker_fixture_fail(s,out,-5);out[3]=exit.value;
 if(exit.left!=0x51b1beefU||exit.right!=0x51b2beefU||exit.value!=259)return mb_worker_fixture_fail(s,out,-6);
 Sleep(0);if(GetLastError()!=MB_WORKER_CREATOR_ERROR)return mb_worker_fixture_fail(s,out,-7);
#if MB_WORKER_CLOSE_EARLY
 if(!CloseHandle(s->handle))return mb_worker_fixture_fail(s,out,-8);
 s->handle=(void *)0;
#endif
 return 1;
}
__declspec(dllexport) mb_worker_i32 WorkerCollect(mb_worker_u64 *out) {
 struct mb_worker_fixture_state *s=mb_worker_relocated_state;
 if(!out || !s->begun)return -11;
#if !MB_WORKER_CLOSE_EARLY
 if(!s->handle)return -11;
#endif
 out[0]=out[1]=out[2]=out[3]=0;
#if MB_OWNED_SYNC
 mb_worker_sync_unblock(&s->gate);
#else
 __atomic_store_n(&s->gate,1,__ATOMIC_RELEASE);
#endif
 SetLastError(MB_WORKER_CREATOR_ERROR);
#if MB_WORKER_CLOSE_EARLY
 while(!__atomic_load_n(&s->done,__ATOMIC_ACQUIRE))Sleep(0);
 out[0]=0;out[1]=s->observed_cookie;out[2]=s->worker_error;out[3]=0xffffffffU;
 s->begun=0;
 if(out[1]!=MB_WORKER_PARAMETER_COOKIE||out[2]!=MB_WORKER_LAST_ERROR)return -16;
#else
 if(WaitForSingleObject(s->handle,0xffffffffU)!=0)return mb_worker_fixture_fail(s,out,-12);
 struct {mb_worker_u32 left,value,right;} exit={0x51c1beefU,0,0x51c2beefU};
 if(!GetExitCodeThread(s->handle,&exit.value))return mb_worker_fixture_fail(s,out,-13);out[0]=exit.value;
 if(exit.left!=0x51c1beefU||exit.right!=0x51c2beefU)return mb_worker_fixture_fail(s,out,-14);
 out[1]=s->observed_cookie;out[2]=s->worker_error;out[3]=WaitForSingleObject(s->handle,0);
 if(!CloseHandle(s->handle))return mb_worker_fixture_fail(s,out,-15);s->handle=(void *)0;s->begun=0;
 if(exit.value!=MB_WORKER_EXPECTED_EXIT||out[1]!=MB_WORKER_PARAMETER_COOKIE||out[2]!=MB_WORKER_LAST_ERROR||out[3]!=0)return -16;
#endif
 if(!__atomic_load_n(&s->entered,__ATOMIC_ACQUIRE)||!__atomic_load_n(&s->done,__ATOMIC_ACQUIRE)||s->failure)return -17;
 if(GetLastError()!=MB_WORKER_CREATOR_ERROR)return -18;
#if MB_OWNED_SYNC
 if(mb_worker_sync_finish()!=1)return -43;
#endif
#if MB_OWNED_INIT_ONCE
 if(mb_worker_initonce_finish()!=1)return -54;
#endif
#if MB_OWNED_MUTEX
 if(mb_worker_mutex_finish()!=1)return -77;
#endif
 return 1;
}
#if MB_OWNED_FILEIO
/* Dedicated export: the full KERNEL32 file I/O exercise runs outside the
 * worker thread flow, so the gate, the wait chain and every other family's
 * fixtures stay untouched (composition independence). */
__declspec(dllexport) mb_worker_i32 WorkerFileIo(mb_worker_u64 *out){return mb_worker_fileio_call(out);}
#endif
