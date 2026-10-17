// shim_root_lexical_host_test.cpp -- Mac-only host test (2026-10-12, fallback-freeze).
//
// Question: what shim root does LuaMod::prepare_mod hand to LuaPathShim_install
// when the native mods directory is the RELATIVE literal "ue4ss/Mods" (as in the
// fallback builds' UE4SSProgram.cpp setup_mod_directory_path), versus an
// ABSOLUTE "<root>/ue4ss/Mods" (as in B4)?
//
// This file re-types ONLY the three lexical std::filesystem steps of that block
// (lexically_normal, filename()=="Mods" -> parent_path, parent_path); it is not
// a copy of the runtime source. The root used for the absolute case is a
// neutral placeholder, not a title id.
//
// Expected (std::filesystem lexical rules): relative -> "" (empty, so the shim
// takes its no-root branch); absolute -> "/placeholder-root".
// Prints how many cases it checked; exits 1 if any failed or none ran.
#include <cstdio>
#include <filesystem>
#include <string>

static std::string derive(const std::string& mods_dir)
{
    std::filesystem::path shim_root = std::filesystem::path(mods_dir).lexically_normal();
    if (shim_root.filename() == "Mods")
    {
        shim_root = shim_root.parent_path();
    }
    shim_root = shim_root.parent_path();
    return shim_root.string();
}

int main()
{
    struct Case
    {
        const char* label;
        const char* mods_dir;
        const char* expected;
    };
    const Case cases[] = {
        {"fallback (relative native mods dir)", "ue4ss/Mods", ""},
        {"fallback variant with ./ prefix", "./ue4ss/Mods", ""},
        {"B4-style absolute native mods dir", "/placeholder-root/ue4ss/Mods", "/placeholder-root"},
    };
    int checked = 0;
    int failed = 0;
    for (const auto& c : cases)
    {
        ++checked;
        const std::string got = derive(c.mods_dir);
        const bool ok = got == c.expected;
        if (!ok)
        {
            ++failed;
        }
        std::printf("[%s] %s: mods_dir=\"%s\" -> shim_root=\"%s\" (empty=%s, expected \"%s\")\n",
                    ok ? "PASS" : "FAIL", c.label, c.mods_dir, got.c_str(), got.empty() ? "yes" : "no", c.expected);
    }
    std::printf("SUMMARY shim-root-lexical: checks=%d pass=%d fail=%d\n", checked, checked - failed, failed);
    if (checked == 0)
    {
        std::printf("FAIL: zero checks ran\n");
        return 1;
    }
    return failed == 0 ? 0 : 1;
}
