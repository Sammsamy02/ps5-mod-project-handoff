#include "mod_service/register_bridge.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <picosha2/picosha2.h>
#include <string>
#include <unistd.h>
using Json = nlohmann::json;
struct Fake {
  Json describe = {{"contract", MFS_CONTRACT}, {"methods", {"mod.register_download", "records.list"}}};
  Json rows = Json::array(), submitted, registration_patch = Json::object();
  std::string override_method, override_reply, reply_identity, corrupt_path;
  unsigned calls = 0, releases = 0, mutations = 0;
};
struct Owned { Fake *fake; std::string text; };
static void release(void *owner, const char *bytes, size_t size) {
  auto *value = static_cast<Owned *>(owner);
  assert(value->text.data() == bytes && value->text.size() == size);
  ++value->fake->releases;
  delete value;
}
static mfs_owned request(mfs_service *service, mfs_view view) {
  auto &fake = *reinterpret_cast<Fake *>(service);
  ++fake.calls;
  Json input = Json::parse(view.bytes, view.bytes + view.length), result;
  const auto method = input.at("method").get<std::string>();
  assert(input.size() == 2 && input.at("params").is_object());
  if (method == "service.describe") result = fake.describe;
  else if (method == "records.list") {
    result = {{"mods", fake.rows}, {"games", Json::array()}};
    if (!fake.corrupt_path.empty()) {
      int fd = open(fake.corrupt_path.c_str(), O_WRONLY);
      assert(fd >= 0 && pwrite(fd, "changed", 7, 0) == 7);
      close(fd);
    }
  } else {
    assert(method == "mod.register_download");
    ++fake.mutations;
    fake.submitted = input.at("params");
    result = fake.submitted;
    result["state"] = "downloaded";
    result["private_echo"] = "must never escape";
    if (!fake.reply_identity.empty()) result["title_id"] = fake.reply_identity;
    result.update(fake.registration_patch);
  }
  auto *owned = new Owned{&fake, method == fake.override_method ? fake.override_reply :
      Json{{"ok", true}, {"result", result}}.dump()};
  return {owned->text.data(), owned->text.size(), owned, release};
}
static Json requirements() {
  return {{"game", "testgame"}, {"mod_id", 1}, {"discovery_complete", true},
      {"nexus_requirements", Json::array()}, {"dlc_requirements", Json::array()},
      {"file_version_constraints_provided", false}, {"installation_supported", false}};
}
int main() {
  char temporary[] = "/tmp/mb-register-test-XXXXXX";
  assert(mkdtemp(temporary));
  char canonical[4096]; assert(realpath(temporary, canonical));
  std::string root = canonical, original = root + "/original";
  int directory = open(root.c_str(), O_RDONLY | O_DIRECTORY);
  int fd = open(original.c_str(), O_CREAT | O_RDWR, 0600);
  assert(directory >= 0 && fd >= 0 && write(fd, "archive", 7) == 7);
  close(fd); fd = open(original.c_str(), O_RDONLY); assert(fd >= 0);
  mb_download_store *store = nullptr;
  assert(!mb_download_store_open(directory, root.c_str(), &store));
  char retained[4096];
  const auto hash = picosha2::hash256_hex_string(std::string("archive"));
  assert(!mb_download_store_publish(store, fd, "testgame", 1, 2, 7, hash.c_str(), retained, sizeof(retained)));
  const std::string id = Json::parse(retained).at("download_id");
  const std::string archive = root + "/" + id + ".archive";
  close(fd); unlink(original.c_str());
  Fake fake;
  mb_mod_service_bridge bridge{reinterpret_cast<mfs_service *>(&fake), request};
  Json receipt;
  auto call = [&](const std::string &metadata = std::string{}, const std::string &title = "PPSA12345",
                  const std::string &download = std::string{}) {
    const auto &selected = download.empty() ? id : download;
    char *output = reinterpret_cast<char *>(1); size_t bytes = 99;
    int status = mb_mod_service_register_download(&bridge, store, selected.data(), selected.size(),
        title.data(), title.size(), metadata.empty() ? nullptr : metadata.data(), metadata.size(), &output, &bytes);
    assert(fake.calls == fake.releases);
    if (status == 200) {
      assert(output && bytes && output[bytes] == 0);
      std::string text(output, bytes);
      assert(text.find(root) == std::string::npos && text.find("archive_path") == std::string::npos &&
          text.find("private_echo") == std::string::npos && text.find("nexus_requirements") == std::string::npos);
      receipt = Json::parse(text); free(output);
    } else assert(!output && !bytes);
    return status;
  };
  auto reset = [&] { fake = Fake{}; };
  assert(call() == 200);
  assert(fake.calls == 3 && fake.mutations == 1);
  assert(fake.submitted.at("archive_path") == archive && fake.submitted.at("requirements").is_null());
  assert(fake.submitted.at("source") == Json({{"provider", "nexus"}, {"domain", "testgame"}, {"mod_id", 1}, {"file_id", 2}}));
  assert(receipt.at("registration") == "registered" && receipt.at("requirements_status") == "unknown");
  Json existing = fake.submitted; existing["state"] = "prepared";
  Json metadata = {{"source", {{"name", "Display"}, {"author", "Author"}, {"version", "2.3"}}},
      {"requirements", requirements()}};
  reset(); assert(call(metadata.dump(), "CUSA12345") == 200);
  assert(fake.submitted.at("requirements") == requirements());
  assert(receipt.at("source").at("name") == "Display" && receipt.at("requirements_status") == "complete");
  reset(); fake.rows.push_back(existing);
  assert(call(metadata.dump()) == 200 && fake.mutations == 0 && fake.calls == 2);
  assert(receipt.at("state") == "prepared" && receipt.at("registration") == "existing" &&
      receipt.at("requirements_status") == "unknown");
  reset(); existing["title_id"] = "CUSA12345"; fake.rows.push_back(existing);
  assert(call() == 409 && fake.mutations == 0);
  existing["title_id"] = "PPSA12345";
  reset(); auto conflicting = existing; conflicting["source"]["file_id"] = 77; fake.rows.push_back(conflicting);
  assert(call() == 409 && fake.mutations == 0);
  reset(); fake.rows = {existing, existing};
  assert(call() == 502 && fake.mutations == 0);
  reset(); fake.rows = {Json{{"download_id", id}}};
  assert(call() == 502 && fake.mutations == 0);
  reset(); fake.rows = {Json{{"download_id", "other"}}};
  assert(call() == 502 && fake.mutations == 0);
  reset(); fake.describe["contract"] = "other";
  assert(call() == 502 && fake.calls == 1 && fake.mutations == 0);
  reset(); fake.describe["methods"] = {"records.list"};
  assert(call() == 502 && fake.calls == 1 && fake.mutations == 0);
  reset(); fake.describe["methods"] = {"mod.register_download", 3};
  assert(call() == 502 && fake.mutations == 0);
  for (const std::string method : {"service.describe", "records.list", "mod.register_download"}) {
    reset(); fake.override_method = method; fake.override_reply = "{\"ok\":false,\"error\":{\"message\":\"/private/secret\"}}";
    assert(call() == 502);
    reset(); fake.override_method = method; fake.override_reply = "{\"ok\":true,\"ok\":false,\"result\":{}}";
    assert(call() == 502);
    reset(); fake.override_method = method; fake.override_reply.assign(MB_MOD_SERVICE_RESPONSE_LIMIT + 1, ' ');
    assert(call() == 502);
    reset(); fake.override_method = method; fake.override_reply.clear();
    assert(call() == 502);
  }
  reset(); fake.override_method = "records.list";
  fake.override_reply = Json{{"ok", true}, {"result", {{"mods", Json::array()}}}}.dump() + std::string(1, '\0') + "ignored";
  assert(call() == 502 && !fake.mutations);
  reset(); fake.reply_identity = "PPSA54321";
  assert(call() == 502 && fake.mutations == 1);
  for (const auto &patch : {Json{{"download_id", "unexpected"}}, Json{{"archive_path", "/private/other"}},
      Json{{"state", "installed"}}, Json{{"source", {{"provider", "nexus"}, {"domain", "othergame"}, {"mod_id", 1}, {"file_id", 2}}}}}) {
    reset(); fake.registration_patch = patch;
    assert(call() == 502 && fake.mutations == 1);
  }
  reset(); fake.registration_patch = {{"requirements", nullptr}};
  assert(call(metadata.dump()) == 502 && fake.mutations == 1);
  reset(); assert(call({}, "../../PPSA12345") == 400 && !fake.calls);
  assert(call({}, "PPSA") == 400 && !fake.calls);
  assert(call({}, "PPSA1234x") == 400 && !fake.calls);
  assert(call({}, "PPSA12345", "dl-../bad") == 400 && !fake.calls);
  assert(call({}, "PPSA12345", "dl-" + std::string(64, '0')) == 404 && fake.calls == 2 && !fake.mutations);
  for (const char *field : {"game", "mod_id", "discovery_complete", "nexus_requirements", "dlc_requirements",
                           "file_version_constraints_provided", "installation_supported"}) {
    reset(); auto invalid = metadata; invalid["requirements"].erase(field);
    assert(call(invalid.dump()) == 400 && !fake.calls);
  }
  reset(); auto invalid = metadata; invalid["requirements"]["game"] = "anothergame";
  assert(call(invalid.dump()) == 400 && fake.calls == 2 && !fake.mutations);
  reset();
  invalid = metadata; invalid["requirements"]["mod_id"] = 2;
  assert(call(invalid.dump()) == 400 && fake.calls == 2 && !fake.mutations);
  reset();
  invalid = metadata; invalid["requirements"]["discovery_complete"] = false;
  assert(call(invalid.dump()) == 400 && !fake.calls);
  invalid = metadata; invalid["source"]["archive_path"] = "/private/secret";
  assert(call(invalid.dump()) == 400 && !fake.calls);
  assert(call("{\"source\":{},\"source\":{},\"requirements\":null}") == 400 && !fake.calls);
  assert(call(std::string(MB_MOD_SERVICE_REGISTRATION_METADATA_LIMIT + 1, ' ')) == 400 && !fake.calls);
  Json row = {{"id", "r-1"}, {"external_requirement", false}, {"game", "testgame"}, {"mod_id", "7"},
      {"name", "Required mod"}, {"notes", "private requirement notes"}, {"url", "https://example.invalid/required"},
      {"satisfaction", "unresolved"}};
  reset(); auto complete = metadata; complete["requirements"]["nexus_requirements"].push_back(row);
  assert(call(complete.dump()) == 200 && fake.submitted["requirements"]["nexus_requirements"][0] == row);
  reset(); complete["requirements"]["nexus_requirements"].push_back(row);
  assert(call(complete.dump()) == 400 && !fake.calls);
  reset(); complete = metadata; row["satisfaction"] = "satisfied";
  complete["requirements"]["nexus_requirements"].push_back(row);
  assert(call(complete.dump()) == 400 && !fake.calls);
  // Native acquisition captures survive restart and reach registration without
  // a browser or caller reconstructing the prerequisite object.
  const auto captured = metadata.dump();
  assert(mb_download_store_publish_metadata(store, id.c_str(), captured.data(), captured.size()) == MB_DOWNLOAD_STORE_OK);
  mb_download_store_free(store); store = nullptr;
  assert(mb_download_store_open(directory, root.c_str(), &store) == MB_DOWNLOAD_STORE_OK);
  reset(); assert(call() == 200 && fake.mutations == 1);
  assert(fake.submitted.at("requirements") == requirements());
  assert(receipt.at("requirements_status") == "complete" && receipt.at("source").at("name") == "Display");
  // Replaying an existing record still does not infer stored requirements.
  existing = fake.submitted; existing["state"] = "downloaded";
  reset(); fake.rows.push_back(existing);
  assert(call() == 200 && fake.mutations == 0 && receipt.at("requirements_status") == "unknown");
  // Hash the real bytes after read-only backend calls, before registration.
  reset(); fake.corrupt_path = archive;
  assert(call() == 409 && fake.calls == 2 && !fake.mutations);
  reset(); assert(call() == 409 && fake.calls == 2 && !fake.mutations);
  mb_download_store_free(store); close(directory);
  std::filesystem::remove_all(root);
  std::cout << "registration privacy, integrity, ownership and replay controls passed\n";
}
