// worker-lua-api-conversion — host route fixture (worker-require-path extraction pattern).
//
// Proves the conversion ROUTES that the 36 audited Lua-API-family sites in
// LuaMod.cpp (c92c92ff) take, using the byte-exact staged headers (symlinks,
// unmodified). One disclosed SHIM: String/StringType.hpp pins the 2-byte
// CharType host profile (see shim header).
//
// Routes under test (per-site mapping in SITE-TABLE.md):
//   R1  RC::ensure_str(narrow)   — sites 1579..6783 (narrow Lua string -> engine wide)
//   R2  RC::to_wstring(narrow)   — site 3145 (RC::to_wstring(path_str)) AND the leaf
//                                  of the PROSPERO ensure_str dispatch (to_charT_string<wchar_t>
//                                  -> to_wstring -> to_wstring(const std::string&) -> DecodeUtf8)
//   R3  RC::to_string(wide)      — sites 7750..8205 (engine wide command -> Lua narrow)
//   R4  RC::to_generic_string(narrow) — print pipeline; staged c76be502 __PROSPERO__
//                                  byte-widen branch and default ensure_str branch
//   N1  negative: invalid UTF-8 -> DecodeUtf8 throws (strictness note, not a garble)
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <Helpers/String.hpp>

static int g_fail = 0;

// Generic unit-vs-byte comparator: accepts any wide element type (char16_t units
// from ensure_str / wchar_t units from to_wstring) and compares VALUE against the
// narrow reference bytes. For ASCII every unit value must equal the byte value.
template <typename Wide>
static auto wide_bytes_equal(const Wide& wide, const char* narrow) -> bool
{
    if (wide.size() != std::strlen(narrow)) return false;
    for (size_t i = 0; i < wide.size(); ++i)
    {
        if (static_cast<unsigned char>(wide[i]) != static_cast<unsigned char>(narrow[i])) return false;
    }
    return true;
}

static auto check(const char* name, bool ok) -> void
{
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++g_fail;
}

int main()
{
    // ---- R2: RC::to_wstring — site 3145 route (iterate_directory __absolute_path)
    //     and the leaf of the PROSPERO ensure_str(narrow) dispatch.
    {
        const char* object_full_name = "/Game/Maps/Townfall/Townfall_P.Townfall_P:PersistentLevel.BP_TownCharacter_C_42";
        auto w = RC::to_wstring(std::string{object_full_name});
        check("R2 to_wstring(string) FindObject full-name arg byte-exact", wide_bytes_equal(w, object_full_name));
        auto wv = RC::to_wstring(std::string_view{object_full_name});
        check("R2 to_wstring(string_view) byte-exact", wide_bytes_equal(wv, object_full_name));

        // RegisterHook / UnregisterHook UFunction path class
        const char* ufunction_path = "Function /Script/Engine.GameplayStatics:ReceiveBeginPlay";
        check("R2 to_wstring RegisterHook UFunction path byte-exact",
              wide_bytes_equal(RC::to_wstring(std::string{ufunction_path}), ufunction_path));

        // LoadAsset / FName(FNAME_Add) asset path class
        const char* asset_path = "/Game/Items/TownfallItemSpawner/BP_TownItemSpawner.BP_TownItemSpawner_C";
        check("R2 to_wstring LoadAsset asset path byte-exact",
              wide_bytes_equal(RC::to_wstring(std::string{asset_path}), asset_path));

        // Site 3145's actual argument class: native mod scripts path
        const char* scripts_path = "ue4ss/Mods/TownfallItemSpawner/Scripts";
        check("R2 to_wstring iterate_directory path byte-exact",
              wide_bytes_equal(RC::to_wstring(std::string{scripts_path}), scripts_path));
    }

    // ---- R1: RC::ensure_str(narrow) -> wide. Under the fixture's 2-byte char16_t
    //     profile the dispatch is to_charT_string<char16_t> -> to_u16string(string_view)
    //     (element-wise byte copy). On PROSPERO (wchar_t) it is to_charT_string<wchar_t>
    //     -> to_wstring(string_view) -> R2's DecodeUtf8 leaf; both 2-byte routes carry
    //     identical code-unit values for ASCII (documented in SITE-TABLE.md).
    {
        const char* short_name = "BP_TownItemSpawner_C";
        auto ensured = RC::ensure_str(std::string_view{short_name});
        check("R1 ensure_str(string_view) FindObject short-name arg byte-exact", wide_bytes_equal(ensured, short_name));

        const char* event_name = "MyMod_CustomEvent";
        check("R1 ensure_str RegisterCustomEvent name byte-exact",
              wide_bytes_equal(RC::ensure_str(std::string_view{event_name}), event_name));

        const char* console_cmd_name = "Gold";
        check("R1 ensure_str RegisterConsoleCommandHandler key byte-exact",
              wide_bytes_equal(RC::ensure_str(std::string_view{console_cmd_name}), console_cmd_name));

        const char* fstring_value = "Hello from Lua 100";
        auto fstr = RC::ensure_str(std::string{fstring_value}); // site 6715: ensure_str(std::string(str))
        check("R1 ensure_str(string) FString ctor value byte-exact", wide_bytes_equal(fstr, fstring_value));
    }

    // ---- R3: RC::to_string(wide) -> narrow UTF-8. Console-exec passthrough routes
    //     (set_string(to_string(cmd/str/command/command_part)), luaL_loadstring source).
    {
        std::wstring cmd = L"Gold 100";
        auto n = RC::to_string(cmd);
        check("R3 to_string(wstring) console command byte-exact", n == "Gold 100");

        std::wstring_view part = L"100";
        check("R3 to_string(wstring_view) param part byte-exact (garble would yield \"10\")",
              RC::to_string(part) == "100");

        std::wstring lua_src = L"local p = FindObject(\"BP_TownCharacter_C\")";
        check("R3 to_string console-Lua source byte-exact (site 7991 luaL_loadstring)",
              RC::to_string(lua_src) ==
                  "local p = FindObject(\"BP_TownCharacter_C\")");

        // const wchar_t* overload (cmd arrives as Unreal::TCHAR* == wchar_t*)
        check("R3 to_string(const wchar_t*) byte-exact", RC::to_string(L"SummonEnemy Boss1") == "SummonEnemy Boss1");
    }

    // ---- Round trip: to_string(ensure_str(x)) == x (parity-shift guard from the audit)
    {
        const char* s = "TownfallItemSpawner 1.0.0";
        auto rt = RC::to_string(RC::ensure_str(std::string{s}));
        check("Round-trip to_string(ensure_str(x)) == x", rt == s);
    }

    // ---- R4: to_generic_string narrow branch (print pipeline)
    {
        const char* banner = "TownfallItemSpawner 1.0.0";
        auto gs = RC::to_generic_string(banner);
        check("R4 to_generic_string(const char*) byte-exact", wide_bytes_equal(gs, banner));
        std::string narrow{banner};
        auto gs2 = RC::to_generic_string(narrow);
        check("R4 to_generic_string(std::string) byte-exact", wide_bytes_equal(gs2, banner));
    }

    // ---- N1: strictness note — invalid UTF-8 now throws instead of silently garbling
    {
        bool threw = false;
        try
        {
            (void)RC::to_wstring(std::string{"\xFF\xFE bad"});
        }
        catch (const std::exception&)
        {
            threw = true;
        }
        check("N1 invalid UTF-8 input throws (documented strictness, no silent garble)", threw);
    }

    std::printf(g_fail == 0 ? "OVERALL: PASS (%d failures)\n" : "OVERALL: FAIL (%d failures)\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
