#ifndef MB_OWNED_WORKER_STATICTLS_H
#define MB_OWNED_WORKER_STATICTLS_H
/* Combined-payload fixture hook (worker-static-tls-7fam): the PE static-TLS
 * family through actual PE imports. The consumer image carries a real TLS
 * directory (data directory 9, patched post-link by build.py onto this
 * image's .rdataT/__tls_used struct - lld-link does not synthesize TLS
 * directories, verified). The native composition admits the directory via
 * the pe_module admission ticket, registers the record at first bind (index
 * written into mb_stls_tls_region+16), dispatches the two callbacks below
 * through the win64cc adapter, and unloads cleanly. The fixture exercises
 * four of the eight bound adapter names as imports: __tls_index (data),
 * __tls_start (data), __tls_end (data), __tls_import (thunk). Error returns
 * -120..-127 are unique across this tree's fixtures (vm -110..-117,
 * moduleenum -100..-109, crtstartup -90..-97, fileio -80..-89). */
typedef __UINT32_TYPE__ mb_stls_u32;
typedef __UINT64_TYPE__ mb_stls_u64;
typedef __INT32_TYPE__ mb_stls_i32;
typedef __UINT8_TYPE__ mb_stls_u8;
__declspec(dllimport) mb_stls_u32 __tls_index;
__declspec(dllimport) extern const mb_stls_u8 __tls_start[];
__declspec(dllimport) extern const mb_stls_u8 __tls_end[];
__declspec(dllimport) void *__tls_import(mb_stls_u32, mb_stls_u64);
__declspec(dllimport) void SetLastError(mb_stls_u32);
#pragma section(".tls", read, write)
#pragma section(".rdataC", read)
#pragma section(".rdataT", read)
/* One .tls object: 16 template bytes, 4-byte AddressOfIndex cell (the owner
 * writes the live index here at register), 4-byte end marker. A single
 * object fixes every offset regardless of declaration ordering, and the
 * reference from TlsProbe-style code keeps /opt:ref from dropping it. */
__declspec(allocate(".tls")) mb_stls_u8 mb_stls_tls_region[24] = {
    (mb_stls_u8)0x54, (mb_stls_u8)0x4c, (mb_stls_u8)0x53, (mb_stls_u8)0x37,
    (mb_stls_u8)0xf0, (mb_stls_u8)0x7f, (mb_stls_u8)0x11, (mb_stls_u8)0x3b,
    (mb_stls_u8)0xa5, (mb_stls_u8)0x5c, (mb_stls_u8)0x92, (mb_stls_u8)0x6e,
    (mb_stls_u8)0x30, (mb_stls_u8)0xd4, (mb_stls_u8)0x88, (mb_stls_u8)0x1c,
    0u, 0u, 0u, 0u,
    (mb_stls_u8)0xe9, (mb_stls_u8)0x37, (mb_stls_u8)0xa1, (mb_stls_u8)0x7d};
#define mb_stls_index_cell (*(volatile mb_stls_u32 *)(mb_stls_tls_region + 16))
static void mb_stls_cb1(void *, mb_stls_u32, void *);
static void mb_stls_cb2(void *, mb_stls_u32, void *);
typedef void (*mb_stls_cb_t)(void *, mb_stls_u32, void *);
__declspec(allocate(".rdataC")) const mb_stls_cb_t mb_stls_callbacks[3] = {
    mb_stls_cb1, mb_stls_cb2, (mb_stls_cb_t)0};
typedef struct {mb_stls_u64 Start, End, Index, Callbacks; mb_stls_u32 ZeroFill, Chars;} mb_stls_dir64;
__declspec(allocate(".rdataT")) const mb_stls_dir64 __tls_used = {
    (mb_stls_u64)mb_stls_tls_region,
    (mb_stls_u64)(mb_stls_tls_region + sizeof(mb_stls_tls_region)),
    (mb_stls_u64)&mb_stls_index_cell,
    (mb_stls_u64)&mb_stls_callbacks[0], 0u, 0x500000u};
/* Deterministic callback evidence: (reason, callback id) pairs.
 * [(1,1),(1,2)] at process attach, then [(2,1),(2,2)] at every
 * mb_win_static_tls_thread_attach worker-call window - the runner opens
 * exactly four: exercise, the begin+collect admission window (collect
 * reuses begin's worker-call admission by design), and two file I/O calls
 * when MB_OWNED_FILEIO is on - then [(0,2),(0,1)] descending at process
 * detach: 12 entries with file I/O, 10 without, fully validated by the
 * final detach callback. */
static volatile mb_stls_u32 mb_stls_log[16][2];
static volatile mb_stls_u32 mb_stls_log_n;
static void mb_stls_log_append(mb_stls_u32 id, mb_stls_u32 reason){
    mb_stls_u32 n=__atomic_fetch_add(&mb_stls_log_n,1,__ATOMIC_ACQ_REL);
    if(n<16){mb_stls_log[n][0]=reason;mb_stls_log[n][1]=id;}
}
/* Cross-thread evidence, published by the internal worker thread. Windows
 * gives a fresh thread its static-TLS block before its start routine runs;
 * this payload documents lazy/no-alloc for never-attached threads, so the
 * honest observation for the internal thread is: same index, NULL block,
 * readable template - per-thread slot isolation under one index. */
static volatile mb_stls_u32 mb_stls_thread_observed;
static volatile mb_stls_u32 mb_stls_thread_index;
static volatile void *mb_stls_thread_block;
static volatile mb_stls_u32 mb_stls_thread_template_ok;
static void *volatile mb_stls_main_block;
static mb_stls_i32 mb_stls_log_matches(mb_stls_u32 n, mb_stls_u32 pairs[][2]){
    if(__atomic_load_n(&mb_stls_log_n,__ATOMIC_ACQUIRE)!=n)return 0;
    for(mb_stls_u32 i=0;i<n;++i){
        if(mb_stls_log[i][0]!=pairs[i][0]||mb_stls_log[i][1]!=pairs[i][1])return 0;
    }
    return 1;
}
static mb_stls_i32 mb_stls_template_at(const void *p){
    static const mb_stls_u8 want[16]={0x54,0x4c,0x53,0x37,0xf0,0x7f,0x11,0x3b,
                                      0xa5,0x5c,0x92,0x6e,0x30,0xd4,0x88,0x1c};
    const mb_stls_u8 *b=(const mb_stls_u8 *)p;
    for(mb_stls_u32 i=0;i<16;++i)if(b[i]!=want[i])return 0;
    return 1;
}
/* cb1 dispatches LAST in the descending detach order, so its r==0 branch
 * observes the complete log (cb2's earlier validation always saw one entry
 * missing - found and fixed during this composition). */
static void mb_stls_cb1(void *h, mb_stls_u32 r, void *v){
    (void)h;(void)v;mb_stls_log_append(1,r);
    if(r==0){
        /* Final dispatch of the run: validate the complete callback log
         * structurally - process-attach pair, ascending (2,1),(2,2) pairs
         * for every worker-call window that actually ran (the exact count
         * depends on how far the runner progressed; cleanup-fault runs
         * abort early by design), then the descending detach pair - and
         * flag any mismatch in this image's own TLS index cell (high bit).
         * The cell lives in the mapped image, so the native composition can
         * audit it inside the entry wrapper while the image is still mapped
         * - the thread's Windows last-error slot cannot carry this verdict
         * because the owner's own detach path resets it via tls_get after
         * the callbacks dispatch (documented tls_get contract). */
        mb_stls_u32 n=__atomic_load_n(&mb_stls_log_n,__ATOMIC_ACQUIRE);
        mb_stls_u32 ok=n>=4 && !(n&1);
        if(ok)for(mb_stls_u32 i=0;ok&&i<n;++i){
            mb_stls_u32 want_r=(i==0||i==1)?1u:(i>=n-2)?0u:2u;
            mb_stls_u32 want_c=(i>=n-2)?(n-i):((i&1)?2u:1u);
            if(mb_stls_log[i][0]!=want_r||mb_stls_log[i][1]!=want_c)ok=0;
        }
        if(!ok)mb_stls_index_cell|=0x80000000u;
    }
}
static void mb_stls_cb2(void *h, mb_stls_u32 r, void *v){(void)h;(void)v;mb_stls_log_append(2,r);}
/* Internal worker thread (no attach notification): publish observations. */
static void mb_stls_thread_publish(void){
    mb_stls_u32 idx=__tls_index;
    __atomic_store_n(&mb_stls_thread_index,idx,__ATOMIC_RELEASE);
    __atomic_store_n(&mb_stls_thread_block,__tls_import(idx,0),__ATOMIC_RELEASE);
    __atomic_store_n(&mb_stls_thread_template_ok,
        (mb_stls_u32)(mb_stls_template_at(__tls_start) && __tls_end==__tls_start+24),__ATOMIC_RELEASE);
    __atomic_store_n(&mb_stls_thread_observed,1,__ATOMIC_RELEASE);
}
/* LoaderExercise window (module ACTIVE): load, index, block, order. */
static mb_stls_i32 mb_stls_exercise(void){
    mb_stls_u32 idx=__tls_index;
    if(idx==0xffffffffu||idx!=__tls_index)return -120; /* index assigned, stable */
    void *blk=__tls_import(idx,0);
    if(!blk||!mb_stls_template_at(blk))return -121;    /* block = template copy */
    if(__tls_import(idx,0)!=blk)return -122;           /* same-thread idempotence */
    *(volatile mb_stls_u32 *)((mb_stls_u8 *)blk+16)=0x5a17beefu; /* scratch write */
    if(*(volatile mb_stls_u32 *)((mb_stls_u8 *)blk+16)!=0x5a17beefu)return -123;
    mb_stls_main_block=blk;
    mb_stls_u32 want[4][2]={{1,1},{1,2},{2,1},{2,2}};  /* attach + this window */
    if(!mb_stls_log_matches(4,want))return -124;       /* callbacks fired in order */
    if(!mb_stls_template_at(__tls_start)||__tls_end!=__tls_start+24)return -125;
    return 1;
}
/* WorkerBegin window: the entered thread keeps its block across calls. */
static mb_stls_i32 mb_stls_begin_check(void){
    void *blk=__tls_import(__tls_index,0);
    if(blk!=(void *)mb_stls_main_block)return -126;    /* same thread, same block */
    return 1;
}
/* WorkerCollect window: isolation from the internal thread + persistence. */
static mb_stls_i32 mb_stls_collect_check(void){
    if(__atomic_load_n(&mb_stls_thread_observed,__ATOMIC_ACQUIRE)!=1)return -127;
    if(__atomic_load_n(&mb_stls_thread_index,__ATOMIC_ACQUIRE)!=__tls_index)return -127;
    if((void *)__atomic_load_n(&mb_stls_thread_block,__ATOMIC_ACQUIRE)!=0)return -127;
    if(__atomic_load_n(&mb_stls_thread_template_ok,__ATOMIC_ACQUIRE)!=1)return -127;
    void *blk=__tls_import(__tls_index,0);
    if(blk!=(void *)mb_stls_main_block)return -127;    /* per-thread persistence */
    if(!mb_stls_template_at(blk))return -127;          /* bytes 0..16 untouched */
    if(*(volatile mb_stls_u32 *)((mb_stls_u8 *)blk+16)!=0x5a17beefu)return -127;
    /* P-attach + the exercise and begin/collect attach windows (collect runs
     * inside begin's admission window; file I/O attaches happen later). */
    mb_stls_u32 want[6][2]={{1,1},{1,2},{2,1},{2,2},{2,1},{2,2}};
    if(!mb_stls_log_matches(6,want))return -127;
    return 1;
}
#endif
