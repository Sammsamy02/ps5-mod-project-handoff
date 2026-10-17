#ifndef MODBRIDGE_CORE_BRIDGE_H
#define MODBRIDGE_CORE_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#define MB_CORE_INPUT_LIMIT_BYTES 8388608
#define MB_CORE_REPORT_CAPACITY 2048
enum mb_core_status {
    MB_CORE_OK=0, MB_CORE_INVALID_ARGUMENT=1, MB_CORE_BUFFER_TOO_SMALL=2,
    MB_CORE_INTERNAL_ERROR=3, MB_CORE_INPUT_LIMIT=4
};
#ifdef __cplusplus
#define MB_CORE_NOEXCEPT noexcept
extern "C" {
#else
#define MB_CORE_NOEXCEPT
#endif
/* Caller owns all memory. length excludes NUL; success needs capacity > length.
 * Size queries/short buffers return BUFFER_TOO_SMALL and leave output empty.
 * Other failures leave length zero and output empty. Parser failures are JSON.
 * length is required; NULL output/input require zero capacity/available. */
int mb_core_inspect_dds(const unsigned char *bytes, size_t available,
                        uint64_t total_size, char *output, size_t capacity,
                        size_t *length) MB_CORE_NOEXCEPT;
int mb_core_selftest(char *output, size_t capacity, size_t *length) MB_CORE_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef MB_CORE_NOEXCEPT
#endif
