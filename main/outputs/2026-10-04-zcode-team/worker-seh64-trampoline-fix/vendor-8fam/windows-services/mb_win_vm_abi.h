#ifndef MB_WIN_VM_ABI_H
#define MB_WIN_VM_ABI_H
/* NEW COMBINED-PAYLOAD GLUE (worker-combined-dll-allfam). The virtual-memory
 * owner (mb_win_vm.{c,h}, vendored unchanged from
 * outputs/2026-10-04-zcode-team/worker-virtual-memory/sources/) ships a
 * native-C ABI only. This unit adds the session lifecycle, the Microsoft x64
 * forwarding thunks and the PE binder, following the mb_win_mutex_abi.h
 * pattern exactly, so the combined payload can exercise the family through
 * actual PE imports. Census-observed KERNEL32.dll names (both retained
 * censuses, UE4SS.dll family "heap"): VirtualAlloc, VirtualFree,
 * VirtualProtect, VirtualQuery, VirtualQueryEx. Library ASCII-folded
 * kernel32.dll; ordinals and other libraries unresolved; invalid session/view
 * -> ERROR; only BOUND writes the output address. Documented Microsoft x64
 * signatures: LPVOID VirtualAlloc(LPVOID,DWORD,DWORD,DWORD) (dwSize is
 * 32-bit), BOOL VirtualFree(LPVOID,DWORD,DWORD), BOOL
 * VirtualProtect(LPVOID,DWORD,DWORD,PDWORD), SIZE_T
 * VirtualQuery(LPCVOID,PMEMORY_BASIC_INFORMATION,DWORD) and SIZE_T
 * VirtualQueryEx(HANDLE,LPCVOID,PMEMORY_BASIC_INFORMATION,DWORD). */
#include "mb_win_vm.h"
#include "../windows-execution/pe_module.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mb_win_vm_abi_status {
    MB_WIN_VM_ABI_OK = 0, MB_WIN_VM_ABI_INVALID_ARGUMENT,
    MB_WIN_VM_ABI_BAD_STATE, MB_WIN_VM_ABI_BUSY
} mb_win_vm_abi_status;
typedef struct mb_win_vm_abi_session {
    mb_pe_module *module;
    struct mb_win_vm_abi_session *next;
    uint8_t active;
} mb_win_vm_abi_session;
/* Stationary zeroed sessions; begin before the module leaves
 * MB_PE_MODULE_EMPTY, end after it returns to EMPTY. Begin/end require
 * global call quiescence; binding and the five thunks are externally
 * serialized. Sessions share one installed owner (the first begin wins);
 * every consumer pins the same owner instance it passes in. The owner's
 * backend (mb_win_vm_ops) is configured by the caller before begin. */
mb_win_vm_abi_status mb_win_vm_abi_begin_session(mb_win_vm_abi_session *,
                                                 mb_win_vm *, mb_pe_module *);
mb_win_vm_abi_status mb_win_vm_abi_end_session(mb_win_vm_abi_session *);
mb_pe_bind_result mb_win_vm_abi_bind(void *, const mb_pe_import *, uint64_t *);
#ifdef __cplusplus
}
#endif
#endif
