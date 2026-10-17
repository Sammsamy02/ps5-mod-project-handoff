#include "NativeLayoutActiveWidths.hpp"
namespace RC::Platform::NativeLayouts {
extern "C" [[gnu::used]] const uint32_t PublicFieldWidthProbe[]{
*WidthDetail::ValueWidth<decltype(std::declval<Unreal::UStruct&>().GetSuperStruct())>(),
*WidthDetail::ValueWidth<decltype(std::declval<Unreal::FField&>().GetFFieldOwnerUnsafe())>(),
*WidthDetail::ValueWidth<decltype(std::declval<Unreal::FField&>().GetOwnerVariant())>(),
*WidthDetail::ValueWidth<decltype(std::declval<Unreal::FField&>().GetNextFFieldUnsafe())>(),
*WidthDetail::ValueWidth<decltype(std::declval<Unreal::FField&>().GetFFieldFNameUnsafe())>(),
*WidthDetail::ValueWidth<decltype(std::declval<Unreal::FProperty&>().GetPropertyFlags())>(),
*WidthDetail::ValueWidth<decltype(std::declval<Unreal::UFunction&>().GetFunctionFlags())>(),
*WidthDetail::ValueWidth<decltype(std::declval<Unreal::FFieldVariant&>().GetRawPointer())>(),
sizeof(Unreal::FFieldVariant),alignof(Unreal::FFieldVariant)
};
}
