// mbm_sdk_entry.cpp -- PS5 Mod Bridge native mod SDK: the one TU every native
// C++ mod links (exactly once). It exports the single ABI entry
// mbm_mod_entry_v1 and adapts the mod's upstream-style exports
//     extern "C" RC::CppUserModBase* start_mod();
//     extern "C" void uninstall_mod(RC::CppUserModBase*);
// to the C table in NativeModAbi.h. No C++ object, exception or allocation
// crosses into the host: every callback is wrapped (Sdk::contain) and returns
// an mbm_status.
//
// Build with -DMBM_SDK_WITH_IMGUI (and the SDK's pinned Dear ImGui sources) to
// enable UE4SS_ENABLE_IMGUI() / register_tab rendering (#11).
#include <DynamicOutput/DynamicOutput.hpp>
#include <Mod/CppUserModBase.hpp>
#include <UE4SSProgram.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <mbm/Sdk.hpp>

#ifdef MBM_SDK_WITH_IMGUI
#include <imgui.h>
#include <imgui_internal.h>
#include <Platform/mbm_imgui_fingerprint.h>
#include <set>
// The mod's copy of IM_ASSERT (mbm_imconfig.h): log once per site, never abort.
__attribute__((visibility("hidden"))) void mbm_imgui_assert_failed(const char* expr, const char* file, int line)
{
    static std::set<std::pair<const char*, int>> seen;
    if (seen.size() > 64 || !seen.insert({file, line}).second) return;
    RC::Sdk::log(MBM_LOG_ERROR, std::string("[SDK] imgui assert (mod side) ") + (expr ? expr : "?") + " at " + (file ? file : "?") + ":" + std::to_string(line));
}
#endif

#include <new>
#include <string>

extern "C" RC::CppUserModBase* start_mod();
extern "C" void uninstall_mod(RC::CppUserModBase* mod);

namespace RC::Sdk
{
    auto globals() -> Globals&
    {
        static Globals g{};
        return g;
    }
    auto last_error_slot() -> std::string&
    {
        static std::string s;
        return s;
    }

    namespace
    {
        struct TaskBox
        {
            std::function<void()> fn;
        };
        auto task_tramp(void* user) -> mbm_status
        {
            auto* box = static_cast<TaskBox*>(user);
            const auto s = contain([&] { box->fn(); });
            delete box;
            return s;
        }
        auto key_store() -> CallbackStore<Input::EventCallbackCallable>&
        {
            static CallbackStore<Input::EventCallbackCallable> s;
            return s;
        }
        auto key_tramp(void* user) -> mbm_status
        {
            auto* fn = static_cast<Input::EventCallbackCallable*>(user);
            return contain([&] { (*fn)(); });
        }
        auto tick_store() -> CallbackStore<std::function<void(float)>>&
        {
            static CallbackStore<std::function<void(float)>> s;
            return s;
        }
        auto tick_tramp(void* user, float dt) -> mbm_status
        {
            auto* fn = static_cast<std::function<void(float)>*>(user);
            return contain([&] { (*fn)(dt); });
        }

        struct TabEntry
        {
            GUI::GUITab::RenderFunctionType fn{};
            CppUserModBase* owner{};
            std::string name;
        };
        auto tab_store() -> CallbackStore<TabEntry>&
        {
            static CallbackStore<TabEntry> s;
            return s;
        }
        bool g_imgui_enabled = false;

        auto tab_tramp(void* user) -> mbm_status
        {
            auto* e = static_cast<TabEntry*>(user);
            // [FIX N2] enable_imgui() builds std::strings for its log lines; an
            // exception there must not leave the module (each image has its own
            // unwinder), so it runs inside the SDK protection too.
            bool shared = g_imgui_enabled;
            if (!shared)
            {
                if (const auto s = contain([&] { shared = enable_imgui(); }); s != MBM_OK) return s;
            }
            if (!shared) return record_fault("tab render refused: mod could not share the host ImGui context");
            return contain([&] { e->fn(e->owner); });
        }
    } // namespace

    auto execute_in_game_thread(std::function<void()> fn) -> bool
    {
        auto* h = host();
        if (!h || !fn) return false;
        auto* box = new (std::nothrow) TaskBox{std::move(fn)};
        if (!box) return false;
        if (h->execute_in_game_thread(h->ctx, token(), &task_tramp, box) != MBM_OK)
        {
            delete box;
            return false;
        }
        return true; // the host drops queued tasks of a stopped mod (box leaks; bounded)
    }

    auto enable_imgui() -> bool
    {
#ifdef MBM_SDK_WITH_IMGUI
        if (g_imgui_enabled) return true;
        auto* h = host();
        const mbm_gui_v1* gui = h ? h->gui : nullptr;
        // [FIX F7] struct_size covers config_fingerprint: an older host (no
        // fingerprint) is refused here.
        if (!gui || gui->magic != MBM_GUI_MAGIC || gui->struct_size < sizeof(mbm_gui_v1))
        {
            log(MBM_LOG_WARNING, "UE4SS_ENABLE_IMGUI refused: this runtime has no native GUI host");
            return false;
        }
        const bool same = gui->imgui_version_num == IMGUI_VERSION_NUM && gui->sizeof_io == sizeof(ImGuiIO) &&
                          gui->sizeof_style == sizeof(ImGuiStyle) && gui->sizeof_vec2 == sizeof(ImVec2) && gui->sizeof_vec4 == sizeof(ImVec4) &&
                          gui->sizeof_drawvert == sizeof(ImDrawVert) && gui->sizeof_drawidx == sizeof(ImDrawIdx);
        // [FIX F7] the context itself and the whole config must match too.
        const bool same_context = gui->sizeof_context == sizeof(ImGuiContext) && gui->config_fingerprint == MBM_IMGUI_CONFIG_FINGERPRINT();
        if (same && !same_context)
        {
            log(MBM_LOG_ERROR, "UE4SS_ENABLE_IMGUI refused: host ImGuiContext size " + std::to_string(gui->sizeof_context) + " / config fingerprint " +
                                       std::to_string(gui->config_fingerprint) + " differ from the mod's " + std::to_string(sizeof(ImGuiContext)) + " / " +
                                       std::to_string(MBM_IMGUI_CONFIG_FINGERPRINT()) + "; rebuild the mod with the SDK's pinned ImGui and Platform/mbm_imconfig.h");
            return false;
        }
        if (!same)
        {
            log(MBM_LOG_ERROR, std::string("UE4SS_ENABLE_IMGUI refused: host Dear ImGui ") + (gui->imgui_version ? gui->imgui_version : "?") +
                                       " / layout differs from the mod's " IMGUI_VERSION " build; rebuild the mod with the SDK's pinned ImGui");
            return false;
        }
        void* context = gui->get_context ? gui->get_context(gui->ctx) : nullptr;
        if (!context)
        {
            log(MBM_LOG_WARNING, "UE4SS_ENABLE_IMGUI: host GUI context not created yet (call it from on_ui_init)");
            return false;
        }
        mbm_gui_alloc_fn a{};
        mbm_gui_free_fn f{};
        void* u{};
        gui->get_allocators(gui->ctx, &a, &f, &u);
        if (!a || !f)
        {
            log(MBM_LOG_ERROR, "UE4SS_ENABLE_IMGUI refused: host supplied no allocator");
            return false;
        }
        ImGui::SetAllocatorFunctions(a, f, u);
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(context));
        g_imgui_enabled = true;
        return true;
#else
        log(MBM_LOG_WARNING, "UE4SS_ENABLE_IMGUI refused: mod was built without -DMBM_SDK_WITH_IMGUI");
        return false;
#endif
    }

    auto keydown_mask(const Input::Handler::ModifierKeyArray& mods) -> uint32_t
    {
        uint32_t m = 0;
        for (auto k : mods)
        {
            if (k == Input::ModifierKey::SHIFT) m |= MBM_KEYMOD_SHIFT;
            if (k == Input::ModifierKey::CONTROL) m |= MBM_KEYMOD_CONTROL;
            if (k == Input::ModifierKey::ALT) m |= MBM_KEYMOD_ALT;
        }
        return m;
    }
} // namespace RC::Sdk

namespace RC::Unreal::UObjectGlobals::Detail
{
    auto hook_store() -> Sdk::CallbackStore<HookEntry>&
    {
        static Sdk::CallbackStore<HookEntry> s;
        return s;
    }
} // namespace RC::Unreal::UObjectGlobals::Detail

namespace RC
{
    CppUserModBase::CppUserModBase() = default;
    CppUserModBase::~CppUserModBase() = default;

    auto CppUserModBase::register_keydown_event(Input::Key key, const Input::EventCallbackCallable& callback, uint8_t) -> void
    {
        register_keydown_event(key, Input::Handler::ModifierKeyArray{}, callback, 0);
    }

    auto CppUserModBase::register_keydown_event(Input::Key key, const Input::Handler::ModifierKeyArray& modifier_keys,
                                                const Input::EventCallbackCallable& callback, uint8_t) -> void
    {
        auto* h = Sdk::host();
        if (!h || !callback) return;
        auto* fn = Sdk::key_store().keep(callback);
        uint64_t id{};
        const auto s = h->register_key_down(h->ctx, Sdk::token(), static_cast<uint32_t>(key), Sdk::keydown_mask(modifier_keys), &Sdk::key_tramp, fn, &id);
        if (s != MBM_OK) Sdk::log(MBM_LOG_WARNING, "register_keydown_event refused by host, status " + std::to_string(s));
    }

    auto CppUserModBase::register_game_thread_tick(std::function<void(float)> fn) -> bool
    {
        auto* h = Sdk::host();
        if (!h || !fn) return false;
        auto* stored = Sdk::tick_store().keep(std::move(fn));
        uint64_t id{};
        return h->register_hook(h->ctx, Sdk::token(), MBM_HOOK_GAME_THREAD_TICK, &Sdk::tick_tramp, stored, &id) == MBM_OK;
    }

    auto CppUserModBase::register_tab(StringViewType tab_name, GUI::GUITab::RenderFunctionType render_function) -> void
    {
        auto* h = Sdk::host();
        if (!h || !render_function) return;
        const mbm_gui_v1* gui = h->gui;
        if (!gui || gui->magic != MBM_GUI_MAGIC || !gui->register_tab)
        {
            Sdk::log(MBM_LOG_WARNING, "register_tab refused: this runtime has no native GUI host (#11)");
            return;
        }
        auto name = Sdk::to_utf8(tab_name);
        auto* entry = Sdk::tab_store().keep(Sdk::TabEntry{render_function, this, name});
        uint64_t id{};
        const auto s = gui->register_tab(gui->ctx, Sdk::token(), name.c_str(), &Sdk::tab_tramp, entry, &id);
        if (s != MBM_OK) Sdk::log(MBM_LOG_WARNING, "register_tab '" + name + "' refused by host, status " + std::to_string(s));
    }
} // namespace RC

namespace
{
    using namespace RC;

    struct ModInstance
    {
        mbm_mod_v1 table{};
        CppUserModBase* mod{};
        std::string name, version, authors, description, sdk;
    };

    auto inst(void* p) -> ModInstance* { return static_cast<ModInstance*>(p); }

    auto t_program_start(void* p) -> mbm_status { return Sdk::contain([&] { inst(p)->mod->on_program_start(); }); }
    auto t_unreal_init(void* p) -> mbm_status { return Sdk::contain([&] { inst(p)->mod->on_unreal_init(); }); }
    auto t_ui_init(void* p) -> mbm_status { return Sdk::contain([&] { inst(p)->mod->on_ui_init(); }); }
    auto t_update(void* p) -> mbm_status { return Sdk::contain([&] { inst(p)->mod->on_update(); }); }
    auto t_cpp_mods_loaded(void* p) -> mbm_status { return Sdk::contain([&] { inst(p)->mod->on_cpp_mods_loaded(); }); }
    auto t_dll_load(void* p, const char* name) -> mbm_status
    {
        return Sdk::contain([&] {
            const auto w = Sdk::to_wide(name ? name : "");
            inst(p)->mod->on_dll_load(w);
        });
    }
    auto t_destroy(void* p) -> mbm_status
    {
        auto* i = inst(p);
        const auto s = Sdk::contain([&] { uninstall_mod(i->mod); });
        if (s == MBM_OK) delete i; // a failed uninstall keeps the instance (host retains the mod)
        return s;
    }
    auto t_last_error(void*) -> const char* { return Sdk::last_error_slot().c_str(); }
} // namespace

extern "C" UE4SS_EXPORT mbm_status mbm_mod_entry_v1(const mbm_host_v1* host, uint64_t mod_token, const mbm_mod_v1** out_mod)
{
    if (!host || !out_mod) return MBM_E_INVALID;
    if (host->magic != MBM_ABI_MAGIC || host->abi_version != MBM_ABI_VERSION || host->struct_size < sizeof(mbm_host_v1)) return MBM_E_INVALID;
    *out_mod = nullptr;
    RC::Sdk::globals() = RC::Sdk::Globals{host, mod_token};
    CppUserModBase* mod{};
    if (const auto s = RC::Sdk::contain([&] { mod = start_mod(); }); s != MBM_OK) return s;
    if (!mod) return RC::Sdk::record_fault("start_mod returned null");
    auto* i = new (std::nothrow) ModInstance{};
    if (!i)
    {
        RC::Sdk::contain([&] { uninstall_mod(mod); });
        return RC::Sdk::record_fault("out of memory creating the mod table");
    }
    i->mod = mod;
    const auto s = RC::Sdk::contain([&] {
        i->name = RC::Sdk::to_utf8(mod->ModName);
        i->version = RC::Sdk::to_utf8(mod->ModVersion);
        i->authors = RC::Sdk::to_utf8(mod->ModAuthors);
        i->description = RC::Sdk::to_utf8(mod->ModDescription);
        i->sdk = RC::Sdk::to_utf8(mod->ModIntendedSDKVersion);
    });
    if (s != MBM_OK)
    {
        RC::Sdk::contain([&] { uninstall_mod(mod); });
        delete i;
        return s;
    }
    auto& t = i->table;
    t.magic = MBM_ABI_MAGIC;
    t.abi_version = MBM_ABI_VERSION;
    t.struct_size = sizeof(mbm_mod_v1);
    t.name = i->name.c_str();
    t.version = i->version.c_str();
    t.authors = i->authors.c_str();
    t.description = i->description.c_str();
    t.intended_sdk = i->sdk.c_str();
    t.instance = i;
    t.on_program_start = &t_program_start;
    t.on_unreal_init = &t_unreal_init;
    t.on_ui_init = &t_ui_init;
    t.on_update = &t_update;
    t.on_cpp_mods_loaded = &t_cpp_mods_loaded;
    t.on_dll_load = &t_dll_load;
    t.destroy = &t_destroy;
    t.last_error = &t_last_error;
    *out_mod = &t;
    return MBM_OK;
}
