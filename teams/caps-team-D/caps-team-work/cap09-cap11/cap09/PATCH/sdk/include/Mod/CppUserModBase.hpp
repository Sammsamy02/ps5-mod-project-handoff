// Mod/CppUserModBase.hpp -- PS5 Mod Bridge native mod SDK.
//
// Same class name, members and virtual lifecycle functions as UE4SS's
// CppUserModBase, so an upstream mod's class compiles unchanged for the
// supported subset. The object lives entirely inside the mod module; the SDK
// entry (mbm_sdk_entry.cpp) turns it into the C table of NativeModAbi.h.
//
// Lifecycle timing on the PS5 host:
//   constructor        inside start_mod(), when the runtime loads the mod
//   on_program_start   after the runtime's program start
//   on_unreal_init     after the runtime's Unreal init (objects usable)
//   on_ui_init         when the native GUI host comes up (#11)
//   on_update          every UE4SS event-loop iteration (not the game thread)
//   on_cpp_mods_loaded after every native mod was started
//   on_dll_load        when another native mod module was loaded
//   on_lua_start/stop  declared for source compatibility, NEVER called on PS5
//                      (no Lua state crosses the module boundary in ABI v1)
#pragma once
#include <DynamicOutput/DynamicOutput.hpp>
#include <Input/Handler.hpp>
#include <String/StringType.hpp>
#include <mbm/Sdk.hpp>

#include <memory>
#include <vector>

#ifndef UE4SS_EXPORT
#define UE4SS_EXPORT __attribute__((visibility("default")))
#endif

namespace RC
{
    class CppUserModBase;
    namespace LuaMadeSimple { class Lua; }

    namespace GUI
    {
        class GUITab
        {
          public:
            using RenderFunctionType = void (*)(CppUserModBase*);
        };
    } // namespace GUI

    struct ModMetadata
    {
        const StringType ModName{};
        const StringType ModVersion{};
        const StringType ModDescription{};
        const StringType ModAuthors{};
        const StringType ModIntendedSDKVersion{};
    };

    class CppUserModBase
    {
      protected:
        std::vector<std::shared_ptr<GUI::GUITab>> GUITabs{};

      public:
        StringType ModName{};
        StringType ModVersion{};
        StringType ModDescription{};
        StringType ModAuthors{};
        StringType ModIntendedSDKVersion{};

      public:
        CppUserModBase();
        virtual ~CppUserModBase();

      public:
        virtual auto on_update() -> void {}
        virtual auto on_unreal_init() -> void {}
        virtual auto on_ui_init() -> void {}
        virtual auto on_program_start() -> void {}
        virtual auto on_dll_load(StringViewType dll_name) -> void { (void)dll_name; }
        virtual auto render_tab() -> void {}
        virtual auto on_cpp_mods_loaded() -> void {}
        virtual auto on_lua_start(StringViewType, LuaMadeSimple::Lua&, LuaMadeSimple::Lua&, LuaMadeSimple::Lua&, LuaMadeSimple::Lua*) -> void {}
        virtual auto on_lua_start(LuaMadeSimple::Lua&, LuaMadeSimple::Lua&, LuaMadeSimple::Lua&, LuaMadeSimple::Lua*) -> void {}
        virtual auto on_lua_stop(StringViewType, LuaMadeSimple::Lua&, LuaMadeSimple::Lua&, LuaMadeSimple::Lua&, LuaMadeSimple::Lua*) -> void {}
        virtual auto on_lua_stop(LuaMadeSimple::Lua&, LuaMadeSimple::Lua&, LuaMadeSimple::Lua&, LuaMadeSimple::Lua*) -> void {}

      protected:
        // GUI tab (#11): the callback runs on the game thread inside the host's
        // ImGui frame. Refused (logged) when the host has no GUI.
        auto register_tab(StringViewType tab_name, GUI::GUITab::RenderFunctionType render_function) -> void;
        auto register_keydown_event(Input::Key key, const Input::EventCallbackCallable& callback, uint8_t custom_data = 0) -> void;
        auto register_keydown_event(Input::Key key, const Input::Handler::ModifierKeyArray& modifier_keys,
                                    const Input::EventCallbackCallable& callback, uint8_t custom_data = 0) -> void;

      public:
        // SDK extension: a per-frame game-thread callback (data-only pump).
        auto register_game_thread_tick(std::function<void(float)> fn) -> bool;
    };
} // namespace RC
