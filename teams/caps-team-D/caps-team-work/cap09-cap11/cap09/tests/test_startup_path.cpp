// test_startup_path.cpp -- review finding B1: the activation-snapshot startup path.
//
// On the console R11 always runs with context.mod_source set. This test drives
// the staged startup path end to end on the Mac:
//   NativeCppModInventory (which mods: snapshot facts + real mods directory)
//   -> FindNativeCppModModule (dlls/main.prx; the STAGED copy of the port
//      header, error_code overloads only [F1])
//   -> the REAL module owner CppModModule.cpp (POSIX branch, unchanged)
//   -> select_entry (ABI v1 only) -> host core attach, in startup order,
// the same sequence the staged UE4SSProgram.cpp hunks run (setup_mods creates a
// CppMod per planned mod = load; start_mods<CppMod> starts them in order).
// UE4SSProgram.cpp itself cannot run on the host; its hunks are target-compiled
// (target_compile_check.py) and anchor-checked (check_sources.py).
#include "fake_engine.hpp"

#include <Platform/CppModModule.hpp>
#include <Platform/NativeCppModInventory.hpp>
#include <Platform/NativeCppModPath.hpp>
#include <Platform/NativeModLoaderPolicy.hpp>

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

using namespace RC::Platform;
using namespace RC::Platform::NativeMods;
namespace fs = std::filesystem;

static void write_file(const fs::path& p, std::string_view bytes)
{
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

static auto names(const CppModInventory& inv) -> std::string
{
    std::string s;
    for (auto& c : inv.start_order) s += (s.empty() ? "" : ",") + c.name;
    return s;
}

static auto has_note(const CppModInventory& inv, std::string_view needle) -> bool
{
    for (auto& n : inv.notes)
        if (n.find(needle) != std::string::npos) return true;
    return false;
}

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Checks c;
    if (argc < 2)
    {
        std::printf("usage: test_startup_path <build-dir>\n");
        return 2;
    }
    const fs::path build = fs::absolute(argv[1]);

    std::printf("== mods.txt dialect (same as NativeModSource) ==\n");
    {
        std::vector<std::pair<std::string, bool>> rows;
        std::string why;
        c.ok(parse_mods_txt("\xef\xbb\xbf" "A : 1\r\n; comment\n\tB:0\n\nC :1", rows, why) && rows.size() == 3 && rows[0] == std::pair<std::string, bool>{"A", true} &&
                 rows[1] == std::pair<std::string, bool>{"B", false} && rows[2].first == "C",
             "BOM, CRLF, comments, blanks, tabs accepted");
        c.ok(!parse_mods_txt("A : 2\n", rows, why) && rows.empty() && why.find("malformed") != std::string::npos, "malformed row refuses the whole file");
        c.ok(!parse_mods_txt(std::string_view("A : 1\0", 6), rows, why) && why.find("NUL") != std::string::npos, "NUL refused");
    }

    std::printf("== mod names ==\n");
    c.ok(valid_cpp_mod_name("NativeLifecycleProbe") && valid_cpp_mod_name("My-Mod_2"), "plain names accepted");
    c.ok(!valid_cpp_mod_name("") && !valid_cpp_mod_name(".") && !valid_cpp_mod_name("..") && !valid_cpp_mod_name("a b") && !valid_cpp_mod_name("a/b") &&
             !valid_cpp_mod_name("a\\b") && !valid_cpp_mod_name("Shared") && !valid_cpp_mod_name("\xc3\xa9t\xc3\xa9") && !valid_cpp_mod_name(std::string(129, 'a')),
         "empty, dot, space, separators, 'shared', non-ASCII and over-long names refused");

    std::printf("== plan: snapshot decides, directory supplies ==\n");
    {
        const fs::path md = "/data/x/ue4ss/Mods";
        std::set<std::string> markers{"A", "Z", "E"};
        auto marker = [&](std::string_view n) { return markers.count(std::string(n)) != 0; };
        // dirs with dlls/: A B C D E ; mods.txt: D:1, B:1, A:0, Q:1 (no dir)
        auto inv = plan_cpp_mods(md, std::string_view("D : 1\nB : 1\nA : 0\nQ : 1\n"), marker, {"E", "C", "A", "B", "D"});
        c.ok(names(inv) == "D,B,A,E", "order = mods.txt ':1' rows in file order, then enabled.txt mods by name (got " + names(inv) + ")");
        c.ok(inv.start_order[0].mod_dir == md / "D", "mod directory is <mods dir>/<name>");
        c.ok(has_note(inv, "C++ mod C has dlls/ but is not enabled"), "a dlls/ mod that is not enabled is reported and not loaded");
        c.ok(names(plan_cpp_mods(md, std::nullopt, {}, {"A"})).empty(), "no mods.txt and no marker: nothing starts (default off)");
        auto bad = plan_cpp_mods(md, std::string_view("B : 7\n"), marker, {"A", "B"});
        c.ok(names(bad) == "A" && has_note(bad, "mods.txt ignored for C++ mods"), "malformed mods.txt ignored; enabled.txt still works");
        auto throwing = plan_cpp_mods(md, std::nullopt, [](std::string_view) -> bool { throw std::invalid_argument("bad key"); }, {"A"});
        c.ok(names(throwing).empty() && has_note(throwing, "lookup failed"), "a throwing snapshot lookup skips that mod, never escapes");
        auto weird = plan_cpp_mods(md, std::nullopt, [](std::string_view) { return true; }, {"ok", "a b", "shared"});
        c.ok(names(weird) == "ok" && has_note(weird, "name is not a plain ASCII mod name"), "invalid folder names skipped with a note");
    }

    std::printf("== directory listing (non-throwing filesystem only) ==\n");
    const fs::path root = build / "startup-root";
    fs::remove_all(root);
    const fs::path mods = root / "ue4ss" / "Mods";
    {
        // real tree: probe (enabled.txt), official (mods.txt), legacy (enabled, refused),
        // disabled (dlls, not enabled; its main.prx is garbage so opening it would log),
        // lua-only, shared, a file named dlls, a mod with an empty dlls/.
        fs::create_directories(mods / "NativeLifecycleProbe" / "dlls");
        fs::copy_file(build / "NativeLifecycleProbe.dylib", mods / "NativeLifecycleProbe" / "dlls" / "main.prx", fs::copy_options::overwrite_existing);
        fs::create_directories(mods / "OfficialExample" / "dlls");
        fs::copy_file(build / "official_example.dylib", mods / "OfficialExample" / "dlls" / "main.prx", fs::copy_options::overwrite_existing);
        fs::create_directories(mods / "LegacyMod" / "dlls");
        fs::copy_file(build / "legacy.dylib", mods / "LegacyMod" / "dlls" / "main.prx", fs::copy_options::overwrite_existing);
        write_file(mods / "DisabledMod" / "dlls" / "main.prx", "MZ\x90 not a module");
        write_file(mods / "LuaOnly" / "Scripts" / "main.lua", "print('x')");
        fs::create_directories(mods / "shared" / "dlls");
        write_file(mods / "FileNotDir" / "dlls", "x");
        fs::create_directories(mods / "EmptyDlls" / "dlls");
        write_file(mods / "mods.txt", "this file is on disk only; the snapshot copy is what counts");
    }
    std::vector<std::string> listed;
    size_t scanned{};
    std::string why;
    c.ok(list_dirs_with_dlls(mods, listed, scanned, why), "listing succeeded");
    {
        std::string all;
        for (auto& n : listed) all += n + ",";
        c.ok(all == "DisabledMod,EmptyDlls,LegacyMod,NativeLifecycleProbe,OfficialExample,shared,", "only folders with a dlls/ DIRECTORY, sorted (got " + all + ")");
        c.ok(scanned == 9, "every entry scanned once (" + std::to_string(scanned) + ")");
    }
    {
        std::vector<std::string> l2;
        size_t s2{};
        std::string w2;
        c.ok(!list_dirs_with_dlls(root / "missing", l2, s2, w2) && w2.find("cannot list") != std::string::npos, "missing directory: refused, no throw");
        auto inv = discover_cpp_mods("ue4ss/Mods", std::nullopt, {});
        c.ok(inv.start_order.empty() && has_note(inv, "absolute mods directory"), "relative mods directory refused (no C++ mod)");
        const fs::path many = build / "many";
        fs::remove_all(many);
        for (int i = 0; i < 300; ++i) fs::create_directories(many / ("m" + std::to_string(i)));
        auto big = discover_cpp_mods(many, std::nullopt, [](std::string_view) { return true; });
        c.ok(big.start_order.empty() && has_note(big, "more than 256 entries"), "over 256 entries refused (bounded)");
        fs::remove_all(many);
    }

    std::printf("== [F1] dlls/ lookup never throws a filesystem error ==\n");
    {
        const fs::path locked = build / "f1-locked";
        fs::remove_all(locked);
        fs::create_directories(locked / "dlls");
        write_file(locked / "dlls" / "main.prx", "x");
        fs::permissions(locked / "dlls", fs::perms::none); // no search permission: stat() on dlls/main.prx fails with EACCES
        bool threw = false;
        std::error_code ec;
        std::optional<fs::path> found;
        try { found = FindNativeCppModModule(locked / "dlls", "M", &ec); }
        catch (...) { threw = true; }
        c.ok(!threw && !found && ec && ec.value() == EACCES, "permission error: no throw, no module, error reported (errno " + std::to_string(ec.value()) + ")");
        threw = false;
        try { found = FindNativeCppModModule(locked / "dlls", "M"); }
        catch (...) { threw = true; }
        c.ok(!threw && !found, "permission error without an error out-parameter: still no throw, no module");
        bool old_throws = false;
        try { (void)fs::is_regular_file(locked / "dlls" / "main.prx"); }
        catch (const fs::filesystem_error&) { old_throws = true; }
        c.ok(old_throws, "control: the throwing overload the port header used DOES throw here (the F1 failure mode is real on this host)");
        fs::permissions(locked / "dlls", fs::perms::owner_all);
        ec.clear();
        found = FindNativeCppModModule(locked / "dlls", "M", &ec);
        c.ok(found && !ec && found->filename() == "main.prx", "readable again: main.prx found, no error");
        fs::remove(locked / "dlls" / "main.prx");
        fs::create_symlink(locked / "nowhere.prx", locked / "dlls" / "main.prx");
        ec.clear();
        found = FindNativeCppModModule(locked / "dlls", "M", &ec);
        c.ok(!found && !ec, "dangling link is 'not found', not an error");
        write_file(locked / "dlls" / "M.prx", "x");
        ec.clear();
        found = FindNativeCppModModule(locked / "dlls", "M", &ec);
        c.ok(found && !ec && found->filename() == "M.prx", "missing main.prx / main.sprx are skipped (ENOENT is not an error): <Name>.prx found");
        fs::remove_all(locked);
    }

    std::printf("== the staged startup path: discover -> load (CppMod ctor) -> start in order ==\n");
    FakeEngine e;
    e.f.ufunction_thunks_ready = true;
    FakeObject object_class{"Object", "Class /Script/CoreUObject.Object", "Class", {"Class"}, {}};
    e.objects = {&object_class};
    Host host{e, "startup-test"};
    e.host = &host;
    // snapshot facts: mods.txt row for OfficialExample; enabled.txt for the probe,
    // the legacy mod and EmptyDlls; none for DisabledMod.
    const std::set<std::string> snapshot_markers{"NativeLifecycleProbe", "LegacyMod", "EmptyDlls"};
    const std::string snapshot_mods_txt = "OfficialExample : 1\nDisabledMod : 0\nLuaOnly : 1\n";
    auto inv = discover_cpp_mods(mods, std::string_view(snapshot_mods_txt), [&](std::string_view n) { return snapshot_markers.count(std::string(n)) != 0; });
    c.ok(names(inv) == "OfficialExample,EmptyDlls,LegacyMod,NativeLifecycleProbe", "planned start order (got " + names(inv) + ")");
    c.ok(has_note(inv, "C++ mod DisabledMod has dlls/ but is not enabled") && has_note(inv, "C++ mod scan: 9 entries, 6 with dlls/, 4 enabled"),
         "disabled mod reported; scan summary line");
    // setup_mods: one CppMod per planned mod (its constructor loads the module)
    struct Loaded
    {
        std::string name;
        std::unique_ptr<CppModModule> module;
        mbm_mod_entry_v1_fn entry{};
        std::string error;
    };
    std::vector<Loaded> loaded;
    std::vector<std::string> log;
    for (auto& cand : inv.start_order)
    {
        Loaded l{cand.name, nullptr, nullptr, {}};
        try
        {
            std::error_code lookup_error;
            const auto file = FindNativeCppModModule(cand.mod_dir / "dlls", cand.name, &lookup_error);
            if (lookup_error) throw std::runtime_error("Native C++ mod dlls folder could not be checked (OS error " + std::to_string(lookup_error.value()) + ")");
            if (!file) throw std::runtime_error("Native C++ mod dlls folder contains no PRX, SPRX, or legacy module candidate");
            auto m = CppModModule::Open(*file);
            auto sel = select_entry(*m);
            if (sel.kind != EntryKind::AbiV1) throw std::runtime_error(sel.reason);
            l.entry = sel.entry;
            l.module = std::move(m);
        }
        catch (const std::exception& ex)
        {
            l.error = ex.what();
            log.push_back("Native C++ mod " + cand.name + " could not be loaded: " + l.error);
        }
        loaded.push_back(std::move(l));
    }
    c.ok(loaded.size() == 4, "one CppMod per planned mod, DisabledMod never opened");
    c.ok(loaded[1].error.find("no PRX") != std::string::npos, "EmptyDlls: not installable, reason logged");
    c.ok(loaded[2].error.find("rebuild it with the PS5 Mod Bridge native mod SDK") != std::string::npos, "LegacyMod (UE4SS C++ ABI): refused with rebuild text");
    // start_mods<CppMod>: installed ones in order; a refusal never stops the rest
    std::vector<std::string> started;
    std::map<std::string, uint64_t> tokens;
    for (auto& l : loaded)
    {
        if (!l.entry) continue; // not installable -> "[NMOD] <name>: not started"
        uint64_t t{};
        if (host.attach(l.name, l.entry, &t) == MBM_OK)
        {
            started.push_back(l.name);
            tokens[l.name] = t;
        }
    }
    c.ok(started == std::vector<std::string>{"OfficialExample", "NativeLifecycleProbe"}, "both ABI-v1 mods started, in planned order, despite two refusals before the probe");
    c.ok(e.count("MyAwesomeMod says hello") == 1 && e.count("[NPROBE] constructed") == 1, "both constructors ran");
    c.ok(log.size() == 2, "exactly two load-refusal lines");
    // later lifecycle (init(): fire_unreal_init_for_cpp_mods; on_program_start)
    host.fire(tokens["NativeLifecycleProbe"], Event::UnrealInit);
    host.fire(tokens["NativeLifecycleProbe"], Event::ProgramStart);
    c.ok(e.count("[NPROBE] on_unreal_init object=Class /Script/CoreUObject.Object") == 1 && e.count("[NPROBE] on_program_start") == 1,
         "started probe receives the later lifecycle events");
    host.shutdown();
    for (auto& l : loaded)
        if (l.module) l.module->Close();
    fs::remove_all(root);
    return c.finish("startup_path");
}
