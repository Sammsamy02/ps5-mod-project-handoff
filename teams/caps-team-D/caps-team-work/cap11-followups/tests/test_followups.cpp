// test_followups.cpp -- host tests for the cap11 follow-ups (Mac-only, never part of a build):
//   [FU-DPI] sharp text: ImGui renders at the backend's pixel scale (DisplayFramebufferScale), batches come out
//            in render-target pixels, the default font is baked at its drawn size and density, so one font texel
//            maps to one screen pixel (texel ratio 1.0; before: 3.08 at 4K);
//   [FU1]    a frame where the surfaces are not ready releases our gameplay-input block, re-applies it when they
//            are back, and close never sends an unmatched unblock;
//   [FU5]    Host::game_thread_pump: the native mods' game-thread frame (tasks + GAME_THREAD_TICK / ENGINE_TICK)
//            runs once per engine frame through the shared FrameGate, not once per pump call.
// Real Dear ImGui 1.92.1 (mbm_imconfig.h), the #9 host core and the #11 GUI host from the include path
// (PATCH/post/include first, then the LIVE typescan-r4/include).
#include "fake_engine.hpp" // Team D cap09/tests (read-only)
#include "soft_raster.hpp" // Team D cap11/tests (read-only)

#include <Platform/NativeGuiHost.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace RC::Platform;
using namespace RC::Platform::NativeGui;
using namespace RC::Platform::NativeMods;

static int g_host_asserts = 0;
void mbm_imgui_assert_failed(const char* expr, const char* file, int line)
{
    ++g_host_asserts;
    std::printf("    [host imgui assert] %s at %s:%d\n", expr, file, line);
}

// ---------------- a backend whose panel is W x H units shown at `scale` pixels per unit ----------------
struct ScaledBackend final : Backend
{
    int w = 550, h = 432; // units (the 4K panel at half size, to keep the test fast)
    float scale = 2.0f;
    bool fail_prepare{}, rebuild_next{}, built{};
    // input model (UE: SetIgnoreMove/LookInput is a counter)
    int ignore_count{}, blocked_calls{}, unblocked_calls{}, unmatched_false{};
    bool blocked{};
    // atlas + frame capture
    std::vector<uint8_t> atlas;
    int aw{}, ah{}, atlas_creates{};
    std::vector<Batch> last;
    uint64_t submits{};
    std::vector<std::string> lines;
    void log(int32_t, std::string_view t) override { lines.emplace_back(t); }
    auto prepare(int* W, int* H, bool* rebuilt) -> bool override
    {
        *rebuilt = false;
        if (fail_prepare) return false;
        if (!built || rebuild_next)
        {
            built = true;
            rebuild_next = false;
            *rebuilt = true;
        }
        *W = w;
        *H = h;
        return true;
    }
    auto pixel_scale() -> float override { return scale; }
    auto create_atlas(const std::vector<Run>& r, int W, int H) -> ImTextureID override
    {
        ++atlas_creates;
        atlas = NativeGui::rasterise(r, W, H);
        aw = W;
        ah = H;
        return 1;
    }
    void submit(const std::vector<Batch>& b) override
    {
        ++submits;
        last = b;
    }
    void set_visible(bool) override {}
    auto set_gameplay_input_blocked(bool b) -> bool override
    {
        if (b)
        {
            ++blocked_calls;
            if (blocked) return true;
            ++ignore_count;
            blocked = true;
            return true;
        }
        ++unblocked_calls;
        if (!blocked) ++unmatched_false;
        else --ignore_count;
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

// Texel-to-pixel ratio of the textured glyph triangles of the last frame: pixel extent / atlas-texel extent.
// 1.0 = every font texel is exactly one render-target pixel. Untextured triangles (white pixel, uv span ~0) and
// the anti-aliased-line strips (one texel high) are excluded by requiring >= 3 texels in both directions.
struct Ratio
{
    size_t glyph_tris{}, within{};
    double median{};
};
static auto texel_ratio(const std::vector<Batch>& batches, int aw, int ah, double tol) -> Ratio
{
    Ratio r;
    std::vector<double> v;
    for (auto& b : batches)
        for (auto& t : b.tris)
        {
            float px0 = 1e9f, px1 = -1e9f, py0 = 1e9f, py1 = -1e9f, u0 = 1e9f, u1 = -1e9f, v0 = 1e9f, v1 = -1e9f;
            for (int k = 0; k < 3; ++k)
            {
                px0 = std::min(px0, t.pos[k][0]); px1 = std::max(px1, t.pos[k][0]);
                py0 = std::min(py0, t.pos[k][1]); py1 = std::max(py1, t.pos[k][1]);
                u0 = std::min(u0, t.uv[k][0]); u1 = std::max(u1, t.uv[k][0]);
                v0 = std::min(v0, t.uv[k][1]); v1 = std::max(v1, t.uv[k][1]);
            }
            const double tu = (u1 - u0) * aw, tv = (v1 - v0) * ah;
            if (tu < 3.0 || tv < 3.0) continue;
            ++r.glyph_tris;
            const double rx = (px1 - px0) / tu, ry = (py1 - py0) / tv;
            v.push_back(0.5 * (rx + ry));
            if (std::fabs(rx - 1.0) <= tol && std::fabs(ry - 1.0) <= tol) ++r.within;
        }
    if (!v.empty())
    {
        std::nth_element(v.begin(), v.begin() + static_cast<long>(v.size() / 2), v.end());
        r.median = v[v.size() / 2];
    }
    return r;
}
// Fraction of glyph-triangle vertex coordinates on axis `axis` (0 = x, 1 = y) that sit exactly on the pixel grid
// (|v - round(v)| < 1e-3). The default font (ProggyClean) has a designed +1 px per 13 px glyph offset in y
// (AddFontDefault GlyphOffset.y), so y sits a constant 0.077 px off the grid at size 20; x must be exact.
static auto on_grid(const std::vector<Batch>& batches, int aw, int ah, int axis) -> double
{
    size_t n{}, ok{};
    for (auto& b : batches)
        for (auto& t : b.tris)
        {
            float u0 = 1e9f, u1 = -1e9f, v0 = 1e9f, v1 = -1e9f;
            for (int k = 0; k < 3; ++k)
            {
                u0 = std::min(u0, t.uv[k][0]); u1 = std::max(u1, t.uv[k][0]);
                v0 = std::min(v0, t.uv[k][1]); v1 = std::max(v1, t.uv[k][1]);
            }
            if ((u1 - u0) * aw < 3.0f || (v1 - v0) * ah < 3.0f) continue;
            for (int k = 0; k < 3; ++k)
            {
                ++n;
                ok += std::fabs(t.pos[k][axis] - std::round(t.pos[k][axis])) < 1e-3f;
            }
        }
    return n ? static_cast<double>(ok) / static_cast<double>(n) : 0.0;
}
static auto baked_sizes(GuiHost& g) -> std::string
{
    ImGui::SetCurrentContext(g.context());
    std::string out;
    ImFontAtlasBuilder* b = ImGui::GetIO().Fonts->Builder;
    for (int i = 0; b && i < b->BakedPool.Size; ++i)
        if (!b->BakedPool[i].WantDestroy)
            out += std::to_string(static_cast<int>(b->BakedPool[i].Size)) + "@" + std::to_string(b->BakedPool[i].RasterizerDensity).substr(0, 4) + " ";
    return out;
}
static auto max_extent(const std::vector<Batch>& batches, float* mx, float* my) -> size_t
{
    size_t n{};
    *mx = *my = 0;
    for (auto& b : batches)
        for (auto& t : b.tris)
            for (int k = 0; k < 3; ++k)
            {
                *mx = std::max(*mx, t.pos[k][0]);
                *my = std::max(*my, t.pos[k][1]);
                ++n;
            }
    return n;
}

// ---------------- in-process mods ----------------
struct FontProbe
{
    const mbm_host_v1* host{};
    uint64_t token{};
    mbm_mod_v1 table{};
    uint64_t tab{};
    float font_size{}, baked_size{}, baked_density{};
    int renders{};
};
static FontProbe* g_font{};
static mbm_status font_render(void* user)
{
    auto& p = *static_cast<FontProbe*>(user);
    ++p.renders;
    ImGui::TextUnformatted("Sharp text: The quick brown fox jumps over the lazy dog 0123456789");
    p.font_size = ImGui::GetFontSize();
    if (ImFontBaked* b = ImGui::GetFontBaked())
    {
        p.baked_size = b->Size;
        p.baked_density = b->RasterizerDensity;
    }
    return MBM_OK;
}
static mbm_status font_destroy(void*) { return MBM_OK; }
static mbm_status font_entry(const mbm_host_v1* host, uint64_t token, const mbm_mod_v1** out)
{
    auto& p = *g_font;
    p.host = host;
    p.token = token;
    p.table.magic = MBM_ABI_MAGIC;
    p.table.abi_version = MBM_ABI_VERSION;
    p.table.struct_size = sizeof(mbm_mod_v1);
    p.table.name = "fontprobe";
    p.table.instance = &p;
    p.table.destroy = &font_destroy;
    if (host->gui->register_tab(host->gui->ctx, token, "Font", &font_render, &p, &p.tab) != MBM_OK) return MBM_E_MOD_FAULT;
    *out = &p.table;
    return MBM_OK;
}

struct TickMod
{
    const mbm_host_v1* host{};
    uint64_t token{};
    mbm_mod_v1 table{};
    int game_ticks{}, engine_ticks{}, tasks{};
    float last_dt{}, max_dt{}, min_dt{1e9f};
};
static TickMod* g_tick{};
static mbm_status game_tick(void* u, float dt)
{
    auto& m = *static_cast<TickMod*>(u);
    ++m.game_ticks;
    m.last_dt = dt;
    if (m.game_ticks > 1)
    {
        m.max_dt = std::max(m.max_dt, dt);
        m.min_dt = std::min(m.min_dt, dt);
    }
    return MBM_OK;
}
static mbm_status engine_tick(void* u, float) { ++static_cast<TickMod*>(u)->engine_ticks; return MBM_OK; }
static mbm_status a_task(void* u) { ++static_cast<TickMod*>(u)->tasks; return MBM_OK; }
static mbm_status tick_destroy(void*) { return MBM_OK; }
static mbm_status tick_entry(const mbm_host_v1* host, uint64_t token, const mbm_mod_v1** out)
{
    auto& m = *g_tick;
    m.host = host;
    m.token = token;
    m.table.magic = MBM_ABI_MAGIC;
    m.table.abi_version = MBM_ABI_VERSION;
    m.table.struct_size = sizeof(mbm_mod_v1);
    m.table.name = "tickmod";
    m.table.instance = &m;
    m.table.destroy = &tick_destroy;
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

    // ===================================================================================================
    std::printf("== [FU-DPI] 4K-like panel: units 550x432 shown at 2.0 pixels per unit ==\n");
    FakeEngine eng;
    Host mods{eng, "fu-test"};
    eng.host = &mods;
    ScaledBackend be;
    GuiHost gui{mods, be};
    mods.set_gui(gui.table());
    c.ok(gui.start(), "GUI host started");
    int start_w{}, start_h{};
    {
        ImGui::SetCurrentContext(gui.context());
        unsigned char* px{};
        ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&px, &start_w, &start_h);
        std::printf("    atlas at start (13 px legacy bake only) %dx%d\n", start_w, start_h);
    }
    FontProbe fp;
    g_font = &fp;
    uint64_t tfp{};
    c.ok(mods.attach("fontprobe", &font_entry, &tfp) == MBM_OK, "font probe tab registered");
    gui.post_key(Key::Toggle);
    frames(gui, 3);
    c.ok(gui.is_open() && be.submits == 3 && fp.renders >= 1, "window opens and renders at scale 2");
    c.ok(be.count("[GUI] font baked size=20 density=2.0") == 1, "default font baked once at its drawn size (20 units) for density 2");
    c.ok(be.atlas_creates == 1 && be.count("[GUI] atlas uploaded runs=") == 1, "atlas uploaded once (bake happens before the first upload)");
    std::printf("    font size=%.1f baked size=%.1f density=%.2f atlas=%dx%d\n", fp.font_size, fp.baked_size, fp.baked_density, be.aw, be.ah);
    c.ok(fp.font_size == 20.0f && fp.baked_size == 20.0f && fp.baked_density == 2.0f,
         "inside the frame the default text uses the 20-unit bake at density 2 (exact hit, no scaled bitmap)");
    {
        float mx{}, my{};
        const size_t verts = max_extent(be.last, &mx, &my);
        std::printf("    vertices=%zu max x=%.1f max y=%.1f (render target %dx%d px)\n", verts, mx, my, be.w * 2, be.h * 2);
        c.ok(verts > 0 && mx <= be.w * 2.0f + 0.5f && my <= be.h * 2.0f + 0.5f, "every vertex lies inside the 1100x864 px render target");
        c.ok(mx > be.w * 1.5f, "batches are in render-target pixels (scaled from units)");
        ImGui::SetCurrentContext(gui.context());
        const ImDrawData* dd = ImGui::GetDrawData();
        c.ok(dd && dd->FramebufferScale.x == 2.0f && dd->DisplaySize.x == static_cast<float>(be.w), "ImGui lays out in units with FramebufferScale 2");
    }
    {
        const Ratio r = texel_ratio(be.last, be.aw, be.ah, 0.02);
        std::printf("    glyph triangles=%zu within 2%% of 1:1=%zu median texel ratio=%.4f (old route: 13 px bake x 20/13 x 2 = %.2f)\n", r.glyph_tris, r.within,
                    r.median, 20.0 / 13.0 * 2.0);
        c.ok(r.glyph_tris > 100 && r.within * 100 >= r.glyph_tris * 99, ">= 99% of glyph triangles map one font texel to one pixel");
        c.ok(std::fabs(r.median - 1.0) < 0.005, "median texel ratio is 1.0");
        const double gx = on_grid(be.last, be.aw, be.ah, 0), gy = on_grid(be.last, be.aw, be.ah, 1);
        std::printf("    glyph vertices on the pixel grid: x %.4f, y %.4f (y: the font's own 1/13 glyph offset); bakes: %s\n", gx, gy, baked_sizes(gui).c_str());
        c.ok(gx > 0.999, "scale 2: every glyph quad lands on whole pixels horizontally");
        c.ok(baked_sizes(gui).find("13@2.00") == std::string::npos, "[review #3] no unused 13 px bake at density 2");
    }
    {
        ImGui::SetCurrentContext(gui.context());
        c.ok(ImGui::GetIO().Fonts->TexList.Size == 1, "the grown atlas left no orphan texture copy behind");
        c.ok(ImGui::GetIO().Fonts->Fonts.Size == 1 && ImGui::GetIO().Fonts->Fonts[0]->LegacySize == 13.0f, "font object and LegacySize untouched (mods' ImFont* stay valid)");
    }

    std::printf("== [FU-DPI] density change at run time (e.g. 1440p scale 1.5, then back to 2.0) ==\n");
    be.scale = 1.5f;
    be.rebuild_next = true;
    frames(gui, 2);
    c.ok(be.count("[GUI] font baked size=20 density=1.5") == 1 && be.atlas_creates == 2, "new density: baked again, atlas uploaded again (once)");
    {
        const Ratio r = texel_ratio(be.last, be.aw, be.ah, 0.03);
        std::printf("    scale 1.5: glyph triangles=%zu within=%zu median=%.4f atlas=%dx%d\n", r.glyph_tris, r.within, r.median, be.aw, be.ah);
        c.ok(r.glyph_tris > 100 && std::fabs(r.median - 1.0) < 0.01, "scale 1.5: median texel ratio 1.0");
        c.ok(fp.baked_density == 1.5f && fp.baked_size == 20.0f, "scale 1.5: the 1.5-density bake is used");
    }
    be.scale = 2.0f;
    be.rebuild_next = true;
    frames(gui, 2);
    c.ok(be.count("[GUI] font baked size=20 density=2.0") == 2 && be.atlas_creates == 3 && fp.baked_density == 2.0f, "back to 2.0: bake reused/refreshed, uploaded once");
    const int frames_before = static_cast<int>(be.submits);
    frames(gui, 5);
    c.ok(be.atlas_creates == 3 && be.count("[GUI] font baked") == 3 && be.submits == static_cast<uint64_t>(frames_before + 5), "steady state: no bake or upload per frame");

    std::printf("== [FU-DPI] scale 1 (1080p TV) is sharp too ==\n");
    {
        FakeEngine e1;
        Host m1{e1, "fu-1x"};
        e1.host = &m1;
        ScaledBackend b1;
        b1.scale = 1.0f;
        b1.w = 1100;
        b1.h = 864;
        GuiHost g1{m1, b1};
        m1.set_gui(g1.table());
        g1.start();
        FontProbe f1;
        g_font = &f1;
        uint64_t t1{};
        m1.attach("fontprobe1", &font_entry, &t1);
        g1.post_key(Key::Toggle);
        frames(g1, 2);
        const Ratio r = texel_ratio(b1.last, b1.aw, b1.ah, 0.02);
        std::printf("    scale 1: glyph triangles=%zu within=%zu median=%.4f (old route: %.2f) atlas=%dx%d\n", r.glyph_tris, r.within, r.median, 20.0 / 13.0,
                    b1.aw, b1.ah);
        c.ok(f1.baked_size == 20.0f && f1.baked_density == 1.0f && std::fabs(r.median - 1.0) < 0.005, "scale 1: 20 px bake, texel ratio 1.0");
        float mx{}, my{};
        max_extent(b1.last, &mx, &my);
        c.ok(mx <= 1100.5f && my <= 864.5f, "scale 1: batches unchanged in size (pixels = units)");
        m1.stop(t1);
        g_font = &fp;
    }

    std::printf("== [review gap] 1080p then 4K: the grown atlas drops the old texture that already had our texture id ==\n");
    {
        FakeEngine e5;
        Host m5{e5, "fu-1to2"};
        e5.host = &m5;
        ScaledBackend b5;
        b5.scale = 1.0f;
        GuiHost g5{m5, b5};
        m5.set_gui(g5.table());
        g5.start();
        g5.post_key(Key::Toggle);
        frames(g5, 2);
        const int w1 = b5.aw, h1 = b5.ah;
        b5.scale = 2.0f;
        b5.rebuild_next = true;
        frames(g5, 3);
        ImGui::SetCurrentContext(g5.context());
        std::printf("    atlas %dx%d -> %dx%d, TexList=%d, bakes: %s\n", w1, h1, b5.aw, b5.ah, ImGui::GetIO().Fonts->TexList.Size, baked_sizes(g5).c_str());
        c.ok(b5.aw * b5.ah > w1 * h1, "the 2.0 bake grew the atlas (a new texture replaced the uploaded one)");
        c.ok(ImGui::GetIO().Fonts->TexList.Size == 1, "the replaced texture (it carried our id) was freed");
        const Ratio r = texel_ratio(b5.last, b5.aw, b5.ah, 0.02);
        c.ok(r.glyph_tris > 100 && std::fabs(r.median - 1.0) < 0.005 && b5.atlas_creates == 2, "after the switch: texel ratio 1.0, atlas uploaded once more");
    }

    std::printf("== [review #2] bounds: a too-large bake is skipped (old behaviour), a too-large atlas fails closed ==\n");
    {
        FakeEngine e6;
        Host m6{e6, "fu-bound"};
        e6.host = &m6;
        ScaledBackend b6;
        Config cfg6;
        cfg6.max_bake_codepoints = 100; // the stock font has 224: the bake must be skipped
        GuiHost g6{m6, b6, cfg6};
        m6.set_gui(g6.table());
        g6.start();
        g6.post_key(Key::Toggle);
        frames(g6, 3);
        const Ratio r = texel_ratio(b6.last, b6.aw, b6.ah, 0.02);
        std::printf("    skipped bake: median texel ratio %.3f (the old route)\n", r.median);
        c.ok(b6.count("[GUI] sharp-font bake for density 2.000000 skipped (224 codepoints at 40 px exceed the bound)") == 1, "one warning names the bound");
        c.ok(g6.is_open() && b6.submits == 3 && b6.atlas_creates == 1 && b6.count("[GUI] font baked") == 0, "GUI still works from the start-up atlas, no retry per frame");
        c.ok(r.median > 2.9 && r.median < 3.3, "text drawn the old way (13 px bake scaled about 3x)");
        FakeEngine e7;
        Host m7{e7, "fu-cap"};
        e7.host = &m7;
        ScaledBackend b7;
        Config cfg7;
        cfg7.max_atlas_texels = 1000;
        GuiHost g7{m7, b7, cfg7};
        g7.start();
        g7.post_key(Key::Toggle);
        frames(g7, 2);
        c.ok(!g7.is_open() && b7.submits == 0 && b7.atlas_creates == 0 && b7.count("is too large to upload; GUI disabled for this session") == 1 && !b7.blocked,
             "atlas above the cap: nothing built or uploaded, GUI off for the session, input not blocked");
    }

    // ===================================================================================================
    std::printf("== [FU1] surfaces not ready while open: our input block is released, then re-applied ==\n");
    {
        FakeEngine e2;
        Host m2{e2, "fu1"};
        e2.host = &m2;
        ScaledBackend b2;
        b2.scale = 1.0f;
        b2.ignore_count = 1; // the game's own block (cutscene) must survive everything below
        b2.blocked = false;
        GuiHost g2{m2, b2};
        g2.start();
        // the model counts our block separately: ignore_count starts at the game's 1
        g2.post_key(Key::Toggle);
        g2.frame(0.016f);
        c.ok(b2.blocked && b2.ignore_count == 2, "open: our block applied on top of the game's");
        b2.fail_prepare = true; // a rebuild that fails part-way (m_built false) or no player yet
        g2.frame(0.016f);
        c.ok(!b2.blocked && b2.ignore_count == 1 && b2.unmatched_false == 0, "not shown: our block released, the game's stays");
        c.ok(b2.count("[GUI] window not shown (surfaces not ready): gameplay input released") == 1, "release logged once");
        g2.frame(0.016f);
        g2.frame(0.016f);
        c.ok(b2.unblocked_calls == 1 && b2.count("gameplay input released") == 1, "released once, not every failing frame");
        b2.fail_prepare = false;
        b2.rebuild_next = true;
        g2.frame(0.016f);
        c.ok(b2.blocked && b2.ignore_count == 2, "surfaces back: block applied again");
        g2.post_key(Key::Toggle);
        g2.frame(0.016f);
        c.ok(!b2.blocked && b2.ignore_count == 1 && b2.unmatched_false == 0, "close restores input exactly (no unmatched unblock)");
        b2.fail_prepare = true;
        g2.post_key(Key::Toggle);
        g2.frame(0.016f);
        g2.post_key(Key::Toggle);
        g2.frame(0.016f);
        c.ok(b2.unmatched_false == 0 && b2.ignore_count == 1 && !g2.is_open(), "open + close while never shown: no block, no unblock, input free");
    }

    // ===================================================================================================
    std::printf("== [FU5] native mod game-thread frame once per engine frame (Host::game_thread_pump) ==\n");
    {
        using namespace std::chrono;
        const auto t0 = steady_clock::time_point{} + hours(1);
        FakeEngine e3;
        e3.f.vmt_engine_tick_armed = true; // ENGINE_TICK is only admitted with the VMT pump armed
        Host h3{e3, "fu5"};
        e3.host = &h3;
        TickMod tm;
        g_tick = &tm;
        uint64_t tok{};
        c.ok(h3.attach("tickmod", &tick_entry, &tok) == MBM_OK, "tick mod attached");
        uint64_t id{};
        c.ok(tm.host->register_hook(tm.host->ctx, tok, MBM_HOOK_GAME_THREAD_TICK, &game_tick, &tm, &id) == MBM_OK, "GAME_THREAD_TICK registered");
        c.ok(tm.host->register_hook(tm.host->ctx, tok, MBM_HOOK_ENGINE_TICK, &engine_tick, &tm, &id) == MBM_OK, "ENGINE_TICK registered");
        // 1) marshal pump only, ~1,960 calls/s (the #9 launch peak), 60 Hz engine for 1 s
        int ran = 0;
        for (int i = 0; i < 1960; ++i)
        {
            float dt{};
            if (h3.game_thread_pump(t0 + microseconds(510 * i), PumpSource::GameThreadPump, &dt)) ++ran;
        }
        std::printf("    marshal only: 1960 pump calls -> %d frames, ticks=%d, dt %.4f..%.4f s\n", ran, tm.game_ticks, tm.min_dt, tm.max_dt);
        c.ok(ran == tm.game_ticks && tm.game_ticks >= 110 && tm.game_ticks <= 126, "1960 marshal calls/s -> one tick per 8 ms gate, not 1960");
        c.ok(tm.engine_ticks == 0, "ENGINE_TICK not delivered without the VMT Tick pump");
        c.ok(tm.min_dt > 0.0079f && tm.max_dt < 0.0090f, "tick dt = time since the previous frame that ran (not since the last pump call)");
        // 2) both pumps live: VMT tick at 60 Hz owns the frame; marshal calls in between change nothing
        const int g0 = tm.game_ticks, e0 = tm.engine_ticks;
        const auto t1 = t0 + seconds(5);
        for (int f = 0; f < 60; ++f)
        {
            const auto base = t1 + microseconds(16667 * f);
            h3.game_thread_pump(base, PumpSource::VmtEngineTick);
            for (int k = 1; k <= 30; ++k) h3.game_thread_pump(base + microseconds(500 * k), PumpSource::GameThreadPump);
        }
        c.ok(tm.game_ticks - g0 == 60 && tm.engine_ticks - e0 == 60, "both pumps (60 VMT + 1800 marshal calls): exactly 60 game ticks and 60 engine ticks");
        // 3) tasks queued between frames run on the next frame that runs, once
        const auto t2 = t1 + seconds(2);
        h3.game_thread_pump(t2, PumpSource::VmtEngineTick);
        tm.host->execute_in_game_thread(tm.host->ctx, tok, &a_task, &tm);
        tm.host->execute_in_game_thread(tm.host->ctx, tok, &a_task, &tm);
        h3.game_thread_pump(t2 + microseconds(500), PumpSource::GameThreadPump); // a marshal call inside the same frame
        c.ok(tm.tasks == 0, "task not run by a pump call inside the same frame");
        h3.game_thread_pump(t2 + microseconds(16667), PumpSource::VmtEngineTick);
        c.ok(tm.tasks == 2, "queued tasks run on the next engine frame");
        const auto st = h3.frame_gate_stats();
        c.ok(st.calls == 1960u + 60u * 31u + 3u && st.due == static_cast<uint64_t>(tm.game_ticks), "gate counted every pump call; frames == ticks delivered");
        // 4) [review gap] hand-over: the VMT pump goes quiet for 1 s (map load) while the marshal pump keeps
        //    firing, then comes back; frames never stop and never double up
        const int g4 = tm.game_ticks, e4 = tm.engine_ticks;
        const auto t3 = t2 + seconds(2);
        int vmt_calls = 0;
        for (int i = 0; i < 4000; ++i) // 2 s in 0.5 ms steps: VMT at 60 Hz except during [0.5 s, 1.5 s)
        {
            const auto now = t3 + microseconds(500 * i);
            const bool vmt_live = i < 1000 || i >= 3000;
            if (vmt_live && i % 33 == 0) { h3.game_thread_pump(now, PumpSource::VmtEngineTick); ++vmt_calls; }
            h3.game_thread_pump(now, PumpSource::GameThreadPump);
        }
        const int gticks = tm.game_ticks - g4, eticks = tm.engine_ticks - e4;
        std::printf("    hand-over: %d VMT calls, game ticks=%d engine ticks=%d over 2 s\n", vmt_calls, gticks, eticks);
        c.ok(eticks >= vmt_calls - 1 && eticks <= vmt_calls, "ENGINE_TICK: every VMT call but at most one at the hand-back is delivered");
        c.ok(gticks >= vmt_calls + 85 && gticks <= vmt_calls + 100, "the marshal pump drives frames once the VMT pump is quiet > 250 ms (about 94 at the 8 ms gate), none doubled");
        h3.stop(tok);
    }

    c.ok(g_host_asserts == 0, "zero host-side ImGui asserts over the whole run");
    mods.stop(tfp);
    return c.finish("followups");
}
