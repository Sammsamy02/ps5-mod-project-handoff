#ifndef MB_WIN_ENTRY_H
#define MB_WIN_ENTRY_H

#include <stdint.h>
#include <limits.h>

#if !defined(__x86_64__) && !defined(_M_X64)
#error "Windows entry bridge requires an AMD64 target"
#endif

#ifdef __cplusplus
static_assert(sizeof(void *) == 8 && sizeof(uintptr_t) == 8 &&
    sizeof(int32_t) == 4 && sizeof(uint32_t) == 4 && CHAR_BIT == 8,
    "Windows x64 entry pointer, BOOL and DWORD widths are required");
extern "C" {
#else
_Static_assert(sizeof(void *) == 8 && sizeof(uintptr_t) == 8 &&
    sizeof(int32_t) == 4 && sizeof(uint32_t) == 4 && CHAR_BIT == 8,
    "Windows x64 entry pointer, BOOL and DWORD widths are required");
#endif

/* Native C ABI callback. Calls exactly one Microsoft x64 DLL entry function
 * with (image_base, reason, reserved), returning its 32-bit BOOL unchanged.
 * unused is ignored. Caller guarantees entry_address is non-NULL, executable,
 * has the DLL entry signature and remains mapped through the entire call.
 * Caller supplies lifecycle policy (including reason/reserved), serialization
 * and memory permissions. No exception/longjmp may cross this call boundary;
 * this bridge provides no exception handling or loader initialization.
 */
int32_t mb_win_call_entry(void *unused, void *entry_address, void *image_base,
    uint32_t reason, void *reserved);

#ifdef __cplusplus
}
#endif
#endif
