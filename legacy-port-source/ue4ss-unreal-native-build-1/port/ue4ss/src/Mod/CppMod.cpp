#include <filesystem>
#include <cstdio>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <Helpers/SysError.hpp>
#else
#include <Platform/CppModModule.hpp>
#ifdef __PROSPERO__
#include <Platform/NativeCppModPath.hpp>
#include <Platform/NativeCppMod.hpp>
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
            const auto selected = Platform::FindNativeCppModModule(m_dlls_path, Unicode::EncodeUtf8(m_mod_name));
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
            auto start = reinterpret_cast<start_type>(module->FindExport("start_mod"));
            auto uninstall = reinterpret_cast<uninstall_type>(module->FindExport("uninstall_mod"));
            m_start_mod_func = start;
            m_uninstall_mod_func = uninstall;
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

#ifdef __PROSPERO__
    namespace
    {
        auto checked_native_mod(const std::shared_ptr<const Platform::NativeCppMod>& native)
            -> const Platform::NativeCppMod&
        {
            if (!native) throw std::invalid_argument("Native C++ mod adoption requires a prepared module");
            return *native;
        }
    }
    CppMod::CppMod(UE4SSProgram& program, std::shared_ptr<const Platform::NativeCppMod> native)
        : Mod(program, Unicode::DecodeUtf8(checked_native_mod(native).name()),
              std::filesystem::path{checked_native_mod(native).path()}),
          m_prepared_native_mod(std::move(native))
    {
        m_dlls_path = m_mod_path / "dlls";
        m_dll_filename = Unicode::DecodeUtf8(m_prepared_native_mod->module_path().filename().native());
        m_start_mod_func = m_prepared_native_mod->start_function();
        m_uninstall_mod_func = m_prepared_native_mod->uninstall_function();
        // Exports and ownership were already committed during activation.
        // Reinstall reuses the same owner and performs no filesystem access.
        try { program.fire_dll_load_for_cpp_mods(Unicode::DecodeUtf8(m_prepared_native_mod->module_path().native())); }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "[UE4SS native module] Loaded module notification failed: %s\n", error.what());
        }
        catch (...) { std::fputs("[UE4SS native module] Loaded module notification failed\n", stderr); }
    }
#endif

    auto CppMod::start_mod() -> void
    {
#ifndef _WIN32
        if (!is_installable() ||
#ifdef __PROSPERO__
            (!m_platform_module && !m_prepared_native_mod) ||
#else
            !m_platform_module ||
#endif
            !m_start_mod_func)
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
        if (m_mod)
        {
            m_mod->on_unreal_init();
        }
    }

    auto CppMod::fire_ui_init() -> void
    {
        if (m_mod)
        {
            m_mod->on_ui_init();
        }
    }

    auto CppMod::fire_program_start() -> void
    {
        if (m_mod)
        {
            m_mod->on_program_start();
        }
    }

    auto CppMod::fire_update() -> void
    {
        if (m_mod)
        {
            m_mod->on_update();
        }
    }

    auto CppMod::fire_dll_load(StringViewType dll_name) -> void
    {
        if (m_mod)
        {
            m_mod->on_dll_load(dll_name);
        }
    }

    auto CppMod::fire_on_cpp_mods_loaded() -> void
    {
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
        m_platform_module.reset();
#endif
    }
} // namespace RC
