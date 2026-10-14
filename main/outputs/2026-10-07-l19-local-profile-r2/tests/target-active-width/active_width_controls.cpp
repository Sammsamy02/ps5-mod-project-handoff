#include "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/src/NativeLayoutWidths.cpp"
#include "NativeLayoutActiveWidths.hpp"

namespace RC::Platform::NativeLayouts {
static_assert(!Find("UFunction","Func"));
static_assert(Find("UStruct","MinAlignment")==4);
static_assert(ActiveWidthDetail::FunctionWidth<Unreal::UFunction>()==8);
static_assert(ActiveWidthDetail::MinAlignmentWidth<Unreal::UStruct>()==2);
static_assert(!Find("UClass","ClassCastFlags"));
static_assert(!Find("FFieldClass","CastFlags"));
static_assert(ActiveWidthDetail::ClassCastFlagsWidth<Unreal::UClass>()==8);
static_assert(ActiveWidthDetail::FieldCastFlagsWidth<Unreal::FFieldClass>()==8);
struct UnknownGetter;
static_assert(!ActiveWidthDetail::FunctionWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::MinAlignmentWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::ClassCastFlagsWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::FieldCastFlagsWidth<UnknownGetter>());
extern "C" [[gnu::used]] const uint32_t ActiveWidthControlFacts[]{
Find("UFunction","Func").value_or(0),*Find("UStruct","MinAlignment"),
*ActiveWidthDetail::FunctionWidth<Unreal::UFunction>(),
*ActiveWidthDetail::MinAlignmentWidth<Unreal::UStruct>(),
*ActiveWidthDetail::ClassCastFlagsWidth<Unreal::UClass>(),
*ActiveWidthDetail::FieldCastFlagsWidth<Unreal::FFieldClass>()};
}
