#ifndef MB_WIN_MODULE_ENUM_ABI_H
#define MB_WIN_MODULE_ENUM_ABI_H
#include "mb_win_module_enum.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Windows ABI binder for the module enumeration family. Fourteen private
 * Win64 thunks implement the exact census-observed KERNEL32.dll names:
 * GetModuleHandleA/W, GetModuleHandleExW, GetModuleFileNameA/W,
 * GetProcAddress, LoadLibraryA/W, LoadLibraryExA/W, FreeLibrary,
 * K32EnumProcessModules, K32GetModuleBaseNameA, K32GetModuleInformation.
 * All bind under library "kernel32.dll" (ASCII case-folded); ordinal imports
 * stay UNRESOLVED. The binder stays native-C calling convention; only the
 * thunks carry the Microsoft x64 ABI. Sessions mirror the mutex/init-once
 * family: begin requires a zeroed session, a single installed owner and an
 * EMPTY mb_pe_module; end requires the original session with its module back
 * at EMPTY; the pin is retained across failures for retry. Thunks called
 * before any session set the last-error domain defensively to 6 and fail.
 */
mb_pe_bind_result mb_win_module_enum_bind(void *context,
    const mb_pe_import *import, uint64_t *value);
mb_win_module_enum_status mb_win_module_enum_abi_begin(
    mb_win_module_enum_session *, mb_win_module_enum *, struct mb_pe_module *);
mb_win_module_enum_status mb_win_module_enum_abi_end(mb_win_module_enum_session *);
/* Last mapped Win32 error of the most recent ABI thunk call on this unit
 * (single-threaded runtime; the family has no TLS last-error dependency). */
uint32_t mb_win_module_enum_abi_last_error(void);
#ifdef __cplusplus
}
#endif
#endif
