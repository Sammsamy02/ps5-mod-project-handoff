"""Actual selected Program loader + actual binding/parser + initializer preflight."""
from pathlib import Path
import hashlib,json,re,shutil,subprocess,sys
O=Path(__file__).resolve().parents[1];W=O.parents[1]
R3=W/'outputs/2026-10-07-l19-local-profile-r3'
parser=W/'.superpowers/native-dependency-spike/ue4ss-native-layout-parser-1'
widths=W/'.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1'
for d in ['units','tests/stubs','tests/fixtures','tests/build','reports','baseline']:(O/d).mkdir(parents=True,exist_ok=True)
baseline=W/'outputs/2026-10-04-zcode-team/worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp'
if not (O/'units/UE4SSProgram.cpp').exists():
    shutil.copyfile(baseline,O/'units/UE4SSProgram.cpp');shutil.copyfile(baseline,O/'baseline/UE4SSProgram.cpp')
red='red' in sys.argv
program=O/'baseline/UE4SSProgram.cpp' if red else O/'units/UE4SSProgram.cpp'
src=program.read_text();start=src.index('    auto UE4SSProgram::load_unreal_offsets_from_file()');end=src.index('    auto UE4SSProgram::setup_unreal()',start)
loader=src[start:end]
profile=json.loads((R3/'profiles/townfall-build-facts.json').read_text())
digest=re.search(r'kDigest="([0-9a-f]+)"',(R3/'units/RuntimeBuildProfile.hpp').read_text())[1]
layout=profile['layout']
idtext='[NativeLayout]\nFormat=1\nTarget='+layout['target']+'\nPointerSize=8\nEngineMajor=5\nEngineMinor=6\nNonShipping=0\nStats=0\nEngineRevision='+layout['engine_revision']+'\nBuildId='+layout['build_id']+'\n'
groups={};virtuals={}
for pin in profile['required_members']:
    c,k=pin['name'].split('::');groups.setdefault(c,[]).append((k,pin['offset']))
members=idtext
for c,pins in groups.items():
    if next(v for k,v in pins if k=='UEP_TotalSize') is None:continue
    members+='['+c+']\n'+''.join(k+'='+hex(v)+'\n' for k,v in pins if v is not None)
vtext=idtext
for pin in profile['required_vtables']:
    if pin['offset'] is not None:
        c,k=pin['name'].split('::');virtuals.setdefault(c,[]).append((k,pin['offset']))
for c,pins in virtuals.items():vtext+='['+c+']\n'+''.join(k+'='+hex(v)+'\n' for k,v in pins)
member_count=sum(1 for line in members.splitlines() if '=' in line)-9
virtual_count=sum(len(x) for x in virtuals.values())
assert member_count==60 and virtual_count==5
for n,text in [('authenticated-r3-members.ini',members),('authenticated-r3-vtables.ini',vtext)]:
    (O/'tests/fixtures'/n).write_text(text)
external_m=idtext+'[UObjectBase]\nUEP_TotalSize=0x28\nNamePrivate=0x18\n'
external_v=idtext+'[UObject]\nProcessEvent=0x268\n'

# Complete native engine static-storage model; no stage/parser/preflight mocks.
stub=O/'tests/stubs'
engine=(R3/'tests/required-binding-host/NativeLayoutEngineIncludes.hpp').read_text().replace('struct BitfieldInfo {uint8_t position,length,storage;};','struct BitfieldInfo {uint8_t position,length,storage;bool operator==(const BitfieldInfo&)const=default;};')
engine=engine.replace('struct UObject; struct FFrame;', 'struct UObject{inline static std::unordered_map<std::string,uint32_t> VTableLayoutMap;}; struct FFrame;')
engine+='\nnamespace RC::Unreal {struct UEngine{inline static std::unordered_map<std::string,uint32_t> VTableLayoutMap;};struct AGameModeBase{inline static std::unordered_map<std::string,uint32_t> VTableLayoutMap;};}\n'
(stub/'NativeLayoutEngineIncludes.hpp').write_text(engine)
schema=(R3/'tests/required-binding-host/NativeLayoutSchema.inc').read_text()
schema=schema[:schema.index('static auto VirtualTargets()')]+'''static auto VirtualTargets(){return std::array<VirtualTarget,4>{{
{"FProperty",&Unreal::FProperty::VTableLayoutMap},{"UObject",&Unreal::UObject::VTableLayoutMap},
{"UEngine",&Unreal::UEngine::VTableLayoutMap},{"AGameModeBase",&Unreal::AGameModeBase::VTableLayoutMap}}};}
'''
(stub/'NativeLayoutSchema.inc').write_text(schema)
controls=(R3/'tests/required-binding-host/controls.cpp').read_text()
legacy=controls[:controls.index('using namespace RC::Platform::NativeLayouts;')]
(O/'tests/legacy_widths.cpp').write_text(legacy)

# Reuse only the real preflight extraction's host infrastructure. Replace its
# synthetic staged storage/getter with the real binding's exported getter.
actual=(R3/'tests/initializer_profile_actual.cpp').read_text()
cut1=actual.index('namespace RC::Platform::NativeLayouts {');cut2=actual.index('enum class DebugBuildMode',cut1)
pre=actual[:cut1]+actual[cut2:actual.index('int main(')]
pre=pre.replace('#define STR(s) s','')
prefix=r'''
#include <NativeLayoutEngineIncludes.hpp>
#if __has_include("NativeLayoutDeclarations.hpp")
#include "NativeLayoutDeclarations.hpp"
#endif
#include <map>
#include <fstream>
'''
classes=sorted(groups)
checks='\n'.join('assert(RC::Unreal::'+c+'::MemberOffsets.empty()&&RC::Unreal::'+c+'::BitfieldInfos.empty());' for c in classes)
checks+='\nassert(RC::Unreal::UObject::VTableLayoutMap.empty()&&RC::Unreal::UEngine::VTableLayoutMap.empty()&&RC::Unreal::AGameModeBase::VTableLayoutMap.empty()&&RC::Unreal::FProperty::VTableLayoutMap.empty());\n'
body=r'''
namespace RC {
namespace Platform {using namespace ::RC::Platform;}
struct Activation {
    bool poison=false;unsigned reads=0;std::map<std::string,std::string>files;
    struct Text {std::string bytes;};
    auto read(const char*name)->std::optional<Text>{++reads;if(poison)throw std::runtime_error("external activation poisoned");auto it=files.find(name);if(it==files.end())return std::nullopt;return Text{it->second};}
};
struct UE4SSProgram {
    struct {std::shared_ptr<Activation>mod_source;}m_native_context;
    std::filesystem::path m_working_directory;
    void load_unreal_offsets_from_file();
};
LOADER
}
int main(int argc,char**argv){assert(argc==4);std::string mode=argv[1];
using namespace RC::Platform::NativeLayouts;
RC::UE4SSProgram program;program.m_working_directory=argv[3];
auto activation=std::make_shared<RC::Activation>();
activation->files={{"ue4ss/MemberVariableLayout.ini",EXTERNAL_M},{"ue4ss/VTableLayout.ini",EXTERNAL_V}};
program.m_native_context.mod_source=activation;
bool compiled=mode!="external_activation"&&mode!="external_files";
if(compiled)activation->poison=true;
if(mode=="compiled_files"||mode=="external_files")program.m_native_context.mod_source.reset();
if(mode=="external_files"){
 std::filesystem::create_directories(program.m_working_directory);
 std::ofstream(program.m_working_directory/"MemberVariableLayout.ini")<<EXTERNAL_M;
 std::ofstream(program.m_working_directory/"VTableLayout.ini")<<EXTERNAL_V;
}
if(mode=="existing_member")RC::Unreal::UObjectBase::MemberOffsets.emplace("cached",99);
if(mode=="existing_bits")RC::Unreal::UObjectBase::BitfieldInfos.emplace("cached",RC::Unreal::BitfieldInfo{1,1,1});
if(mode=="existing_virtual")RC::Unreal::UObject::VTableLayoutMap.emplace("cached",123);
bool failed=false;std::string error;
try{program.load_unreal_offsets_from_file();}catch(const std::exception&e){failed=true;error=e.what();}
bool refusal=mode=="malformed"||mode=="empty"||mode=="inconsistent_pair"||mode.starts_with("existing_");
if(failed!=refusal){std::cerr<<"FAIL "<<mode<<" stage_refused="<<failed<<" detail="<<error<<"\n";return 1;}
if(compiled)assert(activation->reads==0);else assert(activation->reads==(mode=="external_activation"?2:0));
auto staged=GetStagedNativeLayoutFacts();assert(bool(staged)!=refusal);
if(!refusal){assert(staged->members.size()==(compiled?60u:2u));assert(staged->vtables.size()==(compiled?5u:1u));}
if(compiled)assert(log_text.find("compiled-authenticated-r3-test")!=std::string::npos&&log_text.find(DIGEST)!=std::string::npos);
if(mode=="existing_member"){assert(RC::Unreal::UObjectBase::MemberOffsets.size()==1&&RC::Unreal::UObjectBase::MemberOffsets.at("cached")==99);RC::Unreal::UObjectBase::MemberOffsets.clear();}
if(mode=="existing_bits"){assert(RC::Unreal::UObjectBase::BitfieldInfos.size()==1&&RC::Unreal::UObjectBase::BitfieldInfos.at("cached").position==1);RC::Unreal::UObjectBase::BitfieldInfos.clear();}
if(mode=="existing_virtual"){assert(RC::Unreal::UObject::VTableLayoutMap.size()==1&&RC::Unreal::UObject::VTableLayoutMap.at("cached")==123);RC::Unreal::UObject::VTableLayoutMap.clear();}
CHECKS
if(mode=="second_stage"){
 bool second=false;try{program.load_unreal_offsets_from_file();}catch(const std::logic_error&e){second=std::string(e.what())=="Native layout bundle is already staged for this loaded lifetime";}
 assert(second);auto later=GetStagedNativeLayoutFacts();assert(later->members.size()==60&&later->vtables.size()==5);assert(activation->reads==0);
}
bool profile_case=mode=="compiled_activation"||mode=="incomplete"||mode=="wrong_image"||mode=="wrong_base"||mode=="wrong_axes"||mode=="wrong_settings"||mode=="deadline";
if(profile_case){
 Ops ops{argv[2]};assert(SeedArming::g_copyin_initializer_owner.enter());
 auto auth=SeedArming::g_code_cache.prepare(rc::codeprofile::kProfile,0x400000,0x400000,0xc0602c8,1,ops);assert(auth.ok);
 if(mode=="wrong_image")SeedArming::g_code_cache.metadata.sha256[0]='0';
 if(mode=="wrong_base")base=0x500000;
 if(mode=="wrong_settings")UnrealConfig.Stats=StatsMode::ForceOn;
 if(mode=="deadline")fake_ms=60000;
 bool refused=false;unsigned calls=0;try{preflight();++calls;}catch(const std::runtime_error&){refused=true;}
 assert(refused&&calls==0&&SeedArming::cleanup_callback_invocations==1);
 auto require=[&](const char*name){std::istringstream lines(log_text);std::string line;bool found=false;while(std::getline(lines,line))if(line.find(std::string(" ")+name+" ")!=std::string::npos&&line.find(" REFUSE ")!=std::string::npos)found=true;assert(found);};
 require("layout.member.FField::UEP_TotalSize.measured");
 if(mode=="wrong_image")require("image.sha");
 if(mode=="wrong_base")require("image.authenticated");
 if(mode=="wrong_axes")require("layout.stats");
 if(mode=="wrong_settings")require("settings.stats");
 if(mode=="deadline")require("image.deadline");
 CHECKS
}
std::cout<<"PASS "<<mode<<" staged="<<bool(staged)<<" external_reads="<<activation->reads<<" maps_unchanged=1"<<"\n";
}
'''.replace('LOADER',loader).replace('EXTERNAL_M',json.dumps(external_m)).replace('EXTERNAL_V',json.dumps(external_v)).replace('DIGEST',json.dumps(digest)).replace('CHECKS',checks)
harness=O/'tests'/('loader-red.cpp' if red else 'loader.cpp');harness.write_text(prefix+pre+body)
rows=[];all_ok=True
image=W/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
variants={'selected':(members,vtext,digest), 'malformed':('not an ini',vtext,digest),'empty':('',vtext,digest),
          'inconsistent_pair':(members,vtext.replace('Stats=0','Stats=1'),digest),
          'wrong_axes':(members.replace('Stats=0','Stats=1'),vtext.replace('Stats=0','Stats=1'),digest),
          'wrong_digest':(members,vtext,'0'*64),'absent':None}
modes={'selected':['compiled_activation','compiled_files','incomplete','wrong_image','wrong_base','wrong_settings','deadline','existing_member','existing_bits','existing_virtual','second_stage'],
       'malformed':['malformed'],'empty':['empty'],'inconsistent_pair':['inconsistent_pair'],'wrong_axes':['wrong_axes'],'absent':['external_activation','external_files']}
if red:variants={'selected':variants['selected']};modes={'selected':['compiled_activation']}
for variant,pair in variants.items():
    header=O/'tests/fixtures'/('selected-'+variant+'.hpp')
    if pair:
        header.write_text('#pragma once\nnamespace RC::Platform::NativeLayouts {\ninline constexpr CompiledNativeLayoutDeclarations kCompiledNativeLayoutDeclarations{'+','.join(json.dumps(x) for x in (*pair[:2],'compiled-authenticated-r3-test',pair[2]))+'};\n}\n')
    exe=O/'tests/build'/('loader-'+variant+('-red' if red else '-san'))
    cmd=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-O1','-D__PROSPERO__','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer',
         '-I',str(stub),'-I',str(O/'units'),'-I',str(R3/'units'),'-I',str(parser/'include'),'-I',str(widths/'include'),
         str(R3/'units/NativeLayoutBinding.cpp'),str(parser/'src/NativeLayoutFiles.cpp'),str(O/'tests/legacy_widths.cpp'),str(harness),'-o',str(exe)]
    if pair:cmd.insert(1,'-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER="'+str(header)+'"')
    r=subprocess.run(cmd,capture_output=True,text=True);rows.append(dict(kind='compile',variant=variant,argv=cmd,exit=r.returncode,stdout=r.stdout,stderr=r.stderr))
    if variant=='wrong_digest':
        ok=r.returncode!=0 and 'Compiled native declarations must match the selected runtime profile digest' in r.stderr
        all_ok&=ok;print('digest-mismatch expected compile refusal',ok);continue
    if r.returncode:
        print(r.stderr);all_ok=False;break
    for mode in modes[variant]:
        cmd=[str(exe),mode,str(image),str(O/'tests/fixtures'/('external-'+mode))]
        r=subprocess.run(cmd,capture_output=True,text=True);rows.append(dict(kind='run',variant=variant,scenario=mode,argv=cmd,exit=r.returncode,stdout=r.stdout,stderr=r.stderr))
        print(r.stdout or r.stderr,flush=True)
        if r.returncode:all_ok=False;break
    if not all_ok:break
receipt=dict(pass_=all_ok,red=red,actual_program_sha256=hashlib.sha256(program.read_bytes()).hexdigest(),
 actual_binding_sha256=hashlib.sha256((R3/'units/NativeLayoutBinding.cpp').read_bytes()).hexdigest(),runtime_profile_sha256=hashlib.sha256((R3/'units/RuntimeBuildProfile.hpp').read_bytes()).hexdigest(),
 profile_digest=digest,commands=rows,fixture_members=60,fixture_vtables=5,
 fixture_limits='Only authenticated R3 facts with proven totals included; unknown FField/FFieldClass totals cause strict actual preflight refusal. No invented target totals.',
 extraction_boundaries=['actual selected Program loader method; platform/file/activation context is host','actual NativeLayoutBinding.cpp and NativeLayoutFiles.cpp stage/checkempty/snapshot','engine static storage host model; actual width policy via public return-type model and target legacy census','actual initializer preflight/context extraction; host clock/file operations; engine call counter begins immediately after preflight','cleanup callback invocation only, no claim of real FD retirement'],findings=[])
receipt['pass']=receipt.pop('pass_');(O/'reports'/('LOADER-RED.json' if red else 'LOADER-GREEN.json')).write_text(json.dumps(receipt,indent=2)+'\n')
sys.exit(0 if all_ok else 1)
