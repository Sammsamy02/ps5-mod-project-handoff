#pragma once
#include "runtime_owner.hpp"
#include "activated_mod_source.hpp"
#include <Platform/NativeProgramContext.hpp>

namespace modbridge::native
{
// Creates the actual native UE4SSProgram, with mod autostart disabled. The
// caller supplies a genuine, validated, owning native module context; this
// function does not discover mappings or prove their identity/ABI. Construction
// performs UE4SS settings/path/log setup and must run while those are available.
// The returned owner has not called init(). Engine readiness is reported only
// by the real program callback after setup_unreal/properties/Lua sharing.
std::unique_ptr<RuntimeOwner> PrepareDeferredUE4SSRuntime(RC::Platform::NativeProgramContext context);

// Opt-in inventory path for a validated startup activation. Construction
// discovers the buffered Lua inventory; main.lua still runs only after the
// real Unreal initialization performed by start(). Native C++ entries are loaded
// and resolved eagerly and use the existing pre-Unreal start_mod lifecycle.
// Their exact installation root is required only when native entries are listed.
// Both factories require a genuine game context; no other framework ABI is admitted.
std::unique_ptr<RuntimeOwner> PrepareActivatedUE4SSRuntime(
    RC::Platform::NativeProgramContext context, ActivatedMods activation,
    std::string_view expected_title, const std::filesystem::path& activation_root = {});
}
