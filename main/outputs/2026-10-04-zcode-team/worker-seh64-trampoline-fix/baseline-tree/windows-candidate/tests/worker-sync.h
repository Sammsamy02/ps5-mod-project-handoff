#ifndef MB_OWNED_WORKER_SYNC_H
#define MB_OWNED_WORKER_SYNC_H
/* Private owned PE test declarations. Native pthread layouts never enter DLL
 * object storage: CRITICAL_SECTION uses 40 bytes, SRW/CV use eight bytes. */
__declspec(dllimport) void InitializeCriticalSection(void *);
__declspec(dllimport) void EnterCriticalSection(void *);
__declspec(dllimport) void LeaveCriticalSection(void *);
__declspec(dllimport) void DeleteCriticalSection(void *);
__declspec(dllimport) mb_worker_i32 TryEnterCriticalSection(void *);
__declspec(dllimport) mb_worker_i32 InitializeCriticalSectionAndSpinCount(void *,mb_worker_u32);
__declspec(dllimport) mb_worker_u32 SetCriticalSectionSpinCount(void *,mb_worker_u32);
__declspec(dllimport) void InitializeSRWLock(void *);
__declspec(dllimport) void AcquireSRWLockShared(void *);
__declspec(dllimport) void AcquireSRWLockExclusive(void *);
__declspec(dllimport) void ReleaseSRWLockShared(void *);
__declspec(dllimport) void ReleaseSRWLockExclusive(void *);
__declspec(dllimport) unsigned char TryAcquireSRWLockShared(void *);
__declspec(dllimport) unsigned char TryAcquireSRWLockExclusive(void *);
__declspec(dllimport) void InitializeConditionVariable(void *);
__declspec(dllimport) mb_worker_i32 SleepConditionVariableSRW(void *,void *,mb_worker_u32,mb_worker_u32);
__declspec(dllimport) void WakeConditionVariable(void *);
__declspec(dllimport) void WakeAllConditionVariable(void *);
__declspec(dllimport) mb_worker_i32 WaitOnAddress(volatile void *,void *,mb_worker_size,mb_worker_u32);
__declspec(dllimport) void WakeByAddressSingle(void *);
__declspec(dllimport) void WakeByAddressAll(void *);
static struct {
 mb_worker_u64 before,critical[5],after,srw,cv;
 mb_worker_u32 ready,held,critical_live;
} mb_worker_sync;
_Static_assert(sizeof(mb_worker_sync.critical)==40 && sizeof(mb_worker_sync.srw)==8 && sizeof(mb_worker_sync.cv)==8,"Windows synchronization object widths");
static mb_worker_i32 mb_worker_sync_prepare(mb_worker_u32 *gate){
 mb_worker_sync.before=0x51c1beef5a5a1234ULL;mb_worker_sync.after=0x51c2beef5a5a5678ULL;
 mb_worker_sync.ready=0;mb_worker_sync.held=0;mb_worker_sync.critical_live=0;
 mb_worker_u32 unequal=1,equal=0;
 if(!WaitOnAddress(gate,&unequal,4,0) || WaitOnAddress(gate,&equal,4,0) || GetLastError()!=1460)return -31;
 InitializeCriticalSection(mb_worker_sync.critical);mb_worker_sync.critical_live=1;
 if(SetCriticalSectionSpinCount(mb_worker_sync.critical,7)!=0)return -32;
 EnterCriticalSection(mb_worker_sync.critical);EnterCriticalSection(mb_worker_sync.critical);
 mb_worker_sync.held=2;
 if(!TryEnterCriticalSection(mb_worker_sync.critical))return -33;mb_worker_sync.held=3;
 LeaveCriticalSection(mb_worker_sync.critical);LeaveCriticalSection(mb_worker_sync.critical);LeaveCriticalSection(mb_worker_sync.critical);
 mb_worker_sync.held=0;
 DeleteCriticalSection(mb_worker_sync.critical);mb_worker_sync.critical_live=0;
 if(!InitializeCriticalSectionAndSpinCount(mb_worker_sync.critical,11))return -34;mb_worker_sync.critical_live=1;
 mb_worker_u32 previous=SetCriticalSectionSpinCount(mb_worker_sync.critical,15);
 if(previous!=0 && previous!=11)return -35;
 InitializeSRWLock(&mb_worker_sync.srw);InitializeConditionVariable(&mb_worker_sync.cv);
 if(!TryAcquireSRWLockShared(&mb_worker_sync.srw))return -36;ReleaseSRWLockShared(&mb_worker_sync.srw);
 if(!TryAcquireSRWLockExclusive(&mb_worker_sync.srw))return -37;ReleaseSRWLockExclusive(&mb_worker_sync.srw);
 AcquireSRWLockExclusive(&mb_worker_sync.srw);
 if(SleepConditionVariableSRW(&mb_worker_sync.cv,&mb_worker_sync.srw,0,0) || GetLastError()!=1460){ReleaseSRWLockExclusive(&mb_worker_sync.srw);return -38;}
 ReleaseSRWLockExclusive(&mb_worker_sync.srw);WakeConditionVariable(&mb_worker_sync.cv);
 EnterCriticalSection(mb_worker_sync.critical);mb_worker_sync.held=1;
 return 1;
}
static mb_worker_i32 mb_worker_sync_ready(void *worker){
 mb_worker_u32 saved_error=GetLastError(),attempts=0;
 AcquireSRWLockExclusive(&mb_worker_sync.srw);
 while(!mb_worker_sync.ready){
  if(!SleepConditionVariableSRW(&mb_worker_sync.cv,&mb_worker_sync.srw,100,0) && GetLastError()!=1460){ReleaseSRWLockExclusive(&mb_worker_sync.srw);return -39;}
  /* A callback ABI/parameter failure can finish before publishing ready. Never
   * turn that diagnostic into an infinite wait, including spurious wakes. */
  if(!mb_worker_sync.ready && (WaitForSingleObject(worker,0)!=258 || ++attempts==20)){ReleaseSRWLockExclusive(&mb_worker_sync.srw);return -44;}
 }
 ReleaseSRWLockExclusive(&mb_worker_sync.srw);
 AcquireSRWLockShared(&mb_worker_sync.srw);mb_worker_u32 ready=mb_worker_sync.ready;ReleaseSRWLockShared(&mb_worker_sync.srw);
 LeaveCriticalSection(mb_worker_sync.critical);mb_worker_sync.held=0;
 SetLastError(saved_error);
 return ready==1 ? 1 : -40;
}
static mb_worker_i32 mb_worker_sync_started(void){
 mb_worker_i32 failure=TryEnterCriticalSection(mb_worker_sync.critical)!=0;
 if(failure)LeaveCriticalSection(mb_worker_sync.critical);
 AcquireSRWLockExclusive(&mb_worker_sync.srw);mb_worker_sync.ready=failure?2:1;
 WakeAllConditionVariable(&mb_worker_sync.cv);ReleaseSRWLockExclusive(&mb_worker_sync.srw);
 return failure?-41:1;
}
static void mb_worker_sync_unblock(mb_worker_u32 *gate){
 __atomic_store_n(gate,1,__ATOMIC_RELEASE);WakeByAddressSingle(gate);WakeByAddressAll(gate);
 while(mb_worker_sync.held){LeaveCriticalSection(mb_worker_sync.critical);--mb_worker_sync.held;}
}
static mb_worker_i32 mb_worker_sync_finish(void){
 if(mb_worker_sync.held)return -42;
 if(mb_worker_sync.critical_live){DeleteCriticalSection(mb_worker_sync.critical);mb_worker_sync.critical_live=0;}
 return mb_worker_sync.before==0x51c1beef5a5a1234ULL && mb_worker_sync.after==0x51c2beef5a5a5678ULL?1:-43;
}
#endif
