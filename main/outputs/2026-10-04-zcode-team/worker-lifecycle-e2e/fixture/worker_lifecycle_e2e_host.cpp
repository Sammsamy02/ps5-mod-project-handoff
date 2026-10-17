// worker_lifecycle_e2e_host.cpp — worker-lifecycle-e2e ONE host regression for
// the PS5 UE4SS runtime port native mod lifecycle conversion CHAIN.
//
// Compile-time inputs are BYTE-EXACT line-range extractions of all three fix
// units (the driver slices them from sha256-pinned staged files before
// compiling; see extraction/provenance.json emitted by the driver):
//
//   (1) program unit  worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp
//       sha256 cb6cb14aa044124f17cf8d2115b4962a7d91daa686f6e80a0631061b51b8c8af
//         - widen_ascii_native_key (verbatim lines 1807-1822)  -> prog_widen.inc
//         - setup_mods PROSPERO loop (verbatim lines 1835-1843) -> prog_setup_loop.inc
//         - start_mods<LuaMod> PROSPERO branch (verbatim 2043-2051) -> prog_start_mods.inc
//         - find_mod_by_name_internal comparator (verbatim 3056-3063) -> prog_comparator.inc
//   (2) LuaMod unit   worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp
//       sha256 c92c92ff66686bf6f44e18aaec7d36b227d32fc75726f33349f4bdc7412ef3c0
//         - modbridge::normalize_path_for_lua (verbatim lines 42-54) -> luamod_normalize.inc
//         - LuaMod ctor native branch (verbatim lines 934-947) -> luamod_ctor.inc
//   (3) print pipeline worker-iniparser-repro/staged/Helpers/String.hpp
//       sha256 c76be502fbe6f9c2c6589f141c1caded84af482a1f03a428005574850ca47788
//         - to_generic_string PROSPERO byte-widen (verbatim lines 595-636) -> string_to_generic.inc
//
// Runtime inputs are the REAL bundle bytes (worker-itemspawner-bundle/bundle)
// and the REAL production NativeModSource (verbatim-compiled
// worker-preflight-runner/staging-lua/NativeModSource.cpp, composition-pinned
// sha256 2db163bc7feead56badc12ad235711576b837e1f5a05dba04a4d8fb03e53524f;
// header 2b44df14854934470a16fec6ef153501e08a8554ba51748b335fd0733856a450).
//
// Chain driven (host equivalent, per stage):
//   S1 loadlist rows -> NativeModSource (lua_mods + startup_mod_names via a
//      mods.txt `TownfallItemSpawner : 1` row + the staged enabled.txt marker)
//   S2 setup_mods-equivalent: widen_ascii_native_key(name/path) -> LuaMod-ctor
//      lookup via modbridge::normalize_path_for_lua -> found, scripts_path derived
//   S3 start_mods-equivalent: widen_ascii_native_key(name) against the STORED
//      widened name -> match -> start_mod  [the P0 regression; control proves
//      the simulated OLD ensure_str garble would NOT match]
//   S4 main.lua read via the source (bytes == staged sha f586043a...) and the
//      banner line through the FIXED to_generic_string (clean wide, no NULs)
//   S5 determinism is driver-side (run twice, diff) — this binary is
//      deterministic by construction (no clock/env in outputs).
//
// Fixture shims (the ONLY non-verbatim code around the extractions):
//   - LuaMod here is a fixture shell with the SAME members the verbatim ctor
//     block touches (m_native_source, m_mod_path, m_scripts_path,
//     set_installable) plus get_name/is_installable/is_started/start_mod used
//     by the verbatim start branch and comparator.
//   - UE4SSProgram is a fixture namespace providing IsInstalled and the
//     find_mod_by_name template so verbatim lines 2049 and 1841 compile
//     unchanged; its comparator body is the verbatim 3056-3063 extraction.
//   - Output::send / ensure_str / STR exist only so the verbatim ctor error
//     path (line 941) compiles; that path is NOT on the passing chain.
//   - old_ensure_str_simulated reproduces the documented pid-700 NUL-interleave
//     ('T','\0','o','\0',...) mechanism as the OLD-behavior control. It is a
//     simulation of the proven on-target wstring_convert breakage (host libc++
//     does not reproduce the PS5 codecvt stride bug); controls only assert that
//     the garbled wide key does not match the stored clean wide key.
//
// Target parity is BY VALUE (worker-preflight-runner precedent): host wchar_t
// may be 4-byte; every extracted conversion pushes unit values 0x00..0xFF only,
// so produced unit VALUES equal the target's. (-fshort-wchar was measured to
// break host libc++ std::wstring ABI — L"Townfall" gave size()==37 — so it is
// deliberately NOT used.)
//
// Freestanding host binary:
//   clang++ -std=c++20 -fshort-wchar -Wall -Wextra \
//     -I extraction -I <worker-preflight-runner>/staging-lua \
//     fixture/worker_lifecycle_e2e_host.cpp <staging-lua>/NativeModSource.cpp
// argv: [1] bundle root, [2] receipt, [3] json, [4] stable.
// Exit 0 = all checks pass.

#include "Platform/NativeModSource.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Target-parity typedefs (worker-preflight-runner discipline: plain host
// wchar_t, value-level parity; see header note on -fshort-wchar).
// StringViewType is not present in the trimmed port include tree; it is defined
// here at its value semantics (a view of StringType), consistent with
// UE4SSProgram.hpp find_mod_by_name_internal(StringViewType ...) comparing
// directly against elem->get_name() (const StringType&).
namespace File
{
    using CharType = wchar_t;
}
using StringType = std::wstring;
using StringViewType = std::wstring_view;

// Non-extracted shims required by verbatim line 941 (ctor error path, never on
// the passing chain).
#define STR(x) x
namespace Output
{
    enum class LogLevel
    {
        Error
    };
    template <LogLevel L, typename... A>
    auto send(A&&...) -> void
    {
    }
}
// Error-path-only shim: the OLD ensure_str garble is simulated separately for
// controls (old_ensure_str_simulated below); this shim is a passthrough because
// its argument is already the (possibly garbled) StringType.
static auto ensure_str(const StringType& value) -> StringType
{
    return value;
}
// Alias shims matching the real units' name lookups: String.hpp exposes the
// bare `CharType` alias (of File::CharType), and LuaMod.cpp resolves
// `LogLevel` through its Output using-scope.
using CharType = File::CharType;
using Output::LogLevel;

// OLD-behavior control: the documented pid-700 NUL-interleave produced by the
// PS5-broken wstring_convert narrow->wide route ('T','\0','o','\0',...).
// Simulation, not a reproduction claim: see header comment.
static auto old_ensure_str_simulated(const std::string& narrow) -> StringType
{
    StringType wide;
    wide.reserve(narrow.size() * 2);
    for (const char c : narrow)
    {
        wide.push_back(static_cast<File::CharType>(static_cast<unsigned char>(c)));
        wide.push_back(static_cast<File::CharType>(0));
    }
    return wide;
}

// ---------------------------------------------------------------------------
// Extraction (3): print pipeline — String.hpp to_generic_string PROSPERO
// branch. __PROSPERO__ and RC_IS_ANSI are defined here (after all system
// includes) to compile the exact target branch of the verbatim function.
#define RC_IS_ANSI 0
#define __PROSPERO__ 1

// Extraction (1): program unit — the shared hoisted conversion.
#include "prog_widen.inc" // UE4SSProgram.cpp 1807-1822 (cb6cb14a), byte-exact

// Extraction (2): LuaMod unit — the qualified path normalization.
#include "luamod_normalize.inc" // LuaMod.cpp 42-54 (c92c92ff), byte-exact

// Extraction (3): print pipeline.
#include "string_to_generic.inc" // String.hpp 595-636 (c76be502), byte-exact

// ---------------------------------------------------------------------------
// Fixture program/mod shells. The only non-verbatim parts are the shells
// themselves; every conversion, lookup, and branch body comes from the .inc
// extractions quoted above.

class LuaMod;
class FixtureProgram;
static FixtureProgram* g_program = nullptr; // fixture-only indirection

namespace UE4SSProgram
{
    enum class IsInstalled
    {
        No,
        Yes
    };
    // Fixture equivalent of UE4SSProgram::find_mod_by_name<T> — same shape as
    // UE4SSProgram.hpp:378; the comparator body is the verbatim extraction.
    template <typename ModType>
    auto find_mod_by_name(StringViewType mod_name, IsInstalled is_installed) -> ModType*;
}

class FixtureProgram
{
public:
    struct NativeContext
    {
        std::shared_ptr<const RC::Platform::NativeModSource> mod_source;
    } m_native_context; // fixture shape of UE4SSProgram's m_native_context

    std::vector<std::unique_ptr<LuaMod>> m_mods;

    FixtureProgram() { g_program = this; }
    ~FixtureProgram();
    FixtureProgram(const FixtureProgram&) = delete;
    auto operator=(const FixtureProgram&) -> FixtureProgram& = delete;

    template <typename T>
    auto find_mod_by_name(const StringType& name) -> T*; // verbatim call at 1841

    template <typename ModType>
    auto start_mods() -> std::string; // wraps verbatim 2043-2051

    auto setup_mods_equivalent() -> void; // wraps verbatim 1835-1843
};

class LuaMod
{
public:
    StringType m_mod_name;
    StringType m_mod_path;
    std::filesystem::path m_scripts_path;
    std::shared_ptr<const RC::Platform::NativeModSource> m_native_source;
    bool m_installable = true;
    bool m_started = false;
    int m_start_calls = 0;

    // Fixture equivalent of LuaMod::LuaMod(UE4SSProgram&, StringType&&,
    // StringType&&) (c92c92ff line 927): same member names the verbatim native
    // branch below touches; Mod base/m_lua state is out of scope here.
    LuaMod(FixtureProgram&, StringType&& mod_name, StringType&& mod_path)
        : m_mod_name(std::move(mod_name)), m_mod_path(std::move(mod_path)),
          m_native_source(g_program->m_native_context.mod_source)
    {
#include "luamod_ctor.inc" // LuaMod.cpp 934-947 (c92c92ff), byte-exact
    }

    auto get_name() const -> const StringType& { return m_mod_name; }
    auto is_installable() const -> bool { return m_installable; }
    auto is_started() const -> bool { return m_started; }
    auto set_installable(bool value) -> void { m_installable = value; }
    auto start_mod() -> void
    {
        m_started = true;
        ++m_start_calls;
    }
};

// Verbatim comparator (program unit): fixture equivalent of
// UE4SSProgram::find_mod_by_name_internal over m_mods.
template <typename ModType>
auto UE4SSProgram::find_mod_by_name(StringViewType mod_name, IsInstalled is_installed) -> ModType*
{
    auto mod_exists_with_name = std::find_if(g_program->m_mods.begin(), g_program->m_mods.end(), [&](auto& elem) -> bool {
        bool found = true;
#include "prog_comparator.inc" // UE4SSProgram.cpp 3056-3063 (cb6cb14a), byte-exact
        return found;
    });
    return mod_exists_with_name == g_program->m_mods.end() ? nullptr : mod_exists_with_name->get();
}

FixtureProgram::~FixtureProgram()
{
    if (g_program == this)
    {
        g_program = nullptr;
    }
}

template <typename T>
auto FixtureProgram::find_mod_by_name(const StringType& name) -> T*
{
    return UE4SSProgram::find_mod_by_name<T>(name, UE4SSProgram::IsInstalled::No);
}

// Extraction (1) call site: setup_mods PROSPERO native loop, verbatim.
auto FixtureProgram::setup_mods_equivalent() -> void
{
#include "prog_setup_loop.inc" // UE4SSProgram.cpp 1835-1843 (cb6cb14a), byte-exact
}

// Extraction (1) call site: start_mods PROSPERO native branch, verbatim.
// `source` mirrors UE4SSProgram.cpp:2041's `source` binding (fixture shape).
template <typename ModType>
auto FixtureProgram::start_mods() -> std::string
{
    const auto& source = m_native_context.mod_source;
#include "prog_start_mods.inc" // UE4SSProgram.cpp 2043-2051 (cb6cb14a), byte-exact
    return {};
}

// ---------------------------------------------------------------------------
// Harness

static int g_pass = 0, g_fail = 0;
static FILE* g_receipt = nullptr;
static std::string g_stable;

static auto report(bool ok, const std::string& name, const std::string& detail = "") -> void
{
    if (ok)
    {
        ++g_pass;
    }
    else
    {
        ++g_fail;
    }
    std::fprintf(g_receipt, "[%s] %s%s%s\n", ok ? "PASS" : "FAIL", name.c_str(),
                 detail.empty() ? "" : ": ", detail.c_str());
    g_stable += std::string(ok ? "PASS " : "FAIL ") + name + "\n";
}

static auto hex64(uint64_t v) -> std::string
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%llx", static_cast<unsigned long long>(v));
    return buf;
}
static auto fnv1a(const void* data, size_t len) -> uint64_t
{
    const unsigned char* p = static_cast<const unsigned char*>(data);
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < len; ++i)
    {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}
static auto load_file(const std::string& path, std::string& out) -> bool
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}
static auto narrow(const StringType& wide) -> std::string
{
    std::string s;
    s.reserve(wide.size());
    for (const wchar_t c : wide)
    {
        s.push_back(static_cast<char>(c));
    }
    return s;
}
static auto wide_has_nul(const StringType& wide) -> bool
{
    for (const wchar_t c : wide)
    {
        if (c == 0)
        {
            return true;
        }
    }
    return false;
}

static void push_file(std::vector<RC::Platform::NativeModFile>& files, const std::string& key,
                      const std::string& bytes)
{
    auto owner = std::make_shared<const std::string>(bytes);
    RC::Platform::NativeFileBuffer buf;
    buf.owner = owner;  // implicit upcast to shared_ptr<const void>
    buf.bytes = *owner; // view bound to the same immutable owner
    files.push_back({key, buf});
}

static constexpr size_t kStagedMainLuaSize = 21415;
static constexpr size_t kUEHelpersSize = 9962;
static constexpr size_t kSettingsIniSize = 221;

int main(int argc, char** argv)
{
    if (argc < 5)
    {
        std::fprintf(stderr, "usage: %s <bundle root> <receipt> <json> <stable>\n", argv[0]);
        return 3;
    }
    g_receipt = std::fopen(argv[2], "w");
    if (g_receipt == nullptr)
    {
        return 3;
    }
    const std::string bundle = argv[1];

    // C0a: value-level target parity of the extracted conversions regardless of
    // host wchar_t width (unit values are what matters; see header note).
    {
        const std::string key = "TownfallItemSpawner";
        const StringType wide = widen_ascii_native_key(key);
        bool value_ok = wide.size() == key.size();
        for (size_t i = 0; value_ok && i < key.size(); ++i)
        {
            value_ok = static_cast<uint64_t>(wide[i]) == static_cast<unsigned char>(key[i]);
        }
        report(value_ok,
               "C0a value-level parity: widen_ascii_native_key units equal narrow byte values",
               ("sizeof(wchar_t)=" + std::to_string(sizeof(wchar_t))).c_str());
    }

    // --- real bundle bytes off disk ----------------------------------------
    std::string main_lua, enabled_txt, uehelpers_lua, settings_ini;
    const bool disk_ok = load_file(bundle + "/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua", main_lua) &&
                         load_file(bundle + "/ue4ss/Mods/TownfallItemSpawner/enabled.txt", enabled_txt) &&
                         load_file(bundle + "/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua", uehelpers_lua) &&
                         load_file(bundle + "/ue4ss/UE4SS-settings.ini", settings_ini);
    if (!disk_ok)
    {
        std::fprintf(stderr, "cannot read bundle bytes under: %s\n", bundle.c_str());
        return 3;
    }
    report(main_lua.size() == kStagedMainLuaSize && uehelpers_lua.size() == kUEHelpersSize &&
               settings_ini.size() == kSettingsIniSize && enabled_txt.empty(),
           "C0b real bundle bytes loaded with pinned sizes (main.lua 21415 sha f586043a.., "
           "UEHelpers 9962, settings 221, enabled.txt 0)",
           ("main=" + std::to_string(main_lua.size()) + " helpers=" + std::to_string(uehelpers_lua.size()) +
            " settings=" + std::to_string(settings_ini.size()) + " enabled=" + std::to_string(enabled_txt.size()))
               .c_str());

    // --- S1: loadlist rows -> production NativeModSource --------------------
    std::vector<RC::Platform::NativeModFile> files;
    // The five loadlist file rows (worker-itemspawner-bundle/bundle/loadlist.txt):
    // config + lua_mod rows, keyed by their exact loadlist paths. The mods.txt
    // content is the task-specified enable row `TownfallItemSpawner : 1`
    // (canonical UE4SS mods.txt row; the staged bundle carries no mods.txt, its
    // enable marker is the 0-byte enabled.txt — both enable routes are
    // exercised and must dedupe to one startup entry).
    push_file(files, "ue4ss/UE4SS-settings.ini", settings_ini);
    push_file(files, "ue4ss/Mods/mods.txt", "TownfallItemSpawner : 1\n");
    push_file(files, "ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua", main_lua);
    push_file(files, "ue4ss/Mods/TownfallItemSpawner/enabled.txt", enabled_txt);
    push_file(files, "ue4ss/Mods/shared/UEHelpers/UEHelpers.lua", uehelpers_lua);

    auto owned_source = std::make_shared<const RC::Platform::NativeModSource>(std::move(files));
    const auto& source = *owned_source;

    // S1 checks.
    {
        const auto& mods = source.lua_mods();
        report(mods.size() == 1, "S1a production ctor derives exactly one lua_mod row",
               ("count=" + std::to_string(mods.size())).c_str());
        const bool row_ok = mods.size() == 1 && mods[0].name == "TownfallItemSpawner" &&
                            mods[0].path == "ue4ss/Mods/TownfallItemSpawner" &&
                            mods[0].scripts_path == "ue4ss/Mods/TownfallItemSpawner/Scripts";
        report(row_ok, "S1b lua_mod row fields exact (name/path/scripts_path from loadlist keys)",
               row_ok ? "ue4ss/Mods/TownfallItemSpawner/Scripts" : "row mismatch");
        bool shared_present = false;
        for (const auto& mod : mods)
        {
            if (mod.name == "shared" || mod.name == "UEHelpers")
            {
                shared_present = true;
            }
        }
        report(!shared_present, "S1c shared/UEHelpers file row is NOT classified as a lua_mod", "");
        const auto& startup = source.startup_mod_names();
        const bool startup_ok = startup.size() == 1 && startup[0] == "TownfallItemSpawner";
        report(startup_ok,
               "S1d startup_mod_names == [TownfallItemSpawner]: mods.txt row + enabled.txt marker deduped",
               ("count=" + std::to_string(startup.size())).c_str());
        const auto read_back = source.read("ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua");
        report(read_back.has_value() && read_back->bytes.size() == main_lua.size() &&
                   std::equal(read_back->bytes.begin(), read_back->bytes.end(), main_lua.begin()),
               "S1e source.read(main.lua key) round-trips the staged bytes (21415, fnv1a-pinned below)",
               ("fnv1a=" + hex64(fnv1a(main_lua.data(), main_lua.size()))).c_str());
    }

    // --- S2 + S3 + S4: the chain --------------------------------------------
    std::string chain_scripts_path;
    bool chain_installable = false, chain_started = false;
    std::string started_name;
    {
        FixtureProgram program;
        program.m_native_context.mod_source = owned_source;

        // S2: setup_mods-equivalent (verbatim 1835-1843; ctor native branch
        // verbatim 934-947 with normalize_path_for_lua verbatim 42-54).
        program.setup_mods_equivalent();
        const auto& mods = program.m_mods;
        report(mods.size() == 1, "S2a setup_mods-equivalent installed exactly one LuaMod", "");
        LuaMod* installed = mods.empty() ? nullptr : mods[0].get();
        chain_installable = installed && installed->is_installable();
        report(chain_installable,
               "S2b ctor native lookup (normalize_path_for_lua) FOUND the mod: installable, scripts_path derived",
               installed ? narrow(installed->m_scripts_path.wstring()).c_str() : "no mod installed");
        chain_scripts_path = installed ? narrow(installed->m_scripts_path.wstring()) : "";
        report(installed && chain_scripts_path == "ue4ss/Mods/TownfallItemSpawner/Scripts",
               "S2c scripts_path derived from the native row == ue4ss/Mods/TownfallItemSpawner/Scripts",
               chain_scripts_path.c_str());
        report(installed && installed->get_name() == widen_ascii_native_key("TownfallItemSpawner") &&
                   !wide_has_nul(installed->get_name()),
               "S2d stored mod name IS widen_ascii_native_key(name): clean wide, no NULs",
               "");

        // S2 control: OLD conversion at the ctor path boundary — simulated
        // NUL-interleave garble must MISS the native row lookup and set
        // installable(false) (the audit's ctor-miss class).
        {
            bool missed = false;
            std::string how;
            try
            {
                LuaMod control(program, widen_ascii_native_key("TownfallItemSpawner"),
                               old_ensure_str_simulated("ue4ss/Mods/TownfallItemSpawner"));
                missed = !control.is_installable();
                how = control.is_installable() ? "UNEXPECTEDLY found" : "not found, installable(false)";
            }
            catch (const std::exception& e)
            {
                missed = true;
                how = "threw during garbled lookup (still no install): ";
                how += e.what();
            }
            report(missed,
                   "S2e CONTROL (simulated old garble): ctor path lookup MISSES -> set_installable(false)",
                   how.c_str());
        }

        // S3: start_mods-equivalent (verbatim 2043-2051; comparator verbatim
        // 3056-3063). The P0 regression site.
        program.start_mods<LuaMod>();
        LuaMod* started = mods.empty() ? nullptr : mods[0].get();
        chain_started = started && started->is_started();
        report(chain_started,
               "S3a start_mods-equivalent: widen_ascii_native_key(startup name) MATCHED the stored widened name -> start_mod",
               started && started->is_started() ? "started" : "not started");
        report(started && started->m_start_calls == 1,
               "S3b exactly one start_mod call for the one enabled mod", ("calls=" + std::to_string(started ? started->m_start_calls : -1)).c_str());
        started_name = started ? narrow(started->get_name()) : "";
        report(started_name == "TownfallItemSpawner",
               "S3c the started mod is TownfallItemSpawner (wide name round-trips to the narrow native key)",
               started_name.c_str());

        // S3 control: the OLD ensure_str behavior at 2049 — simulated garble
        // against the SAME stored widened name must NOT match (lookup miss =
        // enabled mod never starts, the P0). This is the discriminator proving
        // S3a tests the fix and not a tautology.
        {
            const StringType garbled = old_ensure_str_simulated("TownfallItemSpawner");
            const bool garbled_is_garbled = garbled.size() == 2 * std::string("TownfallItemSpawner").size() &&
                                            wide_has_nul(garbled);
            const LuaMod* missed =
                UE4SSProgram::find_mod_by_name<LuaMod>(garbled, UE4SSProgram::IsInstalled::Yes);
            report(garbled_is_garbled && missed == nullptr,
                   "S3d CONTROL (simulated old ensure_str NUL-interleave at the 2049 site): lookup MISSES the stored widened name",
                   garbled_is_garbled ? (missed == nullptr ? "nullptr as required" : "UNEXPECTEDLY matched")
                                      : "garble simulation degenerate");
        }

        // S4: main.lua read through the source + banner via the FIXED
        // to_generic_string.
        if (started)
        {
            const auto buf = source.read(
                modbridge::normalize_path_for_lua(started->m_scripts_path / "main.lua"));
            const bool bytes_ok = buf.has_value() && buf->bytes.size() == main_lua.size() &&
                                  std::equal(buf->bytes.begin(), buf->bytes.end(), main_lua.begin());
            report(bytes_ok,
                   "S4a read(normalize(scripts_path/main.lua)) via the source == staged main.lua bytes (21415)",
                   ("fnv1a=" + hex64(fnv1a(main_lua.data(), main_lua.size()))).c_str());

            // Banner provenance: the exact bytes main.lua's log() emits at line
            // 746 (MOD_TAG .. " " .. text .. "\n") must be present in the REAL
            // staged main.lua.
            static const char kBannerText[] = "Loaded v1.0. Clean pickup-only release. F7 opens the menu.";
            static const char kModTag[] = "[TownfallItemSpawner]";
            const bool provenance = std::search(main_lua.begin(), main_lua.end(), std::begin(kBannerText),
                                                std::end(kBannerText) - 1) != main_lua.end() &&
                                    std::search(main_lua.begin(), main_lua.end(), std::begin(kModTag),
                                                std::end(kModTag) - 1) != main_lua.end();
            report(provenance,
                   "S4b banner text provenance: line-746 log text + MOD_TAG present in the staged main.lua bytes",
                   "");

            // The banner bytes through the FIXED to_generic_string (verbatim
            // 595-636, PROSPERO branch).
            const std::string banner_narrow =
                std::string(kModTag) + " " + kBannerText + "\n";
            const StringType banner_wide = to_generic_string(banner_narrow.c_str());
            bool clean = banner_wide.size() == banner_narrow.size() && !wide_has_nul(banner_wide);
            for (size_t i = 0; clean && i < banner_narrow.size(); ++i)
            {
                if (static_cast<wchar_t>(static_cast<unsigned char>(banner_narrow[i])) != banner_wide[i])
                {
                    clean = false;
                }
            }
            report(clean,
                   "S4c banner through the FIXED to_generic_string: clean wide, no NUL units, byte-exact widen",
                   ("units=" + std::to_string(banner_wide.size())).c_str());

            // S4 control: simulated OLD route — the same banner through the
            // documented NUL-interleave garbles (size doubles, NUL units), so
            // the S4c assertion discriminates the fix.
            const StringType banner_old = old_ensure_str_simulated(banner_narrow);
            report(banner_old.size() == 2 * banner_narrow.size() && wide_has_nul(banner_old),
                   "S4d CONTROL (simulated old narrow->wide route): banner garbles NUL-interleaved",
                   ("units=" + std::to_string(banner_old.size())).c_str());

            // Extraction breadth: passthrough branches of the verbatim
            // to_generic_string behave (StringType / StringViewType inputs).
            const StringType direct = L"[TownfallItemSpawner] direct";
            const StringType passthrough = to_generic_string(direct);
            const StringViewType view = L"[TownfallItemSpawner] view";
            const StringType from_view = to_generic_string(view);
            report(passthrough == direct && from_view == StringType{view},
                   "S4e to_generic_string wide passthrough branches (StringType/StringViewType) unchanged",
                   "");
        }
        else
        {
            report(false, "S4a-c skipped: mod did not start", "not_reached");
        }
    }

    const bool ok = g_fail == 0;
    FILE* sf = std::fopen(argv[4], "w");
    if (sf != nullptr)
    {
        std::fputs(g_stable.c_str(), sf);
        std::fprintf(sf, "main_lua_fnv1a=%s\nverdict=%s\n",
                     hex64(fnv1a(main_lua.data(), main_lua.size())).c_str(),
                     ok ? "LIFECYCLE-E2E-PASS" : "LIFECYCLE-E2E-FAIL");
        std::fclose(sf);
    }
    FILE* jf = std::fopen(argv[3], "w");
    if (jf != nullptr)
    {
        std::fprintf(jf,
            "{\n"
            "  \"worker\": \"worker-lifecycle-e2e\",\n"
            "  \"mode\": \"chain-e2e\",\n"
            "  \"production_units\": {\n"
            "    \"ue4ssprogram_cpp_wpd1\": \"cb6cb14aa044124f17cf8d2115b4962a7d91daa686f6e80a0631061b51b8c8af\",\n"
            "    \"luamod_cpp_l11_merged\": \"c92c92ff66686bf6f44e18aaec7d36b227d32fc75726f33349f4bdc7412ef3c0\",\n"
            "    \"string_hpp_staged\": \"c76be502fbe6f9c2c6589f141c1caded84af482a1f03a428005574850ca47788\",\n"
            "    \"native_mod_source_cpp\": \"2db163bc7feead56badc12ad235711576b837e1f5a05dba04a4d8fb03e53524f\",\n"
            "    \"native_mod_source_hpp\": \"2b44df14854934470a16fec6ef153501e08a8554ba51748b335fd0733856a450\"\n"
            "  },\n"
            "  \"staged_main_lua_sha256_expected\": \"f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658\",\n"
            "  \"main_lua_size\": %zu,\n"
            "  \"main_lua_fnv1a\": \"%s\",\n"
            "  \"chain\": {\n"
            "    \"scripts_path\": \"%s\",\n"
            "    \"installable\": %s,\n"
            "    \"started\": %s,\n"
            "    \"started_name\": \"%s\"\n"
            "  },\n"
            "  \"passed\": %d,\n"
            "  \"failed\": %d,\n"
            "  \"verdict\": \"%s\"\n"
            "}\n",
            main_lua.size(), hex64(fnv1a(main_lua.data(), main_lua.size())).c_str(),
            chain_scripts_path.c_str(), chain_installable ? "true" : "false",
            chain_started ? "true" : "false", started_name.c_str(),
            g_pass, g_fail, ok ? "LIFECYCLE-E2E-PASS" : "LIFECYCLE-E2E-FAIL");
        std::fclose(jf);
    }
    std::fprintf(g_receipt, "\nverdict=%s (%d passed, %d failed)\n",
                 ok ? "LIFECYCLE-E2E-PASS" : "LIFECYCLE-E2E-FAIL", g_pass, g_fail);
    std::fclose(g_receipt);
    return ok ? 0 : 1;
}
