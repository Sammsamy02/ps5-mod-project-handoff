#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include <Common.hpp>
#ifndef __PROSPERO__
#include <CrashDumper.hpp>
#else
#include <Platform/NativeProgramContext.hpp>
#endif
#include <File/Macros.hpp>
#include <GUI/GUI.hpp>
#include <GUI/GUITab.hpp>
#include <Input/Handler.hpp>
#include <LuaLibrary.hpp>
#include <MProgram.hpp>
#include <Mod/CppMod.hpp>
#include <Mod/LuaMod.hpp>
#include <Mod/Mod.hpp>
#include <SettingsManager.hpp>
#include <Unreal/Core/Containers/Array.hpp>
#include <Unreal/UnrealVersion.hpp>

#include <String/StringType.hpp>

// Used to set up ImGui context and allocator in DLL mods
#define UE4SS_ENABLE_IMGUI()                                                                                                                                   \
    /* Wait for UE4SS to create the imgui context. */                                                                                                          \
    /* Without this, we're setting the context to nullptr and eventually crashing when we use any imgui functions. */                                          \
    {                                                                                                                                                          \
        while ((UE4SSProgram::settings_manager.Debug.DebugConsoleVisible || UE4SSProgram::get_program().m_render_thread.get_id() != std::jthread::id{}) &&     \
               !UE4SSProgram::get_program().get_current_imgui_context())                                                                                       \
        {                                                                                                                                                      \
        }                                                                                                                                                      \
        ImGui::SetCurrentContext(UE4SSProgram::get_current_imgui_context());                                                                                   \
        ImGuiMemAllocFunc alloc_func{};                                                                                                                        \
        ImGuiMemFreeFunc free_func{};                                                                                                                          \
        void* user_data{};                                                                                                                                     \
        UE4SSProgram::get_current_imgui_allocator_functions(&alloc_func, &free_func, &user_data);                                                              \
        ImGui::SetAllocatorFunctions(alloc_func, free_func, user_data);                                                                                        \
    }

namespace RC
{
    namespace Unreal
    {
        class UObject;
        class UObjectBase;
        class UObjectBaseUtility;
        class UWorld;
        class UGameViewportClient;
    } // namespace Unreal

    namespace Output
    {
        class ConsoleDevice;
        class DebugConsoleDevice;
    }

    struct RecognizableStruct
    {
        // Sha1 hash with no salt: "RecognizableString"
        char recognizable_string[41]{"81acd41b7490f7b70ec6455657855733e21d7c0e"};

        LuaLibrary::SetScriptVariableInt32Signature set_script_variable_int32_function;
        LuaLibrary::SetScriptVariableDefaultDataSignature set_script_variable_default_data_function;
        LuaLibrary::CallScriptFunctionSignature call_script_function_function;
        LuaLibrary::IsUE4SSInitializedSignature is_ue4ss_initialized_function;

        // Ensure that we have zero-initialized memory at the end of the struct
        // This means that we can reliably check whether a function exists externally
        uint64_t safety_padding[8]{0};
    };

    struct KeyDownEventData
    {
        // Custom data from the C++ mod.
        // The 'custom_data' variable to UE4SSProgram::register_keydown_event will be used to determine the type of custom_data2.
        uint8_t custom_data{};

        // The C++ mod that created this event.
        CppUserModBase* mod{};
    };

    class UE4SSProgram : public MProgram
    {
      public:
        friend class CppUserModBase; // m_input_handler
#ifdef __PROSPERO__
        friend class CppMod; // success-backed native module notification
#endif

      public:
        constexpr static CharType m_settings_file_name[] = STR("UE4SS-settings.ini");
        constexpr static CharType m_log_file_name[] = STR("UE4SS.log");
        constexpr static CharType m_object_dumper_file_name[] = STR("UE4SS_ObjectDump.txt");

      public:
        RC_UE4SS_API static SettingsManager settings_manager;
        static inline std::atomic_bool cpp_mods_done_loading{};

      public:
#ifdef __PROSPERO__
        std::atomic_bool m_is_program_started{};
#else
        bool m_is_program_started{};
#endif

      protected:
#ifdef HAS_INPUT
        Input::Handler m_input_handler;
#endif
        std::jthread m_event_loop;

      public:
        std::jthread m_render_thread;

      private:
#ifndef __PROSPERO__
        CrashDumper m_crash_dumper{};
#else
        Platform::NativeProgramContext m_native_context;
        // StartupWaitControl is retained in UnrealInitializer static config;
        // its weak cancellation binding must outlive this program safely.
        std::shared_ptr<std::atomic_bool> m_shutdown_requested{std::make_shared<std::atomic_bool>(false)};
        std::recursive_mutex m_engine_ready_callback_mutex;
        std::mutex m_native_startup_mutex;
        bool m_native_mod_start_attempted{};
        bool m_native_init_started{};
        std::atomic_bool m_native_mods_started{};
        bool m_native_owns_output_devices{};
        auto validate_native_services() -> void;
        auto shutdown_native_program() noexcept -> void;
        auto native_mods_enabled() const noexcept -> bool;
#endif

      private:
        std::filesystem::path m_game_path_and_exe_name;
        std::filesystem::path m_root_directory;
        std::filesystem::path m_module_file_path;
        std::filesystem::path m_working_directory;
        std::vector<std::filesystem::path> m_mods_directories;
        std::vector<std::filesystem::path> m_mods_directories_to_remove;
        std::filesystem::path m_game_executable_directory;
        std::filesystem::path m_log_directory;
        std::filesystem::path m_object_dumper_output_directory;
        std::filesystem::path m_default_settings_path_and_file;
        std::filesystem::path m_settings_path_and_file;
        std::filesystem::path m_legacy_root_directory;
        Output::DebugConsoleDevice* m_debug_console_device{};
        Output::ConsoleDevice* m_console_device{};
#ifndef __PROSPERO__
        GUI::DebuggingGUI m_debugging_gui{};
#endif

        using EventCallable = std::function<void()>;
        // Legacy types for backward compatibility with C++ mods
        using LegacyEventCallable = void (*)(void* data);
        struct Event
        {
            LegacyEventCallable callable{};
            void* data{};
        };
        std::vector<EventCallable> m_queued_events{};
        std::mutex m_event_queue_mutex{};
        std::mutex m_render_thread_mutex{};
        std::thread::id m_event_loop_thread_id{};

      private:
#ifndef __PROSPERO__
        std::unique_ptr<PLH::IatHook> m_load_library_a_hook;
        uint64_t m_hook_trampoline_load_library_a;

        std::unique_ptr<PLH::IatHook> m_load_library_ex_a_hook;
        uint64_t m_hook_trampoline_load_library_ex_a;

        std::unique_ptr<PLH::IatHook> m_load_library_w_hook;
        uint64_t m_hook_trampoline_load_library_w;

        std::unique_ptr<PLH::IatHook> m_load_library_ex_w_hook;
        uint64_t m_hook_trampoline_load_library_ex_w;

#endif
      public:
        std::vector<std::unique_ptr<Mod>> m_mods;

        RecognizableStruct m_shared_functions{};

        static inline UE4SSProgram* s_program{};

        bool m_has_game_specific_config{};
#ifdef __PROSPERO__
        std::atomic_bool m_processing_events{};
#else
        bool m_processing_events{};
#endif
        bool m_pause_events_processing{};
        std::atomic_bool m_mods_are_being_touched{};
        bool m_custom_member_variable_layout_loaded{};

      public:
        enum class IsInstalled
        {
            Yes,
            No
        };

        enum class IsStarted
        {
            Yes,
            No
        };

      public:
#ifdef __PROSPERO__
        UE4SSProgram(Platform::NativeProgramContext context, std::initializer_list<BinaryOptions> options);
#else
        UE4SSProgram(const std::filesystem::path& ModuleFilePath, std::initializer_list<BinaryOptions> options);
#endif
        ~UE4SSProgram();
        UE4SSProgram(const UE4SSProgram&) = delete;
        UE4SSProgram(UE4SSProgram&&) = delete;

      private:
        auto setup_paths(const std::filesystem::path& moduleFilePath) -> void;
        enum class FunctionStatus
        {
            Success,
            Failure,
        };
        auto create_emergency_console_for_early_error(File::StringViewType error_message) -> void;
        auto setup_mod_directory_path() -> void;
        auto create_simple_console() -> void;
        auto setup_unreal() -> void;
        auto load_unreal_offsets_from_file() -> void;
        auto share_lua_functions() -> void;
        auto on_program_start() -> void;
        auto setup_unreal_properties() -> void;

      protected:
        auto update() -> void;
        auto setup_cpp_mods() -> void;
        enum class IsInitialStartup
        {
            Yes,
            No
        };
        auto start_cpp_mods(IsInitialStartup = IsInitialStartup::No) -> void;
        auto setup_mods() -> void;
        auto start_lua_mods() -> void;
        auto uninstall_mods() -> void;
        auto fire_unreal_init_for_cpp_mods() -> void;
        auto fire_ui_init_for_cpp_mods() -> void;
        auto fire_program_start_for_cpp_mods() -> void;
        auto fire_dll_load_for_cpp_mods(StringViewType dll_name) -> void;
        auto fire_on_cpp_mods_loaded_for_cpp_mods() -> void;

      public:
        // The loader must request shutdown and wait for its init() caller to
        // return before destroying this object. Engine-hook quiescence remains
        // a separate requirement of the hook owner.
#ifdef __PROSPERO__
        auto request_shutdown() noexcept -> void
        {
            std::lock_guard<std::recursive_mutex> callback_guard(m_engine_ready_callback_mutex);
            m_shutdown_requested->store(true, std::memory_order_release);
            m_processing_events = false;
        }
        // May be called once by the native owner after construction and before
        // init(). It starts C++ mods now and authorizes Lua/mod callbacks in
        // the later update loop. Constructor autostart is separately gated by
        // NativeProgramContext::mod_startup_policy.
        auto start_native_mods() -> void;
        auto native_mod_source() const noexcept -> const std::shared_ptr<const Platform::NativeModSource>&
        {
            return m_native_context.mod_source;
        }
        // Query only after construction/init has returned or its caller joined.
        auto native_startup_error_message() noexcept -> const char*;
#endif
        auto init() -> void;
        auto is_program_started() -> bool;
        auto find_mod_by_id(ModId mod_id) -> Mod*;
        auto find_lua_mod_by_id(ModId mod_id) -> LuaMod*;
        auto queue_reinstall_mods() -> void;
        auto queue_reinstall_mod(LuaMod* mod) -> void;
        auto queue_reinstall_mod(ModId mod_id) -> void;
        auto queue_uninstall_mod(LuaMod* mod) -> void;
        auto queue_uninstall_mod(ModId mod_id) -> void;
        auto queue_reinstall_mod_by_name(const std::string& mod_name) -> void;
        auto queue_reinstall_mod_by_name(std::string_view mod_name) -> void;
        auto queue_uninstall_mod_by_name(const std::string& mod_name) -> void;
        auto queue_uninstall_mod_by_name(std::string_view mod_name) -> void;
        auto queue_start_lua_mod_by_path(const std::filesystem::path& mod_path) -> void;
        auto get_object_dumper_output_directory() -> const File::StringType;
        RC_UE4SS_API auto get_module_directory() -> File::StringType;
        RC_UE4SS_API auto get_game_executable_directory() -> File::StringType;
#ifdef __PROSPERO__
        [[nodiscard]] RC_UE4SS_API auto get_game_executable_path() const -> const std::filesystem::path&;
#endif
        RC_UE4SS_API auto get_working_directory() -> File::StringType;
        RC_UE4SS_API auto get_mods_directory() -> File::StringType;
        RC_UE4SS_API auto get_mods_directories() -> std::vector<std::filesystem::path>&;
        RC_UE4SS_API auto get_mods_txt_entries() -> std::unordered_map<std::string, bool>;
        [[nodiscard]] RC_UE4SS_API auto make_compatible_path(const std::filesystem::path&) const -> std::filesystem::path;
        RC_UE4SS_API auto insert_mods_directory(const std::filesystem::path&, int64_t index) -> void;
        RC_UE4SS_API auto add_mods_directory(const std::filesystem::path&) -> void;
        RC_UE4SS_API auto remove_mods_directory(const std::filesystem::path&) -> void;
        RC_UE4SS_API auto get_legacy_root_directory() -> File::StringType;
        RC_UE4SS_API auto generate_uht_compatible_headers() -> void;
        RC_UE4SS_API auto generate_cxx_headers(const std::filesystem::path& output_dir) -> void;
        RC_UE4SS_API auto generate_lua_types(const std::filesystem::path& output_dir) -> void;
        auto get_debugging_ui() -> GUI::DebuggingGUI&
        {
#ifdef __PROSPERO__
            throw std::runtime_error("Native debugging GUI service is unavailable");
#else
            return m_debugging_gui;
#endif
        };
        auto stop_render_thread() -> void;
        RC_UE4SS_API auto add_gui_tab(std::shared_ptr<GUI::GUITab> tab) -> void;
        RC_UE4SS_API auto remove_gui_tab(std::shared_ptr<GUI::GUITab> tab) -> void;
        RC_UE4SS_API static auto get_current_imgui_context() -> ImGuiContext*
        {
#ifdef __PROSPERO__
            throw std::runtime_error("Native debugging GUI service is unavailable");
#else
            return ImGui::GetCurrentContext();
#endif
        }
        RC_UE4SS_API static auto get_current_imgui_allocator_functions(ImGuiMemAllocFunc* alloc_func, ImGuiMemFreeFunc* free_func, void** user_data) -> void
        {
#ifdef __PROSPERO__
            throw std::runtime_error("Native debugging GUI service is unavailable");
#else
            return ImGui::GetAllocatorFunctions(alloc_func, free_func, user_data);
#endif
        }
        RC_UE4SS_API auto queue_event(EventCallable callable) -> void;
        // Legacy overload for backward compatibility with C++ mods
        RC_UE4SS_API auto queue_event(LegacyEventCallable callable, void* data) -> void;
        RC_UE4SS_API auto is_queue_empty() -> bool;
        RC_UE4SS_API auto can_process_events() -> bool
        {
            return m_processing_events;
        }
        [[nodiscard]] RC_UE4SS_API auto get_event_loop_thread_id() const -> std::thread::id
        {
            return m_event_loop_thread_id;
        }
        RC_UE4SS_API auto is_event_loop_thread() -> bool
        {
            return std::this_thread::get_id() == m_event_loop_thread_id;
        }
        RC_UE4SS_API auto are_mods_being_touched() -> bool
        {
            return m_mods_are_being_touched;
        }
        RC_UE4SS_API auto delete_mod(Mod*) -> void;

      public:
        // API pass-through for use outside the private scope of UE4SSProgram
        RC_UE4SS_API auto register_keydown_event(Input::Key, const Input::EventCallbackCallable&, uint8_t custom_data = 0, void* custom_data2 = nullptr) -> void;
        RC_UE4SS_API auto register_keydown_event(Input::Key,
                                                 const Input::Handler::ModifierKeyArray&,
                                                 const Input::EventCallbackCallable&,
                                                 uint8_t custom_data = 0,
                                                 void* custom_data2 = nullptr) -> void;
        RC_UE4SS_API auto is_keydown_event_registered(Input::Key) -> bool;
        RC_UE4SS_API auto is_keydown_event_registered(Input::Key, const Input::Handler::ModifierKeyArray&) -> bool;
        RC_UE4SS_API auto get_all_input_events(std::function<void(Input::KeySet&)> callback) -> void;
        enum class AllMods
        {
            Yes,
            No,
        };
        // If 'AllMods' is 'Yes', then 'mod' can be nullptr.
        auto unregister_keydown_events_for_lua_mod(LuaMod* mod, AllMods = AllMods::No) -> void;

      private:
        static auto install_cpp_mods() -> void;
        static auto install_lua_mods() -> void;

        using FMBNI_ExtraPredicate = std::function<bool(Mod*)>;
        static auto find_mod_by_name_internal(StringViewType mod_name,
                                              IsInstalled = IsInstalled::No,
                                              IsStarted = IsStarted::No,
                                              FMBNI_ExtraPredicate extra_predicate = {}) -> Mod*;

      public:
        RC_UE4SS_API static auto dump_uobject(Unreal::UObject* object,
                                              std::unordered_set<Unreal::FField*>* dumped_fields,
                                              StringType& out_line,
                                              bool is_below_425,
                                              std::unordered_set<Unreal::UFunction*>* in_dumped_functions = nullptr) -> void;
        RC_UE4SS_API static auto dump_all_objects_and_properties(const File::StringType& output_path_and_file_name) -> void;

        template <typename T>
        static auto find_mod_by_name(StringViewType mod_name, IsInstalled = IsInstalled::No, IsStarted = IsStarted::No) -> T*
        {
            std::abort();
        };
        template <typename T>
        static auto find_mod_by_name(std::string_view mod_name, IsInstalled = IsInstalled::No, IsStarted = IsStarted::No) -> T*
        {
            std::abort();
        };
        template <>
        RC_UE4SS_API auto find_mod_by_name<LuaMod>(StringViewType mod_name, IsInstalled is_installed, IsStarted is_started) -> LuaMod*
        {
            return static_cast<LuaMod*>(find_mod_by_name_internal(mod_name, is_installed, is_started, [](auto elem) -> bool {
                return dynamic_cast<LuaMod*>(elem);
            }));
        }
        template <>
        RC_UE4SS_API auto find_mod_by_name<CppMod>(StringViewType mod_name, IsInstalled is_installed, IsStarted is_started) -> CppMod*
        {
            return static_cast<CppMod*>(find_mod_by_name_internal(mod_name, is_installed, is_started, [](auto elem) -> bool {
                return dynamic_cast<CppMod*>(elem);
            }));
        }
        template <>
        RC_UE4SS_API auto find_mod_by_name<LuaMod>(std::string_view mod_name, IsInstalled is_installed, IsStarted is_started) -> LuaMod*
        {
            return find_mod_by_name<LuaMod>(ensure_str(mod_name), is_installed, is_started);
        }
        template <>
        RC_UE4SS_API auto find_mod_by_name<CppMod>(std::string_view mod_name, IsInstalled is_installed, IsStarted is_started) -> CppMod*
        {
            return find_mod_by_name<CppMod>(ensure_str(mod_name), is_installed, is_started);
        }

        RC_UE4SS_API static auto find_lua_mod_by_name(StringViewType mod_name, IsInstalled = IsInstalled::No, IsStarted = IsStarted::No) -> LuaMod*;
        RC_UE4SS_API static auto find_lua_mod_by_name(std::string_view mod_name, IsInstalled = IsInstalled::No, IsStarted = IsStarted::No) -> LuaMod*;
        RC_UE4SS_API static auto find_lua_mod_by_lua_state(lua_State* lua_state, IsInstalled = IsInstalled::No, IsStarted = IsStarted::No) -> LuaMod*;
        static auto static_cleanup() -> void;
        RC_UE4SS_API static auto get_program() -> UE4SSProgram&
        {
#ifdef __PROSPERO__
            if (!s_program)
                throw std::runtime_error("Native UE4SS program has not passed startup validation");
#endif
            return *s_program;
        }
        RC_UE4SS_API static auto parse_semicolon_separated_string(const StringType& string) -> std::vector<StringType>;

      private:
        friend void* HookedLoadLibraryA(const char* dll_name);
        friend void* HookedLoadLibraryExA(const char* dll_name, void* file, int32_t flags);
        friend void* HookedLoadLibraryW(const wchar_t* dll_name);
        friend void* HookedLoadLibraryExW(const wchar_t* dll_name, void* file, int32_t flags);
        friend auto gui_render_thread_tick() -> void;
    };
} // namespace RC
