#include "read_bridge.h"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <cstring>
extern "C" int mb_mod_service_read(const mb_mod_service_bridge *b,
    enum mb_mod_service_read kind, char **bytes, size_t *length) {
    if (!bytes || !length) return 502;
    *bytes = nullptr; *length = 0;
    if (!mb_mod_service_usable(b)) return 503;
    const char *request;
    if (kind == MB_MOD_SERVICE_DESCRIBE) request = "{\"method\":\"service.describe\",\"params\":{}}";
    else if (kind == MB_MOD_SERVICE_RECORDS) request = "{\"method\":\"records.list\",\"params\":{}}";
    else return 502;
    mfs_owned response = b->request(b->service, {request, std::strlen(request)});
    int status = 502;
    char *copy = nullptr;
    size_t size = response.length;
    if (response.bytes && response.release && size && size <= MB_MOD_SERVICE_RESPONSE_LIMIT) {
        copy = static_cast<char *>(std::malloc(size + 1));
        if (copy) { std::memcpy(copy, response.bytes, size); copy[size] = 0; }
        else status = 503;
    }
    mfs_release(&response);
    if (copy) {
        try {
            auto value = nlohmann::json::parse(copy, copy + size);
            if (value.is_object() && value.contains("ok") && value["ok"].is_boolean() &&
                ((value["ok"].get<bool>() && value.contains("result") && value["result"].is_object()) ||
                 (!value["ok"].get<bool>() && value.contains("error") && value["error"].is_object()))) {
                *bytes = copy; *length = size; return 200;
            }
        } catch (...) { }
        std::free(copy);
    }
    return status;
}

extern "C" int mb_mod_service_call(const mb_mod_service_bridge *b,
    const char *request_bytes, size_t request_bytes_len,
    char **bytes, size_t *length) {
    if (!bytes || !length) return 502;
    *bytes = nullptr; *length = 0;
    if (!mb_mod_service_usable(b)) return 503;
    if (!request_bytes || !request_bytes_len ||
        request_bytes_len > MB_MOD_SERVICE_RESPONSE_LIMIT) return 502;
    mfs_owned response = b->request(b->service,
        {request_bytes, request_bytes_len});
    int status = 502;
    char *copy = nullptr;
    size_t size = response.length;
    if (response.bytes && response.release && size && size <= MB_MOD_SERVICE_RESPONSE_LIMIT) {
        copy = static_cast<char *>(std::malloc(size + 1));
        if (copy) { std::memcpy(copy, response.bytes, size); copy[size] = 0; }
        else status = 503;
    }
    mfs_release(&response);
    if (copy) {
        try {
            auto value = nlohmann::json::parse(copy, copy + size);
            if (value.is_object() && value.contains("ok") && value["ok"].is_boolean() &&
                ((value["ok"].get<bool>() && value.contains("result") && value["result"].is_object()) ||
                 (!value["ok"].get<bool>() && value.contains("error") && value["error"].is_object()))) {
                *bytes = copy; *length = size; return 200;
            }
        } catch (...) { }
        std::free(copy);
    }
    return status;
}
