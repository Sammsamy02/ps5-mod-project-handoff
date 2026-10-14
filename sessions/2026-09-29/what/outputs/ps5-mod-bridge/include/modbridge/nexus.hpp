#pragma once

#include "modbridge/core.hpp"
#include <string_view>

namespace modbridge {

enum class NexusRecordKind { Games, Mods, Mod, Files, OAuthUser, ApiKeyUser };

// Construct one fixed-origin read-only metadata URL. This performs no request
// and supplies no credential, account trust or download/install authority.
// Games/users use empty domain and zero ID; Mods uses domain and zero ID;
// Mod/Files require domain and a positive signed-63-bit ID. Invalid context
// returns nullopt; allocation failures can throw to the caller's ABI boundary.
std::optional<std::string> nexus_metadata_url(NexusRecordKind kind,
                           std::string_view domain = {}, std::uint64_t mod_id = 0);

// Offline, owning metadata only. No visibility, entitlement, download or install
// authorization is inferred. Failures contain no records/profile or provider text.
// Allocation of even the safe error envelope can still throw std::bad_alloc.
Json decode_nexus_metadata(std::span<const std::uint8_t> body, NexusRecordKind kind,
                           std::string_view expected_domain = {},
                           std::uint64_t expected_mod_id = 0);

} // namespace modbridge
