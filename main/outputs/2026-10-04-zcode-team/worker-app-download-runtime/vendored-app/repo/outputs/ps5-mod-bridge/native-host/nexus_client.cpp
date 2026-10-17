#include "nexus_client.h"
#include "download/provider_transfer.h"
#include "modbridge/nexus.hpp"
#include "modbridge/nexus_requirements_query.h"

#include <array>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace {
static_assert(MB_NEXUS_INPUT_LIMIT_BYTES == MB_CURL_PROVIDER_BODY_MAX);
static_assert(MB_NEXUS_CLIENT_CREDENTIAL_LIMIT_BYTES == MB_CURL_PROVIDER_SECRET_MAX);
static_assert(MB_NEXUS_CLIENT_CA_LIMIT_BYTES == MB_CURL_DOWNLOAD_CA_MAX);
static_assert(MB_NEXUS_CLIENT_TIMEOUT_LIMIT_MS == MB_CURL_DOWNLOAD_TIMEOUT_MAX_MS);
static_assert(MB_NEXUS_REQUIREMENTS_REQUEST_LIMIT <= MB_CURL_PROVIDER_BODY_MAX);

struct span_range {
    std::uintptr_t begin, end;
};

bool make_range(const void *pointer, std::size_t bytes, span_range& range) noexcept {
    if (!pointer && bytes) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    if (bytes > std::numeric_limits<std::uintptr_t>::max() - begin) return false;
    range = {begin, begin + bytes};
    return true;
}

bool overlaps(const span_range& a, const span_range& b) noexcept {
    return a.begin != a.end && b.begin != b.end && a.begin < b.end && b.begin < a.end;
}

bool resolve_span(const char *resolve, std::size_t& bytes) noexcept {
    bytes = 0;
    if (!resolve) return true;
    for (std::size_t i = 0; i < MB_CURL_DOWNLOAD_URL_MAX; ++i) {
        if (!resolve[i]) { bytes = i + 1; return true; }
    }
    return false;
}

bool layout_valid(const mb_nexus_client_params_v1& p,
                  const mb_nexus_client_result_v1 *result) noexcept {
    if (p.struct_size != sizeof(p) || p.abi_version != MB_NEXUS_CLIENT_ABI_V1 ||
        !p.raw_capacity || p.raw_capacity > MB_NEXUS_INPUT_LIMIT_BYTES ||
        p.report_capacity > MB_NEXUS_OUTPUT_LIMIT_BYTES)
        return false;
    std::size_t resolve_bytes = 0;
    if (!resolve_span(p.resolve, resolve_bytes)) return false;
    std::array<span_range, 8> ranges{};
    if (!make_range(&p, sizeof(p), ranges[0]) ||
        !make_range(result, sizeof(*result), ranges[1]) ||
        !make_range(p.raw_body, p.raw_capacity, ranges[2]) ||
        !make_range(p.report, p.report_capacity, ranges[3]) ||
        !make_range(p.credential, p.credential_bytes, ranges[4]) ||
        !make_range(p.domain, p.domain_bytes, ranges[5]) ||
        !make_range(p.ca_pem, p.ca_bytes, ranges[6]) ||
        !make_range(p.resolve, resolve_bytes, ranges[7]))
        return false;
    for (std::size_t i = 0; i < ranges.size(); ++i)
        for (std::size_t j = i + 1; j < ranges.size(); ++j)
            if (overlaps(ranges[i], ranges[j])) return false;
    if (p.cancelled && p.cancel_context) {
        span_range context{};
        if (!make_range(p.cancel_context, 1, context)) return false;
        // The opaque object's full extent remains the caller's contract. Its
        // address may not be in storage that this call overwrites or wipes.
        for (std::size_t i : {std::size_t{1}, std::size_t{2}, std::size_t{3}})
            if (overlaps(context, ranges[i])) return false;
    }
    return true;
}

void wipe(void *pointer, std::size_t bytes) noexcept {
    auto *output = static_cast<volatile unsigned char *>(pointer);
    for (std::size_t i = 0; i < bytes; ++i) output[i] = 0;
}

struct raw_wipe {
    void *pointer;
    std::size_t bytes;
    ~raw_wipe() { wipe(pointer, bytes); }
};

struct report_wipe {
    void *pointer;
    std::size_t bytes;
    bool published = false;
    ~report_wipe() { if (!published) wipe(pointer, bytes); }
};

bool arguments_valid(const mb_nexus_client_params_v1& p) noexcept {
    const bool requirements = p.kind == MB_NEXUS_REQUIREMENTS;
    if (!p.credential_bytes || p.credential_bytes > MB_NEXUS_CLIENT_CREDENTIAL_LIMIT_BYTES ||
        p.domain_bytes > MB_NEXUS_DOMAIN_LIMIT_BYTES ||
        !p.ca_bytes || p.ca_bytes > MB_NEXUS_CLIENT_CA_LIMIT_BYTES ||
        !p.timeout_ms || p.timeout_ms > MB_NEXUS_CLIENT_TIMEOUT_LIMIT_MS ||
        !p.connect_timeout_ms || p.connect_timeout_ms > p.timeout_ms ||
        (p.auth_mode != MB_NEXUS_CLIENT_API_KEY && p.auth_mode != MB_NEXUS_CLIENT_BEARER) ||
        (p.kind == MB_NEXUS_OAUTH_USER && p.auth_mode != MB_NEXUS_CLIENT_BEARER) ||
        (p.kind == MB_NEXUS_API_KEY_USER && p.auth_mode != MB_NEXUS_CLIENT_API_KEY) ||
        (requirements && (p.auth_mode != MB_NEXUS_CLIENT_BEARER || !p.domain_bytes ||
                          !p.mod_id || p.mod_id > static_cast<std::uint64_t>(INT32_MAX))))
        return false;
    for (std::size_t i = 0; i < p.credential_bytes; ++i) {
        const auto byte = static_cast<unsigned char>(p.credential[i]);
        if (byte < 33 || byte > 126) return false;
    }
    return true;
}

modbridge::NexusRecordKind record_kind(int kind) noexcept {
    switch (kind) {
    case MB_NEXUS_GAMES: return modbridge::NexusRecordKind::Games;
    case MB_NEXUS_MODS: return modbridge::NexusRecordKind::Mods;
    case MB_NEXUS_MOD: return modbridge::NexusRecordKind::Mod;
    case MB_NEXUS_FILES: return modbridge::NexusRecordKind::Files;
    case MB_NEXUS_OAUTH_USER: return modbridge::NexusRecordKind::OAuthUser;
    case MB_NEXUS_REQUIREMENTS: return modbridge::NexusRecordKind::Requirements;
    default: return modbridge::NexusRecordKind::ApiKeyUser;
    }
}

struct consume_context {
    const mb_nexus_client_params_v1 *params;
    unsigned calls = 0;
    mb_nexus_client_status_v1 status = MB_NEXUS_CLIENT_DECODE_FAILED;
    int decode_status = MB_NEXUS_CLIENT_DECODE_NOT_ATTEMPTED;
    std::size_t required = 0;
    bool decoded = false;
};

int consume_complete_body(void *opaque, const uint8_t *bytes, std::size_t available) noexcept {
    auto& context = *static_cast<consume_context *>(opaque);
    const auto& p = *context.params;
    ++context.calls;
    context.status = MB_NEXUS_CLIENT_DECODE_FAILED;
    context.decode_status = MB_NEXUS_INTERNAL_ERROR;
    context.required = 0;
    context.decoded = false;
    if (context.calls != 1 || bytes != p.raw_body || available > p.raw_capacity)
        return 1;
    try {
        const auto domain = p.domain_bytes ? std::string_view(p.domain, p.domain_bytes)
                                          : std::string_view{};
        const auto report = modbridge::decode_nexus_metadata(
            std::span<const uint8_t>(bytes, available), record_kind(p.kind), domain, p.mod_id);
        const auto status = report.find("status");
        const bool decoded = status != report.end() && status->is_string() && *status == "decoded";
        const std::string owned = report.dump();
        if (owned.size() >= MB_NEXUS_OUTPUT_LIMIT_BYTES) {
            context.decode_status = MB_NEXUS_OUTPUT_LIMIT;
            return 1;
        }
        context.required = owned.size();
        if (p.report_capacity <= owned.size()) {
            context.status = MB_NEXUS_CLIENT_OUTPUT_TOO_SMALL;
            context.decode_status = MB_NEXUS_BUFFER_TOO_SMALL;
            return 1;
        }
        std::memcpy(p.report, owned.c_str(), owned.size() + 1);
        context.status = MB_NEXUS_CLIENT_OK;
        context.decode_status = MB_NEXUS_OK;
        context.decoded = decoded;
        return 0;
    } catch (...) {
        context.required = 0;
        return 1;
    }
}

int cancelled(void *opaque, uint64_t received) noexcept {
    const auto& context = *static_cast<consume_context *>(opaque);
    try {
        return context.params->cancelled(context.params->cancel_context, received);
    } catch (...) {
        return 1;
    }
}
} // namespace

extern "C" int mb_nexus_client_request_v1(const mb_nexus_client_params_v1 *params,
                                         mb_nexus_client_result_v1 *out) noexcept {
    if (!params || !out || !layout_valid(*params, out)) return MB_NEXUS_CLIENT_BAD_ARGUMENT;
    const auto& p = *params;
    wipe(p.raw_body, p.raw_capacity);
    wipe(p.report, p.report_capacity);
    raw_wipe raw_guard{p.raw_body, p.raw_capacity};
    report_wipe report_guard{p.report, p.report_capacity};
    mb_nexus_client_result_v1 result{};
    result.struct_size = sizeof(result);
    result.abi_version = MB_NEXUS_CLIENT_ABI_V1;
    result.status = MB_NEXUS_CLIENT_BAD_ARGUMENT;
    result.decode_status = MB_NEXUS_CLIENT_DECODE_NOT_ATTEMPTED;
    if (!arguments_valid(p)) { *out = result; return result.status; }
    const bool requirements = p.kind == MB_NEXUS_REQUIREMENTS;
    std::array<char, MB_NEXUS_URL_LIMIT_BYTES> url{};
    std::array<char, MB_NEXUS_REQUIREMENTS_REQUEST_LIMIT> request_body{};
    raw_wipe request_body_guard{request_body.data(), request_body.size()};
    const char *request_url = nullptr;
    std::size_t url_bytes = 0;
    std::size_t request_body_bytes = 0;
    if (requirements) {
        if (!modbridge::nexus_requirements_body(
                std::string_view(p.domain, p.domain_bytes), p.mod_id, request_body, request_body_bytes)) {
            result.decode_status = MB_NEXUS_INVALID_ARGUMENT;
            *out = result;
            return result.status;
        }
        request_url = MB_NEXUS_REQUIREMENTS_GRAPHQL_URL;
    } else {
        const int url_status = mb_nexus_metadata_url(p.kind, p.domain, p.domain_bytes, p.mod_id,
                                                    url.data(), url.size(), &url_bytes);
        if (url_status != MB_NEXUS_OK) {
            if (url_status != MB_NEXUS_INVALID_ARGUMENT) {
                result.status = MB_NEXUS_CLIENT_DECODE_FAILED;
                result.decode_status = url_status;
            }
            *out = result;
            return result.status;
        }
        request_url = url.data();
    }
    consume_context context{params};
    mb_curl_provider_transfer_params_v1 transfer{};
    transfer.struct_size = sizeof(transfer);
    transfer.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    transfer.url = request_url;
    transfer.auth_mode = p.auth_mode;
    transfer.credential = p.credential;
    transfer.credential_bytes = p.credential_bytes;
    transfer.ca_pem = p.ca_pem;
    transfer.ca_bytes = p.ca_bytes;
    transfer.resolve = p.resolve;
    transfer.timeout_ms = p.timeout_ms;
    transfer.connect_timeout_ms = p.connect_timeout_ms;
    transfer.cancel_context = &context;
    transfer.cancelled = p.cancelled ? cancelled : nullptr;
    transfer.body = p.raw_body;
    transfer.body_capacity = p.raw_capacity;
    transfer.consume_context = &context;
    transfer.consume_complete_body = consume_complete_body;
    mb_curl_provider_transfer_result_v1 received{};
    received.struct_size = sizeof(received);
    received.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    result.status = MB_NEXUS_CLIENT_TRANSPORT_FAILED;
    try {
        int transport_status;
        if (requirements) {
            mb_curl_provider_post_transfer_params_v1 post{};
            post.struct_size = sizeof(post);
            post.abi_version = MB_CURL_DOWNLOAD_ABI_V1;
            post.provider = transfer;
            post.request_body = request_body.data();
            post.request_body_bytes = request_body_bytes;
            transport_status = mb_curl_provider_post_transfer_v1(&post, &received);
        } else {
            transport_status = mb_curl_provider_transfer_v1(&transfer, &received);
        }
        result.http_status = received.http_status;
        result.curl_code = received.curl_code;
        result.received_bytes = received.received_bytes;
        result.decode_status = context.decode_status;
        if (transport_status == MB_CURL_DOWNLOAD_OK && received.status == MB_CURL_DOWNLOAD_OK &&
            received.complete == 1 && context.calls == 1 && context.status == MB_NEXUS_CLIENT_OK) {
            result.status = MB_NEXUS_CLIENT_OK;
            result.complete = 1;
            result.metadata_decoded = context.decoded ? 1u : 0u;
            result.report_bytes = context.required;
            report_guard.published = true;
        } else if (transport_status == MB_CURL_DOWNLOAD_CONSUMER_FAILED &&
                   received.status == MB_CURL_DOWNLOAD_CONSUMER_FAILED && context.calls) {
            result.status = context.status == MB_NEXUS_CLIENT_OUTPUT_TOO_SMALL
                          ? MB_NEXUS_CLIENT_OUTPUT_TOO_SMALL : MB_NEXUS_CLIENT_DECODE_FAILED;
            if (result.status == MB_NEXUS_CLIENT_OUTPUT_TOO_SMALL)
                result.report_bytes = context.required;
        }
    } catch (...) {
        result.decode_status = context.decode_status;
    }
    *out = result;
    return result.status;
}
