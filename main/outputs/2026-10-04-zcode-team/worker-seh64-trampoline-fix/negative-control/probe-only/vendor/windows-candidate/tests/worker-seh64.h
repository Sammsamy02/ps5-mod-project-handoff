#ifndef MB_OWNED_WORKER_SEH64_H
#define MB_OWNED_WORKER_SEH64_H
/* PROBE-ONLY NEGATIVE CONTROL (aligned spill removed; rsp probe kept).
 * SEH64 fixture hook (worker-seh64-family): the structured-exception family
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
 *   state owner: SetUnhandledExceptionFilter previous-pointer semantics;
 *   guard-funclet alignment discharge (trampoline review F-1, worker-
 *   seh64-trampoline-fix): a real non-constant FILTER funclet and a
 *   __finally funclet that each (a) verify Win64 call-site stack alignment
 *   from inside the funclet via a naked rsp probe (callee entry rsp ==
 *   8 (mod 16), prologue-shape independent) and (b) force a 16-byte-aligned
 *   __m128 stack spill + aligned global store (movaps class - the class the
 *   constant-filter census never compiled). Codes -138 (filter guard) and
 *   -139 (finally guard).
 * Unique error codes -130..-139 (moduleenum -100..-104, crtstartup -90..-97,
 * fileio -80..-89, vm -110..-117). Layout mirrors are contracts: offsets
 * match windows-services/mb_win_seh64.h exactly. Documented boundaries:
 * filters/bodies here touch globals and their own funclet locals only
 * (guard-rsp PARENT-local addressing is out of scope); floating state is
 * not restored; unhandled exceptions report through the owner instead of
 * terminating like real Windows. */
/* 16-byte-aligned 4-float vector: assignment/spill compiles to the movaps
 * class (aligned vector moves) - no intrinsic header needed (this SDK's
 * fixture compiles are -ffreestanding with no compiler header dir). */
typedef float mb_seh_v4 __attribute__((__vector_size__(16), __aligned__(16)));
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
/* Naked rsp probe (F-1 discharge): at a naked function's entry the return
 * address is already pushed, so rsp here is exactly caller_call_site - 8.
 * The Win64 ABI requires every call-site rsp == 0 (mod 16), i.e. THIS entry
 * rsp == 8 (mod 16), independent of any caller prologue shape. A funclet
 * entered with inverted alignment (review F-1) shifts all of its call sites
 * by 8 and this check fails deterministically. */
__attribute__((naked, noinline, used)) static void mb_seh_sp_probe(mb_seh_u64 *out) {
    __asm__ volatile(
        "movq %rsp, %rax\n\t"
        "movq %rax, (%rcx)\n\t"
        "ret\n\t");
}
static __attribute__((aligned(16))) mb_seh_u32 mb_seh_sse_buf[4];
static __attribute__((aligned(16))) mb_seh_u32 mb_seh_sse_buf2[4];
static volatile mb_seh_u32 mb_seh_sse_filter_ok, mb_seh_sse_finally_ok;
static volatile mb_seh_u64 mb_seh_sse_probe_rsp;
/* Aligned SSE filter funclet (-138): forced 16-byte-aligned __m128 spill
 * (movaps class) through a local, aligned global store round-trip, and the
 * naked-probe call-site alignment check. Invoked by the owner's guard
 * trampoline during the search pass. */
__attribute__((noinline)) static mb_seh_i32 mb_seh_filter_sse(void) {
    volatile mb_seh_v4 spill;
    mb_seh_v4 v;
    mb_seh_u64 probe = 0;
    unsigned i;
    static const mb_seh_u32 mb_seh_sse_want[4] = {
        0x3F800000u /*1.0f*/, 0x40000000u /*2.0f*/,
        0x40400000u /*3.0f*/, 0x40800000u /*4.0f*/ };
    mb_seh_sse_buf[0] = 0x3F800000u; mb_seh_sse_buf[1] = 0x40000000u;
    mb_seh_sse_buf[2] = 0x40400000u; mb_seh_sse_buf[3] = 0x40800000u;
    (void)spill; (void)v; /* PROBE-ONLY CONTROL: aligned spill removed */
    mb_seh_sp_probe(&probe);
    mb_seh_sse_probe_rsp = probe;
    if ((probe & 15) != 8) return 0; /* funclet frame not Win64-aligned */
    for (i = 0; i < 4; ++i)
        if (mb_seh_sse_buf[i] != mb_seh_sse_want[i]) return 0;
    mb_seh_sse_filter_ok = 1;
    return 1;
}
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
/* Aligned SSE filter guard (-138, trampoline review F-1): a REAL non-constant
 * filter funclet invoked through the guard trampoline during the search
 * pass. If the funclet entry alignment is inverted the funclet itself
 * returns 0 (probe check) or faults on the aligned spill; either way the
 * exception goes unhandled, RaiseException returns, and this frame must
 * retire with -138. */
__attribute__((noinline, disable_tail_calls)) static mb_seh_i32 mb_seh_guard_sse_filter(void) {
    mb_seh_sse_filter_ok = 0;
    __try {
        RaiseException(0xE0005E68u, 0, 0, (const mb_seh_u64 *)0);
        return -138; /* handled path must transfer, never return here */
    } __except (mb_seh_filter_sse()) {
        if (!mb_seh_sse_filter_ok) return -138;
    }
    if (!mb_seh_sse_filter_ok) return -138;
    return 1;
}
/* Aligned SSE __finally guard (-139, trampoline review F-1): the unwind-pass
 * guard-trampoline invocation of a __finally funclet that forces the same
 * aligned spill + naked rsp probe inside the funclet body. */
__attribute__((noinline, disable_tail_calls)) static mb_seh_i32 mb_seh_guard_sse_finally(void) {
    volatile mb_seh_u32 fin_done = 0;
    mb_seh_sse_finally_ok = 0;
    __try {
        __try {
            RaiseException(0xE0005E69u, 0, 0, (const mb_seh_u64 *)0);
            return -139; /* must transfer, never return here */
        } __finally {
            volatile mb_seh_v4 spill;
            mb_seh_v4 v;
            mb_seh_u64 probe = 0;
            unsigned i;
            static const mb_seh_u32 mb_seh_sse_want2[4] = {
                0x40A00000u /*5.0f*/, 0x40C00000u /*6.0f*/,
                0x40E00000u /*7.0f*/, 0x41000000u /*8.0f*/ };
            mb_seh_sse_buf2[0] = 0x40A00000u; mb_seh_sse_buf2[1] = 0x40C00000u;
            mb_seh_sse_buf2[2] = 0x40E00000u; mb_seh_sse_buf2[3] = 0x41000000u;
            (void)spill; (void)v; /* PROBE-ONLY CONTROL: aligned spill removed */
            mb_seh_sp_probe(&probe);
            mb_seh_sse_probe_rsp = probe;
            if ((probe & 15) == 8) {
                for (i = 0; i < 4; ++i)
                    if (mb_seh_sse_buf2[i] != mb_seh_sse_want2[i]) goto fin_bad;
                mb_seh_sse_finally_ok = 1;
            }
        fin_bad:;
        }
    } __except (1) {
        fin_done = 1;
    }
    if (fin_done != 1 || !mb_seh_sse_finally_ok) return -139;
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
    /* Guard-funclet alignment discharge (F-1): real filter funclet with an
     * aligned SSE spill (-138), then the __finally funclet variant during
     * the unwind pass (-139). */
    {
        mb_seh_i32 sse_filter = mb_seh_guard_sse_filter();
        if (sse_filter != 1) return sse_filter;
    }
    {
        mb_seh_i32 sse_fin = mb_seh_guard_sse_finally();
        if (sse_fin != 1) return sse_fin;
    }
    return 1;
}
#endif
