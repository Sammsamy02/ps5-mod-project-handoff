#include "../src/NativeLayoutWidths.cpp"
namespace RC::Platform::NativeLayouts {
using SourceFuncBase = void (Unreal::UObject::*)(Unreal::FFrame&, void* const);
using SourceFunc419 = std::function<void(Unreal::UObject*, Unreal::FFrame&, void* const)>*;
constexpr auto BaseWidth = WidthDetail::ValueWidth<SourceFuncBase>();
constexpr auto ModernWidth = WidthDetail::ValueWidth<SourceFunc419>();
static_assert(BaseWidth && ModernWidth);
static_assert(Find("UFunction","Func") == ConservativeWidth({BaseWidth,ModernWidth}));
static_assert(Find("UFunction","Func") >= BaseWidth && Find("UFunction","Func") >= ModernWidth);
static_assert(!WidthDetail::ValueWidth<Unreal::ObjectPtr_Private::TNonAccessTrackedObjectPtr<Unreal::UStruct>>());
static_assert(WidthDetail::ValueWidth<Unreal::TObjectPtr<Unreal::UStruct>>());
static_assert(!Find("UStruct","SuperStruct"));
static_assert(!ConservativeWidth({WidthDetail::ValueWidth<Unreal::TObjectPtr<Unreal::UStruct>>(),
    WidthDetail::ValueWidth<Unreal::ObjectPtr_Private::TNonAccessTrackedObjectPtr<Unreal::UStruct>>()}));
static_assert(!Find("Unknown","NoAccessor"));
extern "C" [[gnu::section(".focused_width_facts"),gnu::used]] const uint32_t NativeFocusedWidthFacts[]{
 BaseWidth.value_or(0),ModernWidth.value_or(0),Find("UFunction","Func").value_or(0),
 WidthDetail::ValueWidth<Unreal::TObjectPtr<Unreal::UStruct>>().value_or(0),
 WidthDetail::ValueWidth<Unreal::ObjectPtr_Private::TNonAccessTrackedObjectPtr<Unreal::UStruct>>().value_or(0),
 Find("UStruct","SuperStruct").value_or(0)
};
}
