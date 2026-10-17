// test_gui_host.cpp -- host tests for #11: batcher, atlas runs, PNG writer and the
// GUI host (real Dear ImGui 1.92.1 compiled with mbm_imconfig.h) on top of the
// #9 native mod host. In-process "mods" are C tables whose tab callbacks call
// ImGui directly. Mac-only.
#include "../../cap09/tests/fake_engine.hpp"
#include "soft_raster.hpp"

#include <Platform/NativeGuiHost.hpp>
#include <Platform/StoredPng.hpp>

#include <atomic>
#include <fstream>
#include <random>
#include <thread>

using namespace RC::Platform;
using namespace RC::Platform::NativeGui;
using namespace RC::Platform::NativeMods;

static int g_host_asserts = 0;
void mbm_imgui_assert_failed(const char* expr, const char* file, int line)
{
    ++g_host_asserts;
    std::printf("    [host imgui assert] %s at %s:%d\n", expr, file, line);
}

struct SoftBackend final : Backend
{
    int w = 1280, h = 720;
    bool visible{}, blocked{};
    int visible_changes{}, block_changes{};
    bool fail_atlas{};
    std::vector<uint8_t> atlas;
    int aw{}, ah{};
    size_t runs{};
    uint64_t submits{}, tris{};
    SoftImage last{1, 1};
    std::vector<std::string> lines;
    bool rasterise = true;
    void log(int32_t, std::string_view t) override { lines.emplace_back(t); }
    // Engine model for F1/F2: the player controller may not exist yet
    // (pc_ready=false makes prepare fail, as the real backend does while it
    // cannot resolve one); a map change (rebuild_next) destroys the widget
    // tree, the atlas texture and the player, and a NEW player starts with an
    // ignore-input counter of 0. SetIgnoreMove/LookInput is a counter in UE.
    bool pc_ready = true, built = false, rebuild_next = false, atlas_valid = false;
    int player = 1, ignore_count = 0, blocked_player = 0;
    int atlas_creates{}, block_true_calls{}, unmatched_false{};
    uint64_t blank_submits{}, good_submits{};
    auto prepare(int* W, int* H, bool* rebuilt) -> bool override
    {
        *rebuilt = false;
        if (!pc_ready) return false;
        if (!built || rebuild_next)
        {
            if (built)
            {
                ++player;          // the old player is gone with its counter
                ignore_count = 0;
                blocked_player = 0;
                atlas_valid = false; // the old atlas texture died with the old widget tree
            }
            built = true;
            rebuild_next = false;
            *rebuilt = true;
        }
        *W = w;
        *H = h;
        return true;
    }
    auto create_atlas(const std::vector<Run>& r, int W, int H) -> ImTextureID override
    {
        if (fail_atlas) return 0;
        ++atlas_creates;
        atlas_valid = true;
        atlas = rasterise_runs(r, W, H);
        aw = W;
        ah = H;
        runs = r.size();
        return 1;
    }
    static auto rasterise_runs(const std::vector<Run>& r, int W, int H) -> std::vector<uint8_t> { return NativeGui::rasterise(r, W, H); }
    void submit(const std::vector<Batch>& b) override
    {
        ++submits;
        if (!atlas_valid)
        {
            ++blank_submits; // the real backend draws nothing without a live atlas
            return;
        }
        ++good_submits;
        for (auto& x : b) tris += x.tris.size();
        if (!rasterise) return;
        last = SoftImage(w, h);
        SoftTexture t{aw, ah, &atlas};
        soft_batches(last, b, t);
    }
    void set_visible(bool v) override { visible = v; ++visible_changes; }
    auto set_gameplay_input_blocked(bool b) -> bool override
    {
        ++block_changes;
        if (b)
        {
            ++block_true_calls;
            if (!pc_ready) return false; // nothing to apply it to
            if (blocked_player == player) return true;
            ++ignore_count;
            blocked_player = player;
            blocked = true;
            return true;
        }
        if (blocked_player == player && blocked_player != 0) --ignore_count;
        else ++unmatched_false; // an unblock with no applied block would cancel the game's own
        blocked_player = 0;
        blocked = false;
        return true;
    }
    auto count(const std::string& n) const -> int
    {
        int c = 0;
        for (auto& l : lines) c += l.find(n) != std::string::npos;
        return c;
    }
};

// ---------------- in-process mods ----------------
struct TabMod
{
    const mbm_host_v1* host{};
    uint64_t token{};
    mbm_mod_v1 table{};
    int renders{};
    int clicks{};
    int mode{}; // 0 normal, 1 fault, 2 unbalanced, 3 self-unregister, 4 block until released, 5 image with unknown texture
    uint64_t tab_id{};
    std::atomic<bool> in_render{false};
    std::atomic<bool> release{false};
    bool destroyed{};
};

static TabMod* g_next{};
static mbm_status tab_render(void* user)
{
    auto& m = *static_cast<TabMod*>(user);
    ++m.renders;
    ImGui::TextUnformatted("in-process tab");
    if (ImGui::Button("Click me")) ++m.clicks;
    switch (m.mode)
    {
    case 1: return MBM_E_MOD_FAULT;
    case 2:
        ImGui::BeginChild("leaked child");
        ImGui::PushID("leaked id");
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
        return MBM_OK; // forgot EndChild / PopID / PopStyleColor
    case 3: m.host->gui->unregister_tab(m.host->gui->ctx, m.token, m.tab_id); return MBM_OK;
    case 4:
        m.in_render = true;
        while (!m.release) std::this_thread::yield();
        return MBM_OK;
    case 5: ImGui::Image(ImTextureRef(static_cast<ImTextureID>(1234)), ImVec2(32, 32)); return MBM_OK;
    }
    return MBM_OK;
}
static mbm_status destroy_mod(void* i) { static_cast<TabMod*>(i)->destroyed = true; return MBM_OK; }
static mbm_status entry_tabmod(const mbm_host_v1* host, uint64_t token, const mbm_mod_v1** out)
{
    auto& m = *g_next;
    m.host = host;
    m.token = token;
    m.table.magic = MBM_ABI_MAGIC;
    m.table.abi_version = MBM_ABI_VERSION;
    m.table.struct_size = sizeof(mbm_mod_v1);
    m.table.name = "tabmod";
    m.table.instance = &m;
    m.table.destroy = &destroy_mod;
    if (host->gui->register_tab(host->gui->ctx, token, "Tab", &tab_render, &m, &m.tab_id) != MBM_OK) return MBM_E_MOD_FAULT;
    *out = &m.table;
    return MBM_OK;
}

static void frames(GuiHost& g, int n)
{
    for (int i = 0; i < n; ++i) g.frame(1.0f / 60.0f);
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Checks c;

    std::printf("== batcher: CPU clipping vs scissor (Monte Carlo) ==\n");
    {
        std::mt19937 rng(12345);
        std::uniform_real_distribution<float> d(-50.f, 150.f);
        int mismatches = 0, samples = 0, tri_cases = 0;
        for (int it = 0; it < 2000; ++it)
        {
            // build a fake single-command draw list via ImDrawData from raw arrays
            ImDrawVert v[3];
            for (auto& x : v)
            {
                x.pos = ImVec2(d(rng), d(rng));
                x.uv = ImVec2(0, 0);
                x.col = IM_COL32(255, 255, 255, 255);
            }
            // clip rect 0..100
            const float rx0 = 0, ry0 = 0, rx1 = 100, ry1 = 100;
            // emulate convert() on one triangle using the same internals
            std::vector<NativeGui::detail::V> poly(3), tmp;
            for (int k = 0; k < 3; ++k) poly[k] = NativeGui::detail::V{v[k].pos.x, v[k].pos.y, 0, 0, {1, 1, 1, 1}};
            NativeGui::detail::clip_plane(poly, tmp, 0, rx0, +1);
            if (poly.size() >= 3) NativeGui::detail::clip_plane(poly, tmp, 0, rx1, -1);
            if (poly.size() >= 3) NativeGui::detail::clip_plane(poly, tmp, 1, ry0, +1);
            if (poly.size() >= 3) NativeGui::detail::clip_plane(poly, tmp, 1, ry1, -1);
            ++tri_cases;
            auto inside_tri = [&](float px, float py, const ImVec2& a, const ImVec2& b, const ImVec2& cc) {
                const float d1 = (px - b.x) * (a.y - b.y) - (a.x - b.x) * (py - b.y);
                const float d2 = (px - cc.x) * (b.y - cc.y) - (b.x - cc.x) * (py - cc.y);
                const float d3 = (px - a.x) * (cc.y - a.y) - (cc.x - a.x) * (py - a.y);
                const bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0), pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
                return !(neg && pos);
            };
            for (int s = 0; s < 60; ++s)
            {
                const float px = d(rng), py = d(rng);
                const bool expect = inside_tri(px, py, v[0].pos, v[1].pos, v[2].pos) && px >= rx0 && px <= rx1 && py >= ry0 && py <= ry1;
                bool got = false;
                for (size_t k = 1; k + 1 < poly.size(); ++k)
                    got = got || inside_tri(px, py, ImVec2(poly[0].x, poly[0].y), ImVec2(poly[k].x, poly[k].y), ImVec2(poly[k + 1].x, poly[k + 1].y));
                ++samples;
                if (got != expect) ++mismatches;
            }
        }
        std::printf("    clip cases=%d samples=%d mismatches=%d\n", tri_cases, samples, mismatches);
        c.ok(samples == 120000 && mismatches <= samples / 2000, "clipped polygon == triangle ∩ clip rect (120000 random samples, edge noise only)");
    }

    // A real ImGui context for the remaining tests (same config as production).
    FakeEngine eng;
    Host mods{eng, "gui-test"};
    eng.host = &mods;
    SoftBackend be;
    GuiHost gui{mods, be};
    mods.set_gui(gui.table());

    std::printf("== GUI host start: context, atlas, fingerprint ==\n");
    c.ok(gui.start(), "GUI host started (context + atlas built on CPU)");
    c.ok(gui.context() != nullptr && be.count("[GUI] context up imgui=1.92.1") == 1, "context created and logged");
    const mbm_gui_v1* t = gui.table();
    c.ok(t->magic == MBM_GUI_MAGIC && t->imgui_version_num == IMGUI_VERSION_NUM && t->sizeof_io == sizeof(ImGuiIO) && t->sizeof_drawvert == sizeof(ImDrawVert),
         "gui table fingerprint filled");
    c.ok(t->get_context(t->ctx) == gui.context(), "get_context returns the host context");
    mbm_gui_alloc_fn af{};
    mbm_gui_free_fn ff{};
    void* au{};
    t->get_allocators(t->ctx, &af, &ff, &au);
    c.ok(af && ff, "host allocator exported for mods");

    std::printf("== atlas runs + PNG ==\n");
    {
        ImGui::SetCurrentContext(gui.context());
        unsigned char* px{};
        int w{}, h{};
        ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&px, &w, &h);
        auto runs = atlas_runs(px, w, h, 16);
        auto back = rasterise(runs, w, h);
        size_t nonzero = 0, exact = 0;
        for (int i = 0; i < w * h; ++i)
        {
            nonzero += px[i] != 0;
            exact += back[static_cast<size_t>(i)] == quantise(px[i], 16);
        }
        std::printf("    atlas %dx%d nonzero=%zu runs=%zu\n", w, h, nonzero, runs.size());
        c.ok(exact == static_cast<size_t>(w) * h, "runs rasterise back to the quantised atlas exactly (every pixel)");
        c.ok(runs.size() > 0 && runs.size() < nonzero, "runs are fewer than coverage pixels");
        std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
        for (int i = 0; i < w * h; ++i)
        {
            rgba[i * 4 + 0] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
            rgba[i * 4 + 3] = px[i];
        }
        auto png = encode_png_rgba(rgba.data(), w, h);
        c.ok(png.size() > static_cast<size_t>(w * h * 4) && png[1] == 'P', "PNG encoded");
        std::ofstream("atlas.png", std::ios::binary).write(reinterpret_cast<const char*>(png.data()), static_cast<long>(png.size()));
        std::ofstream("atlas.alpha", std::ios::binary).write(reinterpret_cast<const char*>(px), static_cast<long>(w * h));
        std::printf("    wrote atlas.png (%zu bytes) + atlas.alpha for the independent decoder check\n", png.size());
    }

    std::printf("== closed GUI costs nothing; toggle opens; atlas upload once ==\n");
    frames(gui, 3);
    c.ok(be.submits == 0 && !be.visible, "closed: no frame submitted");
    gui.post_key(Key::Toggle);
    frames(gui, 1);
    c.ok(gui.is_open() && be.visible && be.blocked, "toggle opens: overlay visible, gameplay input blocked");
    c.ok(be.count("[GUI] atlas uploaded runs=") == 1 && be.runs > 0, "atlas uploaded once as runs");
    c.ok(be.submits == 1 && be.tris > 0, "first frame submitted triangles");
    c.ok(be.count("No native mod has registered a GUI tab") == 0, "(text is drawn, not logged)");

    std::printf("== batcher on a real frame: image equals scissor reference ==\n");
    {
        // one more frame with a fresh image; compare to the reference rasteriser on the same draw data
        frames(gui, 1);
        ImGui::SetCurrentContext(gui.context());
        const ImDrawData* dd = ImGui::GetDrawData();
        SoftImage ref(be.w, be.h);
        SoftTexture tex{be.aw, be.ah, &be.atlas};
        soft_reference(ref, dd, tex);
        double mx = 0;
        const double frac = soft_diff(be.last, ref, 0.02f, &mx);
        std::printf("    coverage=%.4f mismatch=%.6f max_err=%.3f\n", soft_coverage(be.last), frac, mx);
        c.ok(soft_coverage(be.last) > 0.01, "frame draws a visible window");
        c.ok(frac < 0.001, "clipped batches == scissored reference (<0.1% edge pixels differ)");
    }

    std::printf("== in-process mod tab: render, nav input, click ==\n");
    TabMod a;
    g_next = &a;
    uint64_t ta{};
    c.ok(mods.attach("tabmod-a", &entry_tabmod, &ta) == MBM_OK, "tab mod attached; tab registered from its constructor");
    c.ok(gui.tab_count() == 1, "one tab registered");
    frames(gui, 2);
    c.ok(a.renders >= 1, "tab render callback runs inside the host frame");
    // drive gamepad navigation: the button is the first navigable item; activate it
    for (int i = 0; i < 6 && a.clicks == 0; ++i)
    {
        gui.post_key(Key::Down);
        frames(gui, 2);
        gui.post_key(Key::Activate);
        frames(gui, 2);
    }
    c.ok(a.clicks >= 1, "pad navigation + Activate pressed the mod's ImGui button");
    gui.post_key(Key::Toggle);
    frames(gui, 1);
    c.ok(!gui.is_open() && !be.visible && !be.blocked, "toggle closes: overlay hidden, gameplay input restored");
    c.ok(be.count("[GUI] closed (gameplay input restored)") == 1, "close logged");
    const int before = a.renders;
    frames(gui, 3);
    c.ok(a.renders == before, "closed GUI does not call tabs");
    gui.post_key(Key::Toggle);
    frames(gui, 2);
    c.ok(gui.is_open() && a.renders > before, "reopen renders again");
    c.ok(be.visible_changes == 3 && be.block_changes == 3, "visibility/input-lock toggled exactly per open/close");

    std::printf("== unbalanced mod (forgot End*/Pop*) is recovered ==\n");
    // Only the SELECTED tab's content renders (ImGui tab bars, same as upstream
    // UE4SS): stop mod a so the next tab becomes the visible one.
    mods.stop(ta);
    TabMod b;
    b.mode = 2;
    g_next = &b;
    uint64_t tb{};
    mods.attach("tabmod-b", &entry_tabmod, &tb);
    const auto rec_before = gui.stats().recovered_frames;
    frames(gui, 3);
    c.ok(gui.stats().recovered_frames >= rec_before + 1, "error recovery re-balanced the ImGui stacks");
    c.ok(mods.state(tb) == ModState::Running, "unbalanced but non-faulting mod keeps running");

    std::printf("== faulting tab: disabled, others keep rendering ==\n");
    TabMod f;
    f.mode = 1;
    g_next = &f;
    uint64_t tf{};
    mods.attach("tabmod-f", &entry_tabmod, &tf);
    // select the faulting tab: ImGui only renders the selected tab item, so
    // render a few frames; tab bar selects the first tab by default, so force
    // the fault through the registry order by making it the only active tab.
    mods.stop(tb);
    frames(gui, 3);
    c.ok(mods.state(tf) == ModState::Faulted && gui.stats().tab_faults >= 1, "tab fault disables the mod");
    c.ok(eng.count("[NMOD] fault in gui tab render") == 1, "tab fault logged once through the #9 host");
    c.ok(gui.tab_count() == 0, "faulted mod's tabs removed");
    frames(gui, 2);
    c.ok(gui.stats().frames > 0 && g_host_asserts == 0, "frames continue, no host ImGui assert");
    c.ok(a.destroyed && b.destroyed, "stopped mods destroyed");

    std::printf("== self-unregister inside render: no deadlock ==\n");
    TabMod s;
    s.mode = 3;
    g_next = &s;
    uint64_t ts{};
    mods.attach("tabmod-s", &entry_tabmod, &ts);
    frames(gui, 3);
    c.ok(s.renders == 1 && gui.tab_count() == 0, "tab unregistered itself during its render (once), next frame gone");

    std::printf("== unknown texture is skipped, counted ==\n");
    TabMod u;
    u.mode = 5;
    g_next = &u;
    uint64_t tu{};
    mods.attach("tabmod-u", &entry_tabmod, &tu);
    frames(gui, 3);
    c.ok(gui.stats().unknown_texture_batches >= 1, "batches with an unknown texture id skipped");

    std::printf("== stop from another thread while the tab renders: drained ==\n");
    mods.stop(tu);
    TabMod k;
    k.mode = 4;
    g_next = &k;
    uint64_t tk{};
    mods.attach("tabmod-k", &entry_tabmod, &tk);
    std::thread game([&] {
        for (int i = 0; i < 20 && !k.in_render; ++i) frames(gui, 1);
    });
    for (int i = 0; i < 2000 && !k.in_render; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    c.ok(k.in_render, "the new tab is rendered on the game thread");
    std::atomic<bool> stopped{false};
    std::thread stopper([&] { mods.stop(tk); stopped = true; });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    c.ok(!stopped && !k.destroyed, "stop waits for the running tab render");
    k.release = true;
    game.join();
    stopper.join();
    c.ok(k.destroyed && gui.tab_count() == 0, "destroy after render returned; tab revoked by the stop listener");

    std::printf("== register_tab validation ==\n");
    uint64_t id{};
    c.ok(t->register_tab(t->ctx, 9999, "x", &tab_render, nullptr, &id) == MBM_E_STATE, "unknown/stopped token refused");
    c.ok(t->register_tab(t->ctx, tk, "x", &tab_render, nullptr, &id) == MBM_E_STATE, "stopped mod refused");

    std::printf("== atlas failure fails closed ==\n");
    {
        FakeEngine e2;
        Host m2{e2};
        e2.host = &m2;
        SoftBackend be2;
        be2.fail_atlas = true;
        GuiHost g2{m2, be2};
        g2.start();
        g2.post_key(Key::Toggle);
        g2.frame(0.016f);
        c.ok(!g2.is_open() && be2.count("[GUI] refused: atlas upload failed") == 1 && be2.submits == 0, "atlas failure: GUI disabled, nothing drawn");
        g2.post_key(Key::Toggle);
        g2.frame(0.016f);
        c.ok(!g2.is_open(), "stays disabled for the session");
    }
    std::printf("== F1: first open before the player controller exists; game-owned block survives ==\n");
    {
        FakeEngine e3;
        Host m3{e3};
        e3.host = &m3;
        SoftBackend be3;
        be3.rasterise = false;
        be3.pc_ready = false;
        be3.ignore_count = 1; // the game holds its own input block (e.g. a cutscene)
        GuiHost g3{m3, be3};
        g3.start();
        g3.post_key(Key::Toggle);
        g3.frame(0.016f);
        c.ok(g3.is_open() && be3.visible && !be3.blocked && be3.ignore_count == 1, "opened with no player yet: visible, nothing applied");
        be3.pc_ready = true;
        g3.frame(0.016f);
        c.ok(be3.blocked && be3.ignore_count == 2, "block applied on the first frame the player exists (first open is blocked)");
        g3.frame(0.016f);
        g3.frame(0.016f);
        c.ok(be3.block_true_calls == 1, "block requested once, not every frame");
        g3.post_key(Key::Toggle);
        g3.frame(0.016f);
        c.ok(!be3.blocked && be3.ignore_count == 1 && be3.unmatched_false == 0, "close undoes only our block; the game's own block stays");
        be3.pc_ready = false;
        g3.post_key(Key::Toggle);
        g3.frame(0.016f);
        g3.post_key(Key::Toggle);
        g3.frame(0.016f);
        c.ok(be3.unmatched_false == 0 && be3.ignore_count == 1, "open+close while no player exists sends no unblock");
        c.ok(be3.count("[GUI] gameplay input blocked") == 1, "block logged when applied");
    }

    std::printf("== F2: map change destroys surfaces; window repopulates ==\n");
    {
        FakeEngine e4;
        Host m4{e4};
        e4.host = &m4;
        SoftBackend be4;
        be4.rasterise = false;
        GuiHost g4{m4, be4};
        g4.start();
        g4.post_key(Key::Toggle);
        g4.frame(0.016f);
        g4.frame(0.016f);
        c.ok(be4.atlas_creates == 1 && be4.good_submits == 2 && be4.blank_submits == 0, "before the map change: atlas once, frames drawn");
        be4.rebuild_next = true; // level / door transition
        g4.frame(0.016f);
        g4.frame(0.016f);
        g4.frame(0.016f);
        c.ok(be4.atlas_creates == 2, "atlas uploaded again after the rebuild");
        c.ok(be4.blank_submits == 0 && be4.good_submits == 5, "every frame after the map change is drawn with a live atlas (no blank window)");
        c.ok(be4.blocked && be4.player == 2 && be4.ignore_count == 1, "input block re-applied to the new player");
        c.ok(be4.count("[GUI] surfaces rebuilt") == 1, "rebuild logged once");
        g4.post_key(Key::Toggle);
        g4.frame(0.016f);
        c.ok(!be4.blocked && be4.ignore_count == 0 && be4.unmatched_false == 0, "close after the map change restores the new player exactly");
    }
    c.ok(g_host_asserts == 0, "zero host-side ImGui asserts over the whole run");
    const auto st = gui.stats();
    std::printf("    stats frames=%llu tab_calls=%llu tab_faults=%llu recovered=%llu tris=%llu live_bytes=%lld peak=%lld\n",
                (unsigned long long)st.frames, (unsigned long long)st.tab_calls, (unsigned long long)st.tab_faults, (unsigned long long)st.recovered_frames,
                (unsigned long long)st.triangles, (long long)NativeGui::detail::alloc_stats().live_bytes.load(), (long long)NativeGui::detail::alloc_stats().peak_bytes.load());
    return c.finish("gui_host");
}
