#!/usr/bin/env python3
"""Execute the selected real binder/parser and full unchanged selected schema."""
from pathlib import Path
import hashlib,json,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[3];OWN=Path(__file__).resolve().parents[1];R3=ROOT/'outputs/2026-10-07-l19-local-profile-r3'
out=OWN/'tests/actual';out.mkdir(exist_ok=True)
schema=ROOT/'.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include/NativeLayoutSchema.inc'
parser=ROOT/'.superpowers/native-dependency-spike/ue4ss-native-layout-parser-1'
widths=ROOT/'.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/include'
text=schema.read_text();(out/'NativeLayoutSchema.inc').write_text(text)
census=json.loads((R3/'reports/ALL-REQUIRED-WIDTHS-GREEN.json').read_text())['observed_widths']
names=sorted(set(re.findall(r'&Unreal::([\w:]+)::(?:MemberOffsets|BitfieldInfos|VTableLayoutMap)',text)))
methods={'UFunction':'std::function<void(UObject*,FFrame&,void* const)>*& GetFunc();uint32_t& GetFunctionFlags();',
 'UStruct':'int16_t& GetMinAlignment();UStruct*& GetSuperStruct();','UClass':'uint64_t& GetClassCastFlags();',
 'FFieldClass':'uint64_t GetCastFlags();','FField':'FFieldVariant GetOwnerVariant();FProperty* GetNextFieldAsProperty();FName GetFName();',
 'FProperty':'EPropertyFlags& GetPropertyFlags();'}
header=['#pragma once','#include <cstdint>','#include <unordered_map>','#include <string>','#include <functional>','#define STR(s) s',
 'namespace RC::File{using StringType=std::string;}','namespace RC::Unreal{',
 'struct BitfieldInfo{uint8_t position,length,storage;};struct UObject;struct FFrame;struct FProperty;',
 'struct FName{uint64_t value;};struct FFieldVariant{void* container;bool legacyIsObject;};enum class EPropertyFlags:uint64_t{};']
maps='inline static std::unordered_map<std::string,int32_t> MemberOffsets;inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;inline static std::unordered_map<std::string,uint32_t> VTableLayoutMap;'
for name in [n for n in names if '::' not in n]:
 nested=[n.split('::',1)[1] for n in names if n.startswith(name+'::')]
 header+=['struct '+name+'{'+methods.get(name,'')+maps+''.join('struct '+n+'{'+maps+'};' for n in nested)+'};']
header+=['static_assert(sizeof(FFieldVariant)==16);','}']
(out/'NativeLayoutEngineIncludes.hpp').write_text('\n'.join(header)+'\n')
cpp=['#include "NativeLayoutBinding.hpp"','#include <NativeLayoutEngineIncludes.hpp>','#include <NativeLayoutWidths.hpp>','#include "BuildFactsChecks.hpp"','#include "RuntimeBuildProfile.hpp"','#include <fstream>','#include <sstream>','#include <iostream>',
 'namespace RC::Platform::NativeLayouts{auto GetAccessorWidth(std::string_view cls,std::string_view key)noexcept->std::optional<uint32_t>{']
for name,width in sorted(census.items()):
 cls,key=name.rsplit('::',1);cpp+=['if(cls=='+json.dumps(cls)+'&&key=='+json.dumps(key)+')return '+str(width)+';']
cpp+=['return std::nullopt;}}','using namespace RC::Platform::NativeLayouts;',
 'std::string read(const std::string&p){std::ifstream f(p);if(!f)throw std::runtime_error("Missing host file");return std::string(std::istreambuf_iterator<char>(f),{});}',
 'void require(bool b,const std::string&s){if(!b)throw std::runtime_error(s);}',
 'int main(int argc,char**argv){if(argc!=5)return 2;std::string mode=argv[1],m=read(argv[2]),v=read(argv[3]);unsigned expectedCount=std::stoul(argv[4]);std::string want;bool expectStage=true;',
 'if(mode=="full_known"){auto b=ParsePair(m,v);require(b.members.size()==expectedCount,"complete known member count");require(b.vtables.size()==std::size(rc::runtimeprofile::kRequiredVtables)-2,"known virtual count");want="Native member class requires positive UEP_TotalSize: FField";expectStage=false;}',
 'else if(mode=="geometry"){auto p=m.find("Func=0xd8");require(p!=m.npos,"Func declaration not found");m.replace(p,9,"Func=0xd9");want="Native member is outside declared class size: UFunction";expectStage=false;}',
 'else if(mode=="unknown"){auto p=m.find("[UFunction]");m.insert(p+12,"NoSuchAccessor=0x20\\n");want="Unknown native member: UFunction::NoSuchAccessor";expectStage=false;}',
 'else if(mode=="aliases"){auto p=m.find("[UObjectBase]");m.insert(p+14,"InternalIndex=0xd\\n");want="Duplicate native member or alias: UObjectBase::InternalIndex_Private";expectStage=false;}',
 'else if(mode=="old_member"){RC::Unreal::UClass::MemberOffsets.emplace("OldCustom",1);want="Native layouts cannot replace existing/cached member maps: UClass";expectStage=false;}',
 'else if(mode=="old_virtual"){RC::Unreal::FProperty::VTableLayoutMap.emplace("OldCustom",8);want="Native layouts cannot replace existing/cached vtable maps";expectStage=false;}',
 'else if(mode!="stageable"&&mode!="restage")return 2;',
 'bool staged=true;std::string detail;try{StageNativeLayoutTexts(m,v);}catch(const std::exception&e){staged=false;detail=e.what();}',
 'require(staged==expectStage,"Unexpected staging status: "+detail);if(!staged)require(detail==want,"Unexpected refusal: "+detail+" expected "+want);',
 'auto facts=GetStagedNativeLayoutFacts();require(bool(facts)==staged,"staged metadata mismatch");',
 'if(staged){require(facts->members.size()==expectedCount,"stageable member count");require(facts->vtables.size()==5,"virtual count");',
 'for(auto&p:rc::runtimeprofile::kRequiredMembers)if(p.known&&!std::string(p.name).starts_with("FField::")&&!std::string(p.name).starts_with("FFieldClass::")){bool found=false;for(auto&f:facts->members)if(f.name==p.name){require(f.offset==p.offset,"offset mismatch");require(!p.width||f.width>=p.width,"footprint minimum");found=true;}require(found,"known fact absent");}',
 'for(auto&p:rc::runtimeprofile::kRequiredVtables)if(p.known){bool found=false;for(auto&f:facts->vtables)if(f.name==p.name){require(f.offset==p.offset&&f.width==sizeof(void*),"virtual mismatch");found=true;}require(found,"known virtual absent");}',
 'rc::buildfacts::Result result{};result.ok=true;for(auto&p:rc::runtimeprofile::kRequiredMembers)rc::buildfacts::require_layout(result,"member",p,&facts->members);for(auto&p:rc::runtimeprofile::kRequiredVtables)rc::buildfacts::require_layout(result,"vtable",p,&facts->vtables);require(!result.ok,"incomplete profile must refuse");',
 'for(const char* key:{"layout.member.FField::UEP_TotalSize.measured","layout.member.FFieldClass::UEP_TotalSize.measured","layout.vtable.UEngine::LoadMap.measured","layout.vtable.AGameModeBase::InitGameState.measured"}){bool found=false;for(auto&c:result.checks)if(c.name==key&&!c.ok)found=true;require(found,std::string("missing unknown refusal: ")+key);}',
 'if(mode=="restage"){bool refused=false;try{StageNativeLayoutTexts(m,v);}catch(const std::exception&e){refused=std::string(e.what())=="Native layout bundle is already staged for this loaded lifetime";}require(refused,"old staged bundle conflict");}}']
for name in names:
 if '::' in name: continue
 cpp+=['require(RC::Unreal::'+name+'::MemberOffsets.size()=='+('size_t(mode=="old_member")' if name=='UClass' else '0')+',"published member map");',
 'require(RC::Unreal::'+name+'::BitfieldInfos.empty(),"published bitfield map");',
 'require(RC::Unreal::'+name+'::VTableLayoutMap.size()=='+('size_t(mode=="old_virtual")' if name=='FProperty' else '0')+',"published virtual map");']
cpp+=['std::cout<<"PASS "<<mode<<" stage="<<staged<<" published_maps=0 "<<detail<<"\\n";}']
source=out/'controls.cpp';source.write_text('\n'.join(cpp)+'\n');exe=out/'actual-binding-san'
cmd=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-I',str(out),'-I',str(R3/'units'),'-I',str(parser/'include'),'-I',str(widths),str(R3/'units/NativeLayoutBinding.cpp'),str(parser/'src/NativeLayoutFiles.cpp'),str(source),'-o',str(exe)]
r=subprocess.run(cmd,capture_output=True,text=True);rows=[dict(argv=cmd,exit=r.returncode,stdout=r.stdout,stderr=r.stderr)]
if r.returncode:print(r.stderr)
else:
 manifest=json.loads((OWN/'generated/UNRESOLVED.json').read_text())
 for mode in ('full_known','stageable','geometry','unknown','aliases','old_member','old_virtual','restage'):
  d=OWN/'generated'/('known-declarations' if mode=='full_known' else 'stageable-known-subset');count=manifest['known_members']-(len(manifest['stageable_subset_excluded_known_members']) if mode!='full_known' else 0)
  argv=[str(exe),mode,str(d/'MemberVariableLayout.ini'),str(d/'VTableLayout.ini'),str(count)];r=subprocess.run(argv,capture_output=True,text=True);rows.append(dict(argv=argv,exit=r.returncode,stdout=r.stdout,stderr=r.stderr));print((r.stdout or r.stderr).strip())
  if r.returncode:break
receipt={'pass':r.returncode==0,'scope':'full selected schema; actual frozen binding/parser; synthetic engine static storage only; no engine calls/publication',
 'commands':rows,'schema_byte_identical':(out/'NativeLayoutSchema.inc').read_bytes()==schema.read_bytes(),
 'host_metadata_model':'54 widths from authenticated selected-target object; ten public signature stubs previously target-validated; all real map target classes supplied',
 'source_hashes':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(__file__).resolve(),schema,R3/'units/NativeLayoutBinding.cpp',R3/'units/NativeLayoutBinding.hpp',R3/'units/NativeLayoutActiveWidths.hpp',parser/'src/NativeLayoutFiles.cpp',source,out/'NativeLayoutEngineIncludes.hpp')}}
(OWN/'reports/ACTUAL-BINDING-GREEN.json').write_text(json.dumps(receipt,indent=2)+'\n');sys.exit(r.returncode)
