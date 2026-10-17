#ifndef MB_MOD_SERVICE_READ_BRIDGE_H
#define MB_MOD_SERVICE_READ_BRIDGE_H
#include "vendor/modforge_service.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MB_MOD_SERVICE_RESPONSE_LIMIT (256u * 1024u)
/* Borrowed for the entire request lifetime. Owner closes service only after
 * host requests stop; bridge never closes the service or unloads its module. */
struct mb_mod_service_bridge {
    mfs_service *service;
    mfs_owned (*request)(mfs_service *, mfs_view);
};
enum mb_mod_service_read { MB_MOD_SERVICE_DESCRIBE, MB_MOD_SERVICE_RECORDS,
                           MB_MOD_SERVICE_REQUEST };
static inline int mb_mod_service_usable(const struct mb_mod_service_bridge *b) {
    return b && b->service && b->request;
}
/* Allocates app-owned bytes; caller frees. 200 preserves service envelope,
 * including ok:false. 502 invalid response, 503 unavailable/allocation failure. */
int mb_mod_service_read(const struct mb_mod_service_bridge *, enum mb_mod_service_read,
                        char **bytes, size_t *length);
/* Generic one-request passthrough: forwards the caller's bounded
 * {"method":...,"params":...} envelope verbatim to the backend and returns
 * the service envelope. Same ownership and status contract as
 * mb_mod_service_read. The backend, not this bridge, validates the method. */
int mb_mod_service_call(const struct mb_mod_service_bridge *,
                        const char *request_bytes, size_t request_bytes_len,
                        char **bytes, size_t *length);
#ifdef __cplusplus
}
#endif
#endif
