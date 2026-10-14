#ifndef MB_WIN_SEH64_ABI_H
#define MB_WIN_SEH64_ABI_H
/* SEH64 family ABI unit (worker-seh64-family, per worker-seh64-design
 * DESIGN.md §4.1): session lifecycle, the PE binder, the Microsoft x64
 * forwarding thunks and the control-plane trampolines. Pattern follows
 * mb_win_vm_abi.h / mb_win_crt_startup_abi.h.
 *
 * Bound names, ASCII-folded, ordinals unresolved; invalid session/view ->
 * ERROR; only BOUND writes the output address.
 *   kernel32.dll (census-observed UE4SS-only stack-walker kit + state owner):
 *     RtlCaptureContext, RtlLookupFunctionEntry, RtlVirtualUnwind,
 *     SetUnhandledExceptionFilter
 *   kernel32.dll (dynamic unwind registration; NOT census-observed - design
 *     5a promotes RtlAddFunctionTable to "required internally"; exposed here
 *     per the worker packet's directed minimum so PE code can register
 *     bridge/growable tables):
 *     RtlAddFunctionTable, RtlRemoveFunctionTable
 *   kernel32.dll (substrate raise entry; NOT census-observed - design 5c/5d
 *     provides raise-record construction as internal substrate; bound per the
 *     worker packet's directed "raise/dispatch/recover" minimum):
 *     RaiseException
 *   vcruntime140.dll (census-observed SEH64 personality):
 *     __C_specific_handler
 * Everything else returns UNRESOLVED so the import pass carries it to its
 * owning family - never a fake success.
 *
 * The owner (mb_win_seh64.c) is instantiated by the integration (one per
 * process here); all sessions share the first installed owner. Registration
 * of a bound module's exception directory happens at its first SEH64 bind
 * (the module image is mapped and LOADING then); it is removed at session
 * end, which requires the module back at MB_PE_MODULE_EMPTY. */
#include "mb_win_seh64.h"
#include "../windows-execution/pe_module.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mb_win_seh64_abi_status {
    MB_WIN_SEH64_ABI_OK = 0, MB_WIN_SEH64_ABI_INVALID_ARGUMENT,
    MB_WIN_SEH64_ABI_BAD_STATE, MB_WIN_SEH64_ABI_BUSY
} mb_win_seh64_abi_status;
typedef struct mb_win_seh64_abi_session {
    mb_pe_module *module;
    struct mb_win_seh64_abi_session *next;
    const void *registered_image;
    uint32_t image_size, pdata_rva, pdata_size;
    uint8_t active, registered;
} mb_win_seh64_abi_session;
/* Stationary zeroed session; begin before the module leaves
 * MB_PE_MODULE_EMPTY, end after it returns to EMPTY. Begin/end require
 * global call quiescence; binding and the thunks are externally serialized.
 * Every session shares the single owner instance it passes in (first wins).
 * The owner itself is instantiated and driven by this unit: open it once
 * before the first session (passing its pointer to the integration for the
 * report/registry evidence) and close it after the last session ends. */
mb_win_seh64_abi_status mb_win_seh64_abi_begin_session(mb_win_seh64_abi_session *,
                                                       mb_win_seh64 *, mb_pe_module *);
mb_win_seh64_abi_status mb_win_seh64_abi_end_session(mb_win_seh64_abi_session *);
mb_pe_bind_result mb_win_seh64_abi_bind(void *, const mb_pe_import *, uint64_t *);
mb_win_seh64_status mb_win_seh64_abi_open_owner(mb_win_seh64 **);
mb_win_seh64_status mb_win_seh64_abi_close_owner(void);
#ifdef __cplusplus
}
#endif
#endif
