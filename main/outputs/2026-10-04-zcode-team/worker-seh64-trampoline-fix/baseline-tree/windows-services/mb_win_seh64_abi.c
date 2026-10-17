#include "mb_win_seh64_abi.h"
#if !defined(__x86_64__)
#error "SEH64 Windows ABI requires AMD64"
#endif
#define WIN64 __attribute__((ms_abi))

/* ---------------------------------------------------- naked Win64 plane */
/* Guard trampoline (design H2, bounded form): invoke fn(arg) with RSP inside
 * the establishing frame: [establisher_rsp-0x28] = return here,
 * [establisher_rsp-0x20..-1] = Win64 shadow space. Bodies/filters that
 * address parent locals through the guard rsp are unsupported (documented);
 * this family's fixtures exercise globals-only bodies. */
/* Naked trampolines take the DEFAULT (SysV-style) argument registers of both
 * compilation routes (host Mach-O x86_64 and the freebsd9/PS5 frontend: the
 * PS5 backend drops ms_abi on naked definitions, so win64cc entry is not
 * available here). The body shuffles into the Win64 registers itself; every
 * C-level caller is native-CC. The bound import entry (svc_capture_context)
 * is the one exception: PE code calls it through the IAT with rcx, so its
 * body keeps the Win64 rcx contract and the only internal caller (svc_raise)
 * invokes it through an explicit rcx-setting asm call. */
__attribute__((naked, used, noinline))
static int32_t call_at_frame_win64(void *fn, uint64_t establisher_rsp, void *arg)
{
    __asm__ volatile(
        "movq %rdx, %r8\n\t"
        "movq %rsi, %rdx\n\t"
        "movq %rdi, %rcx\n\t"
        "pushq %rbx\n\t"
        "pushq %rsi\n\t"
        "movq %rsp, %rbx\n\t"
        "movq %rdx, %rsi\n\t"
        "subq $0x28, %rsi\n\t"
        "leaq 1f(%rip), %rax\n\t"
        "movq %rax, 0x20(%rsi)\n\t"
        "movq %rcx, %r10\n\t"
        "movq %r8, %rcx\n\t"
        "movq %rsi, %rsp\n\t"
        "call *%r10\n\t"
        "1:\n\t"
        "movq %rbx, %rsp\n\t"
        "popq %rsi\n\t"
        "popq %rbx\n\t"
        "ret\n\t");
}

/* NtContinue-equivalent minus syscalls: apply the integer machine context
 * and jump to its rip (win64cc continue trampoline; never returns). The
 * return address is pushed below the new rsp so the final ret restores both
 * rip and the exact establisher rsp. */
__attribute__((naked, noreturn, used, noinline))
static void resume_win64(const mb_win_seh64_context *c)
{
    __asm__ volatile(
        "movq %rdi, %rcx\n\t"
        "movq 0x98(%rcx), %r10\n\t"   /* new rsp */
        "movq 0xF8(%rcx), %r11\n\t"   /* target rip */
        "subq $8, %r10\n\t"
        "movq %r11, (%r10)\n\t"
        "movq %r10, %rsp\n\t"
        "movq 0x90(%rcx), %rbx\n\t"
        "movq 0xA0(%rcx), %rbp\n\t"
        "movq 0xA8(%rcx), %rsi\n\t"
        "movq 0xB0(%rcx), %rdi\n\t"
        "movq 0xB8(%rcx), %r8\n\t"
        "movq 0xC0(%rcx), %r9\n\t"
        "movq 0xC8(%rcx), %r10\n\t"
        "movq 0xD0(%rcx), %r11\n\t"
        "movq 0xD8(%rcx), %r12\n\t"
        "movq 0xE0(%rcx), %r13\n\t"
        "movq 0xE8(%rcx), %r14\n\t"
        "movq 0xF0(%rcx), %r15\n\t"
        "movq 0x78(%rcx), %rax\n\t"
        "movq 0x88(%rcx), %rdx\n\t"
        "ret\n\t");
}

/* RtlCaptureContext semantics: capture the CALLER of this function. At
 * entry: [rsp] = return address (caller rip), rsp+8 = caller rsp, and the
 * callee-saved registers hold caller values. rax/rdx/r8-r11 are caller-dead
 * values (documented divergence: stored as found, never invented). */
__attribute__((naked, used, noinline))
static void svc_capture_context(mb_win_seh64_context *c)
{
    __asm__ volatile(
        "movq (%rsp), %rax\n\t"
        "movq %rax, 0xF8(%rcx)\n\t"
        "leaq 8(%rsp), %rax\n\t"
        "movq %rax, 0x98(%rcx)\n\t"
        "movq %rdx, 0x88(%rcx)\n\t"
        "movq %rbx, 0x90(%rcx)\n\t"
        "movq %rbp, 0xA0(%rcx)\n\t"
        "movq %rsi, 0xA8(%rcx)\n\t"
        "movq %rdi, 0xB0(%rcx)\n\t"
        "movq %r8, 0xB8(%rcx)\n\t"
        "movq %r9, 0xC0(%rcx)\n\t"
        "movq %r10, 0xC8(%rcx)\n\t"
        "movq %r11, 0xD0(%rcx)\n\t"
        "movq %r12, 0xD8(%rcx)\n\t"
        "movq %r13, 0xE0(%rcx)\n\t"
        "movq %r14, 0xE8(%rcx)\n\t"
        "movq %r15, 0xF0(%rcx)\n\t"
        "movw %cs, 0x38(%rcx)\n\t"
        "pushfq\n\t"
        "popq %rax\n\t"
        "movl %eax, 0x44(%rcx)\n\t"
        "movl $0x100007, 0x30(%rcx)\n\t"
        "movq %rax, 0x78(%rcx)\n\t"      /* caller rax (dead value, as found) */
        "movq %rcx, 0x80(%rcx)\n\t"      /* ctx pointer itself (documented) */
        "ret\n\t");
}

/* ------------------------------------------------- Win64 entry points */
static mb_win_seh64 *installed;   /* session glue (defined below) */
static int live(void);
static void ensure_registered(void);

static WIN64 void svc_raise_exception(uint32_t code, uint32_t flags,
                                      uint32_t count, const uint64_t *args)
{
    mb_win_seh64_context context = {0};
    mb_win_seh64_exception_record record = {0};
    if (!live()) return;
    ensure_registered();
    {
        register mb_win_seh64_context *cap __asm__("rcx") = &context;
        __asm__ volatile("call *%1"
            : "+r"(cap)
            : "r"((void *)(uintptr_t)&svc_capture_context)
            : "rax", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "memory");
    }
    record.exception_code = code;
    record.exception_flags = flags & ~(uint32_t)MB_WIN_SEH64_TARGET_UNWIND;
    record.exception_address = (void *)(uintptr_t)context.rip;
    if (count > MB_WIN_SEH64_MAX_ARGS) {
        record.exception_flags |= MB_WIN_SEH64_NONCONTINUABLE;
        count = 0;
    }
    record.number_parameters = count;
    for (uint32_t i = 0; i < count; ++i) record.exception_information[i] = args[i];
    /* OS-role dispatch: search, unwind, transfer. Returns only unhandled. */
    (void)mb_win_seh64_dispatch(installed, &record, &context);
}

static WIN64 uint64_t svc_lookup_function_entry(uint64_t control_pc,
                                                uint64_t *image_base)
{
    const mb_win_seh64_runtime_function *entry = 0;
    const uint8_t *base = 0;
    if (!live() || !image_base) return 0;
    ensure_registered();
    if (mb_win_seh64_lookup_function_entry(installed, control_pc, &entry,
        &base) != MB_WIN_SEH64_OK) return 0;
    *image_base = (uint64_t)(uintptr_t)base;
    return (uint64_t)(uintptr_t)entry;
}

static WIN64 uint64_t svc_virtual_unwind(uint32_t handler_type,
    uint64_t control_pc, const mb_win_seh64_runtime_function *entry,
    mb_win_seh64_context *context, mb_win_seh64_dispatcher_context *dctx,
    mb_win_seh64_context_pointers *pointers)
{
    const uint8_t *base = 0;
    const mb_win_seh64_runtime_function *found = 0;
    if (!live() || !context || !dctx || !entry) return 0;
    base = dctx->image_base;
    if (!base && mb_win_seh64_lookup_function_entry(installed, control_pc,
        &found, &base) != MB_WIN_SEH64_OK) return 0;
    if (mb_win_seh64_virtual_unwind(handler_type, control_pc, base, entry,
        context, dctx, pointers) != MB_WIN_SEH64_OK) return 0;
    return (uint64_t)(uintptr_t)(base + dctx->handler_rva);
}

static WIN64 uint64_t svc_add_function_table(
    const mb_win_seh64_runtime_function *entries, uint32_t count,
    uint64_t base)
{
    unsigned handle = 0;
    if (!live()) return 0;
    if (mb_win_seh64_add_function_table(installed, entries, count, base,
        &handle) != MB_WIN_SEH64_OK) return 0;
    return handle;
}

static WIN64 int32_t svc_remove_function_table(uint64_t handle)
{
    unsigned removed = 0;
    if (!live() || !handle || handle > MB_WIN_SEH64_MAX_TABLES) return 0;
    if (mb_win_seh64_remove_function_table(installed, (unsigned)handle,
        &removed) != MB_WIN_SEH64_OK) return 0;
    return (int32_t)removed;
}

static WIN64 uint64_t svc_set_unhandled_exception_filter(uint64_t filter)
{
    if (!live()) return 0;
    return (uint64_t)(uintptr_t)mb_win_seh64_set_unhandled_filter(installed,
        (int (*)(mb_win_seh64_exception_pointers *))(uintptr_t)filter);
}

/* The census personality: reached through the PE's own import thunk (the
 * .xdata handler RVA resolves to the mapped PE's jmp [__imp_...] which the
 * loader bound to this function). Forwards to the decision core; a selected
 * scope drives the engine's unwind pass exactly like the real personality
 * forwards to RtlUnwind. */
static WIN64 int32_t svc_c_specific_handler(
    mb_win_seh64_exception_record *record, void *establisher_frame,
    mb_win_seh64_context *context, mb_win_seh64_dispatcher_context *dctx)
{
    if (!live()) return MB_WIN_SEH64_EXCEPTION_CONTINUE_SEARCH;
    ensure_registered();
    int32_t disposition = 0;
    uint64_t target_frame = 0, target_ip = 0;
    mb_win_seh64_status st = mb_win_seh64_csh_decide(installed, record,
        establisher_frame, context, dctx, &disposition, &target_frame,
        &target_ip);
    if (st != MB_WIN_SEH64_OK) return MB_WIN_SEH64_EXCEPTION_CONTINUE_SEARCH;
    if (disposition == MB_WIN_SEH64_EXCEPTION_CONTINUE_EXECUTION)
        return MB_WIN_SEH64_EXCEPTION_CONTINUE_EXECUTION;
    if (record->exception_flags & MB_WIN_SEH64_TARGET_UNWIND)
        (void)mb_win_seh64_unwind_to(installed, record, context,
            target_frame, target_ip); /* never returns */
    return MB_WIN_SEH64_EXCEPTION_CONTINUE_SEARCH;
}

/* ------------------------------------------------- control wrappers
 * Native-CC shims handed to the owner; each makes exactly one transition
 * into ms_abi code through a correctly typed pointer. */
typedef int32_t (WIN64 *mb_seh_personality_fn)(void *, void *, void *, void *);

static int control_call_personality(void *personality,
    mb_win_seh64_exception_record *record, void *establisher_frame,
    mb_win_seh64_context *context, mb_win_seh64_dispatcher_context *dctx)
{
    return ((mb_seh_personality_fn)personality)(record, establisher_frame,
        context, dctx);
}

static int control_call_at_frame(void *fn, uint64_t establisher_rsp, void *arg)
{ return (int)call_at_frame_win64(fn, establisher_rsp, arg); }

static void control_resume(const mb_win_seh64_context *context)
{ resume_win64(context); }

/* ------------------------------------------------- session + owner glue */
static mb_win_seh64_abi_session *sessions;
static mb_win_seh64_abi_session **find(mb_win_seh64_abi_session *s)
{ mb_win_seh64_abi_session **p = &sessions; while (*p && *p != s) p = &(*p)->next; return p; }
static int live(void) { return installed && installed->active && sessions; }

/* One lazy in-image registration slot: the module whose exception directory
 * is registered with the owner at its first SEH64 bind. */
static mb_win_seh64_abi_session *ensure_session;

mb_win_seh64_abi_status mb_win_seh64_abi_begin_session(mb_win_seh64_abi_session *s,
                                                       mb_win_seh64 *owner, mb_pe_module *m)
{
    if (!s || !owner || !m) return MB_WIN_SEH64_ABI_INVALID_ARGUMENT;
    if (s->active || s->module || s->next || m->state != MB_PE_MODULE_EMPTY)
        return MB_WIN_SEH64_ABI_BAD_STATE;
    for (mb_win_seh64_abi_session *p = sessions; p; p = p->next)
        if (p == s || p->module == m) return MB_WIN_SEH64_ABI_BAD_STATE;
    if (installed && installed != owner) return MB_WIN_SEH64_ABI_BUSY;
    s->module = m; s->next = sessions; s->active = 1; sessions = s;
    if (!installed) installed = owner;
    return MB_WIN_SEH64_ABI_OK;
}

mb_win_seh64_abi_status mb_win_seh64_abi_end_session(mb_win_seh64_abi_session *s)
{
    if (!s) return MB_WIN_SEH64_ABI_INVALID_ARGUMENT;
    mb_win_seh64_abi_session **p = find(s);
    if (!*p || !s->active) return MB_WIN_SEH64_ABI_BAD_STATE;
    if (s->module->state != MB_PE_MODULE_EMPTY) return MB_WIN_SEH64_ABI_BUSY;
    /* Lease discipline: a released module's table is removed here; lookup
     * after this point refuses (no dangling reads). */
    if (s->registered && s->registered_image)
        mb_win_seh64_unregister_image_table(installed, s->registered_image);
    if (ensure_session == s) ensure_session = 0;
    *p = s->next; *s = (mb_win_seh64_abi_session){0};
    if (!sessions) { installed = (mb_win_seh64 *)0; ensure_session = 0; }
    return MB_WIN_SEH64_ABI_OK;
}

/* Register the bound module's exception directory (design §7 loader
 * amendment: the loader admits the directory under MB_PE_SEH64; the family
 * owns the registry). Idempotent. */
static void ensure_registered(void)
{
    mb_win_seh64_abi_session *s = ensure_session;
    if (!s || s->registered || !installed || !installed->active) return;
    /* The module's parsed info is only guaranteed complete after open
     * returns; resolve the directory here (first thunk use), not at bind. */
    const mb_pe_directory *d = &s->module->info.directories[3];
    if (!d->rva || !d->size || !s->module->image || !s->module->info.image_size)
        return;
    s->image_size = s->module->info.image_size;
    s->pdata_rva = d->rva;
    s->pdata_size = d->size;
    unsigned slot = 0;
    if (mb_win_seh64_register_image_table(installed,
            (const uint8_t *)(uintptr_t)s->module->image, s->image_size,
            s->pdata_rva, s->pdata_size, &slot) == MB_WIN_SEH64_OK) {
        s->registered = 1;
        s->registered_image = s->module->image;
    }
}

static const mb_win_seh64_control control = {
    control_call_personality, control_call_at_frame, control_resume
};
static mb_win_seh64 owner_storage;

mb_win_seh64_status mb_win_seh64_abi_open_owner(mb_win_seh64 **out)
{
    if (!out) return MB_WIN_SEH64_INVALID_ARGUMENT;
    mb_win_seh64_status st = mb_win_seh64_open(&owner_storage, &control);
    if (st != MB_WIN_SEH64_OK && st != MB_WIN_SEH64_BAD_STATE) return st;
    *out = &owner_storage;
    return MB_WIN_SEH64_OK;
}

mb_win_seh64_status mb_win_seh64_abi_close_owner(void)
{ return mb_win_seh64_close(&owner_storage); }

/* ------------------------------------------------------------- binder */
static int match(mb_pe_string v, const char *s, size_t n, int fold)
{
    if (v.size != n) return 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)v.data[i];
        if (fold && c >= 'A' && c <= 'Z') c = (unsigned char)(c + 'a' - 'A');
        if (c != (unsigned char)s[i]) return 0;
    }
    return 1;
}
#define MATCH(v, s, f) match((v), (s), sizeof(s) - 1, (f))
#define BIND(literal, fn) \
    if (MATCH(import->name, literal, 0)) { \
        *value = (uint64_t)(uintptr_t)&fn; \
        pin_directory(session); \
        return MB_PE_BIND_BOUND; }

/* Pin the module on its first SEH64 bind; the directory itself is resolved
 * lazily at the first thunk use (the loader fills mb_pe_info while binding,
 * so bind-time info is not trustworthy). */
static void pin_directory(mb_win_seh64_abi_session *session)
{
    if (ensure_session) return;
    ensure_session = session;
}

mb_pe_bind_result mb_win_seh64_abi_bind(void *context, const mb_pe_import *import,
                                        uint64_t *value)
{
    mb_win_seh64_abi_session *session = context;
    if (!session || !*find(session) || !session->active || !import || !value ||
        (!import->library.data && import->library.size) ||
        (!import->name.data && import->name.size) ||
        (session->module->state != MB_PE_MODULE_LOADING &&
         session->module->state != MB_PE_MODULE_ACTIVE))
        return MB_PE_BIND_ERROR;
    if (import->by_ordinal) return MB_PE_BIND_UNRESOLVED;
    int kernel = MATCH(import->library, "kernel32.dll", 1);
    int vcrun = MATCH(import->library, "vcruntime140.dll", 1);
    if (!kernel && !vcrun) return MB_PE_BIND_UNRESOLVED;
    if (kernel) {
        BIND("RtlCaptureContext", svc_capture_context)
        BIND("RtlLookupFunctionEntry", svc_lookup_function_entry)
        BIND("RtlVirtualUnwind", svc_virtual_unwind)
        BIND("RtlAddFunctionTable", svc_add_function_table)
        BIND("RtlRemoveFunctionTable", svc_remove_function_table)
        BIND("SetUnhandledExceptionFilter", svc_set_unhandled_exception_filter)
        BIND("RaiseException", svc_raise_exception)
    } else {
        BIND("__C_specific_handler", svc_c_specific_handler)
    }
    return MB_PE_BIND_UNRESOLVED;
}
