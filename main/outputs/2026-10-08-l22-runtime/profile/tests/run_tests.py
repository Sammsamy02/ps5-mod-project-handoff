#!/usr/bin/env python3
"""Compile/run actual edited initializer preflight, pure key checker and digest gate."""
from pathlib import Path
import hashlib,json,re,subprocess,sys,configparser
root=Path(__file__).resolve().parents[1];ws=root.parents[2];old=ws/'outputs/2026-10-07-l19-local-profile-r4';test=root/'tests'
source=(root/'units/UnrealInitializer.cpp').read_text()
block=source[source.index('            // L19 PREFLIGHT BEGIN'):source.index('            // L19 PREFLIGHT END')]
block=block.replace('std::chrono::steady_clock::now()','FakeClock::now()')
compiled=(root/'units/CompiledNativeLayoutDeclarations.hpp').read_text()
texts=[json.loads(x.rstrip(',')) for x in compiled.splitlines() if x.startswith('"')]
p=json.loads((root/'profiles/townfall-build-facts.json').read_text());pins={x['name']:x for k in ['required_members','required_vtables'] for x in p[k]}
fixtures=[]
for name,ini in zip(['members','vtables'],texts):
 parser=configparser.ConfigParser();parser.optionxform=str;parser.read_string(ini)
 for section in parser.sections():
  if section=='NativeLayout':continue
  for key,value in parser[section].items():
   full=section+'::'+key;fixtures.append('staged->'+name+'.push_back({'+json.dumps(full)+','+str(int(value,0))+','+str(pins.get(full,{}).get('width') or 8)+'});')
pre=r'''
#include "RuntimeBuildProfile.hpp"
#include "JoinedLayoutPreflight.hpp"
#include "FNameAbiProfile.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <optional>
#include <sstream>
#include <algorithm>
#define STR(x) x
static std::string log_text;
struct Output {template<class...T> static void send(const char*f,const T&...args){std::ostringstream o;o<<f;((o<<" "<<args),...);log_text+=o.str()+"\n";}};
const std::string& ensure_str(const std::string&s){return s;}const char*ensure_str(const char*s){return s;}
static unsigned cleanup=0,thread_prepare=0;static rc::codecache::Metadata image;
struct FakeClock {static auto now(){return std::chrono::steady_clock::time_point(std::chrono::milliseconds(1));}};
namespace SeedArming {static uint64_t module_view_base(){return 0x400000;}static int bridge_code_cache_context(uint64_t,const rc::codecache::Metadata**p){*p=&image;return 1;}static void bridge_copyin_initializer_complete(){++cleanup;}}
namespace RC::Platform::NativeLayouts {
struct Identity {uint32_t format=1,pointer_size=8,engine_major=5,engine_minor=6;bool non_shipping=false,stats=false;std::string target="prospero-x86_64-sysv",engine_revision="5.6.1-0+++UE5",build_id="04bb4330a7d71dcefe1cc2c4d19769d5";};
struct Fact {std::string name;uint32_t offset,width;bool operator==(const Fact&)const=default;};
struct Staged {Identity identity;std::vector<Fact> members,vtables;};
static std::optional<Staged> staged;static auto GetStagedNativeLayoutFacts(){return staged;}
}
enum class DebugBuildMode{Detect,ForceShipping,ForceDebug,ForceTest};enum class StatsMode{Detect,ForceOff,ForceOn};
struct Config {DebugBuildMode DebugBuild=DebugBuildMode::Detect;StatsMode Stats=StatsMode::Detect;}UnrealConfig;
struct Ctx {struct {bool engine_version=false;}config;}ctx;
struct Results {uint32_t build_configuration=0,stats=0;struct {uint16_t major=0,minor=0;}engine_version;}results;
namespace Version {static uint32_t Major=5,Minor=6;}
namespace NativeNameThread {static const int kCandidateBindingData=0;static bool exact_import_admitted(){return true;}template<class...T>void prepare_runtime(T&&...){++thread_prepare;}}
static void preflight(){BLOCK}
int main(int argc,char**argv){assert(argc==2);std::string mode=argv[1];using namespace RC::Platform::NativeLayouts;
 image.ready=true;image.generation=1;image.base=0x400000;image.extent=rc::runtimeprofile::kProfile.extent;image.size=rc::runtimeprofile::kProfile.size;image.deadline_ms=60000;std::copy_n(rc::runtimeprofile::kProfile.sha256,65,image.sha256);
 staged.emplace();FIXTURE
 auto remove=[&](const char* key){std::erase_if(staged->members,[&](const auto&x){return x.name==key;});std::erase_if(staged->vtables,[&](const auto&x){return x.name==key;});};
 bool expected=true;unsigned expected_missing=0;
 if(mode=="baseline_maps"){std::erase_if(staged->vtables,[](const auto&x){return x.name.starts_with("FMalloc::");});expected=false;expected_missing=4;}
 else if(mode=="multiple_missing"){remove("FUObjectArray::ObjObjects");remove("FMalloc::Malloc");remove("FMalloc::QuantizeSize");expected=false;expected_missing=3;}
 else if(mode=="conditional_array_absent")remove("FArrayProperty::Inner");
 else if(mode=="conditional_alignment_absent")remove("FProperty::GetMinAlignment");
 else if(mode=="conditional_present_mismatch"){for(auto&x:staged->vtables)if(x.name=="FProperty::GetMinAlignment")++x.offset;expected=false;}
 else if(mode=="wrong_image"){image.sha256[0]='0';expected=false;}
 else if(mode=="all_map_keys_present"){for(const auto& key:rc::joinedlayout::kKeys)if(key.disposition==rc::joinedlayout::Disposition::Conditional&&!rc::joinedlayout::find(key.kind==rc::joinedlayout::Kind::Member?&staged->members:&staged->vtables,key.name))(key.kind==rc::joinedlayout::Kind::Member?staged->members:staged->vtables).push_back({key.name,0,8});}
 else if(mode=="runtime_discovery_absent"||mode=="conditional_only_missing"||mode=="profile_match"){}
 else return 3;
 const auto before=*staged;
 const auto joined=rc::joinedlayout::evaluate(&staged->members,&staged->vtables);
 assert(joined.checks.size()==63&&joined.required==49&&joined.conditional==12&&joined.deferred==2);
 assert(joined.missing_required==expected_missing);
 bool success=true;try{preflight();}catch(const std::runtime_error&e){success=false;std::cout<<e.what()<<"\n";}
 assert(success==expected);assert(thread_prepare==unsigned(expected));assert(cleanup==unsigned(!expected));
 assert(before.members==staged->members&&before.vtables==staged->vtables);
 unsigned logged=0;size_t at=0;while((at=log_text.find("[LayoutPreflight] key=",at))!=std::string::npos){++logged;++at;}assert(logged==63);
 for(const auto* name:{"FText::StaticSize_Private","FText::ValueOpsProperty"}){auto pos=log_text.find(name);assert(pos!=std::string::npos);auto line=log_text.substr(pos,log_text.find('\n',pos)-pos);assert(line.find("DEFERRED")!=std::string::npos&&line.find("conditional/runtime-discovered")!=std::string::npos);}
 if(mode=="multiple_missing")for(const auto*name:{"FUObjectArray::ObjObjects","FMalloc::Malloc","FMalloc::QuantizeSize"})assert(log_text.find(std::string(" ")+name+" ")!=std::string::npos);
 if(mode=="wrong_image")assert(log_text.find(" image.sha REFUSE ")!=std::string::npos);
 if(mode=="conditional_array_absent"||mode=="conditional_alignment_absent"){
 const auto* key=mode=="conditional_array_absent"?"FArrayProperty::Inner":"FProperty::GetMinAlignment";
 auto pos=log_text.find(std::string(" ")+key+" ");assert(pos!=std::string::npos);auto line=log_text.substr(pos,log_text.find('\n',pos)-pos);assert(line.find("conditional MISSING")!=std::string::npos);
 }
 std::cout<<"PASS "<<mode<<"; all63 logged;49 required+12 conditional+2 runtime-deferred; staged maps unchanged\n";
}
'''.replace('BLOCK',block).replace('FIXTURE','\n'.join(fixtures))
(test/'actual_preflight.cpp').write_text(pre)
results=[]
def run(c,expect=0):
 r=subprocess.run(c,text=True,capture_output=True);entry={'argv':[str(x) for x in c],'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr};results.append(entry)
 assert (r.returncode==0)==(expect==0),entry
 return r
cmd=['clang++','-std=c++20','-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-I'+str(root/'units'),'-I'+str(old/'units'),str(test/'actual_preflight.cpp'),'-o',str(test/'actual_preflight')]
run(cmd)
for scenario in ['profile_match','baseline_maps','multiple_missing','conditional_only_missing','conditional_array_absent','conditional_alignment_absent','conditional_present_mismatch','runtime_discovery_absent','all_map_keys_present','wrong_image']:
 run([str(test/'actual_preflight'),scenario])
# Negative source control: same fixture/profile, actual original initializer block lacks63-key reporting.
original=(ws/'outputs/2026-10-08-r5-thread-final-integration/thread/UnrealInitializer.cpp').read_text()
original_block=original[original.index('            // L19 PREFLIGHT BEGIN'):original.index('            // L19 PREFLIGHT END')].replace('std::chrono::steady_clock::now()','FakeClock::now()')
(test/'baseline_preflight.cpp').write_text(pre.replace(block,original_block))
baseline_cmd=cmd.copy();baseline_cmd[-3]=str(test/'baseline_preflight.cpp');baseline_cmd[-1]=str(test/'baseline_preflight')
run(baseline_cmd)
negative=run([str(test/'baseline_preflight'),'profile_match'],1)
assert 'logged==63' in negative.stderr
# Compile the actual unchanged digest admission header, selecting each original/new half.
(test/'digest_gate.cpp').write_text('#include "NativeLayoutDeclarations.hpp"\nint main(){}\n')
digestcmd=['clang++','-std=c++20','-I'+str(root/'units'),'-I'+str(old/'units'),'-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER="'+str(root/'units/CompiledNativeLayoutDeclarations.hpp')+'"',str(test/'digest_gate.cpp'),'-o',str(test/'digest_gate')]
run(digestcmd)
bad=digestcmd.copy();bad[2]='-I'+str(old/'units')
r=run(bad,1);assert 'Compiled native declarations must match' in r.stderr
# Baseline profile fails the allocator contract; exact joined checker against old maps refuses4 startup allocator keys.
run([sys.executable,str(test/'profile_contract.py'),str(old/'profiles/townfall-build-facts.json')],1)
run([sys.executable,str(test/'profile_contract.py'),str(root/'profiles/townfall-build-facts.json')])
# Identity digest and compiled maps agree with promoted artifact values exactly.
digest=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest();assert texts[3]==digest and digest in (root/'units/RuntimeBuildProfile.hpp').read_text()
virtual=configparser.ConfigParser();virtual.optionxform=str;virtual.read_string(texts[1]);assert len(virtual['FMalloc'])==17
for x in p['required_vtables']:
 section,key=x['name'].split('::');assert int(virtual[section][key],0)==x['offset']
assert source.count('const auto joined=rc::joinedlayout::evaluate(')==1
assert source.index('const auto joined=rc::joinedlayout::evaluate(')<source.index('SeedArming::consume_required_slot_seeds(std::chrono')
assert source.count('statically_verified_empty_callback')==1
(test/'RESULTS.json').write_text(json.dumps({'status':'PASS','scope':'actual initializer preflight block and actual digest gate; host ASan/UBSan; no runtime/native success','profile_digest':digest,'commands':results},indent=2)+'\n')
print('PASS actual preflight10 scenarios, original profile RED, digest mismatch compile rejection; source ordering/counts verified')
