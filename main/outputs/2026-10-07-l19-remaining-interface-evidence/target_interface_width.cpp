#include "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/src/NativeLayoutWidths.cpp"

namespace RC::Platform::NativeLayouts {
static_assert(Find("UClass", "Interfaces") == 16);
static_assert(WidthDetail::ValueWidth<decltype(std::declval<Unreal::UClass&>().GetInterfaces())>() == 16);
static_assert(sizeof(Unreal::FImplementedInterface) == 16);
static_assert(offsetof(Unreal::FImplementedInterface, Class) == 0);
static_assert(offsetof(Unreal::FImplementedInterface, PointerOffset) == 8);
static_assert(offsetof(Unreal::FImplementedInterface, bImplementedByK2) == 12);
extern "C" [[gnu::used]] const uint32_t InterfaceWidthControlFacts[]{
    *Find("UClass", "Interfaces"),
    *WidthDetail::ValueWidth<decltype(std::declval<Unreal::UClass&>().GetInterfaces())>(),
    sizeof(Unreal::FImplementedInterface),
    offsetof(Unreal::FImplementedInterface, Class),
    offsetof(Unreal::FImplementedInterface, PointerOffset),
    offsetof(Unreal::FImplementedInterface, bImplementedByK2)};
}
