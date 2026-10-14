#include "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/src/NativeLayoutWidths.cpp"
#include "NativeLayoutActiveWidths.hpp"

namespace RC::Platform::NativeLayouts {
template<class T> constexpr auto PublicSuperWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetSuperStruct(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSuperStruct())>();
    return std::nullopt;
}
template<class T> constexpr auto PublicOwnerWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetOwnerVariant(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOwnerVariant())>();
    return std::nullopt;
}
template<class T> constexpr auto PublicNextWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetNextFieldAsProperty(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNextFieldAsProperty())>();
    return std::nullopt;
}
template<class T> constexpr auto PublicNameWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetFName(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFName())>();
    return std::nullopt;
}
template<class T> constexpr auto PublicPropertyFlagsWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetPropertyFlags(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPropertyFlags())>();
    return std::nullopt;
}
template<class T> constexpr auto PublicFunctionFlagsWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetFunctionFlags(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFunctionFlags())>();
    return std::nullopt;
}
static_assert(!Find("UStruct", "SuperStruct"));
static_assert(!Find("FField", "Owner"));
static_assert(!Find("FField", "Next"));
static_assert(!Find("FField", "NamePrivate"));
static_assert(!Find("FProperty", "PropertyFlags"));
static_assert(!Find("UFunction", "FunctionFlags"));
static_assert(PublicSuperWidth<Unreal::UStruct>() == 8);
static_assert(PublicOwnerWidth<Unreal::FField>() == 16);
static_assert(PublicNextWidth<Unreal::FField>() == 8);
static_assert(PublicNameWidth<Unreal::FField>() == 8);
static_assert(PublicPropertyFlagsWidth<Unreal::FProperty>() == 8);
static_assert(PublicFunctionFlagsWidth<Unreal::UFunction>() == 4);
struct UnknownGetter;
static_assert(!PublicSuperWidth<UnknownGetter>());
static_assert(!PublicOwnerWidth<UnknownGetter>());
static_assert(!PublicNextWidth<UnknownGetter>());
static_assert(!PublicNameWidth<UnknownGetter>());
static_assert(!PublicPropertyFlagsWidth<UnknownGetter>());
static_assert(!PublicFunctionFlagsWidth<UnknownGetter>());
extern "C" [[gnu::used]] const uint32_t SixPublicGetterFacts[]{
    *PublicSuperWidth<Unreal::UStruct>(), *PublicOwnerWidth<Unreal::FField>(),
    *PublicNextWidth<Unreal::FField>(), *PublicNameWidth<Unreal::FField>(),
    *PublicPropertyFlagsWidth<Unreal::FProperty>(), *PublicFunctionFlagsWidth<Unreal::UFunction>()};
}
