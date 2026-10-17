"""Actual binding/parser staging with explicitly bounded host engine-map storage.
Target controls separately compile the genuine selected public getter signatures.
"""
from pathlib import Path
import subprocess,json,hashlib,sys,os
root=Path(__file__).resolve().parents[1];w=root.parents[1]
red=len(sys.argv)>1 and sys.argv[1]=='red'
virtual_red=len(sys.argv)>1 and sys.argv[1]=='virtual_red'
out=root/'tests/active-width-host';out.mkdir(exist_ok=True)
parser=w/'.superpowers/native-dependency-spike/ue4ss-native-layout-parser-1'
schema=w/'.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include/NativeLayoutSchema.inc'
width=w/'.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1'
rows=[]
names=[('UFunction','Func'),('UFunction','UEP_TotalSize'),('UStruct','MinAlignment'),('UStruct','UEP_TotalSize'),('UStruct','Script'),('UClass','ClassCastFlags'),('UClass','UEP_TotalSize'),('FFieldClass','CastFlags'),('FFieldClass','UEP_TotalSize')]
selected=[]
for cls,key in names:
    matches=[s for s in schema.read_text().splitlines() if s.strip().startswith('{"'+cls+'", "'+key+'",')]
    assert len(matches)==1,(cls,key);selected+=matches
virtual_row=next(s for s in schema.read_text().splitlines() if s.strip()=='{"FProperty", &Unreal::FProperty::VTableLayoutMap},')
(out/'NativeLayoutSchema.inc').write_text('static const std::vector<MemberSchema> MemberSchemaEntries{\n'+'\n'.join(selected)+'\n};\nstatic auto MemberTargets(){return std::array<MemberTarget,4>{{'+','.join('{"'+c+'",&Unreal::'+c+'::MemberOffsets,&Unreal::'+c+'::BitfieldInfos}' for c in ['UFunction','UStruct','UClass','FFieldClass'])+'}};}\nstatic auto VirtualTargets(){return std::array<VirtualTarget,1>{{\n'+virtual_row+'\n}};}\n')
(out/'NativeLayoutEngineIncludes.hpp').write_text(r'''#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>
#include <functional>
#define STR(s) s
namespace RC::File {using StringType=std::string;}
namespace RC::Unreal {
struct BitfieldInfo {uint8_t position,length,storage;};
struct UObject;struct FFrame;
struct UFunction {std::function<void(UObject*,FFrame&,void* const)>*& GetFunc(); inline static std::unordered_map<std::string,int32_t>MemberOffsets;inline static std::unordered_map<std::string,BitfieldInfo>BitfieldInfos;};
struct UStruct {int16_t& GetMinAlignment();inline static std::unordered_map<std::string,int32_t>MemberOffsets;inline static std::unordered_map<std::string,BitfieldInfo>BitfieldInfos;};
struct UClass {uint64_t& GetClassCastFlags();inline static std::unordered_map<std::string,int32_t>MemberOffsets;inline static std::unordered_map<std::string,BitfieldInfo>BitfieldInfos;};
struct FFieldClass {uint64_t GetCastFlags();inline static std::unordered_map<std::string,int32_t>MemberOffsets;inline static std::unordered_map<std::string,BitfieldInfo>BitfieldInfos;};
struct FProperty {inline static std::unordered_map<std::string,uint32_t>VTableLayoutMap;};
}
''')
pre=r'''#include "NativeLayoutBinding.hpp"
#include <NativeLayoutEngineIncludes.hpp>
#include <NativeLayoutWidths.hpp>
#include <iostream>
ACTIVE_INCLUDE
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
else if(mode=="adapter_policy"){ACTIVE_CONTROLS return 0;}
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
'''
control=r'''struct UnknownGetter{};
static_assert(!ActiveWidthDetail::FunctionWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::MinAlignmentWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::ClassCastFlagsWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::FieldCastFlagsWidth<UnknownGetter>());
if(GetBindingAccessorWidth("UFunction","Func")!=8||GetBindingAccessorWidth("UStruct","MinAlignment")!=2)return 1;
if(GetBindingAccessorWidth("UClass","ClassCastFlags")!=8||GetBindingAccessorWidth("FFieldClass","CastFlags")!=8)return 1;
if(GetBindingAccessorWidth("FallbackControl","Known")!=GetAccessorWidth("FallbackControl","Known"))return 1;
if(GetBindingAccessorWidth("Unknown","None"))return 1;
std::cout<<"PASS active signatures and unchanged fallback/unknown policy\n";'''
pre=pre.replace('ACTIVE_INCLUDE','' if red else '#include "NativeLayoutActiveWidths.hpp"').replace('ACTIVE_CONTROLS','return 2;' if red else control)
(out/'controls.cpp').write_text(pre)
exe=out/'controls-san'
binding=w/'outputs/2026-10-07-l19-local-profile/units/NativeLayoutBinding.cpp' if red else root/'units/NativeLayoutBinding.cpp'
cmd=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-I',str(out),'-I',str(root/'units'),'-I',str(parser/'include'),'-I',str(width/'include'),str(binding),str(parser/'src/NativeLayoutFiles.cpp'),str(out/'controls.cpp'),'-o',str(exe)]
r=subprocess.run(cmd,capture_output=True,text=True);rows.append({'argv':cmd,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
if not r.returncode:
    for mode in (['func_good'] if red else ['virtual_metadata'] if virtual_red else ['func_good','func_boundary','alignment_good','alignment_boundary','class_cast_good','class_cast_boundary','field_cast_good','field_cast_boundary','unknown_key','unknown_width','virtual_metadata','adapter_policy']):
        c=[str(exe),mode];r=subprocess.run(c,capture_output=True,text=True);rows.append({'argv':c,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr});print(r.stdout or r.stderr,flush=True)
        if r.returncode:break
else:print(r.stderr,flush=True)
result={'commands':rows,'boundary':'Actual full NativeLayoutBinding.cpp and actual parser run. Host replaces wide-engine static storage and reduces schema to nine rows copied verbatim from genuine schema. Target signatures separately prove legacyFunc/casts unavailable and legacyMinAlignment4; active public widths8/2/8/8. FFieldClass total40 is an owned boundary fixture, not measured native sizeof. No engine publication/call occurs.','source_hashes':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [binding,schema,parser/'src/NativeLayoutFiles.cpp',Path(__file__).resolve()]}}
(root/'reports'/('ACTIVE-WIDTHS-RED.json' if red else 'VIRTUAL-SNAPSHOT-RED.json' if virtual_red else 'ACTIVE-WIDTHS-GREEN.json')).write_text(json.dumps(result,indent=2)+'\n')
sys.exit(r.returncode)
