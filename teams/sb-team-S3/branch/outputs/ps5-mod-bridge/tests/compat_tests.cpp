#include "test.hpp"
#include "modbridge/compat.hpp"
#include <stdexcept>
#include <string>
using namespace modbridge;
namespace {

std::string S(const Json& value) { return value.get<std::string>(); }

// Every invalid input must be an std::invalid_argument with the "compat: " prefix.
#define CHECK_COMPAT_THROWS(expression) do { bool caught_ = false; \
    try { (void)(expression); } \
    catch (const std::invalid_argument& error_) { \
        caught_ = std::string(error_.what()).rfind("compat: ", 0) == 0; } \
    CHECK(caught_); } while (false)

Json build_entry(const char* version, Json pc) {
    return {{"console_content_version", version}, {"pc_build", pc}};
}
Json pc(const char* label, std::uint64_t released) {
    return {{"label", label}, {"released_utc", released}};
}
// Windows: 01 = [1000,2000), 02 = [2000,4000) (03 has no known PC build and is
// skipped), 03 = unknown, 04 = [4000,inf).
Json builds() {
    return Json::array({build_entry("01.000.000", pc("1.00", 1000)), build_entry("02.000.000", pc("1.01", 2000)),
                        build_entry("03.000.000", nullptr), build_entry("04.000.000", pc("1.03", 4000))});
}
Json content_index() {
    return {{"console_content_version", "02.000.000"},
            {"complete", {{"assets", true}, {"classes", false}, {"functions", false}}},
            {"assets", Json::array({"/Game/B/Y", "/Game/A/X", "/Game/A/X"})},
            {"classes", Json::array({"/Script/Engine.Actor", "/Game/X/BP_Y.BP_Y_C"})},
            {"functions", Json::array({"/Script/Engine.Actor:K2_DestroyActor"})}};
}
Json profile_without_index() {
    return {{"schema", "modbridge.game-profile/1"}, {"title_id", "PPSA33286"},
            {"nexus_domain", "silenthilltownfall"}, {"engine", {{"family", "unreal"}, {"version", "5.6"}}},
            {"builds", builds()}};
}
Json profile() {
    auto result = profile_without_index();
    result["content_index"] = content_index();
    return result;
}
Json param(const char* version) { return {{"contentVersion", version}}; }

// ts == 0 leaves the optional uploaded_timestamp absent.
Json file(std::uint64_t id, std::uint64_t ts = 0) {
    Json record = {{"file_id", id}, {"name", "n"}, {"file_name", "f"}, {"version", "1"},
                   {"description", ""}, {"size_kb", 1}, {"source_url", "u"}};
    if (ts != 0) record["uploaded_timestamp"] = ts;
    return record;
}
Json files(std::initializer_list<Json> items) {
    Json result = Json::array();
    for (const auto& item : items) result.push_back(item);
    return result;
}
Json list(std::initializer_list<const char*> items) {
    Json result = Json::array();
    for (const char* item : items) result.push_back(item);
    return result;
}
Json refs(Json assets = Json::array(), Json classes = Json::array(), Json functions = Json::array(),
          Json shorts = Json::array(), bool dynamic = false) {
    return {{"assets", assets}, {"classes", classes}, {"functions", functions},
            {"short_class_names", shorts}, {"dynamic", dynamic}};
}
Json select(std::string_view version, const Json& file_list, std::uint64_t explicit_id = 0) {
    return select_mod_file_for_build(profile(), version, file_list, explicit_id);
}
std::string verdict_of(const Json& selection, std::uint64_t id) {
    for (const auto& candidate : selection.at("candidates"))
        if (candidate.at("file_id").get<std::uint64_t>() == id) return S(candidate.at("verdict"));
    return "<absent>";
}
std::string codes(const Json& result) {
    std::string out;
    for (const auto& reason : result.at("reasons")) out += (out.empty() ? "" : ",") + S(reason.at("code"));
    return out;
}
Json primary_file(std::uint64_t id, std::uint64_t ts) {
    auto record = file(id, ts);
    record["is_primary"] = true;
    return record;
}
// A complete, present reference set for the default index/build 02.
Json present_refs() {
    return refs(list({"/Game/B/Y"}), list({"/Script/Engine.Actor"}), list({"/Script/Engine.Actor:K2_DestroyActor"}),
                list({"Actor"}));
}

} // namespace

// ---------------------------------------------------------------- profile
TEST(profile_valid_is_normalized) {
    const auto normalized = validate_game_profile(profile());
    CHECK_EQ(S(normalized.at("schema")), "modbridge.game-profile/1");
    CHECK_EQ(S(normalized.at("title_id")), "PPSA33286");
    CHECK_EQ(normalized.at("builds").size(), 4u);
    CHECK(normalized.at("builds")[2].at("pc_build").is_null());
    CHECK_EQ(normalized.at("builds")[1].at("pc_build").at("released_utc").get<std::uint64_t>(), 2000u);
    const auto& index = normalized.at("content_index");
    CHECK_EQ(index.at("assets").dump(), R"(["/Game/A/X","/Game/B/Y"])");
    CHECK_EQ(index.at("classes").dump(), R"(["/Game/X/BP_Y.BP_Y_C","/Script/Engine.Actor"])");
    // Normalizing twice is stable.
    CHECK_EQ(validate_game_profile(normalized).dump(), normalized.dump());
    CHECK(!validate_game_profile(profile_without_index()).contains("content_index"));
}

TEST(profile_rejects_invalid) {
    auto with = [](auto mutate) { auto p = profile(); mutate(p); return p; };
    CHECK_COMPAT_THROWS(validate_game_profile(Json::array()));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["schema"] = "modbridge.game-profile/2"; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p.erase("schema"); })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["extra"] = 1; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["title_id"] = "ppsa33286"; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["title_id"] = "PPSA3328"; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["nexus_domain"] = "Silent-Hill"; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["nexus_domain"] = ""; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["engine"]["family"] = "godot"; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["engine"]["version"] = ""; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["builds"] = Json::array(); })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["builds"] = "x"; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["builds"][0]["console_content_version"] = "1.0.0"; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["builds"][1]["console_content_version"] = "01.000.000"; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["builds"][0].erase("pc_build"); })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["builds"][0]["pc_build"]["released_utc"] = 0; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["builds"][0]["pc_build"]["released_utc"] = "1000"; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["builds"][0]["pc_build"]["released_utc"] = 1000.5; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["builds"][0]["pc_build"]["label"] = ""; })));
    // Release dates must strictly increase over the non-null builds.
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["builds"][1]["pc_build"]["released_utc"] = 1000; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["builds"][3]["pc_build"]["released_utc"] = 1500; })));
    // Index.
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["content_index"]["console_content_version"] = "09.000.000"; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["content_index"]["complete"].erase("classes"); })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["content_index"]["complete"]["assets"] = 1; })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["content_index"].erase("functions"); })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["content_index"]["assets"] = list({"/Game/A/X.X"}); })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["content_index"]["assets"] = list({"Game/A/X"}); })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["content_index"]["classes"] = list({"/Script/Engine"}); })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["content_index"]["functions"] = list({"/Script/Engine.Actor"}); })));
    CHECK_COMPAT_THROWS(validate_game_profile(with([](Json& p) { p["content_index"]["classes"] = Json::array({1}); })));
}

// ----------------------------------------------------------- console version
TEST(console_game_version_codes) {
    auto known = console_game_version(param("02.003.000"));
    CHECK(known.at("known").get<bool>());
    CHECK_EQ(S(known.at("content_version")), "02.003.000");
    // Other param.json fields are ignored.
    auto extra = console_game_version(Json{{"contentVersion", "01.000.000"}, {"titleId", "X"}, {"versionFileUri", 3}});
    CHECK(extra.at("known").get<bool>());
    for (const auto& input : {Json::object(), Json::array(), Json("x"), Json(nullptr), Json{{"contentversion", "02.003.000"}}}) {
        auto result = console_game_version(input);
        CHECK(!result.at("known").get<bool>());
        CHECK_EQ(S(result.at("reason")), "missing_content_version");
    }
    for (const auto& value : {Json("2.3.0"), Json("02.003.00"), Json("02.003.0000"), Json("aa.bbb.ccc"), Json(" 02.003.000"),
                              Json(""), Json(2003000), Json(nullptr), Json(true), Json::array()}) {
        auto result = console_game_version(Json{{"contentVersion", value}});
        CHECK(!result.at("known").get<bool>());
        CHECK_EQ(S(result.at("reason")), "invalid_content_version");
    }
}

// --------------------------------------------------------------- selection
TEST(select_fits_window_boundaries) {
    // Build 01 window is [1000, 2000).
    auto result = select("01.000.000", files({file(1, 999), file(2, 1000), file(3, 1999), file(4, 2000), file(5, 5000), file(6)}));
    CHECK_EQ(verdict_of(result, 1), "older_build");
    CHECK_EQ(verdict_of(result, 2), "fits");
    CHECK_EQ(verdict_of(result, 3), "fits");
    CHECK_EQ(verdict_of(result, 4), "newer_than_build");
    CHECK_EQ(verdict_of(result, 5), "newer_than_build");
    CHECK_EQ(verdict_of(result, 6), "undated");
    CHECK_EQ(S(result.at("console_content_version")), "01.000.000");
    CHECK_EQ(result.at("pc_build").at("label").get<std::string>(), "1.00");
    CHECK_EQ(result.at("pc_build").at("released_utc").get<std::uint64_t>(), 1000u);
    // Files 2 and 3 both fit and have distinct timestamps: newest wins.
    CHECK_EQ(S(result.at("status")), "selected");
    CHECK_EQ(result.at("selected_file_id").get<std::uint64_t>(), 3u);
    // Candidates are sorted by file id.
    CHECK_EQ(result.at("candidates")[0].at("file_id").get<std::uint64_t>(), 1u);
    CHECK_EQ(result.at("candidates")[5].at("file_id").get<std::uint64_t>(), 6u);
}

TEST(select_single_fit_and_open_ended_last_build) {
    auto single = select("02.000.000", files({file(1, 1500), file(2, 2500), file(3, 4000)}));
    CHECK_EQ(S(single.at("status")), "selected");
    CHECK_EQ(single.at("selected_file_id").get<std::uint64_t>(), 2u);
    CHECK_EQ(verdict_of(single, 1), "older_build");
    // 03 has no known date, so build 02's window extends to build 04's release.
    CHECK_EQ(verdict_of(single, 3), "newer_than_build");
    auto last = select("04.000.000", files({file(1, 3999), file(2, 4000), file(3, 4000000000ull)}));
    CHECK_EQ(verdict_of(last, 1), "older_build");
    CHECK_EQ(verdict_of(last, 2), "fits");
    CHECK_EQ(verdict_of(last, 3), "fits");
    CHECK_EQ(last.at("selected_file_id").get<std::uint64_t>(), 3u);
}

TEST(select_unknown_build_cases) {
    // Known console version whose PC patch is not known.
    auto null_pc = select("03.000.000", files({file(1, 2500)}));
    CHECK_EQ(S(null_pc.at("status")), "unknown_build");
    CHECK(null_pc.at("pc_build").is_null());
    CHECK(null_pc.at("selected_file_id").is_null());
    CHECK_EQ(null_pc.at("candidates").size(), 0u);
    // Explicit ids cannot override an unknown build.
    CHECK_EQ(S(select("03.000.000", files({file(1, 2500)}), 1).at("status")), "unknown_build");
    // Well-formed but unlisted version.
    CHECK_EQ(S(select("09.000.000", files({file(1, 2500)})).at("status")), "unknown_build");
    // Malformed version text is a caller error.
    CHECK_COMPAT_THROWS(select("9.0", files({})));
    CHECK_COMPAT_THROWS(select("", files({})));
}

TEST(select_no_fit_and_empty) {
    auto none = select("02.000.000", files({}));
    CHECK_EQ(S(none.at("status")), "no_fit");
    CHECK(none.at("selected_file_id").is_null());
    auto only_old = select("02.000.000", files({file(1, 100), file(2, 9000)}));
    CHECK_EQ(S(only_old.at("status")), "no_fit");
    CHECK_EQ(only_old.at("candidates").size(), 2u);
}

TEST(select_undated_absent_null_and_unusable) {
    auto a = file(1);
    auto b = file(2);
    b["uploaded_timestamp"] = nullptr;
    auto c = file(3);
    c["uploaded_timestamp"] = 0;
    auto d = file(4);
    d["uploaded_timestamp"] = "2500";
    auto e = file(5);
    e["uploaded_timestamp"] = -5;
    auto f = file(6);
    f["uploaded_timestamp"] = 2500.5;
    auto result = select("02.000.000", files({a, b, c, d, e, f}));
    for (std::uint64_t id = 1; id <= 6; ++id) CHECK_EQ(verdict_of(result, id), "undated");
    CHECK_EQ(S(result.at("status")), "no_fit");
}

TEST(select_retired_categories) {
    auto old_version = file(1, 2500);
    old_version["category_name"] = "OLD_VERSION";
    auto deleted = file(2, 2500);
    deleted["category_name"] = "DELETED";
    auto archived = file(3, 2500);
    archived["category_name"] = "ARCHIVED";
    auto optional = file(4, 2500);
    optional["category_name"] = "OPTIONAL";
    auto result = select("02.000.000", files({old_version, deleted, archived, optional}));
    CHECK_EQ(verdict_of(result, 1), "retired");
    CHECK_EQ(verdict_of(result, 2), "retired");
    CHECK_EQ(verdict_of(result, 3), "retired");
    CHECK_EQ(verdict_of(result, 4), "fits");
    CHECK_EQ(result.at("selected_file_id").get<std::uint64_t>(), 4u);
    // Retired wins over every dating verdict, and a retired file is never selected
    // even when flagged primary.
    auto retired_primary = primary_file(5, 2500);
    retired_primary["category_name"] = "ARCHIVED";
    auto only_retired = select("02.000.000", files({retired_primary}));
    CHECK_EQ(verdict_of(only_retired, 5), "retired");
    CHECK_EQ(S(only_retired.at("status")), "no_fit");
    CHECK(only_retired.at("candidates")[0].at("primary").get<bool>());
    // A retired primary hint does not suppress the fallback to all candidates.
    auto mixed = select("02.000.000", files({retired_primary, file(6, 2400)}));
    CHECK_EQ(mixed.at("selected_file_id").get<std::uint64_t>(), 6u);
    // Null category_name is not retired.
    auto null_name = file(7, 2500);
    null_name["category_name"] = nullptr;
    CHECK_EQ(S(select("02.000.000", files({null_name})).at("status")), "selected");
}

TEST(select_primary_hints_and_fallback) {
    auto main_file = file(1, 2100);
    main_file["category_name"] = "MAIN";
    auto optional = file(2, 3000);
    optional["category_name"] = "OPTIONAL";
    auto hinted = select("02.000.000", files({main_file, optional}));
    CHECK_EQ(verdict_of(hinted, 2), "fits"); // verdict is independent of the primary hint
    CHECK(hinted.at("candidates")[0].at("primary").get<bool>());
    CHECK(!hinted.at("candidates")[1].at("primary").get<bool>());
    CHECK_EQ(hinted.at("selected_file_id").get<std::uint64_t>(), 1u); // newer optional file ignored

    auto flagged = select("02.000.000", files({file(3, 3000), primary_file(4, 2100)}));
    CHECK_EQ(flagged.at("selected_file_id").get<std::uint64_t>(), 4u);

    // is_primary false / null carry no hint; with no hints all non-retired files compete.
    auto not_primary = file(5, 2200);
    not_primary["is_primary"] = false;
    auto null_primary = file(6, 2300);
    null_primary["is_primary"] = nullptr;
    auto fallback = select("02.000.000", files({not_primary, null_primary}));
    CHECK_EQ(S(fallback.at("status")), "selected");
    CHECK_EQ(fallback.at("selected_file_id").get<std::uint64_t>(), 6u);
    CHECK(!fallback.at("candidates")[0].at("primary").get<bool>());

    // Primary candidates exist but none fits: other fitting files are not substituted.
    auto stale_primary = select("02.000.000", files({primary_file(7, 1500), file(8, 2500)}));
    CHECK_EQ(S(stale_primary.at("status")), "no_fit");
}

TEST(select_newest_unique_upload_and_ties) {
    auto unique = select("02.000.000", files({file(1, 2100), file(2, 2900), file(3, 2500)}));
    CHECK_EQ(S(unique.at("status")), "selected");
    CHECK_EQ(unique.at("selected_file_id").get<std::uint64_t>(), 2u);

    auto tie = select("02.000.000", files({file(1, 2100), file(2, 2900), file(3, 2900)}));
    CHECK_EQ(S(tie.at("status")), "ambiguous");
    CHECK(tie.at("selected_file_id").is_null());
    CHECK_EQ(tie.at("candidates").size(), 3u);

    // A tie below the newest upload does not matter.
    auto lower_tie = select("02.000.000", files({file(1, 2100), file(2, 2100), file(3, 2900)}));
    CHECK_EQ(lower_tie.at("selected_file_id").get<std::uint64_t>(), 3u);

    // A tie with a non-candidate (non-primary) file is not a tie.
    auto hinted_tie = select("02.000.000", files({primary_file(1, 2100), primary_file(2, 2900), file(3, 2900)}));
    CHECK_EQ(hinted_tie.at("selected_file_id").get<std::uint64_t>(), 2u);
    // Equal timestamps outside the window are irrelevant.
    auto outside = select("02.000.000", files({file(1, 2500), file(2, 5000), file(3, 5000)}));
    CHECK_EQ(outside.at("selected_file_id").get<std::uint64_t>(), 1u);
}

TEST(select_explicit_file_id) {
    // Overrides selection but keeps its own verdict.
    auto older = select("02.000.000", files({file(1, 1500), file(2, 2500)}), 1);
    CHECK_EQ(S(older.at("status")), "selected");
    CHECK_EQ(older.at("selected_file_id").get<std::uint64_t>(), 1u);
    CHECK_EQ(verdict_of(older, 1), "older_build");
    // Overrides ambiguity and primary hints.
    auto tie = select("02.000.000", files({file(1, 2900), file(2, 2900), primary_file(3, 2100)}), 2);
    CHECK_EQ(tie.at("selected_file_id").get<std::uint64_t>(), 2u);
    // Retired and undated files can be chosen explicitly.
    auto retired = file(4, 2500);
    retired["category_name"] = "ARCHIVED";
    auto explicit_retired = select("02.000.000", files({retired}), 4);
    CHECK_EQ(S(explicit_retired.at("status")), "selected");
    CHECK_EQ(verdict_of(explicit_retired, 4), "retired");
    CHECK_EQ(S(select("02.000.000", files({file(5)}), 5).at("status")), "selected");
    // Unknown id.
    auto missing = select("02.000.000", files({file(1, 2500)}), 99);
    CHECK_EQ(S(missing.at("status")), "explicit_file_not_found");
    CHECK(missing.at("selected_file_id").is_null());
    CHECK_EQ(missing.at("candidates").size(), 1u);
    // Zero means "not set".
    CHECK_EQ(select("02.000.000", files({file(1, 2500)}), 0).at("selected_file_id").get<std::uint64_t>(), 1u);
}

TEST(select_validates_inputs) {
    CHECK_COMPAT_THROWS(select("02.000.000", Json::object()));
    CHECK_COMPAT_THROWS(select("02.000.000", Json::array({1})));
    CHECK_COMPAT_THROWS(select("02.000.000", files({file(1, 2500), file(1, 2600)})));
    auto no_id = file(1, 2500);
    no_id.erase("file_id");
    CHECK_COMPAT_THROWS(select("02.000.000", files({no_id})));
    auto zero_id = file(1, 2500);
    zero_id["file_id"] = 0;
    CHECK_COMPAT_THROWS(select("02.000.000", files({zero_id})));
    auto string_id = file(1, 2500);
    string_id["file_id"] = "1";
    CHECK_COMPAT_THROWS(select("02.000.000", files({string_id})));
    auto bad_primary = file(1, 2500);
    bad_primary["is_primary"] = "yes";
    CHECK_COMPAT_THROWS(select("02.000.000", files({bad_primary})));
    auto bad_category = file(1, 2500);
    bad_category["category_id"] = "x";
    CHECK_COMPAT_THROWS(select("02.000.000", files({bad_category})));
    auto bad_name = file(1, 2500);
    bad_name["category_name"] = 7;
    CHECK_COMPAT_THROWS(select("02.000.000", files({bad_name})));
    auto good_category = file(1, 2500);
    good_category["category_id"] = 1;
    CHECK_EQ(S(select("02.000.000", files({good_category})).at("status")), "selected");
    // The profile is validated as well.
    auto bad_profile = profile();
    bad_profile["title_id"] = "x";
    CHECK_COMPAT_THROWS(select_mod_file_for_build(bad_profile, "02.000.000", files({}), 0));
}

// ------------------------------------------------------------ Lua extraction
TEST(extract_townfall_script_snippet) {
    // Condensed copy of the relevant parts of TownfallItemSpawner/Scripts/main.lua.
    const std::string source = R"LUA(
-- Townfall Item Spawner v1.0
-- Clean Nexus release build for UE4SS. F7 opens/closes the menu.
-- local ignored = StaticFindObject("/Game/Commented/Out")
local MOD_TAG = "[TownfallItemSpawner]"
local function log(s) print(MOD_TAG .. " " .. tostring(s) .. "\n") end

local function getPlayerController()
    local ok, UEHelpers = pcall(require, "UEHelpers")
    local pc = FindFirstOf("PlayerController")
    return pc
end

local function class(path)
    local c = StaticFindObject(path)
    if not isValid(c) then error("Could not find class: " .. path) end
    return c
end

local function database()
    local db = FindFirstOf("ItemDatabaseSubsystem")
    return db
end

local extraPickupClasses = {
    {name="Axe", path="/Game/Townfall/AlwaysCook/Items/Weapon_Items/BP_Weapon_Axe"},
    {name="Pistol3000", path="/Game/Townfall/AlwaysCook/Items/Weapon_Items/BP_Weapon_Pistol3000"},
    {name="Antibiotics", path="/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Antibiotics"},
    {name="Empty IV Bag", path="/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Empty_IV_Bag"},
    {name="Filled IV Bag", path="/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Filled_IV_Bag"},
    {name="Enhanced IV Bag", path="/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Enhanced_IV_Bag"},
}

local function normalizeClassPath(path)
    if not path or path == "" or path == "None" then return nil end
    path = path:match("'([^']+)'") or path
    if not path:find(".", 1, true) then
        local asset = path:match("([^/]+)$")
        path = path .. "." .. asset
    end
    if path:sub(-2) ~= "_C" then path = path .. "_C" end
    return path
end

local function getPickupComponent(actor)
    local ok, result = pcall(function()
        return actor:GetComponentByClass(class("/Script/Townfall.PickupInteractable"))
    end)
    return result
end

local function spawn(entry)
    local system = class("/Script/Engine.Default__KismetSystemLibrary")
    local pickupClass = StaticFindObject(path)
    local gameplay = class("/Script/Engine.Default__GameplayStatics")
    pcall(function() actor:K2_DestroyActor() end)
    ui.tree = construct("/Script/UMG.WidgetTree", ui.root, "TownfallItemSpawnerWidgetTree")
    ui.canvas = construct("/Script/UMG.CanvasPanel", ui.tree, "TownfallItemSpawnerCanvas")
    ui.panel = construct("/Script/UMG.Border", ui.tree, "TownfallItemSpawnerFrame")
    ui.box = construct("/Script/UMG.VerticalBox", ui.tree, "TownfallItemSpawnerVBox")
    local w = construct("/Script/UMG.TextBlock", ui.tree, name)
    local widgetLib = StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary")
    local userWidgetClass = class("/Script/UMG.UserWidget")
end
)LUA";
    const auto result = extract_lua_references(source);
    CHECK_EQ(result.at("assets").dump(),
             R"(["/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Antibiotics",)"
             R"("/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Empty_IV_Bag",)"
             R"("/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Enhanced_IV_Bag",)"
             R"("/Game/Townfall/AlwaysCook/Items/Collectable_Items/BP_Item_Filled_IV_Bag",)"
             R"("/Game/Townfall/AlwaysCook/Items/Weapon_Items/BP_Weapon_Axe",)"
             R"("/Game/Townfall/AlwaysCook/Items/Weapon_Items/BP_Weapon_Pistol3000"])");
    CHECK_EQ(result.at("classes").dump(),
             R"(["/Script/Engine.GameplayStatics","/Script/Engine.KismetSystemLibrary",)"
             R"("/Script/Townfall.PickupInteractable","/Script/UMG.Border","/Script/UMG.CanvasPanel",)"
             R"("/Script/UMG.TextBlock","/Script/UMG.UserWidget","/Script/UMG.VerticalBox",)"
             R"("/Script/UMG.WidgetBlueprintLibrary","/Script/UMG.WidgetTree"])");
    CHECK_EQ(result.at("functions").dump(), "[]");
    CHECK_EQ(result.at("short_class_names").dump(), R"(["ItemDatabaseSubsystem","PlayerController"])");
    CHECK(result.at("dynamic").get<bool>()); // StaticFindObject(path)
    CHECK_EQ(result.size(), 5u);
}

TEST(extract_reference_kinds) {
    const auto result = extract_lua_references(R"LUA(
local a = "/Game/Pkg/Dir/Thing"
local b = "/Game/Pkg/BP_Y.BP_Y_C"
local c = "/Game/Pkg/BP_Z.Default__BP_Z_C"
local d = "/Game/Pkg/Mesh.Mesh"
local e = "/Script/Engine.Actor"
local f = "/Script/Engine.Actor:K2_DestroyActor"
local g = "/Script/Engine.Default__GameplayStatics"
local h = "/Script/Engine.Default__GameplayStatics:GetPlayerController"
local i = "/Script/Engine"
local j = "/Script/Engine.Default__"
local k = "/Game/Pkg/BP_Y.BP_Y_C:Func"
local l = "/Game/"
local m = "/Game/A B/C"
local n = "/Engine/Foo/Bar"
local o = "Game/Pkg/Thing"
local p = "/Game/Pkg/../Thing"
)LUA");
    CHECK_EQ(result.at("assets").dump(), R"(["/Game/Pkg/BP_Y","/Game/Pkg/BP_Z","/Game/Pkg/Dir/Thing","/Game/Pkg/Mesh"])");
    CHECK_EQ(result.at("classes").dump(),
             R"(["/Game/Pkg/BP_Y.BP_Y_C","/Game/Pkg/BP_Z.BP_Z_C","/Script/Engine.Actor","/Script/Engine.GameplayStatics"])");
    CHECK_EQ(result.at("functions").dump(),
             R"(["/Script/Engine.Actor:K2_DestroyActor","/Script/Engine.GameplayStatics:GetPlayerController"])");
    CHECK_EQ(result.at("short_class_names").dump(), "[]");
    CHECK(!result.at("dynamic").get<bool>());
}

TEST(extract_string_forms_and_escapes) {
    const auto result = extract_lua_references(std::string(R"LUA(
local a = '/Game/Single/Quoted'
local b = "/Game/Double/Quoted"
local c = "/Game/Esc\x41pe/\65\066/\u{43}\z
          tail"
local d = "/Game/Quote\"s"
local e = '/Game/It\'s'
local f = [[/Game/Long/One]]
local g = [==[/Game/Long/Two]] still inside]==]
local h = [=[
/Game/Long/Three]=]
local i = "/Game/Line\
Break"
local j = FindFirstOf [[Short_Long]]
local k = FindAllOf'Single_Quote'
local l = [[
]] .. "/Game/Frag/Ment"
)LUA"));
    CHECK_EQ(result.at("assets").dump(),
             R"(["/Game/Double/Quoted","/Game/EscApe/AB/Ctail","/Game/Long/One","/Game/Long/Three","/Game/Single/Quoted"])");
    CHECK_EQ(result.at("short_class_names").dump(), R"(["Short_Long","Single_Quote"])");
    // The "]]" inside the level-2 bracket and the escaped-quote/newline literals
    // (not valid paths) do not leak references; "/Game/Frag/Ment" is a ".." operand.
    CHECK(!result.at("dynamic").get<bool>());
}

TEST(extract_comments_are_skipped) {
    const auto result = extract_lua_references(R"LUA(
-- FindFirstOf("CommentedClass") "/Game/Line/Comment"
--[[ "/Game/Block/Comment"
FindFirstOf(x) ]]
--[==[ "/Game/Level2/Comment" ]] still comment ]==]
--[ not a block "/Game/Not/ABlock"
---[[ line comment "/Game/Triple/Dash"
local real = "/Game/Real/Ref" -- trailing "/Game/Trailing/Ref"
local text = "-- not a comment /Game/In/String"
--[[ unterminated-looking but closed ]] local after = FindFirstOf("AfterComment")
)LUA");
    CHECK_EQ(result.at("assets").dump(), R"(["/Game/Real/Ref"])");
    CHECK_EQ(result.at("short_class_names").dump(), R"(["AfterComment"])");
    CHECK(!result.at("dynamic").get<bool>());
}

TEST(extract_dynamic_detection) {
    auto dynamic_of = [](const char* source) { return extract_lua_references(source).at("dynamic").get<bool>(); };
    CHECK(!dynamic_of(R"(local x = FindFirstOf("Foo"))"));
    CHECK(!dynamic_of(R"(local x = FindAllOf("Foo") local y = StaticFindObject("/Script/Engine.Actor"))"));
    CHECK(!dynamic_of("local x = 1 + 2"));
    CHECK(dynamic_of("local x = FindFirstOf(name)"));
    CHECK(dynamic_of("local x = FindAllOf(name .. \"Suffix\")"));
    CHECK(dynamic_of("local x = FindFirstOf(\"Prefix\" .. name)"));
    CHECK(dynamic_of("local x = StaticFindObject(path)"));
    CHECK(dynamic_of("local x = StaticFindObject(\"/Game/\" .. name)"));
    CHECK(dynamic_of("local x = StaticFindObject(nil, nil, \"/Script/Engine.Actor\")"));
    CHECK(dynamic_of("local x = FindFirstOf(\"Foo\", 2)"));
    CHECK(dynamic_of("local x = FindFirstOf()"));
    CHECK(dynamic_of("local x = FindFirstOf{\"Foo\"}"));
    CHECK(dynamic_of("pcall(FindFirstOf, \"Foo\")"));
    CHECK(dynamic_of("local x = FindFirstOf(\"not an identifier\")"));
    CHECK(dynamic_of("local x = StaticFindObject(\"/Game/Bad Path/X\")"));
    // Concatenated fragments are not references.
    const auto fragments = extract_lua_references("local x = \"/Game/Prefix/\" .. a .. \"/Game/Tail/X\" .. b");
    CHECK_EQ(fragments.at("assets").dump(), "[]");
    // Literal inside a dynamic call still counts when it stands alone elsewhere.
    const auto mixed = extract_lua_references("local k = \"/Game/Known/One\"\nlocal x = FindFirstOf(name)");
    CHECK_EQ(mixed.at("assets").dump(), R"(["/Game/Known/One"])");
    CHECK(mixed.at("dynamic").get<bool>());
}

TEST(extract_short_class_names_forms) {
    const auto result = extract_lua_references(R"LUA(
FindFirstOf("PlayerController")
FindAllOf("Actor")
StaticFindObject("BP_Thing_C")
FindFirstOf("PlayerController")
local nested = FindFirstOf("Outer"):GetFName():ToString()
)LUA");
    CHECK_EQ(result.at("short_class_names").dump(), R"(["Actor","BP_Thing_C","Outer","PlayerController"])");
    CHECK_EQ(result.at("classes").dump(), "[]");
    CHECK(!result.at("dynamic").get<bool>());
}

TEST(extract_empty_and_limits) {
    const auto empty = extract_lua_references("");
    CHECK_EQ(empty.dump(), R"({"assets":[],"classes":[],"dynamic":false,"functions":[],"short_class_names":[]})");
    const std::size_t limit = 4u * 1024u * 1024u;
    std::string at_limit(limit, ' ');
    CHECK_EQ(extract_lua_references(at_limit).at("assets").size(), 0u);
    CHECK_COMPAT_THROWS(extract_lua_references(std::string(limit + 1, ' ')));
    // A reference exactly at the end of an at-limit source is still seen.
    std::string tail(limit, ' ');
    const std::string literal = "\"/Game/Edge/Case\"";
    tail.replace(limit - literal.size(), literal.size(), literal);
    CHECK_EQ(extract_lua_references(tail).at("assets").dump(), R"(["/Game/Edge/Case"])");
}

TEST(extract_rejects_malformed_lua) {
    CHECK_COMPAT_THROWS(extract_lua_references("local x = \"/Game/Open"));
    CHECK_COMPAT_THROWS(extract_lua_references("local x = '/Game/Open\nlocal y = 1"));
    CHECK_COMPAT_THROWS(extract_lua_references("local x = [[/Game/Open"));
    CHECK_COMPAT_THROWS(extract_lua_references("local x = [==[/Game/Open]=]"));
    CHECK_COMPAT_THROWS(extract_lua_references("--[[ never closed"));
    CHECK_COMPAT_THROWS(extract_lua_references("local x = \"bad \\q escape\""));
    CHECK_COMPAT_THROWS(extract_lua_references("local x = \"\\xZZ\""));
    CHECK_COMPAT_THROWS(extract_lua_references("local x = \"\\300\""));
    CHECK_COMPAT_THROWS(extract_lua_references("local x = \"\\u{110000000}\""));
    CHECK_COMPAT_THROWS(extract_lua_references("local x = \"\\u41\""));
    CHECK_COMPAT_THROWS(extract_lua_references("local x = \"trailing backslash\\"));
    // A lone "[" or "[=" is just punctuation, not a long bracket.
    CHECK_EQ(extract_lua_references("local t = {} t[1] = 2 local u = t[ \"/Game/A/B\" ]").at("assets").dump(),
             R"(["/Game/A/B"])");
}

// -------------------------------------------------------- reference checking
TEST(check_references_no_index) {
    const auto result = check_references(refs(list({"/Game/A/X"}), Json::array(), Json::array(), list({"Actor"})),
                                         profile_without_index());
    CHECK_EQ(S(result.at("status")), "no_index");
    CHECK_EQ(result.at("checked").get<std::uint64_t>(), 0u);
    CHECK_EQ(result.at("present").get<std::uint64_t>(), 0u);
    CHECK_EQ(result.at("missing").dump(), R"({"assets":[],"classes":[],"functions":[],"short_class_names":[]})");
    CHECK_EQ(result.at("unverified").dump(), R"({"assets":[],"classes":[],"functions":[],"short_class_names":[]})");
}

TEST(check_references_present_missing_unverified) {
    auto all = check_references(present_refs(), profile());
    CHECK_EQ(S(all.at("status")), "all_present");
    CHECK_EQ(all.at("checked").get<std::uint64_t>(), 4u);
    CHECK_EQ(all.at("present").get<std::uint64_t>(), 4u);

    // assets is complete -> missing; classes/functions are not -> unverified.
    auto mixed = check_references(
        refs(list({"/Game/A/X", "/Game/Nope/Gone"}), list({"/Script/Engine.Actor", "/Script/Engine.Nope"}),
             list({"/Script/Engine.Actor:Nope"}), list({"Actor", "Nope"})),
        profile());
    CHECK_EQ(S(mixed.at("status")), "missing");
    CHECK_EQ(mixed.at("checked").get<std::uint64_t>(), 7u);
    CHECK_EQ(mixed.at("present").get<std::uint64_t>(), 3u);
    CHECK_EQ(mixed.at("missing").dump(), R"({"assets":["/Game/Nope/Gone"],"classes":[],"functions":[],"short_class_names":[]})");
    CHECK_EQ(mixed.at("unverified").dump(),
             R"({"assets":[],"classes":["/Script/Engine.Nope"],"functions":["/Script/Engine.Actor:Nope"],"short_class_names":["Nope"]})");

    auto only_unverified = check_references(refs(list({"/Game/A/X"}), list({"/Script/Engine.Nope"})), profile());
    CHECK_EQ(S(only_unverified.at("status")), "unverified");
    CHECK_EQ(only_unverified.at("present").get<std::uint64_t>(), 1u);

    // Marking classes and functions complete turns unverified into missing.
    auto strict = profile();
    strict["content_index"]["complete"]["classes"] = true;
    strict["content_index"]["complete"]["functions"] = true;
    auto proven = check_references(refs(Json::array(), list({"/Script/Engine.Nope"}), list({"/Script/Engine.Actor:Nope"}),
                                        list({"Nope"})),
                                   strict);
    CHECK_EQ(S(proven.at("status")), "missing");
    CHECK_EQ(proven.at("missing").at("classes").dump(), R"(["/Script/Engine.Nope"])");
    CHECK_EQ(proven.at("missing").at("functions").dump(), R"(["/Script/Engine.Actor:Nope"])");
    CHECK_EQ(proven.at("missing").at("short_class_names").dump(), R"(["Nope"])");
    CHECK_EQ(proven.at("unverified").at("classes").size(), 0u);

    // Incomplete assets cannot prove absence either.
    auto loose = profile();
    loose["content_index"]["complete"]["assets"] = false;
    auto unproven = check_references(refs(list({"/Game/Nope/Gone"})), loose);
    CHECK_EQ(S(unproven.at("status")), "unverified");
    CHECK_EQ(unproven.at("unverified").at("assets").dump(), R"(["/Game/Nope/Gone"])");
}

TEST(check_references_short_name_matching) {
    // Index has /Script/Engine.Actor and /Game/X/BP_Y.BP_Y_C.
    auto strict = profile();
    strict["content_index"]["complete"]["classes"] = true;
    auto result = check_references(refs(Json::array(), Json::array(), Json::array(),
                                        list({"Actor", "Actor_C", "BP_Y", "BP_Y_C", "Y", "BP_Y_C_C", "ctor"})),
                                   strict);
    CHECK_EQ(result.at("present").get<std::uint64_t>(), 4u);
    CHECK_EQ(result.at("missing").at("short_class_names").dump(), R"(["BP_Y_C_C","Y","ctor"])");
    // The match is on the last "."-segment, never on the package.
    auto package = check_references(refs(Json::array(), Json::array(), Json::array(), list({"Engine", "Script"})), strict);
    CHECK_EQ(package.at("missing").at("short_class_names").size(), 2u);
}

TEST(check_references_dedupe_and_counts) {
    auto result = check_references(refs(list({"/Game/A/X", "/Game/A/X", "/Game/Gone/Z", "/Game/Gone/Z"})), profile());
    CHECK_EQ(result.at("checked").get<std::uint64_t>(), 2u);
    CHECK_EQ(result.at("present").get<std::uint64_t>(), 1u);
    CHECK_EQ(result.at("missing").at("assets").size(), 1u);
    auto empty = check_references(refs(), profile());
    CHECK_EQ(S(empty.at("status")), "all_present");
    CHECK_EQ(empty.at("checked").get<std::uint64_t>(), 0u);
}

TEST(check_references_validates_inputs) {
    CHECK_COMPAT_THROWS(check_references(Json::array(), profile()));
    auto missing_key = refs();
    missing_key.erase("functions");
    CHECK_COMPAT_THROWS(check_references(missing_key, profile()));
    CHECK_COMPAT_THROWS(check_references(refs(list({"/Game/X/Y.Z"})), profile()));
    CHECK_COMPAT_THROWS(check_references(refs(Json::array(), list({"/Game/Pkg"})), profile()));
    CHECK_COMPAT_THROWS(check_references(refs(Json::array(), Json::array(), list({"/Script/A.B"})), profile()));
    CHECK_COMPAT_THROWS(check_references(refs(Json::array(), Json::array(), Json::array(), list({"/Script/A.B"})), profile()));
    CHECK_COMPAT_THROWS(check_references(refs(Json::array({1})), profile()));
    auto bad_dynamic = refs();
    bad_dynamic["dynamic"] = "yes";
    CHECK_COMPAT_THROWS(check_references(bad_dynamic, profile()));
    auto bad_profile = profile();
    bad_profile.erase("builds");
    CHECK_COMPAT_THROWS(check_references(refs(), bad_profile));
    // dynamic is optional on input.
    auto no_dynamic = refs();
    no_dynamic.erase("dynamic");
    CHECK_EQ(S(check_references(no_dynamic, profile()).at("status")), "all_present");
    // Extraction output feeds straight into the checker.
    const auto extracted = extract_lua_references(R"(local a = "/Game/A/X" local b = FindFirstOf("Actor"))");
    CHECK_EQ(S(check_references(extracted, profile()).at("status")), "all_present");
}

// -------------------------------------------------------------- assessment
TEST(assess_compatible) {
    const auto result = assess_mod_compatibility(profile(), param("02.000.000"), files({file(1, 2500)}), present_refs());
    CHECK_EQ(S(result.at("verdict")), "compatible");
    CHECK_EQ(result.at("reasons").size(), 0u);
    CHECK(result.at("game_version").at("known").get<bool>());
    CHECK(result.at("game_version").at("build_listed").get<bool>());
    CHECK_EQ(S(result.at("game_version").at("content_version")), "02.000.000");
    CHECK_EQ(S(result.at("file_selection").at("status")), "selected");
    CHECK_EQ(S(result.at("content").at("status")), "all_present");
    // Deterministic.
    CHECK_EQ(result.dump(), assess_mod_compatibility(profile(), param("02.000.000"), files({file(1, 2500)}), present_refs()).dump());
    // Explicit id selecting a fitting file is also compatible.
    CHECK_EQ(S(assess_mod_compatibility(profile(), param("02.000.000"), files({file(1, 2500), file(2, 2600)}),
                                        present_refs(), 1).at("verdict")),
             "compatible");
}

TEST(assess_unknown_console_version) {
    for (const auto& input : {Json::object(), param("bogus")}) {
        const auto result = assess_mod_compatibility(profile(), input, files({file(1, 2500)}), present_refs());
        CHECK_EQ(S(result.at("verdict")), "unknown");
        CHECK_EQ(codes(result), "unknown_console_version");
        CHECK(!result.at("game_version").at("known").get<bool>());
        CHECK_EQ(S(result.at("content").at("status")), "not_evaluated");
        CHECK(result.at("file_selection").at("selected_file_id").is_null());
    }
    CHECK_EQ(S(assess_mod_compatibility(profile(), Json::object(), files({}), present_refs()).at("reasons")[0].at("detail")),
             "missing_content_version");
    CHECK_EQ(S(assess_mod_compatibility(profile(), param("x"), files({}), present_refs()).at("reasons")[0].at("detail")),
             "invalid_content_version");
}

TEST(assess_unknown_build) {
    // Unlisted version: the (non-matching) index also gets reported.
    auto unlisted = assess_mod_compatibility(profile(), param("09.000.000"), files({file(1, 2500)}), present_refs());
    CHECK_EQ(S(unlisted.at("verdict")), "unknown");
    CHECK_EQ(codes(unlisted), "unknown_build,index_for_other_build");
    CHECK(!unlisted.at("game_version").at("build_listed").get<bool>());
    // Listed but PC build unknown.
    auto null_pc = assess_mod_compatibility(profile(), param("03.000.000"), files({file(1, 2500)}), present_refs());
    CHECK_EQ(S(null_pc.at("verdict")), "unknown");
    CHECK_EQ(codes(null_pc), "unknown_build,index_for_other_build");
    CHECK(null_pc.at("game_version").at("build_listed").get<bool>());
    CHECK(null_pc.at("game_version").at("pc_build").is_null());
    CHECK_EQ(S(null_pc.at("file_selection").at("status")), "unknown_build");
    // With an index for the installed build, the content check decides; the unknown
    // PC patch only means the file cannot be dated and stays a visible caveat.
    auto indexed = profile();
    indexed["content_index"]["console_content_version"] = "03.000.000";
    auto result = assess_mod_compatibility(indexed, param("03.000.000"), files({file(1, 2500)}), present_refs());
    CHECK_EQ(S(result.at("verdict")), "compatible");
    CHECK_EQ(codes(result), "unknown_build");
    // ...and it never hides a proven miss.
    auto indexed_missing = assess_mod_compatibility(indexed, param("03.000.000"), files({file(1, 2500)}),
                                                    refs(list({"/Game/A/X", "/Game/Gone/Z"})));
    CHECK_EQ(S(indexed_missing.at("verdict")), "partially_compatible");
    CHECK_EQ(codes(indexed_missing), "unknown_build,missing_references");
    CHECK_EQ(S(result.at("content").at("status")), "all_present");
}

TEST(assess_file_reason_codes) {
    auto run = [](const Json& file_list, std::uint64_t explicit_id = 0) {
        return assess_mod_compatibility(profile(), param("02.000.000"), file_list, present_refs(), explicit_id);
    };
    auto tie = run(files({file(1, 2900), file(2, 2900)}));
    CHECK_EQ(S(tie.at("verdict")), "partially_compatible");
    CHECK_EQ(codes(tie), "file_ambiguous");
    auto no_fit = run(files({file(1, 100)}));
    CHECK_EQ(S(no_fit.at("verdict")), "partially_compatible");
    CHECK_EQ(codes(no_fit), "file_no_fit");
    CHECK_EQ(codes(run(files({}))), "file_no_fit");
    auto older = run(files({file(1, 100), file(2, 2500)}), 1);
    CHECK_EQ(S(older.at("verdict")), "partially_compatible");
    CHECK_EQ(codes(older), "file_older_build");
    auto newer = run(files({file(1, 9000)}), 1);
    CHECK_EQ(S(newer.at("verdict")), "partially_compatible"); // nothing missing
    CHECK_EQ(codes(newer), "file_newer_than_build");
    auto undated = run(files({file(1)}), 1);
    CHECK_EQ(S(undated.at("verdict")), "compatible"); // cannot be dated: a caveat, not a mismatch
    CHECK_EQ(codes(undated), "file_undated");
    auto not_found = run(files({file(1, 2500)}), 77);
    CHECK_EQ(S(not_found.at("verdict")), "partially_compatible");
    CHECK_EQ(codes(not_found), "file_no_fit");
    CHECK_EQ(S(not_found.at("reasons")[0].at("detail")), "explicit_file_not_found");
    auto retired = file(1, 2500);
    retired["category_name"] = "DELETED";
    auto retired_result = run(files({retired}), 1);
    CHECK_EQ(S(retired_result.at("verdict")), "partially_compatible");
    CHECK_EQ(codes(retired_result), "file_no_fit");
}

TEST(assess_missing_incompatible_and_partial) {
    const auto good = files({file(1, 2500)});
    // Some missing -> partially_compatible.
    auto partial = assess_mod_compatibility(profile(), param("02.000.000"), good,
                                            refs(list({"/Game/A/X", "/Game/Gone/Z"})));
    CHECK_EQ(S(partial.at("verdict")), "partially_compatible");
    CHECK_EQ(codes(partial), "missing_references");
    CHECK_EQ(S(partial.at("reasons")[0].at("detail")), "1 of 2 references missing");
    // Every checked reference missing -> incompatible.
    auto all_missing = assess_mod_compatibility(profile(), param("02.000.000"), good,
                                                refs(list({"/Game/Gone/Z", "/Game/Gone/Q"})));
    CHECK_EQ(S(all_missing.at("verdict")), "incompatible");
    CHECK_EQ(codes(all_missing), "missing_references");
    // Unverified references are not "missing": a lone missing one plus an unverified one is partial.
    auto not_all = assess_mod_compatibility(profile(), param("02.000.000"), good,
                                            refs(list({"/Game/Gone/Z"}), list({"/Script/Engine.Nope"})));
    CHECK_EQ(S(not_all.at("verdict")), "partially_compatible");
    CHECK_EQ(codes(not_all), "missing_references,unverified_references");
    // Newer-than-build file with at least one missing reference -> incompatible.
    auto newer = assess_mod_compatibility(profile(), param("02.000.000"), files({file(1, 9000)}),
                                          refs(list({"/Game/A/X", "/Game/Gone/Z"})), 1);
    CHECK_EQ(S(newer.at("verdict")), "incompatible");
    CHECK_EQ(codes(newer), "file_newer_than_build,missing_references");
    // Older file with a missing reference stays partial.
    auto older = assess_mod_compatibility(profile(), param("02.000.000"), files({file(1, 100)}),
                                          refs(list({"/Game/A/X", "/Game/Gone/Z"})), 1);
    CHECK_EQ(S(older.at("verdict")), "partially_compatible");
    CHECK_EQ(codes(older), "file_older_build,missing_references");
    // Missing references with no file fitting: still partial, both reasons.
    auto no_file = assess_mod_compatibility(profile(), param("02.000.000"), files({}),
                                            refs(list({"/Game/A/X", "/Game/Gone/Z"})));
    CHECK_EQ(S(no_file.at("verdict")), "partially_compatible");
    CHECK_EQ(codes(no_file), "file_no_fit,missing_references");
}

TEST(assess_unverified_and_dynamic_evidence) {
    const auto good = files({file(1, 2500)});
    auto unverified = assess_mod_compatibility(profile(), param("02.000.000"), good,
                                               refs(list({"/Game/A/X"}), list({"/Script/Engine.Nope"})));
    CHECK_EQ(S(unverified.at("verdict")), "unknown");
    CHECK_EQ(codes(unverified), "unverified_references");
    auto dynamic = assess_mod_compatibility(profile(), param("02.000.000"), good, refs(list({"/Game/A/X"}), Json::array(),
                                                                                       Json::array(), Json::array(), true));
    CHECK_EQ(S(dynamic.at("verdict")), "compatible"); // every static reference proven; dynamic is a caveat
    CHECK_EQ(codes(dynamic), "dynamic_references");
    CHECK(dynamic.at("content").at("dynamic").get<bool>());
    auto both = assess_mod_compatibility(profile(), param("02.000.000"), good,
                                         refs(Json::array(), list({"/Script/Engine.Nope"}), Json::array(), Json::array(), true));
    CHECK_EQ(S(both.at("verdict")), "unknown");
    CHECK_EQ(codes(both), "unverified_references,dynamic_references");
    // Dynamic lookups never hide a proven miss.
    auto missing = assess_mod_compatibility(profile(), param("02.000.000"), good,
                                            refs(list({"/Game/Gone/Z"}), Json::array(), Json::array(), Json::array(), true));
    CHECK_EQ(S(missing.at("verdict")), "incompatible");
    CHECK_EQ(codes(missing), "missing_references,dynamic_references");
    // Unverified evidence plus a file that does not fit is only partial.
    auto old_file = assess_mod_compatibility(profile(), param("02.000.000"), files({file(1, 100)}),
                                             refs(Json::array(), list({"/Script/Engine.Nope"})), 1);
    CHECK_EQ(S(old_file.at("verdict")), "partially_compatible");
}

TEST(assess_no_index_and_index_for_other_build) {
    auto no_index = assess_mod_compatibility(profile_without_index(), param("02.000.000"), files({file(1, 2500)}),
                                             present_refs());
    CHECK_EQ(S(no_index.at("verdict")), "unknown");
    CHECK_EQ(codes(no_index), "no_index");
    CHECK_EQ(S(no_index.at("content").at("status")), "not_evaluated");
    CHECK_EQ(S(no_index.at("file_selection").at("status")), "selected");
    auto other = assess_mod_compatibility(profile(), param("01.000.000"), files({file(1, 1500)}), present_refs());
    CHECK_EQ(S(other.at("verdict")), "unknown");
    CHECK_EQ(codes(other), "index_for_other_build");
    CHECK_EQ(S(other.at("reasons")[0].at("detail")), "content index is for 02.000.000, installed is 01.000.000");
    CHECK_EQ(S(other.at("content").at("status")), "not_evaluated");
    // A missing reference cannot be proven without the matching index, even with a newer file.
    auto newer = assess_mod_compatibility(profile(), param("01.000.000"), files({file(1, 9000)}),
                                          refs(list({"/Game/Gone/Z"})), 1);
    CHECK_EQ(S(newer.at("verdict")), "unknown");
    CHECK_EQ(codes(newer), "file_newer_than_build,index_for_other_build");
}

TEST(assess_reason_order_and_validation) {
    // Reason order follows the documented code list.
    auto result = assess_mod_compatibility(profile_without_index(), param("02.000.000"), files({file(1, 100)}),
                                           refs(Json::array(), Json::array(), Json::array(), Json::array(), true), 1);
    CHECK_EQ(codes(result), "file_older_build,dynamic_references,no_index");
    for (const auto& reason : result.at("reasons")) {
        CHECK(reason.contains("code"));
        CHECK(reason.at("detail").is_string());
    }
    CHECK_COMPAT_THROWS(assess_mod_compatibility(Json::object(), param("02.000.000"), files({}), refs()));
    CHECK_COMPAT_THROWS(assess_mod_compatibility(profile(), param("02.000.000"), Json::object(), refs()));
    CHECK_COMPAT_THROWS(assess_mod_compatibility(profile(), param("02.000.000"), files({}), Json::object()));
    // Inputs are validated even when the console version is unknown.
    CHECK_COMPAT_THROWS(assess_mod_compatibility(profile(), Json::object(), Json::object(), refs()));
    CHECK_COMPAT_THROWS(assess_mod_compatibility(profile(), Json::object(), files({}), Json::array()));
}

// ------------------------------------------------------- attach content index
TEST(attach_content_index_connects_a_built_index) {
    const auto base = profile_without_index();
    CHECK(!base.contains("content_index"));
    Json index = {{"console_content_version", "02.000.000"},
                  {"complete", {{"assets", true}, {"classes", true}, {"functions", true}}},
                  {"assets", list({"/Game/A/X"})}, {"classes", Json::array()}, {"functions", Json::array()}};
    const auto attached = attach_content_index(base, index);
    CHECK_EQ(S(attached.at("content_index").at("console_content_version")), "02.000.000");
    // The attached profile now yields a real verdict instead of no_index/unknown.
    CHECK_EQ(S(assess_mod_compatibility(base, param("02.000.000"), files({file(1, 2500)}),
                                        refs(list({"/Game/A/X"}))).at("verdict")), "unknown");
    CHECK_EQ(S(assess_mod_compatibility(attached, param("02.000.000"), files({file(1, 2500)}),
                                        refs(list({"/Game/A/X"}))).at("verdict")), "compatible");
    // An index for a build the profile does not list, or a malformed one, is refused.
    auto other = index;
    other["console_content_version"] = "09.000.000";
    CHECK_COMPAT_THROWS(attach_content_index(base, other));
    auto malformed = index;
    malformed.erase("complete");
    CHECK_COMPAT_THROWS(attach_content_index(base, malformed));
    CHECK_COMPAT_THROWS(attach_content_index(Json::array(), index));
}
