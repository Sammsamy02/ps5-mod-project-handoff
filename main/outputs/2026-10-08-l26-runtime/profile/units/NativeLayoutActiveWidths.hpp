#pragma once
#include <NativeLayoutEngineIncludes.hpp>
#include <NativeLayoutWidths.hpp>
#include <utility>

namespace RC::Platform::NativeLayouts {
namespace ActiveWidthDetail {
template<class T> constexpr auto FunctionWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetFunc(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFunc())>();
    return std::nullopt;
}
template<class T> constexpr auto MinAlignmentWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetMinAlignment(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMinAlignment())>();
    return std::nullopt;
}
template<class T> constexpr auto ClassCastFlagsWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetClassCastFlags(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassCastFlags())>();
    return std::nullopt;
}
template<class T> constexpr auto FieldCastFlagsWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetCastFlags(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCastFlags())>();
    return std::nullopt;
}
template<class T> constexpr auto SuperStructWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetSuperStruct(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSuperStruct())>();
    return std::nullopt;
}
template<class T> constexpr auto FieldOwnerWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetOwnerVariant(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOwnerVariant())>();
    return std::nullopt;
}
template<class T> constexpr auto FieldNextWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetNextFieldAsProperty(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNextFieldAsProperty())>();
    return std::nullopt;
}
template<class T> constexpr auto FieldNameWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetFName(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFName())>();
    return std::nullopt;
}
template<class T> constexpr auto PropertyFlagsWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetPropertyFlags(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPropertyFlags())>();
    return std::nullopt;
}
template<class T> constexpr auto FunctionFlagsWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetFunctionFlags(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFunctionFlags())>();
    return std::nullopt;
}
// [R11-UENUM] UE 4.15 up to below 5.7 the port reads UEnum names only through GetNames415()
// (port UEnum.cpp Version::IsBelow(5, 7) branches). All five versioned Names getters are private
// in the generated wrapper, so the broad table (and a requires-probe) sees none of them and gives
// no width. GetNames415's return type is named here exactly as the wrapper declares it
// (MemberVariableLayout_HeaderWrapper_UEnum.hpp: TArray<TPair<FName,__int64>,FDefaultAllocator>&).
constexpr auto EnumNamesWidth() noexcept -> std::optional<uint32_t> {
    return WidthDetail::ValueWidth<Unreal::TArray<Unreal::TPair<Unreal::FName, __int64>, Unreal::FDefaultAllocator>&>();
}
}
// These public accessors select the genuine active versioned getter. The broad
// provider can be unavailable or include a larger alternate accessor type.
// This adapter derives active access widths without supplying game layout values.
// A by-value wrapper may cover more bytes than the physical native member:
// GetOwnerVariant includes the port's legacy bool. Its whole value footprint
// bounds that existing copy; it does not establish a native variant ABI.
inline auto GetBindingAccessorWidth(std::string_view class_name, std::string_view key) noexcept -> std::optional<uint32_t> {
    if (class_name == "UFunction" && key == "Func")
        return ActiveWidthDetail::FunctionWidth<Unreal::UFunction>();
    if (class_name == "UStruct" && key == "MinAlignment")
        return ActiveWidthDetail::MinAlignmentWidth<Unreal::UStruct>();
    if (class_name == "UClass" && key == "ClassCastFlags")
        return ActiveWidthDetail::ClassCastFlagsWidth<Unreal::UClass>();
    if (class_name == "FFieldClass" && key == "CastFlags")
        return ActiveWidthDetail::FieldCastFlagsWidth<Unreal::FFieldClass>();
    if (class_name == "UStruct" && key == "SuperStruct")
        return ActiveWidthDetail::SuperStructWidth<Unreal::UStruct>();
    if (class_name == "FField" && key == "Owner")
        return ActiveWidthDetail::FieldOwnerWidth<Unreal::FField>();
    if (class_name == "FField" && key == "Next")
        return ActiveWidthDetail::FieldNextWidth<Unreal::FField>();
    if (class_name == "FField" && key == "NamePrivate")
        return ActiveWidthDetail::FieldNameWidth<Unreal::FField>();
    if (class_name == "FProperty" && key == "PropertyFlags")
        return ActiveWidthDetail::PropertyFlagsWidth<Unreal::FProperty>();
    if (class_name == "UFunction" && key == "FunctionFlags")
        return ActiveWidthDetail::FunctionFlagsWidth<Unreal::UFunction>();
    if (class_name == "UEnum" && key == "Names")
        return ActiveWidthDetail::EnumNamesWidth();
    return GetAccessorWidth(class_name, key);
}
}
