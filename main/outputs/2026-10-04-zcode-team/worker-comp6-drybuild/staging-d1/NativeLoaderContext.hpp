#pragma once
#include "OwnedMappedScanSource.hpp"  // comp6-drybuild: quoted -> staging intercept copy
namespace RC::Platform {
// Same-SDK C++ interface, not a portable C/STL ABI. A legitimate in-process
// loader supplies actual retained mappings and already-existing owned-service
// registrations. This factory never registers pointers or discovers modules.
struct NativeLoaderFacts {
    std::filesystem::path module_file_path;
    std::filesystem::path game_executable_path;
    Unreal::Platform::ModuleInput modules;
    Unreal::Platform::OwnedMappedImageFacts mapped;
    std::shared_ptr<Input::PlatformInputSource> input_source;
    Unreal::Platform::CandidateSelectionPolicy scan_policy{Unreal::Platform::CandidateSelectionPolicy::OriginalOnly};
};
// ModuleInput.lifetime must retain ALL supplied bytes, registrations, resolver
// state and actual export callback lifetime. Paths/markers/ranges prove no engine
// identity or ABI. External writes/unmap/protect must remain coordinated by the
// loader beyond the scanner's synchronous lease. MainExe metadata extents must
// describe supplied actual mapped bytes; missing metadata is not invented.
auto MakeNativeProgramContext(NativeLoaderFacts) -> NativeProgramContext;
// Optional serialized source is privately copied and checked against this exact
// retained mapped owner. Unrelated mutable state is not an identity comparison.
// No file acquisition or live mapping discovery is performed here.
auto MakeNativeProgramContext(NativeLoaderFacts, const std::vector<uint8_t>& serialized_elf) -> NativeProgramContext;
}
