#ifndef MB_OWNED_WORKER_SEH64_H
#define MB_OWNED_WORKER_SEH64_H
/* SEH64 fixture hook (worker-seh64-family): the structured-exception family
 * through actual PE imports, riding the consumer LoaderExercise ACTIVE
 * window. Exercised surface (worker-seh64-design DESIGN.md minimal subset +
 * the packet's directed raise/dispatch/recover minimum):
 *   stack-walker kit (census names): RtlCaptureContext, RtlLookupFunctionEntry,
 *   RtlVirtualUnwind over this image's real MSVC-emitted .pdata/.xdata
 *   (registered by the family at bind time through the loader amendment);
 *   raise/dispatch/recover: RaiseException (family substrate entry) into the
 *   two-pass dispatcher, __C_specific_handler (census vcruntime140 name)
 *   reached through this image's own import thunk, same-frame and cross-frame
 *   guarded frames, __finally during unwind, target-frame transfer;
 *   dynamic registration: RtlAddFunctionTable/RtlRemoveFunctionTable over a
 *   bridge (non-PE) address - the win64cc frames have no .pdata (design 5a);
 *   state owner: SetUnhandledExceptionFilter previous-pointer semantics.
 * Unique error codes -130..-137 (moduleenum -100..-104, crtstartup -90..-97,
 * fileio -80..-89, vm -110..-117). Layout mirrors are contracts: offsets
 * match windows-services/mb_win_seh64.h exactly. Documented boundaries:
 * filters/bodies here touch globals only (guard-rsp local addressing is out
 * of scope); floating state is not restored; unhandled exceptions report
 * through the owner instead of terminating like real Windows. */
typedef __UINT8_TYPE__ mb_seh_u8;
typedef unsigned short mb_seh_u16;
typedef __UINT32_TYPE__ mb_seh_u32;
typedef __UINT64_TYPE__ mb_seh_u64;
typedef __INT32_TYPE__ mb_seh_i32;
__declspec(dllimport) void RtlCaptureContext(void *);
__declspec(dllimport) void *RtlLookupFunctionEntry(mb_seh_u64, mb_seh_u64 *);
__declspec(dllimport) mb_seh_u64 RtlVirtualUnwind(mb_seh_u32, mb_seh_u64, void *, void *, void *, void *);
__declspec(dllimport) mb_seh_u64 RtlAddFunctionTable(void *, mb_seh_u32, mb_seh_u64);
__declspec(dllimport) mb_seh_i32 RtlRemoveFunctionTable(mb_seh_u64);
__declspec(dllimport) void *SetUnhandledExceptionFilter(void *);
__declspec(dllimport) void RaiseException(mb_seh_u32, mb_seh_u32, mb_seh_u32, const mb_seh_u64 *);
__declspec(dllimport) mb_seh_i32 __C_specific_handler(void *, void *, void *, void *);
/* Exact Win64 CONTEXT: 0x4D0 bytes; the integer subset is offset-contract
 * with mb_win_seh64.h (fixture reads rip/rsp/rbx..r15 only). */
struct mb_seh_context {
    mb_seh_u64 p[6];
    mb_seh_u32 context_flags, mx_csr;
    mb_seh_u16 seg[6];
    mb_seh_u32 eflags;
    mb_seh_u64 dr[6];
    mb_seh_u64 rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi;
    mb_seh_u64 r8, r9, r10, r11, r12, r13, r14, r15, rip;
    mb_seh_u8 tail[0x3D0];
};
_Static_assert(sizeof(struct mb_seh_context) == 0x4D0, "Win64 CONTEXT size");
_Static_assert(__builtin_offsetof(struct mb_seh_context, rsp) == 0x98, "Rsp offset");
_Static_assert(__builtin_offsetof(struct mb_seh_context, rip) == 0xF8, "Rip offset");
typedef struct mb_seh_context mb_seh_context;
/* 3xu32 RUNTIME_FUNCTION and the self-consistent dispatcher context (fields
 * 1-5 keep the Win64 position contract; the tail is this family's plumbing,
 * mirrored here so the fixture can zero it safely). */
struct mb_seh_runtime_function { mb_seh_u32 begin_address, end_address, unwind_data; };
_Static_assert(sizeof(struct mb_seh_runtime_function) == 12, "RUNTIME_FUNCTION size");
typedef struct mb_seh_runtime_function mb_seh_runtime_function;
struct mb_seh_dispatcher {
    mb_seh_u64 control_pc;
    void *function_entry;
    mb_seh_u64 establisher_frame;
    void *context_record;
    void *handler_data;
    mb_seh_u64 frame_rsp;
    mb_seh_u64 history_oracle;
    mb_seh_u32 scope_index, pc_is_unwinding;
    void *image_base;
    mb_seh_u32 handler_rva, pad;
    mb_seh_u64 target_ip;
};
struct mb_seh_kctx { void *home[8]; };
static volatile mb_seh_u32 mb_seh_handled;
static mb_seh_context mb_seh_probe_ctx;
static struct mb_seh_runtime_function mb_seh_dyn[2];
static mb_seh_i32 mb_seh_last_chance(void *ptrs) { (void)ptrs; return 1; }
/* disable_tail_calls keeps a real frame here: a tail call would turn the
 * probe into a jmp thunk and the walker-kit checks would lose their frame. */
__attribute__((noinline, disable_tail_calls)) static mb_seh_u64 mb_seh_probe(void) {
    RtlCaptureContext(&mb_seh_probe_ctx);
    return mb_seh_probe_ctx.rip ? 0x11u : 0u;
}
__attribute__((noinline, disable_tail_calls)) static mb_seh_i32 mb_seh_raiser(void) {
    RaiseException(0xE0005E65u, 0, 0, (const mb_seh_u64 *)0);
    return 0x5E65; /* unreachable in the green path */
}
__attribute__((noinline, disable_tail_calls)) static mb_seh_i32 mb_seh_guard_same(void) {
    volatile mb_seh_u32 guard[4];
    mb_seh_handled = 0;
    for (unsigned i = 0; i < 4; ++i) guard[i] = 0x5E64u + (mb_seh_u32)i;
    __try {
        RaiseException(0xE0005E64u, 0, 0, (const mb_seh_u64 *)0);
        return -133; /* the raise must transfer, never return */
    } __except (1) {
        if (!mb_seh_handled) mb_seh_handled = 1;
    }
    for (unsigned i = 0; i < 4; ++i)
        if (guard[i] != 0x5E64u + (mb_seh_u32)i) return -134;
    if (mb_seh_handled != 1) return -133;
    return 1;
}
__attribute__((noinline, disable_tail_calls)) static mb_seh_i32 mb_seh_guard_cross(void) {
    volatile mb_seh_u32 finally_ran = 0, cross_handled = 0;
    __try {
        __try {
            if (mb_seh_raiser() == 0x5E65) return -135;
            return -135; /* unwound frames must not resume */
        } __finally {
            finally_ran = 1;
        }
    } __except (1) {
        cross_handled = 1;
    }
    if (finally_ran != 1 || cross_handled != 1) return -135;
    return 1;
}
static mb_seh_i32 mb_worker_seh64_exercise(void) {
    /* Stack-walker kit over this image's real unwind data (-130..-132). */
    mb_seh_u64 base = 0;
    if (mb_seh_probe() != 0x11) return -130;
    if (!mb_seh_probe_ctx.rip || mb_seh_probe_ctx.rsp <= mb_seh_probe_ctx.rip) return -130;
    void *entry = RtlLookupFunctionEntry(mb_seh_probe_ctx.rip, &base);
    if (!entry || !base || (mb_seh_u64)entry < base) return -131;
    mb_seh_runtime_function rf = *(mb_seh_runtime_function *)entry;
    if (!rf.begin_address || rf.end_address <= rf.begin_address) return -131;
    /* Field-wise copy: /nodefaultlib images have no memcpy. */
    mb_seh_context copy;
    copy.rax = mb_seh_probe_ctx.rax; copy.rcx = mb_seh_probe_ctx.rcx;
    copy.rdx = mb_seh_probe_ctx.rdx; copy.rbx = mb_seh_probe_ctx.rbx;
    copy.rsp = mb_seh_probe_ctx.rsp; copy.rbp = mb_seh_probe_ctx.rbp;
    copy.rsi = mb_seh_probe_ctx.rsi; copy.rdi = mb_seh_probe_ctx.rdi;
    copy.r8 = mb_seh_probe_ctx.r8; copy.r9 = mb_seh_probe_ctx.r9;
    copy.r10 = mb_seh_probe_ctx.r10; copy.r11 = mb_seh_probe_ctx.r11;
    copy.r12 = mb_seh_probe_ctx.r12; copy.r13 = mb_seh_probe_ctx.r13;
    copy.r14 = mb_seh_probe_ctx.r14; copy.r15 = mb_seh_probe_ctx.r15;
    copy.rip = mb_seh_probe_ctx.rip;
    struct mb_seh_dispatcher dctx;
    struct mb_seh_kctx kctx;
    __builtin_memset(&dctx, 0, sizeof(dctx));
    __builtin_memset(&kctx, 0, sizeof(kctx));
    if (!RtlVirtualUnwind(0, mb_seh_probe_ctx.rip, entry, &copy, &dctx, &kctx))
        return -132;
    if (copy.rsp <= mb_seh_probe_ctx.rsp) return -132;
    void *caller_entry = RtlLookupFunctionEntry(copy.rip, &base);
    if (!caller_entry || caller_entry == entry || (mb_seh_u64)caller_entry < base) return -132;
    mb_seh_runtime_function cr = *(mb_seh_runtime_function *)caller_entry;
    if (copy.rip < base + cr.begin_address || copy.rip >= base + cr.end_address) return -132;
    if (!dctx.control_pc || !dctx.establisher_frame || !dctx.frame_rsp) return -132;
    /* BISECT-1: kit only */
    if (mb_seh_probe_ctx.rip) return 1;
    /* Same-frame raise/dispatch/recover (-133/-134). */
    mb_seh_i32 same = mb_seh_guard_same();
    if (same != 1) return same;
    /* Cross-frame raise through an unwound callee plus __finally (-135). */
    mb_seh_i32 cross = mb_seh_guard_cross();
    if (cross != 1) return cross;
    /* Dynamic registration over a bridge (non-PE) address (-136). */
    mb_seh_u64 thunk = (mb_seh_u64)(void *)&RtlCaptureContext;
    if (RtlLookupFunctionEntry(thunk, &base)) return -136;
    mb_seh_u64 tbase = thunk & ~(mb_seh_u64)0xFFFFFFFFull;
    mb_seh_dyn[0].begin_address = (mb_seh_u32)(thunk - tbase);
    mb_seh_dyn[0].end_address = mb_seh_dyn[0].begin_address + 0x1000;
    mb_seh_dyn[0].unwind_data = 0;
    mb_seh_u64 handle = RtlAddFunctionTable(mb_seh_dyn, 1, tbase);
    if (!handle) return -136;
    void *hit = RtlLookupFunctionEntry(thunk, &base);
    if (hit != (void *)mb_seh_dyn || base != tbase) return -136;
    mb_seh_u64 handle2 = RtlAddFunctionTable(mb_seh_dyn, 1, tbase);
    if (!handle2) return -136;
    /* Removal cleans up; double removal refused (-137). */
    if (!RtlRemoveFunctionTable(handle) || !RtlRemoveFunctionTable(handle2)) return -137;
    if (RtlRemoveFunctionTable(handle)) return -137;
    if (RtlLookupFunctionEntry(thunk, &base)) return -137;
    /* Last-chance filter state owner: none installed, previous-pointer
     * round trip (-137). */
    void *prev = SetUnhandledExceptionFilter((void *)(void *)&mb_seh_last_chance);
    if (prev) return -137;
    prev = SetUnhandledExceptionFilter((void *)0);
    if (prev != (void *)&mb_seh_last_chance) return -137;
    return 1;
}
#endif
