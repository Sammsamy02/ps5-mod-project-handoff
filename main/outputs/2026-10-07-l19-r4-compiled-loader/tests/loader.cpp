
#include <NativeLayoutEngineIncludes.hpp>
#if __has_include("NativeLayoutDeclarations.hpp")
#include "NativeLayoutDeclarations.hpp"
#endif
#include <map>
#include <fstream>

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
int main(int argc,char**argv){assert(argc==4);std::string mode=argv[1];
using namespace RC::Platform::NativeLayouts;
RC::UE4SSProgram program;program.m_working_directory=argv[3];
auto activation=std::make_shared<RC::Activation>();
activation->files={{"ue4ss/MemberVariableLayout.ini","[NativeLayout]\nFormat=1\nTarget=prospero-x86_64-sysv\nPointerSize=8\nEngineMajor=5\nEngineMinor=6\nNonShipping=0\nStats=0\nEngineRevision=5.6.1-0+++UE5\nBuildId=04bb4330a7d71dcefe1cc2c4d19769d5\n[UObjectBase]\nUEP_TotalSize=0x28\nNamePrivate=0x18\n"},{"ue4ss/VTableLayout.ini","[NativeLayout]\nFormat=1\nTarget=prospero-x86_64-sysv\nPointerSize=8\nEngineMajor=5\nEngineMinor=6\nNonShipping=0\nStats=0\nEngineRevision=5.6.1-0+++UE5\nBuildId=04bb4330a7d71dcefe1cc2c4d19769d5\n[UObject]\nProcessEvent=0x268\n"}};
program.m_native_context.mod_source=activation;
bool compiled=mode!="external_activation"&&mode!="external_files";
if(compiled)activation->poison=true;
if(mode=="compiled_files"||mode=="external_files")program.m_native_context.mod_source.reset();
if(mode=="external_files"){
 std::filesystem::create_directories(program.m_working_directory);
 std::ofstream(program.m_working_directory/"MemberVariableLayout.ini")<<"[NativeLayout]\nFormat=1\nTarget=prospero-x86_64-sysv\nPointerSize=8\nEngineMajor=5\nEngineMinor=6\nNonShipping=0\nStats=0\nEngineRevision=5.6.1-0+++UE5\nBuildId=04bb4330a7d71dcefe1cc2c4d19769d5\n[UObjectBase]\nUEP_TotalSize=0x28\nNamePrivate=0x18\n";
 std::ofstream(program.m_working_directory/"VTableLayout.ini")<<"[NativeLayout]\nFormat=1\nTarget=prospero-x86_64-sysv\nPointerSize=8\nEngineMajor=5\nEngineMinor=6\nNonShipping=0\nStats=0\nEngineRevision=5.6.1-0+++UE5\nBuildId=04bb4330a7d71dcefe1cc2c4d19769d5\n[UObject]\nProcessEvent=0x268\n";
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
if(compiled)assert(log_text.find("compiled-authenticated-r3-test")!=std::string::npos&&log_text.find("efa1c15e6da091298d9764b9d0fa18b1196d8825776b383799806ef0397329fd")!=std::string::npos);
if(mode=="existing_member"){assert(RC::Unreal::UObjectBase::MemberOffsets.size()==1&&RC::Unreal::UObjectBase::MemberOffsets.at("cached")==99);RC::Unreal::UObjectBase::MemberOffsets.clear();}
if(mode=="existing_bits"){assert(RC::Unreal::UObjectBase::BitfieldInfos.size()==1&&RC::Unreal::UObjectBase::BitfieldInfos.at("cached").position==1);RC::Unreal::UObjectBase::BitfieldInfos.clear();}
if(mode=="existing_virtual"){assert(RC::Unreal::UObject::VTableLayoutMap.size()==1&&RC::Unreal::UObject::VTableLayoutMap.at("cached")==123);RC::Unreal::UObject::VTableLayoutMap.clear();}
assert(RC::Unreal::FArrayProperty::MemberOffsets.empty()&&RC::Unreal::FArrayProperty::BitfieldInfos.empty());
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
 assert(RC::Unreal::FArrayProperty::MemberOffsets.empty()&&RC::Unreal::FArrayProperty::BitfieldInfos.empty());
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
std::cout<<"PASS "<<mode<<" staged="<<bool(staged)<<" external_reads="<<activation->reads<<" maps_unchanged=1"<<"\n";
}
