#pragma once

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <Unreal/PlatformModuleSnapshot.hpp>

namespace RC::Input { class PlatformInputSource; }
namespace RC::Unreal::Platform { class OwnedMappedScanSource; }

namespace RC::Platform
{
    // Native loader facts, not process discovery. The loader must accurately
    // bind these paths to the retained module snapshot. Lexical validation below
    // cannot establish that identity or verify mapping permissions.
    struct NativeProgramContext
    {
        std::filesystem::path module_file_path;
        std::filesystem::path game_executable_path;
        std::shared_ptr<const Unreal::Platform::ModuleSnapshot> module_source;
        // Must describe this same module owner when native Unreal scanning starts.
        std::shared_ptr<const Unreal::Platform::OwnedMappedScanSource> scanner_source;
        // Optional actual source. Its provider must support serialized activation
        // on the constructing thread, exclusive processing on UE4SS's update
        // thread, and deactivation after that thread has joined.
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
        // File availability is a narrow startup prerequisite. It establishes
        // neither image identity nor a mapping-to-path relationship.
        if (!std::filesystem::is_regular_file(context.module_file_path) ||
            !std::filesystem::is_regular_file(context.game_executable_path))
            throw std::invalid_argument("Native module and executable paths must name existing regular files");
        (void)std::filesystem::file_size(context.game_executable_path);
    }
}
