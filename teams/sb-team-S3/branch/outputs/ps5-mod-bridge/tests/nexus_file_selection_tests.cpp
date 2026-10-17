#include "modbridge/nexus.hpp"
#include "test.hpp"

#include <algorithm>
#include <limits>
#include <string_view>

namespace {
using modbridge::Json;
Json provider_file(std::uint64_t id = 7) {
    return {{"file_id", id}, {"name", "Source"}, {"file_name", "source.zip"},
        {"version", "file-label"}, {"size_kb", 4}};
}
Json decode_files(const Json& files) {
    const auto body = Json{{"files", files}}.dump();
    return modbridge::decode_nexus_metadata(
        {reinterpret_cast<const std::uint8_t*>(body.data()), body.size()},
        modbridge::NexusRecordKind::Files, "future_game-2", 71);
}
Json candidate(std::uint64_t id = 7, bool primary = true,
               unsigned category = 1, std::string_view name = "MAIN") {
    return {{"file_id", id}, {"is_primary", primary}, {"category_id", category}, {"category_name", name}};
}
void selected(const modbridge::NexusFileSelection& selection, std::uint64_t id, std::string_view basis) {
    CHECK(selection.selected); CHECK_EQ(selection.file_id, id);
    CHECK(selection.basis != nullptr); CHECK(selection.reason != nullptr);
    CHECK_EQ(std::string_view(selection.basis), basis);
    CHECK_EQ(std::string_view(selection.reason), "none");
}
void refused(const modbridge::NexusFileSelection& selection, std::string_view reason) {
    CHECK(!selection.selected); CHECK_EQ(selection.file_id, 0U);
    CHECK(selection.basis != nullptr); CHECK(selection.reason != nullptr);
    CHECK_EQ(std::string_view(selection.basis), "none");
    CHECK_EQ(std::string_view(selection.reason), reason);
}
}

TEST(nexus_file_metadata_preserves_typed_author_primary_and_category_hints) {
    auto file = provider_file();
    file["is_primary"] = true; file["category_id"] = 1; file["category_name"] = "MAIN";
    const auto decoded = decode_files(Json::array({file}));
    CHECK_EQ(decoded.at("status"), "decoded");
    CHECK_EQ(decoded.at("records")[0].at("is_primary"), true);
    CHECK_EQ(decoded.at("records")[0].at("category_id"), 1);
    CHECK_EQ(decoded.at("records")[0].at("category_name"), "MAIN");
    CHECK_EQ(decoded.at("records")[0].at("version"), "file-label");
}

TEST(nexus_file_metadata_wrong_hint_types_are_nullable_unknown) {
    auto file = provider_file();
    file["is_primary"] = "true"; file["category_id"] = 1.0; file["category_name"] = true;
    const auto decoded = decode_files(Json::array({file}));
    CHECK_EQ(decoded.at("status"), "decoded");
    CHECK(decoded.at("records")[0].at("is_primary").is_null());
    CHECK(decoded.at("records")[0].at("category_id").is_null());
    CHECK(decoded.at("records")[0].at("category_name").is_null());
}

TEST(nexus_file_metadata_missing_hints_preserve_old_shape_and_remain_unknown) {
    const auto decoded = decode_files(Json::array({provider_file()}));
    const auto& file = decoded.at("records")[0];
    CHECK(!file.contains("is_primary")); CHECK(!file.contains("category_id")); CHECK(!file.contains("category_name"));
    refused(modbridge::nexus_select_file(decoded.at("records"), 0, true), "file_hints_unknown");
    selected(modbridge::nexus_select_file(decoded.at("records"), 7, false), 7, "explicit_file");
}

TEST(nexus_file_metadata_category_hints_reject_coercion_and_bound_text) {
    for (const auto& hint : {Json(nullptr), Json(true), Json(1.0), Json("1"), Json(-1), Json(0), Json(UINT64_MAX), Json::array()}) {
        auto file = provider_file(); file["category_id"] = hint;
        const auto result = decode_files(Json::array({file}));
        CHECK_EQ(result.at("status"), "decoded"); CHECK(result.at("records")[0].at("category_id").is_null());
    }
    auto file = provider_file(); file["category_id"] = 5; file["category_name"] = std::string(257, 'x');
    auto result = decode_files(Json::array({file}));
    CHECK_EQ(result.at("records")[0].at("category_id"), 5);
    CHECK(result.at("records")[0].at("category_name").is_null());
    file["category_name"] = std::string(256, 'x');
    result = decode_files(Json::array({file}));
    CHECK_EQ(result.at("records")[0].at("category_name"), std::string(256, 'x'));
}

TEST(nexus_file_selection_explicit_exact_membership_deliberately_allows_historical) {
    const auto files = Json::array({candidate(9, false, 7, "ARCHIVED"), candidate(3)});
    selected(modbridge::nexus_select_file(files, 9, false), 9, "explicit_file");
    selected(modbridge::nexus_select_file(files, 9, true), 9, "explicit_file");
    refused(modbridge::nexus_select_file(files, 99, true), "explicit_file_not_found");
    refused(modbridge::nexus_select_file(files, 0, false), "file_choice_required");
    refused(modbridge::nexus_select_file(files, UINT64_MAX, false), "invalid_explicit_file_id");
}

TEST(nexus_file_selection_unique_main_uses_hints_without_id_or_version_sorting) {
    auto current = provider_file(3); current["is_primary"] = true; current["category_id"] = 1;
    current["category_name"] = "MAIN"; current["version"] = "older-file-label";
    auto history = provider_file(999); history["is_primary"] = true; history["category_id"] = 7;
    history["category_name"] = "ARCHIVED"; history["version"] = "99999.0";
    auto patch = provider_file(1000); patch["is_primary"] = false; patch["category_id"] = 2;
    patch["category_name"] = "PATCH"; patch["name"] = "Required Latest Version";
    const auto decoded = decode_files(Json::array({history, patch, current}));
    selected(modbridge::nexus_select_file(decoded.at("records"), 0, true), 3, "unique_current_primary");
    CHECK_EQ(decoded.at("installation_supported"), false);
}

TEST(nexus_file_selection_automatic_historical_patch_optional_deleted_are_not_main) {
    for (const auto& pair : {std::pair{2U, "PATCH"}, std::pair{3U, "OPTION"},
             std::pair{4U, "OLD_VERSION"}, std::pair{6U, "DELETED"}, std::pair{7U, "ARCHIVED"}}) {
        const auto files = Json::array({candidate(9, true, pair.first, pair.second)});
        refused(modbridge::nexus_select_file(files, 0, true), "no_current_primary");
    }
    refused(modbridge::nexus_select_file(Json::array({candidate(3, false)}), 0, true), "no_current_primary");
}

TEST(nexus_file_selection_automatic_unknown_hints_refuse_even_after_current_candidate) {
    for (const auto* key : {"is_primary", "category_id", "category_name"}) {
        auto unknown = candidate(9); unknown.erase(key);
        refused(modbridge::nexus_select_file(Json::array({candidate(3), unknown}), 0, true), "file_hints_unknown");
        unknown[key] = nullptr;
        refused(modbridge::nexus_select_file(Json::array({candidate(3), unknown}), 0, true), "file_hints_unknown");
    }
    auto unknown = candidate(9); unknown["is_primary"] = "true";
    refused(modbridge::nexus_select_file(Json::array({candidate(3), unknown}), 0, true), "file_hints_unknown");
    unknown = candidate(9); unknown["category_id"] = 1.0;
    refused(modbridge::nexus_select_file(Json::array({candidate(3), unknown}), 0, true), "file_hints_unknown");
    unknown = candidate(9, false, 5, "MISCELLANEOUS");
    refused(modbridge::nexus_select_file(Json::array({candidate(3), unknown}), 0, true), "file_hints_unknown");
    selected(modbridge::nexus_select_file(Json::array({candidate(3), unknown}), 3, true), 3, "explicit_file");
}

TEST(nexus_file_selection_automatic_category_contradictions_never_choose_prefix) {
    const auto files = Json::array({candidate(3), candidate(9, false, 1, "ARCHIVED")});
    refused(modbridge::nexus_select_file(files, 0, true), "file_hints_contradictory");
    selected(modbridge::nexus_select_file(files, 3, true), 3, "explicit_file");
    refused(modbridge::nexus_select_file(Json::array({candidate(9, false, 7, "MAIN")}), 0, true), "file_hints_contradictory");
}

TEST(nexus_file_selection_multiple_main_candidates_are_ambiguous) {
    auto files = Json::array({candidate(99), candidate(3)});
    refused(modbridge::nexus_select_file(files, 0, true), "ambiguous_current_primary");
    std::reverse(files.begin(), files.end());
    refused(modbridge::nexus_select_file(files, 0, true), "ambiguous_current_primary");
    selected(modbridge::nexus_select_file(files, 3, true), 3, "explicit_file");
}

TEST(nexus_file_selection_full_unique_id_set_validates_before_explicit_or_primary) {
    const auto duplicate = Json::array({candidate(3), candidate(3, false, 7, "ARCHIVED")});
    refused(modbridge::nexus_select_file(duplicate, 3, false), "duplicate_file_id");
    refused(modbridge::nexus_select_file(duplicate, 0, true), "duplicate_file_id");
    for (const auto& id : {Json(0), Json(-1), Json(true), Json(3.0), Json("3"), Json(9223372036854775808ULL)}) {
        auto bad = candidate(9); bad["file_id"] = id;
        refused(modbridge::nexus_select_file(Json::array({candidate(3), bad}), 3, true), "invalid_file_metadata");
    }
    auto bad = candidate(9); bad.erase("file_id");
    refused(modbridge::nexus_select_file(Json::array({candidate(3), bad}), 3, false), "invalid_file_metadata");
    refused(modbridge::nexus_select_file(Json::array({candidate(3), nullptr}), 3, true), "invalid_file_metadata");
    refused(modbridge::nexus_select_file(Json::object(), 3, true), "invalid_file_metadata");
}

TEST(nexus_file_selection_empty_sets_and_signed63_boundary_are_explicit) {
    refused(modbridge::nexus_select_file(Json::array(), 7, true), "explicit_file_not_found");
    refused(modbridge::nexus_select_file(Json::array(), 0, true), "no_current_primary");
    refused(modbridge::nexus_select_file(Json::array(), 0, false), "file_choice_required");
    const auto files = Json::array({candidate(9223372036854775807ULL)});
    selected(modbridge::nexus_select_file(files, 9223372036854775807ULL, false),
        9223372036854775807ULL, "explicit_file");
}

TEST(nexus_file_selection_bound_is_checked_before_any_positive_choice) {
    Json files = Json::array();
    for (unsigned i = 1; i <= 10000; ++i) files.push_back(Json{{"file_id", i}});
    selected(modbridge::nexus_select_file(files, 10000, false), 10000, "explicit_file");
    files.push_back(Json{{"file_id", 10001}});
    refused(modbridge::nexus_select_file(files, 10000, false), "file_list_limit");
}

TEST(nexus_file_selection_inputs_and_selection_reason_are_order_independent) {
    auto files = Json::array({candidate(3), candidate(8, false, 7, "ARCHIVED"), candidate(9, false, 2, "PATCH")});
    const auto saved = files;
    selected(modbridge::nexus_select_file(files, 0, true), 3, "unique_current_primary"); CHECK_EQ(files, saved);
    std::reverse(files.begin(), files.end());
    selected(modbridge::nexus_select_file(files, 0, true), 3, "unique_current_primary");
    auto unknown = candidate(99); unknown.erase("category_name");
    files.push_back(candidate(100, false, 7, "MAIN")); files.push_back(unknown);
    refused(modbridge::nexus_select_file(files, 0, true), "file_hints_unknown");
    std::reverse(files.begin(), files.end());
    refused(modbridge::nexus_select_file(files, 0, true), "file_hints_unknown");
}
