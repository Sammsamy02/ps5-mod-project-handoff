#include "modbridge/nexus.hpp"
#include "test.hpp"

#include <array>
#include <algorithm>
#include <cstring>
#include <limits>
#include <string_view>

namespace {
using modbridge::Json;
constexpr auto requirements_kind = static_cast<modbridge::NexusRecordKind>(6);
Json empty_response() {
    return {{"data", {{"modRequirements", {
        {"nexusRequirements", {{"nodes", Json::array()}, {"nodesCount", 0}, {"totalCount", 0}}},
        {"dlcRequirements", Json::array()}}}}}};
}
Json decode_requirements(const Json& input) {
    const auto body = input.dump();
    return modbridge::decode_nexus_metadata(
        {reinterpret_cast<const std::uint8_t*>(body.data()), body.size()},
        requirements_kind, "future_game-2", 71);
}
Json requirement(bool external = false) {
    return {{"id", "relation-1"}, {"externalRequirement", external},
        {"gameId", external ? "" : "another_game-2"}, {"modId", external ? "" : "82"},
        {"modName", "必要な作品"}, {"url", "https://provider.example/requirement"}};
}
Json expansion() {
    return {{"gameExpansion", {{"gameId", "future_game-2"}, {"id", "expansion-1"}, {"name", "Expansion"}}}};
}
Json response_with(Json nodes, Json dlc = Json::array()) {
    auto input = empty_response();
    auto& page = input["data"]["modRequirements"]["nexusRequirements"];
    page["nodesCount"] = nodes.size(); page["totalCount"] = nodes.size();
    page["nodes"] = std::move(nodes);
    input["data"]["modRequirements"]["dlcRequirements"] = std::move(dlc);
    return input;
}
void failed(const Json& result, std::string_view status = "malformed") {
    CHECK_EQ(result.at("status"), status);
    CHECK_EQ(result.at("scope"), "nexus_metadata_only");
    CHECK_EQ(result.at("installation_supported"), false);
    CHECK(!result.contains("requirements"));
    CHECK(!result.contains("records"));
    CHECK(!result.contains("profile"));
    CHECK_EQ(result.at("issues").size(), 1U);
    CHECK_EQ(result.at("issues")[0].size(), 1U);
    CHECK(result.dump().find("SecretMarker") == std::string::npos);
}
bool zeroed(std::span<const char> output) {
    return std::all_of(output.begin(), output.end(), [](char c) { return c == 0; });
}
}

TEST(nexus_requirements_kind_constructs_fixed_graphql_origin) {
    const auto url = modbridge::nexus_metadata_url(requirements_kind, "future_game-2", 71);
    CHECK(url.has_value());
    CHECK_EQ(*url, "https://api.nexusmods.com/v2/graphql");
}

TEST(nexus_requirements_explicit_complete_empty_set_is_owning_metadata) {
    const auto result = decode_requirements(empty_response());
    CHECK_EQ(result.at("status"), "decoded");
    CHECK(!result.contains("records"));
    CHECK(!result.contains("profile"));
    CHECK_EQ(result.at("requirements"), Json({
        {"game", "future_game-2"}, {"mod_id", 71}, {"discovery_complete", true},
        {"nexus_requirements", Json::array()}, {"dlc_requirements", Json::array()},
        {"file_version_constraints_provided", false}, {"installation_supported", false}}));
}

TEST(nexus_requirements_writer_emits_one_fixed_query_and_typed_variables) {
    std::array<char, 4096> output{};
    std::size_t written = 0;
    CHECK(modbridge::nexus_requirements_body("future_game-2", 71, output, written));
    const auto body = Json::parse(output.data(), output.data() + written);
    CHECK_EQ(body, Json({{"query", "query ModBridgeRequirements($modId: Int!, $gameDomainName: String!) { modRequirements(modId: $modId, gameDomainName: $gameDomainName) { nexusRequirements { nodes { id externalRequirement gameId modId modName notes url } nodesCount totalCount } dlcRequirements { gameExpansion { gameId id name } notes } } }"},
        {"variables", {{"gameDomainName", "future_game-2"}, {"modId", 71}}}}));
    CHECK_EQ(output[written], '\0');
    CHECK(std::string_view(output.data(), written).find("modsRequiringThisMod") == std::string_view::npos);
}

TEST(nexus_requirements_query_int32_boundary_and_domain_grammar_fail_closed) {
    for (const auto id : {UINT64_C(0), UINT64_C(2147483648), UINT64_MAX}) {
        CHECK(!modbridge::nexus_metadata_url(requirements_kind, "game", id));
        std::array<char, 4096> output; output.fill('x'); std::size_t written = 11;
        CHECK(!modbridge::nexus_requirements_body("game", id, output, written));
        CHECK(zeroed(output)); CHECK_EQ(written, 0U);
    }
    std::array<char, 4096> output{}; std::size_t written = 0;
    CHECK(modbridge::nexus_requirements_body(std::string(80, 'x'), 2147483647, output, written));
    CHECK_EQ(Json::parse(output.data()).at("variables").at("modId"), 2147483647);
    for (const std::string_view domain : {"", "Upper", "../x", "a/b", "a\"b", "a\\b", "a?x", "-x"}) {
        output.fill('x'); written = 11;
        CHECK(!modbridge::nexus_requirements_body(domain, 1, output, written));
        CHECK(zeroed(output)); CHECK_EQ(written, 0U);
    }
}

TEST(nexus_requirements_writer_short_output_wipes_and_aliases_remain_untouched) {
    std::array<char, 4096> full{}; std::size_t written = 0;
    CHECK(modbridge::nexus_requirements_body("game", 1, full, written));
    const auto size = written;
    std::array<char, 4096> output; output.fill('x'); written = 77;
    CHECK(!modbridge::nexus_requirements_body("game", 1, std::span<char>(output.data(), size), written));
    CHECK(zeroed(std::span<const char>(output.data(), size))); CHECK_EQ(output[size], 'x'); CHECK_EQ(written, 0U);
    CHECK(modbridge::nexus_requirements_body("game", 1, std::span<char>(output.data(), size + 1), written));
    CHECK_EQ(written, size); CHECK_EQ(output[size], '\0');
    output.fill('x'); std::memcpy(output.data(), "game", 4); const auto before = output; written = 77;
    CHECK(!modbridge::nexus_requirements_body(std::string_view(output.data(), 4), 1, output, written));
    CHECK(output == before); CHECK_EQ(written, 77U);
    written = 77;
    CHECK(!modbridge::nexus_requirements_body("game", 1,
        {reinterpret_cast<char*>(&written), sizeof(written)}, written));
    CHECK_EQ(written, 77U);
    const auto* id_bytes = reinterpret_cast<const char*>(&written);
    CHECK(!modbridge::nexus_requirements_body(std::string_view(id_bytes, 1), 1, output, written));
    CHECK(output == before); CHECK_EQ(written, 77U);
    std::array<char, 4097> oversized; oversized.fill('x');
    CHECK(!modbridge::nexus_requirements_body("game", 1, oversized, written));
    CHECK(std::all_of(oversized.begin(), oversized.end(), [](char c) { return c == 'x'; }));
    CHECK_EQ(written, 77U);
    CHECK(!modbridge::nexus_requirements_body("game", 1, {}, written)); CHECK_EQ(written, 0U);
}

TEST(nexus_requirements_complete_rows_preserve_internal_external_and_dlc_unresolved) {
    auto internal = requirement(); internal["notes"] = nullptr;
    auto external = requirement(true); external["id"] = "relation-2";
    external["gameId"] = "External Product"; external["modId"] = "external-release";
    external["notes"] = "Read the provider note";
    auto dlc = expansion(); dlc["notes"] = nullptr;
    auto input = response_with(Json::array({internal, external}), Json::array({dlc}));
    input["data"]["modRequirements"]["modsRequiringThisMod"] = "ignored reverse relation";
    input["data"]["modRequirements"]["nexusRequirements"]["nodes"][0]["fileId"] = "SecretMarker";
    const auto result = decode_requirements(input);
    CHECK_EQ(result.at("status"), "decoded");
    CHECK(!result.contains("records"));
    CHECK_EQ(result.at("requirements").at("nexus_requirements"), Json::array({
        {{"id", "relation-1"}, {"external_requirement", false}, {"game", "another_game-2"},
         {"mod_id", "82"}, {"name", "必要な作品"}, {"notes", ""},
         {"url", "https://provider.example/requirement"}, {"satisfaction", "unresolved"}},
        {{"id", "relation-2"}, {"external_requirement", true}, {"game", "External Product"},
         {"mod_id", "external-release"}, {"name", "必要な作品"}, {"notes", "Read the provider note"},
         {"url", "https://provider.example/requirement"}, {"satisfaction", "unresolved"}}}));
    CHECK_EQ(result.at("requirements").at("dlc_requirements"), Json::array({
        {{"game", "future_game-2"}, {"id", "expansion-1"}, {"name", "Expansion"},
         {"notes", ""}, {"satisfaction", "unresolved"}}}));
    CHECK_EQ(result.at("requirements").at("file_version_constraints_provided"), false);
    CHECK_EQ(result.at("requirements").at("installation_supported"), false);
    CHECK(result.dump().find("SecretMarker") == std::string::npos);
}

TEST(nexus_requirements_graphql_errors_never_publish_partial_data_or_provider_messages) {
    for (const auto& errors : {Json(nullptr), Json("SecretMarker"), Json::object(),
                              Json::array({{{"message", "SecretMarker"}}})}) {
        auto input = empty_response(); input["errors"] = errors;
        failed(decode_requirements(input));
    }
    auto input = empty_response(); input["errors"] = Json::array();
    CHECK_EQ(decode_requirements(input).at("status"), "decoded");
}

TEST(nexus_requirements_missing_null_or_wrong_shaped_collections_fail_closed) {
    for (const auto& malformed : {Json(nullptr), Json::array(), Json("SecretMarker")}) {
        auto input = empty_response(); input["data"] = malformed; failed(decode_requirements(input));
        input = empty_response(); input["data"]["modRequirements"] = malformed; failed(decode_requirements(input));
        input = empty_response(); input["data"]["modRequirements"]["nexusRequirements"] = malformed; failed(decode_requirements(input));
        input = empty_response(); input["data"]["modRequirements"]["dlcRequirements"] = malformed;
        if (!malformed.is_array()) failed(decode_requirements(input));
    }
    for (const auto* key : {"nexusRequirements", "dlcRequirements"}) {
        auto input = empty_response(); input["data"]["modRequirements"].erase(key); failed(decode_requirements(input));
    }
    failed(decode_requirements(Json::object()));
}

TEST(nexus_requirements_count_mismatch_truncation_and_noninteger_counts_fail_closed) {
    for (const auto* key : {"nodesCount", "totalCount"}) {
        for (const auto& count : {Json(-1), Json(1), Json(0.0), Json(true), Json("0"), Json(nullptr)}) {
            auto input = empty_response(); input["data"]["modRequirements"]["nexusRequirements"][key] = count;
            failed(decode_requirements(input));
        }
        auto input = empty_response(); input["data"]["modRequirements"]["nexusRequirements"].erase(key);
        failed(decode_requirements(input));
    }
    auto input = response_with(Json::array({requirement()}));
    input["data"]["modRequirements"]["nexusRequirements"]["totalCount"] = 2;
    failed(decode_requirements(input));
}

TEST(nexus_requirements_all_rows_validate_before_owning_publication) {
    for (const auto* key : {"id", "externalRequirement", "gameId", "modId", "modName", "url"}) {
        auto bad = requirement(); bad["id"] = "relation-2"; bad.erase(key);
        failed(decode_requirements(response_with(Json::array({requirement(), bad}))));
        bad = requirement(); bad["id"] = "relation-2"; bad[key] = nullptr;
        failed(decode_requirements(response_with(Json::array({requirement(), bad}))));
    }
    auto duplicate = requirement(); duplicate["notes"] = "SecretMarker";
    failed(decode_requirements(response_with(Json::array({requirement(), duplicate}))));
    auto bad = requirement(); bad["externalRequirement"] = 1;
    failed(decode_requirements(response_with(Json::array({bad}))));
    bad = requirement(); bad["notes"] = 1;
    failed(decode_requirements(response_with(Json::array({bad}))));
}

TEST(nexus_requirements_internal_identity_is_canonical_positive_signed63) {
    for (const auto* id : {"", "0", "01", "+1", "-1", "1.0", " 1", "1 ", "9223372036854775808"}) {
        auto item = requirement(); item["modId"] = id;
        failed(decode_requirements(response_with(Json::array({item}))));
    }
    for (const auto* game : {"", "Upper", "a/b", "-game", "game.domain"}) {
        auto item = requirement(); item["gameId"] = game;
        failed(decode_requirements(response_with(Json::array({item}))));
    }
    auto item = requirement(); item["modId"] = "9223372036854775807";
    CHECK_EQ(decode_requirements(response_with(Json::array({item}))).at("requirements")
        .at("nexus_requirements")[0].at("mod_id"), "9223372036854775807");
}

TEST(nexus_requirements_dlc_shape_is_typed_and_never_installed) {
    for (const auto* key : {"gameId", "id", "name"}) {
        auto item = expansion(); item["gameExpansion"].erase(key);
        failed(decode_requirements(response_with(Json::array(), Json::array({item}))));
        item = expansion(); item["gameExpansion"][key] = 1;
        failed(decode_requirements(response_with(Json::array(), Json::array({item}))));
    }
    auto item = expansion(); item["gameExpansion"] = nullptr;
    failed(decode_requirements(response_with(Json::array(), Json::array({item}))));
    item = expansion(); item["notes"] = true;
    failed(decode_requirements(response_with(Json::array(), Json::array({item}))));
}

TEST(nexus_requirements_relation_and_dlc_limits_refuse_overflow) {
    Json nodes = Json::array();
    for (unsigned i = 0; i != 1024; ++i) {
        auto item = requirement(); item["id"] = "relation-" + std::to_string(i); nodes.push_back(std::move(item));
    }
    CHECK_EQ(decode_requirements(response_with(nodes)).at("requirements").at("nexus_requirements").size(), 1024U);
    auto last = requirement(); last["id"] = "relation-1024"; nodes.push_back(std::move(last));
    failed(decode_requirements(response_with(nodes)), "resource_limit");
    auto input = empty_response(); input["data"]["modRequirements"]["nexusRequirements"]["totalCount"] = 1025;
    failed(decode_requirements(input), "resource_limit");
    Json dlc = Json::array();
    for (unsigned i = 0; i != 1025; ++i) dlc.push_back(expansion());
    failed(decode_requirements(response_with(Json::array(), dlc)), "resource_limit");
}

TEST(nexus_requirements_bound_text_and_source_context_are_checked) {
    for (const auto* key : {"id", "gameId", "modId", "modName", "notes", "url"}) {
        auto item = requirement(true); item[key] = std::string(65537, 'x');
        failed(decode_requirements(response_with(Json::array({item}))));
    }
    const auto body = empty_response().dump();
    for (const auto id : {UINT64_C(0), UINT64_C(2147483648), UINT64_MAX}) {
        failed(modbridge::decode_nexus_metadata(
            {reinterpret_cast<const std::uint8_t*>(body.data()), body.size()}, requirements_kind, "game", id));
    }
}
