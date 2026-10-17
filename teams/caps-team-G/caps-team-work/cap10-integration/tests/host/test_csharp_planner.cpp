// Host test (Mac clang++) for Cap10CSharpMods.hpp: the lazy-start decision.
// Proves plan_csharp_mods requests the probe (-> Mono starts) ONLY when the
// activation snapshot enables a C# mod whose csharp/ holds the exact probe
// image, and otherwise requests nothing. No Mono, no engine. Prints a count.
#include "Cap10CSharpMods.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace fs = std::filesystem;
static int checks = 0;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)

static void write_file(const fs::path& p, const std::string& bytes)
{
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

int main()
{
    const std::string probe = "MZ\x90\x00probe-image-bytes-v1";  // stand-in built-in image
    const std::uint8_t* img = reinterpret_cast<const std::uint8_t*>(probe.data());
    const std::size_t img_n = probe.size();
    auto none = [](std::string_view) { return false; };

    // Case 1: no csharp/ anywhere -> no C# runtime.
    {
        fs::path root = fs::temp_directory_path() / "cap10t1/mods";
        fs::create_directories(root / "LuaOnly/scripts");
        auto plan = RC::Cap10Mods::plan_csharp_mods(root, std::nullopt, none, img, img_n);
        CHECK(!plan.probe_requested);
        CHECK(plan.csharp_mods.empty());
        CHECK(plan.dirs_with_csharp == 0);
        fs::remove_all(root.parent_path());
    }
    // Case 2: csharp/ present with the probe, but the mod is NOT enabled -> nothing.
    {
        fs::path root = fs::temp_directory_path() / "cap10t2/mods";
        write_file(root / "CsMod/csharp/Probe.dll", probe);
        auto plan = RC::Cap10Mods::plan_csharp_mods(root, std::string_view("CsMod:0\n"), none, img, img_n);
        CHECK(plan.dirs_with_csharp == 1);
        CHECK(plan.csharp_mods.empty());
        CHECK(!plan.probe_requested);  // disabled => Mono stays out
        fs::remove_all(root.parent_path());
    }
    // Case 3: enabled via mods.txt + exact probe -> probe requested (Mono starts).
    {
        fs::path root = fs::temp_directory_path() / "cap10t3/mods";
        write_file(root / "CsMod/csharp/Probe.dll", probe);
        auto plan = RC::Cap10Mods::plan_csharp_mods(root, std::string_view("CsMod:1\n"), none, img, img_n);
        CHECK(plan.csharp_mods.size() == 1 && plan.csharp_mods[0] == "CsMod");
        CHECK(plan.probe_requested);
        CHECK(plan.assemblies_checked == 1);
        fs::remove_all(root.parent_path());
    }
    // Case 4: enabled via enabled.txt marker -> probe requested.
    {
        fs::path root = fs::temp_directory_path() / "cap10t4/mods";
        write_file(root / "CsMod/csharp/anything.dll", probe);
        auto marker = [](std::string_view n) { return n == "CsMod"; };
        auto plan = RC::Cap10Mods::plan_csharp_mods(root, std::nullopt, marker, img, img_n);
        CHECK(plan.probe_requested);
        fs::remove_all(root.parent_path());
    }
    // Case 5: enabled but the assembly is NOT the probe -> not run (no Mono).
    {
        fs::path root = fs::temp_directory_path() / "cap10t5/mods";
        write_file(root / "CsMod/csharp/Other.dll", "MZ different assembly entirely");
        auto plan = RC::Cap10Mods::plan_csharp_mods(root, std::string_view("CsMod:1\n"), none, img, img_n);
        CHECK(plan.csharp_mods.size() == 1);
        CHECK(!plan.probe_requested);  // arbitrary C# is refused
        CHECK(plan.assemblies_checked == 1);
        fs::remove_all(root.parent_path());
    }
    // Case 6: a relative mods dir is refused (fail closed).
    {
        auto plan = RC::Cap10Mods::plan_csharp_mods("relative/mods", std::string_view("CsMod:1\n"), none, img, img_n);
        CHECK(!plan.probe_requested);
        CHECK(plan.csharp_mods.empty());
    }
    // Case 7: no probe image supplied -> refuse (fail closed).
    {
        fs::path root = fs::temp_directory_path() / "cap10t7/mods";
        write_file(root / "CsMod/csharp/Probe.dll", probe);
        auto plan = RC::Cap10Mods::plan_csharp_mods(root, std::string_view("CsMod:1\n"), none, nullptr, 0);
        CHECK(!plan.probe_requested);
        fs::remove_all(root.parent_path());
    }
    std::printf("test_csharp_planner items_checked=%d PASS\n", checks);
    return checks == 0 ? 2 : 0;
}
