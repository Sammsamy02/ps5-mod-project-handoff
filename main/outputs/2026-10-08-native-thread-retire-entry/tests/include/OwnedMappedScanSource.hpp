#pragma once
#include <Unreal/PlatformModuleSnapshot.hpp>
// Explicit host supplier-owner fixture; no scan, engine access or VM authority.
namespace RC::Unreal::Platform {
class OwnedMappedScanSource final {
 std::shared_ptr<const ModuleSnapshot> owner_;
public:
 explicit OwnedMappedScanSource(std::shared_ptr<const ModuleSnapshot> o):owner_(std::move(o)){}
 const std::shared_ptr<const ModuleSnapshot>& source_owner() const noexcept{return owner_;}
};
}
