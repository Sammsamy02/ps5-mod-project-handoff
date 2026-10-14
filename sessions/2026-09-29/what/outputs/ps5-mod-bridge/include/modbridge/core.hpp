#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <nlohmann/json.hpp>

namespace modbridge {

using Json = nlohmann::json;
inline constexpr int record_schema_version = 1;
inline constexpr const char* core_version = "0.1.0";
inline constexpr const char* parser_version = "0.1.0";

struct Limits {
    std::uint64_t max_entries = 10000;
    std::uint64_t max_total_bytes = 1073741824;
    std::uint64_t max_entry_bytes = 268435456;
    std::uint64_t max_inspection_bytes = 8388608;
};

// Pure selection only: callers supply a complete, immutable file table in
// nondecreasing package rank after resolving loader configuration/dependencies.
// Equal ranks identify one package. Normalized duplicate or ancestor keys in
// that package reject the whole table. Ranks need not be contiguous.
// Source indices are opaque; this function never opens or redirects a file.
// byte_exact preserves non-ASCII bytes; ascii_insensitive rejects them rather
// than promising an unverified Unicode case convention.
enum class OverrideCase { byte_exact, ascii_insensitive };
enum class OverrideWinner { first_package, last_package };
enum class OverrideStatus {
    selected, passthrough, invalid_input, limit_exceeded, unsupported
};
struct FileOverrideEntry {
    std::uint64_t package_rank;
    std::string_view relative_key;
    std::uint64_t opaque_source_index;
};
struct FileOverridePolicy {
    // Exact virtual root names [A-Za-z0-9_-]+: (e.g. "data0:"). Root
    // spelling is exact under either case policy and must be followed by a
    // separator. Only requests may use roots; table keys are always relative.
    std::span<const std::string_view> virtual_roots;
    OverrideCase case_policy = OverrideCase::byte_exact;
    OverrideWinner winner = OverrideWinner::last_package;
};
struct FileOverrideLimits {
    // Callers may tighten these built-in ceilings, never raise them. Paths
    // additionally have the inventory grammar's fixed 4096-byte raw ceiling.
    std::uint64_t max_entries = 10000;
    std::uint64_t max_key_bytes = 8388608; // request, aliases and table combined
    // Weighted key-processing budget, not a wall-clock/allocator guarantee.
    std::uint64_t max_work_bytes = 67108864;
    std::uint64_t max_virtual_roots = 64;
};
struct FileOverrideChoice {
    OverrideStatus status = OverrideStatus::invalid_input;
    // Populated only after validating the entire table, even for an early hit.
    // Passthrough also requires the entire table and policy to be valid.
    std::optional<std::uint64_t> opaque_source_index;
};
FileOverrideChoice choose_file_override(
    std::string_view request, std::span<const FileOverrideEntry> entries,
    const FileOverridePolicy& policy, const FileOverrideLimits& limits = {});

struct FileOverridePreparation;
// Owns validated keys and policy aliases. Lookup does not rescan the package
// table. Copies share immutable storage; mutations to caller input have no effect.
class FileOverrideTable {
public:
    FileOverrideTable(const FileOverrideTable&) = default;
    FileOverrideTable(FileOverrideTable&&) noexcept = default;
    FileOverrideTable& operator=(const FileOverrideTable&) = default;
    FileOverrideTable& operator=(FileOverrideTable&&) noexcept = default;
    // Per-call weighted work is capped by the stored preparation limit too.
    // Values above the built-in ceiling reject; moved-from tables reject.
    FileOverrideChoice lookup(std::string_view request,
        std::uint64_t max_work_bytes = 524288) const;
private:
    struct Data;
    std::shared_ptr<const Data> data_;
    explicit FileOverrideTable(std::shared_ptr<const Data> data) : data_(std::move(data)) {}
    friend FileOverridePreparation prepare_file_overrides(
        std::span<const FileOverrideEntry>, const FileOverridePolicy&,
        const FileOverrideLimits&);
};
enum class FileOverridePreparationStatus { ready, invalid_input, limit_exceeded, unsupported };
struct FileOverridePreparation {
    FileOverridePreparationStatus status = FileOverridePreparationStatus::invalid_input;
    std::optional<FileOverrideTable> table;
};
// Preparation validates policy and the complete table without a request.
// Lookup separately validates its request and includes it in the retained raw
// key-byte ceiling. Work budgets are per phase; failed preparation owns no table.
FileOverridePreparation prepare_file_overrides(
    std::span<const FileOverrideEntry> entries, const FileOverridePolicy& policy,
    const FileOverrideLimits& limits = {});

std::string sha256_bytes(std::span<const std::uint8_t> bytes);
std::string sha256_file(const std::filesystem::path& path);
// Deterministic, compact JSON with lexically sorted object keys. This is
// nlohmann serialization, not the separate RFC 8785/JCS number convention.
std::string canonical_json(const Json& value);

Json inspect_binary(std::span<const std::uint8_t> bytes, const std::string& path,
                    std::optional<std::uint64_t> total_size = std::nullopt);
Json analyze_package(const std::filesystem::path& input, const Limits& limits);
Json build_plan(const Json& inventory, const Json& target, const Json& backends);
std::string cache_key(const Json& context);
bool cache_evidence_usable(const Json& cached, const Json& current);
Json deploy_scratch(const std::filesystem::path& root,
                    const std::filesystem::path& prepared,
                    const Json& manifest, int fail_after = -1);
Json recover_scratch(const std::filesystem::path& root);
Json uninstall_scratch(const std::filesystem::path& root,
                       const std::string& transaction_id);

} // namespace modbridge
