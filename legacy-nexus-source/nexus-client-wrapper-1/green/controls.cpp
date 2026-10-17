// Explicit unit-only fake private transfer boundary. No socket/TLS/provider
// execution is claimed; the production C++ wrapper and decoder are unchanged.
#include "nexus_client.h"
#include "download/provider_transfer.h"
#include <array>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <string>

enum Mode { deliver, late_fault, no_consumer, cancel_probe, throw_probe };
static Mode mode = deliver;
static unsigned transfers = 0;
static const char *body = "[{\"id\":37,\"domain_name\":\"future_game-2\",\"name\":\"Fixture Game\"}]";
static std::string requested;

extern "C" int mb_curl_provider_transfer_v1(const mb_curl_provider_transfer_params_v1 *p,
                                            mb_curl_provider_transfer_result_v1 *r) {
    ++transfers;
    assert(p->struct_size == sizeof(*p) && p->abi_version == MB_CURL_DOWNLOAD_ABI_V1);
    assert(p->credential_bytes == 21 && std::memcmp(p->credential, "FixtureOnlyCredential", 21) == 0);
    requested = p->url;
    *r = {};
    r->struct_size = sizeof(*r);
    r->abi_version = MB_CURL_DOWNLOAD_ABI_V1;
    r->http_status = 200;
    std::memset(p->body, 0xc5, p->body_capacity);
    if (mode == throw_probe) throw std::runtime_error("unit-only transfer exception");
    if (mode == cancel_probe) {
        assert(p->cancelled && p->cancelled(p->cancel_context, 0) != 0);
        r->status = MB_CURL_DOWNLOAD_CANCELLED;
        return r->status;
    }
    if (mode == no_consumer) { r->complete = 1; return MB_CURL_DOWNLOAD_OK; }
    const std::size_t bytes = std::strlen(body);
    assert(bytes <= p->body_capacity);
    std::memcpy(p->body, body, bytes);
    r->received_bytes = bytes;
    const int consumed = p->consume_complete_body(p->consume_context, p->body, bytes);
    if (mode == late_fault) {
        r->status = MB_CURL_DOWNLOAD_OWNER_FAILED;
        r->curl_code = 42;
        return r->status;
    }
    r->status = consumed ? MB_CURL_DOWNLOAD_CONSUMER_FAILED : MB_CURL_DOWNLOAD_OK;
    r->complete = consumed ? 0u : 1u;
    return r->status;
}

static void zero(const void *storage, std::size_t bytes) {
    const auto *p = static_cast<const unsigned char *>(storage);
    for (std::size_t i = 0; i < bytes; ++i) assert(p[i] == 0);
}
static int throwing_cancel(void *, uint64_t) { throw std::runtime_error("unit-only cancel exception"); }

int main() {
    std::array<uint8_t, 4096> raw{};
    std::array<char, 4096> report{};
    static const uint8_t ca[] = "fixture-ca";
    mb_nexus_client_params_v1 p{};
    p.struct_size = sizeof(p); p.abi_version = MB_NEXUS_CLIENT_ABI_V1;
    p.kind = MB_NEXUS_GAMES; p.auth_mode = MB_NEXUS_CLIENT_API_KEY;
    p.credential = "FixtureOnlyCredential"; p.credential_bytes = 21;
    p.ca_pem = ca; p.ca_bytes = sizeof(ca) - 1;
    p.timeout_ms = 5000; p.connect_timeout_ms = 1000;
    p.raw_body = raw.data(); p.raw_capacity = raw.size();
    p.report = report.data(); p.report_capacity = report.size();
    mb_nexus_client_result_v1 r{};

    // The missing-link RED caller now reaches the implemented C API safely.
    assert(mb_nexus_client_request_v1(nullptr, nullptr) == MB_NEXUS_CLIENT_BAD_ARGUMENT);
    assert(mb_nexus_client_request_v1(&p, &r) == MB_NEXUS_CLIENT_OK);
    assert(r.complete == 1 && r.metadata_decoded == 1 && r.decode_status == MB_NEXUS_OK);
    assert(r.report_bytes == std::strlen(report.data()) && r.http_status == 200);
    assert(requested == "https://api.nexusmods.com/v1/games");
    zero(raw.data(), raw.size());

    // A rejected metadata body still delivers only its safe owning envelope.
    body = "[{\"apikey\":\"ProviderSecretMarker\",";
    assert(mb_nexus_client_request_v1(&p, &r) == MB_NEXUS_CLIENT_OK);
    assert(r.complete == 1 && !r.metadata_decoded && r.decode_status == MB_NEXUS_OK);
    assert(std::strstr(report.data(), "\"status\":\"malformed\"") != nullptr);
    assert(std::strstr(report.data(), "ProviderSecretMarker") == nullptr);
    zero(raw.data(), raw.size());

    // Size query yields required length only on final CONSUMER_FAILED.
    p.report = nullptr; p.report_capacity = 0;
    assert(mb_nexus_client_request_v1(&p, &r) == MB_NEXUS_CLIENT_OUTPUT_TOO_SMALL);
    assert(r.report_bytes > 0 && !r.complete && !r.metadata_decoded);
    assert(r.decode_status == MB_NEXUS_BUFFER_TOO_SMALL);
    zero(raw.data(), raw.size());
    mode = late_fault;
    assert(mb_nexus_client_request_v1(&p, &r) == MB_NEXUS_CLIENT_TRANSPORT_FAILED);
    assert(!r.report_bytes && !r.complete && !r.metadata_decoded);
    zero(raw.data(), raw.size());

    // A late owner failure revokes a fully copied report, including its tail.
    p.report = report.data(); p.report_capacity = report.size();
    report.fill('x');
    assert(mb_nexus_client_request_v1(&p, &r) == MB_NEXUS_CLIENT_TRANSPORT_FAILED);
    assert(!r.report_bytes && !r.complete && !r.metadata_decoded);
    zero(raw.data(), raw.size()); zero(report.data(), report.size());

    // Private-boundary contract/exception failures cannot publish reports.
    for (Mode next : {no_consumer, throw_probe, cancel_probe}) {
        mode = next; report.fill('x'); raw.fill(0xa5);
        p.cancelled = next == cancel_probe ? throwing_cancel : nullptr;
        assert(mb_nexus_client_request_v1(&p, &r) == MB_NEXUS_CLIENT_TRANSPORT_FAILED);
        assert(!r.report_bytes && !r.complete && !r.metadata_decoded);
        zero(raw.data(), raw.size()); zero(report.data(), report.size());
    }
    p.cancelled = nullptr; mode = deliver;

    // All six typed kinds map arbitrary safe identity context to fixed origins.
    struct Route { int kind; uint32_t auth; const char *domain; uint64_t id; const char *url; };
    const Route routes[] = {
        {MB_NEXUS_GAMES, MB_NEXUS_CLIENT_BEARER, nullptr, 0, "https://api.nexusmods.com/v1/games"},
        {MB_NEXUS_MODS, MB_NEXUS_CLIENT_API_KEY, "future_game-2", 0, "https://api.nexusmods.com/v1/games/future_game-2/mods/latest_added"},
        {MB_NEXUS_MOD, MB_NEXUS_CLIENT_BEARER, "future_game-2", 9223372036854775807ULL, "https://api.nexusmods.com/v1/games/future_game-2/mods/9223372036854775807"},
        {MB_NEXUS_FILES, MB_NEXUS_CLIENT_API_KEY, "future_game-2", 71, "https://api.nexusmods.com/v1/games/future_game-2/mods/71/files"},
        {MB_NEXUS_OAUTH_USER, MB_NEXUS_CLIENT_BEARER, nullptr, 0, "https://users.nexusmods.com/oauth/userinfo"},
        {MB_NEXUS_API_KEY_USER, MB_NEXUS_CLIENT_API_KEY, nullptr, 0, "https://api.nexusmods.com/v1/users/validate"}
    };
    body = "[]";
    for (const auto& route : routes) {
        p.kind = route.kind; p.auth_mode = route.auth; p.domain = route.domain;
        p.domain_bytes = route.domain ? std::strlen(route.domain) : 0; p.mod_id = route.id;
        assert(mb_nexus_client_request_v1(&p, &r) == MB_NEXUS_CLIENT_OK);
        assert(r.complete && requested == route.url);
        zero(raw.data(), raw.size());
    }

    // Wrong account auth modes and ID scopes do not call the transport.
    const unsigned before = transfers;
    p.kind = MB_NEXUS_OAUTH_USER; p.auth_mode = MB_NEXUS_CLIENT_API_KEY;
    assert(mb_nexus_client_request_v1(&p, &r) == MB_NEXUS_CLIENT_BAD_ARGUMENT);
    p.kind = MB_NEXUS_API_KEY_USER; p.auth_mode = MB_NEXUS_CLIENT_BEARER;
    assert(mb_nexus_client_request_v1(&p, &r) == MB_NEXUS_CLIENT_BAD_ARGUMENT);
    p.kind = MB_NEXUS_FILES; p.domain = "future_game-2"; p.domain_bytes = 13; p.mod_id = 0;
    assert(mb_nexus_client_request_v1(&p, &r) == MB_NEXUS_CLIENT_BAD_ARGUMENT);
    assert(transfers == before);
    zero(raw.data(), raw.size()); zero(report.data(), report.size());

    // Alias refusal happens before any output/result wipe or transfer.
    raw.fill(0xa5); report.fill('x'); std::memset(&r, 0xa5, sizeof(r));
    std::array<unsigned char, sizeof(r)> original{};
    std::memcpy(original.data(), &r, sizeof(r));
    p.report = reinterpret_cast<char *>(raw.data() + 64); p.report_capacity = 256;
    assert(mb_nexus_client_request_v1(&p, &r) == MB_NEXUS_CLIENT_BAD_ARGUMENT);
    for (uint8_t byte : raw) assert(byte == 0xa5);
    for (char byte : report) assert(byte == 'x');
    assert(std::memcmp(&r, original.data(), sizeof(r)) == 0 && transfers == before);
}
