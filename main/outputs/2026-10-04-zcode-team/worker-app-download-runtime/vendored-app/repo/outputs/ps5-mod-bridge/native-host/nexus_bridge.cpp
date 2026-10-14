#include "nexus_bridge.h"
#include "modbridge/nexus.hpp"

#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace {
bool context_valid(int kind, size_t domain_length, uint64_t mod_id) noexcept {
    const auto largest_id = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    switch (kind) {
    case MB_NEXUS_GAMES:
    case MB_NEXUS_OAUTH_USER:
    case MB_NEXUS_API_KEY_USER:
        return domain_length == 0 && mod_id == 0;
    case MB_NEXUS_MODS:
        return domain_length != 0 && mod_id == 0;
    case MB_NEXUS_MOD:
    case MB_NEXUS_FILES:
        return domain_length != 0 && mod_id != 0 && mod_id <= largest_id;
    case MB_NEXUS_REQUIREMENTS:
        return domain_length != 0 && mod_id != 0 &&
               mod_id <= static_cast<uint64_t>(std::numeric_limits<int32_t>::max());
    default:
        return false;
    }
}
modbridge::NexusRecordKind record_kind(int kind) {
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
}

extern "C" int mb_nexus_decode(int kind, const unsigned char *bytes, size_t available,
        const char *domain, size_t domain_length, uint64_t mod_id,
        char *output, size_t capacity, size_t *length) noexcept {
    if (length) *length = 0;
    if (output && capacity) output[0] = 0;
    if (!length || (!output && capacity) || (!bytes && available)
            || (!domain && domain_length) || domain_length > MB_NEXUS_DOMAIN_LIMIT_BYTES
            || capacity > MB_NEXUS_OUTPUT_LIMIT_BYTES
            || !context_valid(kind, domain_length, mod_id))
        return MB_NEXUS_INVALID_ARGUMENT;
    if (available > MB_NEXUS_INPUT_LIMIT_BYTES) return MB_NEXUS_INPUT_LIMIT;
    try {
        const auto input = available ? std::span<const std::uint8_t>(bytes, available)
                                     : std::span<const std::uint8_t>{};
        const auto expected_domain = domain_length ? std::string_view(domain, domain_length)
                                                   : std::string_view{};
        const auto report = modbridge::decode_nexus_metadata(input, record_kind(kind),
                                                             expected_domain, mod_id);
        const std::string owned = report.dump();
        if (owned.size() >= MB_NEXUS_OUTPUT_LIMIT_BYTES) return MB_NEXUS_OUTPUT_LIMIT;
        *length = owned.size();
        if (capacity <= owned.size()) return MB_NEXUS_BUFFER_TOO_SMALL;
        std::memcpy(output, owned.c_str(), owned.size()+1);
        return MB_NEXUS_OK;
    } catch (...) {
        *length = 0;
        return MB_NEXUS_INTERNAL_ERROR;
    }
}

extern "C" int mb_nexus_metadata_url(int kind, const char *domain, size_t domain_length, uint64_t mod_id,
        char *output, size_t capacity, size_t *length) noexcept {
    if (length) *length = 0;
    if (output && capacity) output[0] = 0;
    if (!length || (!output && capacity) || (!domain && domain_length)
            || domain_length > MB_NEXUS_DOMAIN_LIMIT_BYTES
            || capacity > MB_NEXUS_URL_LIMIT_BYTES
            || !context_valid(kind, domain_length, mod_id))
        return MB_NEXUS_INVALID_ARGUMENT;
    try {
        const auto expected_domain = domain_length ? std::string_view(domain, domain_length)
                                                   : std::string_view{};
        const auto owned = modbridge::nexus_metadata_url(record_kind(kind), expected_domain, mod_id);
        if (!owned) return MB_NEXUS_INVALID_ARGUMENT;
        if (owned->size() >= MB_NEXUS_URL_LIMIT_BYTES) return MB_NEXUS_OUTPUT_LIMIT;
        *length = owned->size();
        if (capacity <= owned->size()) return MB_NEXUS_BUFFER_TOO_SMALL;
        std::memcpy(output, owned->c_str(), owned->size()+1);
        return MB_NEXUS_OK;
    } catch (...) {
        *length = 0;
        return MB_NEXUS_INTERNAL_ERROR;
    }
}
