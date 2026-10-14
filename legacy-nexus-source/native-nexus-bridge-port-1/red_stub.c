/* Missing-feature stub for the recorded RED run only; never product-linked. */
#include "nexus_bridge.h"
int mb_nexus_decode(int kind, const unsigned char *bytes, size_t available,
                    const char *domain, size_t domain_length,
                    uint64_t mod_id, char *output, size_t capacity,
                    size_t *length) {
    (void)kind; (void)bytes; (void)available; (void)domain;
    (void)domain_length; (void)mod_id;
    if (length) *length=0;
    if (output && capacity) output[0]=0;
    return MB_NEXUS_INTERNAL_ERROR;
}
