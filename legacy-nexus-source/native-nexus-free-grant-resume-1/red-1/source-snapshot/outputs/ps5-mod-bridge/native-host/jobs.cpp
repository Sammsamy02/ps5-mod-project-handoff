#include "jobs.h"
#include "download/curl_download.h"
#ifndef MB_HOST_PAK_PREPARATION
#define MB_HOST_PAK_PREPARATION 0
#endif
#if MB_HOST_PAK_PREPARATION
#include "format/private_pak_job.hpp"
#include "iostore/private_iostore_job.hpp"
#endif
extern "C" {
#include "archive/private/fd_io.h"
#include "archive/private/posix_compat.h"
}
#include <nlohmann/json.hpp>
#include <picosha2/picosha2.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <stdexcept>
#include <sys/stat.h>
#include <time.h>
#include <type_traits>
#include <unistd.h>
#include <vector>

namespace {
using Status=mb_nexus_jobs_status_v1;
using Json=nlohmann::json;
constexpr uint64_t magic=UINT64_C(0x4d424a4f42533101);
void wipe(void *p,size_t n) noexcept {
    auto *bytes=static_cast<volatile unsigned char *>(p);
    for(size_t i=0;i<n;++i)bytes[i]=0;
}
struct Range { uintptr_t begin,end; };
bool range(const void *p,size_t n,Range& out) noexcept {
    auto a=reinterpret_cast<uintptr_t>(p);
    if((!p&&n)||n>UINTPTR_MAX-a)return false;
    out={a,a+n};return true;
}
bool overlaps(Range a,Range b) noexcept {
    return a.begin!=a.end&&b.begin!=b.end&&a.begin<b.end&&b.begin<a.end;
}
struct Regions {
    std::array<Range,12> spans{};size_t count=0;
    bool add(const void *p,size_t n) noexcept {
        if(count==spans.size()||!range(p,n,spans[count]))return false;
        ++count;return true;
    }
    bool distinct() const noexcept {
        for(size_t i=0;i<count;++i)for(size_t j=i+1;j<count;++j)
            if(overlaps(spans[i],spans[j]))return false;
        return true;
    }
    bool outside(const void *p,size_t n) const noexcept {
        Range r{};if(!range(p,n,r))return false;
        for(size_t i=0;i<count;++i)if(overlaps(r,spans[i]))return false;
        return true;
    }
};
template<class T> bool aligned(const T *p) noexcept {
    return p&&reinterpret_cast<uintptr_t>(p)%alignof(T)==0;
}
struct Output {
    char *p;size_t n;bool published=false;
    ~Output(){if(!published)wipe(p,n);}
};
struct State {
    enum Value { Queued,Downloading,Analyzing,Analyzed,NeedsAuthorization,Failed } value=Queued;
};
enum class Error { None,Authorization,Account,Hidden,Resolution,Download,Analysis,Storage,Internal };
struct Record {
    char id[33]{},domain[81]{},key[81]{};
    size_t domain_bytes=0,key_bytes=0;
    uint64_t mod_id=0,file_id=0,bytes=0;
    std::array<uint8_t,32> owner{},sha{};
    char cookie[44]{},csrf[44]{},origin[129]{};
    size_t origin_bytes=0;
    State state{};Error error=Error::None;
    bool downloaded=false,leaf_owned=false,identity_valid=false;
    uint64_t device=0,inode=0;
    int fd=-1;
    char report[MB_ARCHIVE_ANALYSIS_OUTPUT_LIMIT]{};
    size_t report_bytes=0;
    uint32_t kind=0;
    char preparation_report[4096]{};
    size_t preparation_bytes=0;
};
struct Intent { char domain[81]{},key[81]{};size_t domain_bytes=0,key_bytes=0;uint64_t mod=0,file=0;uint32_t kind=0; };
struct GenerationIntent {
    char ids[MB_NEXUS_JOBS_LIMIT][33]{},key[81]{};
    uint32_t source_count=0;size_t key_bytes=0;
};
struct Generation {
    enum State { Queued,Prepared,Conflict,Refused } state=Queued;
    char id[33]{},key[81]{},game[81]{},reason[64]{},report[4096]{};
    size_t key_bytes=0,game_bytes=0,report_bytes=0;
    std::array<uint8_t,32> owner{},base_token{};
    char source_ids[MB_NEXUS_JOBS_LIMIT][33]{};
    uint32_t source_indices[MB_NEXUS_JOBS_LIMIT]{},source_count=0;
    uint64_t source_bytes[MB_NEXUS_JOBS_LIMIT]{};
    std::array<std::array<uint8_t,32>,MB_NEXUS_JOBS_LIMIT> source_sha{};
};
struct AcquisitionChoice { uint64_t mod=0,file=0; };
struct AcquisitionIntent {
    char game[81]{},key[81]{};
    size_t game_bytes=0,key_bytes=0;
    uint64_t roots[MB_NEXUS_JOBS_LIMIT]{};
    AcquisitionChoice choices[MB_NEXUS_JOBS_LIMIT]{};
    uint32_t root_count=0,choice_count=0;
    bool primary=false;
};
struct Acquisition {
    AcquisitionIntent intent{};
    std::array<uint8_t,32> owner{};
    char ids[MB_NEXUS_JOBS_LIMIT][33]{};
    AcquisitionChoice selected[MB_NEXUS_JOBS_LIMIT]{};
    uint32_t source_count=0;
};
struct AcquisitionStage {
    std::array<Record,MB_NEXUS_JOBS_LIMIT> records{};
    Acquisition group{};
};
static_assert(std::is_trivially_copyable_v<Record> && std::is_trivially_copyable_v<Acquisition>);
struct AcquisitionDelete {
    void operator()(AcquisitionStage *p) const noexcept {if(p){wipe(p,sizeof(*p));delete p;}}
};
struct PlanScratch {
    std::array<char,MB_NEXUS_AUTH_ACQUISITION_OUTPUT_LIMIT> bytes{};
    ~PlanScratch(){wipe(bytes.data(),bytes.size());}
};
bool alnum(unsigned char c) noexcept {
    return(c>='a'&&c<='z')||(c>='0'&&c<='9');
}
bool domain(std::string_view s) noexcept {
    if(s.empty()||s.size()>80||!alnum(s.front()))return false;
    for(unsigned char c:s)if(!alnum(c)&&c!='-'&&c!='_')return false;
    return true;
}
bool key(std::string_view s) noexcept {
    if(s.size()<8||s.size()>80)return false;
    for(unsigned char c:s)if(!alnum(c)&&!(c>='A'&&c<='Z')&&c!='-'&&c!='_')return false;
    return true;
}
bool token(const char *p,size_t n) noexcept {
    if(n!=43)return false;
    for(size_t i=0;i<n;++i) {
        auto c=static_cast<unsigned char>(p[i]);
        if(!alnum(c)&&!(c>='A'&&c<='Z')&&c!='-'&&c!='_')return false;
    }
    return true;
}
bool identifier(const char *p,size_t n) noexcept {
    if(n!=32)return false;
    for(size_t i=0;i<n;++i)if(!((p[i]>='0'&&p[i]<='9')||(p[i]>='a'&&p[i]<='f')))return false;
    return true;
}
std::array<uint8_t,32> fingerprint(const char *p,size_t n) {
    std::array<uint8_t,32> out{};
    picosha2::hash256(p,p+n,out.begin(),out.end());return out;
}
bool equal(const std::array<uint8_t,32>& a,const std::array<uint8_t,32>& b) noexcept {
    unsigned difference=0;for(size_t i=0;i<a.size();++i)difference|=a[i]^b[i];
    return difference==0;
}
std::string hex(const uint8_t *p,size_t n) {
    static constexpr char digits[]="0123456789abcdef";
    std::string text(n*2,'0');
    for(size_t i=0;i<n;++i){text[2*i]=digits[p[i]>>4];text[2*i+1]=digits[p[i]&15];}
    return text;
}
// The accepted intent is a flat canonical subset, so depth, duplicate keys,
// noncanonical numbers and escaped aliases are rejected before any admission.
class IntentParser {
    std::string_view text;size_t position=0;
    void space() noexcept {while(position<text.size()&&(text[position]==' '||text[position]=='\t'||text[position]=='\r'||text[position]=='\n'))++position;}
    bool take(char c) noexcept {space();if(position==text.size()||text[position]!=c)return false;++position;return true;}
    bool string(std::string_view& out) noexcept {
        space();if(position==text.size()||text[position++]!='"')return false;
        auto start=position;
        while(position<text.size()&&text[position]!='"') {
            auto c=static_cast<unsigned char>(text[position++]);
            if(c<32||c>126||c=='\\')return false;
        }
        if(position==text.size())return false;
        out=text.substr(start,position-start);++position;return true;
    }
    bool number(uint64_t& out) noexcept {
        space();if(position==text.size()||text[position]<'1'||text[position]>'9')return false;
        out=0;
        while(position<text.size()&&text[position]>='0'&&text[position]<='9') {
            auto d=static_cast<uint64_t>(text[position++]-'0');
            if(out>(static_cast<uint64_t>(INT64_MAX)-d)/10)return false;
            out=out*10+d;
        }
        return true;
    }
public:
    explicit IntentParser(std::string_view s):text(s){}
    Status parse_acquisition(AcquisitionIntent& out) noexcept {
        if(!take('{'))return MB_NEXUS_JOBS_BAD_ARGUMENT;
        unsigned seen=0;bool closed=false;
        for(unsigned fields=0;fields<5;++fields) {
            std::string_view name,value;
            if(!string(name)||!take(':'))return MB_NEXUS_JOBS_BAD_ARGUMENT;
            unsigned bit=name=="game"?1u:name=="mod_ids"?2u:name=="file_choices"?4u:
                         name=="file_policy"?8u:name=="idempotency_key"?16u:0u;
            if(!bit)return MB_NEXUS_JOBS_REFUSED;
            if(seen&bit)return MB_NEXUS_JOBS_BAD_ARGUMENT;
            seen|=bit;
            if(bit==2) {
                if(!take('[')||take(']'))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                bool ended=false;
                for(uint32_t i=0;i<MB_NEXUS_JOBS_LIMIT;++i) {
                    uint64_t id=0;if(!number(id)||id>INT32_MAX)return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    for(uint32_t j=0;j<i;++j)if(out.roots[j]==id)return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    out.roots[i]=id;++out.root_count;
                    if(take(']')){ended=true;break;}
                    if(!take(','))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                }
                if(!ended)return MB_NEXUS_JOBS_BAD_ARGUMENT;
            }else if(bit==4) {
                if(!take('['))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                bool ended=take(']');
                for(uint32_t i=0;!ended&&i<MB_NEXUS_JOBS_LIMIT;++i) {
                    if(!take('{'))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    unsigned pair_seen=0;bool pair_closed=false;AcquisitionChoice choice{};
                    for(unsigned part=0;part<2;++part) {
                        uint64_t id=0;
                        if(!string(name)||!take(':')||!number(id))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                        unsigned pair=name=="mod_id"?1u:name=="file_id"?2u:0u;
                        if(!pair||pair_seen&pair)return MB_NEXUS_JOBS_BAD_ARGUMENT;
                        pair_seen|=pair;
                        if(pair==1){if(id>INT32_MAX)return MB_NEXUS_JOBS_BAD_ARGUMENT;choice.mod=id;}
                        else choice.file=id;
                        if(take('}')){pair_closed=true;break;}
                        if(!take(','))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    }
                    if(!pair_closed||pair_seen!=3)return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    for(uint32_t j=0;j<i;++j)if(out.choices[j].mod==choice.mod)return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    out.choices[i]=choice;++out.choice_count;
                    if(take(']')){ended=true;break;}
                    if(!take(','))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                }
                if(!ended)return MB_NEXUS_JOBS_BAD_ARGUMENT;
            }else {
                if(!string(value))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                if(bit==1) {
                    if(!domain(value))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    std::memcpy(out.game,value.data(),value.size());out.game_bytes=value.size();
                }else if(bit==8) {
                    if(value!="explicit_only"&&value!="unique_current_primary")return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    out.primary=value=="unique_current_primary";
                }else {
                    if(!key(value))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    std::memcpy(out.key,value.data(),value.size());out.key_bytes=value.size();
                }
            }
            if(take('}')){closed=true;break;}
            if(!take(','))return MB_NEXUS_JOBS_BAD_ARGUMENT;
        }
        space();
        if(!closed||seen!=31||position!=text.size())return MB_NEXUS_JOBS_BAD_ARGUMENT;
        std::sort(out.roots,out.roots+out.root_count);
        std::sort(out.choices,out.choices+out.choice_count,[](auto a,auto b){return a.mod<b.mod;});
        return MB_NEXUS_JOBS_OK;
    }
    Status parse_generation(GenerationIntent& out) noexcept {
        if(!take('{'))return MB_NEXUS_JOBS_BAD_ARGUMENT;
        unsigned seen=0;bool closed=false;
        for(unsigned fields=0;fields<3;++fields) {
            std::string_view name,value;
            if(!string(name)||!take(':'))return MB_NEXUS_JOBS_BAD_ARGUMENT;
            unsigned bit=name=="kind"?1u:name=="source_job_ids"?2u:name=="idempotency_key"?4u:0u;
            if(!bit)return MB_NEXUS_JOBS_REFUSED;
            if(seen&bit)return MB_NEXUS_JOBS_BAD_ARGUMENT;
            seen|=bit;
            if(bit==2) {
                if(!take('[')||take(']'))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                bool ended=false;
                for(uint32_t i=0;i<MB_NEXUS_JOBS_LIMIT;++i) {
                    if(!string(value)||!identifier(value.data(),value.size()))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    for(uint32_t j=0;j<i;++j)if(!std::memcmp(out.ids[j],value.data(),32))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    std::memcpy(out.ids[i],value.data(),32);++out.source_count;
                    if(take(']')){ended=true;break;}
                    if(!take(','))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                }
                if(!ended)return MB_NEXUS_JOBS_BAD_ARGUMENT;
            }else {
                if(!string(value))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                if(bit==1&&value!="prepare_iostore_generation")return MB_NEXUS_JOBS_REFUSED;
                if(bit==4) {
                    if(!key(value))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    std::memcpy(out.key,value.data(),value.size());out.key_bytes=value.size();
                }
            }
            if(take('}')){closed=true;break;}
            if(!take(','))return MB_NEXUS_JOBS_BAD_ARGUMENT;
        }
        space();return closed&&seen==7&&position==text.size()?MB_NEXUS_JOBS_OK:MB_NEXUS_JOBS_BAD_ARGUMENT;
    }
    Status parse(Intent& out) noexcept {
        if(!take('{'))return MB_NEXUS_JOBS_BAD_ARGUMENT;
        unsigned seen=0;bool closed=false;
        for(unsigned fields=0;fields<5;++fields) {
            std::string_view name,value;
            if(!string(name)||!take(':'))return MB_NEXUS_JOBS_BAD_ARGUMENT;
            unsigned bit=name=="kind"?1u:name=="game"?2u:name=="mod_id"?4u:name=="file_id"?8u:name=="idempotency_key"?16u:0u;
            if(!bit)return MB_NEXUS_JOBS_REFUSED;
            if(seen&bit)return MB_NEXUS_JOBS_BAD_ARGUMENT;
            seen|=bit;
            if(bit==4||bit==8) {
                uint64_t id=0;if(!number(id))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                (bit==4?out.mod:out.file)=id;
            } else {
                if(!string(value))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                if(bit==1) {
                    if(value=="download_analyze")out.kind=0;
                    else if(value=="download_prepare_pak")out.kind=1;
                    else if(value=="download_prepare_pak_component")out.kind=2;
                    else if(value=="download_prepare_ini")out.kind=3;
                    else if(value=="download_prepare")out.kind=4;
                    else if(value=="download_prepare_iostore")out.kind=5;
                    else return MB_NEXUS_JOBS_REFUSED;
                }
                if(bit==2) {
                    if(!domain(value))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    std::memcpy(out.domain,value.data(),value.size());out.domain_bytes=value.size();
                }
                if(bit==16) {
                    if(!key(value))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    std::memcpy(out.key,value.data(),value.size());out.key_bytes=value.size();
                }
            }
            space();if(position<text.size()&&text[position]=='}'){++position;closed=true;break;}
            if(!take(','))return MB_NEXUS_JOBS_BAD_ARGUMENT;
        }
        space();return closed&&seen==31&&position==text.size()?MB_NEXUS_JOBS_OK:MB_NEXUS_JOBS_BAD_ARGUMENT;
    }
};
void bindings_clear(Record& record) noexcept {
    wipe(record.cookie,sizeof(record.cookie));wipe(record.csrf,sizeof(record.csrf));
    wipe(record.origin,sizeof(record.origin));record.origin_bytes=0;
}
const char *state_name(State::Value state) noexcept {
    switch(state) {
        case State::Queued:return "queued";case State::Downloading:return "downloading";
        case State::Analyzing:return "analyzing";case State::Analyzed:return "analyzed";
        case State::NeedsAuthorization:return "needs_nexus_authorization";case State::Failed:return "failed";
    }
    return "failed";
}
const char *error_code(Error error) noexcept {
    switch(error) {
        case Error::Authorization:return "download_authorization";case Error::Account:return "reconnect_required";
        case Error::Hidden:return "content_hidden";case Error::Resolution:return "resolution_failed";
        case Error::Download:return "download_failed";case Error::Analysis:return "analysis_failed";
        case Error::Storage:return "storage_unavailable";case Error::Internal:return "operation_failed";
        case Error::None:return "operation_failed";
    }
    return "operation_failed";
}
const char *error_message(Error error) noexcept {
    switch(error) {
        case Error::Authorization:return "Authorize this file through the Nexus website.";
        case Error::Account:return "Reconnect your Nexus account.";case Error::Hidden:return "This file is unavailable to this account.";
        case Error::Resolution:return "The selected file could not be resolved.";case Error::Download:return "The archive download failed.";
        case Error::Analysis:return "The archive could not be analyzed.";case Error::Storage:return "Private job storage is unavailable.";
        case Error::Internal:case Error::None:return "The operation failed.";
    }
    return "The operation failed.";
}
const char *kind_name(uint32_t kind) noexcept {
    switch(kind) {
        case 1:return "download_prepare_pak";case 2:return "download_prepare_pak_component";
        case 3:return "download_prepare_ini";case 4:return "download_prepare";
        case 5:return "download_prepare_iostore";
        default:return "download_analyze";
    }
}
Json public_job(const Record& record) {
    auto mod=std::to_string(record.mod_id),file=std::to_string(record.file_id);
    Json out={{"id",record.id},{"kind",kind_name(record.kind)},{"game",record.domain},{"mod_id",record.mod_id},{"file_id",record.file_id},
        {"label","Mod "+mod+" \xc2\xb7 file "+file},{"status",state_name(record.state.value)},
        {"stage",record.state.value==State::Analyzed?"analyzed":record.downloaded?"downloaded":"requested"},
        {"compatibility","unresolved"},{"source_url","https://www.nexusmods.com/"+std::string(record.domain)+"/mods/"+mod+"?tab=files&file_id="+file},
        {"runtime_supported",false},{"installation_supported",false}};
    if(record.downloaded){out["sha256"]=hex(record.sha.data(),record.sha.size());out["bytes_downloaded"]=record.bytes;}
    if(record.error!=Error::None)out["error"]={{"code",error_code(record.error)},{"message",error_message(record.error)}};
    if(record.state.value==State::Analyzed)out["analysis_report"]=Json::parse(record.report,record.report+record.report_bytes);
    if(record.state.value==State::Analyzed&&record.preparation_bytes)out["preparation"]=Json::parse(record.preparation_report,record.preparation_report+record.preparation_bytes);
    return out;
}
Json public_generation(const Generation& generation) {
    const char *state=generation.state==Generation::Queued?"queued":generation.state==Generation::Prepared?"prepared":
        generation.state==Generation::Conflict?"conflict":"refused";
    Json ids=Json::array();for(uint32_t i=0;i<generation.source_count;++i)ids.push_back(generation.source_ids[i]);
    Json value={{"id",generation.id},{"kind","prepare_iostore_generation"},{"status",state},{"game",generation.game},
        {"source_job_ids",std::move(ids)},{"compatibility","unresolved"},{"runtime_supported",false},{"installation_supported",false}};
    if(generation.reason[0])value["reason"]=generation.reason;
    if(generation.report_bytes)value["preparation"]=Json::parse(generation.report,generation.report+generation.report_bytes);
    return value;
}
void scrub_json(Json& value) noexcept {
    if(value.is_string()){auto& text=value.get_ref<std::string&>();wipe(text.data(),text.size());}
    else if(value.is_structured())for(auto& child:value)scrub_json(child);
}
struct JsonErase { Json& value;~JsonErase(){scrub_json(value);} };
struct TextErase { std::string& value;~TextErase(){wipe(value.data(),value.size());} };
bool same_acquisition(const AcquisitionIntent& a,const AcquisitionIntent& b) noexcept {
    if(a.game_bytes!=b.game_bytes||std::memcmp(a.game,b.game,a.game_bytes)||a.primary!=b.primary||
       a.root_count!=b.root_count||a.choice_count!=b.choice_count)return false;
    for(uint32_t i=0;i<a.root_count;++i)if(a.roots[i]!=b.roots[i])return false;
    for(uint32_t i=0;i<a.choice_count;++i)if(a.choices[i].mod!=b.choices[i].mod||a.choices[i].file!=b.choices[i].file)return false;
    return true;
}
Json acquisition_intent(const AcquisitionIntent& intent) {
    Json roots=Json::array(),choices=Json::array();
    for(uint32_t i=0;i<intent.root_count;++i)roots.push_back(intent.roots[i]);
    for(uint32_t i=0;i<intent.choice_count;++i)choices.push_back({{"mod_id",intent.choices[i].mod},{"file_id",intent.choices[i].file}});
    return {{"game",intent.game},{"mod_ids",std::move(roots)},{"file_choices",std::move(choices)},
        {"file_policy",intent.primary?"unique_current_primary":"explicit_only"}};
}
Json public_acquisition(const Acquisition& group) {
    Json ids=Json::array(),sources=Json::array();
    for(uint32_t i=0;i<group.source_count;++i) {
        ids.push_back(group.ids[i]);
        sources.push_back({{"game",group.intent.game},{"mod_id",group.selected[i].mod},
            {"file_id",group.selected[i].file},{"job_id",group.ids[i]}});
    }
    return {{"acquisition",{{"state","accepted"},{"game",group.intent.game},
        {"idempotency_key",std::string(group.intent.key,group.intent.key_bytes)},
        {"source_job_ids",std::move(ids)},{"selected_sources",std::move(sources)},
        {"compatibility","unresolved"},{"version_satisfaction","unresolved"},{"download_authorized",false},
        {"runtime_supported",false},{"installation_supported",false},{"acquisition_order_is_game_load_order",false}}}};
}
bool positive_id(const Json& value,uint64_t maximum,uint64_t& id) noexcept {
    if(value.is_number_unsigned())id=value.get<uint64_t>();
    else if(value.type()==Json::value_t::number_integer&&value.get<int64_t>()>0)id=static_cast<uint64_t>(value.get<int64_t>());
    else return false;
    return id&&id<=maximum;
}
bool json_text(const Json& object,const char *field,std::string_view expected) {
    return object.contains(field)&&object.at(field).is_string()&&object.at(field).get_ref<const std::string&>()==expected;
}
bool json_bool(const Json& object,const char *field,bool expected) {
    return object.contains(field)&&object.at(field).is_boolean()&&object.at(field).get<bool>()==expected;
}
Status select_acquisition(const Json& report,Acquisition& group) {
    if(!report.is_object()||report.size()!=1||!report.contains("plan")||!report.at("plan").is_object())return MB_NEXUS_JOBS_SOURCE_FAILED;
    const auto& plan=report.at("plan");uint64_t version=0;
    if(!plan.contains("schema_version")||!positive_id(plan.at("schema_version"),1,version)||
       !json_text(plan,"scope","nexus_acquisition_metadata")||!json_text(plan,"game",group.intent.game)||
       !json_text(plan,"file_policy",group.intent.primary?"unique_current_primary":"explicit_only")||
       !json_text(plan,"compatibility","unresolved")||!json_text(plan,"version_satisfaction","unresolved")||
       !json_bool(plan,"download_authorized",false)||!json_bool(plan,"installation_supported",false)||
       !json_bool(plan,"file_version_constraints_provided",false)||!json_bool(plan,"acquisition_order_is_game_load_order",false)||
       !plan.contains("root_mod_ids")||!plan.at("root_mod_ids").is_array()||plan.at("root_mod_ids").size()!=group.intent.root_count||
       !plan.contains("acquisition_ready")||!plan.at("acquisition_ready").is_boolean()||
       !plan.contains("discovery_complete")||!plan.at("discovery_complete").is_boolean()||
       !plan.contains("unresolved")||!plan.at("unresolved").is_array()||plan.at("unresolved").size()>128||
       !plan.contains("relations")||!plan.at("relations").is_array()||plan.at("relations").size()>128||
       !plan.contains("nodes")||!plan.at("nodes").is_array()||plan.at("nodes").empty()||plan.at("nodes").size()>MB_NEXUS_JOBS_LIMIT||
       !plan.contains("selected_sources")||!plan.at("selected_sources").is_array()||plan.at("selected_sources").size()>MB_NEXUS_JOBS_LIMIT)
        return MB_NEXUS_JOBS_SOURCE_FAILED;
    for(uint32_t i=0;i<group.intent.root_count;++i) {
        uint64_t id=0;if(!positive_id(plan.at("root_mod_ids")[i],INT32_MAX,id)||id!=group.intent.roots[i])return MB_NEXUS_JOBS_SOURCE_FAILED;
    }
    if(!plan.at("acquisition_ready").get<bool>())return MB_NEXUS_JOBS_ACQUISITION_UNRESOLVED;
    const auto& sources=plan.at("selected_sources");const auto& nodes=plan.at("nodes");
    if(!plan.at("discovery_complete").get<bool>()||!plan.at("unresolved").empty()||sources.empty()||sources.size()!=nodes.size())return MB_NEXUS_JOBS_SOURCE_FAILED;
    for(size_t i=0;i<sources.size();++i) {
        const auto& source=sources[i];uint64_t mod=0,file=0;
        if(!source.is_object()||!json_text(source,"game",group.intent.game)||!source.contains("mod_id")||
           !positive_id(source.at("mod_id"),INT32_MAX,mod)||!source.contains("file_id")||
           !positive_id(source.at("file_id"),INT64_MAX,file)||!source.contains("selection_basis")||
           !source.at("selection_basis").is_string()||
           (source.at("selection_basis")!="explicit_file"&&source.at("selection_basis")!="unique_current_primary")||
           !source.contains("file_version")||!source.at("file_version").is_string())return MB_NEXUS_JOBS_SOURCE_FAILED;
        for(size_t j=0;j<i;++j)if(group.selected[j].mod==mod)return MB_NEXUS_JOBS_SOURCE_FAILED;
        group.selected[i]={mod,file};
    }
    std::array<uint64_t,MB_NEXUS_JOBS_LIMIT> node_ids{};
    for(size_t i=0;i<nodes.size();++i) {
        const auto& node=nodes[i];uint64_t id=0;
        if(!node.is_object()||!json_text(node,"game",group.intent.game)||!json_bool(node,"discovery_complete",true)||
           !json_bool(node,"source_visible",true)||!node.contains("mod_id")||!positive_id(node.at("mod_id"),INT32_MAX,id))return MB_NEXUS_JOBS_SOURCE_FAILED;
        for(size_t j=0;j<i;++j)if(node_ids[j]==id)return MB_NEXUS_JOBS_SOURCE_FAILED;
        node_ids[i]=id;bool found=false;for(size_t j=0;j<sources.size();++j)found=found||group.selected[j].mod==id;
        if(!found)return MB_NEXUS_JOBS_SOURCE_FAILED;
    }
    for(uint32_t i=0;i<group.intent.root_count;++i) {
        bool found=false;for(size_t j=0;j<nodes.size();++j)found=found||node_ids[j]==group.intent.roots[i];
        if(!found)return MB_NEXUS_JOBS_SOURCE_FAILED;
    }
    group.source_count=static_cast<uint32_t>(sources.size());return MB_NEXUS_JOBS_OK;
}
}

struct mb_nexus_jobs_context_v1 {
    uint64_t marker=magic;
    bool busy=false,storage_failed=false,closing=false,close_uncertain=false;
    mb_nexus_auth_context_v1 *auth=nullptr;
    int directory=-1;uint64_t device=0,inode=0;
    mb_archive_analysis_context_v1 execution_context=MB_ARCHIVE_ANALYSIS_DESKTOP_PREPARATION;
    std::vector<uint8_t> ca;std::string resolve;
    uint32_t resolver_timeout=0,download_timeout=0,analysis_timeout=0,connect_timeout=0;
    uint64_t cap=0;void *entropy_context=nullptr;mb_nexus_jobs_entropy_fn_v1 entropy=nullptr;
    std::array<Record,MB_NEXUS_JOBS_LIMIT> records{};size_t count=0;
    std::array<Acquisition,MB_NEXUS_JOBS_LIMIT> acquisitions{};size_t acquisition_count=0;
    std::array<Generation,MB_NEXUS_GENERATIONS_LIMIT> generations{};size_t generation_count=0;
#if MB_HOST_PAK_PREPARATION
    bool pak_configured=false;
    std::string pak_game;
    modbridge::CapturedPakBase pak_base;
    uint32_t pak_timeout=0;
    std::array<std::unique_ptr<modbridge::PrivatePakJob>,MB_NEXUS_JOBS_LIMIT> preparations{};
    bool iostore_configured=false;
    std::string iostore_game;
    modbridge::CapturedIoStoreBase iostore_base;
    uint32_t iostore_timeout=0;
    std::array<std::unique_ptr<modbridge::PrivateIoStoreJob>,MB_NEXUS_JOBS_LIMIT> iostore_preparations{};
    std::array<std::unique_ptr<modbridge::PrivateIoStoreJob>,MB_NEXUS_GENERATIONS_LIMIT> generation_preparations{};
#endif
};

namespace {
bool valid(const mb_nexus_jobs_context_v1 *c) noexcept {return aligned(c)&&c->marker==magic;}
bool owned_distinct(const mb_nexus_jobs_context_v1& c,const Regions& r) noexcept {
    return r.distinct()&&r.outside(&c,sizeof(c))&&r.outside(c.ca.data(),c.ca.size())&&
#if MB_HOST_PAK_PREPARATION
        r.outside(c.pak_game.data(),c.pak_game.capacity()+1)&&
        r.outside(c.iostore_game.data(),c.iostore_game.capacity()+1)&&
#endif
        r.outside(c.resolve.data(),c.resolve.capacity()+1)&&
        r.outside(c.auth,c.auth?1:0)&&r.outside(c.entropy_context,c.entropy_context?1:0);
}
struct Busy {
    mb_nexus_jobs_context_v1& c;
    explicit Busy(mb_nexus_jobs_context_v1& value):c(value){c.busy=true;}
    ~Busy(){c.busy=false;}
};
mb_nexus_jobs_result_v1 initial() noexcept {
    mb_nexus_jobs_result_v1 r{};r.struct_size=sizeof(r);r.abi_version=MB_NEXUS_JOBS_ABI_V1;return r;
}
int finish(mb_nexus_jobs_result_v1& r,mb_nexus_jobs_result_v1 *out,Status status) noexcept {
    r.status=status;r.complete=status==MB_NEXUS_JOBS_OK;*out=r;return status;
}
bool output_layout(const void *p,size_t bytes,mb_nexus_jobs_result_v1 *out,char *buffer,size_t capacity,Regions& r) noexcept {
    return aligned(out)&&capacity&&capacity<=MB_NEXUS_JOBS_OUTPUT_LIMIT&&
        r.add(p,bytes)&&r.add(out,sizeof(*out))&&r.add(buffer,capacity);
}
Status admission(mb_nexus_jobs_context_v1& c,const char *cookie,size_t cookie_bytes,const char *csrf=nullptr,size_t csrf_bytes=0,
    const char *origin=nullptr,size_t origin_bytes=0,bool account=false) noexcept {
    mb_nexus_auth_admit_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=MB_NEXUS_AUTH_ABI_V1;
    p.cookie=cookie;p.cookie_bytes=cookie_bytes;p.csrf=csrf;p.csrf_bytes=csrf_bytes;p.origin=origin;p.origin_bytes=origin_bytes;
    p.admission=csrf_bytes||origin_bytes?MB_NEXUS_AUTH_ADMISSION_MUTATION:MB_NEXUS_AUTH_ADMISSION_READ;p.require_account=account;
    mb_nexus_auth_result_v1 result{};
    auto code=mb_nexus_auth_admit_v1(c.auth,&p,&result);
    if(code==MB_NEXUS_AUTH_OK&&result.status==MB_NEXUS_AUTH_OK&&result.complete)return MB_NEXUS_JOBS_OK;
    switch(result.status) {
        case MB_NEXUS_AUTH_SESSION_REQUIRED:return MB_NEXUS_JOBS_SESSION_REQUIRED;
        case MB_NEXUS_AUTH_NOT_CONFIGURED:case MB_NEXUS_AUTH_RECONNECT_REQUIRED:return MB_NEXUS_JOBS_AUTH_REQUIRED;
        case MB_NEXUS_AUTH_REFUSED:return MB_NEXUS_JOBS_REFUSED;
        case MB_NEXUS_AUTH_SOURCE_FAILED:return MB_NEXUS_JOBS_SOURCE_FAILED;
        case MB_NEXUS_AUTH_BUSY:return MB_NEXUS_JOBS_BUSY;
        default:return MB_NEXUS_JOBS_INTERNAL_ERROR;
    }
}
void counts(const mb_nexus_jobs_context_v1& c,mb_nexus_jobs_result_v1& r,const std::array<uint8_t,32> *owner=nullptr) noexcept {
    r.job_count=0;r.queued_count=0;
    for(size_t i=0;i<c.count;++i)if(!owner||equal(c.records[i].owner,*owner)) {
        ++r.job_count;if(c.records[i].state.value==State::Queued)++r.queued_count;
    }
}
void generation_counts(const mb_nexus_jobs_context_v1& c,mb_nexus_jobs_result_v1& r,const std::array<uint8_t,32> *owner=nullptr) noexcept {
    r.job_count=0;r.queued_count=0;
    for(size_t i=0;i<c.generation_count;++i)if(!owner||equal(c.generations[i].owner,*owner)) {
        ++r.job_count;if(c.generations[i].state==Generation::Queued)++r.queued_count;
    }
}
Status serialize(const Json& value,char *out,size_t capacity,mb_nexus_jobs_result_v1& result) {
    auto text=value.dump(-1,' ',true);
    if(text.size()>=capacity)return MB_NEXUS_JOBS_OUTPUT_TOO_SMALL;
    std::memcpy(out,text.data(),text.size());out[text.size()]=0;result.output_bytes=text.size();return MB_NEXUS_JOBS_OK;
}
bool directory_valid(const mb_nexus_jobs_context_v1& c) noexcept {
    struct stat s{};
    return fstat(c.directory,&s)==0&&S_ISDIR(s.st_mode)&&s.st_uid==geteuid()&&(s.st_mode&07777)==0700&&
        static_cast<uint64_t>(s.st_dev)==c.device&&static_cast<uint64_t>(s.st_ino)==c.inode;
}
std::string leaf(const Record& r) {return "mb-job-"+std::string(r.id)+".archive";}
bool file_private(const struct stat& s) noexcept {
    return S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&(s.st_mode&07777)==0600&&s.st_nlink==1&&s.st_size>=0;
}
bool file_identity(const Record& r,const struct stat& s) noexcept {
    return r.identity_valid&&static_cast<uint64_t>(s.st_dev)==r.device&&static_cast<uint64_t>(s.st_ino)==r.inode;
}
bool close_file(mb_nexus_jobs_context_v1& c,Record& record) noexcept {
    if(record.fd<0)return true;
    int descriptor=record.fd;record.fd=-1;
    if(close(descriptor)==0)return true;
    // Never retry an ambiguous close after another descriptor may reuse its
    // number. Latch refusal; process shutdown remains the final OS cleanup.
    c.close_uncertain=true;c.storage_failed=true;return false;
}
bool cleanup_file(mb_nexus_jobs_context_v1& c,Record& record) noexcept {
    if(!record.leaf_owned)return close_file(c,record);
    if(!directory_valid(c))return false;
    if(!record.identity_valid) {
        struct stat descriptor{};
        if(record.fd<0||fstat(record.fd,&descriptor)!=0||!S_ISREG(descriptor.st_mode))return false;
        record.device=static_cast<uint64_t>(descriptor.st_dev);record.inode=static_cast<uint64_t>(descriptor.st_ino);record.identity_valid=true;
    }
    // The leaf is generated from a validated ID; construction cannot allocate.
    char name[48]{};std::memcpy(name,"mb-job-",7);std::memcpy(name+7,record.id,32);std::memcpy(name+39,".archive",8);
    struct stat path{};
    if(mb_archive_posix_fstatat(c.directory,name,&path,AT_SYMLINK_NOFOLLOW)!=0) {
        if(errno!=ENOENT)return false;
    } else {
        if(!file_identity(record,path)||!S_ISREG(path.st_mode))return false;
        if(mb_archive_posix_unlinkat(c.directory,name,0)!=0&&errno!=ENOENT)return false;
    }
    record.leaf_owned=false;return close_file(c,record);
}
void fail_record(mb_nexus_jobs_context_v1& c,Record& record,Error error,bool authorization=false) noexcept {
    bindings_clear(record);wipe(record.report,sizeof(record.report));record.report_bytes=0;
    wipe(record.preparation_report,sizeof(record.preparation_report));record.preparation_bytes=0;
#if MB_HOST_PAK_PREPARATION
    for(size_t i=0;i<c.count;++i)if(&c.records[i]==&record&&c.preparations[i]) {
        if(!c.preparations[i]->cleanup()){c.storage_failed=true;error=Error::Storage;authorization=false;}
        else c.preparations[i].reset();
    }
    for(size_t i=0;i<c.count;++i)if(&c.records[i]==&record&&c.iostore_preparations[i]) {
        if(!c.iostore_preparations[i]->cleanup()){c.storage_failed=true;error=Error::Storage;authorization=false;}
        else c.iostore_preparations[i].reset();
    }
#endif
    if(!cleanup_file(c,record)){c.storage_failed=true;error=Error::Storage;authorization=false;}
    record.error=error;record.state.value=authorization?State::NeedsAuthorization:State::Failed;
}

extern "C" int mb_nexus_jobs_configure_pak_v1(mb_nexus_jobs_context_v1 *c,const mb_nexus_jobs_pak_profile_v1 *p,mb_nexus_jobs_result_v1 *out) noexcept {
    if(!valid(c)||!aligned(p)||!aligned(out)||p->struct_size!=sizeof(*p)||p->abi_version!=MB_NEXUS_JOBS_ABI_V1||p->nexus_game_bytes>80)return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Regions regions;
    if(!regions.add(p,sizeof(*p))||!regions.add(out,sizeof(*out))||!regions.add(p->nexus_game,p->nexus_game_bytes)||!owned_distinct(*c,regions))return MB_NEXUS_JOBS_BAD_ARGUMENT;
    auto result=initial();
    if(c->busy)return finish(result,out,MB_NEXUS_JOBS_BUSY);
    if(c->closing||c->storage_failed)return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);
    if(c->count)return finish(result,out,MB_NEXUS_JOBS_REFUSED);
#if MB_HOST_PAK_PREPARATION
    if(c->pak_configured)return finish(result,out,MB_NEXUS_JOBS_REFUSED);
    if(p->reserved||p->immutable_captured_base!=1||!p->nexus_game_bytes||!domain(std::string_view(p->nexus_game,p->nexus_game_bytes))||
       !p->timeout_ms||p->timeout_ms>MB_NEXUS_JOBS_TIMEOUT_LIMIT_MS)return finish(result,out,MB_NEXUS_JOBS_BAD_ARGUMENT);
    Busy busy(*c);
    try {
        modbridge::CapturedPakBase admitted;
        if(!modbridge::admit_captured_pak_base(p->base_fd,p->expected_bytes,p->expected_sha256,p->timeout_ms,admitted))return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
        c->pak_game.assign(p->nexus_game,p->nexus_game_bytes);c->pak_base=admitted;c->pak_timeout=p->timeout_ms;c->pak_configured=true;
        return finish(result,out,MB_NEXUS_JOBS_OK);
    }catch(...){return finish(result,out,MB_NEXUS_JOBS_INTERNAL_ERROR);}
#else
    return finish(result,out,MB_NEXUS_JOBS_REFUSED);
#endif
}
extern "C" int mb_nexus_jobs_configure_iostore_v1(mb_nexus_jobs_context_v1 *c,const mb_nexus_jobs_iostore_profile_v1 *p,mb_nexus_jobs_result_v1 *out) noexcept {
    if(!valid(c)||!aligned(p)||!aligned(out)||p->struct_size!=sizeof(*p)||p->abi_version!=MB_NEXUS_JOBS_ABI_V1||p->nexus_game_bytes>80)return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Regions regions;
    if(!regions.add(p,sizeof(*p))||!regions.add(out,sizeof(*out))||!regions.add(p->nexus_game,p->nexus_game_bytes)||!owned_distinct(*c,regions))return MB_NEXUS_JOBS_BAD_ARGUMENT;
    auto result=initial();
    if(c->busy)return finish(result,out,MB_NEXUS_JOBS_BUSY);
    if(c->closing||c->storage_failed)return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);
    if(c->count)return finish(result,out,MB_NEXUS_JOBS_REFUSED);
#if MB_HOST_PAK_PREPARATION
    if(c->iostore_configured)return finish(result,out,MB_NEXUS_JOBS_REFUSED);
    if(p->reserved||p->immutable_captured_base!=1||!p->nexus_game_bytes||!domain(std::string_view(p->nexus_game,p->nexus_game_bytes))||
       !p->timeout_ms||p->timeout_ms>MB_NEXUS_JOBS_TIMEOUT_LIMIT_MS)return finish(result,out,MB_NEXUS_JOBS_BAD_ARGUMENT);
    Busy busy(*c);
    try {
        modbridge::CapturedIoStoreBase admitted;
        if(!modbridge::admit_captured_iostore_base(p->base_toc_fd,p->expected_toc_bytes,p->expected_toc_sha256,
            p->base_header_fd,p->expected_header_bytes,p->expected_header_sha256,p->observed_base_ucas_bytes,p->timeout_ms,admitted))
            return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
        c->iostore_game.assign(p->nexus_game,p->nexus_game_bytes);c->iostore_base=admitted;c->iostore_timeout=p->timeout_ms;c->iostore_configured=true;
        return finish(result,out,MB_NEXUS_JOBS_OK);
    }catch(...){return finish(result,out,MB_NEXUS_JOBS_INTERNAL_ERROR);}
#else
    return finish(result,out,MB_NEXUS_JOBS_REFUSED);
#endif
}
struct Scratch {
    std::unique_ptr<uint8_t[]> raw;
    std::unique_ptr<char[]> metadata;
    std::array<char,MB_NEXUS_DOWNLOAD_URI_LIMIT_BYTES> uri{};
    Scratch():raw(new uint8_t[MB_NEXUS_DOWNLOAD_RAW_LIMIT_BYTES]{}),metadata(new char[MB_NEXUS_DOWNLOAD_REPORT_LIMIT_BYTES]{}){}
    void clear_uri() noexcept {wipe(uri.data(),uri.size());}
    ~Scratch(){wipe(raw.get(),MB_NEXUS_DOWNLOAD_RAW_LIMIT_BYTES);wipe(metadata.get(),MB_NEXUS_DOWNLOAD_REPORT_LIMIT_BYTES);clear_uri();}
};
bool complete_file(const mb_curl_download_result_v1& r,uint64_t cap) noexcept {
    return r.status==MB_CURL_DOWNLOAD_OK&&r.complete&&r.file.status==MB_DOWNLOAD_FILE_SINK_OK&&
        r.file.complete&&r.file.readback_verified&&r.file.bytes_received==r.file.bytes_written&&
        r.received_bytes==r.file.bytes_received&&r.file.bytes_received&&r.file.bytes_received<=cap;
}
void prepare_download(mb_nexus_jobs_context_v1& c,Record& record) {
    if(!record.kind)return;
    Json report={{"status","unresolved"},{"artifact_complete",false},{"whole_bundle_handled",false},
                 {"installation_supported",false},{"runtime_supported",false}};
    if(record.kind==4) {
        report["automatic_dispatch"]=true;
        report["dispatch_basis"]="inventory_hint_then_core_validation";
    }
#if MB_HOST_PAK_PREPARATION
    uint32_t handler=record.kind;
    if(record.kind==4) {
        // Routing hints grant no format authority. Prefer a container group over
        // its companion PAK; the selected core rescans every original member.
        const auto inventory=Json::parse(record.report,record.report+record.report_bytes);
        const auto& types=inventory.at("candidate_types");
        const auto paks=types.at("pak").get<uint32_t>();
        const auto inis=types.at("ini").get<uint32_t>();
        const auto iostore=types.at("iostore").get<uint32_t>();
        handler=iostore?5:paks==1?1:!paks&&inis==1?3:0;
        if(!handler) {
            report["status"]="additional_action_required";
            report["unhandled_members"]=inventory.at("regular_count");
            report["reason"]="no_single_preparation_handler";
        }
    }
    if(handler==5) {
        report["handler"]="unreal_iostore";
        if(c.iostore_configured&&c.iostore_game==std::string_view(record.domain,record.domain_bytes)) {
            if(record.bytes>MB_PAK_ARCHIVE_JOB_MAX_INPUT_BYTES)report["status"]="preparation_limit";
            else {
                size_t index=static_cast<size_t>(&record-c.records.data());
                auto owner=std::make_unique<modbridge::PrivateIoStoreJob>(c.directory,record.id,c.iostore_base,c.iostore_timeout);
                auto *job=owner.get();c.iostore_preparations[index]=std::move(owner);
                mb_archive_source_v1 source{};source.struct_size=sizeof(source);source.abi_version=MB_ARCHIVE_ABI_V1;
                source.fd=record.fd;source.size=record.bytes;std::copy(record.sha.begin(),record.sha.end(),source.sha256);
                job->prepare(source);
                using S=modbridge::PrivateIoStoreJob::State;
                report["status"]=job->state==S::Prepared?"prepared":job->state==S::AdditionalActionRequired?"additional_action_required":
                    job->state==S::Stopped?"stopped":job->state==S::StorageFailed?"storage_failed":"refused";
                report["unhandled_members"]=job->unhandled;report["whole_bundle_handled"]=job->whole_bundle_handled;
                report["artifact_complete"]=job->artifact_complete;report["physical_readback_observed"]=job->readback_observed;
                report["stage"]=job->stage;
                if(job->artifact_complete) {
                    report["artifacts"]=Json::array();
                    for(uint32_t i=0;i<MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT;++i)
                        report["artifacts"].push_back(Json{{"index",i},{"bytes",job->artifact_bytes[i]},{"sha256",hex(job->artifact_sha[i].data(),32)}});
                    report["incoming_asset_hashes_verified"]=job->result.incoming_asset_hashes_verified;
                    report["emitted_chunks"]=job->result.emitted_chunks;
                    report["verified_empty_pak_companions"]=job->selection.omitted_empty_paks;
                    report["verified_plaintext_documents"]=job->selection.omitted_documents;
                }
                bool preparation_storage_failed=job->storage_failed();
                if(!job->artifact_complete) {
                    if(!job->cleanup())c.storage_failed=true;
                    else c.iostore_preparations[index].reset();
                }
                if(preparation_storage_failed)c.storage_failed=true;
            }
        }
    }
    else if(handler&&c.pak_configured&&c.pak_game==std::string_view(record.domain,record.domain_bytes)) {
        if(record.bytes>MB_PAK_ARCHIVE_JOB_MAX_INPUT_BYTES)report["status"]="preparation_limit";
        else {
            if(handler) {
                size_t index=static_cast<size_t>(&record-c.records.data());
                auto owner=std::make_unique<modbridge::PrivatePakJob>(c.directory,record.id,c.pak_base,c.pak_timeout);
                auto *job=owner.get();c.preparations[index]=std::move(owner);
                mb_archive_source_v1 source{};source.struct_size=sizeof(source);source.abi_version=MB_ARCHIVE_ABI_V1;
                source.fd=record.fd;source.size=record.bytes;std::copy(record.sha.begin(),record.sha.end(),source.sha256);
                if(handler==3)job->prepare_config(source);
                else job->prepare(source,record.kind==2);
                using S=modbridge::PrivatePakJob::State;
                report["status"]=job->state==S::Prepared?"prepared":job->state==S::AdditionalActionRequired?"additional_action_required":
                    job->state==S::Stopped?"stopped":job->state==S::StorageFailed?"storage_failed":"refused";
                report["unhandled_members"]=job->unhandled;report["whole_bundle_handled"]=job->whole_bundle_handled;
                report["artifact_complete"]=job->artifact_complete;report["physical_readback_observed"]=job->readback_observed;
                report["stage"]=job->stage;
                if(job->artifact_complete){report["artifact_sha256"]=hex(job->artifact_sha.data(),32);report["artifact_bytes"]=job->artifact_bytes;}
                if(job->config_mode) {
                    report["handler"]="unreal_engine_ini";
                    report["selected_sections"]=Json::array({"SystemSettings"});
                    report["section_policy"]="all_source_headers_must_be_selected";
                    report["target_derived"]=job->artifact_complete&&job->config_result.resolved_path_bytes&&job->config_result.resolved_mount_bytes;
                    if(job->artifact_complete) {
                        const auto& config=job->config_result.preparation;
                        report["scalar_winners"]=config.scalar_winners;report["replacements"]=config.replacements;
                        report["additions"]=config.additions;report["merged_bytes"]=config.merged_bytes;
                        report["merged_sha256"]=hex(config.merged_sha256,32);
                    }
                }
                else if(record.kind==4)report["handler"]="unreal_classic_pak";
                // Failed/private partial artifacts are never offered as prepared.
                // Identity-bound cleanup remains retryable via this retained owner.
                bool preparation_storage_failed=job->storage_failed();
                if(!job->artifact_complete&&job->state!=S::AdditionalActionRequired) {
                    if(!job->cleanup())c.storage_failed=true;
                    else c.preparations[index].reset();
                }
                if(preparation_storage_failed)c.storage_failed=true;
            }
        }
    }
#else
    (void)c;
#endif
    auto encoded=report.dump();
    if(encoded.size()>=sizeof(record.preparation_report))throw std::length_error("preparation report limit");
    std::memcpy(record.preparation_report,encoded.data(),encoded.size());record.preparation_report[encoded.size()]=0;record.preparation_bytes=encoded.size();
}
bool generation_clock(uint64_t& value) noexcept {
    struct timespec ts{};
    if(clock_gettime(CLOCK_MONOTONIC,&ts)||ts.tv_sec<0||ts.tv_nsec<0||ts.tv_nsec>=1000000000||
       static_cast<uint64_t>(ts.tv_sec)>(UINT64_MAX-999)/1000)return false;
    value=static_cast<uint64_t>(ts.tv_sec)*1000+static_cast<uint64_t>(ts.tv_nsec)/1000000;return true;
}
struct AcquisitionBudget {
    uint64_t last=0,deadline=0;bool terminal=false;
    bool start(uint32_t timeout) noexcept {
        if(!generation_clock(last)||last>UINT64_MAX-timeout){terminal=true;return false;}
        deadline=last+timeout;return true;
    }
    bool remaining(uint32_t& timeout) noexcept {
        uint64_t now=0;
        if(terminal||!generation_clock(now)||now<last||now>=deadline){terminal=true;return false;}
        last=now;timeout=static_cast<uint32_t>(deadline-now);return true;
    }
};
#if MB_HOST_PAK_PREPARATION
struct GenerationGate {
    uint64_t last=0,deadline=0;bool terminal=false;
    bool stopped() noexcept {
        uint64_t now=0;if(terminal||!generation_clock(now)||now<last||now>=deadline)terminal=true;last=now;return terminal;
    }
};
bool generation_source_verified(mb_nexus_jobs_context_v1& c,Record& r,uint64_t bytes,
    const std::array<uint8_t,32>& expected,GenerationGate& gate) {
    struct stat before{},after{},named{};
    if(gate.stopped()||!directory_valid(c)||r.fd>=0||!r.leaf_owned||!r.identity_valid||r.bytes!=bytes||r.sha!=expected)return false;
    auto name=leaf(r);r.fd=mb_archive_posix_openat(c.directory,name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC,0);
    if(r.fd<0||fstat(r.fd,&before)||!file_private(before)||!file_identity(r,before)||static_cast<uint64_t>(before.st_size)!=bytes)return false;
    std::array<uint8_t,65536> chunk{};picosha2::hash256_one_by_one hash;hash.init();uint64_t at=0;uint32_t calls=0;
    while(at<bytes) {
        if(gate.stopped()||++calls>32768)return false;
        auto n=static_cast<size_t>(std::min<uint64_t>(chunk.size(),bytes-at));
        auto actual=pread(r.fd,chunk.data(),n,static_cast<off_t>(at));
        if(gate.stopped())return false;
        if(actual<0&&errno==EINTR)continue;
        if(actual<=0||static_cast<size_t>(actual)>n)return false;
        hash.process(chunk.begin(),chunk.begin()+actual);at+=static_cast<uint64_t>(actual);
    }
    std::array<uint8_t,32> observed{};hash.finish();hash.get_hash_bytes(observed.begin(),observed.end());
    if(observed!=expected||fstat(r.fd,&after)||!file_private(after)||!file_identity(r,after)||after.st_size!=before.st_size||
       after.st_mode!=before.st_mode||after.st_nlink!=before.st_nlink||
#if defined(__APPLE__)
       after.st_mtimespec.tv_sec!=before.st_mtimespec.tv_sec||after.st_mtimespec.tv_nsec!=before.st_mtimespec.tv_nsec||
       after.st_ctimespec.tv_sec!=before.st_ctimespec.tv_sec||after.st_ctimespec.tv_nsec!=before.st_ctimespec.tv_nsec||
#else
       after.st_mtim.tv_sec!=before.st_mtim.tv_sec||after.st_mtim.tv_nsec!=before.st_mtim.tv_nsec||
       after.st_ctim.tv_sec!=before.st_ctim.tv_sec||after.st_ctim.tv_nsec!=before.st_ctim.tv_nsec||
#endif
       mb_archive_posix_fstatat(c.directory,name.c_str(),&named,AT_SYMLINK_NOFOLLOW)||!file_private(named)||!file_identity(r,named)||
       named.st_size!=before.st_size||gate.stopped())return false;
    return true;
}
void generation_report(Generation& g,const Json& report) {
    auto text=report.dump();if(text.size()>=sizeof(g.report))throw std::length_error("generation report limit");
    std::memcpy(g.report,text.data(),text.size());g.report[text.size()]=0;g.report_bytes=text.size();
}
#endif
Status prepare_generation(mb_nexus_jobs_context_v1& c,Generation& generation) noexcept {
    generation.state=Generation::Refused;
    std::snprintf(generation.reason,sizeof(generation.reason),"generation_refused");
#if MB_HOST_PAK_PREPARATION
    size_t index=static_cast<size_t>(&generation-c.generations.data());
    bool storage_failed=false;
    try {
        if(!c.iostore_configured||generation.base_token!=c.iostore_base.token||c.iostore_game!=std::string_view(generation.game,generation.game_bytes))
            throw std::runtime_error("captured_profile_changed");
        uint64_t now=0;if(!generation_clock(now)||now>UINT64_MAX-c.iostore_timeout)throw std::runtime_error("clock_refused");
        GenerationGate gate{now,now+c.iostore_timeout,false};
        std::array<mb_archive_source_v1,MB_NEXUS_JOBS_LIMIT> sources{};
        for(uint32_t i=0;i<generation.source_count;++i) {
            auto& record=c.records[generation.source_indices[i]];
            if(record.state.value!=State::Analyzed||!record.downloaded||!equal(record.owner,generation.owner)||
               std::memcmp(record.id,generation.source_ids[i],32)||generation.source_bytes[i]>MB_PAK_ARCHIVE_JOB_MAX_INPUT_BYTES||
               !generation_source_verified(c,record,generation.source_bytes[i],generation.source_sha[i],gate)) {
                std::snprintf(generation.reason,sizeof(generation.reason),"source_identity_or_limit_refused");throw std::runtime_error("source_refused");
            }
            auto& source=sources[i];source.struct_size=sizeof(source);source.abi_version=MB_ARCHIVE_ABI_V1;
            source.fd=record.fd;source.size=record.bytes;std::copy(record.sha.begin(),record.sha.end(),source.sha256);
        }
        if(gate.stopped())throw std::runtime_error("verification_deadline");
        uint32_t remaining=static_cast<uint32_t>(gate.deadline-gate.last);
        auto owner=std::make_unique<modbridge::PrivateIoStoreJob>(c.directory,generation.id,c.iostore_base,remaining);
        auto *job=owner.get();c.generation_preparations[index]=std::move(owner);job->prepare_batch(sources.data(),generation.source_count);
        using S=modbridge::PrivateIoStoreJob::State;
        generation.state=job->state==S::Prepared?Generation::Prepared:job->state==S::Conflict?Generation::Conflict:Generation::Refused;
        std::snprintf(generation.reason,sizeof(generation.reason),"%s",job->state==S::Prepared?"":job->state==S::Conflict?"incoming_mod_overlap":
            job->state==S::Stopped?"preparation_stopped":job->state==S::StorageFailed?"storage_failed":"preparation_refused");
        Json report={{"handler","unreal_iostore_generation"},{"status",generation.state==Generation::Prepared?"prepared":
            generation.state==Generation::Conflict?"conflict":"refused"},{"source_count",generation.source_count},
            {"whole_bundle_handled",job->whole_bundle_handled},{"artifact_complete",job->artifact_complete},
            {"physical_readback_observed",job->readback_observed},{"runtime_supported",false},{"installation_supported",false},{"stage",job->stage}};
        if(job->artifact_complete) {
            report["artifacts"]=Json::array();
            for(uint32_t i=0;i<MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT;++i)
                report["artifacts"].push_back(Json{{"index",i},{"bytes",job->artifact_bytes[i]},{"sha256",hex(job->artifact_sha[i].data(),32)}});
            report["incoming_asset_hashes_verified"]=job->result.incoming_asset_hashes_verified;
            report["emitted_chunks"]=job->result.emitted_chunks;
            report["verified_empty_pak_companions"]=job->omitted_empty_paks;
            report["verified_plaintext_documents"]=job->omitted_documents;
        }
        if(generation.state==Generation::Conflict) {
            const auto& conflict=job->batch_result;
            const char *kind=conflict.conflict_kind==MB_IOSTORE_BATCH_CONFLICT_SOURCE_IDENTITY?"source_identity":
                conflict.conflict_kind==MB_IOSTORE_BATCH_CONFLICT_CHUNK_ID?"chunk_id":
                conflict.conflict_kind==MB_IOSTORE_BATCH_CONFLICT_PACKAGE_ID?"package_id":
                conflict.conflict_kind==MB_IOSTORE_BATCH_CONFLICT_DIRECTORY_PATH?"directory_path":"unknown";
            report["conflict"]={{"kind",kind},{"source_a",conflict.conflict_source_a},{"source_b",conflict.conflict_source_b}};
        }
        generation_report(generation,report);storage_failed=job->storage_failed();
    }catch(...) {
        generation.state=Generation::Refused;
    }
    // Every reopened source descriptor is owned by its retained record. Always
    // close it before presenting the generation, even when the owner stopped.
    for(uint32_t i=0;i<generation.source_count;++i)if(!close_file(c,c.records[generation.source_indices[i]]))storage_failed=true;
    if(c.generation_preparations[index]) {
        auto *job=c.generation_preparations[index].get();storage_failed=storage_failed||job->storage_failed();
        if(generation.state!=Generation::Prepared||storage_failed) {
            if(!job->cleanup())storage_failed=true;else c.generation_preparations[index].reset();
        }
    }
    if(storage_failed||c.storage_failed) {
        c.storage_failed=true;generation.state=Generation::Refused;
        wipe(generation.report,sizeof(generation.report));generation.report_bytes=0;
        std::snprintf(generation.reason,sizeof(generation.reason),"storage_failed");return MB_NEXUS_JOBS_STORAGE_FAILED;
    }
#else
    (void)c;
#endif
    return MB_NEXUS_JOBS_OK;
}
}

extern "C" int mb_nexus_jobs_create_v1(const mb_nexus_jobs_create_params_v1 *p,mb_nexus_jobs_context_v1 **handle,mb_nexus_jobs_result_v1 *out) noexcept {
    if(!aligned(p)||!aligned(handle)||!aligned(out)||p->struct_size!=sizeof(*p)||p->abi_version!=MB_NEXUS_JOBS_ABI_V1||
        p->ca_bytes>MB_CURL_DOWNLOAD_CA_MAX||p->archive_resolve_bytes>=MB_CURL_DOWNLOAD_URL_MAX)return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Regions regions;
    if(!regions.add(p,sizeof(*p))||!regions.add(handle,sizeof(*handle))||!regions.add(out,sizeof(*out))||
        !regions.add(p->ca_pem,p->ca_bytes)||!regions.add(p->archive_resolve,p->archive_resolve_bytes? p->archive_resolve_bytes+1:0)||
        !regions.add(p->auth,p->auth?1:0)||!regions.add(p->entropy_context,p->entropy_context?1:0)||!regions.distinct())return MB_NEXUS_JOBS_BAD_ARGUMENT;
    auto result=initial();
    if(*handle||!p->auth||!p->ca_bytes||!p->entropy_read32||p->scratch_directory_fd<0||!p->archive_cap_bytes||
        p->archive_cap_bytes>MB_NEXUS_JOBS_ARCHIVE_LIMIT||
        (p->execution_context!=MB_ARCHIVE_ANALYSIS_DESKTOP_PREPARATION&&p->execution_context!=MB_ARCHIVE_ANALYSIS_PS5_NATIVE)||
        !p->resolver_timeout_ms||p->resolver_timeout_ms>MB_NEXUS_AUTH_TIMEOUT_LIMIT_MS||
        !p->download_timeout_ms||p->download_timeout_ms>MB_NEXUS_JOBS_TIMEOUT_LIMIT_MS||
        !p->analysis_timeout_ms||p->analysis_timeout_ms>MB_NEXUS_JOBS_TIMEOUT_LIMIT_MS||
        !p->connect_timeout_ms||p->connect_timeout_ms>p->resolver_timeout_ms||p->connect_timeout_ms>p->download_timeout_ms)
        return finish(result,out,MB_NEXUS_JOBS_BAD_ARGUMENT);
    if(p->archive_resolve_bytes) {
        if(p->archive_resolve[p->archive_resolve_bytes]!=0)return finish(result,out,MB_NEXUS_JOBS_BAD_ARGUMENT);
        for(size_t i=0;i<p->archive_resolve_bytes;++i){auto c=static_cast<unsigned char>(p->archive_resolve[i]);if(c<33||c>126)return finish(result,out,MB_NEXUS_JOBS_BAD_ARGUMENT);}
    } else if(p->archive_resolve)return finish(result,out,MB_NEXUS_JOBS_BAD_ARGUMENT);
    struct stat directory{};
    if(fstat(p->scratch_directory_fd,&directory)!=0||!S_ISDIR(directory.st_mode)||directory.st_uid!=geteuid()||(directory.st_mode&07777)!=0700)
        return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);
    try {
        auto c=std::make_unique<mb_nexus_jobs_context_v1>();
        c->auth=p->auth;c->directory=p->scratch_directory_fd;c->device=static_cast<uint64_t>(directory.st_dev);c->inode=static_cast<uint64_t>(directory.st_ino);
        c->execution_context=p->execution_context;c->ca.assign(p->ca_pem,p->ca_pem+p->ca_bytes);
        if(p->archive_resolve_bytes)c->resolve.assign(p->archive_resolve,p->archive_resolve_bytes);
        c->resolver_timeout=p->resolver_timeout_ms;c->download_timeout=p->download_timeout_ms;c->analysis_timeout=p->analysis_timeout_ms;c->connect_timeout=p->connect_timeout_ms;
        c->cap=p->archive_cap_bytes;c->entropy_context=p->entropy_context;c->entropy=p->entropy_read32;
        *handle=c.release();return finish(result,out,MB_NEXUS_JOBS_OK);
    }catch(...){return finish(result,out,MB_NEXUS_JOBS_INTERNAL_ERROR);}
}

extern "C" int mb_nexus_jobs_free_v1(mb_nexus_jobs_context_v1 **handle) noexcept {
    if(!aligned(handle))return MB_NEXUS_JOBS_BAD_ARGUMENT;
    auto *c=*handle;if(!c)return MB_NEXUS_JOBS_OK;
    Regions regions;if(!valid(c)||!regions.add(handle,sizeof(*handle))||!owned_distinct(*c,regions))return MB_NEXUS_JOBS_BAD_ARGUMENT;
    if(c->busy)return MB_NEXUS_JOBS_BUSY;
    c->closing=true;bool cleaned=!c->close_uncertain;
    for(size_t i=0;i<c->generation_count;++i) {
        auto& generation=c->generations[i];
        wipe(generation.key,sizeof(generation.key));wipe(generation.owner.data(),generation.owner.size());
        wipe(generation.report,sizeof(generation.report));generation.report_bytes=0;
#if MB_HOST_PAK_PREPARATION
        if(c->generation_preparations[i]&&!c->generation_preparations[i]->cleanup())cleaned=false;
        else c->generation_preparations[i].reset();
#endif
    }
    for(size_t i=0;i<c->count;++i) {
        auto& record=c->records[i];bindings_clear(record);wipe(record.key,sizeof(record.key));wipe(record.owner.data(),record.owner.size());
        wipe(record.report,sizeof(record.report));record.report_bytes=0;
#if MB_HOST_PAK_PREPARATION
        if(c->preparations[i]&&!c->preparations[i]->cleanup())cleaned=false;
        else c->preparations[i].reset();
        if(c->iostore_preparations[i]&&!c->iostore_preparations[i]->cleanup())cleaned=false;
        else c->iostore_preparations[i].reset();
#endif
        if(!cleanup_file(*c,record))cleaned=false;
    }
    if(!cleaned){c->storage_failed=true;return MB_NEXUS_JOBS_STORAGE_FAILED;}
    for(auto& record:c->records)wipe(&record,sizeof(record));
    for(auto& group:c->acquisitions)wipe(&group,sizeof(group));
    for(auto& generation:c->generations)wipe(&generation,sizeof(generation));
    wipe(c->ca.data(),c->ca.size());wipe(c->resolve.data(),c->resolve.size());c->marker=0;delete c;*handle=nullptr;return MB_NEXUS_JOBS_OK;
}

extern "C" int mb_nexus_jobs_submit_v1(mb_nexus_jobs_context_v1 *c,const mb_nexus_jobs_submit_params_v1 *p,mb_nexus_jobs_result_v1 *out) noexcept {
    if(!valid(c)||!aligned(p)||p->struct_size!=sizeof(*p)||p->abi_version!=MB_NEXUS_JOBS_ABI_V1||
        p->cookie_bytes>43||p->csrf_bytes>43||p->origin_bytes>128||!p->intent_bytes||p->intent_bytes>MB_NEXUS_JOBS_INTENT_LIMIT)return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Regions regions;
    if(!output_layout(p,sizeof(*p),out,p->output,p->output_capacity,regions)||!regions.add(p->cookie,p->cookie_bytes)||
        !regions.add(p->csrf,p->csrf_bytes)||!regions.add(p->origin,p->origin_bytes)||!regions.add(p->intent,p->intent_bytes)||!owned_distinct(*c,regions))return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Output output{p->output,p->output_capacity};auto result=initial();
    if(c->busy)return finish(result,out,MB_NEXUS_JOBS_BUSY);
    Busy busy(*c);
    if(!token(p->cookie,p->cookie_bytes)||!token(p->csrf,p->csrf_bytes)||!p->origin_bytes)return finish(result,out,MB_NEXUS_JOBS_BAD_ARGUMENT);
    if(c->closing||c->storage_failed)return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);
    Intent intent{};auto status=IntentParser(std::string_view(reinterpret_cast<const char *>(p->intent),p->intent_bytes)).parse(intent);
    if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
    status=admission(*c,p->cookie,p->cookie_bytes,p->csrf,p->csrf_bytes,p->origin,p->origin_bytes,true);
    if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
    try {
        auto owner=fingerprint(p->cookie,p->cookie_bytes);
        for(size_t i=0;i<c->count;++i) {
            const auto& record=c->records[i];
            if(equal(owner,record.owner)&&std::string_view(record.key,record.key_bytes)==std::string_view(intent.key,intent.key_bytes)) {
                if(record.kind!=intent.kind||record.mod_id!=intent.mod||record.file_id!=intent.file||std::string_view(record.domain,record.domain_bytes)!=std::string_view(intent.domain,intent.domain_bytes))
                    return finish(result,out,MB_NEXUS_JOBS_INTENT_CONFLICT);
                status=serialize(public_job(record),p->output,p->output_capacity,result);
                if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
                status=admission(*c,p->cookie,p->cookie_bytes,p->csrf,p->csrf_bytes,p->origin,p->origin_bytes,true);
                if(status!=MB_NEXUS_JOBS_OK){result.output_bytes=0;return finish(result,out,status);}
                counts(*c,result,&owner);output.published=true;return finish(result,out,MB_NEXUS_JOBS_OK);
            }
        }
        if(c->count==c->records.size())return finish(result,out,MB_NEXUS_JOBS_LIMIT_REACHED);
        Record candidate{};
        struct CandidateErase {Record& record;~CandidateErase(){wipe(&record,sizeof(record));}} erase{candidate};
        std::array<uint8_t,32> random{};
        struct RandomErase {std::array<uint8_t,32>& value;~RandomErase(){wipe(value.data(),value.size());}} random_erase{random};
        int entropy_status=-1;try{entropy_status=c->entropy(c->entropy_context,random.data());}catch(...){entropy_status=-1;}
        unsigned present=0;for(auto b:random)present|=b;
        if(entropy_status||!present)return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
        auto id=hex(random.data(),16);std::memcpy(candidate.id,id.data(),id.size());
        for(size_t i=0;i<c->count;++i)if(std::memcmp(candidate.id,c->records[i].id,32)==0)return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
        for(size_t i=0;i<c->generation_count;++i)if(std::memcmp(candidate.id,c->generations[i].id,32)==0)return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
        std::memcpy(candidate.domain,intent.domain,intent.domain_bytes);candidate.domain_bytes=intent.domain_bytes;
        std::memcpy(candidate.key,intent.key,intent.key_bytes);candidate.key_bytes=intent.key_bytes;
        candidate.mod_id=intent.mod;candidate.file_id=intent.file;candidate.owner=owner;candidate.kind=intent.kind;
        std::memcpy(candidate.cookie,p->cookie,43);std::memcpy(candidate.csrf,p->csrf,43);
        std::memcpy(candidate.origin,p->origin,p->origin_bytes);candidate.origin_bytes=p->origin_bytes;
        status=serialize(public_job(candidate),p->output,p->output_capacity,result);
        if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
        status=admission(*c,p->cookie,p->cookie_bytes,p->csrf,p->csrf_bytes,p->origin,p->origin_bytes,true);
        if(status!=MB_NEXUS_JOBS_OK){result.output_bytes=0;return finish(result,out,status);}
        c->records[c->count++]=candidate;counts(*c,result,&owner);output.published=true;return finish(result,out,MB_NEXUS_JOBS_OK);
    }catch(...){result.output_bytes=0;return finish(result,out,MB_NEXUS_JOBS_INTERNAL_ERROR);}
}
extern "C" int mb_nexus_jobs_submit_acquisition_v1(mb_nexus_jobs_context_v1 *c,const mb_nexus_jobs_submit_params_v1 *p,mb_nexus_jobs_result_v1 *out) noexcept {
    if(!valid(c)||!aligned(p)||p->struct_size!=sizeof(*p)||p->abi_version!=MB_NEXUS_JOBS_ABI_V1||
        p->cookie_bytes>43||p->csrf_bytes>43||p->origin_bytes>128||!p->intent_bytes||p->intent_bytes>MB_NEXUS_JOBS_INTENT_LIMIT)return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Regions regions;
    if(!output_layout(p,sizeof(*p),out,p->output,p->output_capacity,regions)||!regions.add(p->cookie,p->cookie_bytes)||
        !regions.add(p->csrf,p->csrf_bytes)||!regions.add(p->origin,p->origin_bytes)||!regions.add(p->intent,p->intent_bytes)||!owned_distinct(*c,regions))return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Output output{p->output,p->output_capacity};auto result=initial();
    if(c->busy)return finish(result,out,MB_NEXUS_JOBS_BUSY);
    Busy busy(*c);
    if(!token(p->cookie,p->cookie_bytes)||!token(p->csrf,p->csrf_bytes)||!p->origin_bytes)return finish(result,out,MB_NEXUS_JOBS_BAD_ARGUMENT);
    if(c->closing||c->storage_failed)return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);
    AcquisitionIntent intent{};
    struct IntentErase {AcquisitionIntent& intent;~IntentErase(){wipe(&intent,sizeof(intent));}} intent_erase{intent};
    auto status=IntentParser(std::string_view(reinterpret_cast<const char *>(p->intent),p->intent_bytes)).parse_acquisition(intent);
    if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
    status=admission(*c,p->cookie,p->cookie_bytes,p->csrf,p->csrf_bytes,p->origin,p->origin_bytes,true);
    if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
    try {
        const auto owner=fingerprint(p->cookie,p->cookie_bytes);
        for(size_t i=0;i<c->acquisition_count;++i) {
            const auto& prior=c->acquisitions[i];
            if(!equal(prior.owner,owner)||std::string_view(prior.intent.key,prior.intent.key_bytes)!=std::string_view(intent.key,intent.key_bytes))continue;
            if(!same_acquisition(prior.intent,intent))return finish(result,out,MB_NEXUS_JOBS_INTENT_CONFLICT);
            status=serialize(public_acquisition(prior),p->output,p->output_capacity,result);
            if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
            status=admission(*c,p->cookie,p->cookie_bytes,p->csrf,p->csrf_bytes,p->origin,p->origin_bytes,true);
            if(status!=MB_NEXUS_JOBS_OK){result.output_bytes=0;return finish(result,out,status);}
            counts(*c,result,&owner);output.published=true;return finish(result,out,MB_NEXUS_JOBS_OK);
        }
        AcquisitionBudget budget;
        if(!budget.start(c->resolver_timeout))return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
        std::unique_ptr<AcquisitionStage,AcquisitionDelete> staged(new AcquisitionStage{});
        staged->group.intent=intent;staged->group.owner=owner;
        {
            auto scratch=std::make_unique<PlanScratch>();
            Json query=acquisition_intent(intent);JsonErase erase_query{query};
            std::string text=query.dump();TextErase erase_text{text};
            uint32_t remaining=0;
            if(text.size()>MB_NEXUS_AUTH_ACQUISITION_INTENT_LIMIT||!budget.remaining(remaining))return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
            mb_nexus_auth_acquisition_plan_params_v1 request{};request.struct_size=sizeof(request);request.abi_version=MB_NEXUS_AUTH_ABI_V1;
            request.cookie=p->cookie;request.cookie_bytes=p->cookie_bytes;request.csrf=p->csrf;request.csrf_bytes=p->csrf_bytes;
            request.origin=p->origin;request.origin_bytes=p->origin_bytes;request.intent=text.data();request.intent_bytes=text.size();
            request.timeout_ms=remaining;request.connect_timeout_ms=std::min(c->connect_timeout,remaining);
            request.output=scratch->bytes.data();request.output_capacity=scratch->bytes.size();
            mb_nexus_auth_result_v1 planned{};
            const int code=mb_nexus_auth_acquisition_plan_v1(c->auth,&request,&planned);
            if(code!=MB_NEXUS_AUTH_OK||planned.status!=MB_NEXUS_AUTH_OK||!planned.complete) {
                Status error=MB_NEXUS_JOBS_SOURCE_FAILED;
                if(code==MB_NEXUS_AUTH_SESSION_REQUIRED)error=MB_NEXUS_JOBS_SESSION_REQUIRED;
                else if(code==MB_NEXUS_AUTH_RECONNECT_REQUIRED||code==MB_NEXUS_AUTH_NOT_CONFIGURED)error=MB_NEXUS_JOBS_AUTH_REQUIRED;
                else if(code==MB_NEXUS_AUTH_REFUSED||code==MB_NEXUS_AUTH_CONTENT_HIDDEN)error=MB_NEXUS_JOBS_REFUSED;
                else if(code==MB_NEXUS_AUTH_BUSY)error=MB_NEXUS_JOBS_BUSY;
                else if(code==MB_NEXUS_AUTH_INTERNAL_ERROR)error=MB_NEXUS_JOBS_INTERNAL_ERROR;
                return finish(result,out,error);
            }
            if(planned.struct_size!=sizeof(planned)||planned.abi_version!=MB_NEXUS_AUTH_ABI_V1||planned.complete!=1||
               !planned.output_bytes||planned.output_bytes>=scratch->bytes.size()||scratch->bytes[planned.output_bytes]!=0||
               std::memchr(scratch->bytes.data(),0,planned.output_bytes))return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
            Json report=Json::parse(scratch->bytes.data(),scratch->bytes.data()+planned.output_bytes);JsonErase erase_report{report};
            status=select_acquisition(report,staged->group);
            if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
        } // Erase/release all provider, plan and JSON/text scratch before final admission gates.
        const uint32_t count=staged->group.source_count;
        if(count>c->records.size()-c->count||c->acquisition_count==c->acquisitions.size())return finish(result,out,MB_NEXUS_JOBS_LIMIT_REACHED);
        uint32_t remaining=0;
        for(uint32_t i=0;i<count;++i) {
            if(!budget.remaining(remaining))return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
            std::array<uint8_t,32> random{};
            struct RandomErase {std::array<uint8_t,32>& value;~RandomErase(){wipe(value.data(),value.size());}} random_erase{random};
            int entropy_status=-1;try{entropy_status=c->entropy(c->entropy_context,random.data());}catch(...){entropy_status=-1;}
            unsigned present=0;for(auto byte:random)present|=byte;
            if(entropy_status||!present)return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
            auto& record=staged->records[i];auto id=hex(random.data(),16);std::memcpy(record.id,id.data(),id.size());
            for(size_t j=0;j<c->count;++j)if(!std::memcmp(record.id,c->records[j].id,32))return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
            for(size_t j=0;j<c->generation_count;++j)if(!std::memcmp(record.id,c->generations[j].id,32))return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
            for(uint32_t j=0;j<i;++j)if(!std::memcmp(record.id,staged->records[j].id,32))return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
            std::memcpy(record.domain,intent.game,intent.game_bytes);record.domain_bytes=intent.game_bytes;
            record.mod_id=staged->group.selected[i].mod;record.file_id=staged->group.selected[i].file;record.owner=owner;
            std::memcpy(record.cookie,p->cookie,43);std::memcpy(record.csrf,p->csrf,43);
            std::memcpy(record.origin,p->origin,p->origin_bytes);record.origin_bytes=p->origin_bytes;
            std::memcpy(staged->group.ids[i],record.id,32);
            // Kind0 is download_analyze; internal child keys remain empty.
        }
        status=serialize(public_acquisition(staged->group),p->output,p->output_capacity,result);
        if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
        status=admission(*c,p->cookie,p->cookie_bytes,p->csrf,p->csrf_bytes,p->origin,p->origin_bytes,true);
        if(status!=MB_NEXUS_JOBS_OK){result.output_bytes=0;return finish(result,out,status);}
        if(c->closing||c->storage_failed||!directory_valid(*c)) {
            c->storage_failed=true;result.output_bytes=0;return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);
        }
        if(!budget.remaining(remaining)){result.output_bytes=0;return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);}
        // No fallible call, allocation, serialization or optional gate after this
        // sample: fixed POD copies/counters and fixed staging erasure only.
        std::memcpy(c->records.data()+c->count,staged->records.data(),count*sizeof(Record));
        c->acquisitions[c->acquisition_count++]=staged->group;c->count+=count;
        counts(*c,result,&owner);output.published=true;return finish(result,out,MB_NEXUS_JOBS_OK);
    }catch(...){result.output_bytes=0;return finish(result,out,MB_NEXUS_JOBS_INTERNAL_ERROR);}
}

extern "C" int mb_nexus_jobs_list_v1(mb_nexus_jobs_context_v1 *c,const mb_nexus_jobs_list_params_v1 *p,mb_nexus_jobs_result_v1 *out) noexcept {
    if(!valid(c)||!aligned(p)||p->struct_size!=sizeof(*p)||p->abi_version!=MB_NEXUS_JOBS_ABI_V1||p->cookie_bytes>43)return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Regions regions;if(!output_layout(p,sizeof(*p),out,p->output,p->output_capacity,regions)||!regions.add(p->cookie,p->cookie_bytes)||!owned_distinct(*c,regions))return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Output output{p->output,p->output_capacity};auto result=initial();
    if(c->busy)return finish(result,out,MB_NEXUS_JOBS_BUSY);Busy busy(*c);
    if(!token(p->cookie,p->cookie_bytes))return finish(result,out,MB_NEXUS_JOBS_BAD_ARGUMENT);
    if(c->closing)return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);
    auto status=admission(*c,p->cookie,p->cookie_bytes);if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
    try {
        auto owner=fingerprint(p->cookie,p->cookie_bytes);Json items=Json::array();
        for(size_t i=0;i<c->count;++i)if(equal(owner,c->records[i].owner))items.push_back(public_job(c->records[i]));
        status=serialize(Json{{"items",std::move(items)}},p->output,p->output_capacity,result);
        if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
        status=admission(*c,p->cookie,p->cookie_bytes);if(status!=MB_NEXUS_JOBS_OK){result.output_bytes=0;return finish(result,out,status);}
        counts(*c,result,&owner);output.published=true;return finish(result,out,MB_NEXUS_JOBS_OK);
    }catch(...){result.output_bytes=0;return finish(result,out,MB_NEXUS_JOBS_INTERNAL_ERROR);}
}

extern "C" int mb_nexus_jobs_get_v1(mb_nexus_jobs_context_v1 *c,const mb_nexus_jobs_get_params_v1 *p,mb_nexus_jobs_result_v1 *out) noexcept {
    if(!valid(c)||!aligned(p)||p->struct_size!=sizeof(*p)||p->abi_version!=MB_NEXUS_JOBS_ABI_V1||p->cookie_bytes>43||p->id_bytes>32)return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Regions regions;if(!output_layout(p,sizeof(*p),out,p->output,p->output_capacity,regions)||!regions.add(p->cookie,p->cookie_bytes)||!regions.add(p->id,p->id_bytes)||!owned_distinct(*c,regions))return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Output output{p->output,p->output_capacity};auto result=initial();
    if(c->busy)return finish(result,out,MB_NEXUS_JOBS_BUSY);Busy busy(*c);
    if(!token(p->cookie,p->cookie_bytes)||!identifier(p->id,p->id_bytes))return finish(result,out,MB_NEXUS_JOBS_BAD_ARGUMENT);
    if(c->closing)return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);
    auto status=admission(*c,p->cookie,p->cookie_bytes);if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
    try {
        auto owner=fingerprint(p->cookie,p->cookie_bytes);
        for(size_t i=0;i<c->count;++i)if(equal(owner,c->records[i].owner)&&std::memcmp(p->id,c->records[i].id,32)==0) {
            status=serialize(public_job(c->records[i]),p->output,p->output_capacity,result);if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
            status=admission(*c,p->cookie,p->cookie_bytes);if(status!=MB_NEXUS_JOBS_OK){result.output_bytes=0;return finish(result,out,status);}
            counts(*c,result,&owner);output.published=true;return finish(result,out,MB_NEXUS_JOBS_OK);
        }
        return finish(result,out,MB_NEXUS_JOBS_NOT_FOUND);
    }catch(...){result.output_bytes=0;return finish(result,out,MB_NEXUS_JOBS_INTERNAL_ERROR);}
}

extern "C" int mb_nexus_jobs_run_one_v1(mb_nexus_jobs_context_v1 *c,mb_nexus_jobs_result_v1 *out) noexcept {
    Regions regions;if(!valid(c)||!aligned(out)||!regions.add(out,sizeof(*out))||!owned_distinct(*c,regions))return MB_NEXUS_JOBS_BAD_ARGUMENT;
    auto result=initial();if(c->busy)return finish(result,out,MB_NEXUS_JOBS_BUSY);Busy busy(*c);
    if(c->closing)return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);
    Record *selected=nullptr;for(size_t i=0;i<c->count;++i)if(c->records[i].state.value==State::Queued){selected=&c->records[i];break;}
    if(!selected) {
        for(size_t i=0;i<c->generation_count;++i)if(c->generations[i].state==Generation::Queued) {
            if(c->storage_failed)return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);
            result.processed=1;auto status=prepare_generation(*c,c->generations[i]);
            generation_counts(*c,result);return finish(result,out,status);
        }
        counts(*c,result);return finish(result,out,MB_NEXUS_JOBS_OK);
    }
    auto& record=*selected;record.state.value=State::Downloading;result.processed=1;
    try {
        if(c->storage_failed){fail_record(*c,record,Error::Storage);counts(*c,result);return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);}
        Scratch scratch;
        mb_nexus_download_params_v1 request{};request.struct_size=sizeof(request);request.abi_version=MB_NEXUS_DOWNLOAD_ABI_V1;
        request.domain=record.domain;request.domain_bytes=record.domain_bytes;request.mod_id=record.mod_id;request.file_id=record.file_id;
        request.timeout_ms=c->resolver_timeout;request.connect_timeout_ms=c->connect_timeout;
        request.raw_body=scratch.raw.get();request.raw_capacity=MB_NEXUS_DOWNLOAD_RAW_LIMIT_BYTES;
        request.metadata_report=scratch.metadata.get();request.report_capacity=MB_NEXUS_DOWNLOAD_REPORT_LIMIT_BYTES;
        request.uri=scratch.uri.data();request.uri_capacity=scratch.uri.size();
        mb_nexus_auth_resolve_params_v1 auth{};auth.struct_size=sizeof(auth);auth.abi_version=MB_NEXUS_AUTH_ABI_V1;
        auth.cookie=record.cookie;auth.cookie_bytes=43;auth.csrf=record.csrf;auth.csrf_bytes=43;auth.origin=record.origin;auth.origin_bytes=record.origin_bytes;auth.download=&request;
        mb_nexus_auth_resolve_result_v1 resolution{};
        auto resolve_status=mb_nexus_auth_resolve_download_v1(c->auth,&auth,&resolution);
        bindings_clear(record);
        if(resolve_status!=MB_NEXUS_AUTH_OK||resolution.auth.status!=MB_NEXUS_AUTH_OK||!resolution.auth.complete) {
            bool authorization=resolve_status==MB_NEXUS_AUTH_REFUSED&&resolution.auth.status==MB_NEXUS_AUTH_REFUSED&&
                resolution.download.status==MB_NEXUS_DOWNLOAD_AUTH_REQUIRED&&
                resolution.download.stage==MB_NEXUS_DOWNLOAD_STAGE_AUTHORIZATION;
            bool hidden=resolve_status==MB_NEXUS_AUTH_REFUSED&&resolution.download.status==MB_NEXUS_DOWNLOAD_CONTENT_HIDDEN;
            auto error=authorization?Error::Authorization:hidden?Error::Hidden:
                resolve_status==MB_NEXUS_AUTH_RECONNECT_REQUIRED||resolve_status==MB_NEXUS_AUTH_NOT_CONFIGURED||resolve_status==MB_NEXUS_AUTH_SESSION_REQUIRED?Error::Account:Error::Resolution;
            fail_record(*c,record,error,authorization);counts(*c,result);return finish(result,out,c->storage_failed?MB_NEXUS_JOBS_STORAGE_FAILED:MB_NEXUS_JOBS_OK);
        }
        if(resolution.download.status!=MB_NEXUS_DOWNLOAD_OK||!resolution.download.complete) {
            bool authorization=resolution.download.status==MB_NEXUS_DOWNLOAD_AUTH_REQUIRED;
            auto error=authorization?Error::Authorization:resolution.download.status==MB_NEXUS_DOWNLOAD_CONTENT_HIDDEN?Error::Hidden:Error::Resolution;
            fail_record(*c,record,error,authorization);counts(*c,result);return finish(result,out,c->storage_failed?MB_NEXUS_JOBS_STORAGE_FAILED:MB_NEXUS_JOBS_OK);
        }
        if(!resolution.download.uri_bytes||resolution.download.uri_bytes>=scratch.uri.size()||scratch.uri[resolution.download.uri_bytes]!=0||
            std::memchr(scratch.uri.data(),0,resolution.download.uri_bytes)||resolution.download.mod_id!=record.mod_id||resolution.download.file_id!=record.file_id||!resolution.download.user_id) {
            fail_record(*c,record,Error::Resolution);counts(*c,result);return finish(result,out,MB_NEXUS_JOBS_OK);
        }
        if(!directory_valid(*c)){c->storage_failed=true;fail_record(*c,record,Error::Storage);counts(*c,result);return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);}
        auto name=leaf(record);
        record.fd=mb_archive_posix_openat(c->directory,name.c_str(),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(record.fd<0){c->storage_failed=true;fail_record(*c,record,Error::Storage);counts(*c,result);return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);}
        record.leaf_owned=true;
        struct stat created{};
        if(fstat(record.fd,&created)==0){record.device=static_cast<uint64_t>(created.st_dev);record.inode=static_cast<uint64_t>(created.st_ino);record.identity_valid=true;}
        if(!record.identity_valid||fchmod(record.fd,0600)!=0||fstat(record.fd,&created)!=0||!file_private(created)||!file_identity(record,created)||created.st_size!=0) {
            fail_record(*c,record,Error::Storage);counts(*c,result);return finish(result,out,c->storage_failed?MB_NEXUS_JOBS_STORAGE_FAILED:MB_NEXUS_JOBS_OK);
        }
        mb_fd_io_v1 io{};io.struct_size=sizeof(io);io.abi_version=MB_FD_IO_ABI_V1;io.byte_limit=c->cap*2;io.call_limit=UINT64_C(131072);
        mb_curl_download_params_v1 download{};download.struct_size=sizeof(download);download.abi_version=MB_CURL_DOWNLOAD_ABI_V1;
        download.url=scratch.uri.data();download.ca_pem=c->ca.data();download.ca_bytes=c->ca.size();download.resolve=c->resolve.empty()?nullptr:c->resolve.c_str();
        download.timeout_ms=c->download_timeout;download.connect_timeout_ms=c->connect_timeout;
        download.sink.struct_size=sizeof(download.sink);download.sink.abi_version=MB_DOWNLOAD_FILE_SINK_ABI_V1;download.sink.fd=record.fd;download.sink.cap_bytes=c->cap;
        download.sink.io_context=&io;download.sink.read_at=mb_fd_io_read_at;download.sink.write_at=mb_fd_io_write_at;
        mb_curl_download_result_v1 transferred{};auto transfer_status=mb_curl_download_v1(&download,&transferred);scratch.clear_uri();
        if(transfer_status!=MB_CURL_DOWNLOAD_OK||!complete_file(transferred,c->cap)) {
            fail_record(*c,record,Error::Download);counts(*c,result);return finish(result,out,c->storage_failed?MB_NEXUS_JOBS_STORAGE_FAILED:MB_NEXUS_JOBS_OK);
        }
        record.downloaded=true;record.bytes=transferred.file.bytes_received;std::copy(std::begin(transferred.file.sha256),std::end(transferred.file.sha256),record.sha.begin());
        if(!close_file(*c,record)){fail_record(*c,record,Error::Storage);counts(*c,result);return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);}
        record.fd=mb_archive_posix_openat(c->directory,name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC,0);
        struct stat readonly{};
        if(record.fd<0||fstat(record.fd,&readonly)!=0||!file_private(readonly)||!file_identity(record,readonly)||static_cast<uint64_t>(readonly.st_size)!=record.bytes) {
            fail_record(*c,record,Error::Storage);counts(*c,result);return finish(result,out,c->storage_failed?MB_NEXUS_JOBS_STORAGE_FAILED:MB_NEXUS_JOBS_OK);
        }
        record.state.value=State::Analyzing;
        mb_archive_analysis_params_v1 analysis{};analysis.struct_size=sizeof(analysis);analysis.abi_version=MB_ARCHIVE_ANALYSIS_ABI_V1;
        analysis.fd=record.fd;analysis.execution_context=c->execution_context;analysis.expected_device=record.device;analysis.expected_inode=record.inode;analysis.expected_bytes=record.bytes;
        std::copy(record.sha.begin(),record.sha.end(),std::begin(analysis.expected_sha256));analysis.timeout_ms=c->analysis_timeout;
        analysis.report=record.report;analysis.report_capacity=sizeof(record.report);
        mb_archive_analysis_result_v1 analyzed{};auto analysis_status=mb_archive_analyze_v1(&analysis,&analyzed);
        if(analysis_status!=MB_ARCHIVE_ANALYSIS_OK||analyzed.status!=MB_ARCHIVE_ANALYSIS_OK||!analyzed.complete||analyzed.installation_supported||analyzed.runtime_supported||
            !analyzed.report_bytes||analyzed.report_bytes>=sizeof(record.report)||record.report[analyzed.report_bytes]!=0||std::memchr(record.report,0,analyzed.report_bytes)||
            !Json::parse(record.report,record.report+analyzed.report_bytes,nullptr,false).is_object()) {
            fail_record(*c,record,Error::Analysis);counts(*c,result);return finish(result,out,c->storage_failed?MB_NEXUS_JOBS_STORAGE_FAILED:MB_NEXUS_JOBS_OK);
        }
        record.report_bytes=analyzed.report_bytes;
        prepare_download(*c,record);
        if(c->storage_failed){fail_record(*c,record,Error::Storage);counts(*c,result);return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);}
        struct stat observed{},named{};
        if(!directory_valid(*c)||fstat(record.fd,&observed)!=0||!file_private(observed)||!file_identity(record,observed)||static_cast<uint64_t>(observed.st_size)!=record.bytes||!close_file(*c,record)) {
            fail_record(*c,record,Error::Storage);counts(*c,result);return finish(result,out,c->storage_failed?MB_NEXUS_JOBS_STORAGE_FAILED:MB_NEXUS_JOBS_OK);
        }
        if(mb_archive_posix_fstatat(c->directory,name.c_str(),&named,AT_SYMLINK_NOFOLLOW)!=0||!file_private(named)||!file_identity(record,named)||static_cast<uint64_t>(named.st_size)!=record.bytes) {
            fail_record(*c,record,Error::Storage);counts(*c,result);return finish(result,out,c->storage_failed?MB_NEXUS_JOBS_STORAGE_FAILED:MB_NEXUS_JOBS_OK);
        }
        record.state.value=State::Analyzed;record.error=Error::None;counts(*c,result);return finish(result,out,MB_NEXUS_JOBS_OK);
    }catch(...){fail_record(*c,record,Error::Internal);counts(*c,result);return finish(result,out,c->storage_failed?MB_NEXUS_JOBS_STORAGE_FAILED:MB_NEXUS_JOBS_OK);}
}

extern "C" int mb_nexus_generations_submit_v1(mb_nexus_jobs_context_v1 *c,const mb_nexus_jobs_submit_params_v1 *p,mb_nexus_jobs_result_v1 *out) noexcept {
    if(!valid(c)||!aligned(p)||p->struct_size!=sizeof(*p)||p->abi_version!=MB_NEXUS_JOBS_ABI_V1||
        p->cookie_bytes>43||p->csrf_bytes>43||p->origin_bytes>128||!p->intent_bytes||p->intent_bytes>MB_NEXUS_JOBS_INTENT_LIMIT)return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Regions regions;
    if(!output_layout(p,sizeof(*p),out,p->output,p->output_capacity,regions)||!regions.add(p->cookie,p->cookie_bytes)||
        !regions.add(p->csrf,p->csrf_bytes)||!regions.add(p->origin,p->origin_bytes)||!regions.add(p->intent,p->intent_bytes)||!owned_distinct(*c,regions))return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Output output{p->output,p->output_capacity};auto result=initial();
    if(c->busy)return finish(result,out,MB_NEXUS_JOBS_BUSY);
    Busy busy(*c);
    if(!token(p->cookie,p->cookie_bytes)||!token(p->csrf,p->csrf_bytes)||!p->origin_bytes)return finish(result,out,MB_NEXUS_JOBS_BAD_ARGUMENT);
    if(c->closing||c->storage_failed)return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);
    GenerationIntent intent{};
    auto status=IntentParser(std::string_view(reinterpret_cast<const char *>(p->intent),p->intent_bytes)).parse_generation(intent);
    if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
    status=admission(*c,p->cookie,p->cookie_bytes,p->csrf,p->csrf_bytes,p->origin,p->origin_bytes);
    if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
#if MB_HOST_PAK_PREPARATION
    if(!c->iostore_configured)return finish(result,out,MB_NEXUS_JOBS_REFUSED);
    try {
        Generation candidate{};
        struct CandidateErase {Generation& generation;~CandidateErase(){wipe(&generation,sizeof(generation));}} erase{candidate};
        candidate.owner=fingerprint(p->cookie,p->cookie_bytes);candidate.base_token=c->iostore_base.token;
        std::memcpy(candidate.key,intent.key,intent.key_bytes);candidate.key_bytes=intent.key_bytes;
        candidate.source_count=intent.source_count;
        for(uint32_t i=0;i<intent.source_count;++i) {
            const Record *source=nullptr;uint32_t source_index=0;
            for(size_t j=0;j<c->count;++j)if(equal(candidate.owner,c->records[j].owner)&&!std::memcmp(intent.ids[i],c->records[j].id,32)) {
                source=&c->records[j];source_index=static_cast<uint32_t>(j);break;
            }
            if(!source)return finish(result,out,MB_NEXUS_JOBS_NOT_FOUND);
            if(source->state.value!=State::Analyzed||!source->downloaded||!source->leaf_owned||!source->identity_valid||
               !source->report_bytes||!source->bytes||source->bytes>MB_PAK_ARCHIVE_JOB_MAX_INPUT_BYTES||
               c->iostore_game!=std::string_view(source->domain,source->domain_bytes))return finish(result,out,MB_NEXUS_JOBS_REFUSED);
            if(i==0){std::memcpy(candidate.game,source->domain,source->domain_bytes);candidate.game_bytes=source->domain_bytes;}
            if(std::string_view(candidate.game,candidate.game_bytes)!=std::string_view(source->domain,source->domain_bytes))return finish(result,out,MB_NEXUS_JOBS_REFUSED);
            std::memcpy(candidate.source_ids[i],source->id,32);candidate.source_indices[i]=source_index;
            candidate.source_bytes[i]=source->bytes;candidate.source_sha[i]=source->sha;
        }
        for(size_t i=0;i<c->generation_count;++i) {
            const auto& prior=c->generations[i];
            if(!equal(candidate.owner,prior.owner)||std::string_view(prior.key,prior.key_bytes)!=std::string_view(candidate.key,candidate.key_bytes))continue;
            if(candidate.source_count!=prior.source_count||candidate.base_token!=prior.base_token)return finish(result,out,MB_NEXUS_JOBS_INTENT_CONFLICT);
            for(uint32_t j=0;j<candidate.source_count;++j)
                if(std::memcmp(candidate.source_ids[j],prior.source_ids[j],32)||candidate.source_bytes[j]!=prior.source_bytes[j]||candidate.source_sha[j]!=prior.source_sha[j])
                    return finish(result,out,MB_NEXUS_JOBS_INTENT_CONFLICT);
            status=serialize(public_generation(prior),p->output,p->output_capacity,result);
            if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
            status=admission(*c,p->cookie,p->cookie_bytes,p->csrf,p->csrf_bytes,p->origin,p->origin_bytes);
            if(status!=MB_NEXUS_JOBS_OK){result.output_bytes=0;return finish(result,out,status);}
            generation_counts(*c,result,&candidate.owner);output.published=true;return finish(result,out,MB_NEXUS_JOBS_OK);
        }
        if(c->generation_count==c->generations.size())return finish(result,out,MB_NEXUS_JOBS_LIMIT_REACHED);
        std::array<uint8_t,32> random{};
        struct RandomErase {std::array<uint8_t,32>& value;~RandomErase(){wipe(value.data(),value.size());}} random_erase{random};
        int entropy_status=-1;try{entropy_status=c->entropy(c->entropy_context,random.data());}catch(...){entropy_status=-1;}
        unsigned present=0;for(auto b:random)present|=b;
        if(entropy_status||!present)return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
        auto id=hex(random.data(),16);std::memcpy(candidate.id,id.data(),id.size());
        for(size_t i=0;i<c->count;++i)if(!std::memcmp(candidate.id,c->records[i].id,32))return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
        for(size_t i=0;i<c->generation_count;++i)if(!std::memcmp(candidate.id,c->generations[i].id,32))return finish(result,out,MB_NEXUS_JOBS_SOURCE_FAILED);
        status=serialize(public_generation(candidate),p->output,p->output_capacity,result);
        if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
        status=admission(*c,p->cookie,p->cookie_bytes,p->csrf,p->csrf_bytes,p->origin,p->origin_bytes);
        if(status!=MB_NEXUS_JOBS_OK){result.output_bytes=0;return finish(result,out,status);}
        c->generations[c->generation_count++]=candidate;
        generation_counts(*c,result,&candidate.owner);output.published=true;return finish(result,out,MB_NEXUS_JOBS_OK);
    }catch(...){result.output_bytes=0;return finish(result,out,MB_NEXUS_JOBS_INTERNAL_ERROR);}
#else
    return finish(result,out,MB_NEXUS_JOBS_REFUSED);
#endif
}

extern "C" int mb_nexus_generations_list_v1(mb_nexus_jobs_context_v1 *c,const mb_nexus_jobs_list_params_v1 *p,mb_nexus_jobs_result_v1 *out) noexcept {
    if(!valid(c)||!aligned(p)||p->struct_size!=sizeof(*p)||p->abi_version!=MB_NEXUS_JOBS_ABI_V1||p->cookie_bytes>43)return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Regions regions;
    if(!output_layout(p,sizeof(*p),out,p->output,p->output_capacity,regions)||!regions.add(p->cookie,p->cookie_bytes)||!owned_distinct(*c,regions))return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Output output{p->output,p->output_capacity};auto result=initial();
    if(c->busy)return finish(result,out,MB_NEXUS_JOBS_BUSY);
    Busy busy(*c);
    if(!token(p->cookie,p->cookie_bytes))return finish(result,out,MB_NEXUS_JOBS_BAD_ARGUMENT);
    if(c->closing)return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);
    auto status=admission(*c,p->cookie,p->cookie_bytes);if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
    try {
        auto owner=fingerprint(p->cookie,p->cookie_bytes);Json items=Json::array();
        for(size_t i=0;i<c->generation_count;++i)if(equal(owner,c->generations[i].owner))items.push_back(public_generation(c->generations[i]));
        status=serialize(Json{{"items",std::move(items)}},p->output,p->output_capacity,result);
        if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
        status=admission(*c,p->cookie,p->cookie_bytes);if(status!=MB_NEXUS_JOBS_OK){result.output_bytes=0;return finish(result,out,status);}
        generation_counts(*c,result,&owner);output.published=true;return finish(result,out,MB_NEXUS_JOBS_OK);
    }catch(...){result.output_bytes=0;return finish(result,out,MB_NEXUS_JOBS_INTERNAL_ERROR);}
}

extern "C" int mb_nexus_generations_get_v1(mb_nexus_jobs_context_v1 *c,const mb_nexus_jobs_get_params_v1 *p,mb_nexus_jobs_result_v1 *out) noexcept {
    if(!valid(c)||!aligned(p)||p->struct_size!=sizeof(*p)||p->abi_version!=MB_NEXUS_JOBS_ABI_V1||p->cookie_bytes>43||p->id_bytes>32)return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Regions regions;
    if(!output_layout(p,sizeof(*p),out,p->output,p->output_capacity,regions)||!regions.add(p->cookie,p->cookie_bytes)||!regions.add(p->id,p->id_bytes)||!owned_distinct(*c,regions))return MB_NEXUS_JOBS_BAD_ARGUMENT;
    Output output{p->output,p->output_capacity};auto result=initial();
    if(c->busy)return finish(result,out,MB_NEXUS_JOBS_BUSY);
    Busy busy(*c);
    if(!token(p->cookie,p->cookie_bytes)||!identifier(p->id,p->id_bytes))return finish(result,out,MB_NEXUS_JOBS_BAD_ARGUMENT);
    if(c->closing)return finish(result,out,MB_NEXUS_JOBS_STORAGE_FAILED);
    auto status=admission(*c,p->cookie,p->cookie_bytes);if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
    try {
        auto owner=fingerprint(p->cookie,p->cookie_bytes);
        for(size_t i=0;i<c->generation_count;++i)if(equal(owner,c->generations[i].owner)&&!std::memcmp(p->id,c->generations[i].id,32)) {
            status=serialize(public_generation(c->generations[i]),p->output,p->output_capacity,result);
            if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
            status=admission(*c,p->cookie,p->cookie_bytes);if(status!=MB_NEXUS_JOBS_OK){result.output_bytes=0;return finish(result,out,status);}
            generation_counts(*c,result,&owner);output.published=true;return finish(result,out,MB_NEXUS_JOBS_OK);
        }
        return finish(result,out,MB_NEXUS_JOBS_NOT_FOUND);
    }catch(...){result.output_bytes=0;return finish(result,out,MB_NEXUS_JOBS_INTERNAL_ERROR);}
}
