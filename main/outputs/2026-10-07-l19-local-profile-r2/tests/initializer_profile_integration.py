#!/usr/bin/env python3
"""Actual initializer preflight and cache provider; host file/thread boundaries."""
from pathlib import Path
import subprocess,json,hashlib,sys
root=Path(__file__).resolve().parents[1];workspace=root.parents[1]
src=(root/'units/UnrealInitializer.cpp').read_text();bridge=(root/'units/runtime_start_bridge_r14_diag_spanpub.cpp').read_text();binding=(root/'units/NativeLayoutBinding.cpp').read_text()
block=src[src.index('            // L19 PREFLIGHT BEGIN'):src.index('            // L19 PREFLIGHT END')]
block=block.replace('std::chrono::steady_clock::now()','FakeClock::now()')
context=bridge[bridge.index('extern "C" int bridge_code_cache_context('):bridge.index('extern "C" int bridge_code_cache_signature(')]
getter=binding[binding.index('auto GetStagedNativeLayoutFacts()'):binding.index('void SetResolvedNativeLayoutAxes(')]
pre=r'''
#include "RuntimeBuildProfile.hpp"
#include "CodeSignatureProfile.hpp"
#include "CopyinStartup.hpp"
#include "NativeLayoutBinding.hpp"
#include <chrono>
#include <memory>
#include <sstream>
#include <iostream>
#include <cassert>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#define STR(s) s
static std::string log_text;
struct Output{template<class...T>static void send(const char*f,const T&...args){std::ostringstream o;o<<f;((o<<" "<<args),...);log_text+=o.str()+"\n";}};
const std::string& ensure_str(const std::string&s){return s;}const char*ensure_str(const char*s){return s;}
static uint64_t fake_ms=0,base=0x400000;
struct FakeClock{static auto now(){return std::chrono::steady_clock::time_point(std::chrono::milliseconds(fake_ms));}};
static uint64_t module_view_base(){return base;}
struct Ops{const char*image;int open(const char*){return ::open(image,O_RDONLY);}int64_t pread(int fd,void*p,size_t n,uint64_t o){return ::pread(fd,p,n,o);}int close(int fd){return ::close(fd);}uint64_t now_ms(){return fake_ms;}};
namespace SeedArming {
static uint64_t module_view_base(){return ::module_view_base();}
static l13_sink::InitializerOwner g_copyin_initializer_owner;
static rc::codecache::Cache g_code_cache;
static unsigned cleanup_callback_invocations=0;
extern "C" void bridge_copyin_initializer_complete(){++cleanup_callback_invocations;}
CONTEXT
}
namespace RC::Platform::NativeLayouts {
struct Prepared {Identity identity;std::vector<StagedLayoutFact> member_facts,virtual_facts;};
static std::mutex BindingMutex;static std::unique_ptr<Prepared>Staged;
GETTER
}
enum class DebugBuildMode{Detect,ForceShipping,ForceDebug,ForceTest};enum class StatsMode{Detect,ForceOff,ForceOn};
struct Config{DebugBuildMode DebugBuild=DebugBuildMode::Detect;StatsMode Stats=StatsMode::Detect;}UnrealConfig;
struct Ctx{struct{bool engine_version=false;}config;}ctx;
struct Results{uint32_t build_configuration=0,stats=0;struct{uint16_t major=0,minor=0;}engine_version;}results;
namespace Version{static uint32_t Major=5,Minor=6;}
void preflight(){BLOCK}
int main(int argc,char**argv){assert(argc==3);std::string scenario=argv[1];Ops ops{argv[2]};
 assert(SeedArming::g_copyin_initializer_owner.enter());
 auto& cache=SeedArming::g_code_cache;
 auto auth=cache.prepare(rc::codeprofile::kProfile,0x400000,0x400000,0xc0602c8,1,ops);assert(auth.ok);
 using namespace RC::Platform::NativeLayouts;
 Staged=std::make_unique<Prepared>();Staged->identity={1,8,5,6,false,false,"prospero-x86_64-sysv","5.6.1-0+++UE5","04bb4330a7d71dcefe1cc2c4d19769d5"};
 for(const auto&p:rc::runtimeprofile::kRequiredMembers)if(p.known)Staged->member_facts.push_back({p.name,p.offset,p.width});
 for(const auto&p:rc::runtimeprofile::kRequiredVtables)if(p.known)Staged->virtual_facts.push_back({p.name,p.offset,p.width});
 bool resolved=true;std::string required;
 if(scenario=="exact_saved_profile"){}
 else if(scenario=="version_detect"){ctx.config.engine_version=true;Version::Major=0;Version::Minor=0;}
 else if(scenario=="wrong_sha"){cache.metadata.sha256[0]='0';resolved=false;required="image.sha";}
 else if(scenario=="wrong_base"){base=0x500000;resolved=false;required="image.authenticated";}
 else if(scenario=="null_context"){cache.clear();resolved=false;required="image.authenticated";}
 else if(scenario=="wrong_size"){cache.metadata.size--;resolved=false;required="image.size";}
 else if(scenario=="missing_staged"){Staged.reset();resolved=false;required="layout.staged";}
 else if(scenario=="conflict_stats"){UnrealConfig.Stats=StatsMode::ForceOn;resolved=false;required="settings.stats";}
 else if(scenario=="conflict_config"){UnrealConfig.DebugBuild=DebugBuildMode::ForceDebug;resolved=false;required="settings.build-configuration";}
 else if(scenario=="staged_stats"){Staged->identity.stats=true;resolved=false;required="layout.stats";}
 else if(scenario=="staged_revision"){Staged->identity.engine_revision="unknown";resolved=false;required="layout.revision";}
 else if(scenario=="wrong_version"){Version::Minor=5;resolved=false;required="settings.version";}
 else if(scenario=="wrong_version_zero_minor"){Version::Minor=0;resolved=false;required="settings.version";}
 else if(scenario=="known_offset_conflict"){Staged->member_facts[0].offset++;required="layout.member.FUObjectArray::ObjFirstGCIndex.value";}
 else if(scenario=="missing_known_member"){Staged->member_facts.erase(Staged->member_facts.begin());required="layout.member.FUObjectArray::ObjFirstGCIndex.present";}
 else if(scenario=="new_cast_offset_conflict"){for(auto&f:Staged->member_facts)if(f.name=="FFieldClass::CastFlags")f.offset++;required="layout.member.FFieldClass::CastFlags.value";}
 else if(scenario=="new_cdo_missing"){std::erase_if(Staged->member_facts,[](const auto&f){return f.name=="UClass::ClassDefaultObject";});required="layout.member.UClass::ClassDefaultObject.present";}
 else if(scenario=="new_property_slot_conflict"){for(auto&f:Staged->virtual_facts)if(f.name=="FProperty::GetMinAlignment")f.offset+=8;required="layout.vtable.FProperty::GetMinAlignment.value";}
 else if(scenario=="new_property_vtable_missing"){std::erase_if(Staged->virtual_facts,[](const auto&f){return f.name=="FProperty::GetMinAlignment";});required="layout.vtable.FProperty::GetMinAlignment.present";}
 else if(scenario=="owner_denied"){SeedArming::g_copyin_initializer_owner.leave();resolved=false;required="image.authenticated";}
 else if(scenario=="deadline"){fake_ms=60000;resolved=false;required="image.deadline";}
 else if(scenario=="collect_all"){cache.metadata.sha256[0]='0';Staged->identity.stats=true;Version::Major=4;resolved=false;required="settings.version";}
 else return 2;
 bool threw=false;unsigned engine_calls=0;try{preflight();++engine_calls;}catch(const std::exception&e){threw=true;}
 assert(threw&&engine_calls==0&&SeedArming::cleanup_callback_invocations==1);
 assert((results.build_configuration==1&&results.stats==1)==resolved);
 auto check_refused=[&](const std::string&name){std::istringstream lines(log_text);std::string line;bool refused=false;while(std::getline(lines,line))if(line.find(" "+name+" ")!=std::string::npos&&line.find(" REFUSE ")!=std::string::npos)refused=true;assert(refused);};
 if(!required.empty())check_refused(required);
 if(scenario=="collect_all"){check_refused("image.sha");check_refused("layout.stats");check_refused("settings.version");}
 for(const auto&p:rc::runtimeprofile::kRequiredMembers)assert(log_text.find(p.name)!=std::string::npos);
 for(const auto&p:rc::runtimeprofile::kRequiredVtables)assert(log_text.find(p.name)!=std::string::npos);
 for(const auto* key:rc::runtimeprofile::kUnknownRequiredStaticFacts)assert(log_text.find(key)!=std::string::npos);
 std::cout<<"PASS "<<scenario<<" resolved_configuration="<<results.build_configuration<<" resolved_stats="<<results.stats<<" collect_all_refused_before_engine="<<engine_calls<<" cleanup_callback_invocations="<<SeedArming::cleanup_callback_invocations<<"\n";
}
'''.replace('CONTEXT',context).replace('GETTER',getter).replace('BLOCK',block)
p=root/'tests/initializer_profile_actual.cpp';p.write_text(pre);exe=root/'tests/initializer-profile-san'
parser=workspace/'.superpowers/native-dependency-spike/ue4ss-native-layout-parser-1/include'
cmd=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-I',str(root/'units'),'-I',str(parser),str(p),'-o',str(exe)]
r=subprocess.run(cmd,capture_output=True,text=True);rows=[{'argv':cmd,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr}]
if r.returncode:print(r.stderr);sys.exit(r.returncode)
scenarios=['exact_saved_profile','wrong_sha','wrong_base','null_context','wrong_size','missing_staged','conflict_stats','conflict_config','staged_stats','staged_revision','wrong_version','known_offset_conflict','missing_known_member','owner_denied','deadline','collect_all','wrong_version_zero_minor','version_detect','new_cast_offset_conflict','new_cdo_missing','new_property_slot_conflict','new_property_vtable_missing']
for name in ([sys.argv[1]] if len(sys.argv)>1 else scenarios):
 c=[str(exe),name,str(workspace/'outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin')];r=subprocess.run(c,capture_output=True,text=True);rows.append({'argv':c,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr});print(r.stdout or r.stderr)
 if r.returncode:break
(root/'reports'/('INITIALIZER-PROFILE-'+('RED-ZERO-VERSION' if len(sys.argv)>1 else 'GREEN')+'.json')).write_text(json.dumps({'actual_sources':{n:hashlib.sha256((root/'units'/n).read_bytes()).hexdigest() for n in ['UnrealInitializer.cpp','NativeLayoutBinding.cpp','NativeLayoutActiveWidths.hpp','runtime_start_bridge_r14_diag_spanpub.cpp']},'extraction_boundaries':['clock now uses deterministic host clock','public file API executes host open/pread/close','engine static storage replaced by complete staged snapshot fields consumed by actual accessor (no engine map access/publication)','cleanup callback records invocation only; actualFD retirement is tested by retained lifecycle harness'],'commands':rows},indent=2)+'\n');sys.exit(r.returncode)
