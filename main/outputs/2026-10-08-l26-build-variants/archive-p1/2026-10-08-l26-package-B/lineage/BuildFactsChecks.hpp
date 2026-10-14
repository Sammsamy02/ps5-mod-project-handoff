#pragma once
#include "CodeImageCache.hpp"
#include <string>
#include <vector>
// Pure generic facts consumer. Image authentication is owned by the startup
// code cache; this consumer never reads game memory or supplies layout values.
namespace rc::buildfacts {
struct Profile {
 uint32_t schema;const char* name;const char* digest;const char* sha256;uint64_t size,extent,base;
 uint32_t format,pointer_size,major,minor;const char* target;const char* engine_revision;const char* build_id;
 uint32_t build_configuration,stats;const char* build_source;const char* stats_source;
 bool fname_case_known,fname_case_preserving;uint32_t fname_bytes,fname_alignment;
};
struct Declared {
 uint32_t format=0,pointer_size=0,major=0,minor=0;bool non_shipping=false,stats=false;
 std::string target,engine_revision,build_id;
};
// Zero axes mean Detect; version Detect is explicit so minor zero retains
// its valid configured meaning. Supported explicit axes use the
// same numeric meanings as independent PsScanResults fields.
struct Settings {int build_configuration=0,stats=0;uint32_t major=0,minor=0;bool fname_case_preserving=false;uint32_t fname_bytes=0,fname_alignment=0;bool version_detect=false;};
struct Check {std::string name;bool ok;std::string detail;};
struct Result {bool ok=true;uint32_t build_configuration=0,stats=0;std::vector<Check> checks;};
inline bool text(const char*p){return p&&*p;}
inline bool same(const std::string&s,const char*p){return p&&s==p;}
inline Result evaluate_profile(const Profile*p,const codecache::Metadata*m,uint64_t actual_base,const Declared*d,const Settings&s,uint64_t now_ms){
 Result r;auto add=[&](const char*n,bool ok,const char*detail){r.checks.push_back({n,ok,detail});r.ok=r.ok&&ok;};
 add("profile.present",p!=nullptr,"authenticated per-game build profile required");
 add("profile.schema",p&&p->schema==1,"supported typed schema required");
 add("profile.identity",p&&text(p->name)&&codecache::hex_pin(p->digest),"name and evidence digest required");
 add("profile.image-sha",p&&codecache::hex_pin(p->sha256),"complete SHA256 required");
 add("image.authenticated",m&&m->ready&&m->generation!=0,"fresh startup whole-image authentication required");
 add("image.sha",p&&m&&codecache::hex_pin(p->sha256)&&codehash::sha256_hex_equal(m->sha256,p->sha256),"authenticated image SHA must equal profile");
 add("image.size",p&&m&&p->size>=64&&m->size==p->size,"whole file size must equal profile");
 add("image.extent",p&&m&&p->extent!=0&&m->extent==p->extent,"authenticated PT_LOAD extent must equal profile");
 add("image.base",p&&m&&actual_base!=0&&actual_base==p->base&&m->base==actual_base,"actual startup module base must equal profile");
 add("image.deadline",m&&m->deadline_ms>now_ms,"original shared startup deadline remains authoritative");
 add("profile.build-configuration",p&&p->build_configuration>=1&&p->build_configuration<=3&&text(p->build_source),"independent measured build configuration required");
 add("profile.stats",p&&(p->stats==1||p->stats==2)&&text(p->stats_source),"independent measured STATS fact required");
 add("profile.version",p&&p->major>=4&&text(p->engine_revision)&&text(p->build_id),"measured version/revision/link identity required");
 add("profile.layout-abi",p&&p->format==1&&p->pointer_size==8&&text(p->target),"declared native format/target/pointer ABI required");
 add("layout.staged",d!=nullptr,"complete parsed staged native pair required");
 add("layout.format",p&&d&&d->format==p->format,"staged format must equal profile");
 add("layout.target",p&&d&&same(d->target,p->target),"staged target must equal profile");
 add("layout.pointer-size",p&&d&&d->pointer_size==p->pointer_size,"staged pointer size must equal profile");
 add("layout.version",p&&d&&d->major==p->major&&d->minor==p->minor,"staged engine version must equal measured profile");
 add("layout.revision",p&&d&&same(d->engine_revision,p->engine_revision),"staged engine revision must equal profile");
 add("layout.build-id",p&&d&&same(d->build_id,p->build_id),"staged ELF link identity must equal profile");
 add("layout.non-shipping",p&&d&&p->build_configuration>=1&&p->build_configuration<=3&&d->non_shipping==(p->build_configuration!=1),"staged NonShipping must equal measured configuration");
 add("layout.stats",p&&d&&(p->stats==1||p->stats==2)&&d->stats==(p->stats==2),"staged Stats must equal independently measured STATS");
 add("settings.build-configuration",p&&s.build_configuration>=0&&s.build_configuration<=3&&(s.build_configuration==0||uint32_t(s.build_configuration)==p->build_configuration),"Detect or matching explicit configuration required");
 add("settings.stats",p&&s.stats>=0&&s.stats<=2&&(s.stats==0||uint32_t(s.stats)==p->stats),"Detect or matching explicit STATS required");
 add("settings.version",p&&(s.version_detect||(s.major==p->major&&s.minor==p->minor)),"explicit version components must equal measured profile");
 add("profile.fname-case",p&&p->fname_case_known,"independent case-preserving ABI fact required");
 add("settings.fname-case",p&&p->fname_case_known&&s.fname_case_preserving==p->fname_case_preserving,"selected FName ABI must equal profile");
 add("settings.fname-shape",p&&p->fname_bytes!=0&&p->fname_alignment!=0&&s.fname_bytes==p->fname_bytes&&s.fname_alignment==p->fname_alignment,"selected FName output size/alignment must equal profile");
 if(r.ok){r.build_configuration=p->build_configuration;r.stats=p->stats;}
 return r;
}
struct LayoutPin {const char* name;bool known;uint32_t offset,width;const char* source;};
template<class Collection>
inline void require_layout(Result&r,const char*kind,const LayoutPin&pin,const Collection*available){
 bool found=false,agree=false,width_agree=pin.width==0;if(available)for(const auto&entry:*available)if(entry.name==pin.name){found=true;agree=entry.offset==pin.offset;width_agree=pin.width==0||entry.width>=pin.width;break;}
 const auto stem=std::string("layout.")+kind+"."+pin.name;
 r.checks.push_back({stem+".present",found,"required by selected runtime startup/menu/spawn source graph"});
 r.checks.push_back({stem+".measured",pin.known&&text(pin.source),"authenticated per-game value/provenance required; staged values are declarations"});
 r.checks.push_back({stem+".value",pin.known&&found&&agree,"expected offset="+std::to_string(pin.offset)+" source="+(pin.source?pin.source:"unknown")});
 r.checks.push_back({stem+".width",width_agree,"binding validator width must cover measured field width when independently pinned; no active-access proof"});
 r.ok=r.ok&&found&&pin.known&&text(pin.source)&&agree&&width_agree;
}
inline void require_static_fact(Result&r,const char*name,bool known,const char*detail){r.checks.push_back({std::string("profile.static.")+name,known,detail});r.ok=r.ok&&known;}
}
