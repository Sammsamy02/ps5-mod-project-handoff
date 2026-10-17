#include "owner.h"
#include <cstring>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Json = nlohmann::json;

bool bounded_span(mfs_view value, size_t limit) {
    return value.bytes && value.length && value.length <= limit &&
        reinterpret_cast<uintptr_t>(value.bytes) <=
            std::numeric_limits<uintptr_t>::max() - value.length;
}
Json parse(mfs_view value) {
    if (std::memchr(value.bytes, 0, value.length))
        throw std::invalid_argument("embedded JSON terminator");
    size_t nodes = 0;
    std::vector<std::set<std::string>> keys;
    auto gate = [&](int depth, Json::parse_event_t event, Json &item) {
        if (depth > 64 || ++nodes > 200000) throw std::length_error("JSON limit");
        if (event == Json::parse_event_t::object_start) keys.emplace_back();
        else if (event == Json::parse_event_t::object_end) keys.pop_back();
        else if (event == Json::parse_event_t::key &&
            !keys.back().insert(item.get<std::string>()).second)
            throw std::invalid_argument("duplicate JSON key");
        return true;
    };
    return Json::parse(value.bytes, value.bytes + value.length, gate);
}
struct OwnedResult {
    mfs_owned value{};
    ~OwnedResult() { try { mfs_release(&value); } catch (...) {} }
};
struct PendingService {
    mfs_service *value = nullptr;
    void (*close)(mfs_service *);
    ~PendingService() { if (value) { try { close(value); } catch (...) {} } }
};
mb_mod_service_owner_status description_status(const mfs_owned &reply) {
    if (!reply.release || !bounded_span({reply.bytes, reply.length}, MB_MOD_SERVICE_RESPONSE_LIMIT))
        return MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION;
    Json decoded;
    try { decoded = parse({reply.bytes, reply.length}); }
    catch (const std::bad_alloc &) { throw; }
    catch (...) { return MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION; }
    if (!decoded.is_object() || !decoded.contains("ok") || !decoded["ok"].is_boolean() ||
        !decoded["ok"].get<bool>() || !decoded.contains("result") || !decoded["result"].is_object())
        return MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION;
    const auto &result = decoded["result"];
    if (!result.contains("contract") || result["contract"] != MFS_CONTRACT)
        return MB_MOD_SERVICE_OWNER_UNSUPPORTED_CONTRACT;
    if (!result.contains("methods") || !result["methods"].is_array() || result["methods"].size() > 128)
        return MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION;
    std::set<std::string> methods;
    for (const auto &method : result["methods"]) {
        if (!method.is_string()) return MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION;
        const auto &name = method.get_ref<const std::string &>();
        if (name.empty() || name.size() > 128) return MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION;
        for (unsigned char byte : name)
            if (byte < 32 || byte == 127) return MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION;
        if (!methods.insert(name).second) return MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION;
    }
    return methods.contains("service.describe") && methods.contains("records.list")
        ? MB_MOD_SERVICE_OWNER_OK : MB_MOD_SERVICE_OWNER_UNSUPPORTED_CONTRACT;
}
} // namespace

extern "C" enum mb_mod_service_owner_status mb_mod_service_owner_open(
    mb_mod_service_owner *owner, const mb_mod_service_operations *operations, mfs_view config) {
    if (!owner || !operations || !operations->open || !operations->request || !operations->close)
        return MB_MOD_SERVICE_OWNER_INVALID_ARGUMENT;
    if (owner->bridge.service || owner->bridge.request || owner->close)
        return MB_MOD_SERVICE_OWNER_ALREADY_OPEN;
    if (!bounded_span(config, MB_MOD_SERVICE_CONFIG_LIMIT)) return MB_MOD_SERVICE_OWNER_INVALID_ARGUMENT;
    try {
        try { if (!parse(config).is_object()) return MB_MOD_SERVICE_OWNER_INVALID_ARGUMENT; }
        catch (const std::bad_alloc &) { throw; }
        catch (...) { return MB_MOD_SERVICE_OWNER_INVALID_ARGUMENT; }
        const auto ops = *operations;
        PendingService pending{nullptr, ops.close};
        {
            OwnedResult error;
            const mfs_open_params params{MFS_ABI_VERSION, sizeof(mfs_open_params), config, nullptr, nullptr};
            if (ops.open(&params, &pending.value, &error.value) != 0 || !pending.value)
                return MB_MOD_SERVICE_OWNER_OPEN_FAILED;
        }
        {
            static const char describe[] = "{\"method\":\"service.describe\",\"params\":{}}";
            OwnedResult reply{ops.request(pending.value, {describe, sizeof(describe) - 1})};
            const auto status = description_status(reply.value);
            if (status != MB_MOD_SERVICE_OWNER_OK) return status;
        }
        owner->close = ops.close;
        owner->bridge = {pending.value, ops.request};
        pending.value = nullptr;
        return MB_MOD_SERVICE_OWNER_OK;
    } catch (...) { return MB_MOD_SERVICE_OWNER_RESOURCE_FAILED; }
}
extern "C" const mb_mod_service_bridge *mb_mod_service_owner_bridge(const mb_mod_service_owner *owner) {
    return owner && owner->close && mb_mod_service_usable(&owner->bridge) ? &owner->bridge : nullptr;
}
extern "C" void mb_mod_service_owner_close(mb_mod_service_owner *owner) {
    if (!owner) return;
    auto *service = owner->bridge.service;
    auto close = owner->close;
    *owner = {};
    if (service && close) { try { close(service); } catch (...) {} }
}
extern "C" const char *mb_mod_service_owner_status_name(enum mb_mod_service_owner_status status) {
    switch (status) {
        case MB_MOD_SERVICE_OWNER_OK: return "ready";
        case MB_MOD_SERVICE_OWNER_INVALID_ARGUMENT: return "invalid startup config or operations";
        case MB_MOD_SERVICE_OWNER_ALREADY_OPEN: return "service already open";
        case MB_MOD_SERVICE_OWNER_OPEN_FAILED: return "backend open failed";
        case MB_MOD_SERVICE_OWNER_INVALID_DESCRIPTION: return "invalid service.describe reply";
        case MB_MOD_SERVICE_OWNER_UNSUPPORTED_CONTRACT: return "unsupported contract or missing read methods";
        case MB_MOD_SERVICE_OWNER_RESOURCE_FAILED: return "backend startup resource failure";
    }
    return "unknown startup status";
}
