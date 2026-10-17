/* SEH64 structured-exception-handling family: pure-logic owner.
 * First implementation of the worker-seh64-design DESIGN.md surface. See the
 * header for the honest scope and non-goals. Native CC everywhere; every
 * ms_abi transition is injected through mb_win_seh64_control.
 *
 * UNWIND_CODE wire format (pinned here): each u16 = bits 0..3 opcode,
 * bits 4..7 operand (register/size), bits 8..15 prologue offset. Operand
 * words that need more space follow in whole u16 slots (little-endian). */
#include "mb_win_seh64.h"

_Static_assert(sizeof(mb_win_seh64_runtime_function) == 12, "RUNTIME_FUNCTION 3xu32");
_Static_assert(sizeof(mb_win_seh64_exception_record) == 0x98, "EXCEPTION_RECORD x64 0x98");
_Static_assert(sizeof(mb_win_seh64_context) == 0x4D0, "CONTEXT x64 0x4D0");
_Static_assert(offsetof(mb_win_seh64_context, context_flags) == 0x30, "ContextFlags 0x30");
_Static_assert(offsetof(mb_win_seh64_context, mx_csr) == 0x34, "MxCsr 0x34");
_Static_assert(offsetof(mb_win_seh64_context, seg_cs) == 0x38, "SegCs 0x38");
_Static_assert(offsetof(mb_win_seh64_context, eflags) == 0x44, "EFlags 0x44");
_Static_assert(offsetof(mb_win_seh64_context, dr0) == 0x48, "Dr0 0x48");
_Static_assert(offsetof(mb_win_seh64_context, rax) == 0x78, "Rax 0x78");
_Static_assert(offsetof(mb_win_seh64_context, rbx) == 0x90, "Rbx 0x90");
_Static_assert(offsetof(mb_win_seh64_context, rsp) == 0x98, "Rsp 0x98");
_Static_assert(offsetof(mb_win_seh64_context, rbp) == 0xA0, "Rbp 0xA0");
_Static_assert(offsetof(mb_win_seh64_context, r15) == 0xF0, "R15 0xF0");
_Static_assert(offsetof(mb_win_seh64_context, rip) == 0xF8, "Rip 0xF8");
_Static_assert(offsetof(mb_win_seh64_context, flt_save) == 0x100, "FltSave 0x100");

static uint32_t load32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
        ((uint32_t)p[3] << 24);
}
static uint64_t load64(const void *p) {
    const uint8_t *b = p;
    return (uint64_t)load32(b) | ((uint64_t)load32(b + 4) << 32);
}

mb_win_seh64_status mb_win_seh64_open(mb_win_seh64 *o, const mb_win_seh64_control *c)
{
    if (!o || !c || !c->call_personality || !c->call_at_frame || !c->resume)
        return MB_WIN_SEH64_INVALID_ARGUMENT;
    if (o->active) return MB_WIN_SEH64_BAD_STATE;
    for (unsigned i = 0; i < MB_WIN_SEH64_MAX_TABLES; ++i)
        if (o->tables[i].live) return MB_WIN_SEH64_BAD_STATE;
    o->control = *c;
    o->active = 1;
    return MB_WIN_SEH64_OK;
}

mb_win_seh64_status mb_win_seh64_close(mb_win_seh64 *o)
{
    if (!o || !o->active) return MB_WIN_SEH64_INVALID_ARGUMENT;
    for (unsigned i = 0; i < MB_WIN_SEH64_MAX_TABLES; ++i)
        if (o->tables[i].live) return MB_WIN_SEH64_BUSY;
    *o = (mb_win_seh64){0};
    return MB_WIN_SEH64_OK;
}

static int range_sorted(const mb_win_seh64_runtime_function *e, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        if (e[i].begin_address >= e[i].end_address) return 0;
        if (i && e[i - 1].end_address > e[i].begin_address) return 0;
    }
    return 1;
}

mb_win_seh64_status mb_win_seh64_register_image_table(mb_win_seh64 *o,
    const uint8_t *image, uint64_t image_size, uint32_t pdata_rva,
    uint32_t pdata_size, unsigned *slot)
{
    if (!o || !o->active || !image || !slot || !image_size)
        return MB_WIN_SEH64_INVALID_ARGUMENT;
    if ((uint64_t)pdata_rva + pdata_size > image_size || pdata_size < 12 ||
        pdata_size % 12) return MB_WIN_SEH64_INVALID_ARGUMENT;
    const mb_win_seh64_runtime_function *entries =
        (const mb_win_seh64_runtime_function *)(image + pdata_rva);
    if (!range_sorted(entries, pdata_size / 12))
        return MB_WIN_SEH64_INVALID_ARGUMENT;
    for (unsigned i = 0; i < MB_WIN_SEH64_MAX_TABLES; ++i) {
        if (o->tables[i].live) continue;
        o->tables[i] = (mb_win_seh64_table){1, MB_WIN_SEH64_TABLE_IMAGE,
            image, entries, pdata_size / 12};
        *slot = i;
        return MB_WIN_SEH64_OK;
    }
    return MB_WIN_SEH64_LIMIT;
}

mb_win_seh64_status mb_win_seh64_unregister_image_table(mb_win_seh64 *o,
    const uint8_t *image)
{
    if (!o || !o->active || !image) return MB_WIN_SEH64_INVALID_ARGUMENT;
    int found = 0;
    for (unsigned i = 0; i < MB_WIN_SEH64_MAX_TABLES; ++i)
        if (o->tables[i].live && o->tables[i].kind == MB_WIN_SEH64_TABLE_IMAGE &&
            o->tables[i].base == image) {
            o->tables[i] = (mb_win_seh64_table){0};
            found = 1;
        }
    return found ? MB_WIN_SEH64_OK : MB_WIN_SEH64_BAD_STATE;
}

mb_win_seh64_status mb_win_seh64_add_function_table(mb_win_seh64 *o,
    const mb_win_seh64_runtime_function *entries, uint32_t count,
    uint64_t base, unsigned *handle)
{
    if (!o || !o->active || !entries || !count || !handle)
        return MB_WIN_SEH64_INVALID_ARGUMENT;
    if (!range_sorted(entries, count)) return MB_WIN_SEH64_INVALID_ARGUMENT;
    for (unsigned i = 0; i < MB_WIN_SEH64_MAX_TABLES; ++i) {
        if (o->tables[i].live) continue;
        o->tables[i] = (mb_win_seh64_table){1, MB_WIN_SEH64_TABLE_DYNAMIC,
            (const uint8_t *)(uintptr_t)base, entries, count};
        *handle = i + 1;
        return MB_WIN_SEH64_OK;
    }
    return MB_WIN_SEH64_LIMIT;
}

mb_win_seh64_status mb_win_seh64_remove_function_table(mb_win_seh64 *o,
    unsigned handle, unsigned *removed)
{
    if (!o || !o->active || !handle || !removed) return MB_WIN_SEH64_INVALID_ARGUMENT;
    mb_win_seh64_table *t = &o->tables[handle - 1];
    if (!t->live || t->kind != MB_WIN_SEH64_TABLE_DYNAMIC) {
        *removed = 0;
        return MB_WIN_SEH64_OK;
    }
    *t = (mb_win_seh64_table){0};
    *removed = 1;
    return MB_WIN_SEH64_OK;
}

mb_win_seh64_status mb_win_seh64_lookup_function_entry(const mb_win_seh64 *o,
    uint64_t control_pc, const mb_win_seh64_runtime_function **entry,
    const uint8_t **image_base)
{
    if (!o || !o->active || !control_pc || !entry || !image_base)
        return MB_WIN_SEH64_INVALID_ARGUMENT;
    *entry = 0;
    *image_base = 0;
    for (unsigned pass = 0; pass < 2; ++pass)
        for (unsigned i = 0; i < MB_WIN_SEH64_MAX_TABLES; ++i) {
            const mb_win_seh64_table *t = &o->tables[i];
            if (!t->live) continue;
            if ((pass == 0) != (t->kind == MB_WIN_SEH64_TABLE_IMAGE)) continue;
            uint32_t lo = 0, hi = t->count;
            while (lo < hi) {
                uint32_t mid = lo + (hi - lo) / 2;
                uint64_t begin = (uint64_t)(uintptr_t)t->base +
                    t->entries[mid].begin_address;
                uint64_t end = (uint64_t)(uintptr_t)t->base +
                    t->entries[mid].end_address;
                if (control_pc < begin) hi = mid;
                else if (control_pc >= end) lo = mid + 1;
                else {
                    *entry = &t->entries[mid];
                    *image_base = t->base;
                    return MB_WIN_SEH64_OK;
                }
            }
        }
    return MB_WIN_SEH64_NO_UNWIND_INFO;
}

/* Register number -> context home. Unwind operand register numbers are the
 * x64 encodings, which match CONTEXT member order starting at Rax. */
static uint64_t *reg_home(mb_win_seh64_context *c, unsigned reg)
{
    if (reg > 15 || reg == 4) return 0; /* rsp has no unwind home */
    return &c->rax + reg;
}

/* Undo one prologue code in reverse-execution order. Returns total u16 slots
 * consumed, or -1 on a refused/unknown code. */
static int undo_code(unsigned op, unsigned operand, const uint16_t *next,
    uint64_t *rsp, mb_win_seh64_context *context,
    mb_win_seh64_context_pointers *pointers)
{
    uint64_t *home = 0;
    switch (op) {
    case MB_WIN_SEH64_UWOP_PUSH_NONVOL:
    case MB_WIN_SEH64_UWOP_SAVE_NONVOL:
    case MB_WIN_SEH64_UWOP_SAVE_NONVOL_FAR:
        home = reg_home(context, operand);
        if (!home) return -1;
        break;
    default:
        break;
    }
    switch (op) {
    case MB_WIN_SEH64_UWOP_PUSH_NONVOL:
        *home = load64((const void *)(uintptr_t)*rsp);
        if (pointers) {
            if (operand == 3) pointers->rbx = (uint64_t *)(uintptr_t)*rsp;
            else if (operand == 5) pointers->rbp = (uint64_t *)(uintptr_t)*rsp;
            else if (operand == 6) pointers->rsi = (uint64_t *)(uintptr_t)*rsp;
            else if (operand == 7) pointers->rdi = (uint64_t *)(uintptr_t)*rsp;
        }
        *rsp += 8;
        return 1;
    case MB_WIN_SEH64_UWOP_ALLOC_LARGE:
        *rsp += operand ? (uint64_t)next[0] : (uint64_t)next[0] * 8;
        return 2;
    case MB_WIN_SEH64_UWOP_ALLOC_SMALL:
        *rsp += (uint64_t)operand * 8 + 8;
        return 1;
    case MB_WIN_SEH64_UWOP_SET_FPREG:
        /* Frame-register establishment moves no RSP; SET_FPREG-based local
         * addressing is not modeled (documented subset). */
        return 1;
    case MB_WIN_SEH64_UWOP_SAVE_NONVOL: {
        uint64_t where = *rsp + (uint64_t)next[0] * 8;
        *home = load64((const void *)(uintptr_t)where);
        if (pointers) {
            if (operand == 3) pointers->rbx = (uint64_t *)(uintptr_t)where;
            else if (operand == 5) pointers->rbp = (uint64_t *)(uintptr_t)where;
            else if (operand == 6) pointers->rsi = (uint64_t *)(uintptr_t)where;
            else if (operand == 7) pointers->rdi = (uint64_t *)(uintptr_t)where;
        }
        return 2;
    }
    case MB_WIN_SEH64_UWOP_SAVE_NONVOL_FAR: {
        uint64_t where = *rsp + (uint64_t)load32((const uint8_t *)next);
        *home = load64((const void *)(uintptr_t)where);
        return 3;
    }
    case MB_WIN_SEH64_UWOP_SAVE_XMM:
    case MB_WIN_SEH64_UWOP_SAVE_XMM128:
        return 2; /* XMM homes skipped (integer-only restore); slots consumed */
    case MB_WIN_SEH64_UWOP_SAVE_XMM_FAR:
    case MB_WIN_SEH64_UWOP_SAVE_XMM128_FAR:
        return 3;
    case MB_WIN_SEH64_UWOP_PUSH_MACHFRAME:
        return -1; /* interrupt frames: parse-only, refused (documented) */
    default:
        return -1;
    }
}

static unsigned opcode_slots(unsigned op, unsigned operand)
{
    switch (op) {
    case MB_WIN_SEH64_UWOP_ALLOC_LARGE: return 2;
    case MB_WIN_SEH64_UWOP_SAVE_NONVOL: return 2;
    case MB_WIN_SEH64_UWOP_SAVE_NONVOL_FAR: return 3;
    case MB_WIN_SEH64_UWOP_SAVE_XMM: return 2;
    case MB_WIN_SEH64_UWOP_SAVE_XMM_FAR: return 3;
    case MB_WIN_SEH64_UWOP_SAVE_XMM128: return 2;
    case MB_WIN_SEH64_UWOP_SAVE_XMM128_FAR: return 3;
    default: (void)operand; return 1;
    }
}

/* Undo the prologue (or its not-yet-executed tail for prologue faults) and
 * produce the caller context. distance == UINT64_MAX means body. */
static mb_win_seh64_status replay_prologue(const mb_win_seh64_unwind_info *info,
    uint64_t distance, mb_win_seh64_context *context,
    mb_win_seh64_context_pointers *pointers, uint64_t *establisher_out)
{
    const uint16_t *codes = (const uint16_t *)(info + 1);
    unsigned count = info->count_of_codes;
    uint64_t rsp = context->rsp;
    if (establisher_out) *establisher_out = context->rsp;
    unsigned i = 0;
    while (i < count) {
        /* UNWIND_CODE union: low byte = prologue offset, high nibbles =
         * OpInfo (bits 12-15) then UnwindOp (bits 8-11). */
        unsigned op = (codes[i] >> 8) & 0xFu;
        unsigned operand = (codes[i] >> 12) & 0xFu;
        unsigned offset = codes[i] & 0xFFu;
        unsigned slots = opcode_slots(op, operand);
        if (!slots || i + slots > count) return MB_WIN_SEH64_UNSUPPORTED;
        if (distance != UINT64_MAX && distance < info->size_of_prologue &&
            offset <= distance) {
            /* Instruction already executed at the faulting pc: keep it. */
            i += slots;
            continue;
        }
        int consumed = undo_code(op, operand, codes + i + 1, &rsp, context,
            pointers);
        if (consumed < 0) return MB_WIN_SEH64_UNSUPPORTED;
        i += (unsigned)consumed;
    }
    context->rip = load64((const void *)(uintptr_t)rsp);
    context->rsp = rsp + 8;
    return MB_WIN_SEH64_OK;
}

/* Bounded epilogue scan at ControlPc: [add rsp,imm]* pop* ret. Tail jmp
 * forms and interior single-ret splits are refused (documented subset). */
static int try_epilogue(const uint8_t *image,
    const mb_win_seh64_runtime_function *entry, uint64_t control_pc,
    mb_win_seh64_context *context, mb_win_seh64_context_pointers *pointers)
{
    uint64_t begin = (uint64_t)(uintptr_t)(image + entry->begin_address);
    uint64_t end = (uint64_t)(uintptr_t)(image + entry->end_address);
    if (control_pc < begin || control_pc >= end) return 0;
    uint64_t span = end - control_pc;
    if (span > 32) span = 32;
    const uint8_t *pc = (const uint8_t *)(uintptr_t)control_pc;
    uint64_t rsp = context->rsp;
    uint64_t i = 0;
    if (span >= 8 && pc[0] == 0x48 && pc[1] == 0x83 && pc[2] == 0xC4) {
        rsp += (uint64_t)(int8_t)pc[3];
        i = 4;
    } else if (span >= 7 && pc[0] == 0x48 && pc[1] == 0x81 && pc[2] == 0xC4) {
        rsp += load32(pc + 3);
        i = 7;
    } else if (span >= 5 && pc[0] == 0x48 && pc[1] == 0x8D &&
               pc[2] == 0x64 && pc[3] == 0x24) {
        rsp += (uint64_t)(int8_t)pc[4]; /* lea rsp,[rsp+disp8] */
        i = 5;
    }
    while (i < span) {
        if (pc[i] == 0xC3) { /* ret */
            context->rip = load64((const void *)(uintptr_t)rsp);
            context->rsp = rsp + 8;
            return 1;
        }
        unsigned reg;
        if (pc[i] == 0x41 && i + 1 < span && pc[i + 1] >= 0x58 && pc[i + 1] <= 0x5F) {
            reg = 8u + (unsigned)(pc[i + 1] - 0x58); /* REX.B pop r8..r15 */
            i += 2;
        } else if (pc[i] >= 0x58 && pc[i] <= 0x5F) {
            reg = (unsigned)(pc[i] - 0x58); /* pop rax..rdi */
            i += 1;
        } else return 0;
        if (reg == 4) return 0; /* pop rsp: refused */
        if (reg >= 8 || reg == 0 || reg == 1 || reg == 2) {
            /* volatile pop: stack slot discarded, nothing restored */
            continue;
        }
        uint64_t *home = reg_home(context, reg);
        if (!home) return 0;
        *home = load64((const void *)(uintptr_t)rsp);
        if (pointers) {
            if (reg == 5) pointers->rbp = (uint64_t *)(uintptr_t)rsp;
            else if (reg == 6) pointers->rsi = (uint64_t *)(uintptr_t)rsp;
            else if (reg == 7) pointers->rdi = (uint64_t *)(uintptr_t)rsp;
        }
        rsp += 8;
    }
    return 0;
}

mb_win_seh64_status mb_win_seh64_virtual_unwind(uint32_t handler_type,
    uint64_t control_pc, const uint8_t *image_base,
    const mb_win_seh64_runtime_function *entry, mb_win_seh64_context *context,
    mb_win_seh64_dispatcher_context *dctx,
    mb_win_seh64_context_pointers *pointers)
{
    (void)handler_type;
    if (!image_base || !entry || !context || !dctx) return MB_WIN_SEH64_INVALID_ARGUMENT;
    uint64_t begin = (uint64_t)(uintptr_t)(image_base + entry->begin_address);
    uint64_t end = (uint64_t)(uintptr_t)(image_base + entry->end_address);
    if (control_pc < begin || control_pc >= end) return MB_WIN_SEH64_INVALID_ARGUMENT;
    const mb_win_seh64_unwind_info *info =
        (const mb_win_seh64_unwind_info *)(image_base + entry->unwind_data);
    unsigned chain = 0;
    while (MB_WIN_SEH64_INFO_FLAGS(info) & MB_WIN_SEH64_UNW_FLAG_CHAININFO) {
        if (++chain > MB_WIN_SEH64_MAX_CHAIN) return MB_WIN_SEH64_UNSUPPORTED;
        /* Chained handler data IS the RUNTIME_FUNCTION triple (no
         * personality RVA slot), directly after the padded code array. */
        const uint16_t *codes = (const uint16_t *)(info + 1);
        unsigned padded = info->count_of_codes + (info->count_of_codes & 1u);
        uint32_t rva = load32((const uint8_t *)(codes + padded));
        if (!rva) return MB_WIN_SEH64_UNSUPPORTED;
        entry = (const mb_win_seh64_runtime_function *)(image_base + rva);
        if (control_pc < (uint64_t)(uintptr_t)(image_base + entry->begin_address) ||
            control_pc >= (uint64_t)(uintptr_t)(image_base + entry->end_address))
            return MB_WIN_SEH64_UNSUPPORTED;
        info = (const mb_win_seh64_unwind_info *)(image_base + entry->unwind_data);
    }
    unsigned version = MB_WIN_SEH64_INFO_VERSION(info);
    if (version < 1u || version > 2u) return MB_WIN_SEH64_UNSUPPORTED;
    dctx->control_pc = control_pc;
    dctx->function_entry = entry;
    dctx->image_base = image_base;
    dctx->handler_rva = 0;
    dctx->handler_data = 0;
    unsigned flags = MB_WIN_SEH64_INFO_FLAGS(info);
    const uint16_t *codes = (const uint16_t *)(info + 1);
    unsigned padded = info->count_of_codes + (info->count_of_codes & 1u);
    if (flags & (MB_WIN_SEH64_UNW_FLAG_EHANDLER | MB_WIN_SEH64_UNW_FLAG_UHANDLER)) {
        const uint8_t *hr = (const uint8_t *)(codes + padded);
        dctx->handler_rva = load32(hr);
        dctx->handler_data = hr + 4;
    }
    dctx->frame_rsp = context->rsp;
    if (try_epilogue(image_base, entry, control_pc, context, pointers))
        return MB_WIN_SEH64_OK;
    uint64_t distance = control_pc - begin;
    mb_win_seh64_status st = replay_prologue(info, distance, context, pointers,
        &dctx->establisher_frame);
    if (st == MB_WIN_SEH64_OK && MB_WIN_SEH64_INFO_FRAME_REG(info)) {
        /* SET_FPREG: EstablisherFrame is the frame register value = the
         * frame's rsp plus the scaled offset (Win64 contract). */
        dctx->establisher_frame = dctx->frame_rsp +
            (uint64_t)MB_WIN_SEH64_INFO_FRAME_OFF(info) * 16u;
    }
    return st;
}

mb_win_seh64_status mb_win_seh64_csh_decide(mb_win_seh64 *owner,
    mb_win_seh64_exception_record *record, void *establisher_frame,
    mb_win_seh64_context *context, mb_win_seh64_dispatcher_context *dctx,
    int32_t *disposition_out, uint64_t *unwind_target_frame,
    uint64_t *unwind_target_ip)
{
    if (!owner || !record || !dctx || !disposition_out || !unwind_target_frame ||
        !unwind_target_ip) return MB_WIN_SEH64_INVALID_ARGUMENT;
    *disposition_out = MB_WIN_SEH64_EXCEPTION_CONTINUE_SEARCH;
    if (!dctx->handler_data) return MB_WIN_SEH64_OK;
    const uint8_t *image = dctx->image_base;
    uint32_t count = load32((const uint8_t *)dctx->handler_data);
    if (count > 4096u) return MB_WIN_SEH64_UNSUPPORTED;
    const uint8_t *rec = (const uint8_t *)dctx->handler_data + 4;
    /* Scope-table semantics per documented Win64 __C_specific_handler
     * behavior (ReactOS/Wine prior art; resolves the DESIGN.md 5c hedge):
     * filter RVA 0 = __finally (unwind pass only), RVA 1 = constant
     * EXECUTE_HANDLER, otherwise a real filter function called in the
     * establishing frame. Records are innermost-first (compiler order). */
    int unwinding = (record->exception_flags & MB_WIN_SEH64_UNWINDING) != 0;
    for (uint32_t i = 0; i < count; ++i, rec += 16) {
        uint32_t begin_rva = load32(rec), end_rva = load32(rec + 4);
        uint32_t filter_rva = load32(rec + 8), jump_rva = load32(rec + 12);
        if (dctx->control_pc < (uint64_t)(uintptr_t)(image + begin_rva) ||
            dctx->control_pc >= (uint64_t)(uintptr_t)(image + end_rva)) continue;
        if (!unwinding) {
            if (!filter_rva) continue; /* __finally: inactive on search pass */
            int32_t verdict = MB_WIN_SEH64_EXECUTE_HANDLER;
            if (filter_rva != 1u) {
                mb_win_seh64_exception_pointers ptrs = {record, context};
                verdict = owner->control.call_at_frame(
                    (void *)(uintptr_t)(image + filter_rva),
                    dctx->frame_rsp, &ptrs);
            }
            if (verdict == MB_WIN_SEH64_CONTINUE_EXECUTION) {
                if (record->exception_flags & MB_WIN_SEH64_NONCONTINUABLE) {
                    /* Real Windows raises STATUS_NONCONTINUABLE_EXCEPTION;
                     * this prototype demotes to refusal of that scope
                     * (documented divergence, never a silent swallow). */
                    continue;
                }
                *disposition_out = MB_WIN_SEH64_EXCEPTION_CONTINUE_EXECUTION;
                return MB_WIN_SEH64_OK;
            }
            if (verdict != MB_WIN_SEH64_EXECUTE_HANDLER) continue;
            record->exception_flags |= MB_WIN_SEH64_TARGET_UNWIND;
            dctx->scope_index = i;
            *unwind_target_frame = (uint64_t)(uintptr_t)establisher_frame;
            *unwind_target_ip = (uint64_t)(uintptr_t)(image + jump_rva);
            return MB_WIN_SEH64_OK;
        }
        /* Unwind pass: run __finally bodies (filter 0 with a jump target)
         * covering ControlPc. The selected __except body itself runs via the
         * final target transfer, not here. */
        if (!filter_rva && jump_rva)
            /* Finally funclets receive the establisher frame (PSP) in rcx
             * and run just below the frame's rsp. */
            owner->control.call_at_frame(
                (void *)(uintptr_t)(image + jump_rva),
                dctx->frame_rsp, (void *)(uintptr_t)dctx->establisher_frame);
    }
    return MB_WIN_SEH64_OK;
}

mb_win_seh64_status mb_win_seh64_dispatch(mb_win_seh64 *owner,
    mb_win_seh64_exception_record *record, mb_win_seh64_context *context)
{
    if (!owner || !owner->active || !record || !context)
        return MB_WIN_SEH64_INVALID_ARGUMENT;
    if (owner->dispatch_depth >= 8) return MB_WIN_SEH64_LIMIT;
    ++owner->dispatch_depth;
    mb_win_seh64_status st = MB_WIN_SEH64_NO_UNWIND_INFO;
    mb_win_seh64_context walk = *context;
    mb_win_seh64_context_pointers pointers = {0};
    mb_win_seh64_dispatcher_context dctx = {0};
    dctx.context_record = context;
    dctx.history_oracle = 1;
    /* Pass 1: search, faulting frame toward the root. */
    uint64_t target_frame = 0, target_ip = 0;
    int selected = 0;
    for (;;) {
        const mb_win_seh64_runtime_function *entry = 0;
        const uint8_t *image = 0;
        st = mb_win_seh64_lookup_function_entry(owner, walk.rip, &entry, &image);
        if (st != MB_WIN_SEH64_OK) break; /* terminal frame policy */
        st = mb_win_seh64_virtual_unwind(0, walk.rip, image, entry, &walk,
            &dctx, &pointers);
        if (st != MB_WIN_SEH64_OK) break;
        if (dctx.handler_rva) {
            int32_t disposition = 0;
            mb_win_seh64_status cst = mb_win_seh64_csh_decide(owner, record,
                (void *)(uintptr_t)dctx.establisher_frame, context, &dctx,
                &disposition, &target_frame, &target_ip);
            if (cst != MB_WIN_SEH64_OK) { st = cst; break; }
            if (disposition == MB_WIN_SEH64_EXCEPTION_CONTINUE_EXECUTION) {
                --owner->dispatch_depth;
                return MB_WIN_SEH64_OK; /* documented: caller-level continue */
            }
            if (record->exception_flags & MB_WIN_SEH64_TARGET_UNWIND) {
                selected = 1;
                break;
            }
        }
    }
    if (!selected) {
        /* Unhandled: run the last-chance filter, then report honestly (the
         * anti-fake anchor: never swallow, never fake a handled result). */
        if (owner->unhandled_filter) {
            mb_win_seh64_exception_pointers ptrs = {record, context};
            (void)owner->unhandled_filter(&ptrs);
        }
        --owner->dispatch_depth;
        return st == MB_WIN_SEH64_OK ? MB_WIN_SEH64_NO_UNWIND_INFO : st;
    }
    mb_win_seh64_status ust = mb_win_seh64_unwind_to(owner, record, context,
        target_frame, target_ip);
    --owner->dispatch_depth;
    return ust;
}

mb_win_seh64_status mb_win_seh64_unwind_to(mb_win_seh64 *owner,
    mb_win_seh64_exception_record *record, mb_win_seh64_context *context,
    uint64_t target_frame, uint64_t target_ip)
{
    if (!owner || !owner->active || !record || !context)
        return MB_WIN_SEH64_INVALID_ARGUMENT;
    if (owner->dispatch_depth >= 8) return MB_WIN_SEH64_LIMIT;
    ++owner->dispatch_depth;
    mb_win_seh64_status st;
    /* Pass 2: unwind fault->target, running __finally bodies via each
     * frame's personality with UNWINDING (the target frame's own bodies run
     * immediately before the transfer). The transfer context is the target
     * frame's own pre-advance context: rip = selected body, rsp = its
     * establisher frame, nonvolatiles as live in that frame. */
    mb_win_seh64_context unw = *context;
    for (;;) {
        const mb_win_seh64_runtime_function *entry = 0;
        const uint8_t *image = 0;
        st = mb_win_seh64_lookup_function_entry(owner, unw.rip, &entry, &image);
        if (st != MB_WIN_SEH64_OK) { --owner->dispatch_depth; return st; }
        mb_win_seh64_context cur = unw;
        mb_win_seh64_dispatcher_context udctx = {0};
        udctx.context_record = &cur;
        udctx.history_oracle = 1;
        st = mb_win_seh64_virtual_unwind(0, cur.rip, image, entry, &unw,
            &udctx, 0);
        if (st != MB_WIN_SEH64_OK) { --owner->dispatch_depth; return st; }
        if (udctx.handler_rva) {
            mb_win_seh64_exception_record urec = *record;
            urec.exception_flags = (record->exception_flags &
                ~MB_WIN_SEH64_TARGET_UNWIND) | MB_WIN_SEH64_UNWINDING;
            (void)owner->control.call_personality(
                (void *)(uintptr_t)(image + udctx.handler_rva), &urec,
                (void *)(uintptr_t)udctx.establisher_frame, &cur, &udctx);
        }
        if (udctx.establisher_frame == target_frame) {
            mb_win_seh64_context transfer = cur;
            transfer.rip = target_ip;
            transfer.rsp = udctx.frame_rsp;
            --owner->dispatch_depth;
            owner->control.resume(&transfer); /* never returns */
            return MB_WIN_SEH64_OK;
        }
    }}

int (*mb_win_seh64_set_unhandled_filter(mb_win_seh64 *o,
    int (*filter)(mb_win_seh64_exception_pointers *)))(mb_win_seh64_exception_pointers *)
{
    if (!o || !o->active) return 0;
    int (*previous)(mb_win_seh64_exception_pointers *) = o->unhandled_filter;
    o->unhandled_filter = filter;
    return previous;
}

const char *mb_win_seh64_status_name(mb_win_seh64_status s)
{
    switch (s) {
    case MB_WIN_SEH64_OK: return "OK";
    case MB_WIN_SEH64_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case MB_WIN_SEH64_BAD_STATE: return "BAD_STATE";
    case MB_WIN_SEH64_BUSY: return "BUSY";
    case MB_WIN_SEH64_LIMIT: return "LIMIT";
    case MB_WIN_SEH64_UNSUPPORTED: return "UNSUPPORTED";
    case MB_WIN_SEH64_NO_UNWIND_INFO: return "NO_UNWIND_INFO";
    }
    return "UNKNOWN";
}
