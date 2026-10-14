#include "test.hpp"
#include "modbridge/core.hpp"

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace modbridge;

namespace {
const std::array<std::string_view, 2> aliases{"data0:", "data1:"};

FileOverridePolicy policy(
    OverrideCase case_policy = OverrideCase::byte_exact,
    OverrideWinner winner = OverrideWinner::last_package) {
    return {aliases, case_policy, winner};
}

void expect(const FileOverrideChoice& got, OverrideStatus status,
            std::optional<std::uint64_t> source = std::nullopt) {
    CHECK_EQ(static_cast<int>(got.status), static_cast<int>(status));
    CHECK(got.opaque_source_index == source);
}

void rejected(const FileOverridePreparation& got, FileOverridePreparationStatus status) {
    CHECK_EQ(static_cast<int>(got.status), static_cast<int>(status));
    CHECK(!got.table.has_value());
}

FileOverrideTable ready(std::span<const FileOverrideEntry> entries,
                        const FileOverridePolicy& p,
                        const FileOverrideLimits& limits = {}) {
    auto preparation = prepare_file_overrides(entries, p, limits);
    CHECK(preparation.status == FileOverridePreparationStatus::ready);
    CHECK(preparation.table.has_value());
    return std::move(*preparation.table);
}
} // namespace

TEST(prepared_override_first_policy_matches_relative_and_root_separator_variants) {
    const std::array<FileOverrideEntry, 2> entries{{
        {2, "event/file.bin", 0}, {7, "event/file.bin", 41}
    }};
    const auto table = ready(entries,
        policy(OverrideCase::byte_exact, OverrideWinner::first_package));
    for (const auto request : {"event/file.bin", "event\\file.bin",
                              "data0:/event/file.bin", "data1:\\event\\file.bin"})
        expect(table.lookup(request), OverrideStatus::selected, 0);
}

TEST(prepared_override_last_policy_remains_fixed_across_repeated_hits_and_misses) {
    const std::array<FileOverrideEntry, 3> entries{{
        {2, "event/file.bin", 0}, {7, "event/file.bin", 41}, {7, "other.bin", 52}
    }};
    const auto table = ready(entries, policy());
    expect(table.lookup("event/file.bin"), OverrideStatus::selected, 41);
    expect(table.lookup("missing.bin"), OverrideStatus::passthrough);
    expect(table.lookup("other.bin"), OverrideStatus::selected, 52);
    expect(table.lookup("event/file.bin"), OverrideStatus::selected, 41);
    expect(table.lookup("data0:/other.bin"), OverrideStatus::selected, 52);
    expect(table.lookup("event/file.bin"), OverrideStatus::selected, 41);
}

TEST(prepared_override_owns_keys_aliases_entries_and_policy_after_input_destruction) {
    std::optional<FileOverrideTable> table;
    {
        std::string first_key = "event/file.bin", last_key = "event/file.bin";
        std::string root = "data0:";
        std::array<std::string_view, 1> roots{root};
        std::vector<FileOverrideEntry> entries{{0, first_key, 0}, {1, last_key, 41}};
        FileOverridePolicy p{roots, OverrideCase::byte_exact, OverrideWinner::last_package};
        table.emplace(ready(entries, p));
        first_key.assign("changed-first.bin");
        last_key.assign("changed-last.bin");
        root.assign("changed:");
        roots[0] = "different:";
        entries[0] = {9, "different.bin", 999};
        entries.clear();
        p.winner = OverrideWinner::first_package;
        expect(table->lookup("data0:/event/file.bin"), OverrideStatus::selected, 41);
    }
    expect(table->lookup("data0:\\event\\file.bin"), OverrideStatus::selected, 41);
    expect(table->lookup("changed-last.bin"), OverrideStatus::passthrough);
    expect(table->lookup("changed:/event/file.bin"), OverrideStatus::invalid_input);
}

TEST(prepared_override_rejects_complete_table_duplicates_ancestors_late_bad_and_ranks) {
    const std::array<FileOverrideEntry, 2> duplicate{{
        {0, "event/./file.bin", 0}, {0, "event\\file.bin", 41}
    }};
    const std::array<FileOverrideEntry, 3> parent_first{{
        {0, "event", 0}, {0, "event-extra", 7}, {0, "event/file.bin", 41}
    }};
    const std::array<FileOverrideEntry, 3> child_first{{
        {0, "event/file.bin", 41}, {0, "event-extra", 7}, {0, "event", 0}
    }};
    const std::array<FileOverrideEntry, 2> late_bad{{
        {0, "event/file.bin", 0}, {1, "unused/../bad.bin", 41}
    }};
    const std::array<FileOverrideEntry, 3> out_of_order{{
        {0, "event/file.bin", 0}, {2, "other.bin", 41}, {1, "unused.bin", 52}
    }};
    for (const auto entries : {std::span<const FileOverrideEntry>(duplicate),
                              std::span<const FileOverrideEntry>(parent_first),
                              std::span<const FileOverrideEntry>(child_first),
                              std::span<const FileOverrideEntry>(late_bad),
                              std::span<const FileOverrideEntry>(out_of_order)})
        rejected(prepare_file_overrides(entries, policy()),
                 FileOverridePreparationStatus::invalid_input);
}

TEST(prepared_override_rejects_invalid_policy_enums_and_alias_configuration) {
    const std::array<FileOverrideEntry, 1> entries{{{0, "a", 0}}};
    auto p = policy();
    p.case_policy = static_cast<OverrideCase>(99);
    rejected(prepare_file_overrides(entries, p), FileOverridePreparationStatus::invalid_input);
    p = policy();
    p.winner = static_cast<OverrideWinner>(99);
    rejected(prepare_file_overrides(entries, p), FileOverridePreparationStatus::invalid_input);
    const std::array<std::string_view, 2> duplicates{"data0:", "data0:"};
    p = policy();
    p.virtual_roots = duplicates;
    rejected(prepare_file_overrides(entries, p), FileOverridePreparationStatus::invalid_input);
    const std::array<std::string_view, 1> invalid_alias{"bad/root:"};
    p.virtual_roots = invalid_alias;
    rejected(prepare_file_overrides(entries, p), FileOverridePreparationStatus::invalid_input);
}

TEST(prepared_override_lookup_rejects_unknown_absolute_traversal_and_control_requests) {
    const std::array<FileOverrideEntry, 1> entries{{{0, "a", 0}}};
    const auto table = ready(entries, policy());
    for (const auto request : {"", "/a", "\\a", "other:/a", "data0:a", "../a",
                              "data0:/../a", "a/../b", "a.", "a ", "a\tb", "a\nb"})
        expect(table.lookup(request), OverrideStatus::invalid_input);
    const std::string nul("a\0b", 3);
    const std::string del("a\x7f" "b", 3);
    expect(table.lookup(nul), OverrideStatus::invalid_input);
    expect(table.lookup(del), OverrideStatus::invalid_input);
    expect(table.lookup("a"), OverrideStatus::selected, 0);
}

TEST(prepared_override_byte_exact_preserves_case_and_high_bytes) {
    const std::array<FileOverrideEntry, 3> entries{{
        {0, "Event.bin", 0}, {0, "event.bin", 41}, {0, "Caf\xc3\xa9/file.bin", 52}
    }};
    const auto table = ready(entries, policy());
    expect(table.lookup("Event.bin"), OverrideStatus::selected, 0);
    expect(table.lookup("event.bin"), OverrideStatus::selected, 41);
    expect(table.lookup("EVENT.bin"), OverrideStatus::passthrough);
    expect(table.lookup("Caf\xc3\xa9/file.bin"), OverrideStatus::selected, 52);
    expect(table.lookup("CAF\xc3\x89/file.bin"), OverrideStatus::passthrough);
}

TEST(prepared_override_ascii_fold_has_exact_aliases_and_unsupported_non_ascii) {
    const std::array<FileOverrideEntry, 1> entries{{{0, "Event/FILE.bin", 41}}};
    const auto table = ready(entries, policy(OverrideCase::ascii_insensitive));
    expect(table.lookup("data0:/event/file.BIN"), OverrideStatus::selected, 41);
    expect(table.lookup("DATA0:/event/file.bin"), OverrideStatus::invalid_input);
    expect(table.lookup("Caf\xc3\xa9/file.bin"), OverrideStatus::unsupported);
    const std::array<FileOverrideEntry, 2> non_ascii{{
        {0, "match.bin", 0}, {1, "Caf\xc3\xa9/file.bin", 52}
    }};
    rejected(prepare_file_overrides(non_ascii, policy(OverrideCase::ascii_insensitive)),
             FileOverridePreparationStatus::unsupported);
    const std::array<FileOverrideEntry, 2> fold_duplicate{{
        {0, "Event.bin", 0}, {0, "event.bin", 41}
    }};
    rejected(prepare_file_overrides(fold_duplicate, policy(OverrideCase::ascii_insensitive)),
             FileOverridePreparationStatus::invalid_input);
}

TEST(prepared_override_retains_raw_table_alias_bytes_and_adds_each_raw_request) {
    const std::array<std::string_view, 1> roots{"data0:"};
    const std::array<FileOverrideEntry, 1> entries{{{0, "./a", 0}}};
    const FileOverridePolicy p{roots, OverrideCase::byte_exact, OverrideWinner::last_package};
    FileOverrideLimits limits;
    limits.max_key_bytes = 10; // 3 raw table bytes + 6 alias bytes + 1 request byte.
    const auto table = ready(entries, p, limits);
    expect(table.lookup("a"), OverrideStatus::selected, 0);
    expect(table.lookup("./a"), OverrideStatus::limit_exceeded);
    expect(table.lookup("data0:/a"), OverrideStatus::limit_exceeded);
    expect(table.lookup("a"), OverrideStatus::selected, 0);
    limits.max_key_bytes = 9;
    const auto no_request_room = ready(entries, p, limits);
    expect(no_request_room.lookup("a"), OverrideStatus::limit_exceeded);
    limits.max_key_bytes = 8;
    rejected(prepare_file_overrides(entries, p, limits),
             FileOverridePreparationStatus::limit_exceeded);
}

TEST(prepared_override_empty_table_is_ready_and_passes_through_valid_requests) {
    const auto table = ready({}, policy());
    expect(table.lookup("data0:/missing.bin"), OverrideStatus::passthrough);
    expect(table.lookup("missing.bin"), OverrideStatus::passthrough);
    expect(table.lookup("../missing.bin"), OverrideStatus::invalid_input);
    auto p = policy();
    p.virtual_roots = {};
    FileOverrideLimits limits;
    limits.max_entries = 0;
    limits.max_virtual_roots = 0;
    const auto no_aliases = ready({}, p, limits);
    expect(no_aliases.lookup("missing.bin"), OverrideStatus::passthrough);
    expect(no_aliases.lookup("data0:/missing.bin"), OverrideStatus::invalid_input);
}

TEST(prepared_override_query_work_budget_is_bounded_and_does_not_poison_later_lookup) {
    const std::array<FileOverrideEntry, 1> entries{{{0, "event/file.bin", 0}}};
    const auto table = ready(entries, policy());
    expect(table.lookup("event/file.bin", 0), OverrideStatus::limit_exceeded);
    expect(table.lookup("event/file.bin", 1), OverrideStatus::limit_exceeded);
    expect(table.lookup("event/file.bin", 67108865), OverrideStatus::limit_exceeded);
    expect(table.lookup("event/file.bin", 4096), OverrideStatus::selected, 0);
    expect(table.lookup("event/file.bin"), OverrideStatus::selected, 0);
}

TEST(prepared_override_copies_keep_storage_and_moved_from_tables_reject_safely) {
    const std::array<FileOverrideEntry, 1> entries{{{0, "a", 0}}};
    auto original = ready(entries, policy());
    auto copy = original;
    auto moved = std::move(original);
    expect(original.lookup("a"), OverrideStatus::invalid_input);
    expect(copy.lookup("a"), OverrideStatus::selected, 0);
    expect(moved.lookup("data0:/a"), OverrideStatus::selected, 0);
    original = copy;
    expect(original.lookup("a"), OverrideStatus::selected, 0);
    auto assigned = ready({}, policy());
    assigned = std::move(moved);
    expect(moved.lookup("a"), OverrideStatus::invalid_input);
    expect(assigned.lookup("a"), OverrideStatus::selected, 0);
    expect(copy.lookup("a"), OverrideStatus::selected, 0);
}

TEST(prepared_override_preparation_respects_entry_alias_raw_path_and_work_bounds) {
    const std::array<FileOverrideEntry, 1> entries{{{0, "a", 0}}};
    FileOverrideLimits limits;
    limits.max_entries = 0;
    rejected(prepare_file_overrides(entries, policy(), limits),
             FileOverridePreparationStatus::limit_exceeded);
    limits = {};
    limits.max_virtual_roots = 1;
    rejected(prepare_file_overrides(entries, policy(), limits),
             FileOverridePreparationStatus::limit_exceeded);
    limits = {};
    limits.max_work_bytes = 0;
    rejected(prepare_file_overrides(entries, policy(), limits),
             FileOverridePreparationStatus::limit_exceeded);
    limits.max_work_bytes = 67108865;
    rejected(prepare_file_overrides(entries, policy(), limits),
             FileOverridePreparationStatus::limit_exceeded);
    const std::string too_long(4097, 'q');
    const std::array<FileOverrideEntry, 1> long_key{{{0, too_long, 0}}};
    rejected(prepare_file_overrides(long_key, policy()),
             FileOverridePreparationStatus::limit_exceeded);
}

TEST(prepared_override_lookup_enforces_4096_raw_request_ceiling) {
    const std::string at_limit(4096, 'q'), too_long(4097, 'q');
    const std::array<FileOverrideEntry, 1> entries{{{0, at_limit, 41}}};
    const auto table = ready(entries, policy());
    expect(table.lookup(at_limit), OverrideStatus::selected, 41);
    expect(table.lookup(too_long), OverrideStatus::limit_exceeded);
    expect(table.lookup(at_limit), OverrideStatus::selected, 41);
}
