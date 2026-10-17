#ifndef MODBRIDGE_ENTROPY_H
#define MODBRIDGE_ENTROPY_H
#include <stddef.h>
#include <stdint.h>

#define MB_ENTROPY_BYTES 32
#define MB_ENTROPY_API_LIMIT 4
struct mb_entropy_budget { unsigned api_count; };

/* Metadata only. A returned length does not attest how many bytes were written. */
struct mb_entropy_result {
    const char *status;
    const char *reason;
    unsigned call_count;
    int return_code;
    int error_number;
    size_t returned_length;
    int64_t elapsed_ms; /* -1 when timing is unavailable. */
};

/* result and buffer must be distinct, valid caller-owned objects. The complete
   32-byte buffer is cleared before returning. A native call has no hard deadline. */
void mb_entropy_observe(struct mb_entropy_result *result,
                        unsigned char buffer[MB_ENTROPY_BYTES]);
/* Same strict one-call adapter. The shared budget reserves before each actual
   native attempt. Only success leaves 32 bytes for the owned callback to copy.
   All returned failures clear the full buffer capacity. No retry or reset. */
int mb_entropy_read32(struct mb_entropy_result *result,
                      unsigned char buffer[MB_ENTROPY_BYTES],
                      struct mb_entropy_budget *budget);
#endif
