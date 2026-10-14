// NativeGuiHost.hpp -- the native GUI host for UE4SS-style C++ mod tabs (#11).
//
// Built on capability #9's native mod host (NativeModHostCore.hpp): it uses
// that host's CallScope (in-flight accounting), fault policy and stop
// listeners, and fills mbm_host_v1::gui with an mbm_gui_v1 table.
//
// Shape (the same as upstream UE4SS's DebuggingGUI, minus the OS window):
//   * ONE real Dear ImGui 1.92.1 context, owned by the host, created with the
//     host allocator; mods share it through UE4SS_ENABLE_IMGUI() (context +
//     allocator), exactly like upstream mods do across UE4SS.dll;
//   * a main "UE4SS" window with one tab per registered mod tab; each tab's
//     render callback runs on the game thread inside BeginTabItem/EndTabItem,
//     wrapped in a CallScope and in ImGui's own error-recovery state, so a mod
//     that throws or forgets an End*() cannot break the frame or other tabs;
//   * controller input: discrete key events (from the runtime's pad mapping)
//     become ImGui gamepad-navigation key press/release pairs;
//   * output: ImDrawData -> clipped textured triangle batches (CanvasBatcher)
//     handed to a Backend, which on the console draws them into a canvas render
//     target shown by one UMG image (NativeGuiEngineBackend.inc); the font
//     atlas is uploaded once as untextured runs (ImGuiAtlasRuns).
// Engine-free: the Backend interface is the only engine contact; host tests
// implement it with a software rasteriser.
#pragma once
#include <Mod/NativeModAbi.h>
#include <Platform/ImGuiAtlasRuns.hpp>
#include <Platform/ImGuiCanvasBatcher.hpp>
#include <Platform/NativeModHostCore.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <Platform/mbm_imgui_fingerprint.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace RC::Platform::NativeGui
{
    enum class Key : uint8_t
    {
        Toggle,   // open / close the GUI (a pad chord in PS5Pad.ini)
        Up,
        Down,
        Left,
        Right,
        Activate, // CROSS
        Cancel,   // back out of a popup / text field
        PrevTab,
        NextTab,
    };

    struct Config
    {
        std::string window_title = "UE4SS (PS5 Mod Bridge)";
        float font_scale = 1.5f;       // TV viewing distance
        int atlas_levels = 16;         // coverage quantisation for the atlas upload
        size_t max_tabs_total = 64;
        size_t max_tabs_per_mod = 16;
        size_t max_name_bytes = 128;
    };

    class Backend
    {
      public:
        virtual ~Backend() = default;
        virtual void log(int32_t level, std::string_view text) = 0;
        // Game thread. Ensure overlay surfaces exist; report the target size.
        // false = not ready this frame (the frame is skipped, GUI stays open).
        // *rebuilt = true when the surfaces were (re)built in this call (first
        // build, or after a map change destroyed the old ones): every engine
        // object the host handed out before (the atlas texture, the player
        // the input block was applied to) is gone and must be redone. [FIX F2]
        virtual auto prepare(int* width, int* height, bool* rebuilt) -> bool = 0;
        // Game thread, once: upload the atlas; return its texture id (0 = failed).
        virtual auto create_atlas(const std::vector<Run>& runs, int width, int height) -> ImTextureID = 0;
        // Game thread: draw this frame's batches (texture ids are atlas or 0).
        virtual void submit(const std::vector<Batch>& batches) = 0;
        virtual void set_visible(bool visible) = 0;
        // Game thread, only after a successful prepare(). blocked=true: apply
        // the block to the current player; returns true only if it was really
        // applied (or already is for this player). blocked=false: undo exactly
        // what was applied (nothing if nothing was). [FIX F1]
        virtual auto set_gameplay_input_blocked(bool blocked) -> bool = 0;
    };

    namespace detail
    {
        struct AllocStats
        {
            std::atomic<int64_t> live_bytes{0};
            std::atomic<int64_t> peak_bytes{0};
            std::atomic<uint64_t> allocs{0};
        };
        inline auto alloc_stats() -> AllocStats&
        {
            static AllocStats s;
            return s;
        }
        // Size-prefixed so live bytes are exact; 16-byte header keeps alignment.
        inline void* host_alloc(size_t size, void*)
        {
            auto* p = static_cast<unsigned char*>(std::malloc(size + 16));
            if (!p) return nullptr;
            std::memcpy(p, &size, sizeof size);
            auto& s = alloc_stats();
            const int64_t live = s.live_bytes.fetch_add(static_cast<int64_t>(size)) + static_cast<int64_t>(size);
            int64_t peak = s.peak_bytes.load();
            while (live > peak && !s.peak_bytes.compare_exchange_weak(peak, live)) {}
            s.allocs.fetch_add(1);
            return p + 16;
        }
        inline void host_free(void* ptr, void*)
        {
            if (!ptr) return;
            auto* p = static_cast<unsigned char*>(ptr) - 16;
            size_t size{};
            std::memcpy(&size, p, sizeof size);
            alloc_stats().live_bytes.fetch_sub(static_cast<int64_t>(size));
            std::free(p);
        }
        inline auto to_imgui(Key k) -> ImGuiKey
        {
            switch (k)
            {
            case Key::Up: return ImGuiKey_GamepadDpadUp;
            case Key::Down: return ImGuiKey_GamepadDpadDown;
            case Key::Left: return ImGuiKey_GamepadDpadLeft;
            case Key::Right: return ImGuiKey_GamepadDpadRight;
            case Key::Activate: return ImGuiKey_GamepadFaceDown;
            case Key::Cancel: return ImGuiKey_GamepadFaceRight;
            default: return ImGuiKey_None;
            }
        }
    } // namespace detail

    class GuiHost
    {
      public:
        struct Stats
        {
            uint64_t frames{}, tab_calls{}, tab_faults{}, recovered_frames{}, imgui_errors{}, batches{}, triangles{}, unknown_texture_batches{};
            BatchStats last{};
        };

        GuiHost(NativeMods::Host& mods, Backend& backend, Config cfg = {}) : m_mods(mods), m_backend(backend), m_cfg(std::move(cfg))
        {
            std::memset(&m_table, 0, sizeof m_table);
            m_table.magic = MBM_GUI_MAGIC;
            m_table.struct_size = sizeof(mbm_gui_v1);
            m_table.imgui_version = IMGUI_VERSION;
            m_table.imgui_version_num = IMGUI_VERSION_NUM;
            m_table.sizeof_io = sizeof(ImGuiIO);
            m_table.sizeof_style = sizeof(ImGuiStyle);
            m_table.sizeof_vec2 = sizeof(ImVec2);
            m_table.sizeof_vec4 = sizeof(ImVec4);
            m_table.sizeof_drawvert = sizeof(ImDrawVert);
            m_table.sizeof_drawidx = sizeof(ImDrawIdx);
            m_table.sizeof_context = sizeof(ImGuiContext);              // [FIX F7]
            m_table.config_fingerprint = MBM_IMGUI_CONFIG_FINGERPRINT(); // [FIX F7]
            m_table.ctx = this;
            m_table.get_context = &GuiHost::abi_get_context;
            m_table.get_allocators = &GuiHost::abi_get_allocators;
            m_table.register_tab = &GuiHost::abi_register_tab;
            m_table.unregister_tab = &GuiHost::abi_unregister_tab;
            m_table.is_open = &GuiHost::abi_is_open;
            m_mods.add_stop_listener([this](uint64_t token) { revoke_mod(token); });
        }
        ~GuiHost()
        {
            if (m_ctx)
            {
                ImGui::SetCurrentContext(m_ctx);
                ImGui::DestroyContext(m_ctx);
                m_ctx = nullptr;
            }
        }
        GuiHost(const GuiHost&) = delete;
        auto operator=(const GuiHost&) -> GuiHost& = delete;

        auto table() const -> const mbm_gui_v1* { return &m_table; }
        auto stats() const -> Stats { std::lock_guard g{m_mutex}; return m_stats; }
        auto is_open() const -> bool { return m_open.load(); }
        auto context() const -> ImGuiContext* { return m_ctx; }

        // Create the shared context and build the font atlas (CPU only). Any
        // thread, once, before mods' on_ui_init. Returns false on failure.
        auto start() -> bool
        {
            if (m_ctx) return true;
            ImGui::SetAllocatorFunctions(&detail::host_alloc, &detail::host_free, nullptr);
            m_ctx = ImGui::CreateContext();
            if (!m_ctx) return false;
            ImGui::SetCurrentContext(m_ctx);
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr; // no file I/O
            io.LogFilename = nullptr;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard;
            io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
            io.BackendPlatformName = "mbm-ps5-pad";
            io.BackendRendererName = "mbm-canvas-render-target";
            io.ConfigErrorRecovery = true;
            io.ConfigErrorRecoveryEnableAssert = false;  // never abort on a recoverable mod error
            io.ConfigErrorRecoveryEnableDebugLog = false;
            io.ConfigErrorRecoveryEnableTooltip = false;
            io.MouseDrawCursor = false;
            // Recoverable mod errors are reported through this callback (logged,
            // counted) instead of asserting; ImGui requires one reporting route.
            m_ctx->ErrorCallback = &GuiHost::on_imgui_error;
            m_ctx->ErrorCallbackUserData = this;
            ImGui::StyleColorsDark();
            ImGui::GetStyle().ScaleAllSizes(m_cfg.font_scale);
            ImGui::GetStyle().FontScaleMain = m_cfg.font_scale;
            ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1.0f; // opaque panel (see NativeGuiEngineBackend.inc)
            unsigned char* px{};
            int w{}, h{};
            io.Fonts->GetTexDataAsAlpha8(&px, &w, &h);
            if (!px || w <= 0 || h <= 0) return false;
            m_atlas_alpha.assign(px, px + static_cast<size_t>(w) * h);
            m_atlas_w = w;
            m_atlas_h = h;
            m_backend.log(MBM_LOG_NORMAL, "[GUI] context up imgui=" IMGUI_VERSION " atlas=" + std::to_string(w) + "x" + std::to_string(h));
            return true;
        }

        // Thread-safe (input callbacks run on the UE4SS event-loop thread).
        void post_key(Key k)
        {
            std::lock_guard g{m_mutex};
            if (m_keys.size() < 64) m_keys.push_back(k);
        }

        // Game thread, once per frame.
        void frame(float dt)
        {
            if (!m_ctx || m_failed) return;
            std::deque<Key> keys;
            {
                std::lock_guard g{m_mutex};
                keys.swap(m_keys);
            }
            std::vector<Key> nav;
            int tab_step = 0;
            for (Key k : keys)
            {
                if (k == Key::Toggle) set_open(!m_open.load());
                else if (k == Key::NextTab) ++tab_step;
                else if (k == Key::PrevTab) --tab_step;
                else nav.push_back(k);
            }
            if (tab_step != 0) step_tab(tab_step);
            if (!m_open.load()) return;
            int w{}, h{};
            bool rebuilt{};
            if (!m_backend.prepare(&w, &h, &rebuilt) || w <= 0 || h <= 0) return;
            if (rebuilt)
            {
                // [FIX F2] new surfaces (first open or after a map change): the old
                // atlas texture died with the old widget tree; upload it again.
                // The old player is gone too, so the input block is redone below.
                if (m_atlas_tex) m_backend.log(MBM_LOG_NORMAL, "[GUI] surfaces rebuilt: atlas and input block are redone");
                m_atlas_tex = 0;
                m_input_blocked = false;
                m_block_refused = false;
            }
            // [FIX F1] block gameplay input only now that the player controller
            // is resolved (prepare succeeded), and remember whether it worked so
            // close sends the matching unblock only when a block was applied.
            if (!m_input_blocked && !m_block_refused)
            {
                m_input_blocked = m_backend.set_gameplay_input_blocked(true);
                if (m_input_blocked) m_backend.log(MBM_LOG_NORMAL, "[GUI] gameplay input blocked (move + look)");
                else
                {
                    m_block_refused = true;
                    m_backend.log(MBM_LOG_WARNING, "[GUI] refused: gameplay input block unavailable; the game also sees the pad while the window is open");
                }
            }
            ImGui::SetCurrentContext(m_ctx);
            ImGuiIO& io = ImGui::GetIO();
            if (!m_atlas_tex)
            {
                const auto runs = atlas_runs(m_atlas_alpha.data(), m_atlas_w, m_atlas_h, m_cfg.atlas_levels);
                m_atlas_tex = m_backend.create_atlas(runs, m_atlas_w, m_atlas_h);
                if (!m_atlas_tex)
                {
                    m_failed = true;
                    m_backend.log(MBM_LOG_ERROR, "[GUI] refused: atlas upload failed; GUI disabled for this session");
                    set_open(false);
                    return;
                }
                io.Fonts->SetTexID(m_atlas_tex);
                m_backend.log(MBM_LOG_NORMAL, "[GUI] atlas uploaded runs=" + std::to_string(runs.size()));
            }
            io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
            io.DeltaTime = dt > 0.0001f && dt < 0.5f ? dt : 1.0f / 60.0f;
            for (ImGuiKey k : m_release_next) io.AddKeyEvent(k, false);
            m_release_next.clear();
            for (Key k : nav)
            {
                const ImGuiKey ik = detail::to_imgui(k);
                if (ik == ImGuiKey_None) continue;
                io.AddKeyEvent(ik, true);
                m_release_next.push_back(ik); // press this frame, release next frame
            }
            ImGui::NewFrame();
            draw_main_window();
            ImGui::Render();
            BatchStats bs{};
            auto batches = convert(ImGui::GetDrawData(), &bs);
            uint64_t tris = 0, unknown = 0;
            std::vector<Batch> drawable;
            drawable.reserve(batches.size());
            for (auto& b : batches)
            {
                if (b.texture != m_atlas_tex)
                {
                    ++unknown; // a mod ImGui::Image() with a texture we cannot draw: skipped
                    continue;
                }
                tris += b.tris.size();
                drawable.push_back(std::move(b));
            }
            m_backend.submit(drawable);
            std::lock_guard g{m_mutex};
            ++m_stats.frames;
            m_stats.batches += drawable.size();
            m_stats.triangles += tris;
            m_stats.unknown_texture_batches += unknown;
            m_stats.last = bs;
        }

        auto tab_count() const -> size_t
        {
            std::lock_guard g{m_mutex};
            size_t n{};
            for (auto& t : m_tabs) n += t.active ? 1 : 0;
            return n;
        }

        void set_open(bool open)
        {
            if (m_failed && open) return;
            if (m_open.exchange(open) == open) return;
            m_backend.set_visible(open);
            // [FIX F1] opening never blocks here (the player controller may not be
            // resolved yet; frame() applies the block after prepare). Closing
            // undoes the block only if one was actually applied, so it can never
            // cancel an input block the game set itself.
            m_block_refused = false;
            if (!open && m_input_blocked)
            {
                m_backend.set_gameplay_input_blocked(false);
                m_input_blocked = false;
            }
            m_backend.log(MBM_LOG_NORMAL, open ? "[GUI] opened" : "[GUI] closed (gameplay input restored)");
            if (!open && m_ctx)
            {
                // Drop any held navigation key so nothing sticks on reopen.
                ImGui::SetCurrentContext(m_ctx);
                for (ImGuiKey k : m_release_next) ImGui::GetIO().AddKeyEvent(k, false);
                m_release_next.clear();
            }
        }

      private:
        struct Tab
        {
            uint64_t id{};
            uint64_t token{};
            std::string name;
            mbm_gui_render_fn fn{};
            void* user{};
            bool active{};
        };

        void draw_main_window()
        {
            ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin(m_cfg.window_title.c_str()))
            {
                ImGui::End();
                return;
            }
            std::vector<Tab> tabs;
            {
                std::lock_guard g{m_mutex};
                for (auto& t : m_tabs)
                    if (t.active) tabs.push_back(t);
            }
            // Tabs of a mod that faulted elsewhere (or is stopping) disappear.
            tabs.erase(std::remove_if(tabs.begin(), tabs.end(),
                                      [&](const Tab& t) { return m_mods.state(t.token) != NativeMods::ModState::Running; }),
                       tabs.end());
            if (tabs.empty()) ImGui::TextUnformatted("No native mod has registered a GUI tab.");
            else if (ImGui::BeginTabBar("##mbm_tabs"))
            {
                for (auto& t : tabs)
                {
                    const std::string label = t.name + "##mbm" + std::to_string(t.id);
                    const ImGuiTabItemFlags flags = t.id == m_pending_select ? ImGuiTabItemFlags_SetSelected : 0;
                    if (!ImGui::BeginTabItem(label.c_str(), nullptr, flags)) continue;
                    m_current_tab = t.id;
                    run_tab(t);
                    ImGui::EndTabItem();
                }
                m_pending_select = 0;
                ImGui::EndTabBar();
            }
            ImGui::End();
        }

        void run_tab(const Tab& t)
        {
            if (!still_active(t.id)) return; // revoked by an earlier tab this frame
            ImGuiErrorRecoveryState state;
            ImGui::ErrorRecoveryStoreState(&state);
            mbm_status s{MBM_OK};
            bool entered{};
            {
                NativeMods::Host::CallScope scope{m_mods, t.token};
                entered = scope.entered();
                if (entered)
                {
                    try { s = t.fn(t.user); } catch (...) { s = MBM_E_MOD_FAULT; }
                }
            }
            // Re-balance anything the mod left open (Begin/Push without End/Pop).
            ImGuiErrorRecoveryState after;
            ImGui::ErrorRecoveryStoreState(&after);
            const bool unbalanced = std::memcmp(&state, &after, sizeof state) != 0;
            if (unbalanced) ImGui::ErrorRecoveryTryToRecoverState(&state);
            bool faulted{};
            {
                std::lock_guard lk{m_mutex};
                if (entered) ++m_stats.tab_calls;
                if (unbalanced) ++m_stats.recovered_frames;
                if (entered && s != MBM_OK)
                {
                    ++m_stats.tab_faults;
                    for (auto& x : m_tabs)
                        if (x.token == t.token) x.active = false;
                    faulted = true;
                }
            }
            // Outside our lock: the fault revokes the mod and calls the stop listeners.
            if (faulted) m_mods.fault(t.token, "gui tab render", s);
        }

        // PrevTab / NextTab (pad chords): select the neighbouring visible tab.
        void step_tab(int step)
        {
            std::vector<uint64_t> ids;
            {
                std::lock_guard g{m_mutex};
                for (auto& t : m_tabs)
                    if (t.active) ids.push_back(t.id);
            }
            if (ids.empty()) return;
            int cur = 0;
            for (size_t i = 0; i < ids.size(); ++i)
                if (ids[i] == m_current_tab) cur = static_cast<int>(i);
            const int n = static_cast<int>(ids.size());
            m_pending_select = ids[static_cast<size_t>(((cur + step) % n + n) % n)];
        }

        auto still_active(uint64_t id) -> bool
        {
            std::lock_guard g{m_mutex};
            for (auto& t : m_tabs)
                if (t.id == id) return t.active;
            return false;
        }

        void revoke_mod(uint64_t token)
        {
            std::lock_guard g{m_mutex};
            for (auto& t : m_tabs)
                if (t.token == token) t.active = false;
        }

        static void on_imgui_error(ImGuiContext*, void* user, const char* msg)
        {
            auto* h = static_cast<GuiHost*>(user);
            {
                std::lock_guard g{h->m_mutex};
                ++h->m_stats.imgui_errors;
                if (h->m_stats.imgui_errors > 32) return; // bounded log
            }
            h->m_backend.log(MBM_LOG_WARNING, std::string("[GUI] recovered ImGui usage error: ") + (msg ? msg : "?"));
        }

        static auto self(void* ctx) -> GuiHost* { return static_cast<GuiHost*>(ctx); }
        static auto abi_get_context(void* ctx) -> void* { return ctx ? self(ctx)->m_ctx : nullptr; }
        static void abi_get_allocators(void*, mbm_gui_alloc_fn* a, mbm_gui_free_fn* f, void** user)
        {
            if (a) *a = &detail::host_alloc;
            if (f) *f = &detail::host_free;
            if (user) *user = nullptr;
        }
        static auto abi_register_tab(void* ctx, uint64_t token, const char* name, mbm_gui_render_fn fn, void* user, uint64_t* out_id) -> mbm_status
        {
            if (!ctx || !name || !fn) return MBM_E_INVALID;
            try
            {
                auto& h = *self(ctx);
                const auto st = h.m_mods.state(token);
                if (!(st == NativeMods::ModState::Running || st == NativeMods::ModState::Starting)) return MBM_E_STATE;
                const size_t len = std::strlen(name);
                if (len == 0 || len > h.m_cfg.max_name_bytes) return MBM_E_INVALID;
                std::lock_guard g{h.m_mutex};
                size_t total{}, mine{};
                for (auto& t : h.m_tabs)
                {
                    total += t.active ? 1 : 0;
                    mine += (t.active && t.token == token) ? 1 : 0;
                }
                if (total >= h.m_cfg.max_tabs_total || mine >= h.m_cfg.max_tabs_per_mod) return MBM_E_CAPACITY;
                Tab t;
                t.id = ++h.m_next_id;
                t.token = token;
                t.name.assign(name, len);
                t.fn = fn;
                t.user = user;
                t.active = true;
                h.m_tabs.push_back(std::move(t));
                if (out_id) *out_id = h.m_next_id;
                return MBM_OK;
            }
            catch (...)
            {
                return MBM_E_INTERNAL;
            }
        }
        static auto abi_unregister_tab(void* ctx, uint64_t token, uint64_t id) -> mbm_status
        {
            if (!ctx) return MBM_E_INVALID;
            auto& h = *self(ctx);
            std::lock_guard g{h.m_mutex};
            for (auto& t : h.m_tabs)
                if (t.id == id && t.token == token)
                {
                    t.active = false; // safe from inside the tab's own render callback
                    return MBM_OK;
                }
            return MBM_E_NOT_FOUND;
        }
        static auto abi_is_open(void* ctx) -> int32_t { return ctx && self(ctx)->m_open.load() ? 1 : 0; }

        NativeMods::Host& m_mods;
        Backend& m_backend;
        Config m_cfg;
        mbm_gui_v1 m_table{};
        mutable std::mutex m_mutex;
        std::vector<Tab> m_tabs;
        std::deque<Key> m_keys;
        std::vector<ImGuiKey> m_release_next;
        std::vector<uint8_t> m_atlas_alpha;
        int m_atlas_w{}, m_atlas_h{};
        ImTextureID m_atlas_tex{};
        uint64_t m_current_tab{};
        uint64_t m_pending_select{};
        ImGuiContext* m_ctx{};
        std::atomic<bool> m_open{false};
        bool m_failed{};
        bool m_input_blocked{};  // a block was really applied to the current player [F1]
        bool m_block_refused{};  // tried after this open/rebuild and it was unavailable [F1]
        uint64_t m_next_id{};
        Stats m_stats{};
    };
} // namespace RC::Platform::NativeGui
