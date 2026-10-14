#include "mod_service/read_bridge.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
static unsigned releases, calls;
static std::string reply, expected;
static void release(void *, const char *, size_t) { ++releases; }
static mfs_owned request(mfs_service *, mfs_view input) {
    ++calls; assert(std::string(input.bytes, input.length) == expected);
    return {reply.data(), reply.size(), nullptr, release};
}
int main() {
    mb_mod_service_bridge bridge{reinterpret_cast<mfs_service *>(1), request};
    char *output; size_t length;
    assert(!mb_mod_service_usable(nullptr));
    assert(mb_mod_service_read(nullptr, MB_MOD_SERVICE_DESCRIBE, &output, &length) == 503);
    assert(!output && !length && !calls);
    expected = "{\"method\":\"service.describe\",\"params\":{}}";
    reply = "{\"ok\":true,\"result\":{\"contract\":\"modforge-service/1\"}}";
    assert(mb_mod_service_read(&bridge, MB_MOD_SERVICE_DESCRIBE, &output, &length) == 200);
    assert(length == reply.size() && std::string(output, length) == reply && releases == 1);
    free(output);
    expected = "{\"method\":\"records.list\",\"params\":{}}";
    reply = "{\"ok\":false,\"error\":{\"code\":\"unavailable\",\"message\":\"offline\"}}";
    assert(mb_mod_service_read(&bridge, MB_MOD_SERVICE_RECORDS, &output, &length) == 200);
    assert(std::string(output, length) == reply && releases == 2); free(output);
    reply.assign(MB_MOD_SERVICE_RESPONSE_LIMIT + 1, ' ');
    assert(mb_mod_service_read(&bridge, MB_MOD_SERVICE_RECORDS, &output, &length) == 502);
    assert(!output && !length && releases == 3);
    reply = "{\"ok\":true}";
    assert(mb_mod_service_read(&bridge, MB_MOD_SERVICE_RECORDS, &output, &length) == 502);
    assert(!output && !length && releases == 4);
    reply = "not json";
    assert(mb_mod_service_read(&bridge, MB_MOD_SERVICE_RECORDS, &output, &length) == 502);
    assert(releases == 5);
    assert(mb_mod_service_read(&bridge, static_cast<enum mb_mod_service_read>(99), &output, &length) == 502);
    assert(calls == 5 && releases == 5);
}
