#ifndef MB_CURL_PS5_ENTROPY_H
#define MB_CURL_PS5_ENTROPY_H
#include <stdint.h>

/* Target-only source for the serialized curl owner: bind once with a NULL
 * context. One checked sysctl(KERN_ARND) call, no retry/fallback or stored seed.
 * Zero means return code/errno/returned length were exactly 0/0/32. Every
 * returned failure clears a valid 32-byte output. The owner reserves the
 * operation budget and terminally latches source faults; this source has no
 * reset, budget or clock policy. The synchronous OS call has no hard deadline.
 * Caller supplies a valid, exclusively owned 32-byte output for each call. */
int mb_curl_ps5_entropy_read32_v1(void *context, uint8_t output[32]);
#endif
