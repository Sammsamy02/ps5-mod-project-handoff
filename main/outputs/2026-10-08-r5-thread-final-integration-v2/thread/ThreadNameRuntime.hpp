#pragma once
#include "NativeThreadNameAdapter.hpp"
#include <Unreal/UnrealInitializer.hpp>
namespace NativeNameThread {
// Invoked inside genuine initializer ownership before any seeds/maps publish.
void prepare_runtime(const RC::Unreal::UnrealInitializer::Config&,
                     const rc::codecache::Metadata*,const Data&,const char*,std::uint64_t,
                     bool exact_postpackage_import_admitted);
Acquired acquire_runtime() noexcept;
// False requires continued runtime/code residence; outstanding same-thread
// callback tickets are not destroyed or relabeled as a completed teardown.
bool retire_runtime() noexcept;
}
