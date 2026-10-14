// Host test for PakRequestProducer game_pak_mods_directory (#7 step 2 source). Mac-only.
// usage: gamepaks_test <empty scratch dir>
#include "PakRequestProducer.hpp"
#include <cstdio>
#include <fstream>
#include <sys/stat.h>
namespace fs = std::filesystem;
using ModBridge::PakMount::game_pak_mods_directory;
using ModBridge::PakMount::discover_requests;

static int run = 0, failed = 0;
static void check(bool ok, const char* name, const std::string& detail) {
    ++run;
    if (!ok) ++failed;
    std::printf("%s %s %s\n", ok ? "PASS" : "FAIL", name, detail.c_str());
}
static void touch(const fs::path& p) { fs::create_directories(p.parent_path()); std::ofstream(p) << "x"; }
static void trio(const fs::path& dir, const char* stem) {
    touch(dir / (std::string(stem) + ".pak")); touch(dir / (std::string(stem) + ".utoc")); touch(dir / (std::string(stem) + ".ucas"));
}
static fs::path package(const fs::path& root, const char* name) {
    const auto app = root / name;
    fs::create_directories(app / "sce_sys"); fs::create_directories(app / "sce_module");
    touch(app / "eboot.bin");
    return app;
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const fs::path root = fs::path(argv[1]);
    std::string why;

    { // 1. lower-case PS5 package, one mod trio
        auto app = package(root, "t1");
        trio(app / "townfall/content/paks", "pakchunk0-ps5");
        trio(app / "townfall/content/paks/~mods", "zzz_mod_P");
        fs::create_directories(app / "engine/content/paks/~mods");
        why.clear(); auto got = game_pak_mods_directory(app, why);
        check(got == app / "townfall/content/paks/~mods", "lower-case-found", got.string() + " " + why);
        auto rows = got.empty() ? std::vector<ModBridge::PakMount::MountRequest>{} : discover_requests(got);
        check(rows.size() == 1 && rows[0].pak_path_abs == (app / "townfall/content/paks/~mods/zzz_mod_P.pak").string(),
              "lower-case-census", rows.empty() ? "none" : rows[0].pak_path_abs);
    }
    { // 2. PC-cased names
        auto app = package(root, "t2");
        trio(app / "Townfall/Content/Paks/~mods", "a_P");
        why.clear(); auto got = game_pak_mods_directory(app, why);
        check(got == app / "Townfall/Content/Paks/~mods", "pc-case-found", got.string() + " " + why);
    }
    { // 3. no ~mods folder = opt-out with a reason
        auto app = package(root, "t3");
        trio(app / "townfall/content/paks", "pakchunk0-ps5");
        why.clear(); auto got = game_pak_mods_directory(app, why);
        check(got.empty() && why.find("no ~mods folder in") != std::string::npos, "no-tilde-mods", why);
    }
    { // 4. two projects with Content = refused
        auto app = package(root, "t4");
        fs::create_directories(app / "a/content/paks/~mods"); fs::create_directories(app / "b/Content/Paks/~mods");
        why.clear(); auto got = game_pak_mods_directory(app, why);
        check(got.empty() && why.find("several project Content folder") != std::string::npos, "several-projects", why);
    }
    { // 5. only Engine has Content = refused
        auto app = package(root, "t5");
        fs::create_directories(app / "Engine/Content/Paks/~mods");
        why.clear(); auto got = game_pak_mods_directory(app, why);
        check(got.empty() && why.find("no project Content folder") != std::string::npos, "engine-only", why);
    }
    { // 6. an unreadable sibling is skipped and counted
        auto app = package(root, "t6");
        fs::create_directories(app / "townfall/content/paks/~mods");
        ::chmod((app / "sce_sys").c_str(), 0);
        why.clear(); auto got = game_pak_mods_directory(app, why);
        ::chmod((app / "sce_sys").c_str(), 0755);
        check(got == app / "townfall/content/paks/~mods", "unreadable-sibling-skipped", got.string() + " " + why);
        ::chmod((app / "sce_sys").c_str(), 0);
        fs::remove_all(app / "townfall");
        why.clear(); got = game_pak_mods_directory(app, why);
        ::chmod((app / "sce_sys").c_str(), 0755);
        check(got.empty() && why.find("unreadable=1") != std::string::npos, "unreadable-counted", why);
    }
    { // 7. a symlinked project folder is never followed
        auto app = package(root, "t7");
        fs::create_directories(root / "outside/content/paks/~mods");
        fs::create_directory_symlink(root / "outside", app / "townfall");
        why.clear(); auto got = game_pak_mods_directory(app, why);
        check(got.empty() && why.find("no project Content folder") != std::string::npos, "symlink-project-refused", why);
    }
    { // 8. a symlinked ~mods is never followed
        auto app = package(root, "t8");
        fs::create_directories(app / "townfall/content/paks"); fs::create_directories(root / "elsewhere");
        fs::create_directory_symlink(root / "elsewhere", app / "townfall/content/paks/~mods");
        why.clear(); auto got = game_pak_mods_directory(app, why);
        check(got.empty() && why.find("no ~mods folder in") != std::string::npos, "symlink-tilde-mods-refused", why);
    }
    { // 9. a FILE named content does not count
        auto app = package(root, "t9");
        touch(app / "townfall/content");
        why.clear(); auto got = game_pak_mods_directory(app, why);
        check(got.empty() && why.find("no project Content folder") != std::string::npos, "file-named-content", why);
    }
    { // 10. bad game directories
        const char* bad[] = {"", "relative/app0", "/"};
        for (const char* b : bad) {
            why.clear(); auto got = game_pak_mods_directory(fs::path(b), why);
            check(got.empty() && why == "configured game directory unavailable", "bad-game-dir", std::string("'") + b + "' " + why);
        }
        why.clear(); auto got = game_pak_mods_directory(root / "t1/../t1", why);
        check(got.empty() && why == "configured game directory unavailable", "non-normal-game-dir", why);
    }
    { // 11. missing game directory reports the errno
        why.clear(); auto got = game_pak_mods_directory(root / "does-not-exist", why);
        check(got.empty() && why.find("listing unavailable") != std::string::npos && why.find("(errno 2)") != std::string::npos,
              "missing-game-dir-errno", why);
    }
    { // 12. several ~mods spellings = refused
        auto app = package(root, "t12");
        fs::create_directories(app / "townfall/content/paks/~mods"); fs::create_directories(app / "townfall/content/paks/~MODS");
        why.clear(); auto got = game_pak_mods_directory(app, why);
        // a case-insensitive host filesystem folds the two into one folder; then exactly one is found
        bool folded = fs::equivalent(app / "townfall/content/paks/~mods", app / "townfall/content/paks/~MODS");
        check(folded ? !got.empty() : (got.empty() && why.find("several ~mods folder") != std::string::npos),
              "several-tilde-mods", std::string(folded ? "(host folds case) " : "") + got.string() + " " + why);
    }
    std::printf("RUN %s run=%d failed=%d\n", (run > 0 && failed == 0) ? "PASS" : "FAIL", run, failed);
    return (run > 0 && failed == 0) ? 0 : 1;
}
