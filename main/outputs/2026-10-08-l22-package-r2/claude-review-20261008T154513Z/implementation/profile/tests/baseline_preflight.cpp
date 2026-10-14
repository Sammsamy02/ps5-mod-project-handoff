
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
static void preflight(){            // L19 PREFLIGHT BEGIN: collect every independent required fact
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
                NativeNameThread::prepare_runtime(UnrealConfig,profile_image,NativeNameThread::kCandidateBindingData,rc::runtimeprofile::kDigest,now_ms,NativeNameThread::exact_import_admitted());
            }
            catch(...)
            {
                // Preflight precedes the existing consumer's cleanup owner.
                // Preserve one-shot bank retirement on this earlier refusal.
                SeedArming::bridge_copyin_initializer_complete();
                throw;
            }
}
int main(int argc,char**argv){assert(argc==2);std::string mode=argv[1];using namespace RC::Platform::NativeLayouts;
 image.ready=true;image.generation=1;image.base=0x400000;image.extent=rc::runtimeprofile::kProfile.extent;image.size=rc::runtimeprofile::kProfile.size;image.deadline_ms=60000;std::copy_n(rc::runtimeprofile::kProfile.sha256,65,image.sha256);
 staged.emplace();staged->members.push_back({"FArrayProperty::Inner",120,8});
staged->members.push_back({"FArrayProperty::UEP_TotalSize",128,8});
staged->members.push_back({"FBoolProperty::ByteMask",114,1});
staged->members.push_back({"FBoolProperty::ByteOffset",113,1});
staged->members.push_back({"FBoolProperty::FieldMask",115,1});
staged->members.push_back({"FBoolProperty::UEP_TotalSize",120,8});
staged->members.push_back({"FEnumProperty::Enum",120,8});
staged->members.push_back({"FEnumProperty::UEP_TotalSize",128,8});
staged->members.push_back({"FEnumProperty::UnderlyingProp",112,8});
staged->members.push_back({"FField::ClassPrivate",8,8});
staged->members.push_back({"FField::NamePrivate",32,8});
staged->members.push_back({"FField::Next",24,8});
staged->members.push_back({"FField::Owner",16,8});
staged->members.push_back({"FField::UEP_TotalSize",48,8});
staged->members.push_back({"FFieldClass::CastFlags",16,8});
staged->members.push_back({"FFieldClass::FName",0,8});
staged->members.push_back({"FFieldClass::SuperClass",32,8});
staged->members.push_back({"FFieldClass::UEP_TotalSize",64,8});
staged->members.push_back({"FProperty::ArrayDim",44,4});
staged->members.push_back({"FProperty::ElementSize",48,4});
staged->members.push_back({"FProperty::Offset_Internal",68,4});
staged->members.push_back({"FProperty::PropertyFlags",56,8});
staged->members.push_back({"FProperty::UEP_TotalSize",112,8});
staged->members.push_back({"FStructProperty::Struct",112,8});
staged->members.push_back({"FStructProperty::UEP_TotalSize",120,8});
staged->members.push_back({"FUObjectArray::MaxObjectsNotConsideredByGC",8,8});
staged->members.push_back({"FUObjectArray::ObjFirstGCIndex",0,8});
staged->members.push_back({"FUObjectArray::ObjLastNonGCIndex",4,8});
staged->members.push_back({"FUObjectArray::ObjObjects",16,8});
staged->members.push_back({"FUObjectArray::OpenForDisregardForGC",12,8});
staged->members.push_back({"FUObjectArray::PrimarySerialNumber",176,8});
staged->members.push_back({"FUObjectArray::UEP_TotalSize",184,8});
staged->members.push_back({"FUObjectArray::UObjectCreateListeners",104,8});
staged->members.push_back({"FUObjectArray::UObjectDeleteListeners",120,8});
staged->members.push_back({"FUObjectArray::bShouldRecycleObjectIndices",180,8});
staged->members.push_back({"FUObjectItem::Flags",8,4});
staged->members.push_back({"FUObjectItem::Object",0,8});
staged->members.push_back({"FUObjectItem::UEP_TotalSize",24,8});
staged->members.push_back({"TUObjectArray::MaxChunks",24,8});
staged->members.push_back({"TUObjectArray::MaxElements",16,8});
staged->members.push_back({"TUObjectArray::NumChunks",28,8});
staged->members.push_back({"TUObjectArray::NumElements",20,8});
staged->members.push_back({"TUObjectArray::Objects",0,8});
staged->members.push_back({"TUObjectArray::PreAllocatedObjects",8,8});
staged->members.push_back({"TUObjectArray::UEP_TotalSize",32,8});
staged->members.push_back({"UClass::ClassCastFlags",216,8});
staged->members.push_back({"UClass::ClassDefaultObject",272,8});
staged->members.push_back({"UClass::Interfaces",472,16});
staged->members.push_back({"UClass::UEP_TotalSize",512,8});
staged->members.push_back({"UField::Next",40,8});
staged->members.push_back({"UField::UEP_TotalSize",48,8});
staged->members.push_back({"UFunction::Func",216,8});
staged->members.push_back({"UFunction::FunctionFlags",176,4});
staged->members.push_back({"UFunction::NumParms",180,1});
staged->members.push_back({"UFunction::ParmsSize",182,2});
staged->members.push_back({"UFunction::ReturnValueOffset",184,2});
staged->members.push_back({"UFunction::UEP_TotalSize",224,8});
staged->members.push_back({"UObjectBase::ClassPrivate",16,8});
staged->members.push_back({"UObjectBase::InternalIndex_Private",12,4});
staged->members.push_back({"UObjectBase::NamePrivate",24,8});
staged->members.push_back({"UObjectBase::ObjectFlags",8,4});
staged->members.push_back({"UObjectBase::OuterPrivate",32,8});
staged->members.push_back({"UObjectBase::UEP_TotalSize",40,8});
staged->members.push_back({"UStruct::ChildProperties",80,8});
staged->members.push_back({"UStruct::Children",72,8});
staged->members.push_back({"UStruct::MinAlignment",92,2});
staged->members.push_back({"UStruct::PropertiesSize",88,4});
staged->members.push_back({"UStruct::SuperStruct",64,8});
staged->members.push_back({"UStruct::UEP_TotalSize",176,8});
staged->vtables.push_back({"AGameModeBase::InitGameState",1976,8});
staged->vtables.push_back({"FProperty::CopyValuesInternal",248,8});
staged->vtables.push_back({"FProperty::DestroyValueInternal",320,8});
staged->vtables.push_back({"FProperty::GetMinAlignment",368,8});
staged->vtables.push_back({"UEngine::LoadMap",1272,8});
staged->vtables.push_back({"UEngine::Tick",784,8});
staged->vtables.push_back({"UObject::ProcessEvent",616,8});
staged->vtables.push_back({"FMalloc::Malloc",48,8});
staged->vtables.push_back({"FMalloc::TryMalloc",56,8});
staged->vtables.push_back({"FMalloc::Realloc",64,8});
staged->vtables.push_back({"FMalloc::TryRealloc",72,8});
staged->vtables.push_back({"FMalloc::Free",80,8});
staged->vtables.push_back({"FMalloc::QuantizeSize",104,8});
staged->vtables.push_back({"FMalloc::GetAllocationSize",112,8});
staged->vtables.push_back({"FMalloc::Trim",120,8});
staged->vtables.push_back({"FMalloc::SetupTLSCachesOnCurrentThread",128,8});
staged->vtables.push_back({"FMalloc::ClearAndDisableTLSCachesOnCurrentThread",152,8});
staged->vtables.push_back({"FMalloc::InitializeStatsMetadata",160,8});
staged->vtables.push_back({"FMalloc::UpdateStats",168,8});
staged->vtables.push_back({"FMalloc::GetAllocatorStats",176,8});
staged->vtables.push_back({"FMalloc::DumpAllocatorStats",184,8});
staged->vtables.push_back({"FMalloc::IsInternallyThreadSafe",192,8});
staged->vtables.push_back({"FMalloc::ValidateHeap",200,8});
staged->vtables.push_back({"FMalloc::GetDescriptiveName",208,8});
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
