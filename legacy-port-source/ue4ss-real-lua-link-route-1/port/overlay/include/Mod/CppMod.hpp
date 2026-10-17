#pragma once

#include <memory>
#include <vector>

#ifdef _WIN32
#include <Unreal/Core/Windows/MinimalWindowsApi.hpp>
#include <Mod/CppUserModBase.hpp>
#else
namespace RC::Platform { class CppModModule; }
#endif

#include <Mod/Mod.hpp>

#include <String/StringType.hpp>

namespace RC
{
#ifndef _WIN32
    class CppUserModBase;
#endif

    namespace LuaMadeSimple
    {
        class Lua;
    }

    class CppMod : public Mod
    {
      private:
        typedef CppUserModBase* (*start_type)();
        typedef void (*uninstall_type)(CppUserModBase*);

      private:
        StringType m_dll_filename{};
        std::filesystem::path m_dlls_path;

#ifdef _WIN32
        Unreal::Windows::HMODULE m_main_dll_module = NULL;
        void* m_dlls_path_cookie = NULL;
#else
        // Opaque native loader ownership; construction/destruction live in the platform loader.
        std::unique_ptr<Platform::CppModModule> m_platform_module;
#endif
        start_type m_start_mod_func = nullptr;
        uninstall_type m_uninstall_mod_func = nullptr;

        CppUserModBase* m_mod = nullptr;

      public:
        CppMod(UE4SSProgram&, StringType&& mod_name, StringType&& mod_path);
        CppMod(CppMod&) = delete;
        CppMod(CppMod&&) = delete;
        ~CppMod() override;

      public:
        auto start_mod() -> void override;
        auto uninstall() -> void override;

        auto fire_on_lua_start(StringViewType mod_name,
                               LuaMadeSimple::Lua& lua,
                               LuaMadeSimple::Lua& main_lua,
                               LuaMadeSimple::Lua& async_lua,
                               LuaMadeSimple::Lua* hook_lua) -> void;

        auto fire_on_lua_start(LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua& main_lua, LuaMadeSimple::Lua& async_lua, LuaMadeSimple::Lua* hook_lua) -> void;

        auto fire_on_lua_stop(StringViewType mod_name,
                              LuaMadeSimple::Lua& lua,
                              LuaMadeSimple::Lua& main_lua,
                              LuaMadeSimple::Lua& async_lua,
                              LuaMadeSimple::Lua* hook_lua) -> void;

        auto fire_on_lua_stop(LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua& main_lua, LuaMadeSimple::Lua& async_lua, LuaMadeSimple::Lua* hook_lua) -> void;

        auto fire_unreal_init() -> void override;
        auto fire_ui_init() -> void override;
        auto fire_program_start() -> void override;
        auto fire_update() -> void override;
        auto fire_dll_load(StringViewType dll_name) -> void;
        auto fire_on_cpp_mods_loaded() -> void;
    };
} // namespace RC
