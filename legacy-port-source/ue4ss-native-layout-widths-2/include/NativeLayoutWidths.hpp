#pragma once
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
namespace RC::Platform::NativeLayouts {
// Width of the current compiled port's genuine accessor value types. This
// bounds accesses against caller-declared sizes; it does not validate game ABI.
auto GetAccessorWidth(std::string_view class_name, std::string_view canonical_key) noexcept -> std::optional<uint32_t>;
namespace WidthDetail {
template<class T> constexpr auto ValueWidth() noexcept -> std::optional<uint32_t> {
    using Value = std::remove_cvref_t<T>;
    if constexpr (requires { sizeof(Value); }) {
        if constexpr (sizeof(Value) > 0 && sizeof(Value) <= std::numeric_limits<uint32_t>::max())
            return static_cast<uint32_t>(sizeof(Value));
    }
    return std::nullopt;
}
}
}
