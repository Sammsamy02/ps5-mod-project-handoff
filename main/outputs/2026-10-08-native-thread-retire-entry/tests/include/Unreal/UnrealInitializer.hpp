#pragma once
#include <Unreal/PlatformModuleSnapshot.hpp>
#include <Unreal/StartupWaitControl.hpp>
// Host projection of the three actual Config fields consumed by this provider.
// Target compilation uses the complete selected Config declaration instead.
namespace RC::Unreal::Platform {class OwnedMappedScanSource;}
namespace RC::Unreal::UnrealInitializer {
struct Config {
 std::shared_ptr<const Platform::ModuleSnapshot> ModuleSource;
 std::shared_ptr<const Platform::OwnedMappedScanSource> NativeScanSource;
 std::shared_ptr<const StartupWaitControl> StartupWait;
};
}
