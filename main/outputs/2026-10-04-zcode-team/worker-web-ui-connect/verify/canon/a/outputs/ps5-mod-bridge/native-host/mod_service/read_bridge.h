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
enum mb_mod_service_read { MB_MOD_SERVICE_DESCRIBE, MB_MOD_SERVICE_RECORDS };
static inline int mb_mod_service_usable(const struct mb_mod_service_bridge *b) {
    return b && b->service && b->request;
}
/* Allocates app-owned bytes; caller frees. 200 preserves service envelope,
 * including ok:false. 502 invalid response, 503 unavailable/allocation failure. */
int mb_mod_service_read(const struct mb_mod_service_bridge *, enum mb_mod_service_read,
                        char **bytes, size_t *length);
#ifdef __cplusplus
}
#endif
#endif
