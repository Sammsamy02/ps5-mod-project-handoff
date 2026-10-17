"""Actual Program loader, parser, binder and extracted preflight; host storage boundary."""
from pathlib import Path
import hashlib,json,subprocess,sys
O=Path(__file__).resolve().parents[1]; W=O.parents[1]; R3=W/'outputs/2026-10-07-l19-local-profile-r3'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
parser=W/'.superpowers/native-dependency-spike/ue4ss-native-layout-parser-1'
widths=W/'.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1'
for d in ['tests/r4-host/stubs','tests/r4-host/fixtures','tests/r4-host/build']:(O/d).mkdir(parents=True,exist_ok=True)
H=O/'tests/r4-host'; stub=H/'stubs'
program=O/'units/UE4SSProgram.cpp'; src=program.read_text()
loader=src[src.index('    auto UE4SSProgram::load_unreal_offsets_from_file()'):src.index('    auto UE4SSProgram::setup_unreal()')]
initial=(O/'units/UnrealInitializer.cpp').read_text()
block=initial[initial.index('            // L19 PREFLIGHT BEGIN'):initial.index('            // L19 PREFLIGHT END')].replace('std::chrono::steady_clock::now()','FakeClock::now()')
actual=(R3/'tests/initializer_profile_actual.cpp').read_text()
a=actual.index('namespace RC::Platform::NativeLayouts {'); b=actual.index('enum class DebugBuildMode',a)
pre=actual[:a]+actual[b:actual.index('void preflight(){')]
pre+='void preflight(){'+block+'}\n'
bridge=(O/'units/runtime_start_bridge_r14_diag_spanpub.cpp').read_text()
context=bridge[bridge.index('extern "C" int bridge_code_cache_context('):bridge.index('extern "C" int bridge_code_cache_signature(')]
old_context=pre[pre.index('extern "C" int bridge_code_cache_context('):pre.index('\n}\nnamespace SeedArming',0)] if False else None
# The saved infrastructure contains the exact extracted bridge context; assert it.
assert context.strip() in pre
engine=(R3/'tests/required-binding-host/NativeLayoutEngineIncludes.hpp').read_text().replace('struct BitfieldInfo {uint8_t position,length,storage;};','struct BitfieldInfo {uint8_t position,length,storage;bool operator==(const BitfieldInfo&)const=default;};')
engine=engine.replace('struct UObject; struct FFrame;','struct UObject{inline static std::unordered_map<std::string,uint32_t> VTableLayoutMap;}; struct FFrame;')
engine+='\nnamespace RC::Unreal {struct UEngine{inline static std::unordered_map<std::string,uint32_t> VTableLayoutMap;};struct AGameModeBase{inline static std::unordered_map<std::string,uint32_t> VTableLayoutMap;};}\n'
(stub/'NativeLayoutEngineIncludes.hpp').write_text(engine)
schema=(R3/'tests/required-binding-host/NativeLayoutSchema.inc').read_text()
schema=schema[:schema.index('static auto VirtualTargets()')]+'''static auto VirtualTargets(){return std::array<VirtualTarget,4>{{
{"FProperty",&Unreal::FProperty::VTableLayoutMap},{"UObject",&Unreal::UObject::VTableLayoutMap},
{"UEngine",&Unreal::UEngine::VTableLayoutMap},{"AGameModeBase",&Unreal::AGameModeBase::VTableLayoutMap}}};}
'''
(stub/'NativeLayoutSchema.inc').write_text(schema)
controls=(R3/'tests/required-binding-host/controls.cpp').read_text()
(H/'legacy_widths.cpp').write_text(controls[:controls.index('using namespace RC::Platform::NativeLayouts;')])
p=json.loads((O/'profiles/townfall-build-facts.json').read_text()); digest=json.loads((O/'reports/AUDITED-PROFILE-GREEN.json').read_text())['profile_digest']
members=(O/'generated/known-declarations/MemberVariableLayout.ini').read_text(); virtuals=(O/'generated/known-declarations/VTableLayout.ini').read_text()
classes=sorted({x['name'].split('::')[0] for x in p['required_members']})
checks='\n'.join('assert(RC::Unreal::'+c+'::MemberOffsets.empty()&&RC::Unreal::'+c+'::BitfieldInfos.empty());' for c in classes)
checks+='\nassert(RC::Unreal::UObject::VTableLayoutMap.empty()&&RC::Unreal::UEngine::VTableLayoutMap.empty()&&RC::Unreal::AGameModeBase::VTableLayoutMap.empty()&&RC::Unreal::FProperty::VTableLayoutMap.empty());\n'
public=json.loads((O/'reports/ALL-REQUIRED-WIDTHS-GREEN.json').read_text())['observed_widths']
width_checks='\n'.join('assert(GetBindingAccessorWidth('+json.dumps(n.split('::')[0])+','+json.dumps(n.split('::')[1])+')=='+str(v)+');' for n,v in public.items())
publish_checks='\n'.join('assert(RC::Unreal::'+x['name'].split('::')[0]+'::MemberOffsets.at('+json.dumps(x['name'].split('::')[1])+')=='+str(x['offset'])+');' for x in p['required_members'])
publish_checks+='\n'+'\n'.join('assert(RC::Unreal::'+x['name'].split('::')[0]+'::VTableLayoutMap.at('+json.dumps(x['name'].split('::')[1])+')=='+str(x['offset'])+');' for x in p['required_vtables'])
prefix='#include <NativeLayoutEngineIncludes.hpp>\n#include "NativeLayoutDeclarations.hpp"\n#include "NativeLayoutActiveWidths.hpp"\n#include <map>\n#include <fstream>\n#include <sys/wait.h>\n'
body=r'''
namespace RC {
struct Activation {
 bool poison=false;unsigned reads=0;std::map<std::string,std::string>files;
 struct Text{std::string bytes;};
 auto read(const char*name)->std::optional<Text>{++reads;if(poison)throw std::runtime_error("external activation poisoned");auto i=files.find(name);if(i==files.end())return std::nullopt;return Text{i->second};}
};
struct UE4SSProgram{struct{std::shared_ptr<Activation>mod_source;}m_native_context;std::filesystem::path m_working_directory;void load_unreal_offsets_from_file();};
LOADER
}
static std::string replace_one(std::string t,const std::string&a,const std::string&b){auto i=t.find(a);assert(i!=std::string::npos);t.replace(i,a.size(),b);return t;}
static void require_refused(const std::string&name){std::istringstream lines(log_text);std::string line;bool yes=false;while(std::getline(lines,line))if(line.find(" "+name+" ")!=std::string::npos&&line.find(" REFUSE ")!=std::string::npos)yes=true;assert(yes);}
static void empty_maps(){EMPTY_CHECKS}
static int scenario(const std::string&mode,const std::filesystem::path&external){
 using namespace RC::Platform::NativeLayouts;
 RC::UE4SSProgram program;program.m_working_directory=external;
 auto activation=std::make_shared<RC::Activation>(); activation->poison=true; program.m_native_context.mod_source=activation;
 std::string m=MEMBERS,v=VTABLES,required,stage_detail;
 bool stage_refusal=false,profile_refusal=false,publish=false;
 if(mode=="field_total_missing"){m=replace_one(m,"UEP_TotalSize=0x30\n","");stage_refusal=true;}
 if(mode=="fieldclass_total_missing"){m=replace_one(m,"UEP_TotalSize=0x40\n","");stage_refusal=true;}
 if(mode=="field_total_conflict"){m=replace_one(m,"UEP_TotalSize=0x30\n","UEP_TotalSize=0x31\n");profile_refusal=true;required="layout.member.FField::UEP_TotalSize.value";}
 if(mode=="fieldclass_total_conflict"){m=replace_one(m,"UEP_TotalSize=0x40\n","UEP_TotalSize=0x41\n");profile_refusal=true;required="layout.member.FFieldClass::UEP_TotalSize.value";}
 if(mode=="loadmap_missing"){v=replace_one(v,"LoadMap=0x4f8\n","");profile_refusal=true;required="layout.vtable.UEngine::LoadMap.present";}
 if(mode=="loadmap_conflict"){v=replace_one(v,"LoadMap=0x4f8\n","LoadMap=0x500\n");profile_refusal=true;required="layout.vtable.UEngine::LoadMap.value";}
 if(mode=="initgamestate_missing"){v=replace_one(v,"InitGameState=0x7b8\n","");profile_refusal=true;required="layout.vtable.AGameModeBase::InitGameState.present";}
 if(mode=="initgamestate_conflict"){v=replace_one(v,"InitGameState=0x7b8\n","InitGameState=0x7c0\n");profile_refusal=true;required="layout.vtable.AGameModeBase::InitGameState.value";}
 if(mode=="field_size_bounds"){m=replace_one(m,"UEP_TotalSize=0x30\n","UEP_TotalSize=0x27\n");stage_refusal=true;}
 if(mode=="owner_footprint_bounds"){m=replace_one(m,"Owner=0x10\n","Owner=0x21\n");stage_refusal=true;}
 if(mode=="function_footprint_bounds"){m=replace_one(m,"Func=0xd8\n","Func=0xd9\n");stage_refusal=true;}
 if(mode=="unknown_member"){m+="\n[UnsupportedClass]\nUEP_TotalSize=0x28\nUnknown=0\n";stage_refusal=true;}
 if(mode=="malformed"){m="not an ini";stage_refusal=true;}
 if(mode=="empty"){m="";stage_refusal=true;}
 if(mode=="inconsistent_pair"){v=replace_one(v,"Stats=0\n","Stats=1\n");stage_refusal=true;}
 if(mode=="wrong_image"){SeedArming::g_code_cache.metadata.sha256[0]='0';profile_refusal=true;required="image.sha";}
 if(mode=="wrong_base"){base=0x500000;profile_refusal=true;required="image.authenticated";}
 if(mode=="wrong_size"){SeedArming::g_code_cache.metadata.size--;profile_refusal=true;required="image.size";}
 if(mode=="wrong_axes"){m=replace_one(m,"Stats=0\n","Stats=1\n");v=replace_one(v,"Stats=0\n","Stats=1\n");profile_refusal=true;required="layout.stats";}
 if(mode=="wrong_settings"){UnrealConfig.Stats=StatsMode::ForceOn;profile_refusal=true;required="settings.stats";}
 if(mode=="wrong_configuration"){UnrealConfig.DebugBuild=DebugBuildMode::ForceDebug;profile_refusal=true;required="settings.build-configuration";}
 if(mode=="wrong_version"){Version::Minor=5;profile_refusal=true;required="settings.version";}
 if(mode=="owner_denied"){SeedArming::g_copyin_initializer_owner.leave();profile_refusal=true;required="image.authenticated";}
 if(mode=="deadline"){fake_ms=60000;profile_refusal=true;required="image.deadline";}
 if(mode=="collect_all"){SeedArming::g_code_cache.metadata.sha256[0]='0';Version::Minor=5;UnrealConfig.Stats=StatsMode::ForceOn;profile_refusal=true;required="settings.version";}
 if(mode=="r3_baseline"){profile_refusal=true;required="layout.member.FField::UEP_TotalSize.measured";}
 if(mode.starts_with("existing_")){
  if(mode=="existing_member")RC::Unreal::UObjectBase::MemberOffsets.emplace("cached",99);
  if(mode=="existing_virtual")RC::Unreal::UObject::VTableLayoutMap.emplace("cached",123);
  stage_refusal=true;
 }
 bool failed=false;
 try{
  if(mode=="compiled_activation"||mode=="compiled_files"||mode=="second_stage"||mode=="r3_baseline"||mode=="selected_empty"||mode=="selected_malformed"){
   if(mode=="compiled_files")program.m_native_context.mod_source.reset();
   if(mode=="selected_empty"||mode=="selected_malformed")stage_refusal=true;
   program.load_unreal_offsets_from_file();
  }else StageNativeLayoutTexts(m,v);
 }catch(const std::exception&e){failed=true;stage_detail=e.what();}
 assert(failed==stage_refusal);assert(activation->reads==0);
 if(mode=="existing_member"){assert(RC::Unreal::UObjectBase::MemberOffsets.size()==1&&RC::Unreal::UObjectBase::MemberOffsets.at("cached")==99);RC::Unreal::UObjectBase::MemberOffsets.clear();}
 if(mode=="existing_virtual"){assert(RC::Unreal::UObject::VTableLayoutMap.size()==1&&RC::Unreal::UObject::VTableLayoutMap.at("cached")==123);RC::Unreal::UObject::VTableLayoutMap.clear();}
 auto staged=GetStagedNativeLayoutFacts();assert(bool(staged)!=stage_refusal);empty_maps();
 unsigned consumption_markers=0;
 if(!stage_refusal){
  if(mode=="second_stage"){bool refused=false;try{program.load_unreal_offsets_from_file();}catch(const std::logic_error&){refused=true;}assert(refused);assert(GetStagedNativeLayoutFacts()->members.size()==69);}
  bool refused=false;try{preflight();++consumption_markers;}catch(const std::runtime_error&){refused=true;}
  assert(refused==profile_refusal);assert(SeedArming::cleanup_callback_invocations==(profile_refusal?1u:0u));
  if(profile_refusal){assert(consumption_markers==0);require_refused(required);empty_maps();}
  else{
   assert(consumption_markers==1&&results.build_configuration==1&&results.stats==1);
   assert(staged->members.size()==69&&staged->vtables.size()==7);
   for(const auto&f:staged->members)if(f.name=="FField::Owner")assert(f.width==16);
   SetResolvedNativeLayoutAxes(false,false);PublishStagedNativeLayoutMaps(5,6,false,false);publish=true;
   PUBLISH_CHECKS
  }
  if(mode=="r3_baseline"){
   require_refused("layout.member.FFieldClass::UEP_TotalSize.measured");
   require_refused("layout.vtable.UEngine::LoadMap.measured");require_refused("layout.vtable.AGameModeBase::InitGameState.measured");
  }
  if(mode=="collect_all"){require_refused("image.sha");require_refused("settings.stats");require_refused("settings.version");}
  if(mode=="compiled_activation")assert(log_text.find("compiled-audited-townfall-l19-r4")!=std::string::npos&&log_text.find(DIGEST)!=std::string::npos);
 }else assert(consumption_markers==0&&SeedArming::cleanup_callback_invocations==0);
 std::cout<<"PASS "<<mode<<" stage_refused="<<stage_refusal<<" preflight_refused="<<profile_refusal<<" consumption_markers="<<consumption_markers<<" host_maps_published="<<publish<<" external_reads="<<activation->reads<<" detail="<<stage_detail<<"\n";return 0;
}
int main(int argc,char**argv){assert(argc==4);using namespace RC::Platform::NativeLayouts;WIDTH_CHECKS
 assert(SeedArming::g_copyin_initializer_owner.enter());Ops ops{argv[1]};
 assert(SeedArming::g_code_cache.prepare(rc::codeprofile::kProfile,0x400000,0x400000,0xc0602c8,1,ops).ok);
 std::cout<<"AUTH saved-image once sha="<<SeedArming::g_code_cache.metadata.sha256<<"\n";std::cout.flush();
 std::istringstream modes(argv[2]);std::string mode;unsigned count=0;
 while(std::getline(modes,mode,',')){auto pid=fork();assert(pid>=0);if(pid==0){int rc=scenario(mode,argv[3]);std::cout.flush();_exit(rc);}int status=0;assert(waitpid(pid,&status,0)==pid);if(!WIFEXITED(status)||WEXITSTATUS(status)!=0)return 1;++count;}
 std::cout<<"MATRIX PASS count="<<count<<" native_engine_calls=0\n";
}
'''
for key,value in {'LOADER':loader,'EMPTY_CHECKS':checks,'MEMBERS':json.dumps(members),'VTABLES':json.dumps(virtuals),'PUBLISH_CHECKS':publish_checks,'WIDTH_CHECKS':width_checks,'DIGEST':json.dumps(digest)}.items():body=body.replace(key,value)
harness=H/'integration.cpp';harness.write_text(prefix+pre+body)
cases=['compiled_activation','compiled_files','second_stage','field_total_missing','field_total_conflict','fieldclass_total_missing','fieldclass_total_conflict','loadmap_missing','loadmap_conflict','initgamestate_missing','initgamestate_conflict','field_size_bounds','owner_footprint_bounds','function_footprint_bounds','unknown_member','malformed','empty','inconsistent_pair','wrong_image','wrong_base','wrong_size','wrong_axes','wrong_settings','wrong_configuration','wrong_version','owner_denied','deadline','collect_all','existing_member','existing_virtual']
rows=[]; image=W/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
variants={
 'complete':(O/'units/CompiledNativeLayoutDeclarations.hpp',None,cases),
 'r3-baseline':(H/'fixtures/r3-provider.hpp',R3/'units/RuntimeBuildProfile.hpp',['r3_baseline']),
 'selected-empty':(H/'fixtures/empty-provider.hpp',None,['selected_empty']),
 'selected-malformed':(H/'fixtures/malformed-provider.hpp',None,['selected_malformed']),
 'digest-mismatch':(H/'fixtures/digest-provider.hpp',None,[])}
oldp=json.loads((R3/'profiles/townfall-build-facts.json').read_text()); oldd=json.loads((R3/'reports/FINAL-RECEIPT.json').read_text())['profile_digest']
def provider(path,m,v,d):path.write_text('#pragma once\nnamespace RC::Platform::NativeLayouts {inline constexpr CompiledNativeLayoutDeclarations kCompiledNativeLayoutDeclarations{'+','.join(json.dumps(x) for x in [m,v,'compiled-audited-townfall-l19-r4',d])+'};}\n')
# R3 receives the same complete declarations; its four unknown pins must still refuse.
provider(variants['r3-baseline'][0],members,virtuals,oldd)
provider(variants['selected-empty'][0],'',virtuals,digest);provider(variants['selected-malformed'][0],'not an ini',virtuals,digest)
provider(variants['digest-mismatch'][0],members,virtuals,'0'*64)
all_ok=True
for name,(header,old_header,matrix) in variants.items():
 inc=H/'fixtures'/name;inc.mkdir(exist_ok=True)
 if old_header:(inc/'RuntimeBuildProfile.hpp').write_bytes(old_header.read_bytes())
 exe=H/'build'/name
 cmd=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-O1','-D__PROSPERO__','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER="'+str(header)+'"','-I',str(inc),'-I',str(stub),'-I',str(O/'units'),'-I',str(parser/'include'),'-I',str(widths/'include'),str(O/'units/NativeLayoutBinding.cpp'),str(parser/'src/NativeLayoutFiles.cpp'),str(H/'legacy_widths.cpp'),str(harness),'-o',str(exe)]
 # Quoted includes in the loader helper normally resolve the R4 profile beside it.
 # Copy that unchanged helper into the old-profile include directory to exercise R3.
 if old_header:(inc/'NativeLayoutDeclarations.hpp').write_bytes((O/'units/NativeLayoutDeclarations.hpp').read_bytes())
 r=subprocess.run(cmd,capture_output=True,text=True);rows.append({'kind':'compile','variant':name,'argv':cmd,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
 if name=='digest-mismatch':
  ok=r.returncode!=0 and 'Compiled native declarations must match the selected runtime profile digest' in r.stderr
  all_ok&=ok;print('digest mismatch refused',ok,flush=True);continue
 if r.returncode:print(r.stderr);all_ok=False;break
 cmd=[str(exe),str(image),','.join(matrix),str(H/'fixtures/external')]
 r=subprocess.run(cmd,capture_output=True,text=True);rows.append({'kind':'matrix','variant':name,'cases':matrix,'argv':cmd,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr});print(r.stdout if not r.returncode else r.stderr,flush=True)
 if r.returncode:all_ok=False;break
sources=[program,O/'units/UnrealInitializer.cpp',O/'units/runtime_start_bridge_r14_diag_spanpub.cpp',O/'units/NativeLayoutBinding.cpp',O/'units/NativeLayoutBinding.hpp',O/'units/NativeLayoutDeclarations.hpp',O/'units/NativeLayoutActiveWidths.hpp',O/'units/RuntimeBuildProfile.hpp',parser/'src/NativeLayoutFiles.cpp',harness,H/'legacy_widths.cpp',stub/'NativeLayoutEngineIncludes.hpp',stub/'NativeLayoutSchema.inc',Path(__file__)]
receipt={'pass':all_ok,'commands':rows,'required_members':69,'required_vtables':7,'public_widths_checked':54,'Owner_public_footprint':16,'Owner_native_internal_read_bytes':8,'source_pins':{str(f):sha(f) for f in sources},'actual_loader_sha256':hashlib.sha256(loader.encode()).hexdigest(),'actual_preflight_sha256':hashlib.sha256(block.encode()).hexdigest(),'matrix_cases':sum(len(x.get('cases',[])) for x in rows),'image_authentication':'Real saved-image CodeCache prepare once per harness binary parent; forked scenario children inherit authenticated cache and fresh unpublished binder storage','engine_calls':0,'publication_boundary':'Actual PublishStagedNativeLayoutMaps swaps into host static storage only, after actual preflight success; consumption marker is host instrumentation','cleanup_boundary':'Callback invocation count only; no claim of real FD retirement','runtime_ready_claimed':False}
(O/'reports/R4-INTEGRATION.json').write_text(json.dumps(receipt,indent=2)+'\n');sys.exit(0 if all_ok else 1)
