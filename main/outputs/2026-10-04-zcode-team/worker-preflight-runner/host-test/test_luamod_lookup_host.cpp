// test_luamod_lookup_host.cpp — worker-preflight-runner Lua-leg production
// lookup regression (coordinator directive item 4).
//
// Proves the FIXED production conversion path delivers the staged mod bytes
// for the EXACT runtime key — finding the file on disk is NOT sufficient:
//   runtime key "ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua"
//   -> the fixed widen loop (wpd1 UE4SSProgram.cpp setup_mods)
//   -> the fixed modbridge::normalize_path_for_lua (LuaMod.cpp)
//   -> the PRODUCTION NativeModSource::read (verbatim-compiled
//      NativeModSource.cpp, sha 2db163bc...; header sha 2b44df14... — the
//      composition-pinned copies, launch11-combined build script line 178)
//   -> bytes EQUAL to the staged main.lua (21415 bytes, sha256 f586043a...,
//      verified by the driver BEFORE compiling; the binary compares read()
//      output against the loaded staged bytes).
//
// The two fixed conversion functions are VERBATIM extractions compiled from
// staging-lua/fixed_conversion_path.inc (byte-exact sed ranges, provenance in
// that file). Target parity: StringType = std::wstring, File::CharType =
// wchar_t (2-byte wchar_t on the PS5 target — the codecvt stride-bug class
// the fix avoids).
//
// Freestanding host binary: clang++ -std=c++20 -Wall -Wextra
//   -I staging-lua staging-lua/NativeModSource.cpp <this file>
// argv: [1] staged main.lua path, [2] receipt, [3] json, [4] stable.
// Exit 0 = all checks pass. Writes files only (stdout pipes wedge).

#include "Platform/NativeModSource.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Target-parity typedefs (PS5: 2-byte wchar_t). The verbatim wpd1 lambda and
// the production LuaMod StringType semantics.
namespace File
{
    using CharType = wchar_t;
}
using StringType = std::wstring;

#include "../staging-lua/fixed_conversion_path.inc" // verbatim fixed path

static int g_pass = 0, g_fail = 0;
static FILE* g_receipt = nullptr;
static std::string g_stable;

static auto report(bool ok, const std::string& name, const std::string& detail = "") -> void
{
    if (ok) { ++g_pass; } else { ++g_fail; }
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
    for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}
static auto load_file(const std::string& path, std::string& out) -> bool
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { return false; }
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}
static auto narrow(const StringType& wide) -> std::string
{
    // narrow exactly like the production widen inverse: 1:1 for ASCII
    std::string s;
    s.reserve(wide.size());
    for (const wchar_t c : wide) { s.push_back(static_cast<char>(c)); }
    return s;
}

static constexpr const char* kExactKey = "ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua";
static constexpr size_t kStagedSize = 21415;

int main(int argc, char** argv)
{
    if (argc < 5)
    {
        std::fprintf(stderr, "usage: %s <staged main.lua> <receipt> <json> <stable>\n", argv[0]);
        return 3;
    }
    g_receipt = std::fopen(argv[2], "w");
    if (g_receipt == nullptr) { return 3; }

    std::string staged;
    if (!load_file(argv[1], staged))
    {
        std::fprintf(stderr, "cannot read staged file: %s\n", argv[1]);
        return 3;
    }
    report(staged.size() == kStagedSize,
           "M0 staged main.lua loaded with the pinned size 21415",
           "size=" + std::to_string(staged.size()));

    // Production snapshot construction: the exact staged tree entries under
    // ue4ss/, keys exact UTF-8 below ue4ss/ (loadlist-staged discipline).
    std::vector<RC::Platform::NativeModFile> files;
    {
        auto owner = std::make_shared<const std::string>(staged);
        RC::Platform::NativeFileBuffer buf;
        buf.owner = owner;               // implicit upcast to shared_ptr<const void>
        buf.bytes = *owner;              // view bound to the same immutable owner
        files.push_back({kExactKey, buf});
    }
    {
        const std::string enabled = "1\n"; // staged TownfallItemSpawner/enabled.txt marker row
        auto owner = std::make_shared<const std::string>(enabled);
        RC::Platform::NativeFileBuffer buf;
        buf.owner = owner;
        buf.bytes = *owner;
        files.push_back({"ue4ss/Mods/TownfallItemSpawner/enabled.txt", buf});
    }
    std::sort(files.begin(), files.end(),
              [](const RC::Platform::NativeModFile& a, const RC::Platform::NativeModFile& b) {
                  return a.path < b.path;
              });
    const RC::Platform::NativeModSource source(files); // production ctor (derives lua_mods + startup names)

    // M1: the production ctor derives the mod (the read(main.lua) gate).
    {
        bool found = false;
        for (const auto& mod : source.lua_mods())
        {
            if (mod.name == "TownfallItemSpawner") { found = true; }
        }
        report(found, "M1 production ctor derives LuaMod TownfallItemSpawner (Scripts/main.lua gate)",
               "mod absent from derived lua_mods");
    }
    // M2: startup names via staged enabled.txt (production enable path).
    {
        bool enabled = false;
        for (const auto& n : source.startup_mod_names())
        {
            if (n == "TownfallItemSpawner") { enabled = true; }
        }
        report(enabled, "M2 production enable path: staged enabled.txt -> startup_mod_names",
               "mod not enabled");
    }
    // M3/M4: the fixed normalization delivers the EXACT canonical key from
    // both separator conventions.
    {
        const std::string fwd = modbridge::normalize_path_for_lua(std::filesystem::path(kExactKey));
        report(fwd == kExactKey, "M3 normalize_path_for_lua(forward-slash path) == exact runtime key",
               fwd.c_str());
        const std::filesystem::path back("ue4ss\\Mods\\TownfallItemSpawner\\Scripts\\main.lua");
        const std::string norm = modbridge::normalize_path_for_lua(back);
        report(norm == kExactKey, "M4 normalize_path_for_lua(backslash path) == exact runtime key",
               norm.c_str());
    }
    // M5: THE regression — production read() returns the staged bytes.
    {
        const auto buf = source.read(kExactKey);
        bool equal = buf.has_value() && buf->bytes.size() == staged.size() &&
                     std::equal(buf->bytes.begin(), buf->bytes.end(), staged.begin());
        report(buf.has_value() && equal,
               "M5 NativeModSource::read(exact key) returns bytes EQUAL to staged main.lua (21415)",
               ("size=" + std::to_string(buf.has_value() ? buf->bytes.size() : 0) +
                " fnv1a=" + (buf.has_value() ? hex64(fnv1a(buf->bytes.data(), buf->bytes.size()))
                                             : std::string("none")))
                   .c_str());
    }
    // M6: the fixed widen loop — byte-wise, no codecvt garbling, clean
    // round-trip for the mod name (the bug produced NUL-interleaved bytes).
    {
        const std::string name = "TownfallItemSpawner";
        const StringType wide = widen_ascii(name); // verbatim fixed lambda
        bool clean = wide.size() == name.size();
        for (const wchar_t c : wide)
        {
            if (c == 0 || c >= 0x80) { clean = false; }
        }
        report(clean && narrow(wide) == name,
               "M6 fixed widen_ascii: byte-wise widening, no NUL interleave, exact narrow round-trip",
               ("wide_size=" + std::to_string(wide.size())).c_str());
    }
    // M7: refusal discipline of the production read path.
    {
        bool threw = false;
        try
        {
            (void)source.read("ue4ss\\Mods\\TownfallItemSpawner\\Scripts\\main.lua");
        }
        catch (const std::invalid_argument&)
        {
            threw = true; // validate_path rejects backslash keys outright
        }
        report(threw, "M7a backslash key REFUSED by validate_path (invalid character)", "");
        report(!source.read("ue4ss/Mods/TownfallItemSpawner/Scripts/missing.lua").has_value(),
               "M7b absent key returns nullopt (never fabricated bytes)", "");
    }
    // M8: the two production call sites, emulated over the same source.
    {
        const auto scripts_read = source.read(modbridge::normalize_path_for_lua(
            std::filesystem::path("ue4ss/Mods/TownfallItemSpawner/Scripts") / "main.lua"));
        report(scripts_read.has_value() && scripts_read->bytes.size() == kStagedSize,
               "M8a LuaMod.cpp:6883 call-site shape: normalize(scripts_path / main.lua) -> read -> staged bytes",
               "");
        const auto mod_dir = source.read(modbridge::normalize_path_for_lua(
            std::filesystem::path("ue4ss/Mods/TownfallItemSpawner")));
        report(!mod_dir.has_value(),
               "M8b LuaMod.cpp:936 call-site shape: normalize(mod_path) -> read -> nullopt (dir key absent, honest)",
               "");
    }

    const bool ok = g_fail == 0;
    FILE* sf = std::fopen(argv[4], "w");
    if (sf != nullptr)
    {
        std::fputs(g_stable.c_str(), sf);
        std::fprintf(sf, "staged_fnv1a=%s\nverdict=%s\n", hex64(fnv1a(staged.data(), staged.size())).c_str(),
                     ok ? "LUA-LOOKUP-PASS" : "LUA-LOOKUP-FAIL");
        std::fclose(sf);
    }
    FILE* jf = std::fopen(argv[3], "w");
    if (jf != nullptr)
    {
        std::fprintf(jf,
            "{\n  \"worker\": \"worker-preflight-runner\",\n  \"mode\": \"lua-lookup\",\n"
            "  \"production_units\": {\n"
            "    \"native_mod_source_cpp\": \"2db163bc7feead56badc12ad235711576b837e1f5a05dba04a4d8fb03e53524f\",\n"
            "    \"native_mod_source_hpp\": \"2b44df14854934470a16fec6ef153501e08a8554ba51748b335fd0733856a450\",\n"
            "    \"luamod_cpp_fixed\": \"c92c92ff66686bf6f44e18aaec7d36b227d32fc75726f33349f4bdc7412ef3c0\",\n"
            "    \"ue4ssprogram_cpp_fixed\": \"68b8bd187531574df177bcc5266374d12a94158218d75c86df7bc92b4bc42539\"\n"
            "  },\n"
            "  \"staged_main_lua_sha256_expected\": \"f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658\",\n"
            "  \"staged_size\": %zu,\n"
            "  \"read_key\": \"%s\",\n"
            "  \"staged_fnv1a\": \"%s\",\n"
            "  \"passed\": %d,\n  \"failed\": %d,\n  \"verdict\": \"%s\"\n}\n",
            staged.size(), kExactKey, hex64(fnv1a(staged.data(), staged.size())).c_str(),
            g_pass, g_fail, ok ? "LUA-LOOKUP-PASS" : "LUA-LOOKUP-FAIL");
        std::fclose(jf);
    }
    std::fprintf(g_receipt, "\nverdict=%s (%d passed, %d failed)\n",
                 ok ? "LUA-LOOKUP-PASS" : "LUA-LOOKUP-FAIL", g_pass, g_fail);
    std::fclose(g_receipt);
    return ok ? 0 : 1;
}
