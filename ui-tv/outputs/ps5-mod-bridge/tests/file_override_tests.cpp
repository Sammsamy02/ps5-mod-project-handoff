#include "test.hpp"
#include "modbridge/core.hpp"

#include <array>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

using namespace modbridge;

namespace {
const std::array<std::string_view, 2> roots{"data0:", "data1:"};

FileOverridePolicy policy(
    OverrideCase case_policy = OverrideCase::byte_exact,
    OverrideWinner winner = OverrideWinner::last_package) {
    return {roots, case_policy, winner};
}

void expect(const FileOverrideChoice& choice, OverrideStatus status,
            std::optional<std::uint64_t> source = std::nullopt) {
    CHECK_EQ(static_cast<int>(choice.status), static_cast<int>(status));
    CHECK(choice.opaque_source_index == source);
}
} // namespace

TEST(file_override_recognized_root_and_relative_separators_are_equivalent) {
    const std::array<FileOverrideEntry, 1> entries{{
        {0, "event/example.bin", 0}
    }};
    for (const auto request : {
             "data0:/event/example.bin", "data0:\\event\\example.bin",
             "data1:/event/example.bin", "event/example.bin",
             "event\\example.bin", "./event//./example.bin"}) {
        expect(choose_file_override(request, entries, policy()),
               OverrideStatus::selected, 0);
    }
}

TEST(file_override_normalizes_table_keys_without_host_path_rules) {
    const std::array<FileOverrideEntry, 1> entries{{
        {7, ".\\event\\.\\example.bin", 23}
    }};
    expect(choose_file_override("data0:/event/example.bin", entries, policy()),
           OverrideStatus::selected, 23);
}

TEST(file_override_first_package_winner_uses_supplied_rank_order) {
    const std::array<FileOverrideEntry, 3> entries{{
        {2, "event/example.bin", 40},
        {5, "event/example.bin", 7},
        {9, "event/example.bin", 0}
    }};
    expect(choose_file_override("event/example.bin", entries,
               policy(OverrideCase::byte_exact, OverrideWinner::first_package)),
           OverrideStatus::selected, 40);
}

TEST(file_override_last_package_winner_can_have_source_index_zero) {
    const std::array<FileOverrideEntry, 3> entries{{
        {2, "event/example.bin", 40},
        {5, "event/example.bin", 7},
        {9, "event/example.bin", 0}
    }};
    expect(choose_file_override("event/example.bin", entries, policy()),
           OverrideStatus::selected, 0);
}

TEST(file_override_missing_key_and_empty_table_pass_through_without_source) {
    const std::array<FileOverrideEntry, 1> entries{{{0, "present.bin", 13}}};
    expect(choose_file_override("missing.bin", entries, policy()),
           OverrideStatus::passthrough);
    expect(choose_file_override("data0:/missing.bin", {}, policy()),
           OverrideStatus::passthrough);
}

TEST(file_override_bad_unused_row_invalidates_an_early_first_winner) {
    const std::array<FileOverrideEntry, 2> entries{{
        {0, "match.bin", 4}, {1, "unused/../bad.bin", 8}
    }};
    expect(choose_file_override("match.bin", entries,
               policy(OverrideCase::byte_exact, OverrideWinner::first_package)),
           OverrideStatus::invalid_input);
}

TEST(file_override_bad_table_is_invalid_even_when_request_does_not_match) {
    const std::array<FileOverrideEntry, 2> entries{{
        {0, "valid.bin", 4}, {1, "data0:/invalid.bin", 8}
    }};
    expect(choose_file_override("missing.bin", entries, policy()),
           OverrideStatus::invalid_input);
}

TEST(file_override_normalized_duplicate_within_one_package_is_invalid) {
    const std::array<FileOverrideEntry, 2> entries{{
        {3, "event/./example.bin", 8}, {3, "event\\example.bin", 9}
    }};
    expect(choose_file_override("event/example.bin", entries, policy()),
           OverrideStatus::invalid_input);
}

TEST(file_override_ancestor_collision_within_one_package_is_invalid_in_both_orders) {
    const std::array<FileOverrideEntry, 2> parent_first{{
        {3, "event", 8}, {3, "event/example.bin", 9}
    }};
    const std::array<FileOverrideEntry, 2> child_first{{
        {3, "event/example.bin", 9}, {3, "event", 8}
    }};
    expect(choose_file_override("event/example.bin", parent_first, policy()),
           OverrideStatus::invalid_input);
    expect(choose_file_override("event/example.bin", child_first, policy()),
           OverrideStatus::invalid_input);
}

TEST(file_override_ancestor_hidden_by_lexical_neighbor_invalidates_a_missing_request) {
    const std::array<FileOverrideEntry, 3> parent_first{{
        {3, "event", 8}, {3, "event-extra", 10}, {3, "event/file.bin", 9}
    }};
    const std::array<FileOverrideEntry, 3> child_first{{
        {3, "event/file.bin", 9}, {3, "event-extra", 10}, {3, "event", 8}
    }};
    expect(choose_file_override("missing.bin", parent_first, policy()),
           OverrideStatus::invalid_input);
    expect(choose_file_override("missing.bin", child_first, policy()),
           OverrideStatus::invalid_input);
}

TEST(file_override_common_component_prefix_is_not_an_ancestor_collision) {
    const std::array<FileOverrideEntry, 2> entries{{
        {3, "event", 8}, {3, "events/example.bin", 9}
    }};
    expect(choose_file_override("events/example.bin", entries, policy()),
           OverrideStatus::selected, 9);
}

TEST(file_override_cross_package_ancestor_keys_are_not_same_package_collisions) {
    const std::array<FileOverrideEntry, 2> entries{{
        {3, "event", 8}, {4, "event/example.bin", 9}
    }};
    expect(choose_file_override("event/example.bin", entries, policy()),
           OverrideStatus::selected, 9);
}

TEST(file_override_out_of_order_unused_package_rank_invalidates_selection) {
    const std::array<FileOverrideEntry, 3> entries{{
        {2, "match.bin", 8}, {5, "other.bin", 9}, {4, "unused.bin", 10}
    }};
    expect(choose_file_override("match.bin", entries,
               policy(OverrideCase::byte_exact, OverrideWinner::first_package)),
           OverrideStatus::invalid_input);
}

TEST(file_override_byte_exact_and_ascii_fold_have_distinct_matching_rules) {
    const std::array<FileOverrideEntry, 1> entries{{
        {0, "Event/EXAMPLE.bin", 27}
    }};
    expect(choose_file_override("event/example.bin", entries, policy()),
           OverrideStatus::passthrough);
    expect(choose_file_override("Event/EXAMPLE.bin", entries, policy()),
           OverrideStatus::selected, 27);
    expect(choose_file_override("data0:/event/example.BIN", entries,
               policy(OverrideCase::ascii_insensitive)),
           OverrideStatus::selected, 27);
}

TEST(file_override_root_aliases_stay_exact_under_ascii_path_folding) {
    const std::array<FileOverrideEntry, 1> entries{{{0, "event.bin", 4}}};
    expect(choose_file_override("DATA0:/EVENT.bin", entries,
               policy(OverrideCase::ascii_insensitive)),
           OverrideStatus::invalid_input);
}

TEST(file_override_case_distinct_keys_can_coexist_in_byte_exact_package) {
    const std::array<FileOverrideEntry, 2> entries{{
        {0, "Event.bin", 11}, {0, "event.bin", 12}
    }};
    expect(choose_file_override("Event.bin", entries, policy()),
           OverrideStatus::selected, 11);
    expect(choose_file_override("event.bin", entries, policy()),
           OverrideStatus::selected, 12);
    expect(choose_file_override("event.bin", entries,
               policy(OverrideCase::ascii_insensitive)),
           OverrideStatus::invalid_input);
}

TEST(file_override_ascii_fold_detects_same_package_ancestor_collision) {
    const std::array<FileOverrideEntry, 2> entries{{
        {0, "EVENT", 11}, {0, "event/example.bin", 12}
    }};
    expect(choose_file_override("event/example.bin", entries,
               policy(OverrideCase::ascii_insensitive)),
           OverrideStatus::invalid_input);
}

TEST(file_override_byte_exact_preserves_non_ascii_bytes_literally) {
    const std::array<FileOverrideEntry, 1> entries{{
        {0, "Caf\xc3\xa9/file.bin", 31}
    }};
    expect(choose_file_override("Caf\xc3\xa9/file.bin", entries, policy()),
           OverrideStatus::selected, 31);
    expect(choose_file_override("CAF\xc3\x89/file.bin", entries, policy()),
           OverrideStatus::passthrough);
    const std::array<FileOverrideEntry, 1> raw_bytes{{{0, "\xff/file.bin", 32}}};
    expect(choose_file_override("\xff/file.bin", raw_bytes, policy()),
           OverrideStatus::selected, 32);
}

TEST(file_override_ascii_policy_rejects_non_ascii_request_as_unsupported) {
    const std::array<FileOverrideEntry, 1> entries{{{0, "file.bin", 31}}};
    expect(choose_file_override("Caf\xc3\xa9/file.bin", entries,
               policy(OverrideCase::ascii_insensitive)),
           OverrideStatus::unsupported);
}

TEST(file_override_ascii_policy_rejects_non_ascii_unused_table_key) {
    const std::array<FileOverrideEntry, 2> entries{{
        {0, "match.bin", 31}, {1, "Caf\xc3\xa9/file.bin", 32}
    }};
    expect(choose_file_override("match.bin", entries,
               policy(OverrideCase::ascii_insensitive, OverrideWinner::first_package)),
           OverrideStatus::unsupported);
}

TEST(file_override_unknown_absolute_traversal_and_ambiguous_requests_are_invalid) {
    const std::array<FileOverrideEntry, 1> entries{{{0, "safe.bin", 31}}};
    for (const auto request : {
             "", "/safe.bin", "\\safe.bin", "//server/safe.bin", "C:/safe.bin",
             "other:/safe.bin", "data0:safe.bin", "../safe.bin", "a/../safe.bin",
             "data0:/../safe.bin", "a:b", "safe.bin.", "safe.bin ",
             "a./safe.bin", "a /safe.bin", ".", "./"}) {
        expect(choose_file_override(request, entries, policy()),
               OverrideStatus::invalid_input);
    }
}

TEST(file_override_controls_and_del_are_rejected_in_requests_and_unused_keys) {
    for (const unsigned char byte : {0u, 1u, 9u, 10u, 13u, 31u, 127u}) {
        std::string bad = "a";
        bad.push_back(static_cast<char>(byte));
        bad += "b.bin";
        expect(choose_file_override(bad, {}, policy()),
               OverrideStatus::invalid_input);
        const std::array<FileOverrideEntry, 2> entries{{
            {0, "match.bin", 0}, {1, bad, 1}
        }};
        expect(choose_file_override("match.bin", entries, policy()),
               OverrideStatus::invalid_input);
    }
}

TEST(file_override_table_keys_must_be_safe_relative_paths) {
    for (const auto key : {
             "", "/file.bin", "\\file.bin", "data0:/file.bin", "a:b",
             "../file.bin", "a/../file.bin", "file.bin.", "file.bin ",
             "a./file.bin", "a /file.bin", "."}) {
        const std::array<FileOverrideEntry, 2> entries{{
            {0, "match.bin", 0}, {1, key, 1}
        }};
        expect(choose_file_override("match.bin", entries, policy()),
               OverrideStatus::invalid_input);
    }
}

TEST(file_override_alias_definitions_require_exact_ascii_root_grammar) {
    for (const auto alias : {"", ":", "data0", "bad/root:", "bad\\root:",
                             "data.0:", "data 0:", "d\xc3\xa1ta:", "data0::"}) {
        const std::array<std::string_view, 1> aliases{alias};
        auto p = policy();
        p.virtual_roots = aliases;
        expect(choose_file_override("file.bin", {}, p),
               OverrideStatus::invalid_input);
    }
}

TEST(file_override_duplicate_aliases_invalidate_even_a_relative_request) {
    const std::array<std::string_view, 2> aliases{"data0:", "data0:"};
    auto p = policy();
    p.virtual_roots = aliases;
    expect(choose_file_override("file.bin", {}, p), OverrideStatus::invalid_input);
}

TEST(file_override_no_aliases_still_allow_relative_keys) {
    const std::array<FileOverrideEntry, 1> entries{{{0, "file.bin", 0}}};
    auto p = policy();
    p.virtual_roots = {};
    expect(choose_file_override("file.bin", entries, p),
           OverrideStatus::selected, 0);
    expect(choose_file_override("data0:/file.bin", entries, p),
           OverrideStatus::invalid_input);
}

TEST(file_override_entry_limit_is_inclusive_and_zero_allows_empty_table) {
    const std::array<FileOverrideEntry, 2> entries{{
        {0, "a", 0}, {1, "b", 1}
    }};
    FileOverrideLimits limits;
    limits.max_entries = 1;
    expect(choose_file_override("a", entries, policy(), limits),
           OverrideStatus::limit_exceeded);
    limits.max_entries = 2;
    expect(choose_file_override("a", entries, policy(), limits),
           OverrideStatus::selected, 0);
    limits.max_entries = 0;
    expect(choose_file_override("a", {}, policy(), limits),
           OverrideStatus::passthrough);
    expect(choose_file_override("a", std::span(entries).first(1), policy(), limits),
           OverrideStatus::limit_exceeded);
}

TEST(file_override_aggregate_key_budget_includes_request_aliases_and_unused_rows) {
    const std::array<std::string_view, 1> aliases{"data0:"};
    const std::array<FileOverrideEntry, 2> entries{{{0, "a", 0}, {1, "b", 1}}};
    auto p = policy();
    p.virtual_roots = aliases;
    FileOverrideLimits limits;
    limits.max_key_bytes = 9; // 1 request + 6 alias + 1 matching + 1 unused.
    expect(choose_file_override("a", entries, p, limits),
           OverrideStatus::selected, 0);
    limits.max_key_bytes = 8;
    expect(choose_file_override("a", entries, p, limits),
           OverrideStatus::limit_exceeded);
    limits.max_key_bytes = 0;
    p.virtual_roots = {};
    expect(choose_file_override("a", {}, p, limits),
           OverrideStatus::limit_exceeded);
}

TEST(file_override_work_budget_refuses_zero_or_tiny_work_without_a_choice) {
    const std::array<FileOverrideEntry, 1> entries{{{0, "file.bin", 0}}};
    FileOverrideLimits limits;
    limits.max_work_bytes = 0;
    expect(choose_file_override("file.bin", entries, policy(), limits),
           OverrideStatus::limit_exceeded);
    limits.max_work_bytes = 1;
    expect(choose_file_override("file.bin", entries, policy(), limits),
           OverrideStatus::limit_exceeded);
    limits.max_work_bytes = 4096;
    expect(choose_file_override("file.bin", entries, policy(), limits),
           OverrideStatus::selected, 0);
}

TEST(file_override_root_limit_counts_unused_aliases) {
    FileOverrideLimits limits;
    limits.max_virtual_roots = 1;
    expect(choose_file_override("file.bin", {}, policy(), limits),
           OverrideStatus::limit_exceeded);
    limits.max_virtual_roots = 2;
    expect(choose_file_override("file.bin", {}, policy(), limits),
           OverrideStatus::passthrough);
    limits.max_virtual_roots = 0;
    auto p = policy();
    p.virtual_roots = {};
    expect(choose_file_override("file.bin", {}, p, limits),
           OverrideStatus::passthrough);
}

TEST(file_override_raw_path_ceiling_accepts_4096_bytes_and_rejects_4097) {
    const std::string at_limit(4096, 'q');
    const std::array<FileOverrideEntry, 1> entries{{{0, at_limit, 17}}};
    expect(choose_file_override(at_limit, entries, policy()),
           OverrideStatus::selected, 17);
    const std::string too_long(4097, 'q');
    expect(choose_file_override(too_long, entries, policy()),
           OverrideStatus::limit_exceeded);
    const std::array<FileOverrideEntry, 2> bad_table{{
        {0, "match.bin", 0}, {1, too_long, 1}
    }};
    expect(choose_file_override("match.bin", bad_table, policy()),
           OverrideStatus::limit_exceeded);
    const std::string long_alias = std::string(4096, 'q') + ':';
    const std::array<std::string_view, 1> aliases{long_alias};
    auto p = policy();
    p.virtual_roots = aliases;
    expect(choose_file_override("file.bin", {}, p),
           OverrideStatus::limit_exceeded);
}

TEST(file_override_all_limits_can_only_tighten_the_built_in_ceilings) {
    auto p = policy();
    p.virtual_roots = {};
    const std::array<FileOverrideEntry, 1> entries{{{0, "a", 0}}};
    FileOverrideLimits limits;
    limits.max_entries = 10001;
    expect(choose_file_override("a", entries, p, limits),
           OverrideStatus::limit_exceeded);
    limits = {};
    limits.max_key_bytes = 8388609;
    expect(choose_file_override("a", entries, p, limits),
           OverrideStatus::limit_exceeded);
    limits = {};
    limits.max_work_bytes = 67108865;
    expect(choose_file_override("a", entries, p, limits),
           OverrideStatus::limit_exceeded);
    limits = {};
    limits.max_virtual_roots = 65;
    expect(choose_file_override("a", entries, p, limits),
           OverrideStatus::limit_exceeded);
    limits = {};
    limits.max_key_bytes = std::numeric_limits<std::uint64_t>::max();
    expect(choose_file_override("a", entries, p, limits),
           OverrideStatus::limit_exceeded);
}

TEST(file_override_invalid_policy_enums_publish_no_source) {
    const std::array<FileOverrideEntry, 1> entries{{{0, "a", 0}}};
    auto p = policy();
    p.case_policy = static_cast<OverrideCase>(99);
    expect(choose_file_override("a", entries, p), OverrideStatus::invalid_input);
    p = policy();
    p.winner = static_cast<OverrideWinner>(99);
    expect(choose_file_override("a", entries, p), OverrideStatus::invalid_input);
}
