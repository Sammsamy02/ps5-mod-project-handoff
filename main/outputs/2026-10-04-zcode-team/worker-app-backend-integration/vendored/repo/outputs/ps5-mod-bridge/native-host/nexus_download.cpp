#include "nexus_download.h"
#include "nexus_client.h"
#include "download/provider_transfer.h"
#include "modbridge/nexus.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <time.h>

namespace {
using Status = mb_nexus_download_status_v1;
constexpr std::uint64_t max_id = INT64_MAX;
constexpr std::uint64_t ns_per_second = 1000000000;
constexpr std::uint64_t ns_per_ms = 1000000;
static_assert(MB_NEXUS_DOWNLOAD_RAW_LIMIT_BYTES == MB_CURL_PROVIDER_BODY_MAX);
static_assert(MB_NEXUS_DOWNLOAD_REPORT_LIMIT_BYTES == MB_NEXUS_OUTPUT_LIMIT_BYTES);
static_assert(MB_NEXUS_DOWNLOAD_URI_LIMIT_BYTES == MB_CURL_DOWNLOAD_URL_MAX);
static_assert(MB_NEXUS_DOWNLOAD_API_KEY == static_cast<int>(MB_NEXUS_CLIENT_API_KEY));
static_assert(MB_NEXUS_DOWNLOAD_BEARER == static_cast<int>(MB_NEXUS_CLIENT_BEARER));

struct Range { std::uintptr_t begin, end; };

bool range(const void *pointer, std::size_t bytes, Range& out) noexcept {
    if (!pointer && bytes) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    if (bytes > std::numeric_limits<std::uintptr_t>::max() - begin) return false;
    out = {begin, begin + bytes};
    return true;
}

bool overlaps(const Range& a, const Range& b) noexcept {
    return a.begin != a.end && b.begin != b.end && a.begin < b.end && b.begin < a.end;
}

bool resolve_bytes(const char *text, std::size_t& bytes) noexcept {
    bytes = 0;
    if (!text) return true;
    Range maximum{};
    if (!range(text, MB_NEXUS_DOWNLOAD_URI_LIMIT_BYTES, maximum)) return false;
    for (std::size_t i = 0; i < MB_NEXUS_DOWNLOAD_URI_LIMIT_BYTES; ++i) {
        if (!text[i]) { bytes = i + 1; return true; }
    }
    return false;
}

bool layout_valid(const mb_nexus_download_params_v1& p,
                  const mb_nexus_download_result_v1 *out) noexcept {
    if (p.struct_size != sizeof(p) || p.abi_version != MB_NEXUS_DOWNLOAD_ABI_V1 ||
        !p.raw_capacity || p.raw_capacity > MB_NEXUS_DOWNLOAD_RAW_LIMIT_BYTES ||
        !p.report_capacity || p.report_capacity > MB_NEXUS_DOWNLOAD_REPORT_LIMIT_BYTES ||
        !p.uri_capacity || p.uri_capacity > MB_NEXUS_DOWNLOAD_URI_LIMIT_BYTES ||
        p.credential_bytes > MB_NEXUS_DOWNLOAD_CREDENTIAL_LIMIT_BYTES ||
        p.domain_bytes > MB_NEXUS_DOWNLOAD_DOMAIN_LIMIT_BYTES ||
        p.ca_bytes > MB_NEXUS_DOWNLOAD_CA_LIMIT_BYTES)
        return false;
    std::array<Range, 13> spans{};
    if (!range(&p, sizeof(p), spans[0]) || !range(out, sizeof(*out), spans[1]) ||
        !range(p.raw_body, p.raw_capacity, spans[2]) ||
        !range(p.metadata_report, p.report_capacity, spans[3]) ||
        !range(p.uri, p.uri_capacity, spans[4]) ||
        !range(p.credential, p.credential_bytes, spans[5]) ||
        !range(p.domain, p.domain_bytes, spans[6]) ||
        !range(p.ca_pem, p.ca_bytes, spans[7]) ||
        !range(p.grant, p.grant ? sizeof(*p.grant) : 0, spans[10]))
        return false;
    if (p.grant) {
        if (reinterpret_cast<std::uintptr_t>(p.grant) % alignof(mb_nexus_download_grant_v1) ||
            p.grant->struct_size != sizeof(*p.grant) ||
            p.grant->abi_version != MB_NEXUS_DOWNLOAD_ABI_V1 ||
            p.grant->domain_bytes > MB_NEXUS_DOWNLOAD_DOMAIN_LIMIT_BYTES ||
            p.grant->key_bytes > MB_NEXUS_DOWNLOAD_KEY_LIMIT_BYTES ||
            !range(p.grant->domain, p.grant->domain_bytes, spans[11]) ||
            !range(p.grant->key, p.grant->key_bytes, spans[12]))
            return false;
    }
    // Reject known aliases before measuring a NUL-terminated resolve input.
    // In particular, do not scan through a smaller writable buffer for NUL.
    for (std::size_t i = 0; i < spans.size(); ++i)
        for (std::size_t j = i + 1; j < spans.size(); ++j)
            if (overlaps(spans[i], spans[j])) return false;
    for (const char *text : {p.api_resolve, p.users_resolve}) {
        if (!text) continue;
        Range first{};
        if (!range(text, 1, first)) return false;
        for (const auto& span : spans) if (overlaps(first, span)) return false;
    }
    std::size_t api_bytes = 0, users_bytes = 0;
    if (!resolve_bytes(p.api_resolve, api_bytes) || !resolve_bytes(p.users_resolve, users_bytes) ||
        !range(p.api_resolve, api_bytes, spans[8]) ||
        !range(p.users_resolve, users_bytes, spans[9]))
        return false;
    for (std::size_t i = 0; i < spans.size(); ++i)
        for (std::size_t j = i + 1; j < spans.size(); ++j)
            if (overlaps(spans[i], spans[j])) return false;
    if (p.cancelled && p.cancel_context) {
        Range context{};
        if (!range(p.cancel_context, 1, context)) return false;
        for (std::size_t i : {std::size_t{1}, std::size_t{2}, std::size_t{3}, std::size_t{4}})
            if (overlaps(context, spans[i])) return false;
    }
    return true;
}

void wipe(void *storage, std::size_t capacity) noexcept {
    auto *bytes = static_cast<volatile unsigned char *>(storage);
    for (std::size_t i = 0; i < capacity; ++i) bytes[i] = 0;
}

struct Wipe {
    void *storage;
    std::size_t capacity;
    bool published = false;
    ~Wipe() { if (!published) wipe(storage, capacity); }
};

bool ascii_secret(const char *text, std::size_t bytes) noexcept {
    if (!bytes) return false;
    for (std::size_t i = 0; i < bytes; ++i) {
        const auto byte = static_cast<unsigned char>(text[i]);
        if (byte < 33 || byte > 126) return false;
    }
    return true;
}

bool domain_valid(std::string_view domain) noexcept {
    const auto alnum = [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    };
    if (domain.empty() || !alnum(static_cast<unsigned char>(domain.front()))) return false;
    for (unsigned char c : domain)
        if (!alnum(c) && c != '-' && c != '_') return false;
    return true;
}

bool identifier(std::uint64_t id) noexcept { return id && id <= max_id; }

bool resolve_valid(const char *text, std::string_view host) noexcept {
    if (!text) return true;
    const std::string_view mapping(text);
    if (mapping.size() <= host.size() + 5 || mapping.substr(0, host.size()) != host ||
        mapping.substr(host.size(), 5) != ":443:") return false;
    for (unsigned char c : mapping)
        if (c < 33 || c > 126 || c == '/' || c == '@' || c == '#' || c == '?' ||
            c == ';' || c == '*') return false;
    return true;
}

bool arguments_valid(const mb_nexus_download_params_v1& p) noexcept {
    if ((p.auth_mode != MB_NEXUS_DOWNLOAD_API_KEY && p.auth_mode != MB_NEXUS_DOWNLOAD_BEARER) ||
        !ascii_secret(p.credential, p.credential_bytes) ||
        !domain_valid(std::string_view(p.domain, p.domain_bytes)) ||
        !identifier(p.mod_id) || !identifier(p.file_id) || !p.ca_bytes ||
        !p.timeout_ms || p.timeout_ms > MB_NEXUS_DOWNLOAD_TIMEOUT_LIMIT_MS ||
        !p.connect_timeout_ms || p.connect_timeout_ms > p.timeout_ms ||
        !resolve_valid(p.api_resolve, "api.nexusmods.com") ||
        !resolve_valid(p.users_resolve, "users.nexusmods.com")) return false;
    if (p.grant) {
        const auto& g = *p.grant;
        if (!domain_valid(std::string_view(g.domain, g.domain_bytes)) ||
            !identifier(g.mod_id) || !identifier(g.file_id) || !identifier(g.user_id) ||
            !identifier(g.expires) || !ascii_secret(g.key, g.key_bytes)) return false;
    }
    return true;
}

struct Operation {
    const mb_nexus_download_params_v1& p;
    std::uint64_t deadline = 0, last_mono = 0, last_wall = 0;
    long last_wall_nanos = 0;
    bool started = false, have_wall = false;
    Status fault = MB_NEXUS_DOWNLOAD_OK;

    bool monotonic(std::uint64_t& now) noexcept {
        timespec value{};
        errno = 0;
        const int rc = clock_gettime(CLOCK_MONOTONIC, &value);
        const int error = errno;
        if (rc || error || value.tv_sec < 0 || value.tv_nsec < 0 ||
            value.tv_nsec >= static_cast<long>(ns_per_second) ||
            static_cast<std::uint64_t>(value.tv_sec) >
                (std::numeric_limits<std::uint64_t>::max() - ns_per_second + 1) / ns_per_second) {
            fault = MB_NEXUS_DOWNLOAD_INTERNAL_ERROR;
            return false;
        }
        now = static_cast<std::uint64_t>(value.tv_sec) * ns_per_second +
              static_cast<std::uint64_t>(value.tv_nsec);
        if (started && now < last_mono) {
            fault = MB_NEXUS_DOWNLOAD_INTERNAL_ERROR;
            return false;
        }
        last_mono = now;
        if (started && now >= deadline && fault != MB_NEXUS_DOWNLOAD_INTERNAL_ERROR)
            fault = MB_NEXUS_DOWNLOAD_TIMEOUT;
        return fault == MB_NEXUS_DOWNLOAD_OK;
    }

    bool start() noexcept {
        std::uint64_t now = 0;
        if (!monotonic(now)) return false;
        const auto budget = static_cast<std::uint64_t>(p.timeout_ms) * ns_per_ms;
        if (budget > std::numeric_limits<std::uint64_t>::max() - now) {
            fault = MB_NEXUS_DOWNLOAD_INTERNAL_ERROR;
            return false;
        }
        deadline = now + budget;
        started = true;
        return true;
    }

    bool checkpoint(std::uint64_t received = 0) noexcept {
        std::uint64_t now = 0;
        if (!monotonic(now)) return false;
        if (p.cancelled) {
            try {
                if (p.cancelled(p.cancel_context, received)) fault = MB_NEXUS_DOWNLOAD_CANCELLED;
            } catch (...) { fault = MB_NEXUS_DOWNLOAD_CANCELLED; }
        }
        // Include time spent in the caller callback in the whole-operation budget.
        return monotonic(now);
    }

    bool remaining(std::uint32_t& timeout, std::uint32_t& connect) noexcept {
        if (!checkpoint()) return false;
        const auto ns = deadline - last_mono;
        timeout = static_cast<std::uint32_t>(ns / ns_per_ms + (ns % ns_per_ms != 0));
        connect = std::min(p.connect_timeout_ms, timeout);
        return timeout != 0 && connect != 0;
    }

    Status grant_expiry() noexcept {
        if (!p.grant) return MB_NEXUS_DOWNLOAD_OK;
        timespec value{};
        errno = 0;
        const int rc = clock_gettime(CLOCK_REALTIME, &value);
        const int error = errno;
        if (rc || error || value.tv_sec < 0 || value.tv_sec > INT64_C(253402300799) ||
            value.tv_nsec < 0 || value.tv_nsec >= static_cast<long>(ns_per_second))
            return fault = MB_NEXUS_DOWNLOAD_INTERNAL_ERROR;
        const auto now = static_cast<std::uint64_t>(value.tv_sec);
        if (have_wall && (now < last_wall || (now == last_wall && value.tv_nsec < last_wall_nanos)))
            return fault = MB_NEXUS_DOWNLOAD_INTERNAL_ERROR;
        have_wall = true;
        last_wall = now;
        last_wall_nanos = value.tv_nsec;
        return p.grant->expires <= static_cast<std::uint64_t>(value.tv_sec)
            ? MB_NEXUS_DOWNLOAD_AUTH_REQUIRED : MB_NEXUS_DOWNLOAD_OK;
    }
};

int cancelled(void *opaque, std::uint64_t received) noexcept {
    return static_cast<Operation *>(opaque)->checkpoint(received) ? 0 : 1;
}

bool positive_json_id(const modbridge::Json& value, std::uint64_t& id) {
    if (value.is_number_unsigned()) id = value.get<std::uint64_t>();
    else if (value.type() == modbridge::Json::value_t::number_integer) {
        const auto signed_id = value.get<std::int64_t>();
        if (signed_id <= 0) return false;
        id = static_cast<std::uint64_t>(signed_id);
    } else return false;
    return identifier(id);
}

bool read_report(const mb_nexus_download_params_v1& p, std::size_t bytes,
                 modbridge::Json& report) {
    if (!bytes || bytes >= p.report_capacity || p.metadata_report[bytes] != '\0') return false;
    // This is the existing client's normalized single-user/single-mod report,
    // not another provider decoder. Bound even this small second parse.
    std::size_t nodes = 0;
    const auto gate = [&nodes](int depth, modbridge::Json::parse_event_t,
                              modbridge::Json&) {
        if (depth > 16 || ++nodes > 4096) throw std::length_error("report limit");
        return true;
    };
    try {
        report = modbridge::Json::parse(p.metadata_report, p.metadata_report + bytes, gate);
        const auto status = report.find("status");
        return report.is_object() && status != report.end() && status->is_string() &&
               *status == "decoded";
    } catch (...) { return false; }
}

Status metadata_request(Operation& operation, int kind,
                        mb_nexus_client_result_v1& result) {
    const auto& p = operation.p;
    mb_nexus_client_params_v1 request{};
    request.struct_size = sizeof(request);
    request.abi_version = MB_NEXUS_CLIENT_ABI_V1;
    request.kind = kind;
    request.auth_mode = p.auth_mode;
    request.credential = p.credential;
    request.credential_bytes = p.credential_bytes;
    if (kind == MB_NEXUS_MOD) {
        request.domain = p.domain;
        request.domain_bytes = p.domain_bytes;
        request.mod_id = p.mod_id;
    }
    request.ca_pem = p.ca_pem;
    request.ca_bytes = p.ca_bytes;
    request.resolve = kind == MB_NEXUS_OAUTH_USER ? p.users_resolve : p.api_resolve;
    if (!operation.remaining(request.timeout_ms, request.connect_timeout_ms)) return operation.fault;
    request.cancel_context = &operation;
    request.cancelled = cancelled;
    request.raw_body = p.raw_body;
    request.raw_capacity = p.raw_capacity;
    request.report = p.metadata_report;
    request.report_capacity = p.report_capacity;
    result = {};
    const int status = mb_nexus_client_request_v1(&request, &result);
    if (!operation.checkpoint()) return operation.fault;
    if (status == MB_NEXUS_CLIENT_OUTPUT_TOO_SMALL && result.status == status)
        return MB_NEXUS_DOWNLOAD_OUTPUT_TOO_SMALL;
    if (status == MB_NEXUS_CLIENT_TRANSPORT_FAILED || result.status == MB_NEXUS_CLIENT_TRANSPORT_FAILED) {
        // CURLE_OPERATION_TIMEDOUT is 28 in the pinned curl ABI. No curl/TLS
        // private headers enter this C++ translation unit.
        return result.curl_code == 28 ? MB_NEXUS_DOWNLOAD_TIMEOUT : MB_NEXUS_DOWNLOAD_TRANSPORT_FAILED;
    }
    if (status != MB_NEXUS_CLIENT_OK || result.status != MB_NEXUS_CLIENT_OK ||
        result.struct_size != sizeof(result) || result.abi_version != MB_NEXUS_CLIENT_ABI_V1 ||
        result.complete != 1 || result.metadata_decoded != 1)
        return MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED;
    return MB_NEXUS_DOWNLOAD_OK;
}

struct LinkConsumer {
    Operation& operation;
    unsigned calls = 0;
    Status status = MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED;
    std::size_t uri_bytes = 0;
};

int consume_link(void *opaque, const std::uint8_t *bytes, std::size_t available) noexcept {
    auto& consumer = *static_cast<LinkConsumer *>(opaque);
    const auto& p = consumer.operation.p;
    ++consumer.calls;
    if (consumer.calls != 1 || bytes != p.raw_body || available > p.raw_capacity) return 1;
    if (!consumer.operation.checkpoint()) { consumer.status = consumer.operation.fault; return 1; }
    consumer.status = consumer.operation.grant_expiry();
    if (consumer.status != MB_NEXUS_DOWNLOAD_OK) return 1;
    try {
        std::size_t written = 0;
        const bool selected = modbridge::nexus_select_download_uri(
            std::span<const std::uint8_t>(bytes, available),
            std::span<char>(p.uri, p.uri_capacity), written);
        if (!selected) {
            consumer.status = written ? MB_NEXUS_DOWNLOAD_OUTPUT_TOO_SMALL : MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED;
            return 1;
        }
        if (!written || written >= p.uri_capacity || written >= MB_NEXUS_DOWNLOAD_URI_LIMIT_BYTES ||
            p.uri[written] != '\0') {
            consumer.status = MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED;
            return 1;
        }
        consumer.uri_bytes = written;
        consumer.status = MB_NEXUS_DOWNLOAD_OK;
        if (!consumer.operation.checkpoint()) consumer.status = consumer.operation.fault;
        else consumer.status = consumer.operation.grant_expiry();
        return consumer.status == MB_NEXUS_DOWNLOAD_OK ? 0 : 1;
    } catch (...) {
        consumer.status = MB_NEXUS_DOWNLOAD_INTERNAL_ERROR;
        return 1;
    }
}

Status resolve(Operation& operation, mb_nexus_download_result_v1& result) {
    const auto& p = operation.p;
    if (!operation.start()) return operation.fault;
    wipe(p.raw_body, p.raw_capacity);
    wipe(p.metadata_report, p.report_capacity);
    wipe(p.uri, p.uri_capacity);
    if (p.grant) {
        result.stage = MB_NEXUS_DOWNLOAD_STAGE_AUTHORIZATION;
        const auto& grant = *p.grant;
        if (grant.mod_id != p.mod_id || grant.file_id != p.file_id ||
            std::string_view(grant.domain, grant.domain_bytes) != std::string_view(p.domain, p.domain_bytes))
            return MB_NEXUS_DOWNLOAD_AUTH_REQUIRED;
        const auto expiry = operation.grant_expiry();
        if (expiry != MB_NEXUS_DOWNLOAD_OK) return expiry;
    }
    result.stage = MB_NEXUS_DOWNLOAD_STAGE_PROFILE;
    mb_nexus_client_result_v1 received{};
    auto status = metadata_request(operation,
        p.auth_mode == MB_NEXUS_DOWNLOAD_BEARER ? MB_NEXUS_OAUTH_USER : MB_NEXUS_API_KEY_USER, received);
    if (status != MB_NEXUS_DOWNLOAD_OK) return status;
    modbridge::Json report;
    if (!read_report(p, received.report_bytes, report)) return MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED;
    const auto profile = report.find("profile");
    if (profile == report.end() || !profile->is_object()) return MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED;
    const auto user = profile->find("user_id"), premium = profile->find("is_premium");
    if (user == profile->end() || !positive_json_id(*user, result.user_id) ||
        premium == profile->end() || !premium->is_boolean()) return MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED;
    result.is_premium = premium->get<bool>() ? 1u : 0u;
    const auto adult = profile->find("adult_content");
    if (adult != profile->end() && adult->is_boolean()) result.adult_content = adult->get<bool>() ? 1 : 0;
    result.stage = MB_NEXUS_DOWNLOAD_STAGE_AUTHORIZATION;
    if (p.grant) {
        if (p.grant->user_id != result.user_id) return MB_NEXUS_DOWNLOAD_AUTH_REQUIRED;
        status = operation.grant_expiry();
        if (status != MB_NEXUS_DOWNLOAD_OK) return status;
        result.grant_used = 1;
    } else if (!result.is_premium) return MB_NEXUS_DOWNLOAD_AUTH_REQUIRED;

    result.stage = MB_NEXUS_DOWNLOAD_STAGE_MOD;
    status = metadata_request(operation, MB_NEXUS_MOD, received);
    if (status != MB_NEXUS_DOWNLOAD_OK) return status;
    report = nullptr;
    if (!read_report(p, received.report_bytes, report)) return MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED;
    const auto records = report.find("records");
    if (records == report.end() || !records->is_array() || records->size() != 1 ||
        !(*records)[0].is_object()) return MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED;
    const auto& mod = (*records)[0];
    const auto mod_id = mod.find("mod_id"), domain = mod.find("game_domain");
    std::uint64_t source_id = 0;
    if (mod_id == mod.end() || !positive_json_id(*mod_id, source_id) || source_id != p.mod_id ||
        domain == mod.end() || !domain->is_string() ||
        domain->get_ref<const std::string&>() != std::string_view(p.domain, p.domain_bytes))
        return MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED;
    const auto available = mod.find("available"), published = mod.find("status"),
               contains_adult = mod.find("contains_adult_content");
    if (available == mod.end() || !available->is_boolean() || !available->get<bool>() ||
        published == mod.end() || !published->is_string() || *published != "published" ||
        contains_adult == mod.end() || !contains_adult->is_boolean() ||
        (contains_adult->get<bool>() && result.adult_content != 1))
        return MB_NEXUS_DOWNLOAD_CONTENT_HIDDEN;
    if (!operation.checkpoint()) return operation.fault;
    status = operation.grant_expiry();
    if (status != MB_NEXUS_DOWNLOAD_OK) return status;

    result.stage = MB_NEXUS_DOWNLOAD_STAGE_LINK;
    std::array<char, MB_NEXUS_DOWNLOAD_URI_LIMIT_BYTES> url{};
    Wipe url_guard{url.data(), url.size()};
    std::size_t url_bytes = 0;
    const std::string_view key = p.grant ? std::string_view(p.grant->key, p.grant->key_bytes) : std::string_view{};
    if (!modbridge::nexus_download_link_url(std::string_view(p.domain, p.domain_bytes), p.mod_id, p.file_id,
        key, p.grant ? p.grant->expires : 0, std::span<char>(url), url_bytes))
        return MB_NEXUS_DOWNLOAD_INTERNAL_ERROR;
    mb_curl_provider_transfer_params_v1 request{};
    request.struct_size = sizeof(request);
    request.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    request.url = url.data();
    request.auth_mode = p.auth_mode;
    request.credential = p.credential;
    request.credential_bytes = p.credential_bytes;
    request.ca_pem = p.ca_pem;
    request.ca_bytes = p.ca_bytes;
    request.resolve = p.api_resolve;
    if (!operation.remaining(request.timeout_ms, request.connect_timeout_ms)) return operation.fault;
    request.cancel_context = &operation;
    request.cancelled = cancelled;
    request.body = p.raw_body;
    request.body_capacity = p.raw_capacity;
    LinkConsumer consumer{operation};
    request.consume_context = &consumer;
    request.consume_complete_body = consume_link;
    mb_curl_provider_transfer_result_v1 response{};
    const int transfer = mb_curl_provider_transfer_v1(&request, &response);
    if (!operation.checkpoint()) return operation.fault;
    if (transfer == MB_CURL_DOWNLOAD_TIMEOUT || response.status == MB_CURL_DOWNLOAD_TIMEOUT)
        return MB_NEXUS_DOWNLOAD_TIMEOUT;
    if (transfer == MB_CURL_DOWNLOAD_CANCELLED || response.status == MB_CURL_DOWNLOAD_CANCELLED)
        return MB_NEXUS_DOWNLOAD_CANCELLED;
    if (transfer == MB_CURL_DOWNLOAD_CONSUMER_FAILED && response.status == MB_CURL_DOWNLOAD_CONSUMER_FAILED &&
        consumer.calls == 1) return consumer.status;
    if (transfer != MB_CURL_DOWNLOAD_OK || response.status != MB_CURL_DOWNLOAD_OK ||
        response.struct_size != sizeof(response) || response.abi_version != MB_CURL_DOWNLOAD_ABI_V1 ||
        response.complete != 1) return MB_NEXUS_DOWNLOAD_TRANSPORT_FAILED;
    if (consumer.calls != 1 || consumer.status != MB_NEXUS_DOWNLOAD_OK || !consumer.uri_bytes)
        return MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED;
    status = operation.grant_expiry();
    if (status != MB_NEXUS_DOWNLOAD_OK) return status;
    result.uri_bytes = consumer.uri_bytes;
    return MB_NEXUS_DOWNLOAD_OK;
}
} // namespace

extern "C" int mb_nexus_resolve_download_v1(const mb_nexus_download_params_v1 *params,
                                           mb_nexus_download_result_v1 *out) noexcept {
    Range params_span{}, result_span{};
    if (!params || !out || !range(params, sizeof(*params), params_span) ||
        !range(out, sizeof(*out), result_span) ||
        reinterpret_cast<std::uintptr_t>(params) % alignof(mb_nexus_download_params_v1) ||
        reinterpret_cast<std::uintptr_t>(out) % alignof(mb_nexus_download_result_v1) ||
        !layout_valid(*params, out))
        return MB_NEXUS_DOWNLOAD_BAD_ARGUMENT;
    const auto& p = *params;
    Wipe raw_guard{p.raw_body, p.raw_capacity}, report_guard{p.metadata_report, p.report_capacity},
         uri_guard{p.uri, p.uri_capacity};
    mb_nexus_download_result_v1 result{};
    result.struct_size = sizeof(result);
    result.abi_version = MB_NEXUS_DOWNLOAD_ABI_V1;
    result.status = MB_NEXUS_DOWNLOAD_BAD_ARGUMENT;
    result.stage = MB_NEXUS_DOWNLOAD_STAGE_ADMISSION;
    result.adult_content = -1;
    if (!arguments_valid(p)) { *out = result; return result.status; }
    result.mod_id = p.mod_id;
    result.file_id = p.file_id;
    Operation operation{p};
    try { result.status = resolve(operation, result); }
    catch (...) { result.status = MB_NEXUS_DOWNLOAD_INTERNAL_ERROR; }
    // Full scratch erasure is part of the same total budget, before publication.
    // Disable guard duplicates so no large erasure runs after a successful guard.
    wipe(p.raw_body, p.raw_capacity);
    wipe(p.metadata_report, p.report_capacity);
    raw_guard.published = report_guard.published = true;
    if (result.status != MB_NEXUS_DOWNLOAD_OK) {
        wipe(p.uri, p.uri_capacity);
        uri_guard.published = true;
    }
    // Finalize after handles, consumer temporaries and full scratch cleanup.
    // A late clock/cancel fault overrides an earlier consumer or output refusal.
    if (operation.started && !operation.checkpoint()) result.status = operation.fault;
    if (result.status == MB_NEXUS_DOWNLOAD_OK) {
        result.status = operation.grant_expiry();
        // The last wall-clock sample is also covered by the total budget.
        // No caller callback or large wipe may follow this publication gate.
        std::uint64_t now = 0;
        if (!operation.monotonic(now)) result.status = operation.fault;
    }
    if (result.status == MB_NEXUS_DOWNLOAD_OK) {
        result.complete = 1;
        result.stage = MB_NEXUS_DOWNLOAD_STAGE_COMPLETE;
        uri_guard.published = true;
    } else {
        result.uri_bytes = 0;
        if (!uri_guard.published) {
            wipe(p.uri, p.uri_capacity);
            uri_guard.published = true;
            std::uint64_t now = 0;
            if (operation.started && !operation.monotonic(now)) result.status = operation.fault;
        }
    }
    *out = result;
    return result.status;
}
