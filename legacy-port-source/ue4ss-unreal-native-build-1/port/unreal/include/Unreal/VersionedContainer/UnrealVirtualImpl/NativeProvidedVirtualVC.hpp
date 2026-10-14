#pragma once
#include <cstdint>
#include <NativeLayoutBinding.hpp>
#include <Unreal/UnrealVersion.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtualBaseVC.hpp>

namespace RC::Unreal {
// Uses genuine supplied maps staged by the existing Program file-loading path.
// The binder checks known runtime axes and publishes those maps; this class
// never selects or fills member/virtual defaults from the PDB-derived bodies.
class NativeProvidedVirtualVC final : public UnrealVirtualBaseVC {
public:
    auto set_virtual_offsets() -> void override {
        RC::Platform::NativeLayouts::PublishStagedNativeLayoutMaps(
            static_cast<std::uint32_t>(Version::Major),
            static_cast<std::uint32_t>(Version::Minor),
            Version::NonShipping, Version::DebugBuild);
    }
};
}
