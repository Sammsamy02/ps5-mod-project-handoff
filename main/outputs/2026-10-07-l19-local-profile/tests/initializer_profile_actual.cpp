
#include "RuntimeBuildProfile.hpp"
#include "CodeSignatureProfile.hpp"
#include "CopyinStartup.hpp"
#include "NativeLayoutBinding.hpp"
#include <chrono>
#include <memory>
#include <sstream>
#include <iostream>
#include <cassert>
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
namespace RC::Platform::NativeLayouts {
struct Prepared {Identity identity;std::vector<StagedLayoutFact> member_facts,virtual_facts;};
static std::mutex BindingMutex;static std::unique_ptr<Prepared>Staged;
auto GetStagedNativeLayoutFacts() -> std::optional<StagedFacts> {
    std::lock_guard lock{BindingMutex};
    if (!Staged) return std::nullopt;
    return StagedFacts{Staged->identity,Staged->member_facts,Staged->virtual_facts};
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
