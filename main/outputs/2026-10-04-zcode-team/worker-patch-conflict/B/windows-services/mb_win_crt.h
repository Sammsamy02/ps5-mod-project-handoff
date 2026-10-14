#ifndef MB_WIN_CRT_H
#define MB_WIN_CRT_H

#include "mb_win_crt_bind.h"
#include <limits.h>

#if !defined(__x86_64__) && !defined(_M_X64)
#error "Windows CRT services require an AMD64 target"
#endif
#if defined(__clang__) || defined(__GNUC__)
#define MB_WIN_X64_ABI __attribute__((ms_abi))
#else
#error "Windows CRT services require a compiler with ms_abi support"
#endif

#ifdef __cplusplus
static_assert(sizeof(void *) == 8 && sizeof(size_t) == 8 &&
    sizeof(uintptr_t) == 8 && sizeof(int) == 4 && sizeof(uint16_t) == 2 && CHAR_BIT == 8,
    "Windows x64 pointer, size_t, int and code-unit widths are required");
extern "C" {
#else
_Static_assert(sizeof(void *) == 8 && sizeof(size_t) == 8 &&
    sizeof(uintptr_t) == 8 && sizeof(int) == 4 && sizeof(uint16_t) == 2 && CHAR_BIT == 8,
    "Windows x64 pointer, size_t, int and code-unit widths are required");
#endif

/* Caller supplies valid buffers and terminated strings, as for the CRT.
 * memcpy ranges must not overlap. Comparisons use unsigned byte ordering.
 * These entry points use Microsoft x64 ABI; the binder uses the host C ABI.
 */
void *MB_WIN_X64_ABI mb_win_memcpy(void *, const void *, size_t);
void *MB_WIN_X64_ABI mb_win_memmove(void *, const void *, size_t);
void *MB_WIN_X64_ABI mb_win_memset(void *, int, size_t);
int MB_WIN_X64_ABI mb_win_memcmp(const void *, const void *, size_t);
size_t MB_WIN_X64_ABI mb_win_strlen(const char *);
int MB_WIN_X64_ABI mb_win_strcmp(const char *, const char *);

/* Search results point into the original input or are NULL. Search value is
 * converted to its unsigned byte. memchr scans at most count bytes (including
 * embedded NUL) and reads nothing for zero count. strchr/strrchr may find the
 * terminating NUL. strstr returns the first match, or the input for an empty
 * needle; both inputs must be terminated strings. No allocation or writes.
 */
void *MB_WIN_X64_ABI mb_win_memchr(const void *, int, size_t);
char *MB_WIN_X64_ABI mb_win_strchr(const char *, int);
char *MB_WIN_X64_ABI mb_win_strrchr(const char *, int);
char *MB_WIN_X64_ABI mb_win_strstr(const char *, const char *);

/* Windows wide strings use unsigned 16-bit code units, independently of host
 * wchar_t. These operations do not decode UTF-16 or apply locale rules.
 * wcslen counts code units before NUL. wcsncmp stops at count or either NUL;
 * count zero reads nothing. wcscpy includes NUL and requires nonoverlap and
 * enough destination space. Comparison returns preserve only ordering/sign.
 */
size_t MB_WIN_X64_ABI mb_win_wcslen(const uint16_t *);
int MB_WIN_X64_ABI mb_win_wcscmp(const uint16_t *, const uint16_t *);
int MB_WIN_X64_ABI mb_win_wcsncmp(const uint16_t *, const uint16_t *, size_t);
uint16_t *MB_WIN_X64_ABI mb_win_wcscpy(uint16_t *, const uint16_t *);

#ifdef __cplusplus
}
#endif
#endif
