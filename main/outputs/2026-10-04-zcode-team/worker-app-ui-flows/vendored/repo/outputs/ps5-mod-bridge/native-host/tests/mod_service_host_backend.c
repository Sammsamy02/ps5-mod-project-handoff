/* Test-only backend operations. The caller owns MB_TEST_SERVICE_TRACE in a
 * TemporaryDirectory; this fixture never opens a game/provider path. */
#include "mod_service/vendor/modforge_service.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mfs_service { int open; };
static struct mfs_service instance;
static void event(const char *name) {
    const char *path = getenv("MB_TEST_SERVICE_TRACE");
    assert(path);
    FILE *file = fopen(path, "a");
    assert(file);
    assert(fprintf(file, "%s\n", name) > 0);
    assert(!fclose(file));
}
static void release(void *owner, const char *bytes, size_t length) {
    (void)bytes; (void)length;
    assert(instance.open);
    event(owner ? "release_error" : "release_reply");
}
int mfs_open(const mfs_open_params *params, mfs_service **out, mfs_owned *error) {
    assert(params && params->abi_version == 1 && params->struct_size == sizeof(*params));
    assert(!params->on_event && !params->event_user);
    assert(params->config_json.length == sizeof("{\"fixture\":true}") - 1 &&
        !memcmp(params->config_json.bytes, "{\"fixture\":true}", params->config_json.length));
    assert(!instance.open);
    instance.open = 1;
    *out = &instance;
    event("open");
    const char *mode = getenv("MB_TEST_SERVICE_MODE");
    if (mode && !strcmp(mode, "open_fail")) {
        static const char reply[] = "{\"ok\":false,\"error\":{\"code\":\"fixture_failure\"}}";
        *error = (mfs_owned){reply, sizeof(reply) - 1, &instance, release};
        return -1;
    }
    return 0;
}
mfs_owned mfs_request(mfs_service *service, mfs_view request) {
    assert(service == &instance && instance.open);
    assert(request.length == sizeof("{\"method\":\"service.describe\",\"params\":{}}") - 1 &&
        !memcmp(request.bytes, "{\"method\":\"service.describe\",\"params\":{}}", request.length));
    event("describe");
    const char *mode = getenv("MB_TEST_SERVICE_MODE");
    const char *reply = mode && !strcmp(mode, "bad_contract") ?
        "{\"ok\":true,\"result\":{\"contract\":\"modforge-service/2\",\"methods\":[\"service.describe\",\"records.list\"]}}" :
        "{\"ok\":true,\"result\":{\"contract\":\"modforge-service/1\",\"methods\":[\"service.describe\",\"records.list\"]}}";
    return (mfs_owned){reply, strlen(reply), NULL, release};
}
void mfs_close(mfs_service *service) {
    assert(service == &instance && instance.open);
    instance.open = 0;
    event("close");
}
