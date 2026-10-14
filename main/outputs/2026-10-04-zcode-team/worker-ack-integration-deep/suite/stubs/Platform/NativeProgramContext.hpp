// Host-test stub of the canonical port header
// (ue4ss-unreal-native-build-1/port/ue4ss/include/Platform/NativeProgramContext.hpp).
// Mirrors the exact surface the bootstrap consumes. Only for compiling the
// patched bootstrap sources on the host; never shipped.
#pragma once

#include <filesystem>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <Platform/NativeModSource.hpp>

namespace RC::Input { class PlatformInputSource; }
namespace RC::Unreal::Platform { class ModuleSnapshot; class OwnedMappedScanSource; }

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

    struct NativeProgramContext
    {
        std::filesystem::path module_file_path;
        std::filesystem::path game_executable_path;
        std::shared_ptr<const Unreal::Platform::ModuleSnapshot> module_source;
        std::shared_ptr<const Unreal::Platform::OwnedMappedScanSource> scanner_source;
        std::shared_ptr<const NativeModSource> mod_source;
        NativeModStartupPolicy mod_startup_policy{NativeModStartupPolicy::Deferred};
        std::function<void()> on_engine_initialized;
        std::shared_ptr<Input::PlatformInputSource> input_source;
    };

    inline auto ValidateNativeProgramContext(const NativeProgramContext& context) -> void
    {
        for (const auto* path : {&context.module_file_path, &context.game_executable_path})
        {
            const auto& bytes = path->native();
            if (path->empty() || !path->is_absolute() || !path->has_filename() ||
                path->filename() == "." || path->filename() == ".." ||
                path->lexically_normal() != *path || bytes.find('\0') != std::filesystem::path::string_type::npos)
                throw std::invalid_argument("Native program paths must be absolute normalized file paths without NUL");
        }
        if (!context.module_source)
            throw std::invalid_argument("Native program requires an owning loader module snapshot");
        if (!std::filesystem::is_regular_file(context.module_file_path) ||
            !std::filesystem::is_regular_file(context.game_executable_path))
            throw std::invalid_argument("Native module and executable paths must name existing regular files");
        (void)std::filesystem::file_size(context.game_executable_path);
    }
}

namespace RC::Unreal::Platform
{
    // Host stub: the real retained loader snapshot is console-only.
    class ModuleSnapshot {};
    class OwnedMappedScanSource {};
}
