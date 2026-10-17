#pragma once

#include <filesystem>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <Unreal/PlatformModuleSnapshot.hpp>
#include <Platform/NativeModSource.hpp>

namespace RC::Input { class PlatformInputSource; }
namespace RC::Unreal::Platform { class OwnedMappedScanSource; }

namespace RC::Platform
{
    enum class NativeModStartupPolicy : uint8_t
    {
        Deferred,
        Autostart,
    };

    enum class NativeModStartupBlock : uint8_t
    {
        None,
        ProgramNotPublished,
        ExistingError,
        ShuttingDown,
        InitializationStarted,
        AlreadyAttempted,
    };

    constexpr auto ShouldAutostartNativeMods(NativeModStartupPolicy policy) noexcept -> bool
    {
        return policy == NativeModStartupPolicy::Autostart;
    }

    constexpr auto ShouldRunNativeModCallbacks(bool native_platform,
                                                bool native_mods_started) noexcept -> bool
    {
        return !native_platform || native_mods_started;
    }

    // A small, side-effect-free gate shared by the native bootstrap and tests.
    constexpr auto CheckNativeModStartup(bool program_published, bool has_error,
                                        bool shutting_down, bool initialization_started,
                                        bool startup_attempted) noexcept -> NativeModStartupBlock
    {
        if (!program_published) return NativeModStartupBlock::ProgramNotPublished;
        if (has_error) return NativeModStartupBlock::ExistingError;
        if (shutting_down) return NativeModStartupBlock::ShuttingDown;
        if (initialization_started) return NativeModStartupBlock::InitializationStarted;
        if (startup_attempted) return NativeModStartupBlock::AlreadyAttempted;
        return NativeModStartupBlock::None;
    }

    // Native loader facts, not process discovery. The loader must accurately
    // bind these paths to the retained module snapshot. Lexical validation below
    // cannot establish that identity or verify mapping permissions.
    struct NativeProgramContext
    {
        std::filesystem::path module_file_path;
        std::filesystem::path game_executable_path;
        std::shared_ptr<const Unreal::Platform::ModuleSnapshot> module_source;
        // Optional concrete scanner owner for the selected native ps_scan
        // backend. It must describe the same retained loader module snapshot.
        std::shared_ptr<const Unreal::Platform::OwnedMappedScanSource> scanner_source;
        // Optional retained activation files. Presence selects immutable logical
        // ue4ss/ keys for config and mod loading, with no disk fallback.
        std::shared_ptr<const NativeModSource> mod_source;
        // Resolution B (configured roots, 2026-10-04): explicit directory roots
        // supplied by the native bootstrap. CONFIGURATION values, never
        // kernel-observed paths; distinct from game-image identity (mod_source
        // + retained VmScan anchor facts) and from the runtime-module path
        // (module_file_path). When game_executable_path is empty (genuinely
        // unobservable on this target), configured_game_directory is REQUIRED
        // and carries the directory derivations.
        std::filesystem::path configured_game_directory;
        std::filesystem::path configured_working_directory;
        // Deferred avoids enumerating, installing, or starting either mod type
        // during construction. A bootstrap may call start_native_mods() once
        // before init(), or deliberately opt into constructor autostart.
        NativeModStartupPolicy mod_startup_policy{NativeModStartupPolicy::Deferred};
        // Notified on the init caller thread after Unreal setup, property setup,
        // and Lua function sharing, but before the update loop is launched.
        std::function<void()> on_engine_initialized;
        // Optional actual source. Its provider must support serialized activation
        // on the constructing thread, exclusive processing on UE4SS's update
        // thread, and deactivation after that thread has joined.
        std::shared_ptr<Input::PlatformInputSource> input_source;
    };

    inline auto ValidateNativeProgramContext(const NativeProgramContext& context) -> void
    {
        auto lexical_path_valid = [](const std::filesystem::path& path) {
            const auto& bytes = path.native();
            return !(path.empty() || !path.is_absolute() || !path.has_filename() ||
                     path.filename() == "." || path.filename() == ".." ||
                     path.lexically_normal() != path ||
                     bytes.find('\0') != std::filesystem::path::string_type::npos);
        };
        // Runtime-module path: the hard lexical gate is UNCHANGED.
        if (!lexical_path_valid(context.module_file_path))
            throw std::invalid_argument("Native program paths must be absolute normalized file paths without NUL");
        const bool has_game_executable = !context.game_executable_path.empty();
        if (has_game_executable)
        {
            // An observed executable path keeps the original hard lexical gate.
            if (!lexical_path_valid(context.game_executable_path))
                throw std::invalid_argument("Native program paths must be absolute normalized file paths without NUL");
        }
        else
        {
            // Resolution B: no observed executable path exists on this target.
            // The configured game directory is then REQUIRED and must be a
            // clean absolute path; configured roots are DIRECTORIES: no
            // regular-file requirement, no fabrication of a file path.
            if (context.configured_game_directory.empty() ||
                !lexical_path_valid(context.configured_game_directory))
                throw std::invalid_argument("Native program requires an observed game executable path or an explicit configured game directory");
            if (!context.configured_working_directory.empty() &&
                !lexical_path_valid(context.configured_working_directory))
                throw std::invalid_argument("Native configured roots must be absolute normalized directory paths without NUL");
        }
        if (!context.module_source)
            throw std::invalid_argument("Native program requires an owning loader module snapshot");
        // File availability is a narrow startup prerequisite. It establishes
        // neither image identity nor a mapping-to-path relationship.
        // Runtime-module file gate UNCHANGED (proven stat-able in-process).
        // The executable-file gate applies only when a path was OBSERVED;
        // configured directories never pass through is_regular_file.
        if (!std::filesystem::is_regular_file(context.module_file_path) ||
            (has_game_executable && !std::filesystem::is_regular_file(context.game_executable_path)))
            throw std::invalid_argument("Native module and executable paths must name existing regular files");
        if (has_game_executable)
            (void)std::filesystem::file_size(context.game_executable_path);
    }
}
