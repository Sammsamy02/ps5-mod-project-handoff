#pragma once

#include "modbridge/core.hpp"
#include <string_view>

namespace modbridge {

enum class NexusRecordKind { Games, Mods, Mod, Files, OAuthUser, ApiKeyUser, Requirements };

// Construct one fixed-origin read-only metadata URL. This performs no request
// and supplies no credential, account trust or download/install authority.
// Games/users use empty domain and zero ID; Mods uses domain and zero ID;
// Mod/Files require domain and a positive signed-63-bit ID; Requirements uses
// a domain and a positive signed-32-bit GraphQL Int source ID. Invalid context
// returns nullopt; allocation failures can throw to the caller's ABI boundary.
std::optional<std::string> nexus_metadata_url(NexusRecordKind kind,
                           std::string_view domain = {}, std::uint64_t mod_id = 0);

// Write only the fixed requirements query and typed domain/mod variables from
// nexus_requirements_query.h. Output<=4096; success includes NUL and written
// excludes it. Source ID is 1..INT32_MAX. Supported failures wipe the entire
// independent output and written; unsupported/aliased storage stays untouched.
// No arbitrary query, account authority, file/version or installed state is
// supplied. Inputs remain immutable and nothing is retained.
bool nexus_requirements_body(std::string_view domain, std::uint64_t mod_id,
    std::span<char> output, std::size_t& written) noexcept;

// Write the fixed download-link route directly into bounded caller storage.
// Empty key requires zero expiry; otherwise a printable ASCII key (1..1024)
// and a positive signed-63-bit expiry produce one canonical key/expires query.
// Success includes NUL and written excludes it. Failure clears written and
// the full independent output capacity. Unsupported/aliased storage is left
// untouched. Output capacity is at most 8192; no grant is retained or trusted.
bool nexus_download_link_url(std::string_view domain, std::uint64_t mod_id,
    std::uint64_t file_id, std::string_view nxm_key, std::uint64_t expires,
    std::span<char> output, std::size_t& written) noexcept;

// Validate the complete bounded 1..32-mirror response before selecting its
// first URI. URI storage may contain signed query credentials; the caller
// owns its lifetime. Success is NUL-terminated and written excludes NUL.
// Fully valid but short output returns false with required URI byte count;
// malformed/resource failures return false with written=0. Independent output
// (0..8192 bytes) is wiped first; aliased/unsupported storage stays untouched.
// Accessible owned parsed strings are scrubbed before release. This supplies
// no account, source, visibility, entitlement or installation authorization.
bool nexus_select_download_uri(std::span<const std::uint8_t> body,
    std::span<char> output, std::size_t& written);

// Direct bounded authorization_code form writer. No secret owning URL/string
// is constructed. Output<=65536, success includes NUL, written excludes it;
// independent failures wipe output/written, invalid aliases stay untouched.
// Redirect must be an HTTP/HTTPS origin plus exactly /auth/callback. The caller
// owns callback/configuration/state binding and single-use authorization.
bool nexus_oauth_code_form(std::string_view client_id, std::string_view redirect_uri,
    std::string_view code, std::string_view verifier, std::span<char> output,
    std::size_t& written) noexcept;

// Strict complete owning token decode, without a report or JWT claim trust.
// Body<=131072; both private token outputs<=16385 include NUL. Both tokens are
// printable ASCII33..126, 1..16384; Bearer type is case-insensitive; expiry is
// an integer1..31536000. Supported rejection wipes both outputs/lengths/expiry;
// invalid aliases are untouched. Accessible owned parsed strings are scrubbed
// before release. Allocation failure may throw to the no-throw native boundary.
bool nexus_decode_oauth_tokens(std::span<const std::uint8_t> body,
    std::span<char> access_token, std::size_t& access_bytes,
    std::span<char> refresh_token, std::size_t& refresh_bytes,
    std::uint32_t& expires_in_seconds);

// Offline, owning metadata only. No visibility, entitlement, download or install
// authorization is inferred. Requirements returns an object instead of records:
// {game:string, mod_id:uint64, discovery_complete:true,
//  nexus_requirements:[{id:string, external_requirement:bool, game:string,
//    mod_id:string, name:string, notes:string, url:string,
//    satisfaction:"unresolved"}],
//  dlc_requirements:[{game:string,id:string,name:string,notes:string,
//    satisfaction:"unresolved"}],
//  file_version_constraints_provided:false, installation_supported:false}.
// Internal row mod_id is a canonical positive signed-63-bit decimal string;
// external row identities stay typed strings without inferred Nexus authority.
// Counts must establish a complete forward page. DLC/external satisfaction is
// unresolved. Missing/null notes normalize to empty text. Failure envelopes
// contain no records/profile/requirements or raw provider text.
// Allocation of even the safe error envelope can still throw std::bad_alloc.
Json decode_nexus_metadata(std::span<const std::uint8_t> body, NexusRecordKind kind,
                           std::string_view expected_domain = {},
                           std::uint64_t expected_mod_id = 0);

} // namespace modbridge
