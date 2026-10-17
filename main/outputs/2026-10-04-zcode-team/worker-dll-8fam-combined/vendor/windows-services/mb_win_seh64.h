#ifndef MB_WIN_SEH64_H
#define MB_WIN_SEH64_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* SEH64 structured-exception-handling family: pure-logic owner.
 * Implemented per outputs/2026-10-04-zcode-team/worker-seh64-design/DESIGN.md
 * (design-only; this is its first implementation). Freestanding: no OS calls,
 * no allocation, no libc. All layout widths are contracts (house rule).
 *
 * Scope of this prototype (honest, per design §6 M0/M1 + the worker packet's
 * directed minimum):
 *   - Win64 data contracts: CONTEXT, RUNTIME_FUNCTION, UNWIND_INFO,
 *     EXCEPTION_RECORD, DISPATCHER_CONTEXT (self-consistent layout; see H5).
 *   - Function-table registry: in-image .pdata tables (registered per module
 *     session by the ABI unit) and dynamic tables (RtlAddFunctionTable/
 *     RtlRemoveFunctionTable shaped).
 *   - RtlLookupFunctionEntry / RtlVirtualUnwind core (opcode decode, prologue
 *     replay, body convention, bounded epilogue scan, depth-limited chains).
 *   - Two-pass dispatch driver (search then unwind) with injected control
 *     callbacks so every ms_abi transition happens in the ABI unit.
 *   - __C_specific_handler decision core (scope tables, filter 0/1/real,
 *     target selection, noncontinuable check).
 * Not implemented (documented non-goals): signal->exception shim and
 * continue-execution resume (design M2/H3), vectored handlers, callback or
 * growable tables, stack-overflow dispatch, full COLLIDED_UNWIND, C++ EH
 * personalities, guard trampoline rsp parity with ntdll (design H2: bodies
 * and filters that address parent locals via the guard rsp are unsupported;
 * this family's fixtures touch only globals). Byte-exact ntdll parity is not
 * claimed; semantic conformance for MSVC-MSVC round trips is the target. */

#define MB_WIN_SEH64_MAX_TABLES 8
#define MB_WIN_SEH64_MAX_CHAIN 8
#define MB_WIN_SEH64_MAX_ARGS 15

/* UNWIND_INFO flags (winnt.h). */
#define MB_WIN_SEH64_UNW_FLAG_NHANDLER 0u
#define MB_WIN_SEH64_UNW_FLAG_EHANDLER 1u
#define MB_WIN_SEH64_UNW_FLAG_UHANDLER 2u
#define MB_WIN_SEH64_UNW_FLAG_CHAININFO 4u
/* EXCEPTION_RECORD flags. */
#define MB_WIN_SEH64_NONCONTINUABLE 1u
#define MB_WIN_SEH64_UNWINDING 2u
#define MB_WIN_SEH64_EXIT_UNWIND 4u
#define MB_WIN_SEH64_TARGET_UNWIND 8u
#define MB_WIN_SEH64_COLLIDED_UNWIND 32u
/* Filter/personality dispositions. */
#define MB_WIN_SEH64_CONTINUE_EXECUTION (-1)
#define MB_WIN_SEH64_CONTINUE_SEARCH 0
#define MB_WIN_SEH64_EXECUTE_HANDLER 1
#define MB_WIN_SEH64_EXCEPTION_CONTINUE_SEARCH 0u
#define MB_WIN_SEH64_EXCEPTION_CONTINUE_EXECUTION 1u
#define MB_WIN_SEH64_EXCEPTION_EXECUTE_HANDLER 2u
/* RtlVirtualUnwind handler types. */
#define MB_WIN_SEH64_UWOP_PUSH_NONVOL 0u
#define MB_WIN_SEH64_UWOP_ALLOC_LARGE 1u
#define MB_WIN_SEH64_UWOP_ALLOC_SMALL 2u
#define MB_WIN_SEH64_UWOP_SET_FPREG 3u
#define MB_WIN_SEH64_UWOP_SAVE_NONVOL 4u
#define MB_WIN_SEH64_UWOP_SAVE_NONVOL_FAR 5u
#define MB_WIN_SEH64_UWOP_SAVE_XMM 6u
#define MB_WIN_SEH64_UWOP_SAVE_XMM_FAR 7u
#define MB_WIN_SEH64_UWOP_SAVE_XMM128 8u
#define MB_WIN_SEH64_UWOP_SAVE_XMM128_FAR 9u
#define MB_WIN_SEH64_UWOP_PUSH_MACHFRAME 10u

/* Exact Win64 CONTEXT layout subset with full Windows offsets (0x4D0 bytes).
 * Floating state (MxCsr/FltSave/VectorRegister) is carried as opaque bytes:
 * this prototype restores integer state only, which is what the unwind data
 * plane consumes. Offsets pinned by _Static_assert in the .c file. */
typedef struct mb_win_seh64_context {
    uint64_t p1_home, p2_home, p3_home, p4_home, p5_home, p6_home;
    uint32_t context_flags, mx_csr;
    uint16_t seg_cs, seg_ds, seg_es, seg_fs, seg_gs, seg_ss;
    uint32_t eflags;
    uint64_t dr0, dr1, dr2, dr3, dr6, dr7;
    uint64_t rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15, rip;
    uint8_t flt_save[512];
    uint8_t vector_register[26 * 16];
    uint64_t vector_control;
    uint64_t debug_control, last_branch_to_rip, last_branch_from_rip;
    uint64_t last_exception_to_rip, last_exception_from_rip;
} mb_win_seh64_context;

/* 3xu32, 12 bytes. */
typedef struct mb_win_seh64_runtime_function {
    uint32_t begin_address, end_address, unwind_data;
} mb_win_seh64_runtime_function;

/* UNWIND_INFO accessed through byte masks (no bitfields: layout is a
 * contract and bitfield layout is implementation-defined). */
typedef struct mb_win_seh64_unwind_info {
    uint8_t version_flags; /* bits 0..2 version, bits 3..7 flags */
    uint8_t size_of_prologue;
    uint8_t count_of_codes;
    uint8_t frame_reg_off; /* bits 0..3 frame register, bits 4..7 offset/16 */
} mb_win_seh64_unwind_info;

#define MB_WIN_SEH64_INFO_VERSION(info) ((uint32_t)((info)->version_flags & 0x07u))
#define MB_WIN_SEH64_INFO_FLAGS(info) ((uint32_t)((info)->version_flags >> 3))
#define MB_WIN_SEH64_INFO_FRAME_REG(info) ((uint32_t)((info)->frame_reg_off & 0x0Fu))
#define MB_WIN_SEH64_INFO_FRAME_OFF(info) ((uint32_t)((info)->frame_reg_off >> 4))

/* 0x98 bytes on x64. */
typedef struct mb_win_seh64_exception_record {
    uint32_t exception_code, exception_flags;
    struct mb_win_seh64_exception_record *exception_record;
    void *exception_address;
    uint32_t number_parameters;
    uint32_t __alignment;
    uint64_t exception_information[MB_WIN_SEH64_MAX_ARGS];
} mb_win_seh64_exception_record;

typedef struct mb_win_seh64_exception_pointers {
    mb_win_seh64_exception_record *exception_record;
    mb_win_seh64_context *context_record;
} mb_win_seh64_exception_pointers;

/* Self-consistent dispatcher context; the fields personalities consume
 * (ControlPc, FunctionEntry, EstablisherFrame, ContextRecord, HandlerData)
 * keep their Win64 position contract (design §4.3). image_base, handler_rva
 * and target_ip are this implementation's dispatch plumbing. */
typedef struct mb_win_seh64_dispatcher_context {
    uint64_t control_pc;
    const mb_win_seh64_runtime_function *function_entry;
    /* Win64 EstablisherFrame contract: the frame register value when the
     * UNWIND_INFO declares SET_FPREG, otherwise the frame's own rsp. */
    uint64_t establisher_frame;
    mb_win_seh64_context *context_record;
    const void *handler_data;
    /* The frame's own rsp at ControlPc (transfer/invocation base). */
    uint64_t frame_rsp;
    uint64_t history_oracle;
    uint32_t scope_index;
    uint32_t control_pc_is_unwinding;
    const uint8_t *image_base;
    uint32_t handler_rva;
    uint32_t __pad;
    uint64_t target_ip;
} mb_win_seh64_dispatcher_context;

/* Homes of saved nonvolatile registers ( personalities may read caller
 * locals through this). XMM homes are not tracked (documented subset). */
typedef struct mb_win_seh64_context_pointers {
    uint64_t *rbx, *rbp, *rsi, *rdi;
    uint64_t *r12, *r13, *r14, *r15;
} mb_win_seh64_context_pointers;

typedef enum mb_win_seh64_status {
    MB_WIN_SEH64_OK = 0, MB_WIN_SEH64_INVALID_ARGUMENT, MB_WIN_SEH64_BAD_STATE,
    MB_WIN_SEH64_BUSY, MB_WIN_SEH64_LIMIT, MB_WIN_SEH64_UNSUPPORTED,
    MB_WIN_SEH64_NO_UNWIND_INFO
} mb_win_seh64_status;

typedef enum mb_win_seh64_table_kind {
    MB_WIN_SEH64_TABLE_IMAGE = 1, MB_WIN_SEH64_TABLE_DYNAMIC = 2
} mb_win_seh64_table_kind;

typedef struct mb_win_seh64_table {
    uint8_t live, kind;
    const uint8_t *base;                    /* RVA base for entries/unwind data */
    const mb_win_seh64_runtime_function *entries;
    uint32_t count;
} mb_win_seh64_table;

/* Control plane injected by the ABI unit: every transition into ms_abi code
 * (personality invocation, filter/finally execution, final transfer) goes
 * through these. The owner itself is native-CC and never touches ms_abi. */
typedef struct mb_win_seh64_control {
    /* Invoke a resolved personality: Win64 EXCEPTION_DISPOSITION signature. */
    int32_t (*call_personality)(void *personality,
        mb_win_seh64_exception_record *record, void *establisher_frame,
        mb_win_seh64_context *context, mb_win_seh64_dispatcher_context *dctx);
    /* Invoke fn(arg) with RSP inside the establishing frame; returns fn's
     * int32 result and restores the dispatcher stack afterwards. */
    int32_t (*call_at_frame)(void *fn, uint64_t establisher_rsp, void *arg);
    /* Apply a full machine context and jump; never returns. */
    void (*resume)(const mb_win_seh64_context *context);
} mb_win_seh64_control;

typedef struct mb_win_seh64 {
    uint8_t active;
    mb_win_seh64_table tables[MB_WIN_SEH64_MAX_TABLES];
    mb_win_seh64_control control;
    int (*unhandled_filter)(mb_win_seh64_exception_pointers *); /* previous wins */
    unsigned dispatch_depth;
} mb_win_seh64;

mb_win_seh64_status mb_win_seh64_open(mb_win_seh64 *, const mb_win_seh64_control *);
mb_win_seh64_status mb_win_seh64_close(mb_win_seh64 *);

/* Registry. Image tables: entries must be sorted, non-overlapping, within the
 * image; validated here (in-image bounds + 12-byte stride + ordering).
 * Dynamic tables: registered exactly as given; the caller owns the storage. */
mb_win_seh64_status mb_win_seh64_register_image_table(mb_win_seh64 *,
    const uint8_t *image, uint64_t image_size, uint32_t pdata_rva,
    uint32_t pdata_size, unsigned *slot);
mb_win_seh64_status mb_win_seh64_unregister_image_table(mb_win_seh64 *,
    const uint8_t *image);
mb_win_seh64_status mb_win_seh64_add_function_table(mb_win_seh64 *,
    const mb_win_seh64_runtime_function *entries, uint32_t count,
    uint64_t base, unsigned *handle /* index+1; 0 = refusal */);
mb_win_seh64_status mb_win_seh64_remove_function_table(mb_win_seh64 *,
    unsigned handle, unsigned *removed /* 0/1, double remove = 0 */);

/* Lookup: image tables first, then dynamic tables (LIFO within each group).
 * control_pc outside every range -> NULL with MB_WIN_SEH64_NO_UNWIND_INFO. */
mb_win_seh64_status mb_win_seh64_lookup_function_entry(const mb_win_seh64 *,
    uint64_t control_pc, const mb_win_seh64_runtime_function **entry,
    const uint8_t **image_base);

/* RtlVirtualUnwind core. handler_type: 0 search, 1 EHANDLER, 2 UHANDLER.
 * Computes the caller context (three-region rule), fills the dispatcher
 * context (handler RVA + scope-table data when the info declares handlers),
 * and records nonvolatile homes. Refuses PUSH_MACHFRAME and over-deep
 * chains as MB_WIN_SEH64_UNSUPPORTED. */
mb_win_seh64_status mb_win_seh64_virtual_unwind(uint32_t handler_type,
    uint64_t control_pc, const uint8_t *image_base,
    const mb_win_seh64_runtime_function *entry, mb_win_seh64_context *context,
    mb_win_seh64_dispatcher_context *dctx,
    mb_win_seh64_context_pointers *pointers);

/* __C_specific_handler decision core (native-CC mirror of the personality
 * semantics; the ABI unit's ms_abi personality forwards here). Returns the
 * Win64 EXCEPTION_DISPOSITION (0/1/2). When a scope is selected it requests
 * unwind through *unwind_out {establisher_frame, target_ip} and returns
 * MB_WIN_SEH64_EXCEPTION_CONTINUE_SEARCH with the TARGET_UNWIND flag set in
 * the record (the personality then drives the engine's unwind pass). */
mb_win_seh64_status mb_win_seh64_csh_decide(mb_win_seh64 *owner,
    mb_win_seh64_exception_record *record, void *establisher_frame,
    mb_win_seh64_context *context, mb_win_seh64_dispatcher_context *dctx,
    int32_t *disposition_out, uint64_t *unwind_target_frame,
    uint64_t *unwind_target_ip);

/* Dispatch driver (OS-role, design §5c): pass 1 search, then
 * mb_win_seh64_unwind_to (pass 2) to the selected target frame and
 * control->resume into it. A walk that finds no handler invokes the
 * unhandled filter (when installed) and returns
 * MB_WIN_SEH64_NO_UNWIND_INFO / BAD_STATE without transferring control (the
 * anti-fake anchor: an unhandled exception is reported, never swallowed). */
mb_win_seh64_status mb_win_seh64_dispatch(mb_win_seh64 *,
    mb_win_seh64_exception_record *record, mb_win_seh64_context *context);

/* Pass 2: unwind from the fault toward target_frame (running UNWINDING
 * personalities through the injected Win64 transition), then transfer to
 * target_ip. Never returns on success. Exposed for the personality path:
 * __C_specific_handler forwards its selection here exactly as the real
 * personality forwards to RtlUnwind. */
mb_win_seh64_status mb_win_seh64_unwind_to(mb_win_seh64 *,
    mb_win_seh64_exception_record *record, mb_win_seh64_context *context,
    uint64_t target_frame, uint64_t target_ip);

int (*mb_win_seh64_set_unhandled_filter(mb_win_seh64 *,
    int (*filter)(mb_win_seh64_exception_pointers *)))(mb_win_seh64_exception_pointers *);

const char *mb_win_seh64_status_name(mb_win_seh64_status);
#ifdef __cplusplus
}
#endif
#endif
