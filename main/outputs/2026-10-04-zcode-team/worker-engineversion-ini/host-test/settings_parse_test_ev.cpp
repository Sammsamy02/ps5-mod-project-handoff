// Host verification: feeds UE4SS-settings.ini bytes through the vendored
// IniParser (PS5 string semantics: FORCE_U16) and replays the exact
// SettingsManager::deserialize reads + validate_native_services gates from
// native-coherent-runtime-1/source. Host-only; no engine, no console.
#include <IniParser/Ini.hpp>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using RC::Ini::Parser;
using Str = RC::File::StringType; // char16_t under FORCE_U16

static Str to_u16(const std::string& s)
{
    Str out;
    for (unsigned char c : s) out.push_back(static_cast<char16_t>(c));
    return out;
}
static std::string to_u8(const Str& s)
{
    std::string out;
    for (char16_t c : s) out.push_back(static_cast<char>(c));
    return out;
}
static bool iequal(const Str& a, const Str& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        auto lo = [](char16_t c) { return c >= U'A' && c <= U'Z' ? static_cast<char16_t>(c + 32) : c; };
        if (lo(a[i]) != lo(b[i])) return false;
    }
    return true;
}
static std::string slurp(const char* path)
{
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

struct Result { bool gates_ok; std::string detail; };

// Replays SettingsManager.cpp reads with SettingsManager.hpp defaults,
// then evaluates UE4SSProgram.cpp:719-727 validate_native_services throws.
static Result evaluate(const std::string& bytes)
{
    Result r{true, {}};
    if (bytes.find('\0') != std::string::npos) { r.gates_ok = false; r.detail += "NUL byte present; "; }
    Str contents = to_u16(bytes);
    Parser parser;
    try { parser.parse(contents); }
    catch (std::exception& e) { return Result{false, std::string("PARSE-THROW: ") + e.what()}; }

    auto get_bool = [&](const Str& sec, const Str& key, bool def) {
        try { return parser.get_bool(sec, key); } catch (...) { return def; }
    };
    auto get_int = [&](const Str& sec, const Str& key, int64_t def) {
        try { return parser.get_int64(sec, key); } catch (...) { return def; }
    };
    auto get_str = [&](const Str& sec, const Str& key, const Str& def) {
        try { return parser.get_string(sec, key); } catch (...) { return def; }
    };

    const Str O = to_u16("Overrides"), G = to_u16("General"), D = to_u16("Debug"),
              C = to_u16("CrashDump"), E = to_u16("EngineVersionOverride");
    // deserialize reads (defaults from SettingsManager.hpp:25-101)
    Str modsFolderPath = get_str(O, to_u16("ModsFolderPath"), Str{});
    Str controllingModsTxt = get_str(O, to_u16("ControllingModsTxt"), Str{});
    bool useUObjectArrayCache = get_bool(G, to_u16("bUseUObjectArrayCache"), true);
    int64_t secondsToScan = get_int(G, to_u16("SecondsToScanBeforeGivingUp"), 120);
    if (secondsToScan == 30) secondsToScan = 120; // SettingsManager.cpp:137-145
    Str execMethod = get_str(G, to_u16("DefaultExecuteInGameThreadMethod"), Str{});
    Str fnameMethod = get_str(G, to_u16("DefaultFNameToStringMethod"), Str{});
    bool simpleConsole = get_bool(D, to_u16("ConsoleEnabled"), true);
    bool guiConsoleEnabled = get_bool(D, to_u16("GuiConsoleEnabled"), true);
    bool guiConsoleVisible = get_bool(D, to_u16("GuiConsoleVisible"), true);
    Str renderMode = get_str(D, to_u16("RenderMode"), Str{});
    bool crashDump = get_bool(C, to_u16("EnableDumping"), true);
    int64_t majorOverride = get_int(E, to_u16("MajorVersion"), -1);
    int64_t minorOverride = get_int(E, to_u16("MinorVersion"), -1);
    // UE4SSProgram.cpp:869-884 admission replay: both must be != -1 AND
    // within [INT64_C(0), UINT32_MAX] or startup throws std::runtime_error
    // ("...must be in range of a uint32"). No-op version_finder installs
    // only when BOTH are set.
    const bool ev_both_set = (majorOverride != -1 && minorOverride != -1);
    const bool ev_in_range = ev_both_set &&
        majorOverride >= 0 && static_cast<uint64_t>(majorOverride) <= 0xFFFFFFFFull &&
        minorOverride >= 0 && static_cast<uint64_t>(minorOverride) <= 0xFFFFFFFFull;
    if (ev_both_set && !ev_in_range)
        r.gates_ok = false, r.detail += "THROW[injected: Was unable to override engine version from ini file; The number in the ini file must be in range of a uint32]; ";

    // SettingsManager.cpp:86 calls get_list("Overrides") unconditionally:
    try { auto l = parser.get_list(O); (void)l; }
    catch (std::exception& e) { r.gates_ok = false; r.detail += std::string("GET-LIST-THROW: ") + e.what() + "; "; }

    // validate_native_services (UE4SSProgram.cpp:721-727), in throw order:
    auto gate = [&](bool bad, const char* msg) {
        if (bad) { r.gates_ok = false; r.detail += std::string("THROW[") + msg + "]; "; }
    };
    gate(crashDump, "Native crash dumping service is unavailable");
    gate(simpleConsole, "Native simple console service is unavailable");
    gate(guiConsoleEnabled || guiConsoleVisible || !iequal(renderMode, to_u16("ExternalThread")),
         "Native debugging GUI service is unavailable");
    // General.EnableHotReloadSystem / EnableAutoReloadingLuaMods default false and are
    // not set by either variant -> hot-reload gate cannot fire; asserted here:
    gate(get_bool(G, to_u16("EnableHotReloadSystem"), false) ||
         get_bool(G, to_u16("EnableAutoReloadingLuaMods"), false),
         "Native hot reload and filesystem watcher services are unavailable");

    // Expected-value assertions for the canonical first-run config:
    auto expect = [&](bool ok, const std::string& what) {
        if (!ok) { r.gates_ok = false; r.detail += "EXPECT-FAIL[" + what + "]; "; }
    };
    expect(modsFolderPath.empty(), "Overrides.ModsFolderPath empty");
    expect(controllingModsTxt.empty(), "Overrides.ControllingModsTxt empty");
    expect(useUObjectArrayCache == false, "bUseUObjectArrayCache==false");
    expect(secondsToScan == 120, "SecondsToScanBeforeGivingUp==120");
    expect(iequal(execMethod, to_u16("EngineTick")), "DefaultExecuteInGameThreadMethod==EngineTick");
    expect(iequal(fnameMethod, to_u16("Scan")), "DefaultFNameToStringMethod==Scan");
    expect(simpleConsole == false, "ConsoleEnabled==0");
    expect(guiConsoleEnabled == false, "GuiConsoleEnabled==0");
    expect(guiConsoleVisible == false, "GuiConsoleVisible==0");
    expect(iequal(renderMode, to_u16("ExternalThread")), "RenderMode==ExternalThread");
    expect(crashDump == false, "CrashDump.EnableDumping==0");
    expect(majorOverride == 5, "EngineVersionOverride.MajorVersion==5");
    expect(minorOverride == 6, "EngineVersionOverride.MinorVersion==6");
    expect(ev_both_set && ev_in_range, "UP.cpp:869-884 admission: both set, uint32-range");
    if (r.detail.empty()) r.detail = "all gates + expected values OK";
    return r;
}

int main(int argc, char** argv)
{
    if (argc < 2) { std::cerr << "usage: settings_parse_test <ini> [probe-ini...]\n"; return 2; }
    int rc = 0;
    for (int i = 1; i < argc; ++i)
    {
        Result r = evaluate(slurp(argv[i]));
        std::cout << argv[i] << " -> " << (r.gates_ok ? "PASS" : "FAIL") << " (" << r.detail << ")\n";
        if (i == 1 && !r.gates_ok) rc = 1; // only argv[1] (canonical) must pass
    }
    return rc;
}
