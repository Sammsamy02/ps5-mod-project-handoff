#include "mod_service/owner.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
const char description[] = R"({"ok":true,"result":{"contract":"modforge-service/1","methods":["service.describe","records.list"]}})";
struct Fixture {
    mb_mod_service_owner owner{};
    std::string config = R"({"state_root":"/owned/test/state"})";
    std::string reply = description;
    unsigned opens = 0, requests = 0, closes = 0, releases = 0;
    bool fail_open = false, missing_service = false, release_on_success = false;
    bool omit_reply_release = false, omit_reply_bytes = false, oversized_reply = false;
    bool throw_open = false, throw_request = false;
} *fixture;
mfs_service *service() { return reinterpret_cast<mfs_service *>(fixture); }
void release(void *owner, const char *, size_t) {
    assert(owner == fixture);
    assert(fixture->closes == 0);
    ++fixture->releases;
}
int open_service(const mfs_open_params *params, mfs_service **output, mfs_owned *error) {
    ++fixture->opens;
    assert(!mb_mod_service_owner_bridge(&fixture->owner));
    assert(params && params->abi_version == 1 && params->struct_size == sizeof(*params));
    assert(!params->on_event && !params->event_user);
    assert(std::string(params->config_json.bytes, params->config_json.length) == fixture->config);
    *output = fixture->missing_service ? nullptr : service();
    if (fixture->fail_open || fixture->release_on_success || fixture->throw_open)
        *error = {"{\"error\":\"owned open result\"}", 29, fixture, release};
    if (fixture->throw_open) throw std::runtime_error("open failed after allocation");
    return fixture->fail_open ? -1 : 0;
}
mfs_owned request_service(mfs_service *value, mfs_view request) {
    ++fixture->requests;
    assert(value == service());
    assert(!mb_mod_service_owner_bridge(&fixture->owner));
    assert(std::string(request.bytes, request.length) == R"({"method":"service.describe","params":{}})");
    if (fixture->throw_request) throw std::runtime_error("request failed");
    return {fixture->omit_reply_bytes ? nullptr : fixture->reply.data(),
        fixture->oversized_reply ? MB_MOD_SERVICE_RESPONSE_LIMIT + 1 : fixture->reply.size(),
        fixture, fixture->omit_reply_release ? nullptr : release};
}
void close_service(mfs_service *value) {
    assert(value == service());
    assert(!mb_mod_service_owner_bridge(&fixture->owner));
    assert(!fixture->owner.bridge.service && !fixture->owner.bridge.request && !fixture->owner.close);
    ++fixture->closes;
    mb_mod_service_owner_close(&fixture->owner); // Close cannot acquire ownership a second time.
}
const mb_mod_service_operations operations{open_service, request_service, close_service};
auto start(Fixture &f) {
    fixture = &f;
    return mb_mod_service_owner_open(&f.owner, &operations, {f.config.data(), f.config.size()});
}
void empty(const Fixture &f) {
    assert(!mb_mod_service_owner_bridge(&f.owner));
    assert(!f.owner.bridge.service && !f.owner.bridge.request && !f.owner.close);
}
void succeeds_and_closes_once() {
    Fixture f;
    assert(start(f) == MB_MOD_SERVICE_OWNER_OK);
    const auto *borrowed = mb_mod_service_owner_bridge(&f.owner);
    assert(borrowed && borrowed->service == service() && borrowed->request == request_service);
    assert(f.opens == 1 && f.requests == 1 && f.releases == 1 && f.closes == 0);
    assert(mb_mod_service_owner_open(&f.owner, &operations, {"{}", 2}) == MB_MOD_SERVICE_OWNER_ALREADY_OPEN);
    assert(mb_mod_service_owner_bridge(&f.owner) == borrowed && f.opens == 1);
    mb_mod_service_owner_close(&f.owner);
    empty(f);
    assert(!borrowed->service && !borrowed->request && f.closes == 1);
    mb_mod_service_owner_close(&f.owner);
    assert(f.closes == 1);
}
void releases_open_results() {
    Fixture f;
    f.release_on_success = true;
    assert(start(f) == MB_MOD_SERVICE_OWNER_OK);
    assert(f.releases == 2);
    mb_mod_service_owner_close(&f.owner);
    for (bool missing : {false, true}) {
        Fixture failed;
        failed.fail_open = true;
        failed.missing_service = missing;
        assert(start(failed) == MB_MOD_SERVICE_OWNER_OPEN_FAILED);
        empty(failed);
        assert(failed.requests == 0 && failed.releases == 1 && failed.closes == (missing ? 0 : 1));
        mb_mod_service_owner_close(&failed.owner);
        assert(failed.closes == (missing ? 0 : 1));
    }
    Fixture absent;
    absent.missing_service = true;
    assert(start(absent) == MB_MOD_SERVICE_OWNER_OPEN_FAILED);
    empty(absent);
    assert(absent.requests == 0 && absent.closes == 0);
}
void rejects_descriptions_and_cleans_up() {
    struct Case { const char *reply; mb_mod_service_owner_status status; };
    const Case cases[] = {
        {"", MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION},
        {"null", MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION},
        {"not JSON", MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION},
        {R"({"ok":false,"error":{"code":"unavailable","message":"owned fixture"}})", MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION},
        {R"({"ok":true,"result":[]})", MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION},
        {R"({"ok":true,"ok":false,"result":{}})", MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION},
        {R"({"ok":true,"result":{"contract":"modforge-service/2","methods":["service.describe","records.list"]}})", MB_MOD_SERVICE_OWNER_UNSUPPORTED_CONTRACT},
        {R"({"ok":true,"result":{"methods":["service.describe","records.list"]}})", MB_MOD_SERVICE_OWNER_UNSUPPORTED_CONTRACT},
        {R"({"ok":true,"result":{"contract":"modforge-service/1","methods":["service.describe"]}})", MB_MOD_SERVICE_OWNER_UNSUPPORTED_CONTRACT},
        {R"({"ok":true,"result":{"contract":"modforge-service/1","methods":["records.list"]}})", MB_MOD_SERVICE_OWNER_UNSUPPORTED_CONTRACT},
        {R"({"ok":true,"result":{"contract":"modforge-service/1","methods":{}}})", MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION},
        {R"({"ok":true,"result":{"contract":"modforge-service/1","methods":["service.describe","records.list","records.list"]}})", MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION},
        {R"({"ok":true,"result":{"contract":"modforge-service/1","methods":["service.describe","records.list",7]}})", MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION},
        {R"({"ok":true,"result":{"contract":"modforge-service/1","methods":["service.describe","records.list",""]}})", MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION},
        {R"({"ok":true,"result":{"contract":"modforge-service/1","methods":["service.describe","records.list","bad\u0000method"]}})", MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION},
    };
    for (const auto &entry : cases) {
        Fixture f;
        f.reply = entry.reply;
        assert(start(f) == entry.status);
        empty(f);
        assert(f.releases == 1 && f.closes == 1);
        mb_mod_service_owner_close(&f.owner);
        assert(f.closes == 1);
    }
    for (int mode = 0; mode < 5; ++mode) {
        Fixture f;
        f.omit_reply_bytes = mode == 0;
        f.omit_reply_release = mode == 1;
        f.oversized_reply = mode == 2;
        if (mode == 3) f.reply.push_back('\0');
        if (mode == 4) f.reply = std::string(70, '[') + "{}" + std::string(70, ']');
        assert(start(f) == MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION);
        empty(f);
        assert(f.releases == (mode == 1 ? 0 : 1) && f.closes == 1);
    }
}
void additional_methods_do_not_prevent_read_attachment() {
    Fixture f;
    f.reply = R"({"ok":true,"result":{"contract":"modforge-service/1","methods":["service.describe","records.list","mod.register_download","mod.prepare"],"version":"fixture"}})";
    assert(start(f) == MB_MOD_SERVICE_OWNER_OK);
    assert(mb_mod_service_owner_bridge(&f.owner));
    mb_mod_service_owner_close(&f.owner);
}
void invalid_startup_input_never_opens() {
    for (const std::string &config : {std::string{}, std::string("[]"), std::string("no JSON"),
        std::string("{\"key\":1,\"key\":2}"), std::string("{}\0", 3),
        std::string(MB_MOD_SERVICE_CONFIG_LIMIT + 1, ' ')}) {
        Fixture f;
        f.config = config;
        assert(start(f) == MB_MOD_SERVICE_OWNER_INVALID_ARGUMENT);
        empty(f);
        assert(f.opens == 0 && f.closes == 0);
    }
    Fixture f;
    fixture = &f;
    assert(mb_mod_service_owner_open(nullptr, &operations, {"{}", 2}) == MB_MOD_SERVICE_OWNER_INVALID_ARGUMENT);
    assert(mb_mod_service_owner_open(&f.owner, nullptr, {"{}", 2}) == MB_MOD_SERVICE_OWNER_INVALID_ARGUMENT);
    assert(mb_mod_service_owner_open(&f.owner, &operations, {nullptr, 2}) == MB_MOD_SERVICE_OWNER_INVALID_ARGUMENT);
    for (unsigned missing = 0; missing < 3; ++missing) {
        auto incomplete = operations;
        if (missing == 0) incomplete.open = nullptr;
        if (missing == 1) incomplete.request = nullptr;
        if (missing == 2) incomplete.close = nullptr;
        assert(mb_mod_service_owner_open(&f.owner, &incomplete, {"{}", 2}) == MB_MOD_SERVICE_OWNER_INVALID_ARGUMENT);
    }
    assert(f.opens == 0 && f.closes == 0);
    mb_mod_service_owner_close(nullptr);
    assert(!mb_mod_service_owner_bridge(nullptr));
}
void exceptions_cannot_leak_an_open_service() {
    for (bool opening : {false, true}) {
        Fixture f;
        f.throw_open = opening;
        f.throw_request = !opening;
        assert(start(f) == MB_MOD_SERVICE_OWNER_RESOURCE_FAILED);
        empty(f);
        assert(f.releases == (opening ? 1 : 0) && f.closes == 1);
    }
}
} // namespace
int main() {
    succeeds_and_closes_once();
    releases_open_results();
    rejects_descriptions_and_cleans_up();
    additional_methods_do_not_prevent_read_attachment();
    invalid_startup_input_never_opens();
    exceptions_cannot_leak_an_open_service();
    std::cout << "mod service owner lifecycle checks passed\n";
}
