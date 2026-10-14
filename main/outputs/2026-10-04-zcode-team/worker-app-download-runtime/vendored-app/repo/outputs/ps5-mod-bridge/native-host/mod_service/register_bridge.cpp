#include "register_bridge.h"
#include <climits>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Json = nlohmann::json;
constexpr size_t maximum_records = 256;

bool span(const void *data, size_t bytes) {
  return (!bytes || data) && (!data || reinterpret_cast<uintptr_t>(data) <=
      std::numeric_limits<uintptr_t>::max() - bytes);
}
bool ascii_digits(std::string_view value) {
  if (value.empty()) return false;
  for (char byte : value) if (byte < '0' || byte > '9') return false;
  return true;
}
bool title_valid(std::string_view value) {
  return value.size() >= 5 && value.size() <= 20 &&
      (value.substr(0, 4) == "PPSA" || value.substr(0, 4) == "CUSA") &&
      ascii_digits(value.substr(4));
}
bool download_valid(std::string_view value) {
  if (value.size() != 67 || value.substr(0, 3) != "dl-") return false;
  for (char byte : value.substr(3))
    if (!((byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f'))) return false;
  return true;
}
bool domain_valid(std::string_view value) {
  const auto alnum = [](char byte) {
    return (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9');
  };
  if (value.empty() || value.size() > 80 || !alnum(value.front())) return false;
  for (char byte : value) if (!alnum(byte) && byte != '_' && byte != '-') return false;
  return true;
}
bool text_valid(const Json &value, size_t maximum, bool nonempty = false) {
  if (!value.is_string()) return false;
  const auto &text = value.get_ref<const std::string &>();
  size_t characters = 0;
  for (unsigned char byte : text) {
    if (byte < 32 && byte != '\n' && byte != '\r' && byte != '\t') return false;
    if ((byte & 0xc0) != 0x80) ++characters;
  }
  return characters <= maximum && (!nonempty || characters != 0);
}
bool identifier(const Json &value, uint64_t &number, uint64_t maximum = INT64_MAX) {
  if (value.is_number_unsigned()) number = value.get<uint64_t>();
  else if (value.type() == Json::value_t::number_integer && value.get<int64_t>() > 0)
    number = static_cast<uint64_t>(value.get<int64_t>());
  else return false;
  return number > 0 && number <= maximum;
}
bool canonical_identifier(const std::string &value) {
  if (value.empty() || value.size() > 19 || value.front() == '0' || !ascii_digits(value)) return false;
  uint64_t result = 0;
  for (char byte : value) {
    unsigned digit = static_cast<unsigned>(byte - '0');
    if (result > (static_cast<uint64_t>(INT64_MAX) - digit) / 10) return false;
    result = result * 10 + digit;
  }
  return result != 0;
}
bool exact_keys(const Json &value, std::initializer_list<const char *> keys) {
  if (!value.is_object() || value.size() != keys.size()) return false;
  for (const char *key : keys) if (!value.contains(key)) return false;
  return true;
}
bool boolean_is(const Json &value, bool expected) {
  return value.is_boolean() && value.get<bool>() == expected;
}

/* Duplicate object members cannot acquire last-wins authority. Bound parser
 * events/depth before constructing a result, including ignored reply fields. */
Json parse(const char *data, size_t bytes) {
  if (std::memchr(data, 0, bytes)) throw std::invalid_argument("embedded JSON terminator");
  size_t nodes = 0;
  std::vector<std::set<std::string>> object_keys;
  auto gate = [&](int depth, Json::parse_event_t event, Json &value) {
    if (depth > 64 || ++nodes > 200000) throw std::length_error("JSON limit");
    if (event == Json::parse_event_t::object_start) object_keys.emplace_back();
    else if (event == Json::parse_event_t::object_end) object_keys.pop_back();
    else if (event == Json::parse_event_t::key &&
        !object_keys.back().insert(value.get<std::string>()).second)
      throw std::invalid_argument("duplicate JSON key");
    return true;
  };
  return Json::parse(data, data + bytes, gate);
}
bool display_fields(const Json &source, Json &projection, bool exact) {
  if (!source.is_object()) return false;
  if (exact) for (auto item = source.begin(); item != source.end(); ++item)
    if (item.key() != "name" && item.key() != "author" && item.key() != "version") return false;
  for (const char *key : {"name", "author", "version"}) {
    auto item = source.find(key);
    if (item != source.end()) {
      if (!text_valid(*item, std::strcmp(key, "version") ? 1024 : 256)) return false;
      projection[key] = *item;
    }
  }
  return true;
}
bool normalized_requirements(const Json &value, const Json &source) {
  if (!exact_keys(value, {"game", "mod_id", "discovery_complete", "nexus_requirements",
      "dlc_requirements", "file_version_constraints_provided", "installation_supported"})) return false;
  uint64_t mod = 0;
  if (!value["game"].is_string() || value["game"] != source["domain"] ||
      !identifier(value["mod_id"], mod, INT32_MAX) || value["mod_id"] != source["mod_id"] ||
      !boolean_is(value["discovery_complete"], true) ||
      !boolean_is(value["file_version_constraints_provided"], false) ||
      !boolean_is(value["installation_supported"], false) ||
      !value["nexus_requirements"].is_array() || value["nexus_requirements"].size() > 1024 ||
      !value["dlc_requirements"].is_array() || value["dlc_requirements"].size() > 1024) return false;
  std::set<std::string> relation_ids;
  for (const auto &row : value["nexus_requirements"]) {
    if (!exact_keys(row, {"id", "external_requirement", "game", "mod_id", "name", "notes", "url", "satisfaction"}) ||
        !text_valid(row["id"], 256, true) || !row["external_requirement"].is_boolean() ||
        !text_valid(row["game"], 80) || !text_valid(row["mod_id"], 128) ||
        !text_valid(row["name"], 1024) || !text_valid(row["notes"], 65536) ||
        !text_valid(row["url"], 8192) || row["satisfaction"] != "unresolved") return false;
    if (!row["external_requirement"].get<bool>() &&
        (!domain_valid(row["game"].get_ref<const std::string &>()) ||
         !canonical_identifier(row["mod_id"].get_ref<const std::string &>()))) return false;
    if (!relation_ids.insert(row["id"].get<std::string>()).second) return false;
  }
  for (const auto &row : value["dlc_requirements"]) {
    if (!exact_keys(row, {"game", "id", "name", "notes", "satisfaction"}) ||
        !text_valid(row["game"], 80, true) || !domain_valid(row["game"].get_ref<const std::string &>()) ||
        !text_valid(row["id"], 256, true) || !text_valid(row["name"], 1024) ||
        !text_valid(row["notes"], 65536) || row["satisfaction"] != "unresolved") return false;
  }
  return true;
}

bool registration_metadata(const char *metadata, size_t bytes, Json &display, Json &requirements) {
  Json envelope;
  try { envelope = parse(metadata, bytes); }
  catch (const std::bad_alloc &) { throw; }
  catch (...) { return false; }
  if (!exact_keys(envelope, {"source", "requirements"}) ||
      !display_fields(envelope["source"], display, true) ||
      !envelope["requirements"].is_object() || !envelope["requirements"].contains("game") ||
      !envelope["requirements"].contains("mod_id")) return false;
  const Json declared_source = {{"domain", envelope["requirements"]["game"]},
      {"mod_id", envelope["requirements"]["mod_id"]}};
  if (!normalized_requirements(envelope["requirements"], declared_source)) return false;
  requirements = envelope["requirements"];
  return true;
}

struct BackendReply {
  mfs_owned value;
  ~BackendReply() { try { mfs_release(&value); } catch (...) { } }
};
bool backend(const mb_mod_service_bridge &bridge, const char *method, const Json &params, Json &result) {
  const std::string request = Json{{"method", method}, {"params", params}}.dump();
  if (request.size() > MB_MOD_SERVICE_REGISTRATION_REQUEST_LIMIT) return false;
  BackendReply reply{bridge.request(bridge.service, {request.data(), request.size()})};
  if (!reply.value.bytes || !reply.value.release || !reply.value.length ||
      reply.value.length > MB_MOD_SERVICE_RESPONSE_LIMIT || !span(reply.value.bytes, reply.value.length)) return false;
  const auto decoded = parse(reply.value.bytes, reply.value.length);
  if (!decoded.is_object() || !decoded.contains("ok") || !boolean_is(decoded["ok"], true) ||
      !decoded.contains("result") || !decoded["result"].is_object()) return false;
  result = decoded["result"];
  return true;
}
bool supported(const Json &description) {
  if (!description.contains("contract") || description["contract"] != MFS_CONTRACT ||
      !description.contains("methods") || !description["methods"].is_array() ||
      description["methods"].size() > 128) return false;
  std::set<std::string> methods;
  for (const auto &method : description["methods"]) {
    if (!text_valid(method, 128, true) || !methods.insert(method.get<std::string>()).second) return false;
  }
  return methods.contains("mod.register_download") && methods.contains("records.list");
}
bool source_identity(const Json &source, Json &projection) {
  uint64_t mod = 0, file = 0;
  if (!source.is_object() || !source.contains("provider") || !text_valid(source["provider"], 64, true) ||
      !source.contains("domain") || !source["domain"].is_string() ||
      !domain_valid(source["domain"].get_ref<const std::string &>()) ||
      !source.contains("mod_id") || !identifier(source["mod_id"], mod) ||
      !source.contains("file_id") || !identifier(source["file_id"], file)) return false;
  projection = {{"provider", source["provider"]}, {"domain", source["domain"]}, {"mod_id", mod}, {"file_id", file}};
  return display_fields(source, projection, false);
}
bool same_identity(const Json &left, const Json &right) {
  for (const char *key : {"provider", "domain", "mod_id", "file_id"})
    if (left.at(key) != right.at(key)) return false;
  return true;
}
bool record(const Json &value, Json &public_source) {
  if (!value.is_object() || !value.contains("download_id") || !text_valid(value["download_id"], 128, true) ||
      !value.contains("title_id") || !value["title_id"].is_string() ||
      !title_valid(value["title_id"].get_ref<const std::string &>()) || !value.contains("state") ||
      (value["state"] != "downloaded" && value["state"] != "prepared" && value["state"] != "installed") ||
      !value.contains("source")) return false;
  return source_identity(value["source"], public_source);
}
int lookup(mb_download_store *store, const std::string &id, Json &result) {
  char output[8192]{};
  const int status = mb_download_store_lookup(store, id.c_str(), output, sizeof(output));
  if (status == MB_DOWNLOAD_STORE_NOT_FOUND) return 404;
  if (status == MB_DOWNLOAD_STORE_INTEGRITY_FAILED) return 409;
  if (status != MB_DOWNLOAD_STORE_OK) return 503;
  result = parse(output, std::strlen(output));
  uint64_t mod = 0, file = 0;
  if (!result.is_object() || result.value("download_id", std::string{}) != id ||
      !result.contains("game") || !result["game"].is_string() ||
      !domain_valid(result["game"].get_ref<const std::string &>()) ||
      !result.contains("mod_id") || !identifier(result["mod_id"], mod) ||
      !result.contains("file_id") || !identifier(result["file_id"], file) ||
      !result.contains("archive_path") || !text_valid(result["archive_path"], 4096, true) ||
      result["archive_path"].get_ref<const std::string &>().front() != '/') return 409;
  return 200;
}
int publish(const Json &receipt, char **output, size_t *bytes) {
  const std::string text = receipt.dump();
  auto *copy = static_cast<char *>(std::malloc(text.size() + 1));
  if (!copy) return 503;
  std::memcpy(copy, text.c_str(), text.size() + 1);
  *output = copy; *bytes = text.size();
  return 200;
}
Json receipt(const std::string &id, const std::string &title, const Json &source,
             const Json &state, bool existing, bool complete) {
  return {{"download_id", id}, {"title_id", title}, {"state", state}, {"source", source},
      {"registration", existing ? "existing" : "registered"},
      {"requirements_status", complete ? "complete" : "unknown"}};
}
} // namespace

extern "C" int mb_mod_service_register_download(const mb_mod_service_bridge *bridge,
    mb_download_store *store, const char *download_id, size_t download_bytes,
    const char *title_id, size_t title_bytes, const char *metadata, size_t metadata_bytes,
    char **output, size_t *output_bytes) {
  if (!output || !output_bytes) return 400;
  *output = nullptr; *output_bytes = 0;
  if (!span(download_id, download_bytes) || !span(title_id, title_bytes) ||
      !span(metadata, metadata_bytes) || download_bytes != 67 || title_bytes < 5 || title_bytes > 20 ||
      metadata_bytes > MB_MOD_SERVICE_REGISTRATION_METADATA_LIMIT ||
      ((metadata == nullptr) != (metadata_bytes == 0))) return 400;
  if (!download_valid(std::string_view(download_id, download_bytes)) ||
      !title_valid(std::string_view(title_id, title_bytes))) return 400;
  if (!mb_mod_service_usable(bridge) || !store) return 503;
  try {
    const std::string id(download_id, download_bytes), title(title_id, title_bytes);
    Json display = Json::object(), requirements = nullptr;
    if (metadata && !registration_metadata(metadata, metadata_bytes, display, requirements)) return 400;
    Json description, records;
    if (!backend(*bridge, "service.describe", Json::object(), description) || !supported(description) ||
        !backend(*bridge, "records.list", Json::object(), records) || !records.contains("mods") ||
        !records["mods"].is_array() || records["mods"].size() > maximum_records) return 502;
    Json retained;
    const int status = lookup(store, id, retained);
    if (status != 200) return status;
    if (!metadata) {
      std::vector<char> capture(MB_MOD_SERVICE_REGISTRATION_METADATA_LIMIT + 1);
      const auto found = mb_download_store_lookup_metadata(store, id.c_str(), capture.data(), capture.size());
      if (found == MB_DOWNLOAD_STORE_OK) {
        if (!registration_metadata(capture.data(), std::strlen(capture.data()), display, requirements)) return 400;
      } else if (found != MB_DOWNLOAD_STORE_NOT_FOUND) {
        return found == MB_DOWNLOAD_STORE_INTEGRITY_FAILED ? 409 : 503;
      }
    }
    Json source = {{"provider", "nexus"}, {"domain", retained["game"]},
        {"mod_id", retained["mod_id"]}, {"file_id", retained["file_id"]}};
    source.update(display);
    if (!requirements.is_null() &&
        (requirements["game"] != source["domain"] || requirements["mod_id"] != source["mod_id"])) return 400;
    Json existing, existing_source;
    std::set<std::string> ids;
    for (const auto &row : records["mods"]) {
      Json projected_source;
      if (!record(row, projected_source)) return 502;
      const auto row_id = row["download_id"].get<std::string>();
      if (!ids.insert(row_id).second) return 502;
      if (row_id == id) { existing = row; existing_source = std::move(projected_source); }
    }
    if (!existing.is_null()) {
      if (existing["title_id"] != title || !same_identity(existing_source, source)) return 409;
      return publish(receipt(id, title, existing_source, existing["state"], true, false), output, output_bytes);
    }
    Json registered;
    const Json params = {{"download_id", id}, {"title_id", title},
        {"archive_path", retained["archive_path"]}, {"source", source}, {"requirements", requirements}};
    if (!backend(*bridge, "mod.register_download", params, registered)) return 502;
    Json registered_source;
    if (!record(registered, registered_source) || registered["download_id"] != id ||
        registered["title_id"] != title || registered["state"] != "downloaded" ||
        !same_identity(registered_source, source) || !registered.contains("archive_path") ||
        registered["archive_path"] != retained["archive_path"] ||
        (!requirements.is_null() && (!registered.contains("requirements") || registered["requirements"] != requirements))) return 502;
    return publish(receipt(id, title, source, "downloaded", false, !requirements.is_null()), output, output_bytes);
  } catch (const std::bad_alloc &) { return 503; }
  catch (...) { return 502; }
}
