#pragma once

#include <memory>
#include <filesystem>
#include <string_view>

struct mfa_activation;
namespace RC::Platform { class NativeModSource; }

namespace modbridge::native
{
struct ActivationDeleter
{
    void operator()(mfa_activation* activation) const noexcept;
};
using ActivatedMods = std::unique_ptr<mfa_activation, ActivationDeleter>;

// Consumes the activation produced during startup and retains it for every
// returned buffer and loads listed native UE4SS modules during preparation.
// It discovers no game addresses and establishes neither Unreal readiness nor
// successful mod execution.
// expected_title is a caller-supplied game/profile fact, never inferred from
// the mod's own load list. Native entries require a caller-supplied absolute
// activation_root naming that same installation. Only their explicitly listed
// selected files are opened, eagerly; buffers are never passed as native code.
// No root is needed for Lua-only activation. Binary/ABI compatibility remains
// the responsibility of the source-built module and the actual native loader.
std::shared_ptr<const RC::Platform::NativeModSource>
MakeActivatedUE4SSModSource(ActivatedMods activation, std::string_view expected_title,
                           const std::filesystem::path& activation_root = {});
}
