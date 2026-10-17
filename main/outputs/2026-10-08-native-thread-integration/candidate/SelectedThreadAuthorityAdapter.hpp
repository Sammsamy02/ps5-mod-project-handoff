#pragma once
#include "NativeThreadAuthority.hpp"
#include <Unreal/UnrealInitializer.hpp>
namespace NativeThreadIntegration {
struct SelectionReport {
 Prepared prepared;
 bool same_current_source=false,initializer_metadata=false;
 bool cooperative_cancellation_available=false;
 bool import_authority_missing=true,mapping_lifetime_missing=true;
};
// Called during the actual initializer owner scope, never from a callback.
SelectionReport prepare_selected_source(const RC::Unreal::UnrealInitializer::Config&,
                                       const Profile&,const char*,std::uint64_t) noexcept;
void require_selected_authority(const RC::Unreal::UnrealInitializer::Config&,
                               const Profile&,const char*,std::uint64_t);
}
