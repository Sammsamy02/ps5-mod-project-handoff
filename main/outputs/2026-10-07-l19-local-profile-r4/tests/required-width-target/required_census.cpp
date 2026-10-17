#include "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/src/NativeLayoutWidths.cpp"
#include "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l19-local-profile-r4/units/NativeLayoutActiveWidths.hpp"
namespace RC::Platform::NativeLayouts {
static_assert(Find("FUObjectArray","ObjFirstGCIndex").value_or(0)>=1,"FUObjectArray::ObjFirstGCIndex requires available width covering measured field");
static_assert(Find("FUObjectArray","ObjLastNonGCIndex").value_or(0)>=1,"FUObjectArray::ObjLastNonGCIndex requires available width covering measured field");
static_assert(Find("FUObjectArray","MaxObjectsNotConsideredByGC").value_or(0)>=1,"FUObjectArray::MaxObjectsNotConsideredByGC requires available width covering measured field");
static_assert(Find("FUObjectArray","OpenForDisregardForGC").value_or(0)>=1,"FUObjectArray::OpenForDisregardForGC requires available width covering measured field");
static_assert(Find("FUObjectArray","ObjObjects").value_or(0)>=1,"FUObjectArray::ObjObjects requires available width covering measured field");
static_assert(Find("FUObjectArray","UObjectCreateListeners").value_or(0)>=1,"FUObjectArray::UObjectCreateListeners requires available width covering measured field");
static_assert(Find("FUObjectArray","UObjectDeleteListeners").value_or(0)>=1,"FUObjectArray::UObjectDeleteListeners requires available width covering measured field");
static_assert(Find("FUObjectArray","PrimarySerialNumber").value_or(0)>=1,"FUObjectArray::PrimarySerialNumber requires available width covering measured field");
static_assert(Find("FUObjectArray","bShouldRecycleObjectIndices").value_or(0)>=1,"FUObjectArray::bShouldRecycleObjectIndices requires available width covering measured field");
static_assert(Find("TUObjectArray","Objects").value_or(0)>=1,"TUObjectArray::Objects requires available width covering measured field");
static_assert(Find("TUObjectArray","PreAllocatedObjects").value_or(0)>=1,"TUObjectArray::PreAllocatedObjects requires available width covering measured field");
static_assert(Find("TUObjectArray","MaxElements").value_or(0)>=1,"TUObjectArray::MaxElements requires available width covering measured field");
static_assert(Find("TUObjectArray","NumElements").value_or(0)>=1,"TUObjectArray::NumElements requires available width covering measured field");
static_assert(Find("TUObjectArray","MaxChunks").value_or(0)>=1,"TUObjectArray::MaxChunks requires available width covering measured field");
static_assert(Find("TUObjectArray","NumChunks").value_or(0)>=1,"TUObjectArray::NumChunks requires available width covering measured field");
static_assert(Find("FUObjectItem","Object").value_or(0)>=8,"FUObjectItem::Object requires available width covering measured field");
static_assert(Find("FUObjectItem","Flags").value_or(0)>=4,"FUObjectItem::Flags requires available width covering measured field");
static_assert(Find("UObjectBase","NamePrivate").value_or(0)>=8,"UObjectBase::NamePrivate requires available width covering measured field");
static_assert(Find("UObjectBase","OuterPrivate").value_or(0)>=8,"UObjectBase::OuterPrivate requires available width covering measured field");
static_assert(Find("UObjectBase","ClassPrivate").value_or(0)>=8,"UObjectBase::ClassPrivate requires available width covering measured field");
static_assert(Find("UObjectBase","InternalIndex_Private").value_or(0)>=4,"UObjectBase::InternalIndex_Private requires available width covering measured field");
static_assert(Find("UObjectBase","ObjectFlags").value_or(0)>=4,"UObjectBase::ObjectFlags requires available width covering measured field");
static_assert(Find("UField","Next").value_or(0)>=8,"UField::Next requires available width covering measured field");
static_assert(ActiveWidthDetail::SuperStructWidth<Unreal::UStruct>().value_or(0)>=8,"UStruct::SuperStruct requires available width covering measured field");
static_assert(Find("UStruct","Children").value_or(0)>=8,"UStruct::Children requires available width covering measured field");
static_assert(Find("UStruct","ChildProperties").value_or(0)>=8,"UStruct::ChildProperties requires available width covering measured field");
static_assert(Find("UStruct","PropertiesSize").value_or(0)>=4,"UStruct::PropertiesSize requires available width covering measured field");
static_assert(ActiveWidthDetail::MinAlignmentWidth<Unreal::UStruct>().value_or(0)>=2,"UStruct::MinAlignment requires available width covering measured field");
static_assert(Find("UClass","ClassDefaultObject").value_or(0)>=8,"UClass::ClassDefaultObject requires available width covering measured field");
static_assert(Find("UClass","Interfaces").value_or(0)>=16,"UClass::Interfaces requires available width covering measured field");
static_assert(ActiveWidthDetail::ClassCastFlagsWidth<Unreal::UClass>().value_or(0)>=8,"UClass::ClassCastFlags requires available width covering measured field");
static_assert(Find("FField","ClassPrivate").value_or(0)>=8,"FField::ClassPrivate requires available width covering measured field");
static_assert(ActiveWidthDetail::FieldOwnerWidth<Unreal::FField>().value_or(0)>=8,"FField::Owner requires available width covering measured field");
static_assert(ActiveWidthDetail::FieldNextWidth<Unreal::FField>().value_or(0)>=8,"FField::Next requires available width covering measured field");
static_assert(ActiveWidthDetail::FieldNameWidth<Unreal::FField>().value_or(0)>=8,"FField::NamePrivate requires available width covering measured field");
static_assert(Find("FFieldClass","FName").value_or(0)>=8,"FFieldClass::FName requires available width covering measured field");
static_assert(Find("FFieldClass","SuperClass").value_or(0)>=8,"FFieldClass::SuperClass requires available width covering measured field");
static_assert(ActiveWidthDetail::FieldCastFlagsWidth<Unreal::FFieldClass>().value_or(0)>=8,"FFieldClass::CastFlags requires available width covering measured field");
static_assert(Find("FProperty","ArrayDim").value_or(0)>=4,"FProperty::ArrayDim requires available width covering measured field");
static_assert(Find("FProperty","ElementSize").value_or(0)>=4,"FProperty::ElementSize requires available width covering measured field");
static_assert(ActiveWidthDetail::PropertyFlagsWidth<Unreal::FProperty>().value_or(0)>=8,"FProperty::PropertyFlags requires available width covering measured field");
static_assert(Find("FProperty","Offset_Internal").value_or(0)>=4,"FProperty::Offset_Internal requires available width covering measured field");
static_assert(Find("UFunction","NumParms").value_or(0)>=1,"UFunction::NumParms requires available width covering measured field");
static_assert(Find("UFunction","ParmsSize").value_or(0)>=2,"UFunction::ParmsSize requires available width covering measured field");
static_assert(Find("UFunction","ReturnValueOffset").value_or(0)>=2,"UFunction::ReturnValueOffset requires available width covering measured field");
static_assert(ActiveWidthDetail::FunctionFlagsWidth<Unreal::UFunction>().value_or(0)>=4,"UFunction::FunctionFlags requires available width covering measured field");
static_assert(ActiveWidthDetail::FunctionWidth<Unreal::UFunction>().value_or(0)>=8,"UFunction::Func requires available width covering measured field");
static_assert(Find("FStructProperty","Struct").value_or(0)>=8,"FStructProperty::Struct requires available width covering measured field");
static_assert(Find("FArrayProperty","Inner").value_or(0)>=8,"FArrayProperty::Inner requires available width covering measured field");
static_assert(Find("FBoolProperty","ByteOffset").value_or(0)>=1,"FBoolProperty::ByteOffset requires available width covering measured field");
static_assert(Find("FBoolProperty","ByteMask").value_or(0)>=1,"FBoolProperty::ByteMask requires available width covering measured field");
static_assert(Find("FBoolProperty","FieldMask").value_or(0)>=1,"FBoolProperty::FieldMask requires available width covering measured field");
static_assert(Find("FEnumProperty","UnderlyingProp").value_or(0)>=8,"FEnumProperty::UnderlyingProp requires available width covering measured field");
static_assert(Find("FEnumProperty","Enum").value_or(0)>=8,"FEnumProperty::Enum requires available width covering measured field");
extern "C" [[gnu::used]] const uint32_t AllRequiredWidthFacts[]{
Find("FUObjectArray","ObjFirstGCIndex").value_or(0),
Find("FUObjectArray","ObjLastNonGCIndex").value_or(0),
Find("FUObjectArray","MaxObjectsNotConsideredByGC").value_or(0),
Find("FUObjectArray","OpenForDisregardForGC").value_or(0),
Find("FUObjectArray","ObjObjects").value_or(0),
Find("FUObjectArray","UObjectCreateListeners").value_or(0),
Find("FUObjectArray","UObjectDeleteListeners").value_or(0),
Find("FUObjectArray","PrimarySerialNumber").value_or(0),
Find("FUObjectArray","bShouldRecycleObjectIndices").value_or(0),
Find("TUObjectArray","Objects").value_or(0),
Find("TUObjectArray","PreAllocatedObjects").value_or(0),
Find("TUObjectArray","MaxElements").value_or(0),
Find("TUObjectArray","NumElements").value_or(0),
Find("TUObjectArray","MaxChunks").value_or(0),
Find("TUObjectArray","NumChunks").value_or(0),
Find("FUObjectItem","Object").value_or(0),
Find("FUObjectItem","Flags").value_or(0),
Find("UObjectBase","NamePrivate").value_or(0),
Find("UObjectBase","OuterPrivate").value_or(0),
Find("UObjectBase","ClassPrivate").value_or(0),
Find("UObjectBase","InternalIndex_Private").value_or(0),
Find("UObjectBase","ObjectFlags").value_or(0),
Find("UField","Next").value_or(0),
ActiveWidthDetail::SuperStructWidth<Unreal::UStruct>().value_or(0),
Find("UStruct","Children").value_or(0),
Find("UStruct","ChildProperties").value_or(0),
Find("UStruct","PropertiesSize").value_or(0),
ActiveWidthDetail::MinAlignmentWidth<Unreal::UStruct>().value_or(0),
Find("UClass","ClassDefaultObject").value_or(0),
Find("UClass","Interfaces").value_or(0),
ActiveWidthDetail::ClassCastFlagsWidth<Unreal::UClass>().value_or(0),
Find("FField","ClassPrivate").value_or(0),
ActiveWidthDetail::FieldOwnerWidth<Unreal::FField>().value_or(0),
ActiveWidthDetail::FieldNextWidth<Unreal::FField>().value_or(0),
ActiveWidthDetail::FieldNameWidth<Unreal::FField>().value_or(0),
Find("FFieldClass","FName").value_or(0),
Find("FFieldClass","SuperClass").value_or(0),
ActiveWidthDetail::FieldCastFlagsWidth<Unreal::FFieldClass>().value_or(0),
Find("FProperty","ArrayDim").value_or(0),
Find("FProperty","ElementSize").value_or(0),
ActiveWidthDetail::PropertyFlagsWidth<Unreal::FProperty>().value_or(0),
Find("FProperty","Offset_Internal").value_or(0),
Find("UFunction","NumParms").value_or(0),
Find("UFunction","ParmsSize").value_or(0),
Find("UFunction","ReturnValueOffset").value_or(0),
ActiveWidthDetail::FunctionFlagsWidth<Unreal::UFunction>().value_or(0),
ActiveWidthDetail::FunctionWidth<Unreal::UFunction>().value_or(0),
Find("FStructProperty","Struct").value_or(0),
Find("FArrayProperty","Inner").value_or(0),
Find("FBoolProperty","ByteOffset").value_or(0),
Find("FBoolProperty","ByteMask").value_or(0),
Find("FBoolProperty","FieldMask").value_or(0),
Find("FEnumProperty","UnderlyingProp").value_or(0),
Find("FEnumProperty","Enum").value_or(0)
};}
