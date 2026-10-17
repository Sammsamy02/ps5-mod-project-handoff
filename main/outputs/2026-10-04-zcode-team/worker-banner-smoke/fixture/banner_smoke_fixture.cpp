// worker-banner-smoke: host regression for the main.lua load-time path + the
// print banner chain through the FIXED to_generic_string.
//
// Reuses worker-require-path's verified fixture pieces (hashes re-verified in
// build_and_run.sh before use):
//   - LuaMod.require_path_A.inc : staged LuaMod.cpp lines 42-54
//     (modbridge::normalize_path_for_lua local override), byte-exact
//   - LuaMod.require_path_B.inc : staged LuaMod.cpp lines 736-925
//     (native_lua_chunk + friends), byte-exact
//   - Platform/NativeModSource.hpp/.cpp : verbatim port tree
//   - String.to_generic_string.inc : staged Helpers/String.hpp (sha c76be502)
//     lines 595-636, byte-exact sed extraction of the FIXED to_generic_string
//     (__PROSPERO__ byte-widening branch, PS5 codecvt bug workaround)
//
// Under test end to end:
//   LuaMod::start_mod ordering (staged LuaMod.cpp c92c92ff):
//     :6804 setup_lua_global_functions        -> print   (:1551),
//                                                    RegisterKeyBind (:1945),
//                                                    RegisterLoadMapPreHook (:2741)
//     :6813 register_input_globals            -> Key table (:583 F7, :488 RETURN,
//                                                    :510-513 arrows, :644 make_global)
//     :6959 setup_custom_module_loader        -> native require searcher
//     :6966 load_and_execute_script(main.lua) -> :6880 m_native_source->read,
//                                                    :6911 native_lua_chunk,
//                                                    :6918 lua_pcall(0,0,handler)
//   print pipeline (port LuaLibrary.cpp:35-110):
//     luaL_tolstring -> to_generic_string -> Output::send("[Lua] " + text)
//
// STUBS (marked at definition; minimal load-time surface only, no behavior
// simulation beyond registration recording):
//   Key, RegisterKeyBind, RegisterLoadMapPreHook.
// NOT stubbed (referenced by main.lua only inside closures that load never
// calls): ExecuteInGameThread, require/UEHelpers, FindFirstOf, StaticFindObject,
// StaticConstructObject, FName, FText, LoadAsset.

#include <algorithm>
#include <cstdint>
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

// ---------------------------------------------------------------------------
// GLUE (marked): host stand-in for <String/StringType.hpp>. On PROSPERO
// CharType is wchar_t with a 2-byte width; the host macOS wchar_t is 4 bytes,
// which would NOT reproduce the target's unit width. char16_t matches the
// PROSPERO 2-byte width exactly, so the byte-widening conversion under test
// stores the same code units it would on target.
// ---------------------------------------------------------------------------
namespace RC
{
    using CharType = char16_t;
#define STR(str) u##str
    using StringType = std::basic_string<CharType>;
    using StringViewType = std::basic_string_view<CharType>;

#include "extraction/String.to_generic_string.inc"  // staged String.hpp L595-636, byte-exact
}

#include "Platform/NativeModSource.hpp"

namespace RC {
#include "extraction/LuaMod.require_path_B.inc"  // anonymous-namespace loader helpers
}

// PROVENANCE: vendored UE4SS LuaRaw declares LuaLock*/LuaLockInitial/Final in
// lua.h; on target they come from luauser.c. Host fixture: no-op stubs
// (single-threaded), same as worker-require-path. Not under test.
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
};

std::vector<CaseResult> g_cases;
int g_failed = 0;

template <typename Fn>
void run_case(const std::string& name, Fn&& fn)
{
    CaseResult result;
    result.name = name;
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
    if (!result.pass) ++g_failed;
    g_cases.push_back(result);
    std::cout << (result.pass ? "[PASS] " : "[FAIL] ") << name << "\n"
              << "        " << result.detail << "\n";
}

std::string read_file(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
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
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
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

std::string hex_dump(const std::string& in)
{
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (std::size_t i = 0; i < in.size(); ++i)
    {
        const auto b = static_cast<unsigned char>(in[i]);
        if (i && (i % 32) == 0) out += "\n        ";
        out += digits[b >> 4];
        out += digits[b & 0xF];
        out += ' ';
    }
    return out;
}

// GLUE: UTF-8 rendering of the captured StringType. Target log devices render
// each code unit; for the ASCII banner every unit is < 0x80 and maps 1:1, so
// the captured bytes are the log bytes. Non-ASCII units would be emitted as
// proper UTF-8; units >= 0x100 cannot appear from the byte-widening branch.
std::string to_log_bytes(const StringType& in)
{
    std::string out;
    for (CharType unit : in)
    {
        const auto u = static_cast<uint32_t>(unit);
        if (u < 0x80) out.push_back(static_cast<char>(u));
        else if (u < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (u >> 6)));
            out.push_back(static_cast<char>(0x80 | (u & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xE0 | (u >> 12)));
            out.push_back(static_cast<char>(0x80 | ((u >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (u & 0x3F)));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// STUB (marked): Output::send stand-in — the output-device sink. On target
// Output::send writes the StringType into the UE4SS.log device chain; here it
// appends to a capture vector. The conversion under test (to_generic_string,
// staged c76be502 byte-widening) is the REAL code and runs inside
// fixture_global_print below, identical for both the log channel and the
// (unused host-side) FOutputDevice->Log channel (port LuaLibrary.cpp:62-67).
// ---------------------------------------------------------------------------
std::vector<StringType> g_output_sends;

// ---------------------------------------------------------------------------
// STUB (marked): registration recorders. The real runtime registers these
// globals from staged LuaMod.cpp c92c92ff (citations in DEPENDENCY-MAP.md);
// the stubs record only what main.lua's load path hands them.
// ---------------------------------------------------------------------------
struct KeyBind
{
    uint32_t key = 0;
    bool callback_is_function = false;
};
std::vector<KeyBind> g_keybinds;

int stub_RegisterKeyBind(lua_State* L)
{
    KeyBind bind;
    bind.key = static_cast<uint32_t>(lua_tointeger(L, 1));
    bind.callback_is_function = lua_isfunction(L, 2) != 0;
    g_keybinds.push_back(bind);
    return 0;
}

bool g_loadmap_prehook_called = false;
bool g_loadmap_prehook_fn = false;

int stub_RegisterLoadMapPreHook(lua_State* L)
{
    g_loadmap_prehook_called = true;
    g_loadmap_prehook_fn = lua_isfunction(L, 1) != 0;
    return 0;
}

// ---------------------------------------------------------------------------
// STUB (marked): fixture print = MIRROR of RC::LuaLibrary::global_print
// (port tree src/LuaLibrary.cpp:35-110), no-output-device branch: on host
// there is no OutputDeviceRef global, so get_outputdevice_ref (LuaLibrary.cpp
// :17-27) returns nullptr on target too when the device is not set. The
// luaL_tolstring -> to_generic_string -> append chain is byte-faithful;
// luaL_tolstring's produced value is discarded (LuaMadeSimple::discard_value
// equivalent = lua_pop(1), port LuaLibrary.cpp:98-99).
// ---------------------------------------------------------------------------
int fixture_global_print(lua_State* L)
{
    StringType formatted_string = STR("[Lua] ");
    const int stack_size = lua_gettop(L);
    for (int32_t i = 1; i <= stack_size; i++)
    {
        const char* raw_string = luaL_tolstring(L, i, nullptr);
        if (i > 1) formatted_string.append(STR("\t\t"));
        if (raw_string)
        {
            formatted_string.append(to_generic_string(raw_string));
        }
        else
        {
            formatted_string.append(to_generic_string("nil"));
        }
        lua_pop(L, 1);  // discard the luaL_tolstring stack item
    }
    g_output_sends.push_back(formatted_string);  // Output::send stand-in
    return 0;
}

// GLUE: host stand-in for LuaMadeSimple::push_pcall_error_handler (staged
// LuaMod.cpp:6910 uses it before lua_pcall). Captures the message; does not
// walk/print the Lua stack like the real handler.
std::string g_last_lua_error;

int fixture_error_handler(lua_State* L)
{
    const char* msg = lua_tostring(L, -1);
    g_last_lua_error = msg ? msg : "(non-string error object)";
    return 1;
}

// GLUE (copied from worker-require-path fixture): host mirror of the LuaMod
// native-branch searcher context (staged LuaMod.cpp lines 1240-1257 over a
// host struct instead of the LuaMod* upvalue) + its installation (lines
// 1179-1195). On target setup_custom_module_loader runs at start_mod :6959,
// before main.lua executes at :6966.
struct NativeModCtx
{
    const Platform::NativeModSource* native_source = nullptr;
    fs::path m_mod_path;
    fs::path m_scripts_path;
};

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

void install_native_searcher(lua_State* L, NativeModCtx& ctx)
{
    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) throw std::runtime_error("package table not found");
    lua_getfield(L, -1, "searchers");
    if (!lua_istable(L, -1)) throw std::runtime_error("package.searchers not found");
    lua_createtable(L, 2, 0);
    lua_rawgeti(L, -2, 1);  // package.preload searcher stays first
    if (!lua_isfunction(L, -1)) throw std::runtime_error("preload searcher missing");
    lua_rawseti(L, -2, 1);
    lua_pushlightuserdata(L, &ctx);
    lua_pushcclosure(L, custom_module_searcher_native, 1);
    lua_rawseti(L, -2, 2);
    lua_setfield(L, -3, "searchers");
    lua_pop(L, 2);
}

// Key codes: real values from deps/first/Input/include/Input/KeyDef.hpp
// (RC::Input enum: F7=0x76 @138, RETURN=0x0D @30, LEFT_ARROW=0x25 @57,
// UP_ARROW=0x26 @58, RIGHT_ARROW=0x27 @59, DOWN_ARROW=0x28 @60). These are
// exactly what register_input_globals feeds the Key table with
// (staged LuaMod.cpp:488/510-513/583, static_cast<uint32_t>(Input::Key::...)).
constexpr uint32_t KEY_F7 = 0x76;
constexpr uint32_t KEY_UP_ARROW = 0x26;
constexpr uint32_t KEY_DOWN_ARROW = 0x28;
constexpr uint32_t KEY_RETURN = 0x0D;
constexpr uint32_t KEY_LEFT_ARROW = 0x25;
constexpr uint32_t KEY_RIGHT_ARROW = 0x27;

// MIRROR of the target registration order in LuaMod::start_mod / setup:
//   :6804 setup_lua_global_functions -> print, RegisterKeyBind,
//          RegisterLoadMapPreHook (order inside the function: print first)
//   :6813 register_input_globals     -> Key table
//   (no ExecuteInGameThread host-side: that global is deferred-only and
//    main.lua never touches it during load)
void install_stub_surface(lua_State* L)
{
    // :6804 / :1551 — print -> global_print (fixture print pipeline)
    lua_register(L, "print", fixture_global_print);
    // :1945 — RegisterKeyBind
    lua_register(L, "RegisterKeyBind", stub_RegisterKeyBind);
    // :2741 — RegisterLoadMapPreHook
    lua_register(L, "RegisterLoadMapPreHook", stub_RegisterLoadMapPreHook);
    // :6813 / :583... — Key table (only the six entries main.lua's load path
    // touches; the real table has ~120 pairs, LuaMod.cpp:476-644)
    lua_createtable(L, 0, 6);
    lua_pushinteger(L, KEY_F7);
    lua_setfield(L, -2, "F7");
    lua_pushinteger(L, KEY_UP_ARROW);
    lua_setfield(L, -2, "UP_ARROW");
    lua_pushinteger(L, KEY_DOWN_ARROW);
    lua_setfield(L, -2, "DOWN_ARROW");
    lua_pushinteger(L, KEY_RETURN);
    lua_setfield(L, -2, "RETURN");
    lua_pushinteger(L, KEY_LEFT_ARROW);
    lua_setfield(L, -2, "LEFT_ARROW");
    lua_pushinteger(L, KEY_RIGHT_ARROW);
    lua_setfield(L, -2, "RIGHT_ARROW");
    lua_setglobal(L, "Key");
}

// MIRROR of LuaMod::load_and_execute_script (staged LuaMod.cpp:6874-6945),
// native-source branch: :6880-6881 read, :6909 chunkname "@" + path,
// :6911-6913 native_lua_chunk, :6918-6925 lua_pcall(0, 0, handler).
// GLUE: the error handler is the marked fixture stand-in, not
// LuaMadeSimple::push_pcall_error_handler.
bool load_and_execute_main(lua_State* L,
                           const Platform::NativeModSource& source,
                           const std::string& main_key, std::string& error)
{
    const auto buffer = source.read(main_key);
    if (!buffer)
    {
        error = "m_native_source->read returned no buffer for " + main_key;
        return false;
    }
    const std::string chunk_name = "@" + main_key;
    if (int status = native_lua_chunk(L, buffer->bytes, chunk_name.c_str()); status != LUA_OK)
    {
        const char* msg = lua_tostring(L, -1);
        error = std::string("load status ") + std::to_string(status) + ": " +
                (msg ? msg : "(no message)");
        lua_pop(L, 1);
        return false;
    }
    lua_pushcfunction(L, fixture_error_handler);  // [chunk, handler]
    lua_insert(L, 1);                             // [handler, chunk]
    if (int status = lua_pcall(L, 0, 0, 1); status != LUA_OK)
    {
        error = "pcall status " + std::to_string(status) + ": " + g_last_lua_error;
        lua_pop(L, 2);  // error message + handler
        return false;
    }
    lua_remove(L, 1);  // handler
    return true;
}

}  // namespace

int run(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: banner_smoke_fixture <bundle-dir> <results-dir>\n";
        return 2;
    }
    const fs::path bundle = fs::absolute(argv[1]);
    const fs::path results = fs::absolute(argv[2]);

    std::cout << "banner_smoke_fixture: bundle=" << bundle.string() << "\n";

    const std::string main_key = "ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua";
    const std::string main_mod_path = "ue4ss/Mods/TownfallItemSpawner";
    const std::string main_scripts_path = "ue4ss/Mods/TownfallItemSpawner/Scripts";

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

    Platform::NativeModSource source(std::move(files));

    // --- Source bytes == staged main.lua (21,415 B, sha f586043a...) ---
    run_case("source_main_lua_bytes", [&]() -> std::string {
        const auto buffer = source.read(main_key);
        if (!buffer) return std::string("FAIL source.read miss for ") + main_key;
        const std::string staged = read_file(
            bundle / fs::path("ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua"));
        if (buffer->bytes.size() != 21415)
            return "FAIL native buffer size " + std::to_string(buffer->bytes.size()) +
                   " expected 21415 (task-staged size)";
        if (buffer->bytes.size() != staged.size())
            return "FAIL staged size drift: " + std::to_string(staged.size());
        if (std::memcmp(buffer->bytes.data(), staged.data(), staged.size()) != 0)
            return std::string("FAIL native bytes differ from staged main.lua");
        return "source.read -> " + std::to_string(buffer->bytes.size()) +
               " B == staged main.lua (memcmp clean; external sha f586043a... checked by build script)";
    });

    // --- LuaMod ctor native branch context (staged LuaMod.cpp:934-946 mirror,
    //     same glue as worker-require-path) ---
    NativeModCtx ctx;
    ctx.native_source = &source;
    ctx.m_mod_path = std::filesystem::u8path(main_mod_path);
    const auto& mods = source.lua_mods();
    const auto found = std::find_if(mods.begin(), mods.end(),
        [&](const auto& mod) { return mod.path == main_mod_path; });
    if (found == mods.end()) throw std::runtime_error("mod row missing from NativeModSource");
    ctx.m_scripts_path = std::filesystem::u8path(found->scripts_path);
    if (modbridge::normalize_path_for_lua(ctx.m_scripts_path) != main_scripts_path)
        throw std::runtime_error("scripts_path round-trip drifted");

    // --- Case A (control): bare surface, no UE4SS globals at all.
    // Documents the deepest reachable point without stubs: main.lua load-time
    // path must stop at line 694. At :694 `RegisterKeyBind(Key.F7, ...)`,
    // loading the nil global RegisterKeyBind is not an error, but evaluating
    // the argument Key.F7 executes GETFIELD on a nil global first, so the
    // first failing operation is indexing global 'Key' (observed, and the
    // assertion below pins it). Expected: chunk loads, pcall fails, ZERO
    // prints, error attributed to main.lua:694.
    run_case("control_no_stubs_deepest_point", [&]() -> std::string {
        lua_State* L = luaL_newstate();
        if (!L) throw std::runtime_error("luaL_newstate failed");
        luaL_openlibs(L);
        install_native_searcher(L, ctx);
        g_output_sends.clear();
        g_last_lua_error.clear();
        std::string error;
        const bool ok = load_and_execute_main(L, source, main_key, error);
        const std::size_t sends = g_output_sends.size();
        lua_close(L);
        if (ok) return std::string("FAIL main.lua completed on a bare state; expected stop at :694");
        if (sends != 0)
            return "FAIL expected zero prints before the blocker, got " + std::to_string(sends);
        if (error.find("main.lua:694") == std::string::npos)
            return std::string("FAIL error not attributed to main.lua:694: ") + error;
        if (error.find("attempt to index a nil value") == std::string::npos ||
            error.find("'Key'") == std::string::npos)
            return std::string("FAIL unexpected error shape: ") + error;
        return "bare state stops at main.lua:694 attempt to index a nil value "
               "(global 'Key') — first unstubbed dependency hit; zero prints; "
               "banner NOT reachable without stubs";
    });

    // --- Case B: stubbed surface, target registration order, full banner chain.
    run_case("banner_smoke_stubbed_surface", [&]() -> std::string {
        lua_State* L = luaL_newstate();
        if (!L) throw std::runtime_error("luaL_newstate failed");
        luaL_openlibs(L);
        // Target order: globals first (:6804/:6813), then the native
        // require searcher (:6959), then main.lua (:6966).
        install_stub_surface(L);
        install_native_searcher(L, ctx);
        g_output_sends.clear();
        g_keybinds.clear();
        g_loadmap_prehook_called = false;
        g_loadmap_prehook_fn = false;
        g_last_lua_error.clear();

        std::string error;
        const bool ok = load_and_execute_main(L, source, main_key, error);
        if (!ok) return std::string("FAIL main.lua load/execute failed: ") + error;

        std::ostringstream out;
        out << "load+execute OK; ";

        // Exactly one print send: main.lua calls log() once (line 746).
        if (g_output_sends.size() != 1)
            return "FAIL expected exactly 1 print send, got " +
                   std::to_string(g_output_sends.size());
        const StringType& banner = g_output_sends[0];

        // Byte assertions on the FIXED to_generic_string output.
        const std::string expected_banner =
            "[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. "
            "F7 opens the menu.\n";
        const std::string bytes = to_log_bytes(banner);
        if (bytes.size() != expected_banner.size())
            return "FAIL banner byte length " + std::to_string(bytes.size()) + " expected " +
                   std::to_string(expected_banner.size()) + " (hex below)\n" + hex_dump(bytes);
        if (std::memcmp(bytes.data(), expected_banner.data(), bytes.size()) != 0)
            return std::string("FAIL banner bytes differ from expected (hex below)\n") +
                   hex_dump(bytes);

        std::size_t nul_units = 0;
        std::size_t non_ascii_units = 0;
        for (CharType unit : banner)
        {
            if (unit == 0) ++nul_units;
            if (unit > 0x7F) ++non_ascii_units;
        }
        if (nul_units != 0)
            return "FAIL banner contains " + std::to_string(nul_units) + " NUL code units";
        if (non_ascii_units != 0)
            return "FAIL banner contains " + std::to_string(non_ascii_units) +
                   " non-ASCII code units (unexpected for this ASCII literal)";
        if (bytes.find("Loaded v1.0") == std::string::npos)
            return std::string("FAIL substring 'Loaded v1.0' missing: ") + bytes;
        if (bytes.find("F7") == std::string::npos)
            return std::string("FAIL substring 'F7' missing: ") + bytes;
        if (bytes.rfind("[Lua] ", 0) != 0)
            return std::string("FAIL banner lacks [Lua] prefix: ") + bytes;

        out << "banner bytes exact (" << bytes.size() << " B, 0 NUL units, pure ASCII); "
            << "contains 'Loaded v1.0' + 'F7'; ";

        // Keybind registrations: six RegisterKeyBind calls with function
        // callbacks and the exact expected key codes.
        if (g_keybinds.size() != 6)
            return "FAIL expected 6 RegisterKeyBind calls, got " +
                   std::to_string(g_keybinds.size()) + "; " + out.str();
        const std::vector<uint32_t> expected_keys{
            KEY_F7, KEY_UP_ARROW, KEY_DOWN_ARROW, KEY_RETURN, KEY_LEFT_ARROW, KEY_RIGHT_ARROW};
        for (std::size_t i = 0; i < expected_keys.size(); ++i)
        {
            if (!g_keybinds[i].callback_is_function)
                return std::string("FAIL keybind #") + std::to_string(i) +
                       " callback is not a function; " + out.str();
        }
        std::vector<uint32_t> got_keys;
        for (const auto& kb : g_keybinds) got_keys.push_back(kb.key);
        std::sort(got_keys.begin(), got_keys.end());
        std::vector<uint32_t> want_keys(expected_keys);
        std::sort(want_keys.begin(), want_keys.end());
        if (got_keys != want_keys)
            return std::string("FAIL key codes mismatch; ") + out.str();
        out << "6 RegisterKeyBind(func) with key codes "
            << "{F7=0x76,UP=0x26,DOWN=0x28,RETURN=0x0D,LEFT=0x25,RIGHT=0x27}; ";

        // LoadMap prehook: registered with a function (real runtime accepts
        // and stores it; deferred-world behavior documented at
        // LuaMod.cpp:7443-7453 — accepted, never invoked there).
        if (!g_loadmap_prehook_called || !g_loadmap_prehook_fn)
            return std::string("FAIL RegisterLoadMapPreHook not called with a function; ") +
                   out.str();
        out << "RegisterLoadMapPreHook(func) x1";

        // Persist the captured banner bytes for external inspection.
        std::ofstream dump(results / "banner_capture.bin", std::ios::binary);
        dump.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        dump.close();

        return out.str();
    });

    // --- Emit JSON + markdown-friendly inputs ---
    const bool all_pass = g_failed == 0;
    {
        std::ostringstream json;
        json << "{\n  \"fixture\": \"worker-banner-smoke/banner_smoke_fixture\",\n";
        json << "  \"verdict\": \"" << (all_pass ? "PASS" : "FAIL") << "\",\n";
        json << "  \"main_lua\": {\"key\": \"" << json_escape(main_key)
             << "\", \"size\": 21415, \"sha256\": \"f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658\"},\n";
        json << "  \"expected_banner_bytes\": \""
             << json_escape("[Lua] [TownfallItemSpawner] Loaded v1.0. "
                            "Clean pickup-only release. F7 opens the menu.\n") << "\",\n";
        json << "  \"stubbed_globals\": [\"print\", \"RegisterKeyBind\", \"RegisterLoadMapPreHook\", \"Key\"],\n";
        json << "  \"not_stubbed_load_time_free\": [\"ExecuteInGameThread\", \"require\", "
               "\"UEHelpers\", \"FindFirstOf\", \"StaticFindObject\", \"StaticConstructObject\", "
               "\"FName\", \"FText\", \"LoadAsset\"],\n";
        json << "  \"cases\": [\n";
        for (std::size_t i = 0; i < g_cases.size(); ++i)
        {
            const auto& c = g_cases[i];
            json << "    {\"name\": \"" << json_escape(c.name) << "\", \"pass\": "
                 << (c.pass ? "true" : "false") << ", \"detail\": \""
                 << json_escape(c.detail) << "\"}"
                 << (i + 1 < g_cases.size() ? "," : "") << "\n";
        }
        json << "  ],\n  \"overall_pass\": " << (all_pass ? "true" : "false") << "\n}\n";
        std::ofstream out(results / "banner_smoke.json", std::ios::binary);
        out << json.str();
    }

    std::cout << (all_pass ? "OVERALL: PASS\n" : "OVERALL: FAIL\n");
    return all_pass ? 0 : 1;
}

}}  // namespace RC::fixture

int main(int argc, char** argv)
{
    return RC::fixture::run(argc, argv);
}
