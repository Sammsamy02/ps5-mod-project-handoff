// test_gui_sdk_mods.cpp -- #11 cross-module host test: SDK-built mod dylibs that
// link their OWN copy of Dear ImGui share the host's context and allocator
// (UE4SS_ENABLE_IMGUI), render a tab inside the host frame, call all 76 ImGui
// functions of the selected real mods, react to pad navigation, and fail closed
// when their ImGui build does not match the host. Loaded through the runtime's
// real CppModModule (POSIX branch). Mac-only.
#include "../../cap09/tests/fake_engine.hpp"
#include "soft_raster.hpp"

#include <Platform/CppModModule.hpp>
#include <Platform/NativeGuiHost.hpp>
#include <Platform/NativeModLoaderPolicy.hpp>

#include <filesystem>

using namespace RC::Platform;
using namespace RC::Platform::NativeGui;
using namespace RC::Platform::NativeMods;

static int g_host_asserts = 0;
void mbm_imgui_assert_failed(const char* expr, const char* file, int line)
{
    ++g_host_asserts;
    std::printf("    [host imgui assert] %s at %s:%d\n", expr, file, line);
}

struct RecBackend final : Backend
{
    int w = 1920, h = 1080;
    bool visible{}, blocked{};
    std::vector<uint8_t> atlas;
    int aw{}, ah{};
    uint64_t submits{}, tris{};
    SoftImage last{1, 1};
    std::vector<std::string> lines;
    void log(int32_t, std::string_view t) override
    {
        lines.emplace_back(t);
        std::printf("    gui: %.*s\n", static_cast<int>(t.size()), t.data());
    }
    auto prepare(int* W, int* H, bool* rebuilt) -> bool override
    {
        *rebuilt = !built;
        built = true;
        *W = w;
        *H = h;
        return true;
    }
    bool built{};
    auto create_atlas(const std::vector<Run>& r, int W, int H) -> ImTextureID override
    {
        atlas = rasterise(r, W, H);
        aw = W;
        ah = H;
        return 1;
    }
    void submit(const std::vector<Batch>& b) override
    {
        ++submits;
        last = SoftImage(w, h);
        SoftTexture t{aw, ah, &atlas};
        for (auto& x : b) tris += x.tris.size();
        soft_batches(last, b, t);
    }
    void set_visible(bool v) override { visible = v; }
    auto set_gameplay_input_blocked(bool b) -> bool override
    {
        blocked = b;
        return true;
    }
};

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Checks c;
    if (argc < 2) return 2;
    const std::filesystem::path dir = std::filesystem::absolute(argv[1]);

    FakeEngine e;
    e.f.ufunction_thunks_ready = true;
    FakeObject pawn{"BP_Player_C_0", "BP_Player_C /Game/L.L:PersistentLevel.BP_Player_C_0", "BP_Player_C", {"BP_Player_C", "Pawn"}, {}};
    pawn.props["CustomTimeDilation"] = FakeProp{MBM_VT_F32};
    pawn.props["CustomTimeDilation"].f32 = 1.0f;
    FakeObject pc{"PlayerController_0", "PlayerController /Game/L.L:PersistentLevel.PlayerController_0", "PlayerController", {"PlayerController"}, {}};
    pc.props["Pawn"] = FakeProp{MBM_VT_OBJECT};
    pc.props["Pawn"].object = &pawn;
    e.objects = {&pawn, &pc};
    Host mods{e, "gui-sdk-test"};
    e.host = &mods;
    RecBackend be;
    GuiHost gui{mods, be};
    mods.set_gui(gui.table()); // #11 plugs into #9 before any mod is attached

    auto load = [&](const char* file, const char* name, uint64_t* tok, std::unique_ptr<CppModModule>& owner) {
        owner = CppModModule::Open(dir / file);
        auto sel = select_entry(*owner);
        return sel.kind == EntryKind::AbiV1 && mods.attach(name, sel.entry, tok) == MBM_OK;
    };

    std::printf("== NativeGuiProbe (own ImGui copy, shared host context) ==\n");
    std::unique_ptr<CppModModule> m_probe, m_bad, m_noimgui, m_badctx;
    uint64_t t_probe{}, t_bad{}, t_noimgui{}, t_badctx{};
    c.ok(load("NativeGuiProbe.dylib", "NativeGuiProbe", &t_probe, m_probe), "probe attached");
    c.ok(e.count("[NGUI] constructed") == 1, "probe constructed (registered its tab in the constructor)");
    c.ok(load("NativeGuiProbe_badlayout.dylib", "BadLayout", &t_bad, m_bad), "mismatched-ImGui-layout probe attached");
    c.ok(load("NoImGui.dylib", "NoImGui", &t_noimgui, m_noimgui), "mod built without SDK ImGui attached");
    c.ok(load("NativeGuiProbe_badctx.dylib", "BadContext", &t_badctx, m_badctx), "[F7] other-ImGui-config probe attached");
    c.ok(gui.tab_count() == 4, "four tabs registered before the GUI host starts");

    c.ok(gui.start(), "GUI host started");
    const auto ui_inits = mods.fire_all(Event::UiInit);
    c.ok(ui_inits == 4, "on_ui_init fired on every running mod");
    c.ok(e.count("[NGUI] ui_init imgui=ok") == 1, "probe shared the host ImGui context (version + layout fingerprint matched)");
    c.ok(e.count("[NGUI] ui_init imgui=refused") == 2 && e.count("layout differs from the mod's") == 1,
         "mismatched layout refused gracefully (no ImGui abort)");
    c.ok(e.count("BadContext: UE4SS_ENABLE_IMGUI refused: host ImGuiContext size") == 1 && e.count("BadContext: UE4SS_ENABLE_IMGUI refused: host Dear ImGui") == 0,
         "[F7] other ImGui config: passes the old six-size check, refused by the context-size / config-fingerprint check");
    c.ok(gui.table()->sizeof_context == sizeof(ImGuiContext) && gui.table()->config_fingerprint == MBM_IMGUI_CONFIG_FINGERPRINT() && gui.table()->config_fingerprint != 0,
         "[F7] host table publishes sizeof(ImGuiContext) and the config fingerprint");

    std::printf("== frames ==\n");
    gui.post_key(Key::Toggle);
    for (int i = 0; i < 16; ++i) gui.frame(1.0f / 60.0f);
    c.ok(be.visible && be.blocked && be.submits == 16, "GUI open, 16 frames submitted");
    c.ok(e.count("[NGUI] frame n=1") == 1, "probe tab rendered inside the host frame");
    c.ok(e.count("[NGUI] coverage 76/76 distinct ImGui functions exercised") == 1, "all 76 real-mod ImGui functions exercised across the module boundary");
    c.ok(e.count("[SDK] imgui assert") == 0 && g_host_asserts == 0, "zero ImGui asserts on either side");
    c.ok(soft_coverage(be.last) > 0.02, "frame visibly drew the tab (software raster coverage > 2%)");

    std::printf("== pad navigation presses the probe's button -> game-thread action ==\n");
    int attempts = 0;
    for (int i = 0; i < 8 && e.count("[NGUI] button speed=") == 0; ++i)
    {
        ++attempts;
        gui.post_key(Key::Activate);
        gui.frame(1.0f / 60.0f);
        gui.frame(1.0f / 60.0f);
        mods.game_thread_frame(1.0f / 60.0f, PumpSource::GameThreadPump);
        if (e.count("[NGUI] button speed=") == 0)
        {
            gui.post_key(Key::Down);
            gui.frame(1.0f / 60.0f);
        }
    }
    std::printf("    activate attempts=%d (1 = the button had the default nav focus)\n", attempts);
    c.ok(e.count("[NGUI] button speed=2.00 write=true") == 1 && pawn.props["CustomTimeDilation"].f32 == 2.0f,
         "Activate on the focused button doubled the player speed (via execute_in_game_thread)");

    std::printf("== the two refused mods: their tabs fault closed, probe keeps working ==\n");
    // make each refused tab the visible one by stopping the probe temporarily is not
    // needed: select tabs with PrevTab/NextTab navigation.
    for (int i = 0; i < 8; ++i)
    {
        gui.post_key(Key::NextTab);
        gui.frame(1.0f / 60.0f);
        gui.frame(1.0f / 60.0f);
    }
    c.ok(mods.state(t_bad) == ModState::Faulted || mods.state(t_noimgui) == ModState::Faulted, "a refused mod's tab render faulted and was disabled");
    c.ok(mods.state(t_badctx) == ModState::Faulted, "[F7] the other-config mod's tab render was refused (mod disabled), host context untouched");
    c.ok(e.count("tab render refused: mod could not share the host ImGui context") >= 1, "refusal reason logged");
    c.ok(mods.state(t_probe) == ModState::Running, "probe still running");

    std::printf("== close / reopen / stop ==\n");
    gui.post_key(Key::Toggle);
    gui.frame(0.016f);
    c.ok(!be.visible && !be.blocked, "closed: overlay hidden, gameplay input restored");
    gui.post_key(Key::Toggle);
    for (int i = 0; i < 3; ++i) gui.frame(0.016f);
    c.ok(be.visible, "reopened");
    mods.shutdown();
    c.ok(e.count("NativeGuiProbe: [NGUI] destroyed frames=") == 1, "probe destroyed via uninstall_mod after revoke + drain");
    c.ok(gui.tab_count() == 0, "all tabs revoked");
    for (int i = 0; i < 3; ++i) gui.frame(0.016f);
    c.ok(g_host_asserts == 0, "host frames after every mod stopped: no assert");
    m_probe->Close();
    m_bad->Close();
    m_noimgui->Close();
    m_badctx->Close();
    for (auto& l : e.lines)
        if (l.find("destroy") != std::string::npos || l.find("stopped") != std::string::npos || l.find("fault") != std::string::npos ||
            l.find("refused") != std::string::npos)
            std::printf("    host-log %s\n", l.c_str());
    // print the coverage line for the python coverage gate
    for (auto& l : e.lines)
        if (l.find("[NGUI] coverage-list") != std::string::npos) std::printf("COVERAGE-LINE %s\n", l.c_str());
    return c.finish("gui_sdk_mods");
}
