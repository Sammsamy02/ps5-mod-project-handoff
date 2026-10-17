// [LFB] host fixture (agent-EY 2026-10-09, Claude item 5): the REAL
// converter — include/LuaFNameBridge.hpp, included VERBATIM — under
// -fshort-wchar (wchar_t == 2 bytes, the Prospero shape) with the REAL Lua
// 5.5 interpreter. Claude's exact spec:
//
//   "Host test with the REAL converter code: 'PlayerController' must arrive
//    as 16 UTF-16 code units at the stub ctor. Compile with -fshort-wchar."
//
// What is REAL here (vs stubbed):
//   REAL  Lua 5.5 (luaL_newstate/luaL_dostring/lua_pushlstring/lua_tolstring).
//   REAL  include/LuaFNameBridge.hpp — the ENTIRE production converter:
//         the hand UTF-8->UTF-16 loop, the hand UTF-16->UTF-8 loop, the
//         Bridge (stack + memory sources, consumption parity), to_fname's
//         direct slot-2 ctor call, the [LUA-FN] first-20-uses logging with
//         the pool-decode OK/MISMATCH verdict, the [LFB] composition marker,
//         and the reverse push_fname_to_lua/push_wide_to_lua.
//   REAL  <String/StringType.hpp> (deps) — CharType = wchar_t == 2 under
//         -fshort-wchar, exactly the target width.
//   STUB  <Unreal/NameTypes.hpp>/<DynamicOutput/Output.hpp> (tests/lfb-stubs
//         shadow them): the FName surface + a recording slot-2 ctor + a
//         pool-decoder stub that decodes the RECORDED units back (the
//         round-trip verdict is therefore real, not hardcoded).
//
// Suites (argv): green (default) | mismatch (stub decoder lies by one 'X').
// The green suite includes the refusal battery (embedded NUL / overlong /
// invalid UTF-8) and the 20-use log bound.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <lua.hpp>

// ---- stub universe FIRST (shadows), then the REAL converter header ----
#include <Unreal/NameTypes.hpp> // fixture stub (records ctor inputs)
#include <DynamicOutput/Output.hpp> // fixture stub (captures send lines)
#include <LuaFNameBridge.hpp> // THE CODE UNDER TEST (verbatim, tree include/)

static_assert(sizeof(RC::CharType) == 2, "this fixture must run under -fshort-wchar (Claude item 5)");

using RC::CharType;
using RC::StringType;

// =====================================================================================
// Stub out-of-line pieces: the recording slot-2 ctor + seam notices.
// =====================================================================================
namespace
{
    struct CtorRecord
    {
        int last_mode = -1;
        std::vector<uint16_t> last_units{};
        int calls = 0;
    };
    CtorRecord g_ctor;

    std::vector<std::string> g_seam_notices;
} // namespace

namespace RC::Unreal
{
    FName::Fn FName::ConstructorInternal{};

    FName FName::Fn::operator()(const RC::CharType* units, EFindName mode) const
    {
        g_ctor.last_mode = static_cast<int>(mode);
        g_ctor.last_units.clear();
        for (size_t i = 0; units[i] != 0 && i < 1024; ++i)
        {
            g_ctor.last_units.push_back(static_cast<uint16_t>(units[i]));
        }
        ++g_ctor.calls;
        FName out;
        const uint32_t id = g_ctor.last_units.empty() ? 0u : static_cast<uint32_t>(g_ctor.last_units.size());
        out.ComparisonIndex = FNameEntryId::FromUnstableInt(id);
        // Record the exact units in the stub pool so FNamePoolDecoder::
        // try_tostring (used by the bridge's [LUA-FN] verdict) decodes the
        // REAL ctor input back. resize + [] only (see stub header: even the
        // (ptr,count) ctor overreads under -fshort-wchar on the host).
        {
            auto& pool = FNamePoolDecoder::lfb_pool();
            std::lock_guard<std::mutex> lock(pool.mutex);
            if (id >= pool.by_id.size()) { pool.by_id.resize(id + 1); }
            StringType recorded;
            recorded.resize(g_ctor.last_units.size());
            for (size_t i = 0; i < g_ctor.last_units.size(); ++i) { recorded[i] = units[i]; }
            pool.by_id[id] = std::move(recorded);
        }
        return out;
    }

    StringType FName::ToString() const
    {
        StringType out;
        if (FNamePoolDecoder::try_tostring(*this, out)) { return out; }
        return FNamePoolDecoder::lfb_literal(STR("UE4SS_None"), 11);
    }

    void NotifyFNameCtorUnreadyOnce() noexcept
    {
        g_seam_notices.emplace_back("[FN] ctor unready");
    }

    void NotifyFNameSeamRefusedOnce(const char* Reason, uint32 Length, uint32 At) noexcept
    {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "[FN] seam refused: reason='%s' len=%u at=%u", Reason ? Reason : "?", Length, At);
        g_seam_notices.emplace_back(buf);
    }
} // namespace RC::Unreal

// =====================================================================================
// Helpers
// =====================================================================================
namespace
{
    auto fail(const char* what) -> int
    {
        std::printf("lua-fname-bridge: FAIL (%s)\n", what);
        return 1;
    }

    auto log_count_with(const char* narrow_needle) -> int
    {
        const std::string needle{narrow_needle};
        int n = 0;
        for (const auto& line : RC::Output::g_lfb_log)
        {
            // hand UTF-16 -> narrow for the needle search (ASCII needles)
            std::string narrow;
            narrow.reserve(line.size());
            for (CharType c : line)
            {
                const auto v = static_cast<uint32_t>(c);
                narrow.push_back(v <= 0x7F ? static_cast<char>(v) : '?');
            }
            if (narrow.find(needle) != std::string::npos) { ++n; }
        }
        return n;
    }

    auto top(lua_State* L) -> int { return lua_gettop(L); }
} // namespace

int main(int argc, char** argv)
{
    const bool mismatch_mode = (argc > 1 && std::strcmp(argv[1], "mismatch") == 0);
    if (mismatch_mode)
    {
        RC::Unreal::FNamePoolDecoder::lfb_pool().lie = true;
    }

    lua_State* L = luaL_newstate();
    if (!L) { return fail("no lua state"); }

    // ===== [1] Claude item 5: REAL Lua string -> REAL bridge -> stub ctor;
    //           'PlayerController' must arrive as 16 UTF-16 code units. =====
    if (luaL_dostring(L, "return 'PlayerController'") != LUA_OK || !lua_isstring(L, -1))
    {
        return fail("lua chunk");
    }
    {
        const RC::LuaType::LuaFNameBridge::Bridge name{L};
        if (!name.ok()) { return fail("bridge refused a valid ASCII name"); }
        if (name.unit_count() != 16) { return fail("unit count != 16"); }
        if (name.input_len() != 16) { return fail("input byte len != 16"); }
        if (name.units()[16] != CharType(0)) { return fail("buffer not NUL-terminated"); }
        static const char16_t kExpect[] = u"PlayerController";
        for (size_t i = 0; i < 16; ++i)
        {
            if (static_cast<uint16_t>(name.units()[i]) != static_cast<uint16_t>(kExpect[i]))
            {
                return fail("unit mismatch vs u\"PlayerController\"");
            }
        }
        // to_fname: slot-2 ctor DIRECT — the stub ctor receives the 16 units.
        g_ctor.last_units.clear();
        const auto made = name.to_fname(RC::Unreal::FNAME_Find);
        if (g_ctor.last_units.size() != 16) { return fail("stub ctor received != 16 units"); }
        if (g_ctor.last_mode != 0) { return fail("ctor mode != FNAME_Find"); }
        for (size_t i = 0; i < 16; ++i)
        {
            if (g_ctor.last_units[i] != static_cast<uint16_t>(kExpect[i])) { return fail("ctor unit content mismatch"); }
        }
        if (made.IsNone()) { return fail("constructed NAME_None"); }
        if (made.GetComparisonIndex().ToUnstableInt() != 16) { return fail("synthetic id != 16"); }
    }

    // ===== [2] [LUA-FN] log line: in/len/id/decoded/OK (or MISMATCH mode) =====
    {
        const int lines = log_count_with("[LUA-FN] in='PlayerController' len=16 id=0x10 decoded='PlayerController'");
        if (mismatch_mode)
        {
            // lying decoder: decoded='PlayerControllerX' + MISMATCH
            const int mism = log_count_with("decoded='PlayerControllerX' MISMATCH");
            if (mism < 1) { return fail("mismatch mode: no MISMATCH line"); }
        }
        else if (lines < 1) { return fail("no OK [LUA-FN] line for PlayerController"); }
    }

    // ===== [3] Consumption parity: Bridge removes the string exactly like
    //           LuaMadeSimple::Lua::get_string; the next arg slides to 1. =====
    {
        lua_settop(L, 0);
        lua_pushstring(L, "PlayerController");
        lua_pushinteger(L, 7);
        { const RC::LuaType::LuaFNameBridge::Bridge name{L}; (void)name; }
        if (top(L) != 1 || !lua_isinteger(L, 1) || static_cast<int>(lua_tointeger(L, 1)) != 7)
        {
            return fail("consumption parity (string not removed / integer shifted)");
        }
    }

    // ===== [4] Multi-byte UTF-8: 'café' (U+00E9 as 0xC3 0xA9) + astral U+1D11E. =====
    {
        lua_settop(L, 0);
        if (luaL_dostring(L, "return 'caf\\195\\169'") != LUA_OK || !lua_isstring(L, -1)) { return fail("cafe chunk"); }
        const RC::LuaType::LuaFNameBridge::Bridge cafe{L};
        if (!cafe.ok() || cafe.unit_count() != 4 || static_cast<uint16_t>(cafe.units()[3]) != 0xE9)
        {
            return fail("cafe 2-byte UTF-8 decode");
        }
        lua_settop(L, 0);
        // U+1D11E MUSICAL SYMBOL G CLEF: UTF-8 F0 9D 84 9E -> surrogate pair D834 DD1E
        lua_pushlstring(L, "\xF0\x9D\x84\x9E", 4);
        const RC::LuaType::LuaFNameBridge::Bridge clef{L};
        if (!clef.ok() || clef.unit_count() != 2 || static_cast<uint16_t>(clef.units()[0]) != 0xD834 || static_cast<uint16_t>(clef.units()[1]) != 0xDD1E)
        {
            return fail("astral surrogate pair");
        }
    }

    // ===== [5] Reverse direction: pool decode -> hand UTF-16 -> UTF-8 ->
    //           lua_pushlstring (Claude item 4). =====
    {
        lua_settop(L, 0);
        // An FName whose stub pool entry is 'PlayerController' (recorded in [1]).
        RC::Unreal::FName made{};
        made.ComparisonIndex = RC::Unreal::FNameEntryId::FromUnstableInt(16);
        RC::LuaType::LuaFNameBridge::push_fname_to_lua(L, made);
        size_t len = 0;
        const char* s = lua_tolstring(L, -1, &len);
        if (!s || len != 16 || std::string_view{s, len} != std::string_view{"PlayerController"})
        {
            // mismatch mode: the lying decoder appends 'X' — accept exactly that.
            if (!mismatch_mode || !s || len != 17 || std::string_view{s, len} != std::string_view{"PlayerControllerX"})
            {
                return fail("push_fname_to_lua round trip");
            }
        }
        // Wide -> UTF-8 with a surrogate pair.
        lua_settop(L, 0);
        const CharType clef_units[] = {CharType(0xD834), CharType(0xDD1E)};
        RC::LuaType::LuaFNameBridge::push_wide_to_lua(L, clef_units, 2);
        s = lua_tolstring(L, -1, &len);
        if (!s || len != 4 || static_cast<unsigned char>(s[0]) != 0xF0 || static_cast<unsigned char>(s[1]) != 0x9D ||
            static_cast<unsigned char>(s[2]) != 0x84 || static_cast<unsigned char>(s[3]) != 0x9E)
        {
            return fail("push_wide_to_lua surrogate re-encode");
        }
    }

    // ===== [6] Refusals (seam-parity, non-throwing): embedded NUL,
    //           overlong, invalid UTF-8. NAME_None, ctor NOT called. =====
    {
        const int ctor_before = g_ctor.calls;
        lua_settop(L, 0);
        lua_pushlstring(L, "Pl\0yer", 6);
        {
            const RC::LuaType::LuaFNameBridge::Bridge bad{L};
            if (bad.ok()) { return fail("embedded NUL not refused"); }
            if (!bad.to_fname(RC::Unreal::FNAME_Find).IsNone()) { return fail("embedded NUL constructed a name"); }
        }
        lua_settop(L, 0);
        std::string big(2000, 'x');
        lua_pushlstring(L, big.data(), big.size());
        {
            const RC::LuaType::LuaFNameBridge::Bridge overlong{L};
            if (overlong.ok()) { return fail("overlong not refused"); }
            if (!overlong.to_fname(RC::Unreal::FNAME_Add).IsNone()) { return fail("overlong constructed a name"); }
        }
        lua_settop(L, 0);
        lua_pushlstring(L, "\xC3\x28", 2);
        {
            const RC::LuaType::LuaFNameBridge::Bridge invalid{L};
            if (invalid.ok()) { return fail("invalid UTF-8 not refused"); }
        }
        if (g_ctor.calls != ctor_before) { return fail("a refused input reached the ctor"); }
        bool saw_embedded = false;
        bool saw_overlong = false;
        for (const auto& notice : g_seam_notices)
        {
            if (notice.find("embedded-nul") != std::string::npos) { saw_embedded = true; }
            if (notice.find("overlong") != std::string::npos) { saw_overlong = true; }
        }
        if (!saw_embedded) { return fail("no embedded-nul seam notice"); }
        if (!saw_overlong) { return fail("no overlong seam notice"); }
        // A refused string is still CONSUMED (removed) — parity again.
        if (top(L) != 0) { return fail("refused string not consumed"); }
    }

    // ===== [7] Memory-source constructor (the FindObject binding shape). =====
    {
        const RC::LuaType::LuaFNameBridge::Bridge mem{"PlayerController", 16};
        if (!mem.ok() || mem.unit_count() != 16) { return fail("memory-source bridge"); }
        g_ctor.last_units.clear();
        const auto made = mem.to_fname(RC::Unreal::FNAME_Add);
        if (g_ctor.last_units.size() != 16 || g_ctor.last_mode != 1 || made.IsNone()) { return fail("memory-source to_fname"); }
    }

    // ===== [8] First-20-uses log bound (Claude item 3). =====
    {
        const int before = log_count_with("[LUA-FN]");
        for (int i = 0; i < 25; ++i)
        {
            const RC::LuaType::LuaFNameBridge::Bridge rep{"ItemDatabaseSubsystem", 21};
            (void)rep.to_fname(RC::Unreal::FNAME_Find);
        }
        const int after = log_count_with("[LUA-FN]");
        if (after - before > 20) { return fail("more than 20 logged uses"); }
        if (after < 1) { return fail("no logged uses at all"); }
    }

    // ===== [9] [LFB] composition marker: exactly once per process. =====
    if (log_count_with("[LFB] lua-fname-bridge composed") != 1)
    {
        return fail("[LFB] composition marker not exactly once");
    }

    lua_close(L);
    std::printf(mismatch_mode ? "[ST] lua-fname-bridge mismatch-mode PASS (MISMATCH verdict fired)\n"
                              : "[ST] lua-fname PlayerController PASS (16 code units at the stub ctor)\n");
    std::printf("lua-fname-bridge %s: ALL PASS\n", mismatch_mode ? "mismatch" : "green");
    return 0;
}
