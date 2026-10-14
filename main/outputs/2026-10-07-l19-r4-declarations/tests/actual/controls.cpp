#include "NativeLayoutBinding.hpp"
#include <NativeLayoutEngineIncludes.hpp>
#include <NativeLayoutWidths.hpp>
#include "BuildFactsChecks.hpp"
#include "RuntimeBuildProfile.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
namespace RC::Platform::NativeLayouts{auto GetAccessorWidth(std::string_view cls,std::string_view key)noexcept->std::optional<uint32_t>{
if(cls=="FArrayProperty"&&key=="Inner")return 8;
if(cls=="FBoolProperty"&&key=="ByteMask")return 1;
if(cls=="FBoolProperty"&&key=="ByteOffset")return 1;
if(cls=="FBoolProperty"&&key=="FieldMask")return 1;
if(cls=="FEnumProperty"&&key=="Enum")return 8;
if(cls=="FEnumProperty"&&key=="UnderlyingProp")return 8;
if(cls=="FField"&&key=="ClassPrivate")return 8;
if(cls=="FField"&&key=="NamePrivate")return 8;
if(cls=="FField"&&key=="Next")return 8;
if(cls=="FField"&&key=="Owner")return 16;
if(cls=="FFieldClass"&&key=="CastFlags")return 8;
if(cls=="FFieldClass"&&key=="FName")return 8;
if(cls=="FFieldClass"&&key=="SuperClass")return 8;
if(cls=="FProperty"&&key=="ArrayDim")return 4;
if(cls=="FProperty"&&key=="ElementSize")return 4;
if(cls=="FProperty"&&key=="Offset_Internal")return 4;
if(cls=="FProperty"&&key=="PropertyFlags")return 8;
if(cls=="FStructProperty"&&key=="Struct")return 8;
if(cls=="FUObjectArray"&&key=="MaxObjectsNotConsideredByGC")return 4;
if(cls=="FUObjectArray"&&key=="ObjFirstGCIndex")return 4;
if(cls=="FUObjectArray"&&key=="ObjLastNonGCIndex")return 4;
if(cls=="FUObjectArray"&&key=="ObjObjects")return 1;
if(cls=="FUObjectArray"&&key=="OpenForDisregardForGC")return 4;
if(cls=="FUObjectArray"&&key=="PrimarySerialNumber")return 4;
if(cls=="FUObjectArray"&&key=="UObjectCreateListeners")return 16;
if(cls=="FUObjectArray"&&key=="UObjectDeleteListeners")return 16;
if(cls=="FUObjectArray"&&key=="bShouldRecycleObjectIndices")return 1;
if(cls=="FUObjectItem"&&key=="Flags")return 4;
if(cls=="FUObjectItem"&&key=="Object")return 8;
if(cls=="TUObjectArray"&&key=="MaxChunks")return 4;
if(cls=="TUObjectArray"&&key=="MaxElements")return 4;
if(cls=="TUObjectArray"&&key=="NumChunks")return 4;
if(cls=="TUObjectArray"&&key=="NumElements")return 4;
if(cls=="TUObjectArray"&&key=="Objects")return 8;
if(cls=="TUObjectArray"&&key=="PreAllocatedObjects")return 8;
if(cls=="UClass"&&key=="ClassCastFlags")return 8;
if(cls=="UClass"&&key=="ClassDefaultObject")return 8;
if(cls=="UClass"&&key=="Interfaces")return 16;
if(cls=="UField"&&key=="Next")return 8;
if(cls=="UFunction"&&key=="Func")return 8;
if(cls=="UFunction"&&key=="FunctionFlags")return 4;
if(cls=="UFunction"&&key=="NumParms")return 1;
if(cls=="UFunction"&&key=="ParmsSize")return 2;
if(cls=="UFunction"&&key=="ReturnValueOffset")return 2;
if(cls=="UObjectBase"&&key=="ClassPrivate")return 8;
if(cls=="UObjectBase"&&key=="InternalIndex_Private")return 4;
if(cls=="UObjectBase"&&key=="NamePrivate")return 8;
if(cls=="UObjectBase"&&key=="ObjectFlags")return 4;
if(cls=="UObjectBase"&&key=="OuterPrivate")return 8;
if(cls=="UStruct"&&key=="ChildProperties")return 8;
if(cls=="UStruct"&&key=="Children")return 8;
if(cls=="UStruct"&&key=="MinAlignment")return 2;
if(cls=="UStruct"&&key=="PropertiesSize")return 4;
if(cls=="UStruct"&&key=="SuperStruct")return 8;
return std::nullopt;}}
using namespace RC::Platform::NativeLayouts;
std::string read(const std::string&p){std::ifstream f(p);if(!f)throw std::runtime_error("Missing host file");return std::string(std::istreambuf_iterator<char>(f),{});}
void require(bool b,const std::string&s){if(!b)throw std::runtime_error(s);}
int main(int argc,char**argv){if(argc!=5)return 2;std::string mode=argv[1],m=read(argv[2]),v=read(argv[3]);unsigned expectedCount=std::stoul(argv[4]);std::string want;bool expectStage=true;
if(mode=="full_known"){auto b=ParsePair(m,v);require(b.members.size()==expectedCount,"complete known member count");require(b.vtables.size()==std::size(rc::runtimeprofile::kRequiredVtables)-2,"known virtual count");want="Native member class requires positive UEP_TotalSize: FField";expectStage=false;}
else if(mode=="geometry"){auto p=m.find("Func=0xd8");require(p!=m.npos,"Func declaration not found");m.replace(p,9,"Func=0xd9");want="Native member is outside declared class size: UFunction";expectStage=false;}
else if(mode=="unknown"){auto p=m.find("[UFunction]");m.insert(p+12,"NoSuchAccessor=0x20\n");want="Unknown native member: UFunction::NoSuchAccessor";expectStage=false;}
else if(mode=="aliases"){auto p=m.find("[UObjectBase]");m.insert(p+14,"InternalIndex=0xd\n");want="Duplicate native member or alias: UObjectBase::InternalIndex_Private";expectStage=false;}
else if(mode=="old_member"){RC::Unreal::UClass::MemberOffsets.emplace("OldCustom",1);want="Native layouts cannot replace existing/cached member maps: UClass";expectStage=false;}
else if(mode=="old_virtual"){RC::Unreal::FProperty::VTableLayoutMap.emplace("OldCustom",8);want="Native layouts cannot replace existing/cached vtable maps";expectStage=false;}
else if(mode!="stageable"&&mode!="restage")return 2;
bool staged=true;std::string detail;try{StageNativeLayoutTexts(m,v);}catch(const std::exception&e){staged=false;detail=e.what();}
require(staged==expectStage,"Unexpected staging status: "+detail);if(!staged)require(detail==want,"Unexpected refusal: "+detail+" expected "+want);
auto facts=GetStagedNativeLayoutFacts();require(bool(facts)==staged,"staged metadata mismatch");
if(staged){require(facts->members.size()==expectedCount,"stageable member count");require(facts->vtables.size()==5,"virtual count");
for(auto&p:rc::runtimeprofile::kRequiredMembers)if(p.known&&!std::string(p.name).starts_with("FField::")&&!std::string(p.name).starts_with("FFieldClass::")){bool found=false;for(auto&f:facts->members)if(f.name==p.name){require(f.offset==p.offset,"offset mismatch");require(!p.width||f.width>=p.width,"footprint minimum");found=true;}require(found,"known fact absent");}
for(auto&p:rc::runtimeprofile::kRequiredVtables)if(p.known){bool found=false;for(auto&f:facts->vtables)if(f.name==p.name){require(f.offset==p.offset&&f.width==sizeof(void*),"virtual mismatch");found=true;}require(found,"known virtual absent");}
rc::buildfacts::Result result{};result.ok=true;for(auto&p:rc::runtimeprofile::kRequiredMembers)rc::buildfacts::require_layout(result,"member",p,&facts->members);for(auto&p:rc::runtimeprofile::kRequiredVtables)rc::buildfacts::require_layout(result,"vtable",p,&facts->vtables);require(!result.ok,"incomplete profile must refuse");
for(const char* key:{"layout.member.FField::UEP_TotalSize.measured","layout.member.FFieldClass::UEP_TotalSize.measured","layout.vtable.UEngine::LoadMap.measured","layout.vtable.AGameModeBase::InitGameState.measured"}){bool found=false;for(auto&c:result.checks)if(c.name==key&&!c.ok)found=true;require(found,std::string("missing unknown refusal: ")+key);}
if(mode=="restage"){bool refused=false;try{StageNativeLayoutTexts(m,v);}catch(const std::exception&e){refused=std::string(e.what())=="Native layout bundle is already staged for this loaded lifetime";}require(refused,"old staged bundle conflict");}}
require(RC::Unreal::AActor::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::AActor::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::AActor::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::AGameMode::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::AGameMode::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::AGameMode::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::AGameModeBase::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::AGameModeBase::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::AGameModeBase::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FArchive::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FArchive::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FArchive::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FArchiveState::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FArchiveState::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FArchiveState::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FArrayProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FArrayProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FArrayProperty::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FBoolProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FBoolProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FBoolProperty::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FByteProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FByteProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FByteProperty::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FClassProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FClassProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FClassProperty::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FDelegateProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FDelegateProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FDelegateProperty::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FEnumProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FEnumProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FEnumProperty::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FField::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FField::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FField::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FFieldClass::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FFieldClass::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FFieldClass::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FFieldPathProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FFieldPathProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FFieldPathProperty::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FInterfaceProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FInterfaceProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FInterfaceProperty::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FMalloc::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FMalloc::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FMalloc::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FMapProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FMapProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FMapProperty::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FMulticastDelegateProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FMulticastDelegateProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FMulticastDelegateProperty::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FNumericProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FNumericProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FNumericProperty::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FObjectPropertyBase::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FObjectPropertyBase::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FObjectPropertyBase::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FOutputDevice::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FOutputDevice::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FOutputDevice::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FProperty::VTableLayoutMap.size()==size_t(mode=="old_virtual"),"published virtual map");
require(RC::Unreal::FSetProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FSetProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FSetProperty::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FSoftClassProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FSoftClassProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FSoftClassProperty::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FStructProperty::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FStructProperty::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FStructProperty::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FUObjectArray::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FUObjectArray::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FUObjectArray::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FUObjectHashTables::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FUObjectHashTables::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FUObjectHashTables::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FUObjectItem::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FUObjectItem::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FUObjectItem::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::FWorldContext::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::FWorldContext::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::FWorldContext::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::ITextData::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::ITextData::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::ITextData::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::TUObjectArray::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::TUObjectArray::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::TUObjectArray::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::UClass::MemberOffsets.size()==size_t(mode=="old_member"),"published member map");
require(RC::Unreal::UClass::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::UClass::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::UDataTable::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::UDataTable::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::UDataTable::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::UEngine::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::UEngine::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::UEngine::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::UEnum::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::UEnum::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::UEnum::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::UField::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::UField::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::UField::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::UFunction::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::UFunction::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::UFunction::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::UGameViewportClient::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::UGameViewportClient::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::UGameViewportClient::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::ULocalPlayer::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::ULocalPlayer::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::ULocalPlayer::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::UObject::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::UObject::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::UObject::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::UObjectBase::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::UObjectBase::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::UObjectBase::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::UObjectBaseUtility::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::UObjectBaseUtility::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::UObjectBaseUtility::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::UPlayer::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::UPlayer::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::UPlayer::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::UScriptStruct::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::UScriptStruct::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::UScriptStruct::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::UStruct::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::UStruct::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::UStruct::VTableLayoutMap.size()==0,"published virtual map");
require(RC::Unreal::UWorld::MemberOffsets.size()==0,"published member map");
require(RC::Unreal::UWorld::BitfieldInfos.empty(),"published bitfield map");
require(RC::Unreal::UWorld::VTableLayoutMap.size()==0,"published virtual map");
std::cout<<"PASS "<<mode<<" stage="<<staged<<" published_maps=0 "<<detail<<"\n";}
