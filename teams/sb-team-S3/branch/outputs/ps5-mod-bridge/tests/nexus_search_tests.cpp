// C++ twin of tests/nexus_search_tests.py (requests, text rules and the strict page decoder). Owned fixtures only:
// nothing here contacts Nexus. The query constants are typed out below, so a silent edit of one fails.
#include "modbridge/nexus.hpp"
#include "modbridge/nexus_search_query.h"
#include "test.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {
using modbridge::Json;
using modbridge::NexusBrowseSort;
constexpr std::string_view game = "arbitrarygame";

constexpr std::string_view hand_query =
    "query ModBridgeSearch($gameDomainName: String!, $name: String!, $count: Int!, $offset: Int!) { "
    "mods(viewUploaderHidden: false, viewUserBlockedContent: false, "
    "filter: {op: AND, gameDomainName: [{value: $gameDomainName, op: EQUALS}], "
    "name: [{value: $name, op: WILDCARD}], status: [{value: \"published\", op: EQUALS}]}, "
    "sort: [{relevance: {direction: DESC}}, {downloads: {direction: DESC}}], "
    "offset: $offset, count: $count) { "
    "nodes { modId name summary version author adultContent status uploader { name } game { id domainName } } "
    "nodesCount totalCount } }";
constexpr std::string_view browse_head =
    "query ModBridgeBrowse($gameDomainName: String!, $count: Int!, $offset: Int!) { "
    "mods(viewUploaderHidden: false, viewUserBlockedContent: false, "
    "filter: {op: AND, gameDomainName: [{value: $gameDomainName, op: EQUALS}], "
    "status: [{value: \"published\", op: EQUALS}]}, sort: [";
constexpr std::string_view browse_tail =
    "], offset: $offset, count: $count) { "
    "nodes { modId name summary version author adultContent status uploader { name } game { id domainName } } "
    "nodesCount totalCount } }";

std::string browse_query(std::string_view sort) {
    return std::string(browse_head) + std::string(sort) + std::string(browse_tail);
}
const std::array<std::pair<std::string_view, std::string_view>, 4> browse_sorts{{
    {"downloads", "{downloads: {direction: DESC}}, {name: {direction: ASC}}"},
    {"endorsements", "{endorsements: {direction: DESC}}, {name: {direction: ASC}}"},
    {"updated", "{updatedAt: {direction: DESC}}, {name: {direction: ASC}}"},
    {"name", "{name: {direction: ASC}}"}}};

// The request body the way json.dumps(ensure_ascii=True, separators=(",", ":")) writes it.
std::string json_text(std::string_view query) {
    std::string out = "\"";
    for (const char c : query) {
        if (c == '"') out += "\\\"";
        else out.push_back(c);
    }
    return out + "\"";
}
std::string search_text_or_empty(std::string_view text) {
    std::string normalized;
    return modbridge::nexus_search_text(text, normalized) ? normalized : std::string("\x01<invalid>");
}
bool search_body(std::string_view domain, std::string_view text, std::uint64_t page, std::string& body) {
    std::array<char, 4096> out{};
    std::size_t written = 0;
    const bool ok = modbridge::nexus_search_body(domain, text, page, out, written);
    body.assign(out.data(), written);
    return ok;
}
bool browse_body(std::string_view domain, NexusBrowseSort sort, std::uint64_t page, std::string& body) {
    std::array<char, 4096> out{};
    std::size_t written = 0;
    const bool ok = modbridge::nexus_browse_body(domain, sort, page, out, written);
    body.assign(out.data(), written);
    return ok;
}
std::string repeated(std::string_view unit, std::size_t count) {
    std::string result;
    for (std::size_t i = 0; i < count; ++i) result += unit;
    return result;
}

Json node(std::uint64_t mod_id = 17, const Json& changes = Json::object()) {
    Json value = {{"modId", mod_id}, {"name", "Owned fixture"}, {"summary", "Synthetic test data"}, {"version", "1.2"},
                  {"author", "Fixture author"}, {"adultContent", false}, {"status", "published"},
                  {"uploader", {{"name", "Uploader"}}}, {"game", {{"id", 444}, {"domainName", std::string(game)}}}};
    for (const auto& [key, item] : changes.items()) value[key] = item;
    return value;
}
Json nodes_of(std::uint64_t first, std::uint64_t last) {
    Json rows = Json::array();
    for (auto i = first; i <= last; ++i) rows.push_back(node(i));
    return rows;
}
Json reply(const Json& nodes, std::int64_t total = -1, const Json& changes = Json::object()) {
    Json mods = {{"nodes", nodes}, {"nodesCount", nodes.size()},
                 {"totalCount", total < 0 ? nodes.size() : static_cast<std::uint64_t>(total)}};
    for (const auto& [key, item] : changes.items()) mods[key] = item;
    return {{"data", {{"mods", std::move(mods)}}}};
}
Json decode_text(std::string_view text, std::string_view domain = game, std::uint64_t page = 1) {
    return modbridge::decode_nexus_listing(
        {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()}, domain, page);
}
Json decode(const Json& value, std::string_view domain = game, std::uint64_t page = 1) {
    return decode_text(value.dump(), domain, page);
}
void failed(const Json& result, std::string_view status = "malformed") {
    CHECK_EQ(result.at("status"), status);
    CHECK_EQ(result.at("scope"), "nexus_metadata_only");
    CHECK_EQ(result.at("installation_supported"), false);
    CHECK(!result.contains("records"));
    CHECK(!result.contains("total"));
    CHECK(!result.contains("profile"));
    CHECK_EQ(result.at("issues").size(), 1U);
    CHECK_EQ(result.at("issues")[0].size(), 1U);
    CHECK(result.dump().find("SecretMarker") == std::string::npos);
}
} // namespace

// ---------------------------------------------------------------- the fixed requests

TEST(nexus_search_query_is_one_fixed_constant_with_four_typed_variables) {
    CHECK_EQ(std::string_view(MB_NEXUS_SEARCH_GRAPHQL_QUERY), hand_query);
    CHECK_EQ(std::string_view(MB_NEXUS_LISTING_GRAPHQL_URL), "https://api.nexusmods.com/v2/graphql");
    CHECK_EQ(MB_NEXUS_LISTING_PAGE_SIZE, 20U);
    CHECK_EQ(MB_NEXUS_LISTING_MAX_PAGE, 50U);
    CHECK_EQ(MB_NEXUS_SEARCH_TEXT_MIN, 2U);
    CHECK_EQ(MB_NEXUS_SEARCH_TEXT_MAX, 100U);
    // Read only: no mutation, no subscription, one operation, every variable used where it belongs.
    CHECK(hand_query.starts_with("query "));
    CHECK(hand_query.find("mutation") == std::string_view::npos);
    CHECK(hand_query.find("subscription") == std::string_view::npos);
    for (const std::string_view use : {"{value: $gameDomainName, op: EQUALS}", "{value: $name, op: WILDCARD}",
                                       "offset: $offset, count: $count"}) {
        const auto first = hand_query.find(use);
        CHECK(first != std::string_view::npos);
        CHECK(hand_query.find(use, first + 1) == std::string_view::npos);
    }
}

TEST(nexus_search_body_is_the_constant_query_plus_typed_variables_byte_for_byte) {
    std::string body;
    CHECK(search_body("skyrim", "better ranger", 2, body));
    const auto expected = "{\"query\":" + json_text(hand_query) +
        ",\"variables\":{\"gameDomainName\":\"skyrim\",\"name\":\"better ranger\",\"count\":20,\"offset\":20}}";
    CHECK_EQ(body, expected);
    const auto record = Json::parse(body);
    CHECK_EQ(record.size(), 2U);
    CHECK_EQ(record.at("query"), std::string(hand_query));
    CHECK_EQ(record.at("variables"), Json({{"gameDomainName", "skyrim"}, {"name", "better ranger"}, {"count", 20}, {"offset", 20}}));
    CHECK(body.size() < MB_NEXUS_LISTING_REQUEST_LIMIT);
    // The same bytes with the text trimmed and collapsed: text is normalized before it is sent.
    std::string again;
    CHECK(search_body("skyrim", "  better   ranger ", 2, again));
    CHECK_EQ(again, body);
}

TEST(nexus_search_offset_follows_the_page_and_count_is_fixed) {
    for (const auto& [page, offset] : std::array<std::pair<unsigned, unsigned>, 4>{{{1, 0}, {2, 20}, {3, 40}, {50, 980}}}) {
        std::string body;
        CHECK(search_body(game, "ab", page, body));
        const auto variables = Json::parse(body).at("variables");
        CHECK_EQ(variables.at("count"), 20);
        CHECK_EQ(variables.at("offset"), offset);
    }
}

TEST(nexus_search_typed_text_is_data_never_part_of_the_query) {
    const std::array<std::string, 12> hostile{{"x\"}) { viewer } #", "a\\b", "\" } } mutation { x(y: \"",
        "名前 \xf0\x9f\x98\x80 emoji", "$offset", "{{}}", "a\xe2\x80\x8b" "b", "50% off", "'; DROP", "</script>",
        "e\xcc\x81\xcc\x81", "ab\xe2\x80\x8b"}};
    for (const auto& text : hostile) {
        std::string body;
        CHECK(search_body(game, text, 1, body));
        for (const unsigned char c : body) CHECK(c >= 0x20 && c < 0x7f); // ASCII only
        const auto record = Json::parse(body);
        CHECK_EQ(record.at("query"), std::string(hand_query));
        CHECK_EQ(record.at("variables").at("name"), search_text_or_empty(text));
        // The query text appears exactly once: the typed text did not add a second one.
        CHECK(body.find("\"query\"") == body.rfind("\"query\""));
    }
}

TEST(nexus_search_text_is_trimmed_and_inner_space_runs_collapse) {
    const std::array<std::pair<std::string, std::string>, 7> cases{{
        {"  better   ranger ", "better ranger"}, {"ab", "ab"}, {"  ab\xe3\x80\x80" "cd  ", "ab cd"},
        {repeated("x", 100), repeated("x", 100)}, {"  " + repeated("x", 100) + "  ", repeated("x", 100)},
        {"名前", "名前"}, {repeated("\xf0\x9f\x98\x80", 100), repeated("\xf0\x9f\x98\x80", 100)}}};
    for (const auto& [raw, expected] : cases) {
        std::string normalized;
        CHECK(modbridge::nexus_search_text(raw, normalized));
        CHECK_EQ(normalized, expected);
    }
    // One normalized form is the cache key and the only form sent.
    CHECK_EQ(search_text_or_empty("a   b"), search_text_or_empty(" a b "));
    std::string body;
    CHECK(search_body(game, repeated("\xf0\x9f\x98\x80", 100), 1, body));
    CHECK(body.size() < MB_NEXUS_LISTING_REQUEST_LIMIT);
    // Every Unicode space Python's str.split() splits on collapses; zero-width characters are not spaces.
    for (const std::string space : {"\xc2\xa0", "\xe1\x9a\x80", "\xe2\x80\x80", "\xe2\x80\x8a", "\xe2\x80\xaf", "\xe2\x81\x9f", "\xe3\x80\x80", " "}) {
        std::string normalized;
        CHECK(modbridge::nexus_search_text("a" + space + space + "b", normalized));
        CHECK_EQ(normalized, "a b");
    }
    std::string normalized;
    CHECK(modbridge::nexus_search_text("a\xe2\x80\x8b" "b", normalized));
    CHECK_EQ(normalized, "a\xe2\x80\x8b" "b");
}

TEST(nexus_search_invalid_text_is_refused_before_anything_is_built) {
    const std::array<std::string, 24> bad{{"", " ", "a", " a ", "  a  ", repeated("x", 101), " " + repeated("x", 101) + " ",
        repeated("x", 1025), std::string("\0ab", 3), std::string("a\0", 2), "a\tb", "a\nb", "ab\n", "\rab", "\x7f" "ab",
        "a\xc2\x85" "b", "a\x1b" "b", "a\xe2\x80\xa8" "b", "a\xe2\x80\xa9" "b", "ab\xed\xa0\x80", "a\xff" "b", "\xc0\x80" "ab",
        "ab\xf4\x90\x80\x80", "ab\xe2\x82"}};
    for (const auto& text : bad) {
        std::string normalized = "must be cleared";
        CHECK(!modbridge::nexus_search_text(text, normalized));
        CHECK(normalized.empty());
        std::array<char, 4096> out{};
        out.fill('x');
        std::size_t written = 99;
        CHECK(!modbridge::nexus_search_body(game, text, 1, out, written));
        CHECK_EQ(written, 0U);
        for (const char c : out) CHECK_EQ(c, '\0'); // an independent output is wiped on a supported failure
    }
}

TEST(nexus_search_and_browse_refuse_a_bad_page_or_game_domain) {
    std::string body;
    for (const std::uint64_t page : {0ULL, 51ULL, 1000ULL, 1ULL << 63}) {
        CHECK(!search_body(game, "ab", page, body));
        CHECK(!browse_body(game, NexusBrowseSort::Downloads, page, body));
    }
    for (const std::uint64_t page : {1ULL, 2ULL, 49ULL, 50ULL}) {
        CHECK(search_body(game, "ab", page, body));
        CHECK(browse_body(game, NexusBrowseSort::Name, page, body));
    }
    for (const std::string_view domain : {"../../users", "NAME", "", "a b", "-x", "ab\n"}) {
        CHECK(!search_body(domain, "ab", 1, body));
        CHECK(!browse_body(domain, NexusBrowseSort::Name, 1, body));
    }
    CHECK(!browse_body(game, static_cast<NexusBrowseSort>(4), 1, body));
    CHECK(!browse_body(game, static_cast<NexusBrowseSort>(-1), 1, body));
}

TEST(nexus_search_output_storage_rules) {
    std::array<char, 4096> out{};
    std::size_t written = 0;
    // An output too small for the body fails and is wiped; a body of exactly the capacity needs one more for the NUL.
    std::array<char, 100> small{};
    small.fill('x');
    CHECK(!modbridge::nexus_search_body(game, "ab", 1, small, written));
    CHECK_EQ(written, 0U);
    for (const char c : small) CHECK_EQ(c, '\0');
    // Storage larger than the limit, or aliasing an input, is unsupported: refused and untouched.
    std::array<char, MB_NEXUS_LISTING_REQUEST_LIMIT + 1> large{};
    large.fill('x');
    CHECK(!modbridge::nexus_search_body(game, "ab", 1, large, written));
    CHECK_EQ(large[0], 'x');
    std::string text = "abcdefgh";
    std::span<char> alias(text.data(), text.size());
    CHECK(!modbridge::nexus_search_body(game, text, 1, alias, written));
    CHECK_EQ(text, "abcdefgh");
    CHECK(modbridge::nexus_search_body(game, "ab", 1, out, written));
    CHECK_EQ(out[written], '\0');
    CHECK_EQ(std::string_view(out.data()).size(), written);
}

TEST(nexus_listing_pages_follow_the_total_and_stop_at_fifty) {
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 11> cases{{{0, 0}, {1, 1}, {20, 1}, {21, 2}, {40, 2}, {41, 3},
        {999, 50}, {1000, 50}, {1001, 50}, {5000, 50}, {2147483647, 50}}};
    for (const auto& [total, pages] : cases) CHECK_EQ(modbridge::nexus_listing_pages(total), pages);
}

TEST(nexus_browse_each_sort_is_one_fixed_constant_query_with_three_typed_variables) {
    CHECK_EQ(std::string_view(MB_NEXUS_BROWSE_QUERY_HEAD), browse_head);
    CHECK_EQ(std::string_view(MB_NEXUS_BROWSE_QUERY_TAIL), browse_tail);
    for (const auto& [key, sort] : browse_sorts) {
        CHECK_EQ(std::string_view(modbridge::nexus_browse_sort_key(*modbridge::nexus_browse_sort(key))), key);
        const auto query = browse_query(sort);
        CHECK(query.find("name: [") == std::string::npos);
        CHECK(query.find("$name") == std::string::npos);
        CHECK(query.find("relevance") == std::string::npos);
        CHECK(query.find("WILDCARD") == std::string::npos);
        CHECK(query.find("mutation") == std::string::npos);
        CHECK(query.find("viewUploaderHidden: false, viewUserBlockedContent: false") != std::string::npos);
        CHECK(query.find("status: [{value: \"published\", op: EQUALS}]") != std::string::npos);
    }
    // The wire keys and their order (the first is the default).
    CHECK(modbridge::nexus_browse_sort("downloads").value() == NexusBrowseSort::Downloads);
    CHECK(modbridge::nexus_browse_sort("endorsements").value() == NexusBrowseSort::Endorsements);
    CHECK(modbridge::nexus_browse_sort("updated").value() == NexusBrowseSort::Updated);
    CHECK(modbridge::nexus_browse_sort("name").value() == NexusBrowseSort::Name);
}

TEST(nexus_browse_body_is_the_sorts_constant_query_plus_typed_variables_byte_for_byte) {
    for (std::size_t i = 0; i < browse_sorts.size(); ++i) {
        std::string body;
        CHECK(browse_body("skyrim", static_cast<NexusBrowseSort>(i), 2, body));
        const auto expected = "{\"query\":" + json_text(browse_query(browse_sorts[i].second)) +
            ",\"variables\":{\"gameDomainName\":\"skyrim\",\"count\":20,\"offset\":20}}";
        CHECK_EQ(body, expected);
        const auto record = Json::parse(body);
        CHECK_EQ(record.size(), 2U);
        CHECK_EQ(record.at("variables"), Json({{"gameDomainName", "skyrim"}, {"count", 20}, {"offset", 20}}));
        CHECK(body.size() < MB_NEXUS_LISTING_REQUEST_LIMIT);
    }
}

TEST(nexus_browse_only_the_four_sort_keys_are_accepted) {
    for (const std::string_view bad : {"", "Downloads", "downloads ", " name", "relevance", "createdAt", "updatedAt",
                                       "uniqueDownloads", "random", "size", "lastComment", "downloads,name",
                                       "name\"}", "name: {direction: ASC}", "$offset"})
        CHECK(!modbridge::nexus_browse_sort(bad).has_value());
}

// ---------------------------------------------------------------- the strict decoder

TEST(nexus_listing_a_valid_page_decodes_to_the_rest_record_inputs) {
    const auto result = decode(reply(Json::array({node(17), node(18, {{"adultContent", true}, {"name", "第二の作品"}})})));
    CHECK_EQ(result.at("status"), "decoded");
    CHECK_EQ(result.at("schema_version"), 1);
    CHECK_EQ(result.at("scope"), "nexus_metadata_only");
    CHECK(result.at("issues").empty());
    CHECK_EQ(result.at("total"), 2);
    CHECK_EQ(result.at("records")[0], Json({{"mod_id", 17}, {"game_id", 444}, {"game_domain", std::string(game)},
        {"name", "Owned fixture"}, {"author", "Fixture author"}, {"summary", "Synthetic test data"}, {"description", ""},
        {"version", "1.2"}, {"contains_adult_content", false}, {"available", true}, {"status", "published"},
        {"source_url", "https://www.nexusmods.com/arbitrarygame/mods/17"}}));
    CHECK_EQ(result.at("records")[1].at("name"), "第二の作品");
    CHECK_EQ(result.at("records")[1].at("contains_adult_content"), true);
}

TEST(nexus_listing_empty_results_and_pages_past_the_end_are_complete_answers) {
    auto result = decode(reply(Json::array()));
    CHECK_EQ(result.at("status"), "decoded");
    CHECK(result.at("records").empty());
    CHECK_EQ(result.at("total"), 0);
    // Page 2 of 25 holds the last five; page 3 holds none.
    result = decode(reply(nodes_of(1, 5), 25), game, 2);
    CHECK_EQ(result.at("status"), "decoded");
    CHECK_EQ(result.at("records").size(), 5U);
    result = decode(reply(Json::array(), 25), game, 3);
    CHECK_EQ(result.at("status"), "decoded");
    CHECK(result.at("records").empty());
    CHECK_EQ(result.at("total"), 25);
    const auto full = decode(reply(nodes_of(1, 20), 2147483647));
    CHECK_EQ(full.at("status"), "decoded");
    CHECK_EQ(full.at("records").size(), 20U);
    CHECK_EQ(full.at("total"), 2147483647);
    // Page 50 starts at offset 980: 20 of 1000 remain.
    CHECK_EQ(decode(reply(nodes_of(1, 20), 1000), game, 50).at("status"), "decoded");
}

TEST(nexus_listing_graphql_errors_are_never_partial_data) {
    const auto good = reply(Json::array({node()}));
    const std::array<Json, 5> errors{{Json::array({{{"message", "SecretMarker"}}}), Json::array({Json::object()}),
        Json::array({nullptr}), Json::array({"x"}), Json::array({{{"message", "x"}}, {{"message", "x"}}, {{"message", "x"}}})}};
    for (const auto& value : errors) {
        auto with_errors = good;
        with_errors["errors"] = value;
        failed(decode(with_errors));
        failed(decode(Json{{"errors", value}}));
        failed(decode(Json{{"data", nullptr}, {"errors", value}}));
    }
    for (const Json& value : {Json(nullptr), Json::object(), Json("none"), Json(0), Json(false)}) {
        auto with_errors = good;
        with_errors["errors"] = value;
        failed(decode(with_errors));
    }
    auto empty_errors = good;
    empty_errors["errors"] = Json::array();
    CHECK_EQ(decode(empty_errors).at("status"), "decoded");
}

TEST(nexus_listing_shape_errors_are_malformed) {
    for (const Json& body : {Json::object(), Json::array(), Json("x"), Json(5), Json(nullptr), Json{{"data", nullptr}},
                             Json{{"data", Json::array()}}, Json{{"data", Json::object()}},
                             Json{{"data", {{"mods", nullptr}}}}, Json{{"data", {{"mods", Json::array()}}}},
                             Json{{"data", {{"mods", Json::object()}}}}, Json{{"data", {{"modRequirements", Json::object()}}}}})
        failed(decode(body));
    for (const char* field : {"nodes", "nodesCount", "totalCount"}) {
        auto value = reply(Json::array({node()}));
        value["data"]["mods"].erase(field);
        failed(decode(value));
    }
    for (const Json& bad : {Json(nullptr), Json::object(), Json("x"), Json(5), Json(true)}) {
        auto value = reply(Json::array({node()}));
        value["data"]["mods"]["nodes"] = bad;
        failed(decode(value));
    }
    for (const char* field : {"nodesCount", "totalCount"})
        for (const Json& bad : {Json(nullptr), Json("1"), Json(1.0), Json(1.5), Json(true), Json(false), Json(-1),
                                Json::array({1}), Json{{"v", 1}}}) {
            auto value = reply(Json::array({node()}));
            value["data"]["mods"][field] = bad;
            failed(decode(value));
        }
    failed(decode(reply(Json::array({"not an object"}))));
    failed(decode(reply(Json::array({nullptr}))));
    failed(decode(reply(Json::array({Json::array({node()})}))));
}

TEST(nexus_listing_counts_must_describe_exactly_the_requested_page) {
    const auto three = nodes_of(1, 3);
    CHECK_EQ(decode(reply(three)).at("status"), "decoded");
    failed(decode(reply(three, -1, {{"nodesCount", 2}})));        // nodesCount disagrees with the nodes
    failed(decode(reply(three, -1, {{"nodesCount", 4}})));
    failed(decode(reply(three, 2)));                              // fewer matches than nodes returned
    failed(decode(reply(three, 4)));                              // a short page although more matched
    failed(decode(reply(three, 100)));                            // truncated: a full page was due
    failed(decode(reply(nodes_of(1, 20), 19)));
    failed(decode(reply(nodes_of(1, 20), 25), game, 2));          // 5 were due, 20 came
    failed(decode(reply(Json::array(), 5)));                      // matches exist but the page is empty
    failed(decode(reply(Json::array(), 45), game, 2));            // 20 were due
    failed(decode(reply(three, 3), game, 2));                     // page 2 starts past the end: none are due
}

TEST(nexus_listing_bounds) {
    const auto twenty = nodes_of(1, 20);
    CHECK_EQ(decode(reply(twenty)).at("status"), "decoded");
    failed(decode(reply(nodes_of(1, 21))), "resource_limit");
    failed(decode(reply(twenty, -1, {{"nodesCount", 21}})), "resource_limit");
    failed(decode(reply(nodes_of(1, 199))), "resource_limit");
    failed(decode(reply(Json::array(), 2147483648LL)), "resource_limit");
    failed(decode_text(R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":9223372036854775808}}})"), "resource_limit");
    failed(decode(reply(Json::array(), -1, {{"nodesCount", 1000000000}})), "resource_limit");
    failed(decode(reply(Json::array()), game, 0));
    failed(decode(reply(Json::array()), game, 51));
    failed(decode(reply(Json::array()), "NOT A DOMAIN", 1));
}

TEST(nexus_listing_every_node_field_is_typed) {
    for (const char* key : {"modId", "name", "summary", "version", "author", "adultContent", "status", "uploader", "game"}) {
        auto value = node();
        value.erase(key);
        failed(decode(reply(Json::array({value}))));
    }
    const std::vector<std::pair<const char*, Json>> bad{{
        {"uploader", Json::object()}, {"uploader", nullptr}, {"uploader", "x"}, {"uploader", {{"name", nullptr}}},
        {"uploader", {{"name", 5}}}, {"game", Json::object()}, {"game", nullptr}, {"game", {{"id", 444}}},
        {"game", {{"domainName", std::string(game)}}}, {"game", {{"id", "444"}, {"domainName", std::string(game)}}},
        {"game", {{"id", 0}, {"domainName", std::string(game)}}}, {"game", {{"id", true}, {"domainName", std::string(game)}}},
        {"game", {{"id", 2147483648LL}, {"domainName", std::string(game)}}}, {"game", {{"id", 444}, {"domainName", nullptr}}},
        {"modId", "17"}, {"modId", 17.0}, {"modId", true}, {"modId", 0}, {"modId", -3}, {"modId", 2147483648LL},
        {"modId", nullptr}, {"name", nullptr}, {"name", 5}, {"summary", nullptr}, {"summary", 5}, {"version", nullptr},
        {"version", 1.2}, {"author", 5}, {"author", Json::array({"x"})}, {"adultContent", "false"}, {"adultContent", 0},
        {"adultContent", 1}, {"adultContent", "true"}, {"adultContent", Json::array()}, {"status", nullptr}, {"status", 5},
        {"status", true}, {"game", {{"id", 444.0}, {"domainName", std::string(game)}}}}};
    for (const auto& [key, value] : bad) failed(decode(reply(Json::array({node(17, {{key, value}})}))));
}

TEST(nexus_listing_a_hit_for_another_game_is_never_returned) {
    failed(decode(reply(Json::array({node(17, {{"game", {{"id", 444}, {"domainName", "othergame"}}}})}))));
    failed(decode(reply(Json::array({node(1), node(2, {{"game", {{"id", 9}, {"domainName", "othergame"}}}})}))));
    failed(decode(reply(Json::array({node(17, {{"game", {{"id", 444}, {"domainName", "ARBITRARYGAME"}}}})}))));
}

TEST(nexus_listing_duplicate_mods_in_a_page_are_refused) {
    failed(decode(reply(Json::array({node(17), node(18), node(17)}))));
}

TEST(nexus_listing_author_falls_back_to_the_uploader_and_unknown_adult_stays_unknown) {
    for (const Json& author : {Json(nullptr), Json("")})
        CHECK_EQ(decode(reply(Json::array({node(17, {{"author", author}})}))).at("records")[0].at("author"), "Uploader");
    CHECK(decode(reply(Json::array({node(17, {{"adultContent", nullptr}})}))).at("records")[0].at("contains_adult_content").is_null());
}

TEST(nexus_listing_text_bounds_and_control_characters) {
    struct Bound { const char* key; std::size_t limit; };
    for (const Bound bound : {Bound{"name", 1024}, Bound{"author", 1024}, Bound{"version", 256}, Bound{"summary", 65536}, Bound{"status", 64}}) {
        CHECK_EQ(decode(reply(Json::array({node(17, {{bound.key, repeated("界", bound.limit)}})}))).at("status"), "decoded");
        failed(decode(reply(Json::array({node(17, {{bound.key, repeated("界", bound.limit + 1)}})}))));
    }
    for (const char* control : {"\\u0000", "\\u0001", "\\u0008", "\\u000b", "\\u000c", "\\u000e", "\\u001f"}) {
        for (const char* key : {"name", "summary", "version", "author", "status"}) {
            auto text = reply(Json::array({node()})).dump();
            const std::string field = std::string("\"") + key + "\":";
            auto at = text.find(field);
            CHECK(at != std::string::npos);
            // Replace the field's first value character run with text that carries the control character.
            const auto open = text.find('"', at + field.size());
            const auto close = text.find('"', open + 1);
            text.replace(open + 1, close - open - 1, std::string("safe") + control + "unsafe");
            failed(decode_text(text));
        }
        auto text = reply(Json::array({node()})).dump();
        const auto at = text.find("\"Uploader\"");
        text.replace(at + 1, 8, std::string("a") + control);
        failed(decode_text(text));
    }
    // Line breaks and tabs are ordinary text in a summary, as in the REST records.
    CHECK_EQ(decode(reply(Json::array({node(17, {{"summary", "a\nb\tc\r"}})}))).at("status"), "decoded");
}

TEST(nexus_listing_extra_provider_fields_are_ignored_and_not_returned) {
    auto value = reply(Json::array({node(17, {{"downloads", 5}, {"endorsements", 9}, {"updatedAt", "2026-01-01T00:00:00Z"},
        {"apikey", "must-not-leak"}, {"uploader", {{"name", "Uploader"}, {"memberId", 7}, {"email", "x@example.test"}}},
        {"game", {{"id", 444}, {"domainName", std::string(game)}, {"name", "Arbitrary"}}}})}));
    value["data"]["mods"]["facets"] = {{"x", 1}};
    value["extensions"] = {{"cost", 5}};
    const auto result = decode(value);
    CHECK_EQ(result.at("status"), "decoded");
    CHECK(result.dump().find("must-not-leak") == std::string::npos);
    CHECK_EQ(result.at("records")[0].size(), 12U);
}

TEST(nexus_listing_the_parse_gate_applies) {
    const auto base = reply(Json::array({node()})).dump();
    for (const std::string& body : {std::string(), std::string("{"), std::string("not json"), std::string("\xff\xfe"),
                                    base.substr(0, base.size() - 1), base + " x", std::string("{} {}"), std::string("\0", 1) + base})
        failed(decode_text(body));
    CHECK_EQ(decode_text("\xef\xbb\xbf" + base).at("status"), "decoded");
    // Repeated keys, non-JSON constants, odd number forms, overflow, bad escapes and unpaired surrogates.
    for (const std::string body : {
            R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0,"totalCount":0}}})",
            R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":NaN}}})",
            R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":Infinity}}})",
            R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":-Infinity}}})",
            R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0.0}}})",
            R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0e0}}})",
            R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":1e999}}})",
            R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":00}}})",
            R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"x":1e999})",
            R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"x":"\ud800"})",
            R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"x":"\u0001"})",
            R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"\u0001":1})",
            R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"data":1})"})
        failed(decode_text(body));
    auto doubled_id = base;
    doubled_id.replace(doubled_id.find("\"modId\":17"), 10, "\"modId\":17,\"modId\":18");
    failed(decode_text(doubled_id));
    auto doubled_name = base;
    doubled_name.replace(doubled_name.find("\"name\":\"Owned fixture\""), 22, "\"name\":\"a\",\"name\":\"b\"");
    failed(decode_text(doubled_name));
    CHECK_EQ(decode_text(R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"x":"\n\t\r"})").at("status"), "decoded");
}

TEST(nexus_listing_depth_array_and_size_limits) {
    const auto base = reply(Json::array()).dump();
    const auto nested = [](std::size_t depth) {
        return std::string(R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"x":)") +
               std::string(depth, '[') + std::string(depth, ']') + "}";
    };
    CHECK_EQ(decode_text(nested(60)).at("status"), "decoded");
    failed(decode_text(nested(100)), "resource_limit");
    failed(decode_text(nested(100000)), "resource_limit");
    std::string wide = R"({"data":{"mods":{"nodes":[],"nodesCount":0,"totalCount":0}},"x":[)";
    for (int i = 0; i < 10001; ++i) wide += i ? ",0" : "0";
    wide += "]}";
    failed(decode_text(wide), "resource_limit");
    auto padded = base + std::string(8U * 1024U * 1024U - base.size(), ' ');
    CHECK_EQ(decode_text(padded).at("status"), "decoded");
    failed(decode_text(padded + " "), "resource_limit");
}
