#ifndef MODBRIDGE_BACKEND_H
#define MODBRIDGE_BACKEND_H

/* Future native-backend interface. The desktop core does not yet dynamically
 * load backend modules. JSON records use UTF-8 and explicit byte lengths;
 * no C++ containers or game pointers cross this boundary. */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_BACKEND_ABI_VERSION 1u

typedef struct mb_backend_handle mb_backend_handle;

typedef struct mb_json_view {
    const char* bytes;
    size_t length;
} mb_json_view;

/* Every owned result has its producing module's release callback. The host
 * calls it once after consuming the bytes, before unloading the module. */
typedef struct mb_owned_json {
    const char* bytes;
    size_t length;
    void* owner;
    void (*release)(void* owner, const char* bytes, size_t length);
} mb_owned_json;

typedef enum mb_operation {
    MB_ANALYZE = 1,
    MB_ASSESS = 2,
    MB_PREPARE = 3,
    MB_VALIDATE = 4
} mb_operation;

typedef struct mb_backend_api {
    uint32_t abi_version;
    uint32_t struct_size;
    /* Returned descriptor includes backend ID/version, operations, requirements
     * and evidence scope. It does not establish a working console capability. */
    mb_owned_json (*describe)(mb_backend_handle* handle);
    mb_owned_json (*invoke)(mb_backend_handle* handle, mb_operation operation,
                           mb_json_view request);
    /* Destroy only after calls/results/callbacks have quiesced. */
    void (*destroy)(mb_backend_handle* handle);
} mb_backend_api;

#ifdef __cplusplus
}
#endif
#endif
