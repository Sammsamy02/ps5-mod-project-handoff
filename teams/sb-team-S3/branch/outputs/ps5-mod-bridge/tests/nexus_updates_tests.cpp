// C++ twin of tests/updates_tests.py: the pure "newer file" resolver. No network, no clock, no files.
#include "modbridge/nexus.hpp"
#include "test.hpp"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace {
using modbridge::Json;

Json listed(std::initializer_list<std::uint64_t> ids, const Json& extra = Json::object()) {
    Json files = Json::array();
    for (const auto i : ids) {
        Json file = {{"file_id", i}, {"name", "File " + std::to_string(i)}, {"version", "v" + std::to_string(i)},
                     {"category_name", "MAIN"}, {"uploaded_timestamp", 1000 + i}, {"description", "private"}, {"size_kb", 1}};
        for (const auto& [key, value] : extra.items()) file[key] = value;
        files.push_back(std::move(file));
    }
    return files;
}
Json edge(std::uint64_t old_id, std::uint64_t new_id) {
    return {{"old_file_id", old_id}, {"new_file_id", new_id}, {"old_file_name", "File " + std::to_string(old_id)},
            {"new_file_name", "File " + std::to_string(new_id)}, {"uploaded_timestamp", 2000 + new_id}, {"uploaded_time", ""}};
}
Json chain(const std::vector<std::uint64_t>& ids) {
    Json edges = Json::array();
    for (std::size_t i = 0; i + 1 < ids.size(); ++i) edges.push_back(edge(ids[i], ids[i + 1]));
    return edges;
}
Json latest(std::uint64_t i) {
    return {{"file_id", i}, {"name", "File " + std::to_string(i)}, {"version", "v" + std::to_string(i)},
            {"category_name", "MAIN"}, {"uploaded_timestamp", 1000 + i}};
}
Json outcome(const char* status, const Json& file, std::uint64_t steps) {
    return {{"status", status}, {"latest", file}, {"steps", steps}};
}
Json unknown(std::uint64_t steps = 0) { return outcome("unknown", nullptr, steps); }
Json resolve(std::uint64_t file_id, const Json& files, const Json& updates) {
    return modbridge::nexus_resolve_file_update(file_id, files, updates);
}
Json join_rows(Json a, const Json& b) { for (const auto& item : b) a.push_back(item); return a; }
} // namespace

TEST(nexus_update_no_edge_and_still_listed_is_current_with_the_file_itself_as_latest) {
    CHECK_EQ(resolve(5, listed({5, 6}), Json::array()), outcome("current", latest(5), 0));
    // Edges that do not start at this file do not matter.
    CHECK_EQ(resolve(6, listed({5, 6, 9}), chain({5, 9})), outcome("current", latest(6), 0));
}

TEST(nexus_update_current_file_absent_from_the_list_and_no_edge_is_unknown) {
    CHECK_EQ(resolve(5, listed({6, 7}), Json::array()), unknown());
    CHECK_EQ(resolve(5, Json::array(), Json::array()), unknown());
    CHECK_EQ(resolve(5, listed({6}), chain({6, 7})), unknown());
}

TEST(nexus_update_a_chain_to_a_listed_file_is_newer_and_counts_steps) {
    CHECK_EQ(resolve(1, listed({1, 2}), chain({1, 2})), outcome("newer", latest(2), 1));
    CHECK_EQ(resolve(1, listed({2, 4}), chain({1, 3, 4})), outcome("newer", latest(4), 2));
    // The current file itself may already be gone from the list (archived); the chain still resolves.
    CHECK_EQ(resolve(1, listed({2, 3}), chain({1, 2, 3})).at("steps"), 2);
    // A middle file of the chain does not need to be listed, only the end.
    CHECK_EQ(resolve(1, listed({1, 4}), chain({1, 2, 3, 4})).at("status"), "newer");
}

TEST(nexus_update_a_chain_that_ends_at_an_unlisted_file_is_unknown) {
    CHECK_EQ(resolve(1, listed({1, 2}), chain({1, 2, 3})), unknown(2));
    CHECK_EQ(resolve(1, listed({1}), chain({1, 2})).at("status"), "unknown");
}

TEST(nexus_update_cycles_are_unknown) {
    for (const auto& edges : {chain({1, 1}), chain({1, 2, 1}), chain({1, 2, 3, 1}), chain({1, 2, 3, 2}),
                              Json::array({edge(1, 2), edge(2, 2)})}) {
        const auto result = resolve(1, listed({1, 2, 3}), edges);
        CHECK_EQ(result.at("status"), "unknown");
        CHECK(result.at("latest").is_null());
    }
}

TEST(nexus_update_a_branch_on_the_path_is_unknown_but_a_branch_elsewhere_is_ignored) {
    CHECK_EQ(resolve(1, listed({1, 2, 3}), Json::array({edge(1, 2), edge(1, 3)})).at("status"), "unknown");
    CHECK_EQ(resolve(1, listed({1, 2, 3, 4}), Json::array({edge(1, 2), edge(2, 3), edge(2, 4)})).at("status"), "unknown");
    // Two different olds merging into one new file is not a branch.
    CHECK_EQ(resolve(1, listed({1, 2, 3}), Json::array({edge(1, 3), edge(2, 3)})).at("status"), "newer");
    // A branch away from the followed chain does not affect it.
    CHECK_EQ(resolve(1, listed({1, 2, 8, 9}), Json::array({edge(1, 2), edge(8, 9), edge(8, 1)})).at("status"), "newer");
    CHECK_EQ(resolve(1, listed({1, 2, 8, 9}), Json::array({edge(1, 2), edge(7, 8), edge(7, 9)})).at("latest"), latest(2));
    // An identical repeated edge is one edge, not a branch.
    CHECK_EQ(resolve(1, listed({1, 2}), Json::array({edge(1, 2), edge(1, 2)})).at("status"), "newer");
}

TEST(nexus_update_the_step_cap_is_sixty_four_edges) {
    std::vector<std::uint64_t> ids;
    Json files = Json::array();
    for (std::uint64_t i = 1; i <= 66; ++i) { ids.push_back(i); files.push_back(listed({i})[0]); }
    const auto exactly = resolve(1, files, chain({ids.begin(), ids.begin() + 65}));
    CHECK_EQ(exactly.at("status"), "newer");
    CHECK_EQ(exactly.at("steps"), 64);
    CHECK_EQ(exactly.at("latest").at("file_id"), 65);
    const auto over = resolve(1, files, chain({ids.begin(), ids.begin() + 66}));
    CHECK_EQ(over.at("status"), "unknown");
    CHECK(over.at("latest").is_null());
    // A cap hit is never turned into "the last file we reached".
    CHECK_EQ(resolve(1, listed({1, 65}), chain({ids.begin(), ids.begin() + 66})).at("status"), "unknown");
}

TEST(nexus_update_a_long_acyclic_chain_does_not_recurse) {
    std::vector<std::uint64_t> ids;
    Json files = Json::array();
    for (std::uint64_t i = 1; i <= 10000; ++i) { ids.push_back(i); files.push_back({{"file_id", i}}); }
    CHECK_EQ(resolve(1, files, chain(ids)).at("status"), "unknown");
    CHECK_EQ(resolve(9990, files, chain(ids)).at("status"), "newer");
}

TEST(nexus_update_result_is_deterministic_and_independent_of_input_order) {
    auto files = listed({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
    auto edges = join_rows(chain({1, 4, 7, 11}), Json::array({edge(2, 3), edge(5, 6), edge(9, 10)}));
    const auto expected = resolve(1, files, edges);
    CHECK_EQ(expected, outcome("newer", latest(11), 3));
    auto file_rows = files.get<std::vector<Json>>();
    auto edge_rows = edges.get<std::vector<Json>>();
    unsigned seed = 7;
    const auto next = [&seed]() { seed = seed * 1103515245U + 12345U; return (seed >> 16) & 0x7fffU; };
    for (int round = 0; round < 50; ++round) {
        for (std::size_t i = file_rows.size(); i > 1; --i) std::swap(file_rows[i - 1], file_rows[next() % i]);
        for (std::size_t i = edge_rows.size(); i > 1; --i) std::swap(edge_rows[i - 1], edge_rows[next() % i]);
        CHECK_EQ(resolve(1, Json(file_rows), Json(edge_rows)), expected);
    }
}

TEST(nexus_update_inputs_are_not_modified) {
    const auto files = listed({1, 2});
    const auto edges = chain({1, 2});
    const auto before = Json::array({files, edges});
    auto result = resolve(1, files, edges);
    CHECK_EQ(Json::array({files, edges}), before);
    result["latest"]["name"] = "changed";
    CHECK_EQ(files[1].at("name"), "File 2");
}

TEST(nexus_update_latest_exposes_only_the_summary_with_typed_optional_fields) {
    const auto result = resolve(1, listed({1, 2}), chain({1, 2}));
    CHECK_EQ(result.size(), 3U);
    CHECK_EQ(result.at("latest").size(), 5U);
    const Json sparse = Json::array({{{"file_id", 1}, {"name", "One"}, {"version", "1"}},
        {{"file_id", 2}, {"name", 5}, {"version", nullptr}, {"category_name", 7}, {"uploaded_timestamp", true}}});
    CHECK_EQ(resolve(1, sparse, chain({1, 2})).at("latest"),
             Json({{"file_id", 2}, {"name", nullptr}, {"version", nullptr}, {"category_name", nullptr}, {"uploaded_timestamp", nullptr}}));
    CHECK_EQ(resolve(1, Json::array({sparse[0]}), Json::array()).at("latest"),
             Json({{"file_id", 1}, {"name", "One"}, {"version", "1"}, {"category_name", nullptr}, {"uploaded_timestamp", nullptr}}));
}

TEST(nexus_update_malformed_input_is_unknown_and_never_throws) {
    const auto files = listed({1, 2});
    const auto edges = chain({1, 2});
    // The file id argument is an unsigned integer; the JSON-typed variants of the Python test are the rows below.
    CHECK_EQ(modbridge::nexus_resolve_file_update(0, files, edges), unknown());
    CHECK_EQ(modbridge::nexus_resolve_file_update(1ULL << 63, files, edges), unknown());
    CHECK_EQ(modbridge::nexus_resolve_file_update(~0ULL, files, edges), unknown());
    const std::vector<std::pair<Json, Json>> cases{
        {nullptr, edges}, {files, nullptr}, {"files", edges}, {files, Json{{"old_file_id", 1}}},
        {join_rows(files, Json::array({"x"})), edges}, {join_rows(files, Json::array({{{"file_id", 0}}})), edges},
        {join_rows(files, Json::array({{{"file_id", true}}})), edges}, {join_rows(files, Json::array({Json::object()})), edges},
        {files, join_rows(edges, Json::array({{{"old_file_id", 1}}}))}, {files, join_rows(edges, Json::array({7}))},
        {files, Json::array({{{"old_file_id", 1}, {"new_file_id", "2"}}})}, {files, Json::array({{{"old_file_id", 1}, {"new_file_id", 0}}})},
        {files, Json::array({{{"old_file_id", true}, {"new_file_id", 2}}})},
        // Duplicate listed ids cannot be trusted either.
        {join_rows(files, listed({2})), edges}};
    for (const auto& [rows, links] : cases) CHECK_EQ(resolve(1, rows, links), unknown());
}

TEST(nexus_update_extra_keys_in_inputs_are_ignored) {
    const auto files = listed({1, 2}, {{"key", "must-not-leak"}});
    auto edges = chain({1, 2});
    for (auto& item : edges) item["key"] = "must-not-leak";
    const auto result = resolve(1, files, edges);
    CHECK_EQ(result, outcome("newer", latest(2), 1));
    CHECK(result.dump().find("must-not-leak") == std::string::npos);
}

// ---- the files.json decoder that feeds the resolver (twin of NexusClient._parse_file_updates) ----
namespace {
constexpr auto file_updates_kind = modbridge::NexusRecordKind::FileUpdates;
Json file_row(std::uint64_t id) {
    return {{"file_id", id}, {"name", "File"}, {"file_name", "file.zip"}, {"version", "1"}, {"size_kb", 3}};
}
Json links_row(std::uint64_t old_id, std::uint64_t new_id) {
    return {{"old_file_id", old_id}, {"new_file_id", new_id}, {"old_file_name", "Old"}, {"new_file_name", "New"},
            {"uploaded_timestamp", 1700000000}, {"uploaded_time", "2024-01-01T00:00:00.000+00:00"}};
}
Json decode_files(const Json& input) {
    const auto text = input.dump();
    return modbridge::decode_nexus_metadata({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()},
                                            file_updates_kind, "future_game-2", 71);
}
void malformed(const Json& result) {
    CHECK_EQ(result.at("status"), "malformed");
    CHECK(!result.contains("updates"));
    CHECK(!result.contains("records"));
}
}

TEST(nexus_file_updates_kind_uses_the_files_route) {
    const auto url = modbridge::nexus_metadata_url(file_updates_kind, "future_game-2", 71);
    CHECK(url.has_value());
    CHECK_EQ(*url, "https://api.nexusmods.com/v1/games/future_game-2/mods/71/files");
    CHECK(!modbridge::nexus_metadata_url(file_updates_kind, "future_game-2", 0).has_value());
    CHECK(!modbridge::nexus_metadata_url(file_updates_kind, "", 71).has_value());
}

TEST(nexus_file_updates_decode_files_and_links) {
    const auto result = decode_files({{"files", Json::array({file_row(1), file_row(2)})},
                                      {"file_updates", Json::array({links_row(1, 2)})}});
    CHECK_EQ(result.at("status"), "decoded");
    CHECK_EQ(result.at("updates").at("files").size(), 2U);
    CHECK_EQ(result.at("updates").at("files")[0].at("file_id"), 1);
    CHECK_EQ(result.at("updates").at("file_updates"), Json::array({links_row(1, 2)}));
    // Identical repeated links collapse to one; extra provider keys are dropped.
    auto repeated_link = links_row(1, 2);
    repeated_link["extra"] = "dropped";
    const auto twice = decode_files({{"files", Json::array({file_row(1)})},
                                     {"file_updates", Json::array({links_row(1, 2), links_row(1, 2), repeated_link})}});
    CHECK_EQ(twice.at("updates").at("file_updates").size(), 1U);
    CHECK_EQ(decode_files({{"files", Json::array()}, {"file_updates", Json::array()}}).at("status"), "decoded");
}

TEST(nexus_file_updates_malformed_or_absent_links_are_malformed_never_no_links) {
    const auto files = Json::array({file_row(1)});
    malformed(decode_files({{"files", files}}));                                  // absent
    malformed(decode_files({{"files", files}, {"file_updates", nullptr}}));
    malformed(decode_files({{"files", files}, {"file_updates", Json::object()}}));
    malformed(decode_files({{"file_updates", Json::array()}}));                   // no files
    for (const char* field : {"old_file_id", "new_file_id", "old_file_name", "new_file_name", "uploaded_timestamp", "uploaded_time"}) {
        auto row = links_row(1, 2);
        row.erase(field);
        malformed(decode_files({{"files", files}, {"file_updates", Json::array({row})}}));
    }
    const std::vector<std::pair<const char*, Json>> bad{{"old_file_id", 0}, {"old_file_id", "1"}, {"old_file_id", true},
        {"new_file_id", -1}, {"new_file_id", 1.5}, {"uploaded_timestamp", 0}, {"uploaded_timestamp", 4102444801LL},
        {"uploaded_timestamp", "1"}, {"uploaded_timestamp", true}, {"uploaded_timestamp", 1.0}, {"old_file_name", 5},
        {"new_file_name", nullptr}, {"uploaded_time", 5}, {"uploaded_time", std::string(65, 'x')},
        {"old_file_name", std::string(1025, 'x')}};
    for (const auto& [field, value] : bad) {
        auto row = links_row(1, 2);
        row[field] = value;
        malformed(decode_files({{"files", files}, {"file_updates", Json::array({row})}}));
    }
    malformed(decode_files({{"files", files}, {"file_updates", Json::array({7})}}));
    malformed(decode_files({{"files", files}, {"file_updates", Json::array({links_row(1, 2), "x"})}}));
    // The same bad row is fine to keep at its limits.
    auto limits = links_row(1, 2);
    limits["uploaded_timestamp"] = 4102444800LL;
    limits["uploaded_time"] = std::string(64, 'x');
    CHECK_EQ(decode_files({{"files", files}, {"file_updates", Json::array({limits})}}).at("status"), "decoded");
}

TEST(nexus_file_updates_decode_feeds_the_resolver) {
    const auto result = decode_files({{"files", Json::array({file_row(1), file_row(2)})},
                                      {"file_updates", Json::array({links_row(1, 2)})}});
    const auto answer = modbridge::nexus_resolve_file_update(1, result.at("updates").at("files"), result.at("updates").at("file_updates"));
    CHECK_EQ(answer.at("status"), "newer");
    CHECK_EQ(answer.at("latest").at("file_id"), 2);
    CHECK_EQ(answer.at("steps"), 1);
}
