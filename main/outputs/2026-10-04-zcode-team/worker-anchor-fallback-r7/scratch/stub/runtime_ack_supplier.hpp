#pragma once
#include <memory>
#include <string_view>
#include "NativeLoaderContext.hpp"
#include "activated_mod_source.hpp"
#include "runtime_owner.hpp"
namespace modbridge { namespace native {
std::unique_ptr<RuntimeOwner> StartActivatedRuntimeWithAck(
    RC::Platform::NativeProgramContext &&, ActivatedMods &&,
    std::string_view, const char *);
bool StopRuntimeWithAck(std::unique_ptr<RuntimeOwner> &);
}}  // namespace modbridge::native
