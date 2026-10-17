#include <filesystem>
#include <cstdio>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <Helpers/SysError.hpp>
#else
#include <Platform/CppModModule.hpp>
#ifdef __PROSPERO__
#include <Platform/NativeCppModPath.hpp>
// [CAP09] ABI-v1 native mod host (engine-free core + runtime binding).
#include "NativeModEngineServices.inc"
#include <unordered_map>
#endif
#include <Helpers/Utf16Utf8.hpp>
#include <UE4SSProgram.hpp>
#endif
#include <DynamicOutput/Output.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Helpers/String.hpp>
#include <Mod/CppMod.hpp>

namespace RC
{
#ifdef __PROSPERO__
    namespace
    {
        // [CAP09] Per-CppMod ABI-v1 state, kept outside the class so CppMod.hpp
        // (included by many units) keeps its exact layout.
        struct NativeAbiState
        {
            mbm_mod_entry_v1_fn entry{};
            uint64_t token{};
        };
        auto native_abi_states() -> std::unordered_map<const CppMod*, NativeAbiState>&
        {
            static std::unordered_map<const CppMod*, NativeAbiState> states;
            return states;
        }
        std::mutex g_native_abi_mutex;
        auto native_abi_token(const CppMod* mod) -> uint64_t
        {
            std::lock_guard g{g_native_abi_mutex};
            auto it = native_abi_states().find(mod);
            return it == native_abi_states().end() ? 0 : it->second.token;
        }
        void native_abi_fire(const CppMod* mod, Platform::NativeMods::Event event, const char* arg = nullptr)
        {
            if (const auto token = native_abi_token(mod)) Platform::NativeMods::runtime_host().fire(token, event, arg);
        }
    } // namespace
#endif

    CppMod::CppMod(UE4SSProgram& program, StringType&& mod_name, StringType&& mod_path)
#ifdef _WIN32
        : Mod(program, std::move(mod_name), std::move(mod_path))
#else
        : Mod(program, std::move(mod_name), std::filesystem::path{Unicode::EncodeUtf8(mod_path)})
#endif
    {
#ifdef _WIN32
        m_dlls_path = m_mod_path / STR("dlls");

        if (!std::filesystem::exists(m_dlls_path))
        {
            Output::send<LogLevel::Warning>(STR("Could not find the dlls folder for mod {}\n"), m_mod_name);
            set_installable(false);
            return;
        }

        auto dll_path = m_dlls_path / STR("main.dll");
        if (!std::filesystem::exists(dll_path))
        {
            dll_path = m_dlls_path / fmt::format(STR("{}.dll"), mod_name);

            if (!std::filesystem::exists(dll_path))
            {
                Output::send<LogLevel::Warning>(STR("Failed to load C++ mod {}, dlls folder must contain either main.dll or {}\n"),
                                                m_mod_name, ensure_str(dll_path.filename()));
                set_installable(false);
                return;
            }
        }

        m_dll_filename = ensure_str(dll_path.filename());

        // Add mods dlls directory to search path for dynamic/shared linked libraries in mods
        m_dlls_path_cookie = AddDllDirectory(m_dlls_path.c_str());
        m_main_dll_module = LoadLibraryExW(dll_path.c_str(), NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

        if (!m_main_dll_module)
        {
            Output::send<LogLevel::Warning>(STR("Failed to load dll <{}> for mod {}, error: {}\n"),
                                            ensure_str(dll_path), m_mod_name, SysError(GetLastError()).c_str());
            set_installable(false);
            return;
        }

        m_start_mod_func = reinterpret_cast<start_type>(GetProcAddress(m_main_dll_module, "start_mod"));
        m_uninstall_mod_func = reinterpret_cast<uninstall_type>(GetProcAddress(m_main_dll_module, "uninstall_mod"));

        if (!m_start_mod_func || !m_uninstall_mod_func)
        {
            Output::send<LogLevel::Warning>(STR("Failed to find exported mod lifecycle functions for mod {}\n"), m_mod_name);

            FreeLibrary(m_main_dll_module);
            m_main_dll_module = NULL;

            set_installable(false);
            return;
        }
#else
        StringType loaded_module_path;
        try
        {
            // Keep the existing mod layout. A binary adapter must supply a native
            // artifact here; the native module owner explicitly refuses Windows PE.
            m_dlls_path = m_mod_path / "dlls";
#ifdef __PROSPERO__
            // [CAP09 F1] error_code lookup (staged NativeCppModPath.hpp): an OS
            // error refuses this mod with its error number instead of throwing
            // a filesystem_error.
            std::error_code lookup_error;
            const auto selected = Platform::FindNativeCppModModule(m_dlls_path, Unicode::EncodeUtf8(m_mod_name), &lookup_error);
            if (lookup_error)
                throw std::runtime_error("Native C++ mod dlls folder could not be checked (OS error " + std::to_string(lookup_error.value()) + ")");
            if (!selected)
                throw std::runtime_error("Native C++ mod dlls folder contains no PRX, SPRX, or legacy module candidate");
            const auto dll_path = *selected;
#else
            auto dll_path = m_dlls_path / "main.dll";
            if (!std::filesystem::exists(dll_path))
                dll_path = m_dlls_path / (Unicode::EncodeUtf8(m_mod_name) + ".dll");
#endif
            m_dll_filename = Unicode::DecodeUtf8(dll_path.filename().native());
            loaded_module_path = Unicode::DecodeUtf8(dll_path.native());
            auto module = Platform::CppModModule::Open(dll_path);
#ifdef __PROSPERO__
            // [CAP09] Only the ABI-v1 entry is admitted: the runtime exports no
            // UE4SS C++ symbol, so a start_mod-only module (UE4SS C++ ABI) is
            // refused with a rebuild instruction instead of being called.
            auto selection = Platform::NativeMods::select_entry(*module);
            if (selection.kind != Platform::NativeMods::EntryKind::AbiV1)
                throw std::runtime_error(selection.reason);
            {
                std::lock_guard g{g_native_abi_mutex};
                native_abi_states()[this] = NativeAbiState{selection.entry, 0};
            }
#else
            auto start = reinterpret_cast<start_type>(module->FindExport("start_mod"));
            auto uninstall = reinterpret_cast<uninstall_type>(module->FindExport("uninstall_mod"));
            m_start_mod_func = start;
            m_uninstall_mod_func = uninstall;
#endif
            m_platform_module = std::move(module);
        }
        catch (const std::exception& error)
        {
            set_installable(false);
            Output::send<LogLevel::Warning>(STR("Native C++ mod {} could not be loaded: {}\n"),
                                            m_mod_name, ensure_str(error.what()));
        }
        if (m_platform_module)
        {
            // Notify only after usable exports and module ownership commit.
            // Existing listeners may fail; that does not undo a real load.
            try { program.fire_dll_load_for_cpp_mods(loaded_module_path); }
            catch (const std::exception& error)
            {
                std::fprintf(stderr, "[UE4SS native module] Loaded module notification failed: %s\n", error.what());
            }
            catch (...)
            {
                std::fputs("[UE4SS native module] Loaded module notification failed\n", stderr);
            }
        }
#endif
    }

    auto CppMod::start_mod() -> void
    {
#ifdef __PROSPERO__
        // [CAP09] ABI-v1: attach through the native mod host (validates the mod
        // table, isolates faults, owns every registration).
        if (!is_installable() || !m_platform_module)
            throw std::logic_error("Native C++ mod is not loaded");
        if (m_is_started) return;
        mbm_mod_entry_v1_fn entry{};
        {
            std::lock_guard g{g_native_abi_mutex};
            auto it = native_abi_states().find(this);
            if (it != native_abi_states().end()) entry = it->second.entry;
        }
        if (!entry) throw std::logic_error("Native C++ mod has no admitted entry");
        uint64_t token{};
        std::string why;
        const auto status = Platform::NativeMods::runtime_host().attach(Unicode::EncodeUtf8(m_mod_name), entry, &token, &why);
        if (status == MBM_OK)
        {
            std::lock_guard g{g_native_abi_mutex};
            native_abi_states()[this].token = token;
            m_is_started = true;
        }
        else
        {
            Output::send<LogLevel::Warning>(STR("Native C++ mod {} refused at start: {}\n"), m_mod_name, Unicode::DecodeUtf8(why));
        }
        return;
#endif
#ifndef _WIN32
        if (!is_installable() || !m_platform_module || !m_start_mod_func)
            throw std::logic_error("Native C++ mod is not loaded");
        if (m_is_started) return;
#endif
        try
        {
            m_mod = m_start_mod_func();
            m_is_started = m_mod != nullptr;
        }
        catch (std::exception& e)
        {
            if (!Output::has_internal_error())
            {
                Output::send<LogLevel::Warning>(STR("Failed to load dll <{}> for mod {}, because: {}\n"),
                                                ensure_str((m_dlls_path / m_dll_filename)),
                                                m_mod_name,
                                                ensure_str(e.what()));
            }
            else
            {
#ifdef _WIN32
                printf_s("Internal Error: %s\n", e.what());
#else
                std::printf("Internal Error: %s\n", e.what());
#endif
            }
        }
    }

    auto CppMod::uninstall() -> void
    {
        Output::send(STR("Stopping C++ mod '{}' for uninstall\n"), m_mod_name);
#ifdef __PROSPERO__
        // [CAP09] revoke -> drain -> destroy, owned by the host.
        if (const auto token = native_abi_token(this))
        {
            const auto final_state = Platform::NativeMods::runtime_host().stop(token);
            if (final_state == Platform::NativeMods::ModState::Stopped || final_state == Platform::NativeMods::ModState::Retained) m_is_started = false;
        }
        return;
#endif
        if (m_mod && m_uninstall_mod_func)
        {
            m_uninstall_mod_func(m_mod);
#ifndef _WIN32
            // Clear ownership state only after the mod confirms teardown by returning.
            // A throwing uninstall leaves the live state intact for the host failure path.
            m_mod = nullptr;
            m_is_started = false;
#endif
        }
    }

    auto CppMod::fire_on_lua_start(StringViewType mod_name,
                                   LuaMadeSimple::Lua& lua,
                                   LuaMadeSimple::Lua& main_lua,
                                   LuaMadeSimple::Lua& async_lua,
                                   LuaMadeSimple::Lua* hook_lua) -> void
    {
        if (m_mod)
        {
            // Call new API
            m_mod->on_lua_start(mod_name, lua, main_lua, async_lua, hook_lua);

            // Call old deprecated API for backwards compatibility
            std::vector<LuaMadeSimple::Lua*> hook_luas;
            if (hook_lua)
            {
                hook_luas.push_back(hook_lua);
            }
            m_mod->on_lua_start(mod_name, lua, main_lua, async_lua, hook_luas);
        }
    }

    auto CppMod::fire_on_lua_start(LuaMadeSimple::Lua& lua,
                                   LuaMadeSimple::Lua& main_lua,
                                   LuaMadeSimple::Lua& async_lua,
                                   LuaMadeSimple::Lua* hook_lua) -> void
    {
        if (m_mod)
        {
            // Call new API
            m_mod->on_lua_start(lua, main_lua, async_lua, hook_lua);

            // Call old deprecated API for backwards compatibility
            std::vector<LuaMadeSimple::Lua*> hook_luas;
            if (hook_lua)
            {
                hook_luas.push_back(hook_lua);
            }
            m_mod->on_lua_start(lua, main_lua, async_lua, hook_luas);
        }
    }

    auto CppMod::fire_on_lua_stop(StringViewType mod_name,
                                  LuaMadeSimple::Lua& lua,
                                  LuaMadeSimple::Lua& main_lua,
                                  LuaMadeSimple::Lua& async_lua,
                                  LuaMadeSimple::Lua* hook_lua) -> void
    {
        if (m_mod)
        {
            // Call new API
            m_mod->on_lua_stop(mod_name, lua, main_lua, async_lua, hook_lua);

            // Call old deprecated API for backwards compatibility
            std::vector<LuaMadeSimple::Lua*> hook_luas;
            if (hook_lua)
            {
                hook_luas.push_back(hook_lua);
            }
            m_mod->on_lua_stop(mod_name, lua, main_lua, async_lua, hook_luas);
        }
    }

    auto CppMod::fire_on_lua_stop(LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua& main_lua, LuaMadeSimple::Lua& async_lua, LuaMadeSimple::Lua* hook_lua) -> void
    {
        if (m_mod)
        {
            // Call new API
            m_mod->on_lua_stop(lua, main_lua, async_lua, hook_lua);

            // Call old deprecated API for backwards compatibility
            std::vector<LuaMadeSimple::Lua*> hook_luas;
            if (hook_lua)
            {
                hook_luas.push_back(hook_lua);
            }
            m_mod->on_lua_stop(lua, main_lua, async_lua, hook_luas);
        }
    }

    auto CppMod::fire_unreal_init() -> void
    {
#ifdef __PROSPERO__
        // Unreal is initialized from here on: reflection services open up.
        Platform::NativeMods::Runtime::unreal_ready.store(true, std::memory_order_release);
        native_abi_fire(this, Platform::NativeMods::Event::UnrealInit);
        return;
#endif
        if (m_mod)
        {
            m_mod->on_unreal_init();
        }
    }

    auto CppMod::fire_ui_init() -> void
    {
#ifdef __PROSPERO__
        native_abi_fire(this, Platform::NativeMods::Event::UiInit);
        return;
#endif
        if (m_mod)
        {
            m_mod->on_ui_init();
        }
    }

    auto CppMod::fire_program_start() -> void
    {
#ifdef __PROSPERO__
        native_abi_fire(this, Platform::NativeMods::Event::ProgramStart);
        return;
#endif
        if (m_mod)
        {
            m_mod->on_program_start();
        }
    }

    auto CppMod::fire_update() -> void
    {
#ifdef __PROSPERO__
        native_abi_fire(this, Platform::NativeMods::Event::Update);
        return;
#endif
        if (m_mod)
        {
            m_mod->on_update();
        }
    }

    auto CppMod::fire_dll_load(StringViewType dll_name) -> void
    {
#ifdef __PROSPERO__
        native_abi_fire(this, Platform::NativeMods::Event::DllLoad, Unicode::EncodeUtf8(dll_name).c_str());
        return;
#endif
        if (m_mod)
        {
            m_mod->on_dll_load(dll_name);
        }
    }

    auto CppMod::fire_on_cpp_mods_loaded() -> void
    {
#ifdef __PROSPERO__
        native_abi_fire(this, Platform::NativeMods::Event::CppModsLoaded);
        return;
#endif
        if (m_mod)
        {
            m_mod->on_cpp_mods_loaded();
        }
    }

    CppMod::~CppMod()
    {
#ifdef _WIN32
        if (m_main_dll_module)
        {
            FreeLibrary(m_main_dll_module);
            RemoveDllDirectory(m_dlls_path_cookie);
        }
#else
        // The host must have stopped mod callbacks and called uninstall before
        // destruction, as in upstream. Release the native loader owner once.
#ifdef __PROSPERO__
        // [CAP09] ABI-v1 modules stay resident for the process lifetime: a mod
        // may have started its own threads, and a stop that timed out leaves
        // the mod retained. Unloading code that might still run is never safe.
        bool abi_v1{};
        {
            std::lock_guard g{g_native_abi_mutex};
            abi_v1 = native_abi_states().erase(this) != 0;
        }
        if (abi_v1)
        {
            (void)m_platform_module.release(); // intentional: resident until process exit
            return;
        }
#endif
        m_platform_module.reset();
#endif
    }
} // namespace RC
