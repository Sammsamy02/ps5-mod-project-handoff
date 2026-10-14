#pragma once
#include <cstdint>
#include <string>
#include <vector>
// Prior 63 startup/menu/spawn dependencies plus l24/l25 container promotion: map/set keys are startup-required.
// Only staged declarations are inspected. No game object reads or map insertion.
namespace rc::joinedlayout {
enum class Kind { Member, Vtable, Runtime };
enum class Disposition { Required, Conditional, Deferred };
struct Key { const char* name; Kind kind; Disposition disposition; bool pinned; uint32_t offset; const char* source_classification; };
inline constexpr Key kKeys[]={
 {"FUObjectArray::ObjObjects",Kind::Member,Disposition::Required,true,16,"startup_menu_source_dependency"},
 {"TUObjectArray::Objects",Kind::Member,Disposition::Required,true,0,"startup_menu_source_dependency"},
 {"TUObjectArray::NumElements",Kind::Member,Disposition::Required,true,20,"startup_menu_source_dependency"},
 {"TUObjectArray::NumChunks",Kind::Member,Disposition::Required,true,28,"startup_menu_source_dependency"},
 {"TUObjectArray::MaxChunks",Kind::Member,Disposition::Required,true,24,"startup_menu_source_dependency"},
 {"FUObjectItem::Object",Kind::Member,Disposition::Required,true,0,"startup_menu_source_dependency"},
 {"FUObjectItem::Flags",Kind::Member,Disposition::Required,true,8,"startup_menu_source_dependency"},
 {"UObjectBase::ClassPrivate",Kind::Member,Disposition::Required,true,16,"startup_menu_source_dependency"},
 {"UObjectBase::NamePrivate",Kind::Member,Disposition::Required,true,24,"startup_menu_source_dependency"},
 {"UObjectBase::OuterPrivate",Kind::Member,Disposition::Required,true,32,"startup_menu_source_dependency"},
 {"UObjectBase::ObjectFlags",Kind::Member,Disposition::Required,true,8,"startup_menu_source_dependency"},
 {"UObjectBase::InternalIndex_Private",Kind::Member,Disposition::Required,true,12,"startup_menu_source_dependency"},
 {"UStruct::SuperStruct",Kind::Member,Disposition::Required,true,64,"startup_menu_source_dependency"},
 {"UStruct::Children",Kind::Member,Disposition::Required,true,72,"startup_menu_source_dependency"},
 {"UStruct::ChildProperties",Kind::Member,Disposition::Required,true,80,"startup_menu_source_dependency"},
 {"UField::Next",Kind::Member,Disposition::Required,true,40,"startup_menu_source_dependency"},
 {"FField::ClassPrivate",Kind::Member,Disposition::Required,true,8,"startup_menu_source_dependency"},
 {"FField::NamePrivate",Kind::Member,Disposition::Required,true,32,"startup_menu_source_dependency"},
 {"FField::Next",Kind::Member,Disposition::Required,true,24,"startup_menu_source_dependency"},
 {"FFieldClass::FName",Kind::Member,Disposition::Required,true,0,"startup_menu_source_dependency"},
 {"FFieldClass::CastFlags",Kind::Member,Disposition::Required,true,16,"startup_menu_source_dependency"},
 {"FFieldClass::SuperClass",Kind::Member,Disposition::Required,true,32,"startup_menu_source_dependency"},
 {"UFunction::NumParms",Kind::Member,Disposition::Required,true,180,"startup_menu_source_dependency"},
 {"UFunction::ParmsSize",Kind::Member,Disposition::Required,true,182,"startup_menu_source_dependency"},
 {"UFunction::ReturnValueOffset",Kind::Member,Disposition::Required,true,184,"startup_menu_source_dependency"},
 {"UFunction::FunctionFlags",Kind::Member,Disposition::Required,true,176,"startup_menu_source_dependency"},
 {"UFunction::Func",Kind::Member,Disposition::Required,true,216,"startup_menu_source_dependency"},
 {"FProperty::PropertyFlags",Kind::Member,Disposition::Required,true,56,"startup_menu_source_dependency"},
 {"FProperty::ArrayDim",Kind::Member,Disposition::Required,true,44,"startup_menu_source_dependency"},
 {"FProperty::ElementSize",Kind::Member,Disposition::Required,true,48,"startup_menu_source_dependency"},
 {"FProperty::Offset_Internal",Kind::Member,Disposition::Required,true,68,"startup_menu_source_dependency"},
 {"UObject::ProcessEvent",Kind::Vtable,Disposition::Required,true,616,"startup_menu_source_dependency"},
 {"FStructProperty::Struct",Kind::Member,Disposition::Required,true,112,"startup_menu_source_dependency"},
 {"UStruct::PropertiesSize",Kind::Member,Disposition::Required,true,88,"startup_menu_source_dependency"},
 {"UStruct::MinAlignment",Kind::Member,Disposition::Required,true,92,"startup_menu_source_dependency"},
 {"FProperty::CopyValuesInternal",Kind::Vtable,Disposition::Required,true,248,"startup_menu_source_dependency"},
 {"FProperty::DestroyValueInternal",Kind::Vtable,Disposition::Required,true,320,"startup_menu_source_dependency"},
 {"FBoolProperty::ByteOffset",Kind::Member,Disposition::Required,true,113,"startup_menu_source_dependency"},
 {"FBoolProperty::ByteMask",Kind::Member,Disposition::Required,true,114,"startup_menu_source_dependency"},
 {"FBoolProperty::FieldMask",Kind::Member,Disposition::Required,true,115,"startup_menu_source_dependency"},
 {"FEnumProperty::UnderlyingProp",Kind::Member,Disposition::Required,true,112,"startup_menu_source_dependency"},
 {"FEnumProperty::Enum",Kind::Member,Disposition::Required,true,120,"startup_menu_source_dependency"},
 {"FArrayProperty::Inner",Kind::Member,Disposition::Required,true,120,"startup_core_array_inner"},
 {"FProperty::GetMinAlignment",Kind::Vtable,Disposition::Conditional,true,368,"spawn_explicit_array_conversion"},
 {"FMalloc::QuantizeSize",Kind::Vtable,Disposition::Required,true,104,"startup_menu_source_dependency"},
 {"FMalloc::Realloc",Kind::Vtable,Disposition::Required,true,64,"startup_menu_source_dependency"},
 {"FMalloc::Free",Kind::Vtable,Disposition::Required,true,80,"startup_menu_source_dependency"},
 {"FText::StaticSize_Private",Kind::Runtime,Disposition::Deferred,false,0,"startup_menu_runtime_discovery_not_profile_offset"},
 {"FText::ValueOpsProperty",Kind::Runtime,Disposition::Deferred,false,0,"startup_menu_runtime_discovery_not_profile_offset"},
 {"FMalloc::Malloc",Kind::Vtable,Disposition::Required,true,48,"startup_menu_source_dependency"},
 {"UEngine::LoadMap",Kind::Vtable,Disposition::Required,true,1272,"startup_menu_source_dependency"},
 {"UEngine::Tick",Kind::Vtable,Disposition::Required,true,784,"startup_menu_source_dependency"},
 {"AGameModeBase::InitGameState",Kind::Vtable,Disposition::Required,true,1976,"startup_menu_source_dependency"},
 {"AActor::BeginPlay",Kind::Vtable,Disposition::Conditional,false,0,"disabled_initializer_branch_only"},
 {"AActor::EndPlay",Kind::Vtable,Disposition::Conditional,false,0,"disabled_initializer_branch_only"},
 {"AActor::Tick",Kind::Vtable,Disposition::Conditional,false,0,"disabled_initializer_branch_only"},
 {"UGameViewportClient::Tick",Kind::Vtable,Disposition::Conditional,false,0,"disabled_initializer_branch_only"},
 {"UObject::ProcessConsoleExec",Kind::Vtable,Disposition::Conditional,false,0,"disabled_initializer_branch_only"},
 {"UStruct::Link",Kind::Vtable,Disposition::Conditional,false,0,"disabled_initializer_branch_only"},
 {"FProperty::RepNotifyFunc",Kind::Member,Disposition::Conditional,false,0,"conditional_optional_property_dependency"},
 {"FProperty::HasIntrusiveUnsetOptionalState",Kind::Vtable,Disposition::Conditional,false,0,"conditional_optional_property_dependency"},
 {"FProperty::IsIntrusiveOptionalValueSet",Kind::Vtable,Disposition::Conditional,false,0,"conditional_optional_property_dependency"},
 {"FProperty::InitializeValueInternal",Kind::Vtable,Disposition::Conditional,false,0,"conditional_optional_property_dependency"},
 {"FMapProperty::KeyProp",Kind::Member,Disposition::Required,true,112,"startup_core_map_set_members"},
 {"FMapProperty::ValueProp",Kind::Member,Disposition::Required,true,120,"startup_core_map_set_members"},
 {"FSetProperty::ElementProp",Kind::Member,Disposition::Required,true,112,"startup_core_map_set_members"}
};
static_assert(sizeof(kKeys)/sizeof(kKeys[0])==66);
inline const char* disposition(Disposition d) { return d==Disposition::Required?"startup-required":d==Disposition::Conditional?"conditional":"conditional/runtime-discovered"; }
inline const char* kind(Kind k) { return k==Kind::Member?"member":k==Kind::Vtable?"vtable":"runtime-discovery"; }
template<class Collection> inline const typename Collection::value_type* find(const Collection* values,const char* name) {
 if(values)for(const auto& value:*values)if(value.name==name)return &value;
 return nullptr;
}
template<class Collection> inline bool conditional_absent(Kind kind,const char* name,const Collection* values) {
 for(const auto& key:kKeys)if(key.kind==kind&&key.disposition==Disposition::Conditional&&std::string(key.name)==name)return !find(values,name);
 return false;
}
struct Check { const Key* key; bool present=false,accepted=true; const char* status; std::string detail; };
struct Result { bool ok=true; unsigned required=0,conditional=0,deferred=0,missing_required=0; std::vector<Check> checks; };
template<class Collection> inline Result evaluate(const Collection* members,const Collection* vtables) {
 Result result;
 for(const auto& key:kKeys) {
  if(key.disposition==Disposition::Deferred) {
   ++result.deferred;
   result.checks.push_back({&key,false,true,"DEFERRED","discovered by TypeChecker after reflection startup; not a layout-map key; no value invented"});
   continue;
  }
  const bool required=key.disposition==Disposition::Required;
  if(required)++result.required;else ++result.conditional;
  const auto* entry=find(key.kind==Kind::Member?members:vtables,key.name);
  // [S1][B6] The staged layout IS the per-game profile's layout (one source of
  // truth). The preflight no longer compares each offset against a baked UE 5.6
  // number (key.offset) — that false-refused a correct UE4 layout (READINESS
  // D4). It keeps the GENERIC checks: a required key must be PRESENT, and a
  // pinned key must carry a resolvable (nonzero, or an intentionally-zero base
  // slot) staged offset. Offset CORRECTNESS is enforced against the profile by
  // the live layout binder + field_gate/layout_binder_gate, not here. key.offset
  // is retained only as human reference and is never the acceptance criterion.
  const bool matches=entry!=nullptr; // presence is the generic criterion
  const char* status=!entry?"MISSING":key.pinned?"PRESENT":"PRESENT_UNPINNED";
  const bool accepted=required?matches:(!entry||matches);
  if(!accepted){result.ok=false;if(required)++result.missing_required;}
  result.checks.push_back({&key,entry!=nullptr,accepted,status,required?"required before seeded startup/menu calls": "conditional path; existing lazy checks remain authoritative when used"});
 }
 return result;
}
}
