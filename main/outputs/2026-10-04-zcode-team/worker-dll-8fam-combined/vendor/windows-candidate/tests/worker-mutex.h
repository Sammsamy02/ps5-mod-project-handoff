#ifndef MB_OWNED_WORKER_MUTEX_H
#define MB_OWNED_WORKER_MUTEX_H
/* worker-mutex-fix-2: vendored FIXED revision of the real
 * outputs/ps5-mod-bridge/windows-candidate/tests/worker-mutex.h.
 * Two changes only (diffs shipped as worker-mutex.h.diff):
 *  1. Reorder the named-mutex checks: this adapter's documented policy is that
 *     an owner re-wait NEVER grants (finite timeout -> WAIT_TIMEOUT, infinite
 *     -> WAIT_FAILED + 771; no recursion). The old order expected the owner
 *     re-wait through a SECOND handle to succeed (WAIT_OBJECT_0) at -68 while
 *     also expecting a further owner re-wait to time out at -69: mutually
 *     inconsistent, and the first half failed at runtime with -68, which is
 *     the composed -68/begin-failure teardown hang (build 4 checkpoint).
 *     New order: acquire via `opened`, owner re-wait times out (-69, adapter
 *     deviation, documented), release, then acquire through the second handle
 *     (proves both handles name the same object) and release it again.
 *  2. MFR-2: mb_worker_mutex_finish retires EVERY kept token on EVERY exit
 *     (success or failure) instead of only on the success tail; failure codes
 *     are accumulated (first failure wins) and retirement happens even when an
 *     earlier check failed, so a failed Begin/Collect can never leave tokens
 *     behind that keep mb_win_mutex_close BUSY (the driver's cleanup retry
 *     would otherwise hang forever). */
typedef __UINT16_TYPE__ mb_worker_u16;
__declspec(dllimport) void *CreateMutexW(void *,mb_worker_i32,const mb_worker_u16 *);
__declspec(dllimport) void *CreateMutexExW(void *,const mb_worker_u16 *,mb_worker_u32,mb_worker_u32);
__declspec(dllimport) void *OpenMutexW(mb_worker_u32,mb_worker_i32,const mb_worker_u16 *);
__declspec(dllimport) mb_worker_i32 ReleaseMutex(void *);
__declspec(dllimport) mb_worker_u32 WaitForSingleObjectEx(void *,mb_worker_u32,mb_worker_i32);
#define MB_WORKER_MUTEX_WAIT_OBJECT_0 0x0u
#define MB_WORKER_MUTEX_WAIT_ABANDONED 0x80u
#define MB_WORKER_MUTEX_WAIT_TIMEOUT 0x102u
#define MB_WORKER_MUTEX_WAIT_FAILED 0xffffffffu
#define MB_WORKER_MUTEX_ERROR_NOT_OWNER 288u
#define MB_WORKER_MUTEX_ERROR_NOT_SUPPORTED 50u
#define MB_WORKER_MUTEX_ERROR_ALREADY_EXISTS 183u
#define MB_WORKER_MUTEX_ERROR_INVALID_PARAMETER 87u
static const mb_worker_u16 mb_worker_mutex_named[]={'M','b','W','o','r','k','e','r','.','N','a','m','e','d',0};
static const mb_worker_u16 mb_worker_mutex_named_folded[]={'m','b','w','o','r','k','e','r','.','n','a','m','e','d',0};
static const mb_worker_u16 mb_worker_mutex_abandon[]={'M','b','W','o','r','k','e','r','.','A','b','a','n','d','o','n',0};
static const mb_worker_u16 mb_worker_mutex_abandon_folded[]={'m','b','w','o','r','k','e','r','.','a','b','a','n','d','o','n',0};
static const mb_worker_u16 mb_worker_mutex_backslash[]={'B','a','d','\\','N','a','m','e',0};
static struct {
 void *unnamed,*named,*opened,*exists,*abandon_open,*worker_abandon;
 void *tokens[8];mb_worker_u32 token_count;
 mb_worker_u32 alertable,recursion,transfered,reacquired;
} mb_worker_mutex_state;
/* Every public handle token this fixture creates is recorded via keep() and
 * retired through the combined CloseHandle dispatch (workers thread handles
 * first, mutex tokens fallback). Retirement runs on every exit path (MFR-2). */
__declspec(dllimport) mb_worker_i32 CloseHandle(void *);
static void mb_worker_mutex_keep(void *token){
 if(token && mb_worker_mutex_state.token_count<8)mb_worker_mutex_state.tokens[mb_worker_mutex_state.token_count++]=token;}
/* Creator-side documented semantics before the worker exists. */
static mb_worker_i32 mb_worker_mutex_prepare(void){
 mb_worker_mutex_state.unnamed=CreateMutexW(0,0,0);mb_worker_mutex_keep(mb_worker_mutex_state.unnamed);
 if(!mb_worker_mutex_state.unnamed)return -60;
 if(WaitForSingleObjectEx(mb_worker_mutex_state.unnamed,1,0)!=MB_WORKER_MUTEX_WAIT_OBJECT_0)return -61; /* unowned mutex is signaled: a finite wait acquires */
 if(ReleaseMutex(mb_worker_mutex_state.unnamed)!=1)return -62; /* creator became the owner above */
 if(WaitForSingleObjectEx(mb_worker_mutex_state.unnamed,0,1)!=MB_WORKER_MUTEX_WAIT_FAILED || GetLastError()!=MB_WORKER_MUTEX_ERROR_NOT_SUPPORTED)return -63;
 mb_worker_mutex_state.named=CreateMutexExW(0,mb_worker_mutex_named,0,0x1f0001u);mb_worker_mutex_keep(mb_worker_mutex_state.named);
 if(!mb_worker_mutex_state.named)return -64;
 mb_worker_mutex_state.opened=OpenMutexW(0x1f0001u,0,mb_worker_mutex_named_folded);mb_worker_mutex_keep(mb_worker_mutex_state.opened);
 if(!mb_worker_mutex_state.opened)return -65; /* folded lookup */
 mb_worker_mutex_state.exists=CreateMutexW(0,1,mb_worker_mutex_named);mb_worker_mutex_keep(mb_worker_mutex_state.exists);
 if(!mb_worker_mutex_state.exists || GetLastError()!=MB_WORKER_MUTEX_ERROR_ALREADY_EXISTS)return -66;
 if(ReleaseMutex(mb_worker_mutex_state.exists)!=0 || GetLastError()!=MB_WORKER_MUTEX_ERROR_NOT_OWNER)return -67; /* initial owner ignored */
 /* Acquire the unowned named object through `opened`. */
 if(WaitForSingleObjectEx(mb_worker_mutex_state.opened,0,0)!=MB_WORKER_MUTEX_WAIT_OBJECT_0)return -68;
 /* Owner re-wait: this adapter never re-grants (no recursion; adapter policy
  * documented in mb_win_mutex.h): finite timeout follows the timeout path. */
 mb_worker_mutex_state.recursion=WaitForSingleObjectEx(mb_worker_mutex_state.opened,1,0);
 if(mb_worker_mutex_state.recursion!=MB_WORKER_MUTEX_WAIT_TIMEOUT)return -69;
 if(ReleaseMutex(mb_worker_mutex_state.opened)!=1)return -70;
 /* Second handle to the same object: once the object is unowned again, the
  * wait through the folded second handle must grant, proving both handles
  * reference the one object; release it to leave the object unowned. */
 {void *second=OpenMutexW(0x1f0001u,0,mb_worker_mutex_named_folded);mb_worker_mutex_keep(second);
  if(!second)return -68;
  if(WaitForSingleObjectEx(second,0,0)!=MB_WORKER_MUTEX_WAIT_OBJECT_0)return -68;
  if(ReleaseMutex(second)!=1)return -70;}
 if(CreateMutexExW(0,mb_worker_mutex_backslash,0,0)!=0 || GetLastError()!=MB_WORKER_MUTEX_ERROR_INVALID_PARAMETER)return -71;
 return 1;
}
/* Worker thread: acquire and publish a named mutex, then exit WITHOUT
 * release so native thread exit abandons it. */
static mb_worker_i32 mb_worker_mutex_thread_acquire(void){
 mb_worker_mutex_state.worker_abandon=CreateMutexW(0,1,mb_worker_mutex_abandon);
 mb_worker_mutex_keep(mb_worker_mutex_state.worker_abandon);
 if(!mb_worker_mutex_state.worker_abandon)return 0;
 return 1;
}
/* Creator-side after join: WAIT_ABANDONED plus ownership transfer.
 * MFR-2: every check accumulates into `failure` (first failure wins) and
 * falls through to the retirement loop, so the kept tokens retire on ALL
 * exits; -78 is returned only when retirement itself fails. */
static mb_worker_i32 mb_worker_mutex_finish(void){
 mb_worker_i32 failure=1;
 mb_worker_mutex_state.abandon_open=OpenMutexW(0,0,mb_worker_mutex_abandon_folded);
 mb_worker_mutex_keep(mb_worker_mutex_state.abandon_open);
 if(!mb_worker_mutex_state.abandon_open)failure=-72;
 else if(WaitForSingleObjectEx(mb_worker_mutex_state.abandon_open,5000,0)!=MB_WORKER_MUTEX_WAIT_ABANDONED)failure=-73;
 else if(ReleaseMutex(mb_worker_mutex_state.abandon_open)!=1)failure=-74; /* ownership transferred to creator */
 else{
  mb_worker_mutex_state.transfered=1;
  if(WaitForSingleObjectEx(mb_worker_mutex_state.abandon_open,0,0)!=MB_WORKER_MUTEX_WAIT_OBJECT_0)failure=-75;
  else if(ReleaseMutex(mb_worker_mutex_state.abandon_open)!=1)failure=-76;
  else mb_worker_mutex_state.reacquired=1;
 }
 for(mb_worker_u32 i=0;i<mb_worker_mutex_state.token_count;++i)
  if(mb_worker_mutex_state.tokens[i] && !CloseHandle(mb_worker_mutex_state.tokens[i]) && failure==1)failure=-78;
 mb_worker_mutex_state.token_count=0;
 return failure;
}
#endif
