#pragma once

#include <memory>
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
// returned buffer. This only supplies files; it discovers no game addresses
// and establishes neither Unreal readiness nor successful mod execution.
// expected_title is a caller-supplied game/profile fact, never inferred from
// the mod's own load list. No filesystem access occurs in this adapter.
std::shared_ptr<const RC::Platform::NativeModSource>
MakeActivatedUE4SSModSource(ActivatedMods activation, std::string_view expected_title);
}
