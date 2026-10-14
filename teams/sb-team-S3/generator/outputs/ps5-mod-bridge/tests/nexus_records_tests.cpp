#include "modbridge/nexus.hpp"
#include "test.hpp"

#include <string_view>

namespace {
using modbridge::Json;
using modbridge::NexusRecordKind;
Json decode(std::string_view text, NexusRecordKind kind,
            std::string_view domain = {}, std::uint64_t mod_id = 0) {
    return modbridge::decode_nexus_metadata(
        {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()}, kind, domain, mod_id);
}
Json decode(const Json& input, NexusRecordKind kind,
            std::string_view domain = {}, std::uint64_t mod_id = 0) {
    const auto text = input.dump();
    return decode(std::string_view(text), kind, domain, mod_id);
}
void decoded(const Json& result) {
    CHECK_EQ(result.value("status", std::string{}), "decoded");
    CHECK_EQ(result.at("schema_version"), 1);
    CHECK_EQ(result.at("scope"), "nexus_metadata_only");
    CHECK_EQ(result.at("installation_supported"), false);
    CHECK(result.at("issues").empty());
}
void failed(const Json& result, std::string_view status = "malformed") {
    CHECK_EQ(result.value("status", std::string{}), status);
    CHECK_EQ(result.at("schema_version"), 1);
    CHECK_EQ(result.at("scope"), "nexus_metadata_only");
    CHECK_EQ(result.at("installation_supported"), false);
    CHECK(!result.contains("records"));
    CHECK(!result.contains("profile"));
    CHECK_EQ(result.at("issues").size(), 1U);
    CHECK_EQ(result.at("issues")[0].size(), 1U);
    CHECK(result.at("issues")[0].at("code").is_string());
}
Json game() {
    return {{"id", 37}, {"domain_name", "another_game-2"}, {"name", "別のゲーム"}};
}
Json mod() {
    return {{"mod_id", 4451}, {"game_id", 37}, {"domain_name", "another_game-2"},
            {"name", "作品 — édition"}, {"author", "Ḥélène\t作品"},
            {"contains_adult_content", false}, {"available", true}, {"status", "published"}};
}
Json file() {
    return {{"file_id", 92817}, {"name", "Content"}, {"file_name", "作品.zip"},
            {"version", "v2"}, {"size_kb", 12.5}};
}
Json api_user() {
    return {{"user_id", 53}, {"name", "利用者"}, {"is_premium", true}, {"is_supporter", false}};
}
std::string repeated(std::string_view codepoint, std::size_t count) {
    std::string result;
    for (std::size_t i = 0; i < count; ++i) result += codepoint;
    return result;
}
} // namespace

TEST(nexus_metadata_routes_all_six_kinds_for_arbitrary_game_domains) {
    using modbridge::nexus_metadata_url;
    const auto url = [](NexusRecordKind kind, std::string_view domain = {}, std::uint64_t id = 0) {
        const auto result = nexus_metadata_url(kind, domain, id);
        CHECK(result.has_value());
        return *result;
    };
    CHECK_EQ(url(NexusRecordKind::Games), "https://api.nexusmods.com/v1/games");
    CHECK_EQ(url(NexusRecordKind::Mods, "future_game-2"),
             "https://api.nexusmods.com/v1/games/future_game-2/mods/latest_added");
    CHECK_EQ(url(NexusRecordKind::Mod, "future_game-2", 71),
             "https://api.nexusmods.com/v1/games/future_game-2/mods/71");
    CHECK_EQ(url(NexusRecordKind::Files, "future_game-2", 9223372036854775807ULL),
             "https://api.nexusmods.com/v1/games/future_game-2/mods/9223372036854775807/files");
    CHECK_EQ(url(NexusRecordKind::OAuthUser), "https://users.nexusmods.com/oauth/userinfo");
    CHECK_EQ(url(NexusRecordKind::ApiKeyUser), "https://api.nexusmods.com/v1/users/validate");
    CHECK(nexus_metadata_url(NexusRecordKind::Mods, std::string(80, 'x')).has_value());
}

TEST(nexus_metadata_routes_reject_unusable_context_and_path_injection) {
    using modbridge::nexus_metadata_url;
    CHECK(!nexus_metadata_url(static_cast<NexusRecordKind>(99)));
    CHECK(!nexus_metadata_url(NexusRecordKind::Games, "domain"));
    CHECK(!nexus_metadata_url(NexusRecordKind::Games, {}, 1));
    CHECK(!nexus_metadata_url(NexusRecordKind::OAuthUser, "domain"));
    CHECK(!nexus_metadata_url(NexusRecordKind::ApiKeyUser, {}, 1));
    CHECK(!nexus_metadata_url(NexusRecordKind::Mods, "domain", 1));
    CHECK(!nexus_metadata_url(NexusRecordKind::Mod, "domain", 0));
    CHECK(!nexus_metadata_url(NexusRecordKind::Files, "domain", UINT64_MAX));
    for (const std::string invalid : {"", "../games", "https://evil.invalid", "foo/bar", "foo?x=y", "foo#part", "UPPER", "-domain", "foo%2fbar", "foo\r\nx:y"})
        CHECK(!nexus_metadata_url(NexusRecordKind::Mods, invalid));
    CHECK(!nexus_metadata_url(NexusRecordKind::Mods, std::string("foo\0bar", 7)));
    CHECK(!nexus_metadata_url(NexusRecordKind::Mods, std::string(81, 'x')));
}

TEST(nexus_games_construct_sources_and_discard_provider_fields) {
    auto item = game();
    item["mods"] = 0;
    item["id"] = 9223372036854775807ULL;
    item["source_url"] = "https://evil.invalid/SecretMarker";
    item["email"] = "SecretMarker";
    const auto result = decode(Json::array({item}), NexusRecordKind::Games);
    decoded(result);
    CHECK_EQ(result.at("records"), Json::array({{
        {"id", 9223372036854775807ULL}, {"domain_name", "another_game-2"},
        {"name", "別のゲーム"}, {"mods", 0},
        {"source_url", "https://www.nexusmods.com/another_game-2"}}}));
    CHECK(result.dump().find("SecretMarker") == std::string::npos);
}

TEST(nexus_mods_preserve_authorship_and_unavailable_unknown_content_facts) {
    auto item = mod();
    item["available"] = false;
    item["status"] = "archived";
    item["contains_adult_content"] = "false";
    item["summary"] = "One\nTwo\rThree\t四";
    item["description"] = "<p>Content fact</p>";
    const auto result = decode(Json::array({item}), NexusRecordKind::Mods, "another_game-2");
    decoded(result);
    CHECK_EQ(result.at("records"), Json::array({{
        {"mod_id", 4451}, {"game_id", 37}, {"game_domain", "another_game-2"},
        {"name", "作品 — édition"}, {"author", "Ḥélène\t作品"},
        {"summary", "One\nTwo\rThree\t四"}, {"description", "<p>Content fact</p>"},
        {"version", ""}, {"contains_adult_content", nullptr}, {"available", false},
        {"status", "archived"}, {"source_url", "https://www.nexusmods.com/another_game-2/mods/4451"}}}));
}

TEST(nexus_single_mod_checks_external_identity_and_retains_adult_truth) {
    auto item = mod();
    item["contains_adult_content"] = true;
    const auto result = decode(item, NexusRecordKind::Mod, "another_game-2", 4451);
    decoded(result);
    CHECK_EQ(result.at("records").size(), 1U);
    CHECK_EQ(result.at("records")[0].at("contains_adult_content"), true);
    failed(decode(item, NexusRecordKind::Mod, "another_game-2", 4452));
    failed(decode(item, NexusRecordKind::Mod, "different"));
    failed(decode(Json::array({item}), NexusRecordKind::Mods, "another_game-2", 4452));
}

TEST(nexus_files_construct_download_source_without_download_uri) {
    auto item = file();
    item["URI"] = "https://evil.invalid/SecretMarker";
    item["size_kb"] = 1000000000000ULL;
    const auto result = decode(Json{{"files", Json::array({item})}}, NexusRecordKind::Files, "unlisted-game", 721);
    decoded(result);
    CHECK_EQ(result.at("records"), Json::array({{
        {"file_id", 92817}, {"name", "Content"}, {"file_name", "作品.zip"},
        {"version", "v2"}, {"description", ""}, {"size_kb", 1000000000000ULL},
        {"source_url", "https://www.nexusmods.com/unlisted-game/mods/721?tab=files&file_id=92817"}}}));
    CHECK(result.dump().find("SecretMarker") == std::string::npos);
    const auto fractional = decode(Json{{"files", Json::array({file()})}}, NexusRecordKind::Files, "unlisted-game", 721);
    decoded(fractional);
    CHECK_EQ(fractional.at("records")[0].at("size_kb"), 12.5);
}

TEST(nexus_files_keep_typed_upload_timestamp_hint) {
    auto dated = file();
    dated["uploaded_timestamp"] = 1758153600;
    auto record = decode(Json{{"files", Json::array({dated})}}, NexusRecordKind::Files, "unlisted-game", 721);
    decoded(record);
    CHECK_EQ(record.at("records")[0].at("uploaded_timestamp"), 1758153600U);
    CHECK(!decode(Json{{"files", Json::array({file()})}}, NexusRecordKind::Files, "unlisted-game", 721)
               .at("records")[0].contains("uploaded_timestamp"));
    for (const Json& bad : {Json(0), Json(-5), Json(1.5), Json("1758153600"), Json(nullptr),
                           Json(4102444801ULL), Json::array()}) {
        auto item = file();
        item["uploaded_timestamp"] = bad;
        record = decode(Json{{"files", Json::array({item})}}, NexusRecordKind::Files, "unlisted-game", 721);
        decoded(record);
        CHECK(record.at("records")[0].at("uploaded_timestamp").is_null());
    }
}

TEST(nexus_oauth_profile_derives_exact_roles_and_preference) {
    const Json input = {{"sub", "00053"}, {"name", "利用者"},
        {"membership_roles", Json::array({"premium", "Supporter", "other"})},
        {"preferences", {{"adult", true}}}, {"email", "SecretMarker"},
        {"access_token", "SecretMarker"}, {"is_supporter", true}};
    const auto result = decode(input, NexusRecordKind::OAuthUser);
    decoded(result);
    CHECK(!result.contains("records"));
    CHECK_EQ(result.at("profile"), Json({{"user_id", 53}, {"display_name", "利用者"},
        {"is_premium", true}, {"is_supporter", false}, {"adult_content", true}}));
    CHECK(result.dump().find("SecretMarker") == std::string::npos);
    auto supporter = input;
    supporter["membership_roles"] = Json::array({"supporter"});
    CHECK_EQ(decode(supporter, NexusRecordKind::OAuthUser).at("profile").at("is_supporter"), true);
}

TEST(nexus_adult_preference_unknown_is_null_and_api_key_never_infers_it) {
    Json oauth = {{"sub", "53"}, {"name", "User"}, {"membership_roles", Json::array()}};
    for (const auto& preference : {Json(), Json("true"), Json(1), Json::array()}) {
        oauth["preferences"] = {{"adult", preference}};
        const auto result = decode(oauth, NexusRecordKind::OAuthUser);
        decoded(result);
        CHECK(result.at("profile").at("adult_content").is_null());
    }
    auto input = api_user();
    input["preferences"] = {{"adult", true}};
    input["adult_content"] = true;
    const auto result = decode(input, NexusRecordKind::ApiKeyUser);
    decoded(result);
    CHECK_EQ(result.at("profile"), Json({{"user_id", 53}, {"display_name", "利用者"},
        {"is_premium", true}, {"is_supporter", false}, {"adult_content", nullptr}}));
}

TEST(nexus_identifiers_require_positive_integer_signed63_values) {
    for (const auto& id : {Json(0), Json(-1), Json(true), Json(1.0), Json("1"), Json(9223372036854775808ULL)}) {
        auto input = game(); input["id"] = id;
        failed(decode(Json::array({input}), NexusRecordKind::Games));
        auto profile = api_user(); profile["user_id"] = id;
        failed(decode(profile, NexusRecordKind::ApiKeyUser));
    }
}

TEST(nexus_domains_are_generic_strict_ascii_and_external_context_is_required) {
    for (const auto& domain : {"", "Upper", "-start", "a/b", "a.b", "遊戲"}) {
        auto input = game(); input["domain_name"] = domain;
        failed(decode(Json::array({input}), NexusRecordKind::Games));
        failed(decode(mod(), NexusRecordKind::Mod, domain));
    }
    auto input = game(); input["domain_name"] = std::string(80, 'a');
    decoded(decode(Json::array({input}), NexusRecordKind::Games));
    input["domain_name"] = std::string(81, 'a');
    failed(decode(Json::array({input}), NexusRecordKind::Games));
    failed(decode(mod(), NexusRecordKind::Mod));
    failed(decode(mod(), NexusRecordKind::Mod, "another_game-2", 9223372036854775808ULL));
    failed(decode(Json{{"files", Json::array()}}, NexusRecordKind::Files, "another_game-2", 0));
}

TEST(nexus_required_and_optional_field_types_are_not_coerced) {
    auto input = game();
    for (const auto& count : {Json(-1), Json(true), Json(2.0), Json("2"), Json()}) {
        input["mods"] = count;
        failed(decode(Json::array({input}), NexusRecordKind::Games));
    }
    auto item = mod(); item["available"] = "true";
    failed(decode(item, NexusRecordKind::Mod, "another_game-2"));
    item = mod(); item["author"] = nullptr;
    failed(decode(item, NexusRecordKind::Mod, "another_game-2"));
    item = mod(); item["summary"] = false;
    failed(decode(item, NexusRecordKind::Mod, "another_game-2"));
    auto profile = api_user(); profile["is_premium"] = 1;
    failed(decode(profile, NexusRecordKind::ApiKeyUser));
    failed(decode(Json::object(), NexusRecordKind::Games));
    failed(decode(Json::array(), NexusRecordKind::Mod, "another_game-2"));
    failed(decode(Json::array(), NexusRecordKind::Files, "another_game-2", 1));
    failed(decode(Json(), NexusRecordKind::OAuthUser));
}

TEST(nexus_files_reject_bad_sizes_and_missing_required_version) {
    for (const auto& size : {Json(-1), Json(true), Json("12"), Json(), Json(1000000000001ULL), Json(1000000000000.5)}) {
        auto input = file(); input["size_kb"] = size;
        failed(decode(Json{{"files", Json::array({input})}}, NexusRecordKind::Files, "generic", 9));
    }
    auto input = file(); input.erase("version");
    failed(decode(Json{{"files", Json::array({input})}}, NexusRecordKind::Files, "generic", 9));
    failed(decode(Json{{"files", Json::object()}}, NexusRecordKind::Files, "generic", 9));
}

TEST(nexus_oauth_identifiers_and_roles_are_bounded_exact_types) {
    Json input = {{"sub", "53"}, {"name", "User"}, {"membership_roles", Json::array()}};
    for (const auto& sub : {Json(53), Json("0"), Json("-1"), Json("+1"), Json(" 1"), Json("١"),
                            Json("9223372036854775808"), Json("00000000000000000001")}) {
        input["sub"] = sub;
        failed(decode(input, NexusRecordKind::OAuthUser));
    }
    input["sub"] = "53";
    for (const auto& roles : {Json(), Json::object(), Json::array({true}), Json::array({std::string(129, 'x')})}) {
        input["membership_roles"] = roles;
        failed(decode(input, NexusRecordKind::OAuthUser));
    }
    input["membership_roles"] = Json::array();
    for (unsigned i = 0; i < 100; ++i) input["membership_roles"].push_back("other");
    decoded(decode(input, NexusRecordKind::OAuthUser));
    input["membership_roles"].push_back("other");
    failed(decode(input, NexusRecordKind::OAuthUser));
}

TEST(nexus_text_limits_count_unicode_codepoints_and_reject_controls) {
    auto input = game(); input["name"] = repeated("界", 256);
    decoded(decode(Json::array({input}), NexusRecordKind::Games));
    input["name"] = repeated("界", 257);
    failed(decode(Json::array({input}), NexusRecordKind::Games));
    input["name"] = std::string("safe\0unsafe", 11);
    failed(decode(Json::array({input}), NexusRecordKind::Games));
    input["name"] = std::string("safe\x01unsafe");
    failed(decode(Json::array({input}), NexusRecordKind::Games));
    auto item = mod(); item["author"] = repeated("😀", 1024);
    item["summary"] = std::string(65536, 'x'); item["description"] = std::string(65536, 'y');
    item["version"] = repeated("界", 256);
    decoded(decode(item, NexusRecordKind::Mod, "another_game-2"));
    item["description"] = std::string(65537, 'y');
    failed(decode(item, NexusRecordKind::Mod, "another_game-2"));
}

TEST(nexus_late_record_failure_discards_the_entire_accepted_prefix) {
    auto first = game(); first["name"] = "AcceptedPrefixMarker";
    auto last = game(); last["id"] = 0;
    const auto result = decode(Json::array({first, last}), NexusRecordKind::Games);
    failed(result);
    CHECK(result.dump().find("AcceptedPrefixMarker") == std::string::npos);
    const auto malformed = decode(std::string_view("[" + first.dump() + ",{\"name\":\"SecretMarker\""), NexusRecordKind::Games);
    failed(malformed);
    CHECK(malformed.dump().find("SecretMarker") == std::string::npos);
    CHECK_EQ(result, malformed);
}

TEST(nexus_parser_rejects_duplicate_keys_invalid_utf8_and_trailing_data) {
    failed(decode(std::string_view("[{\"id\":37,\"\\u0069d\":37,\"domain_name\":\"generic\",\"name\":\"Name\"}]"), NexusRecordKind::Games));
    auto text = api_user().dump(); text.pop_back(); text += ",\"unknown\":{\"x\":1,\"x\":2}}";
    failed(decode(std::string_view(text), NexusRecordKind::ApiKeyUser));
    text = "[{\"id\":1,\"domain_name\":\"generic\",\"name\":\"";
    text.push_back(static_cast<char>(0xff)); text += "\"}]";
    failed(decode(std::string_view(text), NexusRecordKind::Games));
    for (std::string_view invalid : {"[] []", "[]SecretMarker", "", "[{\"id\":NaN}]"})
        failed(decode(invalid, NexusRecordKind::Games));
}

TEST(nexus_parser_bounds_depth_before_accepting_unknown_nested_values) {
    auto input = game(); Json nested = nullptr;
    for (unsigned i = 0; i < 62; ++i) nested = Json::array({std::move(nested)});
    input["unknown"] = nested;
    decoded(decode(Json::array({input}), NexusRecordKind::Games));
    input["unknown"] = Json::array({std::move(nested)});
    failed(decode(Json::array({input}), NexusRecordKind::Games), "resource_limit");
}

TEST(nexus_parser_bounds_each_array_even_when_unknown_fields_are_discarded) {
    auto input = api_user(); input["unknown"] = Json::array();
    for (unsigned i = 0; i < 10000; ++i) input["unknown"].push_back(0);
    decoded(decode(input, NexusRecordKind::ApiKeyUser));
    input["unknown"].push_back(0);
    failed(decode(input, NexusRecordKind::ApiKeyUser), "resource_limit");
}

TEST(nexus_parser_bounds_values_plus_keys_before_building_wide_objects) {
    auto prefix = api_user().dump(); prefix.pop_back(); prefix += ",\"unknown\":{";
    std::string fields;
    for (unsigned i = 0; i < 99990; ++i) {
        if (!fields.empty()) fields += ',';
        fields += "\"k" + std::to_string(i) + "\":0";
    }
    decoded(decode(std::string_view(prefix + fields + "}}"), NexusRecordKind::ApiKeyUser));
    for (unsigned i = 99990; i < 100000; ++i) fields += ",\"k" + std::to_string(i) + "\":0";
    failed(decode(std::string_view(prefix + fields + "}}"), NexusRecordKind::ApiKeyUser), "resource_limit");
}

TEST(nexus_input_byte_limit_rejects_oversize_without_a_prefix_result) {
    std::string text = "[]"; text.resize(8U * 1024U * 1024U, ' ');
    decoded(decode(std::string_view(text), NexusRecordKind::Games));
    text += ' ';
    failed(decode(std::string_view(text), NexusRecordKind::Games), "resource_limit");
}

TEST(nexus_metadata_never_returns_credentials_or_authorization_markers) {
    auto input = mod();
    for (const auto* key : {"apikey", "access_token", "refresh_token", "email", "URI", "raw_html"}) input[key] = "SecretMarker";
    for (const auto* key : {"verified", "authorized", "installed", "download_authorized", "visible"}) input[key] = true;
    const auto result = decode(input, NexusRecordKind::Mod, "another_game-2"); decoded(result);
    CHECK(result.dump().find("SecretMarker") == std::string::npos);
    for (const auto* key : {"verified", "authorized", "installed", "download_authorized", "visible"}) {
        CHECK(!result.contains(key)); CHECK(!result.at("records")[0].contains(key));
    }
    auto item = mod(); item.erase("contains_adult_content");
    CHECK(decode(item, NexusRecordKind::Mod, "another_game-2").at("records")[0].at("contains_adult_content").is_null());
}
