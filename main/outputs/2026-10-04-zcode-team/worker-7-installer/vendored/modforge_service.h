/*
 * modforge_service.h — the mod backend's contract with the PS5 app.
 *
 * Contract "modforge-service/1". The app (UI, Nexus account, downloads) links
 * the backend library and talks to it only through this header. Everything
 * the backend does happens on the console: preparing original Nexus
 * archives, prerequisite planning, FOMOD choices, conflict and load-order
 * resolution, reversible install/uninstall, crash recovery, and reporting
 * what the in-game runtime is doing. Credentials, accounts and downloading
 * stay in the app; the backend never contacts a provider.
 *
 * Style matches ModBridge's backend.h: a plain C ABI, UTF-8 JSON with explicit
 * lengths in both directions, and every result owned by the backend with its
 * own release callback. No C++ types, allocators or game pointers cross it.
 *
 *   mfs_open_params p = {MFS_ABI_VERSION, sizeof p, {cfg, cfg_len}, on_event, ctx};
 *   mfs_service *svc;
 *   mfs_owned err;
 *   if (mfs_open(&p, &svc, &err) != 0) { show(err); mfs_release(&err); }
 *   mfs_owned r = mfs_request(svc, (mfs_view){req, req_len});
 *   ... parse r.bytes ... ; mfs_release(&r);
 *   mfs_close(svc);
 *
 * Requests and replies, the job model, events, error codes and the durable
 * record states are specified in docs/SERVICE.md. In short:
 *
 *   request  {"method": "<name>", "params": {...}}
 *   reply    {"ok": true, "result": {...}}  or  {"ok": false, "error": {"code", "message", ...}}
 *
 * Long operations (mod.prepare, install.apply, mod.uninstall) return a
 * job id at once and report through events and job.status. States the app
 * shows: downloaded -> prepared -> installed -> running (see SERVICE.md).
 *
 * Threading: mfs_request may be called from any thread; calls are serialised
 * internally and return promptly (no call blocks on a job). on_event is
 * invoked from a backend worker thread, one event at a time; it must copy what
 * it needs and must not call into the service. mfs_close cancels running jobs,
 * waits for them to stop at a safe point, and persists state.
 */
#ifndef MODFORGE_SERVICE_H
#define MODFORGE_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MFS_ABI_VERSION 1u
#define MFS_CONTRACT "modforge-service/1"

typedef struct mfs_service mfs_service;

typedef struct mfs_view {
    const char *bytes;   /* UTF-8 JSON, not necessarily NUL-terminated */
    size_t length;
} mfs_view;

/* Owned by the backend. Release exactly once with mfs_release (or by calling
 * release(owner, bytes, length) directly). bytes is NUL-terminated for
 * convenience; length excludes the NUL. */
typedef struct mfs_owned {
    const char *bytes;
    size_t length;
    void *owner;
    void (*release)(void *owner, const char *bytes, size_t length);
} mfs_owned;

/* One JSON event object, valid only during the call. */
typedef void (*mfs_event_fn)(void *user, mfs_view event_json);

typedef struct mfs_open_params {
    uint32_t abi_version;     /* MFS_ABI_VERSION                                 */
    uint32_t struct_size;     /* sizeof(mfs_open_params)                         */
    mfs_view config_json;     /* see SERVICE.md "Configuration"                  */
    mfs_event_fn on_event;    /* may be NULL (poll job.status instead)           */
    void *event_user;
} mfs_open_params;

/* Open the service: loads engine definitions and profiles, then recovers any
 * install that was interrupted (power loss, crash) before returning. Returns 0
 * and *out, or a negative value with *error set to a JSON error object
 * (release it). */
int mfs_open(const mfs_open_params *params, mfs_service **out, mfs_owned *error);

/* One request, one reply (see SERVICE.md for every method). Never NULL bytes;
 * failures are {"ok": false, ...} replies. */
mfs_owned mfs_request(mfs_service *svc, mfs_view request_json);

/* Cancel jobs, wait for them to reach a safe point, persist, free. */
void mfs_close(mfs_service *svc);

static inline void mfs_release(mfs_owned *o) {
    if (o && o->release) o->release(o->owner, o->bytes, o->length);
    if (o) {
        o->bytes = 0;
        o->length = 0;
        o->owner = 0;
        o->release = 0;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* MODFORGE_SERVICE_H */
