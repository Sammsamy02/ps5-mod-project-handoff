/* Host-only fault seam. These are pinned PS5 values, NOT the host's flags. */
#ifndef MB_PS5_MEMORY_TEST_BACKEND_H
#define MB_PS5_MEMORY_TEST_BACKEND_H
#include <stddef.h>
#include <stdint.h>
#define MB_NATIVE_PROT_READ 1
#define MB_NATIVE_PROT_WRITE 2
#define MB_NATIVE_PROT_EXEC 4
#define MB_NATIVE_MAP_PRIVATE 2
#define MB_NATIVE_MAP_ANON 0x1000
#define MB_NATIVE_MAP_FAILED ((void *)(intptr_t)-1)
int mb_pe_ps5_test_pagesize(void);
void *mb_pe_ps5_test_map(void *, size_t, int, int, int, int64_t);
int mb_pe_ps5_test_protect(const void *, size_t, int);
int mb_pe_ps5_test_unmap(void *, size_t);
#define MB_NATIVE_PAGESIZE mb_pe_ps5_test_pagesize
#define MB_NATIVE_MAP mb_pe_ps5_test_map
#define MB_NATIVE_PROTECT mb_pe_ps5_test_protect
#define MB_NATIVE_UNMAP mb_pe_ps5_test_unmap
#endif
