#ifndef MB_OWNED_WORKER_MUTEX_H
#define MB_OWNED_WORKER_MUTEX_H
/* Owned PE fixture: KERNEL32 mutex family through actual imports. Public
 * handles are pointer-sized tokens; BOOL/DWORD results are 32-bit; names are
 * UTF-16. Only documented behavior is asserted: ERROR_ALREADY_EXISTS (183)
 * with a valid handle on an existing name, initial owner ignored on the
 * existing object, non-owner ReleaseMutex fails 288, ASCII case folding plus
 * the Local\ prefix in OpenMutexW, backslash names refused 87, alertable
 * waits fail 50 explicitly, WAIT_TIMEOUT 258, and the worker exiting while
 * owning a named mutex yields WAIT_ABANDONED 0x80 plus ownership transfer at
 * the creator. WaitForSingleObject (plain) is intentionally NOT used here:
 * the existing worker fixture already binds it for thread handles, and the
 * controller composes the shared KERNEL32 entry with this family's mutex
 * route. Every token is retired via the combined CloseHandle dispatch
 * (workers thread handles first, mutex tokens fallback). */
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
/* Integration fix (worker-mutex-deadlock): every public handle token this
 * fixture creates must be retired before the creator-side finish returns, or
 * the native mutex owner can never close (BUSY on outstanding handles) and
 * the SDK driver retries cleanup forever. Requires the combined CloseHandle
 * dispatch proposed for owned_runtime.c (workers thread handles first, mutex
 * tokens second). */
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
 if(WaitForSingleObjectEx(mb_worker_mutex_state.opened,0,0)!=MB_WORKER_MUTEX_WAIT_OBJECT_0)return -68;
 {void *second=OpenMutexW(0x1f0001u,0,mb_worker_mutex_named_folded);mb_worker_mutex_keep(second);
 if(WaitForSingleObjectEx(second,0,0)!=MB_WORKER_MUTEX_WAIT_OBJECT_0)return -68;}
 mb_worker_mutex_state.recursion=WaitForSingleObjectEx(mb_worker_mutex_state.opened,1,0);
 if(mb_worker_mutex_state.recursion!=MB_WORKER_MUTEX_WAIT_TIMEOUT)return -69; /* owner re-wait: finite timeout */
 if(ReleaseMutex(mb_worker_mutex_state.opened)!=1)return -70;
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
/* Creator-side after join: WAIT_ABANDONED plus ownership transfer. */
static mb_worker_i32 mb_worker_mutex_finish(void){
 mb_worker_mutex_state.abandon_open=OpenMutexW(0,0,mb_worker_mutex_abandon_folded);
 mb_worker_mutex_keep(mb_worker_mutex_state.abandon_open);
 if(!mb_worker_mutex_state.abandon_open)return -72;
 mb_worker_u32 wait=WaitForSingleObjectEx(mb_worker_mutex_state.abandon_open,5000,0);
 if(wait!=MB_WORKER_MUTEX_WAIT_ABANDONED)return -73;
 if(ReleaseMutex(mb_worker_mutex_state.abandon_open)!=1)return -74; /* ownership transferred to creator */
 mb_worker_mutex_state.transfered=1;
 if(WaitForSingleObjectEx(mb_worker_mutex_state.abandon_open,0,0)!=MB_WORKER_MUTEX_WAIT_OBJECT_0)return -75;
 if(ReleaseMutex(mb_worker_mutex_state.abandon_open)!=1)return -76;
 mb_worker_mutex_state.reacquired=1;
 for(mb_worker_u32 i=0;i<mb_worker_mutex_state.token_count;++i)
  if(mb_worker_mutex_state.tokens[i] && !CloseHandle(mb_worker_mutex_state.tokens[i]))return -78;
 mb_worker_mutex_state.token_count=0;
 return 1;
}
#endif
