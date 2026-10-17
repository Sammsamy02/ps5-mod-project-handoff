#include <NativeLayoutEngineIncludes.hpp>
#include "NativeLayoutDeclarations.hpp"
#include "NativeLayoutActiveWidths.hpp"
#include <map>
#include <fstream>
#include <sys/wait.h>

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
extern "C" int bridge_code_cache_context(uint64_t module_base,const rc::codecache::Metadata** out)
{
    if(out)*out=nullptr;
    if(!out||!g_copyin_initializer_owner.allows())return 0;
    if(!g_code_cache.identity(module_base))return 0;
    *out=&g_code_cache.metadata;return 1;
}

}
enum class DebugBuildMode{Detect,ForceShipping,ForceDebug,ForceTest};enum class StatsMode{Detect,ForceOff,ForceOn};
struct Config{DebugBuildMode DebugBuild=DebugBuildMode::Detect;StatsMode Stats=StatsMode::Detect;}UnrealConfig;
struct Ctx{struct{bool engine_version=false;}config;}ctx;
struct Results{uint32_t build_configuration=0,stats=0;struct{uint16_t major=0,minor=0;}engine_version;}results;
namespace Version{static uint32_t Major=5,Minor=6;}
void preflight(){            // L19 PREFLIGHT BEGIN: collect every independent required fact
            // before any seeded binding, native call or map publication.
            try
            {
                const uint64_t actual_base=SeedArming::module_view_base();
                const rc::codecache::Metadata* profile_image=nullptr;
                if(SeedArming::bridge_code_cache_context(actual_base,&profile_image)!=1) profile_image=nullptr;
                const auto staged=RC::Platform::NativeLayouts::GetStagedNativeLayoutFacts();
                rc::buildfacts::Declared declared{};
                if(staged)
                {
                    const auto& id=staged->identity;
                    declared={id.format,id.pointer_size,id.engine_major,id.engine_minor,id.non_shipping,id.stats,
                              id.target,id.engine_revision,id.build_id};
                }
                rc::buildfacts::Settings selected{};
                selected.build_configuration=-1;
                switch(UnrealConfig.DebugBuild)
                {
                case DebugBuildMode::Detect: selected.build_configuration=0; break;
                case DebugBuildMode::ForceShipping: selected.build_configuration=1; break;
                case DebugBuildMode::ForceDebug: selected.build_configuration=2; break;
                case DebugBuildMode::ForceTest: selected.build_configuration=3; break;
                }
                selected.stats=-1;
                switch(UnrealConfig.Stats)
                {
                case StatsMode::Detect: selected.stats=0; break;
                case StatsMode::ForceOff: selected.stats=1; break;
                case StatsMode::ForceOn: selected.stats=2; break;
                }
                selected.version_detect=ctx.config.engine_version;
                selected.major=ctx.config.engine_version?0:static_cast<uint32_t>(Version::Major);
                selected.minor=ctx.config.engine_version?0:static_cast<uint32_t>(Version::Minor);
                selected.fname_case_preserving=rc::fnameprofile::kCasePreserving;
                selected.fname_bytes=rc::fnameprofile::kOutputBytes;
                selected.fname_alignment=rc::fnameprofile::kOutputAlignment;
                const auto now_ms=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    FakeClock::now().time_since_epoch()).count());
                auto facts=rc::buildfacts::evaluate_profile(&rc::runtimeprofile::kProfile,profile_image,actual_base,
                                               staged?&declared:nullptr,selected,now_ms);
                // Axes stay independently resolved even when a later required
                // layout fact is absent; that absence still refuses startup.
                results.build_configuration=facts.build_configuration;
                results.stats=facts.stats;
                if(facts.ok)
                {
                    results.engine_version.major=static_cast<uint16_t>(rc::runtimeprofile::kProfile.major);
                    results.engine_version.minor=static_cast<uint16_t>(rc::runtimeprofile::kProfile.minor);
                }
                for(const auto& pin:rc::runtimeprofile::kRequiredMembers)
                    rc::buildfacts::require_layout(facts,"member",pin,staged?&staged->members:nullptr);
                for(const auto& pin:rc::runtimeprofile::kRequiredVtables)
                    rc::buildfacts::require_layout(facts,"vtable",pin,staged?&staged->vtables:nullptr);
                for(const auto* key:rc::runtimeprofile::kUnknownRequiredStaticFacts)
                    rc::buildfacts::require_static_fact(facts,key,false,"target fact remains unproved in retained evidence; no template inference");
                Output::send(STR("[BuildProfile] digest={} image={} base=0x{:X} build_configuration={} stats={} configuration_source={} stats_source={}\n"),
                    ensure_str(rc::runtimeprofile::kDigest),ensure_str(profile_image?profile_image->sha256:"unavailable"),actual_base,
                    results.build_configuration,results.stats,ensure_str(rc::runtimeprofile::kProfile.build_source),ensure_str(rc::runtimeprofile::kProfile.stats_source));
                std::string failures;
                for(const auto& fact:facts.checks)
                {
                    Output::send(STR("[BuildProfile] check={} result={} detail={}\n"),ensure_str(fact.name),
                                 ensure_str(fact.ok?"PASS":"REFUSE"),ensure_str(fact.detail));
                    if(!fact.ok) failures+=fact.name+"; ";
                }
                if(!facts.ok) throw std::runtime_error("[BuildProfile] collect-all preflight refused: "+failures);
            }
            catch(...)
            {
                // Preflight precedes the existing consumer's cleanup owner.
                // Preserve one-shot bank retirement on this earlier refusal.
                SeedArming::bridge_copyin_initializer_complete();
                throw;
            }
}

namespace RC {
struct Activation {
 bool poison=false;unsigned reads=0;std::map<std::string,std::string>files;
 struct Text{std::string bytes;};
 auto read(const char*name)->std::optional<Text>{++reads;if(poison)throw std::runtime_error("external activation poisoned");auto i=files.find(name);if(i==files.end())return std::nullopt;return Text{i->second};}
};
struct UE4SSProgram{struct{std::shared_ptr<Activation>mod_source;}m_native_context;std::filesystem::path m_working_directory;void load_unreal_offsets_from_file();};
    auto UE4SSProgram::load_unreal_offsets_from_file() -> void
    {
#ifdef __PROSPERO__
        if (const auto compiled = Platform::NativeLayouts::GetCompiledNativeLayoutDeclarations(); compiled)
        {
            Output::send(STR("[NativeLayout] declaration_provider=compiled source={} profile_digest={}\n"),
                         ensure_str(std::string{compiled->selection_source}), ensure_str(std::string{compiled->profile_digest}));
            Platform::NativeLayouts::StageNativeLayoutTexts(compiled->members, compiled->vtables);
        }
        else if (m_native_context.mod_source)
        {
            const auto members = m_native_context.mod_source->read("ue4ss/MemberVariableLayout.ini");
            const auto virtuals = m_native_context.mod_source->read("ue4ss/VTableLayout.ini");
            if (!members || !virtuals)
                throw std::runtime_error("Native activation requires both member and vtable layout files");
            Platform::NativeLayouts::StageNativeLayoutTexts(members->bytes, virtuals->bytes);
        }
        else
            Platform::NativeLayouts::StageNativeLayoutFiles(m_working_directory);
        // Native maps publish together only after real engine axes are resolved.
#else
        std::filesystem::path file_path = m_working_directory / "MemberVariableLayout.ini";
        if (std::filesystem::exists(file_path))
        {
            auto file = File::open(file_path);
            if (auto file_contents = file.read_all(); !file_contents.empty())
            {
                Ini::Parser parser;
                parser.parse(file_contents);
                file.close();

                // The following code is auto-generated.
#include <MacroSetter.hpp>

                m_custom_member_variable_layout_loaded = true;
            }
        }
#endif
    }


}
static std::string replace_one(std::string t,const std::string&a,const std::string&b){auto i=t.find(a);assert(i!=std::string::npos);t.replace(i,a.size(),b);return t;}
static void require_refused(const std::string&name){std::istringstream lines(log_text);std::string line;bool yes=false;while(std::getline(lines,line))if(line.find(" "+name+" ")!=std::string::npos&&line.find(" REFUSE ")!=std::string::npos)yes=true;assert(yes);}
static void empty_maps(){assert(RC::Unreal::FArrayProperty::MemberOffsets.empty()&&RC::Unreal::FArrayProperty::BitfieldInfos.empty());
assert(RC::Unreal::FBoolProperty::MemberOffsets.empty()&&RC::Unreal::FBoolProperty::BitfieldInfos.empty());
assert(RC::Unreal::FEnumProperty::MemberOffsets.empty()&&RC::Unreal::FEnumProperty::BitfieldInfos.empty());
assert(RC::Unreal::FField::MemberOffsets.empty()&&RC::Unreal::FField::BitfieldInfos.empty());
assert(RC::Unreal::FFieldClass::MemberOffsets.empty()&&RC::Unreal::FFieldClass::BitfieldInfos.empty());
assert(RC::Unreal::FProperty::MemberOffsets.empty()&&RC::Unreal::FProperty::BitfieldInfos.empty());
assert(RC::Unreal::FStructProperty::MemberOffsets.empty()&&RC::Unreal::FStructProperty::BitfieldInfos.empty());
assert(RC::Unreal::FUObjectArray::MemberOffsets.empty()&&RC::Unreal::FUObjectArray::BitfieldInfos.empty());
assert(RC::Unreal::FUObjectItem::MemberOffsets.empty()&&RC::Unreal::FUObjectItem::BitfieldInfos.empty());
assert(RC::Unreal::TUObjectArray::MemberOffsets.empty()&&RC::Unreal::TUObjectArray::BitfieldInfos.empty());
assert(RC::Unreal::UClass::MemberOffsets.empty()&&RC::Unreal::UClass::BitfieldInfos.empty());
assert(RC::Unreal::UField::MemberOffsets.empty()&&RC::Unreal::UField::BitfieldInfos.empty());
assert(RC::Unreal::UFunction::MemberOffsets.empty()&&RC::Unreal::UFunction::BitfieldInfos.empty());
assert(RC::Unreal::UObjectBase::MemberOffsets.empty()&&RC::Unreal::UObjectBase::BitfieldInfos.empty());
assert(RC::Unreal::UStruct::MemberOffsets.empty()&&RC::Unreal::UStruct::BitfieldInfos.empty());
assert(RC::Unreal::UObject::VTableLayoutMap.empty()&&RC::Unreal::UEngine::VTableLayoutMap.empty()&&RC::Unreal::AGameModeBase::VTableLayoutMap.empty()&&RC::Unreal::FProperty::VTableLayoutMap.empty());
}
static int scenario(const std::string&mode,const std::filesystem::path&external){
 using namespace RC::Platform::NativeLayouts;
 RC::UE4SSProgram program;program.m_working_directory=external;
 auto activation=std::make_shared<RC::Activation>(); activation->poison=true; program.m_native_context.mod_source=activation;
 std::string m="[NativeLayout]\nFormat=1\nTarget=prospero-x86_64-sysv\nPointerSize=8\nEngineMajor=5\nEngineMinor=6\nNonShipping=0\nStats=0\nEngineRevision=5.6.1-0+++UE5\nBuildId=04bb4330a7d71dcefe1cc2c4d19769d5\n\n[FArrayProperty]\nInner=0x78\nUEP_TotalSize=0x80\n\n[FBoolProperty]\nByteMask=0x72\nByteOffset=0x71\nFieldMask=0x73\nUEP_TotalSize=0x78\n\n[FEnumProperty]\nEnum=0x78\nUEP_TotalSize=0x80\nUnderlyingProp=0x70\n\n[FField]\nClassPrivate=0x8\nNamePrivate=0x20\nNext=0x18\nOwner=0x10\nUEP_TotalSize=0x30\n\n[FFieldClass]\nCastFlags=0x10\nFName=0x0\nSuperClass=0x20\nUEP_TotalSize=0x40\n\n[FProperty]\nArrayDim=0x2c\nElementSize=0x30\nOffset_Internal=0x44\nPropertyFlags=0x38\nUEP_TotalSize=0x70\n\n[FStructProperty]\nStruct=0x70\nUEP_TotalSize=0x78\n\n[FUObjectArray]\nMaxObjectsNotConsideredByGC=0x8\nObjFirstGCIndex=0x0\nObjLastNonGCIndex=0x4\nObjObjects=0x10\nOpenForDisregardForGC=0xc\nPrimarySerialNumber=0xb0\nUEP_TotalSize=0xb8\nUObjectCreateListeners=0x68\nUObjectDeleteListeners=0x78\nbShouldRecycleObjectIndices=0xb4\n\n[FUObjectItem]\nFlags=0x8\nObject=0x0\nUEP_TotalSize=0x18\n\n[TUObjectArray]\nMaxChunks=0x18\nMaxElements=0x10\nNumChunks=0x1c\nNumElements=0x14\nObjects=0x0\nPreAllocatedObjects=0x8\nUEP_TotalSize=0x20\n\n[UClass]\nClassCastFlags=0xd8\nClassDefaultObject=0x110\nInterfaces=0x1d8\nUEP_TotalSize=0x200\n\n[UField]\nNext=0x28\nUEP_TotalSize=0x30\n\n[UFunction]\nFunc=0xd8\nFunctionFlags=0xb0\nNumParms=0xb4\nParmsSize=0xb6\nReturnValueOffset=0xb8\nUEP_TotalSize=0xe0\n\n[UObjectBase]\nClassPrivate=0x10\nInternalIndex_Private=0xc\nNamePrivate=0x18\nObjectFlags=0x8\nOuterPrivate=0x20\nUEP_TotalSize=0x28\n\n[UStruct]\nChildProperties=0x50\nChildren=0x48\nMinAlignment=0x5c\nPropertiesSize=0x58\nSuperStruct=0x40\nUEP_TotalSize=0xb0\n",v="[NativeLayout]\nFormat=1\nTarget=prospero-x86_64-sysv\nPointerSize=8\nEngineMajor=5\nEngineMinor=6\nNonShipping=0\nStats=0\nEngineRevision=5.6.1-0+++UE5\nBuildId=04bb4330a7d71dcefe1cc2c4d19769d5\n\n[AGameModeBase]\nInitGameState=0x7b8\n\n[FProperty]\nCopyValuesInternal=0xf8\nDestroyValueInternal=0x140\nGetMinAlignment=0x170\n\n[UEngine]\nLoadMap=0x4f8\nTick=0x310\n\n[UObject]\nProcessEvent=0x268\n",required,stage_detail;
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
   assert(RC::Unreal::FUObjectArray::MemberOffsets.at("ObjFirstGCIndex")==0);
assert(RC::Unreal::FUObjectArray::MemberOffsets.at("ObjLastNonGCIndex")==4);
assert(RC::Unreal::FUObjectArray::MemberOffsets.at("MaxObjectsNotConsideredByGC")==8);
assert(RC::Unreal::FUObjectArray::MemberOffsets.at("OpenForDisregardForGC")==12);
assert(RC::Unreal::FUObjectArray::MemberOffsets.at("ObjObjects")==16);
assert(RC::Unreal::FUObjectArray::MemberOffsets.at("UObjectCreateListeners")==104);
assert(RC::Unreal::FUObjectArray::MemberOffsets.at("UObjectDeleteListeners")==120);
assert(RC::Unreal::FUObjectArray::MemberOffsets.at("PrimarySerialNumber")==176);
assert(RC::Unreal::FUObjectArray::MemberOffsets.at("bShouldRecycleObjectIndices")==180);
assert(RC::Unreal::FUObjectArray::MemberOffsets.at("UEP_TotalSize")==184);
assert(RC::Unreal::TUObjectArray::MemberOffsets.at("Objects")==0);
assert(RC::Unreal::TUObjectArray::MemberOffsets.at("PreAllocatedObjects")==8);
assert(RC::Unreal::TUObjectArray::MemberOffsets.at("MaxElements")==16);
assert(RC::Unreal::TUObjectArray::MemberOffsets.at("NumElements")==20);
assert(RC::Unreal::TUObjectArray::MemberOffsets.at("MaxChunks")==24);
assert(RC::Unreal::TUObjectArray::MemberOffsets.at("NumChunks")==28);
assert(RC::Unreal::TUObjectArray::MemberOffsets.at("UEP_TotalSize")==32);
assert(RC::Unreal::FUObjectItem::MemberOffsets.at("UEP_TotalSize")==24);
assert(RC::Unreal::FUObjectItem::MemberOffsets.at("Object")==0);
assert(RC::Unreal::FUObjectItem::MemberOffsets.at("Flags")==8);
assert(RC::Unreal::UObjectBase::MemberOffsets.at("UEP_TotalSize")==40);
assert(RC::Unreal::UObjectBase::MemberOffsets.at("NamePrivate")==24);
assert(RC::Unreal::UObjectBase::MemberOffsets.at("OuterPrivate")==32);
assert(RC::Unreal::UObjectBase::MemberOffsets.at("ClassPrivate")==16);
assert(RC::Unreal::UObjectBase::MemberOffsets.at("InternalIndex_Private")==12);
assert(RC::Unreal::UObjectBase::MemberOffsets.at("ObjectFlags")==8);
assert(RC::Unreal::UField::MemberOffsets.at("UEP_TotalSize")==48);
assert(RC::Unreal::UField::MemberOffsets.at("Next")==40);
assert(RC::Unreal::UStruct::MemberOffsets.at("UEP_TotalSize")==176);
assert(RC::Unreal::UStruct::MemberOffsets.at("SuperStruct")==64);
assert(RC::Unreal::UStruct::MemberOffsets.at("Children")==72);
assert(RC::Unreal::UStruct::MemberOffsets.at("ChildProperties")==80);
assert(RC::Unreal::UStruct::MemberOffsets.at("PropertiesSize")==88);
assert(RC::Unreal::UStruct::MemberOffsets.at("MinAlignment")==92);
assert(RC::Unreal::UClass::MemberOffsets.at("UEP_TotalSize")==512);
assert(RC::Unreal::UClass::MemberOffsets.at("ClassDefaultObject")==272);
assert(RC::Unreal::UClass::MemberOffsets.at("Interfaces")==472);
assert(RC::Unreal::UClass::MemberOffsets.at("ClassCastFlags")==216);
assert(RC::Unreal::FField::MemberOffsets.at("UEP_TotalSize")==48);
assert(RC::Unreal::FField::MemberOffsets.at("ClassPrivate")==8);
assert(RC::Unreal::FField::MemberOffsets.at("Owner")==16);
assert(RC::Unreal::FField::MemberOffsets.at("Next")==24);
assert(RC::Unreal::FField::MemberOffsets.at("NamePrivate")==32);
assert(RC::Unreal::FFieldClass::MemberOffsets.at("UEP_TotalSize")==64);
assert(RC::Unreal::FFieldClass::MemberOffsets.at("FName")==0);
assert(RC::Unreal::FFieldClass::MemberOffsets.at("SuperClass")==32);
assert(RC::Unreal::FFieldClass::MemberOffsets.at("CastFlags")==16);
assert(RC::Unreal::FProperty::MemberOffsets.at("UEP_TotalSize")==112);
assert(RC::Unreal::FProperty::MemberOffsets.at("ArrayDim")==44);
assert(RC::Unreal::FProperty::MemberOffsets.at("ElementSize")==48);
assert(RC::Unreal::FProperty::MemberOffsets.at("PropertyFlags")==56);
assert(RC::Unreal::FProperty::MemberOffsets.at("Offset_Internal")==68);
assert(RC::Unreal::UFunction::MemberOffsets.at("UEP_TotalSize")==224);
assert(RC::Unreal::UFunction::MemberOffsets.at("NumParms")==180);
assert(RC::Unreal::UFunction::MemberOffsets.at("ParmsSize")==182);
assert(RC::Unreal::UFunction::MemberOffsets.at("ReturnValueOffset")==184);
assert(RC::Unreal::UFunction::MemberOffsets.at("FunctionFlags")==176);
assert(RC::Unreal::UFunction::MemberOffsets.at("Func")==216);
assert(RC::Unreal::FStructProperty::MemberOffsets.at("UEP_TotalSize")==120);
assert(RC::Unreal::FStructProperty::MemberOffsets.at("Struct")==112);
assert(RC::Unreal::FArrayProperty::MemberOffsets.at("UEP_TotalSize")==128);
assert(RC::Unreal::FArrayProperty::MemberOffsets.at("Inner")==120);
assert(RC::Unreal::FBoolProperty::MemberOffsets.at("UEP_TotalSize")==120);
assert(RC::Unreal::FBoolProperty::MemberOffsets.at("ByteOffset")==113);
assert(RC::Unreal::FBoolProperty::MemberOffsets.at("ByteMask")==114);
assert(RC::Unreal::FBoolProperty::MemberOffsets.at("FieldMask")==115);
assert(RC::Unreal::FEnumProperty::MemberOffsets.at("UEP_TotalSize")==128);
assert(RC::Unreal::FEnumProperty::MemberOffsets.at("UnderlyingProp")==112);
assert(RC::Unreal::FEnumProperty::MemberOffsets.at("Enum")==120);
assert(RC::Unreal::UObject::VTableLayoutMap.at("ProcessEvent")==616);
assert(RC::Unreal::UEngine::VTableLayoutMap.at("LoadMap")==1272);
assert(RC::Unreal::UEngine::VTableLayoutMap.at("Tick")==784);
assert(RC::Unreal::AGameModeBase::VTableLayoutMap.at("InitGameState")==1976);
assert(RC::Unreal::FProperty::VTableLayoutMap.at("CopyValuesInternal")==248);
assert(RC::Unreal::FProperty::VTableLayoutMap.at("DestroyValueInternal")==320);
assert(RC::Unreal::FProperty::VTableLayoutMap.at("GetMinAlignment")==368);
  }
  if(mode=="r3_baseline"){
   require_refused("layout.member.FFieldClass::UEP_TotalSize.measured");
   require_refused("layout.vtable.UEngine::LoadMap.measured");require_refused("layout.vtable.AGameModeBase::InitGameState.measured");
  }
  if(mode=="collect_all"){require_refused("image.sha");require_refused("settings.stats");require_refused("settings.version");}
  if(mode=="compiled_activation")assert(log_text.find("compiled-audited-townfall-l19-r4")!=std::string::npos&&log_text.find("0a09270e315de7f955b2f90514881fc9c0ac2d992d57d76dc7463a04891204bc")!=std::string::npos);
 }else assert(consumption_markers==0&&SeedArming::cleanup_callback_invocations==0);
 std::cout<<"PASS "<<mode<<" stage_refused="<<stage_refusal<<" preflight_refused="<<profile_refusal<<" consumption_markers="<<consumption_markers<<" host_maps_published="<<publish<<" external_reads="<<activation->reads<<" detail="<<stage_detail<<"\n";return 0;
}
int main(int argc,char**argv){assert(argc==4);using namespace RC::Platform::NativeLayouts;assert(GetBindingAccessorWidth("FUObjectArray","ObjFirstGCIndex")==4);
assert(GetBindingAccessorWidth("FUObjectArray","ObjLastNonGCIndex")==4);
assert(GetBindingAccessorWidth("FUObjectArray","MaxObjectsNotConsideredByGC")==4);
assert(GetBindingAccessorWidth("FUObjectArray","OpenForDisregardForGC")==4);
assert(GetBindingAccessorWidth("FUObjectArray","ObjObjects")==1);
assert(GetBindingAccessorWidth("FUObjectArray","UObjectCreateListeners")==16);
assert(GetBindingAccessorWidth("FUObjectArray","UObjectDeleteListeners")==16);
assert(GetBindingAccessorWidth("FUObjectArray","PrimarySerialNumber")==4);
assert(GetBindingAccessorWidth("FUObjectArray","bShouldRecycleObjectIndices")==1);
assert(GetBindingAccessorWidth("TUObjectArray","Objects")==8);
assert(GetBindingAccessorWidth("TUObjectArray","PreAllocatedObjects")==8);
assert(GetBindingAccessorWidth("TUObjectArray","MaxElements")==4);
assert(GetBindingAccessorWidth("TUObjectArray","NumElements")==4);
assert(GetBindingAccessorWidth("TUObjectArray","MaxChunks")==4);
assert(GetBindingAccessorWidth("TUObjectArray","NumChunks")==4);
assert(GetBindingAccessorWidth("FUObjectItem","Object")==8);
assert(GetBindingAccessorWidth("FUObjectItem","Flags")==4);
assert(GetBindingAccessorWidth("UObjectBase","NamePrivate")==8);
assert(GetBindingAccessorWidth("UObjectBase","OuterPrivate")==8);
assert(GetBindingAccessorWidth("UObjectBase","ClassPrivate")==8);
assert(GetBindingAccessorWidth("UObjectBase","InternalIndex_Private")==4);
assert(GetBindingAccessorWidth("UObjectBase","ObjectFlags")==4);
assert(GetBindingAccessorWidth("UField","Next")==8);
assert(GetBindingAccessorWidth("UStruct","SuperStruct")==8);
assert(GetBindingAccessorWidth("UStruct","Children")==8);
assert(GetBindingAccessorWidth("UStruct","ChildProperties")==8);
assert(GetBindingAccessorWidth("UStruct","PropertiesSize")==4);
assert(GetBindingAccessorWidth("UStruct","MinAlignment")==2);
assert(GetBindingAccessorWidth("UClass","ClassDefaultObject")==8);
assert(GetBindingAccessorWidth("UClass","Interfaces")==16);
assert(GetBindingAccessorWidth("UClass","ClassCastFlags")==8);
assert(GetBindingAccessorWidth("FField","ClassPrivate")==8);
assert(GetBindingAccessorWidth("FField","Owner")==16);
assert(GetBindingAccessorWidth("FField","Next")==8);
assert(GetBindingAccessorWidth("FField","NamePrivate")==8);
assert(GetBindingAccessorWidth("FFieldClass","FName")==8);
assert(GetBindingAccessorWidth("FFieldClass","SuperClass")==8);
assert(GetBindingAccessorWidth("FFieldClass","CastFlags")==8);
assert(GetBindingAccessorWidth("FProperty","ArrayDim")==4);
assert(GetBindingAccessorWidth("FProperty","ElementSize")==4);
assert(GetBindingAccessorWidth("FProperty","PropertyFlags")==8);
assert(GetBindingAccessorWidth("FProperty","Offset_Internal")==4);
assert(GetBindingAccessorWidth("UFunction","NumParms")==1);
assert(GetBindingAccessorWidth("UFunction","ParmsSize")==2);
assert(GetBindingAccessorWidth("UFunction","ReturnValueOffset")==2);
assert(GetBindingAccessorWidth("UFunction","FunctionFlags")==4);
assert(GetBindingAccessorWidth("UFunction","Func")==8);
assert(GetBindingAccessorWidth("FStructProperty","Struct")==8);
assert(GetBindingAccessorWidth("FArrayProperty","Inner")==8);
assert(GetBindingAccessorWidth("FBoolProperty","ByteOffset")==1);
assert(GetBindingAccessorWidth("FBoolProperty","ByteMask")==1);
assert(GetBindingAccessorWidth("FBoolProperty","FieldMask")==1);
assert(GetBindingAccessorWidth("FEnumProperty","UnderlyingProp")==8);
assert(GetBindingAccessorWidth("FEnumProperty","Enum")==8);
 assert(SeedArming::g_copyin_initializer_owner.enter());Ops ops{argv[1]};
 assert(SeedArming::g_code_cache.prepare(rc::codeprofile::kProfile,0x400000,0x400000,0xc0602c8,1,ops).ok);
 std::cout<<"AUTH saved-image once sha="<<SeedArming::g_code_cache.metadata.sha256<<"\n";std::cout.flush();
 std::istringstream modes(argv[2]);std::string mode;unsigned count=0;
 while(std::getline(modes,mode,',')){auto pid=fork();assert(pid>=0);if(pid==0){int rc=scenario(mode,argv[3]);std::cout.flush();_exit(rc);}int status=0;assert(waitpid(pid,&status,0)==pid);if(!WIFEXITED(status)||WEXITSTATUS(status)!=0)return 1;++count;}
 std::cout<<"MATRIX PASS count="<<count<<" native_engine_calls=0\n";
}
