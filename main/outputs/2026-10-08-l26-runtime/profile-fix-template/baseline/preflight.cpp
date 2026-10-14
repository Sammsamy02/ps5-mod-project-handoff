
#include "JoinedLayoutPreflight.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
struct Fact {std::string name;uint32_t offset;bool operator==(const Fact&)const=default;};
int main(int argc,char**argv){assert(argc==2);std::string mode=argv[1];std::vector<Fact> members,vtables;
members.push_back({"FArrayProperty::Inner",120});
members.push_back({"FArrayProperty::UEP_TotalSize",128});
members.push_back({"FBoolProperty::ByteMask",114});
members.push_back({"FBoolProperty::ByteOffset",113});
members.push_back({"FBoolProperty::FieldMask",115});
members.push_back({"FBoolProperty::UEP_TotalSize",120});
members.push_back({"FEnumProperty::Enum",120});
members.push_back({"FEnumProperty::UEP_TotalSize",128});
members.push_back({"FEnumProperty::UnderlyingProp",112});
members.push_back({"FField::ClassPrivate",8});
members.push_back({"FField::NamePrivate",32});
members.push_back({"FField::Next",24});
members.push_back({"FField::Owner",16});
members.push_back({"FField::UEP_TotalSize",48});
members.push_back({"FFieldClass::CastFlags",16});
members.push_back({"FFieldClass::FName",0});
members.push_back({"FFieldClass::SuperClass",32});
members.push_back({"FFieldClass::UEP_TotalSize",64});
members.push_back({"FProperty::ArrayDim",44});
members.push_back({"FProperty::ElementSize",48});
members.push_back({"FProperty::Offset_Internal",68});
members.push_back({"FProperty::PropertyFlags",56});
members.push_back({"FProperty::UEP_TotalSize",112});
members.push_back({"FStructProperty::Struct",112});
members.push_back({"FStructProperty::UEP_TotalSize",120});
members.push_back({"FUObjectArray::MaxObjectsNotConsideredByGC",8});
members.push_back({"FUObjectArray::ObjFirstGCIndex",0});
members.push_back({"FUObjectArray::ObjLastNonGCIndex",4});
members.push_back({"FUObjectArray::ObjObjects",16});
members.push_back({"FUObjectArray::OpenForDisregardForGC",12});
members.push_back({"FUObjectArray::PrimarySerialNumber",176});
members.push_back({"FUObjectArray::UEP_TotalSize",184});
members.push_back({"FUObjectArray::UObjectCreateListeners",104});
members.push_back({"FUObjectArray::UObjectDeleteListeners",120});
members.push_back({"FUObjectArray::bShouldRecycleObjectIndices",180});
members.push_back({"FUObjectItem::Flags",8});
members.push_back({"FUObjectItem::Object",0});
members.push_back({"FUObjectItem::UEP_TotalSize",24});
members.push_back({"TUObjectArray::MaxChunks",24});
members.push_back({"TUObjectArray::MaxElements",16});
members.push_back({"TUObjectArray::NumChunks",28});
members.push_back({"TUObjectArray::NumElements",20});
members.push_back({"TUObjectArray::Objects",0});
members.push_back({"TUObjectArray::PreAllocatedObjects",8});
members.push_back({"TUObjectArray::UEP_TotalSize",32});
members.push_back({"UClass::ClassCastFlags",216});
members.push_back({"UClass::ClassDefaultObject",272});
members.push_back({"UClass::Interfaces",472});
members.push_back({"UClass::UEP_TotalSize",512});
members.push_back({"UField::Next",40});
members.push_back({"UField::UEP_TotalSize",48});
members.push_back({"UFunction::Func",216});
members.push_back({"UFunction::FunctionFlags",176});
members.push_back({"UFunction::NumParms",180});
members.push_back({"UFunction::ParmsSize",182});
members.push_back({"UFunction::ReturnValueOffset",184});
members.push_back({"UFunction::UEP_TotalSize",224});
members.push_back({"UObjectBase::ClassPrivate",16});
members.push_back({"UObjectBase::InternalIndex_Private",12});
members.push_back({"UObjectBase::NamePrivate",24});
members.push_back({"UObjectBase::ObjectFlags",8});
members.push_back({"UObjectBase::OuterPrivate",32});
members.push_back({"UObjectBase::UEP_TotalSize",40});
members.push_back({"UStruct::ChildProperties",80});
members.push_back({"UStruct::Children",72});
members.push_back({"UStruct::MinAlignment",92});
members.push_back({"UStruct::PropertiesSize",88});
members.push_back({"UStruct::SuperStruct",64});
members.push_back({"UStruct::UEP_TotalSize",176});
members.push_back({"FMapProperty::KeyProp",112});
members.push_back({"FMapProperty::UEP_TotalSize",160});
members.push_back({"FMapProperty::ValueProp",120});
members.push_back({"FSetProperty::ElementProp",112});
members.push_back({"FSetProperty::UEP_TotalSize",144});
vtables.push_back({"AGameModeBase::InitGameState",1976});
vtables.push_back({"FProperty::CopyValuesInternal",248});
vtables.push_back({"FProperty::DestroyValueInternal",320});
vtables.push_back({"FProperty::GetMinAlignment",368});
vtables.push_back({"UEngine::LoadMap",1272});
vtables.push_back({"UEngine::Tick",784});
vtables.push_back({"UObject::ProcessEvent",616});
vtables.push_back({"FMalloc::Malloc",48});
vtables.push_back({"FMalloc::TryMalloc",56});
vtables.push_back({"FMalloc::Realloc",64});
vtables.push_back({"FMalloc::TryRealloc",72});
vtables.push_back({"FMalloc::Free",80});
vtables.push_back({"FMalloc::QuantizeSize",104});
vtables.push_back({"FMalloc::GetAllocationSize",112});
vtables.push_back({"FMalloc::Trim",120});
vtables.push_back({"FMalloc::SetupTLSCachesOnCurrentThread",128});
vtables.push_back({"FMalloc::ClearAndDisableTLSCachesOnCurrentThread",152});
vtables.push_back({"FMalloc::InitializeStatsMetadata",160});
vtables.push_back({"FMalloc::UpdateStats",168});
vtables.push_back({"FMalloc::GetAllocatorStats",176});
vtables.push_back({"FMalloc::DumpAllocatorStats",184});
vtables.push_back({"FMalloc::IsInternallyThreadSafe",192});
vtables.push_back({"FMalloc::ValidateHeap",200});
vtables.push_back({"FMalloc::GetDescriptiveName",208});
 auto remove=[&](const char*n){std::erase_if(members,[&](const auto&x){return x.name==n;});};
 bool expected=true;
 if(mode=="inner_missing"){remove("FArrayProperty::Inner");expected=false;}
 else if(mode=="inner_mismatch"){for(auto&x:members)if(x.name=="FArrayProperty::Inner")++x.offset;expected=false;}
 else if(mode=="map_key_missing"){remove("FMapProperty::KeyProp");expected=false;}
 else if(mode=="map_key_mismatch"){for(auto&x:members)if(x.name=="FMapProperty::KeyProp")++x.offset;expected=false;}
 else if(mode=="set_element_missing"){remove("FSetProperty::ElementProp");expected=false;}
 else if(mode=="map_value_mismatch"){for(auto&x:members)if(x.name=="FMapProperty::ValueProp")x.offset+=40;expected=false;}
 else if(mode=="container_absent"){remove("FMapProperty::KeyProp");remove("FMapProperty::ValueProp");remove("FSetProperty::ElementProp");expected=false;}
 else if(mode=="multi_missing"){remove("FArrayProperty::Inner");remove("FField::ClassPrivate");expected=false;}
 else if(mode=="conditional_mismatch"){for(auto&x:vtables)if(x.name=="FProperty::GetMinAlignment")++x.offset;expected=false;}
 else if(mode!="baseline")return 3;
 const auto before=members;const auto before_v=vtables;
 const auto r=rc::joinedlayout::evaluate(&members,&vtables);
 assert(r.checks.size()==66&&r.required==53&&r.conditional==11&&r.deferred==2);assert(r.ok==expected);
 assert(before==members&&before_v==vtables);
 assert(!rc::joinedlayout::conditional_absent(rc::joinedlayout::Kind::Member,"FArrayProperty::Inner",&members));
 // Promoted container keys are startup-required: absence or conflict refuses.
 for(const char*n:{"FMapProperty::KeyProp","FMapProperty::ValueProp","FSetProperty::ElementProp"})
  assert(!rc::joinedlayout::conditional_absent(rc::joinedlayout::Kind::Member,n,&members));
 if(mode=="multi_missing")assert(r.missing_required==2);
 if(mode=="container_absent")assert(r.missing_required==3);
 if(mode=="set_element_missing")assert(r.missing_required==1);
 if(mode=="conditional_mismatch")assert(r.missing_required==0);
 std::cout<<"PASS "<<mode<<" 66 keys, 53 required, 11 conditional, 2 deferred; no mutation\n";
}
