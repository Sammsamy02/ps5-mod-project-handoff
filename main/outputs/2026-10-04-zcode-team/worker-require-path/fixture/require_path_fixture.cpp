// worker-require-path: host regression for the native Lua require chain.
//
// Under test (REAL code, byte-exact extraction / verbatim copy):
//   - LuaMod.require_path_A.inc : staged LuaMod.cpp lines 42-54
//     (modbridge::normalize_path_for_lua local override)
//   - LuaMod.require_path_B.inc : staged LuaMod.cpp lines 736-925
//     (native_lua_push_text / NativeLuaReader / native_lua_reader /
//      native_lua_chunk / native_lua_file / native_lua_loadfile /
//      native_lua_dofile_continue / native_lua_dofile / native_lua_module)
//   - NativeModSource.hpp/.cpp  : port tree src, byte-identical to the
//     lineage-overlay-2-l11-b Platform/NativeModSource.hpp header
//     (validate_path + read + ctor-derived lua_mods rows incl. scripts_path)
//
// GLUE (reconstructed, marked at definition): the native branch of
// LuaMod::custom_module_searcher (staged LuaMod.cpp lines 1240-1257) and the
// native-branch searcher installation (lines 1179-1195) mirrored over a host
// context struct instead of the LuaMod* upvalue. All resolution logic itself
// lives in the extracted native_lua_module above.
//
// Host supplies the "activation" bytes by reading the staged Item Spawner
// bundle per its loadlist.txt rows (roles lua_mod/config), the host
// equivalent of MakeActivatedUE4SSModSource's file registration.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "lua.hpp"
#include "extraction/LuaMod.require_path_A.inc"  // global scope: modbridge
#include "Platform/NativeModSource.hpp"

namespace RC {
#include "extraction/LuaMod.require_path_B.inc"  // anonymous-namespace helpers
}

// PROVENANCE: the vendored UE4SS LuaRaw fork declares LuaLock/LuaUnlock/
// LuaLockInitial/LuaLockFinal in lua.h and calls them from lapi.c/lstate.c;
// on target these are defined by deps/first/LuaRaw/src/luauser.c (Windows
// CRITICAL_SECTION). Host fixture is single-threaded: no-op stubs preserve
// call sites without threading semantics. Not part of the code under test.
extern "C" {
void LuaLock(lua_State*) {}
void LuaUnlock(lua_State*) {}
void LuaLockInitial(lua_State*) {}
void LuaLockFinal(lua_State*) {}
}

namespace fs = std::filesystem;

namespace RC { namespace fixture {

namespace {

struct CaseResult
{
    std::string name;
    bool pass = false;
    std::string detail;
    double ms = 0.0;
};

std::vector<CaseResult> g_cases;

template <typename Fn>
CaseResult run_case(const std::string& name, Fn&& fn)
{
    CaseResult result;
    result.name = name;
    const auto t0 = std::chrono::steady_clock::now();
    try
    {
        result.detail = fn();
        result.pass = result.detail.rfind("FAIL", 0) != 0;
    }
    catch (const std::exception& e)
    {
        result.detail = std::string("FAIL exception: ") + e.what();
    }
    catch (...)
    {
        result.detail = "FAIL exception: unknown";
    }
    const auto t1 = std::chrono::steady_clock::now();
    result.ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    g_cases.push_back(result);
    std::cout << (result.pass ? "[PASS] " : "[FAIL] ") << name << " (" << result.ms << " ms)\n"
              << "        " << result.detail << "\n";
    return result;
}

// GLUE: host mirror of the LuaMod native branch context. On target the
// searcher's upvalue is LuaMod* and it reads m_native_source / m_scripts_path.
struct NativeModCtx
{
    const Platform::NativeModSource* native_source = nullptr;
    fs::path m_mod_path;
    fs::path m_scripts_path;
};

// GLUE: byte-faithful mirror of LuaMod::custom_module_searcher's native
// branch (staged LuaMod.cpp lines 1240-1257). Only the upvalue type differs.
int custom_module_searcher_native(lua_State* L)
{
    auto* native_mod = static_cast<NativeModCtx*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (native_mod && native_mod->native_source)
    {
        int result = -1;
        try
        {
            result = native_lua_module(L, *native_mod->native_source,
                                       modbridge::normalize_path_for_lua(native_mod->m_scripts_path));
        }
        catch (const std::exception& error)
        {
            native_lua_push_text(L, error.what());
        }
        catch (...)
        {
            native_lua_push_text(L, "Native Lua module loading failed");
        }
        return result < 0 ? lua_error(L) : result;
    }
    lua_pushstring(L, "module name is required");
    return 1;
}

std::string read_file(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Calls package.searchers[2] with a Lua value on top of the arguments.
// Returns LUA_OK plus the raw returns as strings where string-typed.
int call_searcher_va(lua_State* L, int& nresults, std::string& first, std::string& second)
{
    first.clear();
    second.clear();
    if (std::getenv("REQUIRE_PATH_DEBUG"))
        std::fprintf(stderr, "[debug] top=%d callee=%s arg=%s\n", lua_gettop(L),
                     lua_typename(L, lua_type(L, -2)), lua_typename(L, lua_type(L, -1)));
    const int status = lua_pcall(L, 1, LUA_MULTRET, 0);
    nresults = lua_gettop(L);
    if (status != LUA_OK)
    {
        const char* err = lua_tostring(L, -1);
        first = err ? err : "";
        lua_pop(L, 1);
        return status;
    }
    if (nresults >= 1 && lua_isstring(L, -nresults)) first = lua_tostring(L, -nresults);
    if (nresults >= 2 && lua_isstring(L, -nresults + 1)) second = lua_tostring(L, -nresults + 1);
    lua_pop(L, nresults);
    return LUA_OK;
}

// Push package.searchers[2] and the module-name argument:
// leaves [searcher, name] on the stack, ready for lua_pcall(L, 1, MULTRET, 0).
void push_searcher_call(lua_State* L, const char* name)
{
    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) throw std::runtime_error("package table missing");
    lua_getfield(L, -1, "searchers");
    if (!lua_istable(L, -1)) throw std::runtime_error("package.searchers missing");
    lua_rawgeti(L, -1, 2);
    if (!lua_iscfunction(L, -1))
    {
        lua_pop(L, 3);
        throw std::runtime_error("native searcher missing at [2]");
    }
    lua_replace(L, -3);  // stack: [searcher, searchers]
    lua_pop(L, 1);       // stack: [searcher]
    lua_pushstring(L, name);
}

// Full require(name) in protected mode; returns status and module/error text.
int call_require(lua_State* L, const char* name, std::string& text, bool& is_table)
{
    lua_getglobal(L, "require");
    if (!lua_isfunction(L, -1)) throw std::runtime_error("require missing");
    lua_pushstring(L, name);
    text.clear();
    is_table = false;
    const int status = lua_pcall(L, 1, 1, 0);
    if (status == LUA_OK)
    {
        is_table = lua_istable(L, -1) != 0;
        if (is_table)
        {
            lua_getfield(L, -1, "GetPlayer");
            text = lua_isfunction(L, -1) ? "table with GetPlayer" : "table";
            lua_pop(L, 1);
        }
        else
        {
            text = "non-table return";
        }
    }
    else
    {
        const char* err = lua_tostring(L, -1);
        text = err ? err : "";
    }
    lua_pop(L, 1);
    return status;
}

std::string json_escape(const std::string& in)
{
    std::string out;
    for (char c : in)
    {
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += buf;
                }
                else
                {
                    out += c;
                }
        }
    }
    return out;
}

}  // namespace

int run(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: require_path_fixture <bundle-dir> <results-dir>\n";
        return 2;
    }
    const fs::path bundle = fs::absolute(argv[1]);
    const fs::path results = fs::absolute(argv[2]);

    std::cout << "require_path_fixture: bundle=" << bundle.string() << "\n";

    // --- Register bundle files exactly as the loadlist rows declare them ---
    std::vector<std::shared_ptr<std::string>> owners;
    std::vector<Platform::NativeModFile> files;
    std::size_t registered = 0;
    {
        const std::string loadlist = read_file(bundle / "loadlist.txt");
        std::istringstream lines(loadlist);
        std::string line;
        while (std::getline(lines, line))
        {
            if (line.rfind("file\t", 0) != 0) continue;
            std::vector<std::string> parts;
            std::istringstream fields(line);
            std::string part;
            while (std::getline(fields, part, '\t')) parts.push_back(part);
            if (parts.size() < 4) throw std::runtime_error("malformed loadlist row: " + line);
            const std::string& role = parts[1];
            const std::string& path = parts[2];
            const std::size_t declared = static_cast<std::size_t>(std::stoull(parts[3]));
            if (role != "lua_mod" && role != "config" && role != "script") continue;
            std::string bytes = read_file(bundle / fs::path(path));
            if (bytes.size() != declared)
                throw std::runtime_error("size mismatch for " + path + ": declared " +
                                         parts[3] + " actual " + std::to_string(bytes.size()));
            owners.push_back(std::make_shared<std::string>(std::move(bytes)));
            files.push_back({path,
                             {owners.back(),
                              std::string_view(owners.back()->data(), owners.back()->size())}});
            ++registered;
        }
    }
    std::cout << "registered " << registered << " loadlist files into NativeModSource\n";

    // --- Real NativeModSource construction (sorts, validates, derives rows) ---
    Platform::NativeModSource source(std::move(files));

    run_case("native_source_construction", [&] {
        std::ostringstream out;
        out << registered << " files; lua_mods=";
        for (const auto& mod : source.lua_mods())
            out << "{" << mod.name << "," << mod.path << "," << mod.scripts_path << "}";
        out << " startup=";
        for (const auto& name : source.startup_mod_names()) out << name << ",";
        std::string detail = out.str();
        const auto& mods = source.lua_mods();
        if (mods.size() != 1) return "FAIL expected exactly 1 lua_mod row, got " + std::to_string(mods.size()) + "; " + detail;
        if (mods[0].name != "TownfallItemSpawner" || mods[0].path != "ue4ss/Mods/TownfallItemSpawner" ||
            mods[0].scripts_path != "ue4ss/Mods/TownfallItemSpawner/Scripts")
            return "FAIL unexpected lua_mod row; " + detail;
        if (std::find(source.startup_mod_names().begin(), source.startup_mod_names().end(),
                      "TownfallItemSpawner") == source.startup_mod_names().end())
            return "FAIL TownfallItemSpawner not a startup mod; " + detail;
        return detail;
    });

    // --- LuaMod ctor native branch mirror (staged LuaMod.cpp lines 934-946) ---
    NativeModCtx ctx;
    ctx.native_source = &source;
    ctx.m_mod_path = std::filesystem::u8path("ue4ss/Mods/TownfallItemSpawner");

    run_case("lua_mod_ctor_scripts_path", [&] {
        const auto path = modbridge::normalize_path_for_lua(ctx.m_mod_path);
        const auto& mods = ctx.native_source->lua_mods();
        const auto found = std::find_if(mods.begin(), mods.end(),
            [&](const auto& mod) { return mod.path == path; });
        if (found == mods.end()) return std::string("FAIL mod absent from native Lua source: ") + path;
        ctx.m_scripts_path = std::filesystem::u8path(found->scripts_path);
        const auto scripts = modbridge::normalize_path_for_lua(ctx.m_scripts_path);
        if (scripts != found->scripts_path)
            return "FAIL u8path round-trip drifted: registered '" + found->scripts_path +
                   "' normalized '" + scripts + "'";
        return "m_mod_path='" + path + "' m_scripts_path(u8path)->normalized='" + scripts + "'";
    });

    const std::string scripts_path = modbridge::normalize_path_for_lua(ctx.m_scripts_path);
    const std::string shared = "ue4ss/Mods/shared/";

    // Independent candidate construction for the names under test (mirrors
    // native_lua_module's paths vector, LuaMod.cpp lines 898-905).
    const auto build_candidates = [&](const std::string& relative) {
        return std::vector<std::string>{
            scripts_path + "/" + relative + ".lua",
            scripts_path + "/" + relative + "/init.lua",
            shared + relative + ".lua",
            shared + relative + "/init.lua",
            shared + relative + "/" + relative + ".lua",
        };
    };
    const auto build_missing = [&](const std::vector<std::string>& candidates) {
        std::string missing;
        for (const auto& path : candidates)
            missing += "\n\tno native Lua file '" + path + "'";
        return missing;
    };

    const std::string uehelpers_relative = "UEHelpers";
    const std::vector<std::string> uehelpers_candidates = build_candidates(uehelpers_relative);
    const std::string expected_hit = uehelpers_candidates[4];  // shared/<m>/<m>.lua

    // --- Candidate-by-candidate probe through the REAL source.read ---
    run_case("candidate_probe_source_read", [&] {
        std::ostringstream out;
        int hit_index = -1;
        for (std::size_t i = 0; i < uehelpers_candidates.size(); ++i)
        {
            const auto buffer = source.read(uehelpers_candidates[i]);
            out << "[" << i << "] " << (buffer ? std::to_string(buffer->bytes.size()) + "B" : "miss")
                << " " << uehelpers_candidates[i] << "\n        ";
            if (buffer) { hit_index = static_cast<int>(i); }
            else if (hit_index >= 0)
                return "FAIL read hit before candidate " + std::to_string(i);
        }
        if (hit_index != 4) return "FAIL expected hit at index 4, got " + std::to_string(hit_index);
        return out.str();
    });

    // --- Lua state + native-branch searcher installation (lines 1179-1195) ---
    lua_State* L = luaL_newstate();
    if (!L) throw std::runtime_error("luaL_newstate failed");
    luaL_openlibs(L);
    {
        lua_getglobal(L, "package");
        if (!lua_istable(L, -1)) throw std::runtime_error("package table not found");
        lua_getfield(L, -1, "searchers");
        if (!lua_istable(L, -1)) throw std::runtime_error("package.searchers not found");
        lua_createtable(L, 2, 0);
        lua_rawgeti(L, -2, 1);  // keep package.preload searcher first
        if (!lua_isfunction(L, -1)) throw std::runtime_error("preload searcher missing");
        lua_rawseti(L, -2, 1);
        lua_pushlightuserdata(L, &ctx);
        lua_pushcclosure(L, custom_module_searcher_native, 1);
        lua_rawseti(L, -2, 2);
        lua_setfield(L, -3, "searchers");
        lua_pop(L, 2);
    }

    // --- Positive: searcher-level resolution of require("UEHelpers") ---
    run_case("searcher_UEHelpers", [&] {
        push_searcher_call(L, "UEHelpers");
        int nresults = 0;
        std::string first, second;
        const int status = call_searcher_va(L, nresults, first, second);
        if (status != LUA_OK) return "FAIL searcher raised: " + first;
        if (nresults != 2) return "FAIL expected 2 returns (chunk, path), got " + std::to_string(nresults);
        if (second != expected_hit)
            return "FAIL resolved '" + second + "' expected '" + expected_hit + "'";
        // chunk type + chunkname are on the stack from the call above? No --
        // call_searcher_va already popped; redo the call keeping the stack.
        push_searcher_call(L, "UEHelpers");
        if (lua_pcall(L, 1, LUA_MULTRET, 0) != LUA_OK)
            return std::string("FAIL second searcher call raised");
        const std::string path = lua_tostring(L, -1);
        if (!lua_isfunction(L, -2)) { lua_pop(L, 2); return std::string("FAIL first return is not a chunk function"); }
        lua_pushvalue(L, -2);  // copy chunk to top for getinfo
        lua_Debug ar{};
        if (lua_getinfo(L, ">S", &ar) == 0) { lua_pop(L, 2); return std::string("FAIL lua_getinfo failed"); }
        const std::string chunk_source = ar.source;
        lua_pop(L, 2);  // chunk, path
        const std::string expect_chunk = "@" + expected_hit;
        if (chunk_source != expect_chunk)
            return "FAIL chunk source '" + chunk_source + "' expected '" + expect_chunk + "'";
        return "resolved='" + path + "' chunk_type=function chunk_source='" + chunk_source + "'";
    });

    // --- Positive: full require("UEHelpers") executes the chunk.
    // UEHelpers.lua line 35 runs `CreateInvalidObject()` at top level; that
    // global is registered by the UE4SS Lua environment on target and is
    // absent on a bare host state. Expected here: require resolves + loads
    // the chunk, execution raises, and the error text carries the resolved
    // chunkname (proof the executed chunk came from the resolved bytes).
    run_case("require_UEHelpers_chunk_attribution", [&] {
        std::string text;
        bool is_table = false;
        const int status = call_require(L, "UEHelpers", text, is_table);
        const std::string prefix = expected_hit + ":";
        if (text.rfind(prefix, 0) != 0)
            return std::string("FAIL execution error not attributed to resolved chunk; got: ") + text;
        if (text.find("CreateInvalidObject") == std::string::npos)
            return std::string("FAIL unexpected error (expected missing UE4SS global): ") + text;
        return "require resolved + loaded the chunk; execution stopped at UE4SS-global "
               "dependency (expected off-target): " + text;
    });

    // --- Positive: full require("UEHelpers") with host env stub.
    // PROVENANCE: stubs ONLY the UE4SS global CreateInvalidObject as a
    // sentinel (top-level caches never dereference it during load). The real
    // global is registered by the UE4SS Lua environment on target. This case
    // proves the require plumbing end to end: searcher -> chunk -> execution
    // -> module table return -> package.loaded caching.
    run_case("require_UEHelpers_full_with_env_stub", [&] {
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "loaded");
        lua_pushnil(L);
        lua_setfield(L, -2, "UEHelpers");  // failed loads are not cached; belt and braces
        lua_pop(L, 2);
        lua_pushcfunction(L, [](lua_State* St) -> int {
            lua_createtable(St, 0, 0);  // sentinel; never dereferenced at load time
            return 1;
        });
        lua_setglobal(L, "CreateInvalidObject");
        std::string text;
        bool is_table = false;
        const int status = call_require(L, "UEHelpers", text, is_table);
        if (status != LUA_OK) return std::string("FAIL require raised even with stub: ") + text;
        if (!is_table) return std::string("FAIL require returned non-table: ") + text;
        // verify module contents + loaded caching
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "loaded");
        lua_getfield(L, -1, "UEHelpers");
        const bool cached = lua_istable(L, -1) != 0;
        lua_pop(L, 3);
        if (!cached) return std::string("FAIL module not cached in package.loaded");
        return "require returned module table (" + text + "), cached in package.loaded; "
               "loader plumbing proven end to end (host stub of CreateInvalidObject only)";
    });

    // --- Byte equality: REAL source.read bytes vs staged bundle file ---
    std::string dump_path;
    run_case("bytes_equal_staged_UEHelpers", [&] {
        const auto buffer = source.read(expected_hit);
        if (!buffer) return "FAIL source.read returned no buffer for " + expected_hit;
        const std::string staged = read_file(bundle / fs::path("ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"));
        if (buffer->bytes.size() != staged.size())
            return "FAIL size mismatch: native " + std::to_string(buffer->bytes.size()) +
                   " staged " + std::to_string(staged.size());
        if (std::memcmp(buffer->bytes.data(), staged.data(), staged.size()) != 0)
            return std::string("FAIL byte mismatch against staged UEHelpers.lua");
        const fs::path dump = results / "resolved_UEHelpers.lua";
        std::ofstream out(dump, std::ios::binary);
        out.write(buffer->bytes.data(), static_cast<std::streamsize>(buffer->bytes.size()));
        out.close();
        dump_path = dump.string();
        return "native buffer " + std::to_string(buffer->bytes.size()) +
               "B == staged UEHelpers.lua " + std::to_string(staged.size()) +
               "B (memcmp clean); dumped to " + dump_path;
    });

    // --- Negative: searcher("NonexistentModule") lists exactly the candidates ---
    const std::vector<std::string> missing_candidates = build_candidates("NonexistentModule");
    const std::string expected_missing = build_missing(missing_candidates);
    run_case("searcher_NonexistentModule", [&] {
        push_searcher_call(L, "NonexistentModule");
        int nresults = 0;
        std::string first, second;
        const int status = call_searcher_va(L, nresults, first, second);
        if (status != LUA_OK) return "FAIL searcher raised instead of reporting: " + first;
        if (nresults != 1) return "FAIL expected 1 return, got " + std::to_string(nresults);
        if (first != expected_missing) return "FAIL message mismatch:\nEXPECTED <<" + expected_missing +
                                                  ">>\nACTUAL   <<" + first + ">>";
        return "exact multi-candidate message (" + std::to_string(missing_candidates.size()) +
               " candidates):\n        " + first;
    });

    // --- Negative: full require("NonexistentModule") surfaces the message ---
    run_case("require_NonexistentModule_full", [&] {
        std::string text;
        bool is_table = false;
        const int status = call_require(L, "NonexistentModule", text, is_table);
        if (status == LUA_OK) return std::string("FAIL require unexpectedly succeeded");
        if (text.find(expected_missing) == std::string::npos)
            return std::string("FAIL require error lacks exact candidate list:\nACTUAL <<") + text + ">>";
        return std::string("require error = \"module 'NonexistentModule' not found:\" + preload miss + exact 5-candidate list");
    });

    // --- Normalization matrix (dots to slashes, case, invalid names) ---
    struct NormCase { std::string arg; bool string_arg; std::string expect; };
    const std::string long_name(4097, 'x');
    const std::vector<NormCase> norm_cases{
        {"a.b", true, "dot-to-slash"},
        {"uehelpers", true, "case-miss"},
        {"..", true, "invalid"},
        {"a:b", true, "invalid"},
        {"a\\b", true, "invalid"},
        {"/a", true, "invalid"},
        {"a/", true, "invalid"},
        {"a//b", true, "invalid"},
        {"", true, "invalid"},
        {long_name, true, "invalid"},
        {"42", false, "not-a-string"},
    };
    int norm_index = 0;
    for (const auto& norm : norm_cases)
    {
        const std::string label = norm.arg.size() > 24
            ? "normalization[" + std::to_string(norm_index) + "]_<" +
              std::to_string(norm.arg.size()) + " chars>"
            : "normalization_" + norm.expect + "_" +
              (norm.string_arg ? (norm.arg.empty() ? "<empty>" : norm.arg) : "<number>");
        ++norm_index;
        run_case(label, [&] {
            if (norm.string_arg)
            {
                push_searcher_call(L, norm.arg.c_str());
            }
            else
            {
                lua_getglobal(L, "package");
                lua_getfield(L, -1, "searchers");
                lua_rawgeti(L, -1, 2);
                if (!lua_iscfunction(L, -1))
                {
                    lua_pop(L, 3);
                    throw std::runtime_error("native searcher missing at [2]");
                }
                lua_replace(L, -3);  // stack: [searcher, searchers]
                lua_pop(L, 1);       // stack: [searcher]
                lua_pushinteger(L, 42);
            }
            int nresults = 0;
            std::string first, second;
            const int status = call_searcher_va(L, nresults, first, second);
            if (status != LUA_OK) return "FAIL searcher raised: " + first;
            if (norm.expect == "invalid" || norm.expect == "not-a-string")
            {
                if (nresults != 1) return "FAIL expected 1 return, got " + std::to_string(nresults);
                const std::string expected_text = norm.expect == "invalid"
                    ? "invalid native Lua module name" : "module name must be a string";
                if (first != expected_text) return "FAIL expected '" + expected_text + "' got '" + first + "'";
                return first;
            }
            if (norm.expect == "dot-to-slash")
            {
                // native_lua_module maps '.' to '/' FIRST; every candidate is
                // built from the slash form, including the shared/<m>/<m>.lua
                // shape (shared/a/b/a/b.lua for name "a.b").
                const auto dot_candidates = build_candidates("a/b");
                if (first != build_missing(dot_candidates))
                    return std::string("FAIL dot candidate list mismatch:\nACTUAL   <<" + first +
                                       ">>\nEXPECTED <<" + build_missing(dot_candidates) + ">>");
                if (first.find("a.b") != std::string::npos || first.find('\\') != std::string::npos ||
                    first.find("..") != std::string::npos)
                    return std::string("FAIL leaked dot/backslash into candidates:\n") + first;
                return std::string("dots mapped to slashes across all 5 candidates (e.g. ue4ss/Mods/shared/a/b/a/b.lua)");
            }
            if (norm.expect == "case-miss")
            {
                if (first != build_missing(build_candidates("uehelpers")))
                    return "FAIL lowercase candidate list mismatch:\n" + first;
                if (first.find("UEHelpers.lua") != std::string::npos)
                    return std::string("FAIL case-folded onto UEHelpers bundle key");
                return std::string("lowercase name misses (case preserved); 5 lowercase candidates listed");
            }
            return std::string("FAIL unknown expectation");
        });
    }

    // --- Host-only u8path ASCII round-trip observation ---
    run_case("host_u8path_ascii_roundtrip", [&] {
        const std::string key = scripts_path;
        const auto round = modbridge::normalize_path_for_lua(std::filesystem::u8path(key));
        if (round != key) return std::string("FAIL host u8path round-trip drifted for ASCII key");
        return std::string("host libc++: u8path(\x27" + key + "\x27)->string() byte-clean (PROSPERO libc++ NOT exercised here; PROVENANCE-UNVERIFIED)");
    });

    // --- Emit JSON ---
    const bool all_pass = std::all_of(g_cases.begin(), g_cases.end(),
                                      [](const CaseResult& c) { return c.pass; });
    std::ostringstream json;
    json << "{\n  \"fixture\": \"worker-require-path/require_path_fixture\",\n";
    json << "  \"verdict\": \"" << (all_pass ? "PASS" : "FAIL") << "\",\n";
    json << "  \"resolved_path\": \"" << json_escape(expected_hit) << "\",\n";
    json << "  \"scripts_path\": \"" << json_escape(scripts_path) << "\",\n";
    json << "  \"candidates\": [\n";
    for (std::size_t i = 0; i < uehelpers_candidates.size(); ++i)
        json << "    {\"index\": " << i << ", \"path\": \"" << json_escape(uehelpers_candidates[i])
             << "\", \"hit\": " << (i == 4 ? "true" : "false") << "}"
             << (i + 1 < uehelpers_candidates.size() ? "," : "") << "\n";
    json << "  ],\n";
    json << "  \"byte_equality\": {\n    \"staged\": \"ue4ss/Mods/shared/UEHelpers/UEHelpers.lua\",\n";
    json << "    \"expected_size\": 9962,\n    \"expected_sha256\": \"5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7\",\n";
    json << "    \"resolved_dump\": \"" << json_escape(dump_path) << "\"\n  },\n";
    json << "  \"nonexistent_message\": \"" << json_escape(expected_missing) << "\",\n";
    json << "  \"cases\": [\n";
    for (std::size_t i = 0; i < g_cases.size(); ++i)
    {
        const auto& c = g_cases[i];
        json << "    {\"name\": \"" << json_escape(c.name) << "\", \"pass\": "
             << (c.pass ? "true" : "false") << ", \"ms\": " << c.ms
             << ", \"detail\": \"" << json_escape(c.detail) << "\"}"
             << (i + 1 < g_cases.size() ? "," : "") << "\n";
    }
    json << "  ],\n  \"overall_pass\": " << (all_pass ? "true" : "false") << "\n}\n";
    {
        std::ofstream out(results / "require_path.json", std::ios::binary);
        out << json.str();
    }

    lua_close(L);
    std::cout << (all_pass ? "OVERALL: PASS\n" : "OVERALL: FAIL\n");
    return all_pass ? 0 : 1;
}

}}  // namespace RC::fixture

int main(int argc, char** argv)
{
    return RC::fixture::run(argc, argv);
}
