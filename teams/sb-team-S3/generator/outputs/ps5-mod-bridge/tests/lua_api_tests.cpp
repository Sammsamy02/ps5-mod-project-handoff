#include "test.hpp"
#include "modbridge/compat.hpp"
#include "modbridge/lua_api.hpp"
#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
using namespace modbridge;
namespace {

// Every invalid input must be an std::invalid_argument with the "lua_api: " prefix.
#define CHECK_LUA_API_THROWS(expression) do { bool caught_ = false; \
    try { (void)(expression); } \
    catch (const std::invalid_argument& error_) { \
        caught_ = std::string(error_.what()).rfind("lua_api: ", 0) == 0; } \
    CHECK(caught_); } while (false)

Json usage_of(const std::string& source) { return extract_lua_api_usage(source); }
Json functions_of(const std::string& source) { return usage_of(source).at("functions"); }
std::string dump(const Json& value) { return value.dump(); }

Json entry(const char* status, const char* note = nullptr) {
    Json result = {{"status", status}};
    if (note != nullptr) result["note"] = note;
    return result;
}
// A valid manifest; callers override pieces.
Json runtime(Json functions = Json::object(), Json modules = Json::object(), bool console_wired = false) {
    if (functions.is_null()) functions = Json::object(); // `{}` converts to null, not to an empty object
    if (modules.is_null()) modules = Json::object();
    return {{"schema", "modbridge.lua-runtime/1"}, {"runtime", "ps5-test"}, {"version", "1"},
            {"console_wired", console_wired}, {"functions", std::move(functions)}, {"modules", std::move(modules)}};
}
Json usage(Json functions = Json::object(), Json modules = Json::array(), Json shadowed = Json::array(),
           bool dynamic_require = false) {
    if (functions.is_null()) functions = Json::object(); // `{}` converts to null, not to an empty object
    return {{"functions", std::move(functions)}, {"modules", std::move(modules)},
            {"shadowed", std::move(shadowed)}, {"dynamic_require", dynamic_require}};
}

} // namespace

// ---------------------------------------------------------------------------
// Catalog
// ---------------------------------------------------------------------------
TEST(lua_api_catalog_is_sorted_unique_and_complete) {
    const auto catalog = lua_api_catalog();
    CHECK_EQ(catalog.size(), 97u);
    CHECK(std::is_sorted(catalog.begin(), catalog.end()));
    CHECK_EQ(std::set<std::string_view>(catalog.begin(), catalog.end()).size(), catalog.size());
    for (const char* name : {
             "StaticFindObject", "FindFirstOf", "FindAllOf", "FindObject", "FindObjects", "ForEachUObject",
             "StaticConstructObject", "LoadAsset", "CreateInvalidObject", "RegisterHook", "UnregisterHook",
             "NotifyOnNewObject", "ExecuteInGameThread", "ExecuteAsync", "ExecuteWithDelay", "LoopAsync",
             "RegisterKeyBind", "IsKeyBindRegistered", "RegisterConsoleCommandHandler",
             "RegisterConsoleCommandGlobalHandler", "RegisterCustomEvent", "UnregisterCustomEvent",
             "RegisterLoadMapPreHook", "RegisterLoadMapPostHook", "RegisterInitGameStatePreHook",
             "RegisterInitGameStatePostHook", "RegisterBeginPlayPreHook", "RegisterBeginPlayPostHook",
             "RegisterProcessConsoleExecPreHook", "RegisterProcessConsoleExecPostHook",
             "RegisterCallFunctionByNameWithArgumentsPreHook", "RegisterCallFunctionByNameWithArgumentsPostHook",
             "RegisterULocalPlayerExecPreHook", "RegisterULocalPlayerExecPostHook", "IterateGameDirectories",
             "DumpAllObjects", "GenerateSDK", "GenerateLuaTypes", "GenerateUHTCompatibleHeaders", "FName", "FText"})
        CHECK(std::binary_search(catalog.begin(), catalog.end(), std::string_view(name)));
}

TEST(lua_api_catalog_covers_upstream_globals_beyond_the_docs_page) {
    // Upstream LuaMod.cpp installs more than docs.ue4ss.com lists: the delayed
    // game-thread actions, async key binds, EndPlay hooks, thread queries and
    // the global tables and values mods index (Key.F7, ModifierKey.CONTROL, ...).
    const auto r = functions_of(R"LUA(
ExecuteInGameThreadWithDelay(100, f)
LoopInGameThreadWithDelay(50, g)
local h = RetriggerableExecuteInGameThreadWithDelay(10, f)
RegisterKeyBindAsync(Key.F1, {ModifierKey.CONTROL}, f)
RegisterEndPlayPreHook(f)
if IsInGameThread() then print(UE4SS.GetVersion()) end
local name = NAME_None
ModRef:SetSharedVariable("x", 1)
local e = EObjectFlags.RF_Transient
)LUA");
    CHECK_EQ(dump(r),
             R"({"EObjectFlags":1,"ExecuteInGameThreadWithDelay":1,"IsInGameThread":1,"Key":1,)"
             R"("LoopInGameThreadWithDelay":1,"ModRef":1,"ModifierKey":1,"NAME_None":1,"RegisterEndPlayPreHook":1,)"
             R"("RegisterKeyBindAsync":1,"RetriggerableExecuteInGameThreadWithDelay":1,"UE4SS":1})");
    // UE4SS-internal globals are not API.
    const auto catalog = lua_api_catalog();
    for (const char* name : {"AsyncThread", "HookThread", "MainThread", "__OriginalReturnValue", "ue4ss_loaded_modules", "print"})
        CHECK(!std::binary_search(catalog.begin(), catalog.end(), std::string_view(name)));
}
TEST(lua_api_catalog_excludes_lua_base_library) {
    const auto catalog = lua_api_catalog();
    for (const char* name : {"print", "pairs", "ipairs", "require", "pcall", "xpcall", "type", "tostring", "tonumber",
                             "setmetatable", "getmetatable", "select", "next", "error", "assert", "load", "dofile",
                             "rawget", "string", "table", "math", "os", "io"})
        CHECK(!std::binary_search(catalog.begin(), catalog.end(), std::string_view(name)));
    // Names are matched exactly and case-sensitively.
    CHECK(!std::binary_search(catalog.begin(), catalog.end(), std::string_view("registerhook")));
    CHECK(!std::binary_search(catalog.begin(), catalog.end(), std::string_view("RegisterHook ")));
}

// ---------------------------------------------------------------------------
// Extraction: uses
// ---------------------------------------------------------------------------
TEST(lua_api_empty_and_plain_scripts) {
    CHECK_EQ(dump(usage_of("")), R"({"dynamic_require":false,"functions":{},"modules":[],"shadowed":[]})");
    CHECK_EQ(dump(usage_of("local x = 1\nprint(x)\nfor i, v in pairs({}) do end")),
             R"({"dynamic_require":false,"functions":{},"modules":[],"shadowed":[]})");
    // Lua base library names are never API uses.
    CHECK_EQ(functions_of("print('x') pcall(error) setmetatable({}, {}) require('M')").size(), 0u);
}

TEST(lua_api_counts_calls_and_bare_references) {
    const auto f = functions_of(R"LUA(
RegisterHook("/Script/Engine.Actor:ReceiveBeginPlay", function() end)
RegisterHook("/Script/Engine.Actor:ReceiveTick", function() end)
local run = ExecuteInGameThread
run(function() end)
ExecuteInGameThread(function() end)
local pcallable = { cb = ExecuteAsync }
pcall(FindFirstOf, "Actor")
local n = FName("x") .. FName("y")
)LUA");
    CHECK_EQ(dump(f), R"({"ExecuteAsync":1,"ExecuteInGameThread":2,"FName":2,"FindFirstOf":1,"RegisterHook":2})");
}

TEST(lua_api_comments_strings_and_long_brackets_never_hit) {
    const auto f = functions_of(R"LUA(
-- RegisterHook("x", f)
--[[ ExecuteAsync(f)
     FindFirstOf("Actor") ]]
--[==[ LoopAsync(1, f) ]] still comment ]==]
---[[ line comment, not a block: ExecuteWithDelay(1, f)
local a = "RegisterHook FindFirstOf"
local b = 'ExecuteInGameThread'
local c = "quote \" FName(\"x\") inside"
local d = 'it\'s FText'
local e = [[ StaticFindObject ]]
local g = [==[ ]] FindAllOf ]=] ]==]
local h = "line one \
continues RegisterKeyBind"
local i = "\u{52}egisterHook"  -- the Lua string is data, never an identifier
FindObject("real use")   -- ExecuteAsync in a trailing comment
)LUA");
    CHECK_EQ(dump(f), R"({"FindObject":1})");
}

TEST(lua_api_field_and_method_access_is_not_a_global_use) {
    const auto f = functions_of(R"LUA(
Obj.RegisterHook(1)
Obj:FindFirstOf("x")
a.b.StaticFindObject("p")
a . FName ("n")
a:
   FText("t")
Helpers.ExecuteAsync(f)
local x = tbl.ExecuteInGameThread
local y = tbl["LoopAsync"]
)LUA");
    CHECK_EQ(dump(f), "{}");
    // Concatenation and other operators are not field access.
    CHECK_EQ(dump(functions_of("local s = 'a'..FName('x') local t = 1 + LoadAsset(1) local u = {FText}")),
             R"({"FName":1,"FText":1,"LoadAsset":1})");
    // A name that merely contains a catalog name is a different identifier.
    CHECK_EQ(dump(functions_of("MyRegisterHook() RegisterHook2() _FName() FName_() xFText()")), "{}");
}

TEST(lua_api_function_and_table_key_forms) {
    // `function Name.sub()` indexes the global; method/field definitions are field access.
    CHECK_EQ(dump(functions_of("function FindObject.sub() end")), R"({"FindObject":1})");
    const auto field = usage_of("function obj.RegisterHook() end\nfunction obj:FName() end\nfunction a.b.LoadAsset() end");
    CHECK_EQ(dump(field.at("functions")), "{}");
    CHECK_EQ(field.at("shadowed").size(), 0u);
    // A table key is a field name: neither a use nor a shadow.
    const auto keys = usage_of(R"LUA(
local t = { FName = 1, ExecuteAsync = 2; LoopAsync = 3 }
f({ a = 1, FText = 3 })
local u = { nested = { RegisterHook = 1 } }
local v = { [1] = FindFirstOf }
)LUA");
    CHECK_EQ(dump(keys.at("functions")), R"({"FindFirstOf":1})");
    CHECK_EQ(keys.at("shadowed").size(), 0u);
    // Positional table values are uses.
    CHECK_EQ(dump(functions_of("local t = { FName, FText }")), R"({"FName":1,"FText":1})");
    // `==` is a comparison, not an assignment.
    const auto compare = usage_of("if FName == nil then end if ExecuteAsync ~= nil then end");
    CHECK_EQ(dump(compare.at("functions")), R"({"ExecuteAsync":1,"FName":1})");
    CHECK_EQ(compare.at("shadowed").size(), 0u);
}

// ---------------------------------------------------------------------------
// Extraction: shadowing
// ---------------------------------------------------------------------------
TEST(lua_api_shadowed_declarations_are_not_api_use) {
    const auto r = usage_of(R"LUA(
function RegisterHook(name, cb) end
RegisterHook("x", function() end)
local function FindAllOf() return {} end
local RegisterKeyBind = function() end
local ok, LoadAsset = pcall(f)
local a <const>, FText <close> = 1, 2
FindAllOf()
RegisterKeyBind(1, f)
ExecuteInGameThread(f)
)LUA");
    CHECK_EQ(dump(r.at("shadowed")), R"(["FText","FindAllOf","LoadAsset","RegisterHook","RegisterKeyBind"])");
    CHECK_EQ(dump(r.at("functions")), R"({"ExecuteInGameThread":1})");
}

TEST(lua_api_assignment_declares_a_global_unless_it_is_a_table_key) {
    const auto polyfill = usage_of("ExecuteInGameThread = ExecuteInGameThread or function(f) f() end\nExecuteInGameThread(g)");
    CHECK_EQ(dump(polyfill.at("shadowed")), R"(["ExecuteInGameThread"])");
    CHECK_EQ(dump(polyfill.at("functions")), "{}");
    CHECK_EQ(dump(usage_of("a, FName = 1, 2").at("shadowed")), R"(["FName"])");
    CHECK_EQ(dump(usage_of("for LoopAsync = 1, 3 do end").at("shadowed")), R"(["LoopAsync"])");
    // Inside a table constructor `, Name =` is a key even after a positional value.
    const auto table = usage_of("local t = { a, FName = 2, ExecuteAsync = 3 }");
    CHECK_EQ(table.at("shadowed").size(), 0u);
    CHECK_EQ(dump(table.at("functions")), "{}");
}

TEST(lua_api_shadowing_applies_to_the_whole_script_and_to_one_name_only) {
    const auto r = usage_of(R"LUA(
FindFirstOf("Before")
local FindFirstOf = function() end
FindFirstOf("After")
FindAllOf("Other")
)LUA");
    CHECK_EQ(dump(r.at("shadowed")), R"(["FindFirstOf"])");
    CHECK_EQ(dump(r.at("functions")), R"({"FindAllOf":1})");
}

TEST(lua_api_non_catalog_declarations_are_ignored) {
    const auto r = usage_of("function helper() end local function other() end local RegisterHooks = 1 local x = 1");
    CHECK_EQ(dump(r), R"({"dynamic_require":false,"functions":{},"modules":[],"shadowed":[]})");
}

// ---------------------------------------------------------------------------
// Extraction: modules
// ---------------------------------------------------------------------------
TEST(lua_api_require_literal_forms) {
    const auto r = usage_of(R"LUA(
local a = require("UEHelpers")
local b = require "Json"
local c = require 'Lib.sub/mod-x_1'
local d = require([[LongForm]])
local e = require("UEHelpers").GetPlayer()
local ok, f = pcall(require, "Safe")
local ok2, g = pcall(require, "Safe2", extra)
local x = require("Json") -- duplicates collapse
)LUA");
    CHECK_EQ(dump(r.at("modules")), R"(["Json","Lib.sub/mod-x_1","LongForm","Safe","Safe2","UEHelpers"])");
    CHECK(!r.at("dynamic_require").get<bool>());
    CHECK_EQ(r.at("functions").size(), 0u);
}

TEST(lua_api_require_ignored_in_comments_strings_and_field_access) {
    const auto r = usage_of(R"LUA(
-- require("Commented")
local s = "require('InString')"
local l = [[ require("InLong") ]]
package.require("Field")
loader:require("Method")
)LUA");
    CHECK_EQ(r.at("modules").size(), 0u);
    CHECK(!r.at("dynamic_require").get<bool>());
}

TEST(lua_api_dynamic_require) {
    for (const char* source : {"require(name)", "require('a' .. b)", "require(1)", "require()", "require({})",
                               "require{}", "local r = require", "pcall(require, name)", "pcall(require)",
                               "xpcall(require, handler, 'X')", "require('X', 'extra') == nil and require(y)"}) {
        const auto r = usage_of(source);
        CHECK(r.at("dynamic_require").get<bool>());
    }
    // A literal that is a module name is still listed next to a dynamic one.
    const auto mixed = usage_of("require('Known') require(unknown)");
    CHECK_EQ(dump(mixed.at("modules")), R"(["Known"])");
    CHECK(mixed.at("dynamic_require").get<bool>());
    // Declaring or defining require is not a dynamic require.
    for (const char* source : {"local require = mine", "function require(n) end", "require = mine"})
        CHECK(!usage_of(source).at("dynamic_require").get<bool>());
}

TEST(lua_api_require_literal_that_is_not_a_module_name_is_dynamic) {
    for (const std::string& source : {std::string("require('has space')"), std::string("require('')"),
                                      std::string("require('a:b')"), std::string("require('Caf\\u{e9}')"),
                                      "require('" + std::string(129, 'a') + "')"}) {
        const auto r = usage_of(source);
        CHECK_EQ(r.at("modules").size(), 0u);
        CHECK(r.at("dynamic_require").get<bool>());
    }
    const auto longest = usage_of("require('" + std::string(128, 'a') + "')");
    CHECK_EQ(longest.at("modules").size(), 1u);
    CHECK(!longest.at("dynamic_require").get<bool>());
}

TEST(lua_api_module_list_is_bounded) {
    std::string source;
    for (int i = 0; i < 1024; ++i) source += "require('m" + std::to_string(i) + "')\n";
    auto r = usage_of(source);
    CHECK_EQ(r.at("modules").size(), 1024u);
    CHECK(!r.at("dynamic_require").get<bool>());
    source += "require('m1')\n"; // an already listed module is not an overflow
    CHECK(!usage_of(source).at("dynamic_require").get<bool>());
    source += "require('overflow')\n";
    r = usage_of(source);
    CHECK_EQ(r.at("modules").size(), 1024u);
    CHECK(r.at("dynamic_require").get<bool>());
    // The bounded output is itself a valid usage report.
    CHECK_EQ(assess_lua_runtime_coverage(r, runtime()).at("status"), "blocked");
}

// ---------------------------------------------------------------------------
// Extraction: determinism, limits, malformed input
// ---------------------------------------------------------------------------
TEST(lua_api_output_is_deterministic_and_sorted) {
    const std::string a = "ExecuteAsync(f) FindFirstOf('A') require('Z') require('A') local RegisterHook = 1 local FName = 2";
    const std::string b = "local FName = 2 local RegisterHook = 1 require('A') require('Z') FindFirstOf('A') ExecuteAsync(f)";
    CHECK_EQ(dump(usage_of(a)), dump(usage_of(b)));
    CHECK_EQ(dump(usage_of(a)),
             R"({"dynamic_require":false,"functions":{"ExecuteAsync":1,"FindFirstOf":1},"modules":["A","Z"],)"
             R"("shadowed":["FName","RegisterHook"]})");
    CHECK_EQ(dump(usage_of(a)), dump(usage_of(a)));
    // Whitespace and layout do not change the result.
    CHECK_EQ(dump(usage_of("ExecuteAsync\n\t(\n f )")), dump(usage_of("ExecuteAsync(f)")));
}

TEST(lua_api_input_limit_is_4_mib) {
    const std::size_t limit = 4u * 1024u * 1024u;
    CHECK_EQ(usage_of(std::string(limit, ' ')).at("functions").size(), 0u);
    CHECK_LUA_API_THROWS(extract_lua_api_usage(std::string(limit + 1, ' ')));
    // A use exactly at the end of an at-limit source is still seen.
    std::string tail(limit, ' ');
    const std::string call = "FindFirstOf";
    tail.replace(limit - call.size(), call.size(), call);
    CHECK_EQ(dump(functions_of(tail)), R"({"FindFirstOf":1})");
    // A dense at-limit script of uses stays bounded and counts them all.
    std::string dense;
    while (dense.size() + 12 <= limit) dense += "FindFirstOf ";
    CHECK_EQ(functions_of(dense).at("FindFirstOf").get<std::uint64_t>(), dense.size() / 12);
}

TEST(lua_api_malformed_lua_is_rejected_with_the_lua_api_prefix) {
    for (const char* source : {"local x = \"RegisterHook", "local x = 'a\nlocal y = 1", "local x = [[ open",
                               "local x = [==[ open ]=]", "--[[ never closed", "local x = \"bad \\q escape\"",
                               "local x = \"\\xZZ\"", "local x = \"\\300\"", "local x = \"\\u{110000000}\"",
                               "local x = \"\\u41\"", "local x = \"trailing backslash\\"})
        CHECK_LUA_API_THROWS(extract_lua_api_usage(source));
}

TEST(lua_api_sharing_the_lexer_keeps_the_compat_error_prefix) {
    for (const char* source : {"local x = \"/Game/Open", "--[[ never closed", "local x = \"\\q\""}) {
        bool prefixed = false;
        try { (void)extract_lua_references(source); }
        catch (const std::invalid_argument& error) { prefixed = std::string(error.what()).rfind("compat: lua: ", 0) == 0; }
        CHECK(prefixed);
    }
}

TEST(lua_api_item_spawner_like_script) {
    const auto r = usage_of(R"LUA(
local ok, UEHelpers = pcall(require, "UEHelpers")
local function construct(path, outer, name)
    return StaticConstructObject(StaticFindObject(path), outer, FName(name))
end
local function label(text) return FText(text or "") end
local pc = FindFirstOf("PlayerController")
LoadAsset("/Game/Items/BP_X")
pcall(RegisterLoadMapPreHook, function() end)
RegisterKeyBind(Key.F7, function() ExecuteInGameThread(toggleMenu) end)
RegisterKeyBind(Key.UP_ARROW, function() ExecuteInGameThread(function() moveSelection(-1) end) end)
widget:SetText(FText("x")) -- SetText is a method; the FText argument is a global use
)LUA");
    CHECK_EQ(dump(r),
             R"({"dynamic_require":false,"functions":{"ExecuteInGameThread":2,"FName":1,"FText":2,)"
             R"("FindFirstOf":1,"Key":2,"LoadAsset":1,"RegisterKeyBind":2,"RegisterLoadMapPreHook":1,)"
             R"("StaticConstructObject":1,"StaticFindObject":1},"modules":["UEHelpers"],)"
             R"("optional":["RegisterLoadMapPreHook"],"shadowed":[]})");
}

TEST(lua_api_pcall_only_uses_are_optional) {
    // Every use is the function argument of pcall/xpcall: optional.
    auto r = usage_of("pcall(RegisterLoadMapPreHook, function() end)\nxpcall(LoadAsset, handler, '/Game/X')\n"
                      "local ok = pcall(FindFirstOf)\nFindFirstOf('PlayerController')\n");
    CHECK_EQ(dump(r.at("optional")), R"(["LoadAsset","RegisterLoadMapPreHook"])");
    CHECK_EQ(r.at("functions").at("FindFirstOf"), 2); // one guarded, one plain: not optional
    // No optional key when nothing is optional; field pcall and nested calls do not count.
    r = usage_of("t.pcall(RegisterHook, f)\npcall(function() RegisterHook('/Script/A.B:C', f) end)\n");
    CHECK(!r.contains("optional"));
    CHECK_EQ(r.at("functions").at("RegisterHook"), 2);
    // The protected call's argument must be the name itself, not a call or a field of it.
    r = usage_of("pcall(FName('x'))\npcall(Key.F1)\n");
    CHECK(!r.contains("optional"));
}

TEST(lua_coverage_optional_absent_function_is_partial_not_blocked) {
    const Json r = runtime({{"RegisterKeyBind", entry("implemented")}, {"RegisterLoadMapPreHook", entry("absent", "never fires")}});
    const auto result = assess_lua_runtime_coverage(
        usage_of("RegisterKeyBind(1, f)\npcall(RegisterLoadMapPreHook, function() end)\n"), r);
    CHECK_EQ(result.at("status"), "partial");
    CHECK_EQ(dump(result.at("missing")), "[]");
    CHECK_EQ(dump(result.at("optional_missing")), R"(["RegisterLoadMapPreHook"])");
    bool row_optional = false;
    for (const auto& row : result.at("functions")) if (row.at("name") == "RegisterLoadMapPreHook") row_optional = row.at("optional") == true;
    CHECK(row_optional);
    // The same function called plainly blocks; and an unlisted optional one is optional-missing too.
    CHECK_EQ(assess_lua_runtime_coverage(usage_of("RegisterLoadMapPreHook(f)\n"), r).at("status"), "blocked");
    const auto unlisted = assess_lua_runtime_coverage(usage_of("pcall(NotifyOnNewObject, '/Script/A.B', f)\n"), r);
    CHECK_EQ(unlisted.at("status"), "partial");
    CHECK_EQ(dump(unlisted.at("optional_missing")), R"(["NotifyOnNewObject"])");
    CHECK(!assess_lua_runtime_coverage(usage_of("RegisterKeyBind(1, f)\n"), r).contains("optional_missing"));
}

TEST(lua_coverage_rejects_bad_optional_lists) {
    const Json r = runtime();
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(Json{{"functions", {{"FName", 1}}}, {"modules", Json::array()},
        {"shadowed", Json::array()}, {"dynamic_require", false}, {"optional", Json::array({"LoadAsset"})}}, r));  // not counted
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(Json{{"functions", {{"FName", 1}}}, {"modules", Json::array()},
        {"shadowed", Json::array()}, {"dynamic_require", false}, {"optional", Json::array({"FName", "FName"})}}, r));
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(Json{{"functions", {{"FName", 1}}}, {"modules", Json::array()},
        {"shadowed", Json::array()}, {"dynamic_require", false}, {"optional", "FName"}}, r));
}

// ---------------------------------------------------------------------------
// Runtime manifest validation
// ---------------------------------------------------------------------------
TEST(lua_runtime_valid_manifest_is_normalized) {
    const Json manifest = runtime({{"RegisterKeyBind", entry("implemented", "caller keys")},
                                   {"ExecuteInGameThread", entry("absent")}},
                                  {{"UEHelpers", entry("partial", "initializes")}}, true);
    const auto normalized = validate_lua_runtime(manifest);
    CHECK_EQ(normalized, manifest);
    CHECK_EQ(dump(normalized),
             R"({"console_wired":true,"functions":{"ExecuteInGameThread":{"status":"absent"},)"
             R"("RegisterKeyBind":{"note":"caller keys","status":"implemented"}},"modules":{"UEHelpers":)"
             R"({"note":"initializes","status":"partial"}},"runtime":"ps5-test","schema":"modbridge.lua-runtime/1",)"
             R"("version":"1"})");
    CHECK_EQ(validate_lua_runtime(runtime()), runtime()); // empty function and module maps are valid
    // Boundary values.
    Json edge = runtime({{"FName", entry("partial", std::string(512, 'n').c_str())}},
                        {{std::string(128, 'M'), entry("absent")}, {"A.b_c-d/e", entry("implemented")}});
    edge["runtime"] = std::string("a") + std::string(63, '-');
    edge["version"] = std::string(32, '~');
    CHECK_EQ(validate_lua_runtime(edge).at("runtime").get<std::string>().size(), 64u);
    for (const char* status : {"implemented", "partial", "absent"})
        (void)validate_lua_runtime(runtime({{"FName", entry(status)}}));
}

TEST(lua_runtime_rejects_bad_structure) {
    CHECK_LUA_API_THROWS(validate_lua_runtime(Json::array()));
    CHECK_LUA_API_THROWS(validate_lua_runtime(nullptr));
    CHECK_LUA_API_THROWS(validate_lua_runtime("text"));
    for (const char* key : {"schema", "runtime", "version", "console_wired", "functions", "modules"}) {
        Json manifest = runtime();
        manifest.erase(key);
        CHECK_LUA_API_THROWS(validate_lua_runtime(manifest));
    }
    Json extra = runtime();
    extra["extra"] = 1;
    CHECK_LUA_API_THROWS(validate_lua_runtime(extra));
    Json wrong_schema = runtime();
    wrong_schema["schema"] = "modbridge.lua-runtime/2";
    CHECK_LUA_API_THROWS(validate_lua_runtime(wrong_schema));
    wrong_schema["schema"] = 1;
    CHECK_LUA_API_THROWS(validate_lua_runtime(wrong_schema));
}

TEST(lua_runtime_rejects_bad_identity_fields) {
    for (const Json& bad : {Json(""), Json("Upper"), Json(".dot"), Json("-dash"), Json("_under"), Json("has space"),
                            Json("a/b"), Json(std::string(65, 'a')), Json(7), Json(nullptr)}) {
        Json manifest = runtime();
        manifest["runtime"] = bad;
        CHECK_LUA_API_THROWS(validate_lua_runtime(manifest));
    }
    for (const Json& bad : {Json(""), Json(std::string(33, '1')), Json("tab\there"), Json("new\nline"),
                            Json("caf\xc3\xa9"), Json(1), Json(nullptr)}) {
        Json manifest = runtime();
        manifest["version"] = bad;
        CHECK_LUA_API_THROWS(validate_lua_runtime(manifest));
    }
    for (const Json& bad : {Json("false"), Json(0), Json(1), Json(nullptr)}) {
        Json manifest = runtime();
        manifest["console_wired"] = bad;
        CHECK_LUA_API_THROWS(validate_lua_runtime(manifest));
    }
}

TEST(lua_runtime_rejects_bad_function_entries) {
    // Names must be catalog names: not Lua base functions, not near misses.
    for (const char* name : {"print", "pairs", "registerhook", "RegisterHook ", "", "Unknown", "FName2"})
        CHECK_LUA_API_THROWS(validate_lua_runtime(runtime({{name, entry("absent")}})));
    for (const Json& bad : {Json("absent"), Json::array(), Json(nullptr), Json(1)})
        CHECK_LUA_API_THROWS(validate_lua_runtime(runtime({{"FName", bad}})));
    CHECK_LUA_API_THROWS(validate_lua_runtime(runtime({{"FName", Json::object()}})));                    // no status
    CHECK_LUA_API_THROWS(validate_lua_runtime(runtime({{"FName", {{"status", "absent"}, {"extra", 1}}}})));
    CHECK_LUA_API_THROWS(validate_lua_runtime(runtime({{"FName", {{"note", "only a note"}}}})));
    for (const Json& status : {Json("Implemented"), Json("unknown"), Json(""), Json(true), Json(nullptr)})
        CHECK_LUA_API_THROWS(validate_lua_runtime(runtime({{"FName", {{"status", status}}}})));
    for (const Json& note : {Json(std::string(513, 'n')), Json("line\nbreak"), Json("tab\t"), Json("caf\xc3\xa9"),
                             Json(1), Json(nullptr), Json::array()})
        CHECK_LUA_API_THROWS(validate_lua_runtime(runtime({{"FName", {{"status", "absent"}, {"note", note}}}})));
    // functions must be an object.
    Json manifest = runtime();
    manifest["functions"] = Json::array();
    CHECK_LUA_API_THROWS(validate_lua_runtime(manifest));
}

TEST(lua_runtime_rejects_bad_module_entries) {
    for (const std::string& name : {std::string(""), std::string("has space"), std::string("a:b"),
                                    std::string("a\\b"), std::string("caf\xc3\xa9"), std::string(129, 'm')})
        CHECK_LUA_API_THROWS(validate_lua_runtime(runtime({}, {{name, entry("absent")}})));
    CHECK_LUA_API_THROWS(validate_lua_runtime(runtime({}, {{"M", {{"status", "absent"}, {"extra", true}}}})));
    CHECK_LUA_API_THROWS(validate_lua_runtime(runtime({}, {{"M", {{"status", "maybe"}}}})));
    Json manifest = runtime();
    manifest["modules"] = Json::array({"M"});
    CHECK_LUA_API_THROWS(validate_lua_runtime(manifest));
    Json many = Json::object();
    for (int i = 0; i < 1024; ++i) many["m" + std::to_string(i)] = entry("absent");
    (void)validate_lua_runtime(runtime({}, many));
    many["overflow"] = entry("absent");
    CHECK_LUA_API_THROWS(validate_lua_runtime(runtime({}, many)));
}

// ---------------------------------------------------------------------------
// Coverage
// ---------------------------------------------------------------------------
TEST(lua_coverage_covered_when_everything_used_is_implemented) {
    const auto result = assess_lua_runtime_coverage(
        usage({{"RegisterKeyBind", 2}, {"CreateInvalidObject", 1}}, Json::array({"UEHelpers"})),
        runtime({{"RegisterKeyBind", entry("implemented")}, {"CreateInvalidObject", entry("implemented")},
                 {"ExecuteInGameThread", entry("absent")}},
                {{"UEHelpers", entry("implemented")}}));
    CHECK_EQ(result.at("status"), "covered");
    CHECK_EQ(result.at("missing").size(), 0u);
    CHECK_EQ(result.at("partial").size(), 0u);
    CHECK_EQ(result.at("missing_modules").size(), 0u);
    CHECK_EQ(result.at("partial_modules").size(), 0u);
    // Only used names are listed; an absent function the script does not use is not.
    CHECK_EQ(result.at("functions").size(), 2u);
    CHECK_EQ(result.at("runtime"), "ps5-test");
    CHECK_EQ(result.at("runtime_version"), "1");
    CHECK_EQ(result.at("console_wired"), false);
    CHECK_EQ(result.at("dynamic_require"), false);
    CHECK_EQ(assess_lua_runtime_coverage(usage(), runtime()).at("status"), "covered"); // nothing used
}

TEST(lua_coverage_partial_function_module_or_dynamic_require) {
    const Json both = runtime({{"ExecuteAsync", entry("partial", "single signature")}, {"FName", entry("implemented")}},
                              {{"UEHelpers", entry("partial", "initializes")}, {"Json", entry("implemented")}});
    auto function_partial = assess_lua_runtime_coverage(usage({{"ExecuteAsync", 3}, {"FName", 1}}), both);
    CHECK_EQ(function_partial.at("status"), "partial");
    CHECK_EQ(dump(function_partial.at("partial")), R"(["ExecuteAsync"])");
    CHECK_EQ(function_partial.at("missing").size(), 0u);

    auto module_partial = assess_lua_runtime_coverage(usage({}, Json::array({"Json", "UEHelpers"})), both);
    CHECK_EQ(module_partial.at("status"), "partial");
    CHECK_EQ(dump(module_partial.at("partial_modules")), R"(["UEHelpers"])");

    auto dynamic = assess_lua_runtime_coverage(usage({{"FName", 1}}, Json::array(), Json::array(), true), both);
    CHECK_EQ(dynamic.at("status"), "partial");
    CHECK_EQ(dynamic.at("dynamic_require"), true);
    CHECK_EQ(dynamic.at("partial").size(), 0u); // dynamic_require alone lists nothing as partial
}

TEST(lua_coverage_blocked_by_absent_unlisted_or_missing_module) {
    const Json r = runtime({{"FName", entry("implemented")}, {"ExecuteInGameThread", entry("absent")},
                            {"ExecuteAsync", entry("partial")}},
                           {{"Known", entry("implemented")}, {"Gone", entry("absent")}});
    // Declared absent.
    auto declared = assess_lua_runtime_coverage(usage({{"ExecuteInGameThread", 6}}), r);
    CHECK_EQ(declared.at("status"), "blocked");
    CHECK_EQ(dump(declared.at("missing")), R"(["ExecuteInGameThread"])");
    // Not listed in the manifest at all: fail closed, as absent.
    auto unlisted = assess_lua_runtime_coverage(usage({{"FindFirstOf", 2}, {"FName", 1}}), r);
    CHECK_EQ(unlisted.at("status"), "blocked");
    CHECK_EQ(dump(unlisted.at("missing")), R"(["FindFirstOf"])");
    const auto& row = unlisted.at("functions").at(1); // rows are sorted: FName, FindFirstOf
    CHECK_EQ(row.at("name"), "FindFirstOf");
    CHECK_EQ(row.at("status"), "absent");
    CHECK(row.contains("note"));
    // A module declared absent, and one not listed.
    auto gone = assess_lua_runtime_coverage(usage({}, Json::array({"Gone", "Known"})), r);
    CHECK_EQ(gone.at("status"), "blocked");
    CHECK_EQ(dump(gone.at("missing_modules")), R"(["Gone"])");
    auto unlisted_module = assess_lua_runtime_coverage(usage({}, Json::array({"Nowhere"})), r);
    CHECK_EQ(unlisted_module.at("status"), "blocked");
    CHECK_EQ(dump(unlisted_module.at("missing_modules")), R"(["Nowhere"])");
    // Blocked wins over partial and over dynamic_require.
    auto all = assess_lua_runtime_coverage(
        usage({{"ExecuteAsync", 1}, {"ExecuteInGameThread", 1}}, Json::array(), Json::array(), true), r);
    CHECK_EQ(all.at("status"), "blocked");
    CHECK_EQ(dump(all.at("partial")), R"(["ExecuteAsync"])");
    CHECK_EQ(dump(all.at("missing")), R"(["ExecuteInGameThread"])");
}

TEST(lua_coverage_rows_are_sorted_and_carry_manifest_notes) {
    const auto result = assess_lua_runtime_coverage(
        usage({{"StaticFindObject", 4}, {"ExecuteInGameThread", 6}, {"FName", 2}, {"RegisterKeyBind", 6}}),
        runtime({{"RegisterKeyBind", entry("implemented", "caller keys")}, {"StaticFindObject", entry("absent", "nil lookup")},
                 {"ExecuteInGameThread", entry("absent")}}));
    CHECK_EQ(dump(result.at("functions")),
             R"([{"name":"ExecuteInGameThread","status":"absent","uses":6},)"
             R"({"name":"FName","note":"not listed in the runtime manifest; counted as absent","status":"absent","uses":2},)"
             R"({"name":"RegisterKeyBind","note":"caller keys","status":"implemented","uses":6},)"
             R"({"name":"StaticFindObject","note":"nil lookup","status":"absent","uses":4}])");
    CHECK_EQ(dump(result.at("missing")), R"(["ExecuteInGameThread","FName","StaticFindObject"])");
    CHECK_EQ(result.at("status"), "blocked");
    CHECK_EQ(result.size(), 11u);
}

TEST(lua_coverage_console_wired_is_reported_never_folded_into_status) {
    const Json functions = {{"FName", entry("implemented")}};
    for (bool wired : {false, true}) {
        const auto covered = assess_lua_runtime_coverage(usage({{"FName", 1}}), runtime(functions, Json::object(), wired));
        CHECK_EQ(covered.at("console_wired"), wired);
        CHECK_EQ(covered.at("status"), "covered");
        const auto blocked = assess_lua_runtime_coverage(usage({{"FText", 1}}), runtime(functions, Json::object(), wired));
        CHECK_EQ(blocked.at("console_wired"), wired);
        CHECK_EQ(blocked.at("status"), "blocked");
    }
}

TEST(lua_coverage_passes_shadowed_names_through) {
    const auto result = assess_lua_runtime_coverage(usage({}, Json::array(), Json::array({"RegisterHook", "FName"})), runtime());
    CHECK_EQ(dump(result.at("shadowed")), R"(["FName","RegisterHook"])");
    CHECK_EQ(result.at("status"), "covered"); // a shadowed name is not API use
}

TEST(lua_coverage_rejects_invalid_usage_and_manifest) {
    const Json ok = runtime();
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(Json::array(), ok));
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(nullptr, ok));
    for (const char* key : {"functions", "modules", "shadowed", "dynamic_require"}) {
        Json broken = usage();
        broken.erase(key);
        CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(broken, ok));
    }
    Json extra = usage();
    extra["extra"] = 1;
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(extra, ok));
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(usage({{"print", 1}}), ok));            // not a catalog name
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(usage({{"FName", 0}}), ok));            // zero uses
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(usage({{"FName", -1}}), ok));
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(usage({{"FName", "1"}}), ok));
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(usage({{"FName", 1.5}}), ok));
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(usage({{"FName", 4294967296ull}}), ok));
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(usage({}, Json::array({"bad name"})), ok));
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(usage({}, Json::array({1})), ok));
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(usage({}, Json::object()), ok));
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(usage({}, Json::array(), Json::array({"print"})), ok));
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(usage({{"FName", 1}}, Json::array(), Json::array({"FName"})), ok));
    Json bad_dynamic = usage();
    bad_dynamic["dynamic_require"] = "yes";
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(bad_dynamic, ok));
    // The manifest is validated too.
    Json bad_manifest = runtime();
    bad_manifest["extra"] = true;
    CHECK_LUA_API_THROWS(assess_lua_runtime_coverage(usage(), bad_manifest));
}

TEST(lua_coverage_end_to_end_from_source) {
    const Json r = runtime({{"RegisterKeyBind", entry("implemented", "caller keys")}, {"ExecuteInGameThread", entry("absent")},
                            {"ExecuteAsync", entry("partial")}},
                           {{"UEHelpers", entry("partial", "initializes")}});
    const auto result = assess_lua_runtime_coverage(usage_of(R"LUA(
local ok, H = pcall(require, "UEHelpers")
RegisterKeyBind(1, function() ExecuteInGameThread(f) end)
ExecuteAsync(g)
)LUA"), r);
    CHECK_EQ(result.at("status"), "blocked");
    CHECK_EQ(dump(result.at("missing")), R"(["ExecuteInGameThread"])");
    CHECK_EQ(dump(result.at("partial")), R"(["ExecuteAsync"])");
    CHECK_EQ(dump(result.at("partial_modules")), R"(["UEHelpers"])");
    // The same script against a runtime that implements everything it uses is covered.
    const Json full = runtime({{"RegisterKeyBind", entry("implemented")}, {"ExecuteInGameThread", entry("implemented")},
                               {"ExecuteAsync", entry("implemented")}},
                              {{"UEHelpers", entry("implemented")}});
    CHECK_EQ(assess_lua_runtime_coverage(usage_of("local ok, H = pcall(require, 'UEHelpers') ExecuteAsync(g)"), full).at("status"),
             "covered");
}

// ---------------------------------------------------------------------------
// Inventory fact
// ---------------------------------------------------------------------------
TEST(lua_api_inventory_records_usage_next_to_references) {
    test::TempDir dir;
    const std::string script = "local ok, H = pcall(require, 'UEHelpers')\nRegisterKeyBind(1, function() ExecuteInGameThread(f) end)\n"
                               "local a = StaticFindObject('/Game/A/B')\n";
    std::ofstream(dir.path / "main.lua", std::ios::binary) << script;
    std::ofstream(dir.path / "broken.lua", std::ios::binary) << "local x = \"never closed\n";
    std::ofstream(dir.path / "data.txt", std::ios::binary) << "ExecuteInGameThread";
    const auto inventory = analyze_package(dir.path, Limits{});
    bool saw_main = false, saw_broken = false;
    for (const auto& item : inventory.at("entries")) {
        const auto& path = item.at("path").get_ref<const std::string&>();
        const auto& facts = item.at("facts");
        if (path == "main.lua") {
            saw_main = true;
            CHECK_EQ(facts.at("lua_api"), extract_lua_api_usage(script));
            CHECK_EQ(facts.at("lua_api").at("functions").at("ExecuteInGameThread"), 1);
            CHECK_EQ(facts.at("lua_references"), extract_lua_references(script)); // unchanged fact
        } else if (path == "broken.lua") {
            saw_broken = true;
            CHECK_EQ(facts.at("lua_references_unavailable"), "unreadable");
            CHECK(!facts.contains("lua_api")); // unreadable is never recorded as an empty usage
            CHECK(!facts.contains("lua_references"));
        } else {
            CHECK(!facts.contains("lua_api")); // only Lua entries carry it
        }
    }
    CHECK(saw_main);
    CHECK(saw_broken);
}
