#ifndef MB_WIN_CRT_BIND_H
#define MB_WIN_CRT_BIND_H

#include "../windows-image/pe_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This callback has the platform's ordinary C ABI. Native PS5 callers include
 * this header: Clang's PS5 C frontend cannot parse the Microsoft ABI entry
 * declarations in mb_win_crt.h. The callable object is built through LLVM IR.
 *
 * Exact supported pairs (DLL ASCII case-insensitive, symbol case-sensitive):
 * VCRUNTIME140.dll: memcpy, memmove, memset, memcmp, memchr, strchr, strrchr, strstr
 * api-ms-win-crt-string-l1-1-0.dll: strlen, strcmp, wcslen, wcscmp, wcsncmp, wcscpy
 * Counted strings need no NUL terminator. All other pairs and ordinal imports
 * return UNRESOLVED. NULL import/output or nonempty NULL string views return
 * ERROR. Only BOUND writes *value. Context is unused and may be NULL.
 * Binding alone makes no assertion that a PE image is ready to execute.
 */
mb_pe_bind_result mb_win_crt_bind(void *, const mb_pe_import *, uint64_t *value);

#ifdef __cplusplus
}
#endif
#endif
