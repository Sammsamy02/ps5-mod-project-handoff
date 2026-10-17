#ifndef MB_MOD_SERVICE_OWNER_H
#define MB_MOD_SERVICE_OWNER_H
#include "read_bridge.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MB_MOD_SERVICE_CONFIG_LIMIT (64u * 1024u)

/* Exact backend C ABI operations. The app supplies these from its linked
 * backend; they are never selected by a browser request. */
struct mb_mod_service_operations {
    int (*open)(const mfs_open_params *, mfs_service **, mfs_owned *);
    mfs_owned (*request)(mfs_service *, mfs_view);
    void (*close)(mfs_service *);
};

/* Zero-initialize before first use. Do not copy or alter a live owner.
 * Call close only after every borrower has stopped using the bridge. */
struct mb_mod_service_owner {
    struct mb_mod_service_bridge bridge;
    void (*close)(mfs_service *);
};

enum mb_mod_service_owner_status {
    MB_MOD_SERVICE_OWNER_OK = 0,
    MB_MOD_SERVICE_OWNER_INVALID_ARGUMENT,
    MB_MOD_SERVICE_OWNER_ALREADY_OPEN,
    MB_MOD_SERVICE_OWNER_OPEN_FAILED,
    MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION,
    MB_MOD_SERVICE_OWNER_UNSUPPORTED_CONTRACT,
    MB_MOD_SERVICE_OWNER_RESOURCE_FAILED
};

/* Config is an app-owned JSON object, borrowed only for this startup call.
 * Opening may recover/write backend state. Events are disabled; callers poll.
 * Open must succeed and service.describe must advertise MFS_CONTRACT,
 * service.describe, and records.list before a bridge becomes available.
 * Additional advertised methods do not enable additional app operations.
 * Failure leaves an initially empty owner empty and releases backend results
 * and any returned service. An already-open owner is retained unchanged. */
enum mb_mod_service_owner_status mb_mod_service_owner_open(
    struct mb_mod_service_owner *, const struct mb_mod_service_operations *, mfs_view config);
const struct mb_mod_service_bridge *mb_mod_service_owner_bridge(const struct mb_mod_service_owner *);
/* Clears the borrowed bridge before closing the backend exactly once.
 * Repeated close calls are harmless. */
void mb_mod_service_owner_close(struct mb_mod_service_owner *);
const char *mb_mod_service_owner_status_name(enum mb_mod_service_owner_status);

#ifdef __cplusplus
}
#endif
#endif
