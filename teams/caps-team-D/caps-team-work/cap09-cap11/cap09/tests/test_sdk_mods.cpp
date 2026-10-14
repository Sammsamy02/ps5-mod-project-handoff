// test_sdk_mods.cpp -- end-to-end host test: real module loader + SDK-built mods.
//
// Loads mods built as Mac dylibs from the SDK through the runtime's REAL
// native module owner (CppModModule.cpp, its POSIX dlopen branch, compiled
// unchanged from the loader tree), selects the entry with the staged policy,
// attaches them to the staged host core with a fake engine, and drives the
// whole lifecycle. Mac-only: proves the ABI/SDK/host logic, not PS5 loading.
#include "fake_engine.hpp"

#include <Platform/CppModModule.hpp>
#include <Platform/NativeModLoaderPolicy.hpp>

#include <filesystem>
#include <fstream>

using namespace RC::Platform;
using namespace RC::Platform::NativeMods;

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Checks c;
    if (argc < 2)
    {
        std::printf("usage: test_sdk_mods <build-dir>\n");
        return 2;
    }
    const fs::path dir = fs::absolute(argv[1]);

    FakeEngine e;
    e.f.ufunction_thunks_ready = true;
    FakeObject object_class{"Object", "Class /Script/CoreUObject.Object", "Class", {"Class"}, {}};
    FakeObject pawn{"BP_Player_C_0", "BP_Player_C /Game/Maps/Level.Level:PersistentLevel.BP_Player_C_0", "BP_Player_C",
                    {"BP_Player_C", "Character", "Pawn", "Actor"}, {}};
    pawn.props["CustomTimeDilation"] = FakeProp{MBM_VT_F32};
    pawn.props["CustomTimeDilation"].f32 = 1.0f;
    FakeObject pc{"PlayerController_0", "PlayerController /Game/Maps/Level.Level:PersistentLevel.PlayerController_0", "PlayerController",
                  {"PlayerController", "Controller", "Actor"}, {}};
    pc.props["Pawn"] = FakeProp{MBM_VT_OBJECT};
    pc.props["Pawn"].object = &pawn;
    e.objects = {&object_class, &pawn, &pc};
    Host host{e, "test-host"};
    e.host = &host;

    std::printf("== loader: refusals ==\n");
    {
        const auto pe = dir / "pe_mod.dll";
        {
            std::ofstream f(pe, std::ios::binary);
            f << "MZ\x90\x00 fake windows module";
        }
        bool refused = false;
        std::string what;
        try { CppModModule::Open(pe); }
        catch (const std::exception& ex) { refused = true; what = ex.what(); }
        c.ok(refused && what.find("Windows PE") != std::string::npos, "Windows PE mod refused by the real module owner");

        auto legacy = CppModModule::Open(dir / "legacy.dylib");
        auto sel = select_entry(*legacy);
        c.ok(sel.kind == EntryKind::LegacyCppAbi && sel.reason.find("rebuild it with the PS5 Mod Bridge native mod SDK") != std::string::npos,
             "legacy UE4SS C++ ABI module refused with rebuild instruction");
        legacy->Close();
    }

    std::printf("== official UE4SS guide example, unchanged source ==\n");
    uint64_t t_official{};
    std::unique_ptr<CppModModule> m_official = CppModModule::Open(dir / "official_example.dylib");
    {
        auto sel = select_entry(*m_official);
        c.ok(sel.kind == EntryKind::AbiV1 && sel.entry, "official example exposes mbm_mod_entry_v1 via the SDK");
        c.ok(host.attach("MyAwesomeMod", sel.entry, &t_official) == MBM_OK, "official example attaches");
        c.ok(e.count("MyAwesomeMod says hello") == 1, "constructor Output::send<Verbose> reached the host log");
        c.ok(e.count("[" + std::to_string(MBM_LOG_VERBOSE) + "] MyAwesomeMod: MyAwesomeMod says hello") == 1, "log level Verbose preserved");
        c.ok(e.count("name='MyAwesomeMod' version='1.0'") == 1, "metadata crossed the ABI as UTF-8");
        host.fire(t_official, Event::UnrealInit);
        c.ok(e.count("Object Name: Class /Script/CoreUObject.Object") == 1, "on_unreal_init StaticFindObject + GetFullName through the host");
    }

    std::printf("== NativeLifecycleProbe (the #9 launch mod) ==\n");
    uint64_t t_probe{};
    std::unique_ptr<CppModModule> m_probe = CppModModule::Open(dir / "NativeLifecycleProbe.dylib");
    {
        auto sel = select_entry(*m_probe);
        c.ok(sel.kind == EntryKind::AbiV1, "probe entry selected");
        c.ok(host.attach("NativeLifecycleProbe", sel.entry, &t_probe) == MBM_OK, "probe attaches");
        c.ok(e.count("[NPROBE] constructed") == 1, "probe constructed");
        host.fire(t_probe, Event::ProgramStart);
        host.fire(t_probe, Event::UnrealInit);
        c.ok(e.count("[NPROBE] on_program_start") == 1, "on_program_start delivered");
        c.ok(e.count("[NPROBE] on_unreal_init object=Class /Script/CoreUObject.Object") == 1, "probe reflected lookup");
        c.ok(e.count("[NPROBE] admission family=") == MBM_HOOK_FAMILY_COUNT - 1, "probe printed the full admission table (17 families)");
        c.ok(e.count("[NPROBE] admission family=process-event route=REFUSED live=0 via=code-detour-only") == 1, "global ProcessEvent reported REFUSED");
        c.ok(e.count("[NPROBE] admission family=ufunction route=DATA_ONLY_PARTIAL live=1") == 1, "per-UFunction hook reported data-only live");
        c.ok(e.count("[NPROBE] RegisterHook ClientRestart id=") == 1 && e.count("id=-1") == 0, "RegisterHook admitted");
        c.ok(e.count("[NPROBE] game-thread tick registered=true") == 1, "tick registered");
        c.ok(e.keys.size() == 1 && e.keys.begin()->second.first == 0x78, "F9 key bind installed through the host");
        // engine fires the hooked function twice (map load)
        e.fire_hook("/Script/Engine.PlayerController:ClientRestart", &pc, nullptr);
        e.fire_hook("/Script/Engine.PlayerController:ClientRestart", &pc, nullptr);
        c.ok(e.count("[NPROBE] hook ClientRestart post n=2 context=PlayerController_0") == 1, "per-UFunction post hook reached the mod with context");
        host.game_thread_frame(0.016f, PumpSource::GameThreadPump);
        c.ok(e.count("[NPROBE] game-thread tick n=1") == 1, "game-thread tick reached the mod");
        e.press(0x78);
        c.ok(e.count("[NPROBE] key F9") == 0, "key handler only queued a game-thread task");
        host.game_thread_frame(0.016f, PumpSource::GameThreadPump);
        c.ok(e.count("[NPROBE] key F9 speed=2.00 write=true readback=2.00 pawn=BP_Player_C") == 1, "F9 doubled the pawn's CustomTimeDilation");
        c.ok(pawn.props["CustomTimeDilation"].f32 == 2.0f, "engine-side property actually written");
        e.press(0x78);
        host.game_thread_frame(0.016f, PumpSource::GameThreadPump);
        c.ok(pawn.props["CustomTimeDilation"].f32 == 1.0f, "second F9 restores normal speed");
        host.fire(t_probe, Event::Update);
        host.fire(t_probe, Event::Update);
        c.ok(e.count("[NPROBE] on_update n=1") == 1, "on_update delivered (logged once)");
    }

    std::printf("== throwing mod: contained, disabled, host keeps running ==\n");
    uint64_t t_throw{};
    std::unique_ptr<CppModModule> m_throw = CppModModule::Open(dir / "throwing.dylib");
    {
        auto sel = select_entry(*m_throw);
        c.ok(host.attach("Thrower", sel.entry, &t_throw) == MBM_OK, "throwing mod attaches");
        const auto s = host.fire(t_throw, Event::Update);
        c.ok(s == MBM_E_MOD_FAULT, "C++ exception became MBM_E_MOD_FAULT at the boundary");
        c.ok(e.count("[NMOD] fault in on_update status=-10 error=deliberate on_update failure") == 1, "fault logged with the exception text");
        c.ok(host.state(t_throw) == ModState::Faulted, "throwing mod disabled");
        c.ok(host.fire(t_probe, Event::Update) == MBM_OK, "other mods unaffected");
    }

    std::printf("== shutdown: revoke, drain, destroy ==\n");
    host.shutdown();
    c.ok(e.count("[NPROBE] destroyed hook_hits=2 ticks=3") == 1, "probe destructor ran via uninstall_mod");
    c.ok(host.state(t_probe) == ModState::Stopped && host.state(t_official) == ModState::Stopped && host.state(t_throw) == ModState::Stopped,
         "every mod stopped");
    c.ok(e.removed_hooks.size() == 1, "probe's engine hook removed at stop");
    e.press(0x78);
    host.game_thread_frame(0.016f, PumpSource::GameThreadPump);
    c.ok(e.count("[NPROBE] key F9") == 2, "no callback reaches a stopped mod");
    // Modules stay resident (runtime policy); the test closes them only after stop.
    m_probe->Close();
    m_official->Close();
    m_throw->Close();
    return c.finish("sdk_mods");
}
