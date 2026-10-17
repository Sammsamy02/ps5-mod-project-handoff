#pragma once
#include <optional>
#include <string_view>

namespace RC::Platform::NativeLayouts {
// Immutable pair selected at build time. The normal initializer still authenticates
// the fresh image, base, lifetime, axes and required facts before publication.
struct CompiledNativeLayoutDeclarations {
    std::string_view members;
    std::string_view vtables;
    std::string_view selection_source;
    std::string_view profile_digest;
};
}

// Explicit selection only: header presence alone never enables a provider.
// The generated header defines an inline constexpr kCompiledNativeLayoutDeclarations
// in RC::Platform::NativeLayouts using the type declared above.
#ifdef RC_NATIVE_LAYOUT_DECLARATIONS_HEADER
#include <RuntimeBuildProfile.hpp>
#include RC_NATIVE_LAYOUT_DECLARATIONS_HEADER
static_assert(RC::Platform::NativeLayouts::kCompiledNativeLayoutDeclarations.profile_digest ==
                  std::string_view{rc::runtimeprofile::kDigest},
              "Compiled native declarations must match the selected runtime profile digest");
#endif

namespace RC::Platform::NativeLayouts {
inline constexpr auto GetCompiledNativeLayoutDeclarations()
    -> std::optional<CompiledNativeLayoutDeclarations> {
#ifdef RC_NATIVE_LAYOUT_DECLARATIONS_HEADER
    return kCompiledNativeLayoutDeclarations;
#else
    return std::nullopt;
#endif
}
}
