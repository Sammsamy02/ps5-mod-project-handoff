#ifndef MB_OWNED_WORKER_INITONCE_H
#define MB_OWNED_WORKER_INITONCE_H
/* Owned PE fixture: one-time initialization through actual KERNEL32 imports.
 * Public INIT_ONCE storage is exactly eight bytes (RTL_RUN_ONCE { PVOID }),
 * here zeroed u64 cells; both APIs return 32-bit BOOL. Context values are
 * fixture sentinels. Only documented behavior is asserted: begin grants
 * pending=1; complete stores the context; CHECK_ONLY reads a completed object
 * and fails with 997 on a never-begun one; unknown flag bits fail with 87.
 * Duplicate-complete failure is asserted as failure without pinning its
 * adapter-policy error code. */
__declspec(dllimport) mb_worker_i32 InitOnceBeginInitialize(void *,mb_worker_u32,mb_worker_i32 *,void **);
__declspec(dllimport) mb_worker_i32 InitOnceComplete(void *,mb_worker_u32,void *);
#define MB_WORKER_ONCE_FLAGS_CHECK_ONLY 0x1u
#define MB_WORKER_ONCE_FLAGS_UNKNOWN 0x8u
#define MB_WORKER_ONCE_ERROR_IO_PENDING 997u
#define MB_WORKER_ONCE_ERROR_INVALID_PARAMETER 87u
#define MB_WORKER_ONCE_CONTEXT_A ((void *)(mb_worker_ptr)0x51d1beef5a5a0001ULL)
#define MB_WORKER_ONCE_CONTEXT_B ((void *)(mb_worker_ptr)0x51d2beef5a5a0002ULL)
static struct {
 mb_worker_u64 before,once_a,once_b,after;
 mb_worker_i32 thread_pending;
 void *thread_checked_context;
} mb_worker_initonce_state;
_Static_assert(sizeof(mb_worker_initonce_state.once_a)==8 && sizeof(mb_worker_initonce_state.once_b)==8,"Windows INIT_ONCE storage width");
/* Creator-side documented semantics before the worker exists. */
static mb_worker_i32 mb_worker_initonce_prepare(void){
 mb_worker_i32 pending=0;void *context=0;
 mb_worker_initonce_state.before=0x51d3beef5a5a1111ULL;mb_worker_initonce_state.after=0x51d4beef5a5a2222ULL;
 mb_worker_initonce_state.once_a=0;mb_worker_initonce_state.once_b=0;
 mb_worker_initonce_state.thread_pending=0;mb_worker_initonce_state.thread_checked_context=(void *)1;
 if(!InitOnceBeginInitialize(&mb_worker_initonce_state.once_a,0,&pending,&context) || pending!=1)return -45;
 if(!InitOnceComplete(&mb_worker_initonce_state.once_a,0,MB_WORKER_ONCE_CONTEXT_A))return -46;
 pending=-1;context=(void *)1;
 if(!InitOnceBeginInitialize(&mb_worker_initonce_state.once_a,MB_WORKER_ONCE_FLAGS_CHECK_ONLY,&pending,&context) || pending!=0 || context!=MB_WORKER_ONCE_CONTEXT_A)return -47;
 if(InitOnceComplete(&mb_worker_initonce_state.once_a,0,MB_WORKER_ONCE_CONTEXT_A))return -48;
 pending=-1;
 if(InitOnceBeginInitialize(&mb_worker_initonce_state.once_b,MB_WORKER_ONCE_FLAGS_CHECK_ONLY,&pending,&context) || GetLastError()!=MB_WORKER_ONCE_ERROR_IO_PENDING)return -49;
 pending=-1;
 if(InitOnceBeginInitialize(&mb_worker_initonce_state.once_a,MB_WORKER_ONCE_FLAGS_UNKNOWN,&pending,&context) || GetLastError()!=MB_WORKER_ONCE_ERROR_INVALID_PARAMETER)return -50;
 /* A Windows application must re-zero INIT_ONCE storage to restart it. */
 mb_worker_initonce_state.once_a=0;
 return 1;
}
/* Worker thread: acquire the initialization right before the gated wait. */
static mb_worker_i32 mb_worker_initonce_thread_begin(void){
 mb_worker_i32 pending=0;void *context=0;
 if(!InitOnceBeginInitialize(&mb_worker_initonce_state.once_b,0,&pending,&context) || pending!=1)return 0;
 mb_worker_initonce_state.thread_pending=pending;
 return 1;
}
/* Worker thread: complete and verify through CHECK_ONLY after the gate. */
static mb_worker_i32 mb_worker_initonce_thread_complete(void){
 mb_worker_i32 pending=0;void *context=0;
 if(!InitOnceComplete(&mb_worker_initonce_state.once_b,0,MB_WORKER_ONCE_CONTEXT_B))return 0;
 if(!InitOnceBeginInitialize(&mb_worker_initonce_state.once_b,MB_WORKER_ONCE_FLAGS_CHECK_ONLY,&pending,&context) || pending!=0 || context!=MB_WORKER_ONCE_CONTEXT_B)return 0;
 mb_worker_initonce_state.thread_checked_context=context;
 return 1;
}
/* Creator-side after join: the completed object still holds its context. */
static mb_worker_i32 mb_worker_initonce_finish(void){
 mb_worker_i32 pending=0;void *context=0;
 if(!InitOnceBeginInitialize(&mb_worker_initonce_state.once_b,MB_WORKER_ONCE_FLAGS_CHECK_ONLY,&pending,&context) || pending!=0 || context!=MB_WORKER_ONCE_CONTEXT_B)return -51;
 if(mb_worker_initonce_state.thread_pending!=1 || mb_worker_initonce_state.thread_checked_context!=MB_WORKER_ONCE_CONTEXT_B)return -52;
 if(mb_worker_initonce_state.before!=0x51d3beef5a5a1111ULL || mb_worker_initonce_state.after!=0x51d4beef5a5a2222ULL)return -53;
 return 1;
}
#endif
