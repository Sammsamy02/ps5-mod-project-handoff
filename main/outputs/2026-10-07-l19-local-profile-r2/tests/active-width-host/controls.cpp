#include "NativeLayoutBinding.hpp"
#include <NativeLayoutEngineIncludes.hpp>
#include <NativeLayoutWidths.hpp>
#include <iostream>
#include "NativeLayoutActiveWidths.hpp"
namespace RC::Platform::NativeLayouts {
auto GetAccessorWidth(std::string_view cls,std::string_view key) noexcept -> std::optional<uint32_t> {
 if(cls=="UStruct"&&key=="MinAlignment")return 4;
 if(cls=="FallbackControl"&&key=="Known")return 4;
 return std::nullopt;
}
}
using namespace RC::Platform::NativeLayouts;
int main(int argc,char**argv){if(argc!=2)return 2;std::string mode=argv[1];
const std::string id="[NativeLayout]\nFormat=1\nTarget=prospero-x86_64-sysv\nPointerSize=8\nEngineMajor=5\nEngineMinor=6\nNonShipping=0\nStats=0\nEngineRevision=owned-host-control\nBuildId=owned-host-control\n";
std::string m=id;
std::string v=id;
bool expected=true;
if(mode=="func_good")m+="[UFunction]\nUEP_TotalSize=0xE0\nFunc=0xD8\n";
else if(mode=="func_boundary"){m+="[UFunction]\nUEP_TotalSize=0xE0\nFunc=0xD9\n";expected=false;}
else if(mode=="alignment_good")m+="[UStruct]\nUEP_TotalSize=0xB0\nMinAlignment=0xAE\n";
else if(mode=="alignment_boundary"){m+="[UStruct]\nUEP_TotalSize=0xB0\nMinAlignment=0xAF\n";expected=false;}
else if(mode=="class_cast_good")m+="[UClass]\nUEP_TotalSize=0x200\nClassCastFlags=0xD8\n";
else if(mode=="class_cast_boundary"){m+="[UClass]\nUEP_TotalSize=0x200\nClassCastFlags=0x1F9\n";expected=false;}
// FFieldClass total0x40 here is an owned geometry fixture, not a native sizeof claim.
else if(mode=="field_cast_good")m+="[FFieldClass]\nUEP_TotalSize=0x40\nCastFlags=0x10\n";
else if(mode=="field_cast_boundary"){m+="[FFieldClass]\nUEP_TotalSize=0x40\nCastFlags=0x39\n";expected=false;}
else if(mode=="unknown_key"){m+="[UStruct]\nUEP_TotalSize=0xB0\nNoAccessor=0x60\n";expected=false;}
else if(mode=="unknown_width"){m+="[UStruct]\nUEP_TotalSize=0xB0\nScript=0x60\n";expected=false;}
else if(mode=="virtual_metadata")v+="[FProperty]\nGetMinAlignment=0x170\n";
else if(mode=="adapter_policy"){struct UnknownGetter{};
static_assert(!ActiveWidthDetail::FunctionWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::MinAlignmentWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::ClassCastFlagsWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::FieldCastFlagsWidth<UnknownGetter>());
if(GetBindingAccessorWidth("UFunction","Func")!=8||GetBindingAccessorWidth("UStruct","MinAlignment")!=2)return 1;
if(GetBindingAccessorWidth("UClass","ClassCastFlags")!=8||GetBindingAccessorWidth("FFieldClass","CastFlags")!=8)return 1;
if(GetBindingAccessorWidth("FallbackControl","Known")!=GetAccessorWidth("FallbackControl","Known"))return 1;
if(GetBindingAccessorWidth("Unknown","None"))return 1;
std::cout<<"PASS active signatures and unchanged fallback/unknown policy\n"; return 0;}
else return 2;
bool staged=true;std::string detail;
try{StageNativeLayoutTexts(m,v);}catch(const std::exception&e){staged=false;detail=e.what();}
if(staged!=expected){std::cerr<<"FAIL "<<mode<<" expected_stage="<<expected<<" observed_stage="<<staged<<" detail="<<detail<<"\n";return 1;}
if(!expected){std::string want;
 if(mode=="func_boundary")want="Native member is outside declared class size: UFunction";
 if(mode=="alignment_boundary")want="Native member is outside declared class size: UStruct";
 if(mode=="class_cast_boundary")want="Native member is outside declared class size: UClass";
 if(mode=="field_cast_boundary")want="Native member is outside declared class size: FFieldClass";
 if(mode=="unknown_key")want="Unknown native member: UStruct::NoAccessor";
 if(mode=="unknown_width")want="Native accessor width unavailable: UStruct::Script";
 if(detail!=want){std::cerr<<"FAIL wrong refusal "<<detail<<" expected "<<want<<"\n";return 1;}
}
auto facts=GetStagedNativeLayoutFacts();if(bool(facts)!=staged)return 1;
if(staged&&mode=="virtual_metadata"){if(facts->vtables.size()!=1||facts->vtables[0].name!="FProperty::GetMinAlignment"||facts->vtables[0].offset!=0x170)return 1;if(facts->vtables[0].width!=sizeof(void*)){std::cerr<<"FAIL virtual_metadata expected_width="<<sizeof(void*)<<" observed_width="<<facts->vtables[0].width<<"\n";return 1;}}
else if(staged){bool found=false;for(auto&f:facts->members)if(f.name=="UFunction::Func"||f.name=="UStruct::MinAlignment"||f.name=="UClass::ClassCastFlags"||f.name=="FFieldClass::CastFlags"){unsigned want=mode=="alignment_good"?2:8;if(f.width!=want)return 1;found=true;}if(!found)return 1;}
if(!RC::Unreal::UFunction::MemberOffsets.empty()||!RC::Unreal::UStruct::MemberOffsets.empty())return 1;
if(!RC::Unreal::UClass::MemberOffsets.empty()||!RC::Unreal::FFieldClass::MemberOffsets.empty())return 1;
if(!RC::Unreal::UFunction::BitfieldInfos.empty()||!RC::Unreal::UStruct::BitfieldInfos.empty()||!RC::Unreal::UClass::BitfieldInfos.empty()||!RC::Unreal::FFieldClass::BitfieldInfos.empty())return 1;
if(!RC::Unreal::FProperty::VTableLayoutMap.empty())return 1;
std::cout<<"PASS "<<mode<<" stage="<<staged<<" engine_maps_empty=1 "<<detail<<"\n";
}
