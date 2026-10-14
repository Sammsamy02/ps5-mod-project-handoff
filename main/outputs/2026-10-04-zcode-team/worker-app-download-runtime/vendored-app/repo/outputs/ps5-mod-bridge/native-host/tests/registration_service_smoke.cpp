/* Owned-temp-dir integration against Claude's actual C ABI service shell.
 * The external backend source is pinned by the accompanying runner. */
#include "../mod_service/register_bridge.h"
#include <nlohmann/json.hpp>
#include <picosha2/picosha2.h>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
using Json = nlohmann::json;

static std::atomic<unsigned> changes{0};
static void on_event(void *, mfs_view event) {
    auto j = Json::parse(event.bytes, event.bytes + event.length);
    if (j.value("event", "") == "records.changed") ++changes;
}
static Json call(mfs_service *service, const char *method) {
    std::string request = Json{{"method", method}, {"params", Json::object()}}.dump();
    mfs_owned r = mfs_request(service, {request.data(), request.size()});
    assert(r.bytes && r.release);
    auto value = Json::parse(r.bytes, r.bytes + r.length);
    mfs_release(&r);
    assert(value.at("ok") == true);
    return value.at("result");
}
static mfs_service *open_service(const std::string &root) {
    std::string config = Json{{"state_dir", root + "/backend"},
                             {"mod_root_base", root + "/mod-roots"}}.dump();
    mfs_open_params params{MFS_ABI_VERSION, sizeof(mfs_open_params),
                          {config.data(), config.size()}, on_event, nullptr};
    mfs_service *service = nullptr;
    mfs_owned error{};
    assert(mfs_open(&params, &service, &error) == 0 && service);
    mfs_release(&error);
    return service;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    std::string root = argv[1], downloads = root + "/downloads";
    assert(mkdir(downloads.c_str(), 0700) == 0);
    int directory = open(downloads.c_str(), O_RDONLY | O_DIRECTORY);
    assert(directory >= 0);
    mb_download_store *store = nullptr;
    assert(mb_download_store_open(directory, downloads.c_str(), &store) == MB_DOWNLOAD_STORE_OK);
    std::string bytes = "owned original archive fixture; no game content";
    std::string sha = picosha2::hash256_hex_string(bytes);
    std::string source_path = root + "/original";
    int source = open(source_path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
    assert(source >= 0 && write(source, bytes.data(), bytes.size()) == static_cast<ssize_t>(bytes.size()));
    assert(close(source) == 0);
    source = open(source_path.c_str(), O_RDONLY);
    char retained[4096]{};
    assert(mb_download_store_publish(store, source, "fixturegame", 123, 456,
                                    bytes.size(), sha.c_str(), retained, sizeof retained) == MB_DOWNLOAD_STORE_OK);
    close(source);
    assert(unlink(source_path.c_str()) == 0);
    std::string id = Json::parse(retained).at("download_id");
    mfs_service *service = open_service(root);
    mb_mod_service_bridge bridge{service, mfs_request};
    auto register_once = [&](const char *title, int expected) {
        char *output = nullptr;
        size_t size = 0;
        int result = mb_mod_service_register_download(&bridge, store, id.data(), id.size(),
            title, std::strlen(title), nullptr, 0, &output, &size);
        assert(result == expected);
        if (expected != 200) { assert(!output && size == 0); return Json(); }
        assert(output && size && std::strstr(output, "archive_path") == nullptr &&
               std::strstr(output, root.c_str()) == nullptr);
        Json value = Json::parse(output, output + size);
        free(output);
        return value;
    };
    auto first = register_once("PPSA00001", 200);
    assert(first.at("registration") == "registered" && first.at("requirements_status") == "unknown");
    assert(first.at("state") == "downloaded");
    assert(changes == 1);
    auto repeat = register_once("PPSA00001", 200);
    assert(repeat.at("registration") == "existing" && changes == 1);
    register_once("PPSA00002", 409);
    assert(changes == 1);
    auto records = call(service, "records.list");
    assert(records.at("mods").size() == 1 && records["mods"][0]["title_id"] == "PPSA00001");
    mfs_close(service);
    service = open_service(root);
    bridge.service = service;
    auto restored = register_once("PPSA00001", 200);
    assert(restored.at("registration") == "existing" && changes == 1);
    mfs_close(service);
    // A second retained file carries the complete native acquisition capture.
    // Reopen both owners before registering, proving no in-memory handoff or
    // second provider read is needed to deliver prerequisites to the service.
    source = open((downloads + "/" + id + ".archive").c_str(), O_RDONLY);
    assert(source >= 0);
    assert(mb_download_store_publish(store, source, "fixturegame", 123, 789,
        bytes.size(), sha.c_str(), retained, sizeof retained) == MB_DOWNLOAD_STORE_OK);
    close(source);
    id = Json::parse(retained).at("download_id");
    Json requirements = {{"game", "fixturegame"}, {"mod_id", 123}, {"discovery_complete", true},
        {"nexus_requirements", Json::array()}, {"dlc_requirements", Json::array()},
        {"file_version_constraints_provided", false}, {"installation_supported", false}};
    const std::string captured = Json{{"source", {{"name", "Captured fixture"}, {"version", "2"}}},
        {"requirements", requirements}}.dump();
    assert(mb_download_store_publish_metadata(store, id.c_str(), captured.data(), captured.size()) == MB_DOWNLOAD_STORE_OK);
    mb_download_store_free(store);
    store = nullptr;
    assert(mb_download_store_open(directory, downloads.c_str(), &store) == MB_DOWNLOAD_STORE_OK);
    service = open_service(root);
    bridge.service = service;
    auto captured_result = register_once("PPSA00001", 200);
    assert(captured_result.at("requirements_status") == "complete");
    assert(captured_result.at("source").at("name") == "Captured fixture");
    assert(call(service, "records.list").at("mods").size() == 2);
    mfs_close(service);
    mb_download_store_free(store);
    close(directory);
    return 0;
}
