#include "auth.h"
#include "oauth_code.h"
#include "nexus_client.h"
#include "modbridge/core.hpp"
#include "modbridge/nexus.hpp"
#include "modbridge/nexus_requirements_query.h"
#include <picosha2/picosha2.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <time.h>
#include <vector>

namespace {
using Status = mb_nexus_auth_status_v1;
constexpr uint64_t second = 1000000000;
constexpr uint64_t millisecond = 1000000;
constexpr uint64_t magic = UINT64_C(0x4d42415554483101);
void wipe(void *p, size_t bytes) noexcept {
    auto *out = static_cast<volatile unsigned char *>(p);
    for (size_t i=0;i<bytes;++i) out[i]=0;
}
struct TokenDelete { void operator()(mb_nexus_oauth_tokens_v1 *p) const noexcept {
    if(p) { wipe(p,sizeof(*p)); delete p; }
}};
using Tokens = std::unique_ptr<mb_nexus_oauth_tokens_v1,TokenDelete>;
struct Session { uint64_t expires=0; char cookie[44]{},csrf[44]{}; };
struct Pending { uint64_t expires=0; char cookie[44]{},state[44]{},verifier[44]{}; };
struct Range { uintptr_t begin,end; };
bool range(const void *p,size_t bytes,Range& r) noexcept {
    const auto a=reinterpret_cast<uintptr_t>(p);
    if((!p&&bytes)||bytes>UINTPTR_MAX-a) return false;
    r={a,a+bytes};return true;
}
bool overlap(Range a,Range b) noexcept {
    return a.begin!=a.end&&b.begin!=b.end&&a.begin<b.end&&b.begin<a.end;
}
struct Regions {
    std::array<Range,24> ranges{};size_t count=0;
    bool add(const void *p,size_t n) noexcept {
        if(count==ranges.size()||!range(p,n,ranges[count])) return false;
        ++count;return true;
    }
    bool distinct() const noexcept {
        for(size_t i=0;i<count;++i) for(size_t j=i+1;j<count;++j)
            if(overlap(ranges[i],ranges[j])) return false;
        return true;
    }
    bool outside(const void *p,size_t n) const noexcept {
        Range r{};if(!range(p,n,r)) return false;
        for(size_t i=0;i<count;++i) if(overlap(r,ranges[i])) return false;
        return true;
    }
};
struct Output {
    char *p;size_t capacity;bool committed=false;
    ~Output(){if(!committed)wipe(p,capacity);}
};
struct TextErase {
    std::string& value;
    ~TextErase(){wipe(value.data(),value.size());}
};
bool ascii(std::string_view text,bool space=false) noexcept {
    for(unsigned char c:text)if(c<(space?32:33)||c>126)return false;
    return true;
}
bool token(std::string_view text) noexcept {
    if(text.size()!=43)return false;
    for(unsigned char c:text)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;
    return true;
}
bool equal(std::string_view a,std::string_view b) noexcept {
    if(a.size()!=b.size())return false;
    unsigned difference=0;for(size_t i=0;i<a.size();++i)difference|=static_cast<unsigned char>(a[i])^static_cast<unsigned char>(b[i]);
    return difference==0;
}
bool origin_valid(std::string_view value) noexcept {
    if(value.empty()||!ascii(value)||value.size()>MB_NEXUS_AUTH_ORIGIN_LIMIT)return false;
    const auto prefix=value.starts_with("http://")?7u:value.starts_with("https://")?8u:0u;
    if(!prefix||value.size()<=prefix)return false;
    auto host=value.substr(prefix);
    for(char c:host)if(c=='/'||c=='@'||c=='?'||c=='#'||c=='\\'||c=='%')return false;
    std::string_view port;
    if(host.front()=='[') {
        auto close=host.find(']');if(close==std::string_view::npos||close<2)return false;
        for(char c:host.substr(1,close-1))if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')||c==':'||c=='.'))return false;
        if(close+1<host.size()){if(host[close+1]!=':')return false;port=host.substr(close+2);}
    } else {
        const auto colon=host.find(':');if(colon!=std::string_view::npos){port=host.substr(colon+1);host=host.substr(0,colon);}
        if(host.empty())return false;
        for(unsigned char c:host)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='.'))return false;
    }
    if(value.back()==':')return false;
    if(!port.empty()) {
        if(port.size()>5||port.front()=='0')return false;
        unsigned n=0;for(char c:port){if(c<'0'||c>'9')return false;n=n*10+static_cast<unsigned>(c-'0');}
        if(!n||n>65535)return false;
    }
    return true;
}
bool resolve_valid(std::string_view value,std::string_view host) noexcept {
    if(value.empty())return true;
    if(value.size()>=MB_NEXUS_AUTH_RESOLVE_LIMIT||!ascii(value)||
        !value.starts_with(host)||value.substr(host.size(),5)!=":443:"||value.size()<=host.size()+5)return false;
    for(char c:value)if(c=='/'||c=='@'||c=='?'||c=='#'||c==';'||c=='*'||c=='\\')return false;
    return true;
}
std::string_view view(const char *p,size_t n) noexcept {return n?std::string_view(p,n):std::string_view{};}
void base64url(const uint8_t *data,size_t bytes,char *out) noexcept {
    constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    size_t pos=0;
    for(size_t i=0;i<bytes;i+=3) {
        uint32_t x=static_cast<uint32_t>(data[i])<<16;
        if(i+1<bytes)x|=static_cast<uint32_t>(data[i+1])<<8;
        if(i+2<bytes)x|=data[i+2];
        out[pos++]=alphabet[(x>>18)&63];out[pos++]=alphabet[(x>>12)&63];
        if(i+1<bytes)out[pos++]=alphabet[(x>>6)&63];if(i+2<bytes)out[pos++]=alphabet[x&63];
    }out[pos]=0;
}
void encode(std::string& out,std::string_view input) {
    constexpr char hex[]="0123456789ABCDEF";
    for(unsigned char c:input) {
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out.push_back(static_cast<char>(c));
        else{out.push_back('%');out.push_back(hex[c>>4]);out.push_back(hex[c&15]);}
    }
}
struct Work {
    std::array<char,MB_NEXUS_OAUTH_FORM_LIMIT_BYTES> form{};
    std::array<uint8_t,MB_NEXUS_OAUTH_RAW_LIMIT_BYTES> raw{};
    std::array<char,16384> report{};
    ~Work(){wipe(this,sizeof(*this));}
};
struct CatalogWork {
    std::array<uint8_t,MB_NEXUS_INPUT_LIMIT_BYTES> raw{};
    std::array<char,MB_NEXUS_OUTPUT_LIMIT_BYTES> report{};
    void clear() noexcept {wipe(this,sizeof(*this));}
    ~CatalogWork(){clear();}
};
struct AcquisitionIntent {
    std::string game;
    std::vector<uint64_t> root_mod_ids;
    std::map<uint64_t,uint64_t> file_choices;
    bool unique_current_primary=false;
};
bool catalog_domain(std::string_view value) noexcept;
struct IntentFrame { bool object=false;std::set<std::string> keys; };
bool exact_json_keys(const modbridge::Json& value,
                     std::initializer_list<std::string_view> expected) {
    if(!value.is_object()||value.size()!=expected.size())return false;
    for(const auto key:expected)if(!value.contains(std::string(key)))return false;
    return true;
}
bool positive_json_integer(const modbridge::Json& value,uint64_t maximum,uint64_t& result) noexcept {
    if(value.is_number_unsigned())result=value.get<uint64_t>();
    else if(value.type()==modbridge::Json::value_t::number_integer) {
        const auto signed_value=value.get<int64_t>();if(signed_value<=0)return false;
        result=static_cast<uint64_t>(signed_value);
    } else return false;
    return result>0&&result<=maximum;
}
bool parse_acquisition_intent(const char *data,size_t bytes,AcquisitionIntent& intent) {
    if(!data||!bytes||bytes>MB_NEXUS_AUTH_ACQUISITION_INTENT_LIMIT)return false;
    std::vector<IntentFrame> frames;size_t events=0;
    auto gate=[&](int depth,modbridge::Json::parse_event_t event,modbridge::Json& value) {
        if(depth>8||++events>2048)throw std::length_error("acquisition intent limit");
        using Event=modbridge::Json::parse_event_t;
        switch(event) {
        case Event::object_start: frames.push_back({true,{}});break;
        case Event::array_start: frames.push_back({false,{}});break;
        case Event::key:
            if(frames.empty()||!frames.back().object||
               !frames.back().keys.insert(value.get<std::string>()).second)
                throw std::invalid_argument("duplicate acquisition intent key");
            break;
        case Event::object_end:
        case Event::array_end:
            if(frames.empty())throw std::invalid_argument("invalid acquisition intent");
            frames.pop_back();break;
        case Event::value: break;
        }
        return true;
    };
    try {
        const auto value=modbridge::Json::parse(data,data+bytes,gate);
        if(!exact_json_keys(value,{"game","mod_ids","file_choices","file_policy"})||
           !value.at("game").is_string()||!value.at("mod_ids").is_array()||
           value.at("mod_ids").empty()||value.at("mod_ids").size()>8||
           !value.at("file_choices").is_array()||value.at("file_choices").size()>8||
           !value.at("file_policy").is_string())return false;
        const auto& game=value.at("game").get_ref<const std::string&>();
        if(!catalog_domain(game))return false;
        const auto& policy=value.at("file_policy").get_ref<const std::string&>();
        if(policy!="explicit_only"&&policy!="unique_current_primary")return false;
        AcquisitionIntent parsed;parsed.game=game;parsed.unique_current_primary=policy=="unique_current_primary";
        std::set<uint64_t> roots;
        for(const auto& item:value.at("mod_ids")) {
            uint64_t id=0;if(!positive_json_integer(item,static_cast<uint64_t>(INT32_MAX),id)||!roots.insert(id).second)return false;
            parsed.root_mod_ids.push_back(id);
        }
        std::sort(parsed.root_mod_ids.begin(),parsed.root_mod_ids.end());
        for(const auto& item:value.at("file_choices")) {
            if(!exact_json_keys(item,{"mod_id","file_id"}))return false;
            uint64_t mod_id=0,file_id=0;
            if(!positive_json_integer(item.at("mod_id"),static_cast<uint64_t>(INT32_MAX),mod_id)||
               !positive_json_integer(item.at("file_id"),static_cast<uint64_t>(INT64_MAX),file_id)||
               !parsed.file_choices.emplace(mod_id,file_id).second)return false;
        }
        intent=std::move(parsed);return true;
    } catch(...) {return false;}
}
void scrub_json_strings(modbridge::Json& value) noexcept {
    if(value.is_string()) {
        auto& text=value.get_ref<std::string&>();wipe(text.data(),text.size());
    } else if(value.is_structured())for(auto& child:value)scrub_json_strings(child);
}
struct JsonErase {
    modbridge::Json& value;
    ~JsonErase(){scrub_json_strings(value);}
};
struct ParsedCallback {
    char state[44]{},code[MB_NEXUS_OAUTH_CODE_LIMIT_BYTES+1]{};
    size_t code_bytes=0;bool denied=false,cleared=false;
    void clear() noexcept {wipe(this,sizeof(*this));cleared=true;}
    ~ParsedCallback(){if(!cleared)clear();}
};
int digit(char c) noexcept {return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;}
bool decode(std::string_view input,char *out,size_t capacity,size_t& written) noexcept {
    written=0;
    for(size_t i=0;i<input.size();++i) {
        unsigned char c=static_cast<unsigned char>(input[i]);
        if(c=='%'){if(i+2>=input.size())return false;int a=digit(input[i+1]),b=digit(input[i+2]);if(a<0||b<0)return false;c=static_cast<unsigned char>(a*16+b);i+=2;}
        else if(c=='+')c=' ';
        if(c<33||c>126||written+1>=capacity)return false;out[written++]=static_cast<char>(c);
    }out[written]=0;return written!=0;
}
bool parse_callback(std::string_view target,ParsedCallback& parsed) noexcept {
    constexpr std::string_view prefix="/auth/callback?";
    if(!target.starts_with(prefix)||!ascii(target)||target.find('#')!=std::string_view::npos||target.find('\\')!=std::string_view::npos)return false;
    auto query=target.substr(prefix.size());bool state=false,value=false;unsigned fields=0;
    while(!query.empty()) {
        const auto split=query.find('&');const auto pair=query.substr(0,split);const auto equals=pair.find('=');
        if(equals==std::string_view::npos||++fields>2)return false;
        char key[6]{};size_t key_bytes=0;
        if(!decode(pair.substr(0,equals),key,sizeof(key),key_bytes))return false;
        if(std::string_view(key,key_bytes)=="state") {
            size_t n=0;if(state||!decode(pair.substr(equals+1),parsed.state,sizeof(parsed.state),n)||!token(view(parsed.state,n)))return false;state=true;
        } else if(std::string_view(key,key_bytes)=="code") {
            if(value||!decode(pair.substr(equals+1),parsed.code,sizeof(parsed.code),parsed.code_bytes))return false;value=true;
        } else if(std::string_view(key,key_bytes)=="error") {
            char error[14]{};size_t n=0;if(value||!decode(pair.substr(equals+1),error,sizeof(error),n)||view(error,n)!="access_denied")return false;
            parsed.denied=true;value=true;
        } else return false;
        if(split==std::string_view::npos)break;query.remove_prefix(split+1);if(query.empty())return false;
    }return state&&value&&fields==2;
}
} // namespace

struct mb_nexus_auth_context_v1 {
    uint64_t identity=magic,last_mono=0;bool have_mono=false,busy=false,configured=false,source_failed=false;
    std::string origin,client,redirect,scope,api_resolve,users_resolve;
    std::vector<uint8_t> ca;
    void *entropy_context=nullptr;mb_nexus_auth_entropy_fn_v1 entropy_read32=nullptr;
    std::array<Session,16> sessions{};Pending pending{};Tokens active;
    ~mb_nexus_auth_context_v1(){active.reset();wipe(&pending,sizeof(pending));wipe(sessions.data(),sizeof(sessions));identity=0;}
};

namespace {
bool owned_distinct(const Regions& inputs,const mb_nexus_auth_context_v1& c) noexcept {
    if(!inputs.distinct()||!inputs.outside(&c,sizeof(c)))return false;
    for(const auto *s:{&c.origin,&c.client,&c.redirect,&c.scope,&c.api_resolve,&c.users_resolve})
        if(!inputs.outside(s->data(),s->size()+1))return false;
    return inputs.outside(c.ca.data(),c.ca.size())&&(!c.active||inputs.outside(c.active.get(),sizeof(*c.active)));
}
template<class P,class R> bool base_layout(mb_nexus_auth_context_v1 *c,const P *p,R *r,Regions& spans) noexcept {
    Range ignored{};
    if(!c||!p||!r||!range(c,sizeof(*c),ignored)||!range(p,sizeof(*p),ignored)||!range(r,sizeof(*r),ignored)||
        reinterpret_cast<uintptr_t>(c)%alignof(mb_nexus_auth_context_v1)||reinterpret_cast<uintptr_t>(p)%alignof(P)||reinterpret_cast<uintptr_t>(r)%alignof(R)||
        c->identity!=magic||p->struct_size!=sizeof(*p)||p->abi_version!=MB_NEXUS_AUTH_ABI_V1)return false;
    return spans.add(p,sizeof(*p))&&spans.add(r,sizeof(*r));
}
bool add_cookie(Regions& r,const char *p,size_t n) noexcept {return n<=43&&r.add(p,n);}
bool add_output(Regions& r,char *p,size_t n) noexcept {return n&&n<=MB_NEXUS_AUTH_OUTPUT_LIMIT&&r.add(p,n);}
bool add_catalog_output(Regions& r,char *p,size_t n) noexcept {return n&&n<=MB_NEXUS_AUTH_CATALOG_OUTPUT_LIMIT&&r.add(p,n);}
bool add_acquisition_output(Regions& r,char *p,size_t n) noexcept {
    return n&&n<=MB_NEXUS_AUTH_ACQUISITION_OUTPUT_LIMIT&&r.add(p,n);
}
bool add_binding(Regions& r,const char *cookie,size_t cn,const char *csrf,size_t sn,const char *origin,size_t on) noexcept {
    return add_cookie(r,cookie,cn)&&sn<=43&&on<=MB_NEXUS_AUTH_ORIGIN_LIMIT&&r.add(csrf,sn)&&r.add(origin,on);
}
bool callback_context(const Regions& r,void *p,const mb_nexus_auth_context_v1 *c=nullptr) noexcept {
    if(!p)return true;
    // The opaque object's full extent remains its owner's contract. Its start
    // cannot be within any specified span or the context's mutable ownership.
    if(!r.outside(p,1))return false;
    if(!c)return true;
    Regions context;return context.add(p,1)&&owned_distinct(context,*c);
}
bool now(mb_nexus_auth_context_v1& c,uint64_t& value) noexcept {
    if(c.source_failed)return false;
    timespec t{};errno=0;int rc=clock_gettime(CLOCK_MONOTONIC,&t);int error=errno;
    if(rc||error||t.tv_sec<0||t.tv_nsec<0||t.tv_nsec>=static_cast<long>(second)||
        static_cast<uint64_t>(t.tv_sec)>(UINT64_MAX-second+1)/second){c.source_failed=true;return false;}
    value=static_cast<uint64_t>(t.tv_sec)*second+static_cast<uint64_t>(t.tv_nsec);
    if(c.have_mono&&value<c.last_mono){c.source_failed=true;return false;}
    c.last_mono=value;c.have_mono=true;return true;
}
bool expiry(uint64_t start,uint64_t seconds,uint64_t& out) noexcept {
    if(seconds>(UINT64_MAX-start)/second)return false;out=start+seconds*second;return true;
}
bool fresh(mb_nexus_auth_context_v1& c,char out[44]) noexcept {
    uint8_t data[32]{};int rc=-1;
    try{rc=c.entropy_read32(c.entropy_context,data);}catch(...){rc=-1;}
    if(!rc)base64url(data,sizeof(data),out);wipe(data,sizeof(data));
    if(rc){wipe(out,44);c.source_failed=true;return false;}return true;
}
void expire(mb_nexus_auth_context_v1& c,uint64_t value) noexcept {
    for(auto& s:c.sessions)if(s.expires&&value>=s.expires)wipe(&s,sizeof(s));
    if(c.pending.expires&&value>=c.pending.expires)wipe(&c.pending,sizeof(c.pending));
}
Session *session(mb_nexus_auth_context_v1& c,std::string_view cookie,uint64_t value) noexcept {
    if(!token(cookie))return nullptr;
    for(auto& s:c.sessions)if(s.expires>value&&equal(cookie,view(s.cookie,43)))return &s;
    return nullptr;
}
Status binding(mb_nexus_auth_context_v1& c,std::string_view cookie,std::string_view csrf,std::string_view origin,uint64_t value) noexcept {
    auto *s=session(c,cookie,value);if(!s)return MB_NEXUS_AUTH_SESSION_REQUIRED;
    if(origin!=c.origin||!token(csrf)||!equal(csrf,view(s->csrf,43)))return MB_NEXUS_AUTH_REFUSED;
    return MB_NEXUS_AUTH_OK;
}
struct Busy {mb_nexus_auth_context_v1& c;explicit Busy(mb_nexus_auth_context_v1& v):c(v){c.busy=true;}~Busy(){c.busy=false;}};
mb_nexus_auth_result_v1 result(const mb_nexus_auth_context_v1& c,Status status) noexcept {
    mb_nexus_auth_result_v1 r{};r.struct_size=sizeof(r);r.abi_version=1;r.status=status;r.configured=c.configured;r.credentials_present=c.active!=nullptr;return r;
}
int finish(mb_nexus_auth_context_v1& c,mb_nexus_auth_result_v1 *out,Status status,size_t bytes=0,bool new_session=false) noexcept {
    auto r=result(c,status);if(status==MB_NEXUS_AUTH_OK){r.complete=1;r.output_bytes=bytes;r.new_session=new_session;}*out=r;return status;
}
struct Operation {
    mb_nexus_auth_context_v1& c;uint32_t timeout,connect;void *cancel_context;mb_nexus_auth_cancel_fn_v1 cancel;
    uint64_t deadline=0;Status fault=MB_NEXUS_AUTH_OK;bool started=false;
    bool sample(uint64_t& value) noexcept {
        if(!now(c,value)){fault=MB_NEXUS_AUTH_SOURCE_FAILED;return false;}
        if(started&&value>=deadline)fault=MB_NEXUS_AUTH_TIMEOUT;return fault==MB_NEXUS_AUTH_OK;
    }
    bool start() noexcept {uint64_t t=0;if(!sample(t))return false;if(timeout>(UINT64_MAX-t)/millisecond){fault=MB_NEXUS_AUTH_SOURCE_FAILED;return false;}deadline=t+timeout*millisecond;started=true;return true;}
    bool check(uint64_t received=0) noexcept {
        uint64_t t=0;if(!sample(t))return false;
        if(cancel){try{if(cancel(cancel_context,received))fault=MB_NEXUS_AUTH_CANCELLED;}catch(...){fault=MB_NEXUS_AUTH_CANCELLED;}}
        return sample(t);
    }
    bool remaining(uint32_t& total,uint32_t& connection) noexcept {
        if(!check())return false;uint64_t ns=deadline-c.last_mono;total=static_cast<uint32_t>(ns/millisecond+(ns%millisecond!=0));connection=std::min(connect,total);return total&&connection;
    }
};
int cancelled(void *p,uint64_t received) noexcept {return static_cast<Operation*>(p)->check(received)?0:1;}
bool network(uint32_t timeout,uint32_t connect) noexcept {return timeout&&timeout<=30000&&connect&&connect<=timeout;}
Status token_gate(Operation& op,const mb_nexus_oauth_tokens_v1& t) noexcept {
    if(!op.check())return op.fault;return op.c.last_mono<t.expires_at_monotonic_ns?MB_NEXUS_AUTH_OK:MB_NEXUS_AUTH_RECONNECT_REQUIRED;
}
bool valid_tokens(const mb_nexus_oauth_tokens_v1& t) noexcept {
    return t.access_token_bytes&&t.access_token_bytes<=16384&&t.refresh_token_bytes&&t.refresh_token_bytes<=16384&&
        t.expires_in_seconds&&t.expires_in_seconds<=31536000&&!t.reserved&&t.expires_at_monotonic_ns&&
        !t.access_token[t.access_token_bytes]&&!t.refresh_token[t.refresh_token_bytes]&&
        ascii(view(t.access_token,t.access_token_bytes))&&ascii(view(t.refresh_token,t.refresh_token_bytes));
}
bool public_profile(const char *data,size_t bytes,size_t capacity,modbridge::Json& profile) {
    if(!bytes||bytes>=capacity||data[bytes])return false;
    size_t nodes=0;auto gate=[&nodes](int depth,modbridge::Json::parse_event_t,modbridge::Json&){if(depth>16||++nodes>4096)throw std::length_error("profile limit");return true;};
    try{
        const auto report=modbridge::Json::parse(data,data+bytes,gate);
        if(!report.is_object()||report.value("status",std::string{})!="decoded"||!report.contains("profile"))return false;
        const auto& p=report.at("profile");if(!p.is_object())return false;
        const auto& id=p.at("user_id");uint64_t user=0;
        if(id.is_number_unsigned())user=id.get<uint64_t>();else if(id.type()==modbridge::Json::value_t::number_integer&&id.get<int64_t>()>0)user=static_cast<uint64_t>(id.get<int64_t>());else return false;
        const auto& name=p.at("display_name");const auto& premium=p.at("is_premium");const auto& supporter=p.at("is_supporter");const auto& adult=p.at("adult_content");
        if(!user||user>INT64_MAX||!name.is_string()||!premium.is_boolean()||!supporter.is_boolean()||(!adult.is_null()&&!adult.is_boolean()))return false;
        const auto& text=name.get_ref<const std::string&>();size_t chars=0;
        for(unsigned char ch:text){if(ch<32&&ch!='\n'&&ch!='\r'&&ch!='\t')return false;if((ch&0xc0)!=0x80)++chars;}if(chars>256)return false;
        profile={{"user_id",user},{"display_name",text},{"is_premium",premium},{"is_supporter",supporter},{"adult_content",adult}};return true;
    }catch(...){return false;}
}
Status userinfo(Operation& op,const mb_nexus_oauth_tokens_v1& tokens,Work& work,std::string& summary) {
    auto status=token_gate(op,tokens);if(status!=MB_NEXUS_AUTH_OK)return status;
    mb_nexus_client_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.kind=MB_NEXUS_OAUTH_USER;p.auth_mode=MB_NEXUS_CLIENT_BEARER;
    p.credential=tokens.access_token;p.credential_bytes=tokens.access_token_bytes;p.ca_pem=op.c.ca.data();p.ca_bytes=op.c.ca.size();
    p.resolve=op.c.users_resolve.empty()?nullptr:op.c.users_resolve.c_str();if(!op.remaining(p.timeout_ms,p.connect_timeout_ms))return op.fault;
    p.cancel_context=&op;p.cancelled=cancelled;p.raw_body=work.raw.data();p.raw_capacity=work.raw.size();p.report=work.report.data();p.report_capacity=work.report.size();
    mb_nexus_client_result_v1 received{};const auto rc=mb_nexus_client_request_v1(&p,&received);
    status=token_gate(op,tokens);if(status!=MB_NEXUS_AUTH_OK)return status;
    if(rc==MB_NEXUS_CLIENT_TRANSPORT_FAILED)return received.curl_code==28?MB_NEXUS_AUTH_TIMEOUT:MB_NEXUS_AUTH_TRANSPORT_FAILED;
    if(rc!=MB_NEXUS_CLIENT_OK||received.status!=MB_NEXUS_CLIENT_OK||received.struct_size!=sizeof(received)||received.abi_version!=1||received.complete!=1||received.metadata_decoded!=1)return MB_NEXUS_AUTH_PAYLOAD_FAILED;
    modbridge::Json profile;if(!public_profile(work.report.data(),received.report_bytes,work.report.size(),profile))return MB_NEXUS_AUTH_PAYLOAD_FAILED;
    summary=modbridge::Json{{"status","connected"},{"live_service","verified"},{"credentials_present",true},{"account",std::move(profile)},{"console_translation","unverified"}}.dump();return MB_NEXUS_AUTH_OK;
}
bool catalog_domain(std::string_view value) noexcept {
    const auto alnum=[](unsigned char c){return(c>='a'&&c<='z')||(c>='0'&&c<='9');};
    if(value.empty()||value.size()>MB_NEXUS_DOMAIN_LIMIT_BYTES||!alnum(static_cast<unsigned char>(value.front())))return false;
    for(unsigned char c:value)if(!alnum(c)&&c!='-'&&c!='_')return false;return true;
}
bool catalog_report(const char *data,size_t bytes,size_t capacity,modbridge::Json& records) {
    if(!bytes||bytes>=capacity||data[bytes])return false;
    size_t nodes=0;
    auto gate=[&nodes](int depth,modbridge::Json::parse_event_t,modbridge::Json&){
        if(depth>64||++nodes>200000)throw std::length_error("catalog report limit");return true;
    };
    try {
        auto report=modbridge::Json::parse(data,data+bytes,gate);
        if(!report.is_object()||report.value("status",std::string{})!="decoded"||
           report.value("scope",std::string{})!="nexus_metadata_only"||!report.contains("records")||
           !report.at("records").is_array()||report.at("records").size()>10000)return false;
        records=std::move(report["records"]);return true;
    } catch(...) {return false;}
}
bool requirement_text(const modbridge::Json& value,size_t maximum,bool nonempty=false) noexcept {
    if(!value.is_string())return false;
    const auto& text=value.get_ref<const std::string&>();size_t characters=0;
    for(unsigned char byte:text) {
        if(byte<32&&byte!='\n'&&byte!='\r'&&byte!='\t')return false;
        if((byte&0xc0)!=0x80)++characters;
    }
    return characters<=maximum&&(!nonempty||characters!=0);
}
bool requirement_identifier(const modbridge::Json& value,uint64_t& output) noexcept {
    if(value.is_number_unsigned())output=value.get<uint64_t>();
    else if(value.type()==modbridge::Json::value_t::number_integer) {
        const auto signed_value=value.get<int64_t>();if(signed_value<=0)return false;
        output=static_cast<uint64_t>(signed_value);
    } else return false;
    return output>0&&output<=static_cast<uint64_t>(INT32_MAX);
}
bool canonical_requirement_mod(std::string_view text) noexcept {
    if(text.empty()||text.size()>19||text.front()<'1'||text.front()>'9')return false;
    uint64_t number=0;
    for(unsigned char byte:text) {
        if(byte<'0'||byte>'9')return false;
        const uint64_t digit=static_cast<uint64_t>(byte-'0');
        if(number>(static_cast<uint64_t>(INT64_MAX)-digit)/10)return false;
        number=number*10+digit;
    }
    return number>0;
}
bool requirements_report(const char *data,size_t bytes,size_t capacity,std::string_view domain,
                         uint64_t mod_id,modbridge::Json& requirements) {
    if(!bytes||bytes>=capacity||data[bytes])return false;
    size_t nodes=0;
    auto gate=[&nodes](int depth,modbridge::Json::parse_event_t,modbridge::Json&) {
        if(depth>64||++nodes>200000)throw std::length_error("requirements report limit");return true;
    };
    try {
        auto report=modbridge::Json::parse(data,data+bytes,gate);
        if(!report.is_object()||report.value("schema_version",0)!=1||
           report.value("status",std::string{})!="decoded"||
           report.value("scope",std::string{})!="nexus_metadata_only"||
           !report.contains("installation_supported")||!report.at("installation_supported").is_boolean()||
           report.at("installation_supported").get<bool>()||!report.contains("issues")||
           !report.at("issues").is_array()||!report.at("issues").empty()||
           !report.contains("requirements")||!report.at("requirements").is_object())return false;
        const auto& value=report.at("requirements");uint64_t source_id=0;
        if(!value.contains("game")||!value.at("game").is_string()||
           value.at("game").get_ref<const std::string&>()!=domain||
           !value.contains("mod_id")||!requirement_identifier(value.at("mod_id"),source_id)||source_id!=mod_id||
           !value.contains("discovery_complete")||!value.at("discovery_complete").is_boolean()||
           !value.at("discovery_complete").get<bool>()||
           !value.contains("file_version_constraints_provided")||
           !value.at("file_version_constraints_provided").is_boolean()||
           value.at("file_version_constraints_provided").get<bool>()||
           !value.contains("installation_supported")||!value.at("installation_supported").is_boolean()||
           value.at("installation_supported").get<bool>()||
           !value.contains("nexus_requirements")||!value.at("nexus_requirements").is_array()||
           value.at("nexus_requirements").size()>MB_NEXUS_REQUIREMENTS_NODE_LIMIT||
           !value.contains("dlc_requirements")||!value.at("dlc_requirements").is_array()||
           value.at("dlc_requirements").size()>MB_NEXUS_REQUIREMENTS_NODE_LIMIT)return false;
        std::set<std::string> relation_ids;
        for(const auto& row:value.at("nexus_requirements")) {
            if(!row.is_object()||!row.contains("id")||!requirement_text(row.at("id"),256,true)||
               !row.contains("external_requirement")||!row.at("external_requirement").is_boolean()||
               !row.contains("game")||!requirement_text(row.at("game"),80)||
               !row.contains("mod_id")||!requirement_text(row.at("mod_id"),
                   row.at("external_requirement").get<bool>()?128u:19u)||
               !row.contains("name")||!requirement_text(row.at("name"),1024)||
               !row.contains("notes")||!requirement_text(row.at("notes"),65536)||
               !row.contains("url")||!requirement_text(row.at("url"),8192)||
               !row.contains("satisfaction")||!row.at("satisfaction").is_string()||
               row.at("satisfaction").get_ref<const std::string&>()!="unresolved")return false;
            const auto& game=row.at("game").get_ref<const std::string&>();
            const auto& related_mod=row.at("mod_id").get_ref<const std::string&>();
            if(!row.at("external_requirement").get<bool>()&&
               (!catalog_domain(game)||!canonical_requirement_mod(related_mod)))return false;
            if(!relation_ids.insert(row.at("id").get<std::string>()).second)return false;
        }
        for(const auto& row:value.at("dlc_requirements")) {
            if(!row.is_object()||!row.contains("game")||!requirement_text(row.at("game"),80,true)||
               !catalog_domain(row.at("game").get_ref<const std::string&>())||
               !row.contains("id")||!requirement_text(row.at("id"),256,true)||
               !row.contains("name")||!requirement_text(row.at("name"),1024)||
               !row.contains("notes")||!requirement_text(row.at("notes"),65536)||
               !row.contains("satisfaction")||!row.at("satisfaction").is_string()||
               row.at("satisfaction").get_ref<const std::string&>()!="unresolved")return false;
        }
        requirements=value;
        return true;
    } catch(...) {return false;}
}
int nexus_kind(mb_nexus_auth_catalog_kind_v1 kind) noexcept {
    switch(kind) {
    case MB_NEXUS_AUTH_CATALOG_GAMES:return MB_NEXUS_GAMES;
    case MB_NEXUS_AUTH_CATALOG_MODS:return MB_NEXUS_MODS;
    case MB_NEXUS_AUTH_CATALOG_MOD:return MB_NEXUS_MOD;
    case MB_NEXUS_AUTH_CATALOG_FILES:return MB_NEXUS_FILES;
    case MB_NEXUS_AUTH_CATALOG_REQUIREMENTS:return MB_NEXUS_REQUIREMENTS;
    default:return -1;
    }
}
Status catalog_request(Operation& op,const mb_nexus_oauth_tokens_v1& tokens,CatalogWork& work,
                       mb_nexus_auth_catalog_kind_v1 kind,std::string_view domain,uint64_t mod_id,
                       modbridge::Json& records) {
    auto status=token_gate(op,tokens);if(status!=MB_NEXUS_AUTH_OK)return status;
    mb_nexus_client_params_v1 request{};request.struct_size=sizeof(request);request.abi_version=MB_NEXUS_CLIENT_ABI_V1;
    request.kind=nexus_kind(kind);request.auth_mode=MB_NEXUS_CLIENT_BEARER;
    request.credential=tokens.access_token;request.credential_bytes=tokens.access_token_bytes;
    request.domain=domain.empty()?nullptr:domain.data();request.domain_bytes=domain.size();request.mod_id=mod_id;
    request.ca_pem=op.c.ca.data();request.ca_bytes=op.c.ca.size();
    request.resolve=op.c.api_resolve.empty()?nullptr:op.c.api_resolve.c_str();
    if(!op.remaining(request.timeout_ms,request.connect_timeout_ms))return op.fault;
    request.cancel_context=&op;request.cancelled=cancelled;
    request.raw_body=work.raw.data();request.raw_capacity=work.raw.size();
    request.report=work.report.data();request.report_capacity=work.report.size();
    mb_nexus_client_result_v1 received{};const int rc=mb_nexus_client_request_v1(&request,&received);
    status=token_gate(op,tokens);if(status!=MB_NEXUS_AUTH_OK)return status;
    if(rc==MB_NEXUS_CLIENT_TRANSPORT_FAILED)return received.curl_code==28?MB_NEXUS_AUTH_TIMEOUT:MB_NEXUS_AUTH_TRANSPORT_FAILED;
    if(rc!=MB_NEXUS_CLIENT_OK||received.status!=MB_NEXUS_CLIENT_OK||
       received.struct_size!=sizeof(received)||received.abi_version!=MB_NEXUS_CLIENT_ABI_V1||
       received.complete!=1||received.metadata_decoded!=1)return MB_NEXUS_AUTH_PAYLOAD_FAILED;
    return catalog_report(work.report.data(),received.report_bytes,work.report.size(),records)
        ?MB_NEXUS_AUTH_OK:MB_NEXUS_AUTH_PAYLOAD_FAILED;
}
Status requirements_request(Operation& op,const mb_nexus_oauth_tokens_v1& tokens,CatalogWork& work,
                            std::string_view domain,uint64_t mod_id,modbridge::Json& requirements) {
    auto status=token_gate(op,tokens);if(status!=MB_NEXUS_AUTH_OK)return status;
    mb_nexus_client_params_v1 request{};request.struct_size=sizeof(request);request.abi_version=MB_NEXUS_CLIENT_ABI_V1;
    request.kind=MB_NEXUS_REQUIREMENTS;request.auth_mode=MB_NEXUS_CLIENT_BEARER;
    request.credential=tokens.access_token;request.credential_bytes=tokens.access_token_bytes;
    request.domain=domain.data();request.domain_bytes=domain.size();request.mod_id=mod_id;
    request.ca_pem=op.c.ca.data();request.ca_bytes=op.c.ca.size();
    request.resolve=op.c.api_resolve.empty()?nullptr:op.c.api_resolve.c_str();
    if(!op.remaining(request.timeout_ms,request.connect_timeout_ms))return op.fault;
    request.cancel_context=&op;request.cancelled=cancelled;
    request.raw_body=work.raw.data();request.raw_capacity=work.raw.size();
    request.report=work.report.data();request.report_capacity=work.report.size();
    mb_nexus_client_result_v1 received{};const int rc=mb_nexus_client_request_v1(&request,&received);
    status=token_gate(op,tokens);if(status!=MB_NEXUS_AUTH_OK)return status;
    if(rc==MB_NEXUS_CLIENT_TRANSPORT_FAILED)return received.curl_code==28?MB_NEXUS_AUTH_TIMEOUT:MB_NEXUS_AUTH_TRANSPORT_FAILED;
    if(rc!=MB_NEXUS_CLIENT_OK||received.status!=MB_NEXUS_CLIENT_OK||
       received.struct_size!=sizeof(received)||received.abi_version!=MB_NEXUS_CLIENT_ABI_V1||
       received.complete!=1||received.metadata_decoded!=1)return MB_NEXUS_AUTH_PAYLOAD_FAILED;
    return requirements_report(work.report.data(),received.report_bytes,work.report.size(),domain,mod_id,requirements)
        ?MB_NEXUS_AUTH_OK:MB_NEXUS_AUTH_PAYLOAD_FAILED;
}
bool visible_mod(const modbridge::Json& record,bool allow_adult) noexcept {
    try {
        if(!record.is_object()||!record.contains("available")||!record.at("available").is_boolean()||
           !record.at("available").get<bool>()||!record.contains("status")||!record.at("status").is_string()||
           record.at("status").get_ref<const std::string&>()!="published"||
           !record.contains("contains_adult_content")||!record.at("contains_adult_content").is_boolean())return false;
        return !record.at("contains_adult_content").get<bool>()||allow_adult;
    } catch(...) {return false;}
}
bool adult_candidate(const modbridge::Json& record) noexcept {
    try {
        return record.is_object()&&record.value("available",false)&&
            record.value("status",std::string{})=="published"&&
            record.contains("contains_adult_content")&&record.at("contains_adult_content").is_boolean()&&
            record.at("contains_adult_content").get<bool>();
    } catch(...) {return false;}
}
Status fresh_adult_preference(Operation& op,const mb_nexus_oauth_tokens_v1& tokens,bool& allowed) {
    allowed=false;auto work=std::make_unique<Work>();std::string summary;TextErase erase{summary};
    const auto status=userinfo(op,tokens,*work,summary);work.reset();if(status!=MB_NEXUS_AUTH_OK)return status;
    try {
        auto report=modbridge::Json::parse(summary);
        if(!report.is_object()||report.value("status",std::string{})!="connected"||
           !report.contains("account")||!report.at("account").is_object()||
           !report.at("account").contains("adult_content"))return MB_NEXUS_AUTH_PAYLOAD_FAILED;
        const auto& adult=report.at("account").at("adult_content");
        allowed=adult.is_boolean()&&adult.get<bool>();return MB_NEXUS_AUTH_OK;
    } catch(...) {return MB_NEXUS_AUTH_PAYLOAD_FAILED;}
}
struct AcquisitionNode {
    uint64_t mod_id=0;
    bool processed=false,visible=false,discovery_complete=false,selected=false;
    std::string name,file_version,selection_basis;
    size_t file_count=0;
    uint64_t file_id=0;
    std::string registration_metadata;
    ~AcquisitionNode(){wipe(registration_metadata.data(),registration_metadata.size());}
};
struct AcquisitionEdge { uint64_t from=0,to=0;size_t relation_index=0; };
void add_unresolved(modbridge::Json& unresolved,bool& any,bool& overflow,modbridge::Json fact) {
    any=true;
    if(unresolved.size()<128)unresolved.push_back(std::move(fact));
    else if(!overflow) {unresolved.back()={{"reason","unresolved_limit"}};overflow=true;}
}
bool parse_canonical_mod_id(std::string_view text,uint64_t& id) noexcept {
    if(text.empty()||text.size()>19||text.front()<'1'||text.front()>'9')return false;
    uint64_t value=0;
    for(unsigned char byte:text) {
        if(byte<'0'||byte>'9')return false;
        const uint64_t digit=static_cast<uint64_t>(byte-'0');
        if(value>(static_cast<uint64_t>(INT64_MAX)-digit)/10)return false;
        value=value*10+digit;
    }
    id=value;return value!=0;
}
Status build_acquisition_plan(Operation& op,const mb_nexus_oauth_tokens_v1& tokens,CatalogWork& work,
                              const AcquisitionIntent& intent,modbridge::Json& plan,
                              modbridge::Json *capture=nullptr) {
    std::map<uint64_t,AcquisitionNode> nodes;
    std::set<uint64_t> pending,used_choices;
    std::vector<AcquisitionEdge> edges;
    modbridge::Json relations=modbridge::Json::array(),unresolved=modbridge::Json::array();
    bool has_unresolved=false,unresolved_overflow=false,discovery_complete=true,relation_overflow=false;
    for(uint64_t id:intent.root_mod_ids) {
        AcquisitionNode node;node.mod_id=id;nodes.emplace(id,std::move(node));pending.insert(id);
    }
    auto mark_unresolved=[&](size_t index,const char *reason) {
        relations[index]["resolution_status"]="unresolved";
        relations[index]["reason"]=reason;
    };
    while(!pending.empty()) {
        if(!op.check())return op.fault;
        const uint64_t mod_id=*pending.begin();pending.erase(pending.begin());
        auto node_it=nodes.find(mod_id);if(node_it==nodes.end())return MB_NEXUS_AUTH_INTERNAL_ERROR;
        auto& node=node_it->second;if(node.processed)continue;node.processed=true;
        if(intent.file_choices.contains(mod_id))used_choices.insert(mod_id);

        modbridge::Json source_records;
        JsonErase erase_source{source_records};
        auto status=catalog_request(op,tokens,work,MB_NEXUS_AUTH_CATALOG_MOD,intent.game,mod_id,source_records);
        if(status!=MB_NEXUS_AUTH_OK)return status;
        if(!source_records.is_array()||source_records.size()!=1||!source_records[0].is_object())
            return MB_NEXUS_AUTH_PAYLOAD_FAILED;
        const auto& source=source_records[0];
        bool allow_adult=false;
        if(!visible_mod(source,false)&&adult_candidate(source)) {
            status=fresh_adult_preference(op,tokens,allow_adult);
            if(status!=MB_NEXUS_AUTH_OK)return status;
        }
        if(!visible_mod(source,allow_adult)) {
            node.visible=false;node.discovery_complete=false;discovery_complete=false;
            add_unresolved(unresolved,has_unresolved,unresolved_overflow,
                           modbridge::Json{{"reason","hidden_mod"},{"mod_id",mod_id}});
            continue;
        }
        node.visible=true;
        node.name=source.at("name").get<std::string>();

        modbridge::Json requirement_object;
        JsonErase erase_requirements{requirement_object};
        status=requirements_request(op,tokens,work,intent.game,mod_id,requirement_object);
        if(status!=MB_NEXUS_AUTH_OK)return status;
        node.discovery_complete=true;

        std::vector<const modbridge::Json*> nexus_rows;
        for(const auto& row:requirement_object.at("nexus_requirements"))nexus_rows.push_back(&row);
        std::sort(nexus_rows.begin(),nexus_rows.end(),[](const auto *a,const auto *b) {
            return std::tie(a->at("id"),a->at("external_requirement"),a->at("game"),a->at("mod_id"))<
                   std::tie(b->at("id"),b->at("external_requirement"),b->at("game"),b->at("mod_id"));
        });
        for(const auto *row_ptr:nexus_rows) {
            if(relations.size()>=128) {
                discovery_complete=false;
                if(!relation_overflow) {
                    add_unresolved(unresolved,has_unresolved,unresolved_overflow,
                                   modbridge::Json{{"reason","relation_limit"},{"from_mod_id",mod_id}});
                    relation_overflow=true;
                }
                break;
            }
            const auto& row=*row_ptr;const bool external=row.at("external_requirement").get<bool>();
            const auto& child_game=row.at("game").get_ref<const std::string&>();
            const auto& child_text=row.at("mod_id").get_ref<const std::string&>();
            modbridge::Json relation={{"from_mod_id",mod_id},{"relation_id",row.at("id")},
                {"kind","nexus"},{"external_requirement",external},{"target_game",child_game},
                {"target_mod_id",child_text},{"satisfaction","unresolved"},
                {"resolution_status","unresolved"}};
            const size_t relation_index=relations.size();
            uint64_t child_id=0;const char *reason=nullptr;
            if(external)reason="external_requirement";
            else if(child_game!=intent.game)reason="cross_game";
            else if(!parse_canonical_mod_id(child_text,child_id))return MB_NEXUS_AUTH_PAYLOAD_FAILED;
            else if(child_id>static_cast<uint64_t>(INT32_MAX))reason="child_id_out_of_range";
            else if(!nodes.contains(child_id)&&nodes.size()>=8)reason="closure_limit";
            if(reason) {
                relation["reason"]=reason;relations.push_back(std::move(relation));
                add_unresolved(unresolved,has_unresolved,unresolved_overflow,
                    modbridge::Json{{"reason",reason},{"from_mod_id",mod_id},
                        {"relation_id",row.at("id")},{"target_game",child_game},{"target_mod_id",child_text}});
                if(!std::strcmp(reason,"cross_game")||!std::strcmp(reason,"child_id_out_of_range")||
                   !std::strcmp(reason,"closure_limit"))discovery_complete=false;
                continue;
            }
            if(!nodes.contains(child_id)) {
                AcquisitionNode child;child.mod_id=child_id;nodes.emplace(child_id,std::move(child));pending.insert(child_id);
            }
            relation["resolution_status"]="pending";relations.push_back(std::move(relation));
            edges.push_back({mod_id,child_id,relation_index});
        }
        if(relations.size()<128) {
            std::vector<const modbridge::Json*> dlc_rows;
            for(const auto& row:requirement_object.at("dlc_requirements"))dlc_rows.push_back(&row);
            std::sort(dlc_rows.begin(),dlc_rows.end(),[](const auto *a,const auto *b) {
                return std::tie(a->at("game"),a->at("id"),a->at("name"))<
                       std::tie(b->at("game"),b->at("id"),b->at("name"));
            });
            for(const auto *row_ptr:dlc_rows) {
                if(relations.size()>=128) {
                    discovery_complete=false;
                    if(!relation_overflow) {
                        add_unresolved(unresolved,has_unresolved,unresolved_overflow,
                                       modbridge::Json{{"reason","relation_limit"},{"from_mod_id",mod_id}});
                        relation_overflow=true;
                    }
                    break;
                }
                const auto& row=*row_ptr;const auto& game=row.at("game").get_ref<const std::string&>();
                const auto& id=row.at("id").get_ref<const std::string&>();
                relations.push_back({{"from_mod_id",mod_id},{"relation_id",id},{"kind","dlc"},
                    {"target_game",game},{"target_id",id},{"target_name",row.at("name")},
                    {"satisfaction","unresolved"},{"resolution_status","unresolved"},
                    {"reason","dlc_requirement"}});
                add_unresolved(unresolved,has_unresolved,unresolved_overflow,
                    modbridge::Json{{"reason","dlc_requirement"},{"from_mod_id",mod_id},
                        {"relation_id",id},{"target_game",game},{"target_id",id}});
            }
        } else if(!requirement_object.at("dlc_requirements").empty()) {
            discovery_complete=false;
            if(!relation_overflow) {
                add_unresolved(unresolved,has_unresolved,unresolved_overflow,
                               modbridge::Json{{"reason","relation_limit"},{"from_mod_id",mod_id}});
                relation_overflow=true;
            }
        }

        modbridge::Json files;
        JsonErase erase_files{files};
        status=catalog_request(op,tokens,work,MB_NEXUS_AUTH_CATALOG_FILES,intent.game,mod_id,files);
        if(status!=MB_NEXUS_AUTH_OK)return status;
        if(!files.is_array()||files.size()>10000)return MB_NEXUS_AUTH_PAYLOAD_FAILED;
        node.file_count=files.size();
        const auto selected_choice=intent.file_choices.find(mod_id);
        const uint64_t explicit_id=selected_choice==intent.file_choices.end()?0:selected_choice->second;
        const auto selection=modbridge::nexus_select_file(files,explicit_id,intent.unique_current_primary);
        if(!selection.selected) {
            const std::string_view detail=selection.reason?selection.reason:"file_selection_failed";
            if(detail=="invalid_file_metadata"||detail=="duplicate_file_id"||detail=="file_list_limit"||
               detail=="file_selection_failed")return MB_NEXUS_AUTH_PAYLOAD_FAILED;
            const char *reason=nullptr;
            if(detail=="ambiguous_current_primary")reason="file_choice_ambiguous";
            else if(detail=="explicit_file_not_found"||detail=="invalid_explicit_file_id"||
                    detail=="file_hints_contradictory")reason="file_choice_invalid";
            else if(detail=="file_choice_required"||detail=="file_hints_unknown"||detail=="no_current_primary")
                reason="file_choice_missing";
            else return MB_NEXUS_AUTH_INTERNAL_ERROR;
            add_unresolved(unresolved,has_unresolved,unresolved_overflow,
                modbridge::Json{{"reason",reason},{"mod_id",mod_id},{"selector_detail",detail}});
        } else {
            node.selected=true;node.file_id=selection.file_id;
            node.selection_basis=selection.basis?selection.basis:"none";
            bool found=false;
            for(const auto& file:files) {
                if(file.at("file_id").get<uint64_t>()==node.file_id) {
                    node.file_version=file.at("version").get<std::string>();found=true;break;
                }
            }
            if(!found)return MB_NEXUS_AUTH_PAYLOAD_FAILED;
            if(capture) {
                modbridge::Json metadata={{"source",modbridge::Json::object()},{"requirements",requirement_object}};
                JsonErase erase_metadata{metadata};
                for(const auto *field:{"name","author"}) {
                    const auto value=source.find(field);
                    if(value!=source.end()&&value->is_string()&&!value->get_ref<const std::string&>().empty())
                        metadata["source"][field]=*value;
                }
                if(!node.file_version.empty())metadata["source"]["version"]=node.file_version;
                std::string rendered=metadata.dump();TextErase erase_rendered{rendered};
                if(rendered.size()<=MB_NEXUS_AUTH_REGISTRATION_METADATA_LIMIT)
                    node.registration_metadata=std::move(rendered);
            }
        }
    }

    for(const auto& [choice_mod,file_id]:intent.file_choices) {
        if(!used_choices.contains(choice_mod))
            add_unresolved(unresolved,has_unresolved,unresolved_overflow,
                modbridge::Json{{"reason","unused_file_choice"},{"mod_id",choice_mod},{"file_id",file_id}});
    }
    for(const auto& edge:edges) {
        const auto target=nodes.find(edge.to);
        if(target==nodes.end()||!target->second.processed||!target->second.visible) {
            mark_unresolved(edge.relation_index,"hidden_mod");
            add_unresolved(unresolved,has_unresolved,unresolved_overflow,
                modbridge::Json{{"reason","hidden_mod"},{"from_mod_id",edge.from},
                    {"relation_id",relations[edge.relation_index].at("relation_id")},
                    {"target_game",intent.game},{"target_mod_id",std::to_string(edge.to)}});
            discovery_complete=false;
        } else relations[edge.relation_index]["resolution_status"]="discovered";
    }

    std::map<uint64_t,unsigned> colors;std::vector<uint64_t> prerequisite_order;
    auto visit=[&](auto&& self,uint64_t id)->void {
        colors[id]=1;
        std::vector<const AcquisitionEdge*> outgoing;
        for(const auto& edge:edges)if(edge.from==id)outgoing.push_back(&edge);
        std::sort(outgoing.begin(),outgoing.end(),[](const auto *a,const auto *b) {
            if(a->to!=b->to)return a->to<b->to;
            return a->relation_index<b->relation_index;
        });
        for(const auto *edge:outgoing) {
            if(relations[edge->relation_index].value("resolution_status",std::string{})=="unresolved")continue;
            const unsigned color=colors[edge->to];
            if(color==1) {
                mark_unresolved(edge->relation_index,"cycle");
                add_unresolved(unresolved,has_unresolved,unresolved_overflow,
                    modbridge::Json{{"reason","cycle"},{"from_mod_id",edge->from},
                        {"relation_id",relations[edge->relation_index].at("relation_id")},
                        {"target_game",intent.game},{"target_mod_id",std::to_string(edge->to)}});
            } else {
                if(color==0)self(self,edge->to);
            }
        }
        colors[id]=2;prerequisite_order.push_back(id);
    };
    for(const auto& [id,node]:nodes)if(node.processed&&node.visible&&colors[id]==0)visit(visit,id);

    modbridge::Json node_facts=modbridge::Json::array();bool every_node_selected=true;
    for(const auto& [id,node]:nodes) {
        if(!node.processed||!node.visible||!node.discovery_complete)discovery_complete=false;
        every_node_selected=every_node_selected&&node.processed&&node.visible&&node.selected;
        modbridge::Json fact={{"game",intent.game},{"mod_id",id},
            {"discovery_complete",node.discovery_complete},{"source_visible",node.visible}};
        if(node.visible){fact["name"]=node.name;fact["file_count"]=node.file_count;}
        node_facts.push_back(std::move(fact));
    }
    if(unresolved_overflow)discovery_complete=false;
    const bool ready=discovery_complete&&!has_unresolved&&every_node_selected&&
                     prerequisite_order.size()==nodes.size();
    modbridge::Json selected_sources=modbridge::Json::array();
    if(capture)*capture={{"items",modbridge::Json::array()}};
    if(ready)for(uint64_t id:prerequisite_order) {
        const auto& node=nodes.at(id);
        selected_sources.push_back({{"game",intent.game},{"mod_id",id},{"file_id",node.file_id},
            {"selection_basis",node.selection_basis},{"file_version",node.file_version}});
        if(capture)capture->at("items").push_back({{"game",intent.game},{"mod_id",id},{"file_id",node.file_id},
            {"metadata",node.registration_metadata.empty()?modbridge::Json(nullptr):
                modbridge::Json::parse(node.registration_metadata)}});
    }
    plan={{"schema_version",1},{"scope","nexus_acquisition_metadata"},{"game",intent.game},
          {"root_mod_ids",intent.root_mod_ids},{"file_policy",intent.unique_current_primary?
              "unique_current_primary":"explicit_only"},{"nodes",std::move(node_facts)},
          {"relations",std::move(relations)},{"selected_sources",std::move(selected_sources)},
          {"unresolved",std::move(unresolved)},{"acquisition_ready",ready},
          {"discovery_complete",discovery_complete},{"compatibility","unresolved"},
          {"version_satisfaction","unresolved"},{"file_version_constraints_provided",false},
          {"download_authorized",false},{"installation_supported",false},
          {"acquisition_order_is_game_load_order",false}};
    return MB_NEXUS_AUTH_OK;
}
bool copy_output(char *out,size_t capacity,std::string_view value) noexcept {
    if(value.size()>=capacity)return false;std::memcpy(out,value.data(),value.size());out[value.size()]=0;return true;
}
} // namespace

extern "C" int mb_nexus_auth_create_v1(const mb_nexus_auth_create_params_v1 *p,mb_nexus_auth_context_v1 **out,mb_nexus_auth_result_v1 *r) noexcept {
    Regions spans;Range ignored{};
    if(!p||!out||!r||!range(p,sizeof(*p),ignored)||!range(out,sizeof(*out),ignored)||!range(r,sizeof(*r),ignored)||
        reinterpret_cast<uintptr_t>(p)%alignof(mb_nexus_auth_create_params_v1)||reinterpret_cast<uintptr_t>(out)%alignof(mb_nexus_auth_context_v1*)||reinterpret_cast<uintptr_t>(r)%alignof(mb_nexus_auth_result_v1)||
        p->struct_size!=sizeof(*p)||p->abi_version!=1||p->origin_bytes>128||p->client_id_bytes>128||p->redirect_uri_bytes>256||p->scope_bytes>1024||p->ca_bytes>1048576||p->api_resolve_bytes>=8192||p->users_resolve_bytes>=8192||
        !spans.add(p,sizeof(*p))||!spans.add(out,sizeof(*out))||!spans.add(r,sizeof(*r))||!spans.add(p->origin,p->origin_bytes)||!spans.add(p->client_id,p->client_id_bytes)||!spans.add(p->redirect_uri,p->redirect_uri_bytes)||!spans.add(p->scope,p->scope_bytes)||!spans.add(p->ca_pem,p->ca_bytes)||!spans.add(p->api_resolve,p->api_resolve_bytes)||!spans.add(p->users_resolve,p->users_resolve_bytes)||!spans.distinct()||!callback_context(spans,p->entropy_context))return MB_NEXUS_AUTH_BAD_ARGUMENT;
    mb_nexus_auth_result_v1 result{};result.struct_size=sizeof(result);result.abi_version=1;result.status=MB_NEXUS_AUTH_BAD_ARGUMENT;
    if(*out||!p->entropy_read32||!origin_valid(view(p->origin,p->origin_bytes))){*r=result;return result.status;}
    const auto client=view(p->client_id,p->client_id_bytes);bool configured=!client.empty();
    if(configured) {
        for(char ch:client)if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='-'||ch=='_')){*r=result;return result.status;}
        if(client=="public_test"||!p->ca_bytes||!ascii(view(p->scope,p->scope_bytes),true)||!resolve_valid(view(p->api_resolve,p->api_resolve_bytes),"api.nexusmods.com")||!resolve_valid(view(p->users_resolve,p->users_resolve_bytes),"users.nexusmods.com")){*r=result;return result.status;}
    } else if(p->redirect_uri_bytes||p->scope_bytes||p->ca_bytes||p->api_resolve_bytes||p->users_resolve_bytes){*r=result;return result.status;}
    try{
        auto c=std::make_unique<mb_nexus_auth_context_v1>();c->origin.assign(p->origin,p->origin_bytes);
        if(configured&&view(p->redirect_uri,p->redirect_uri_bytes)!=c->origin+"/auth/callback"){*r=result;return result.status;}
        c->configured=configured;c->client=client;c->redirect=view(p->redirect_uri,p->redirect_uri_bytes);c->scope=view(p->scope,p->scope_bytes);
        if(p->ca_bytes)c->ca.assign(p->ca_pem,p->ca_pem+p->ca_bytes);
        c->api_resolve=view(p->api_resolve,p->api_resolve_bytes);c->users_resolve=view(p->users_resolve,p->users_resolve_bytes);c->entropy_context=p->entropy_context;c->entropy_read32=p->entropy_read32;
        uint64_t value=0;if(!now(*c,value)){result.status=MB_NEXUS_AUTH_SOURCE_FAILED;*r=result;return result.status;}
        result.status=MB_NEXUS_AUTH_OK;result.complete=1;result.configured=configured;*out=c.release();*r=result;return result.status;
    }catch(...){result.status=MB_NEXUS_AUTH_INTERNAL_ERROR;*r=result;return result.status;}
}
extern "C" int mb_nexus_auth_free_v1(mb_nexus_auth_context_v1 **p) noexcept {
    Range ignored{};
    if(!p||!range(p,sizeof(*p),ignored)||reinterpret_cast<uintptr_t>(p)%alignof(mb_nexus_auth_context_v1*))return MB_NEXUS_AUTH_BAD_ARGUMENT;
    if(!*p)return MB_NEXUS_AUTH_OK;auto *c=*p;
    if(!range(c,sizeof(*c),ignored)||reinterpret_cast<uintptr_t>(c)%alignof(mb_nexus_auth_context_v1)||c->identity!=magic)return MB_NEXUS_AUTH_BAD_ARGUMENT;
    Regions slot;if(!slot.add(p,sizeof(*p))||!owned_distinct(slot,*c))return MB_NEXUS_AUTH_BAD_ARGUMENT;
    if(c->busy)return MB_NEXUS_AUTH_BUSY;
    delete c;*p=nullptr;return MB_NEXUS_AUTH_OK;
}
extern "C" int mb_nexus_auth_session_v1(mb_nexus_auth_context_v1 *c,const mb_nexus_auth_session_params_v1 *p,mb_nexus_auth_result_v1 *r) noexcept {
    Regions spans;if(!base_layout(c,p,r,spans)||!add_cookie(spans,p->cookie,p->cookie_bytes)||!add_output(spans,p->cookie_output,p->cookie_capacity)||!add_output(spans,p->csrf_output,p->csrf_capacity)||!owned_distinct(spans,*c))return MB_NEXUS_AUTH_BAD_ARGUMENT;
    Output cookie{p->cookie_output,p->cookie_capacity},csrf{p->csrf_output,p->csrf_capacity};
    if(c->busy)return finish(*c,r,MB_NEXUS_AUTH_BUSY);Busy busy(*c);
    if(p->cookie_capacity<44||p->csrf_capacity<44)return finish(*c,r,MB_NEXUS_AUTH_OUTPUT_TOO_SMALL);
    uint64_t t=0;if(!now(*c,t))return finish(*c,r,MB_NEXUS_AUTH_SOURCE_FAILED);expire(*c,t);
    Session candidate{};Session *existing=session(*c,view(p->cookie,p->cookie_bytes),t);Session *slot=existing;bool added=!existing;
    if(!slot){for(auto& s:c->sessions)if(!s.expires){slot=&s;break;}if(!slot)return finish(*c,r,MB_NEXUS_AUTH_BUSY);if(!fresh(*c,candidate.cookie)||!fresh(*c,candidate.csrf)||!expiry(t,1800,candidate.expires)){wipe(&candidate,sizeof(candidate));return finish(*c,r,MB_NEXUS_AUTH_SOURCE_FAILED);}}
    else candidate=*existing;
    std::memcpy(p->cookie_output,candidate.cookie,44);std::memcpy(p->csrf_output,candidate.csrf,44);
    if(!now(*c,t)||t>=candidate.expires){wipe(&candidate,sizeof(candidate));return finish(*c,r,c->source_failed?MB_NEXUS_AUTH_SOURCE_FAILED:MB_NEXUS_AUTH_SESSION_REQUIRED);}
    *slot=candidate;wipe(&candidate,sizeof(candidate));cookie.committed=csrf.committed=true;return finish(*c,r,MB_NEXUS_AUTH_OK,0,added);
}
extern "C" int mb_nexus_auth_start_v1(mb_nexus_auth_context_v1 *c,const mb_nexus_auth_start_params_v1 *p,mb_nexus_auth_result_v1 *r) noexcept {
    Regions spans;if(!base_layout(c,p,r,spans)||!add_binding(spans,p->cookie,p->cookie_bytes,p->csrf,p->csrf_bytes,p->origin,p->origin_bytes)||!add_output(spans,p->authorization_url,p->authorization_capacity)||!owned_distinct(spans,*c))return MB_NEXUS_AUTH_BAD_ARGUMENT;
    Output output{p->authorization_url,p->authorization_capacity};if(c->busy)return finish(*c,r,MB_NEXUS_AUTH_BUSY);Busy busy(*c);
    uint64_t t=0;if(!now(*c,t))return finish(*c,r,MB_NEXUS_AUTH_SOURCE_FAILED);expire(*c,t);
    auto status=binding(*c,view(p->cookie,p->cookie_bytes),view(p->csrf,p->csrf_bytes),view(p->origin,p->origin_bytes),t);if(status!=MB_NEXUS_AUTH_OK)return finish(*c,r,status);
    if(!c->configured)return finish(*c,r,MB_NEXUS_AUTH_NOT_CONFIGURED);
    if(c->pending.expires&&!equal(view(c->pending.cookie,43),view(p->cookie,p->cookie_bytes)))return finish(*c,r,MB_NEXUS_AUTH_BUSY);
    Pending pending{};std::memcpy(pending.cookie,p->cookie,43);
    if(!fresh(*c,pending.state)||!fresh(*c,pending.verifier)||!expiry(t,300,pending.expires)){wipe(&pending,sizeof(pending));return finish(*c,r,MB_NEXUS_AUTH_SOURCE_FAILED);}
    Status final=MB_NEXUS_AUTH_INTERNAL_ERROR;size_t length=0;
    try{
        {std::array<uint8_t,32> digest{};picosha2::hash256(pending.verifier,pending.verifier+43,digest.begin(),digest.end());char challenge[44]{};base64url(digest.data(),digest.size(),challenge);
         std::string url="https://users.nexusmods.com/oauth/authorize?client_id=";TextErase erase{url};encode(url,c->client);url+="&response_type=code&scope=";encode(url,c->scope);url+="&redirect_uri=";encode(url,c->redirect);url+="&state=";url+=pending.state;url+="&code_challenge_method=S256&code_challenge=";url+=challenge;
         length=url.size();final=copy_output(p->authorization_url,p->authorization_capacity,url)?MB_NEXUS_AUTH_OK:MB_NEXUS_AUTH_OUTPUT_TOO_SMALL;wipe(challenge,sizeof(challenge));wipe(digest.data(),digest.size());}
        if(final==MB_NEXUS_AUTH_OK&&(!now(*c,t)||t>=pending.expires||binding(*c,view(p->cookie,p->cookie_bytes),view(p->csrf,p->csrf_bytes),view(p->origin,p->origin_bytes),t)!=MB_NEXUS_AUTH_OK))final=c->source_failed?MB_NEXUS_AUTH_SOURCE_FAILED:MB_NEXUS_AUTH_SESSION_REQUIRED;
        if(final==MB_NEXUS_AUTH_OK){wipe(&c->pending,sizeof(c->pending));c->pending=pending;output.committed=true;}
    }catch(...){final=MB_NEXUS_AUTH_INTERNAL_ERROR;}
    wipe(&pending,sizeof(pending));return finish(*c,r,final,length);
}
extern "C" int mb_nexus_auth_callback_v1(mb_nexus_auth_context_v1 *c,const mb_nexus_auth_callback_params_v1 *p,mb_nexus_auth_result_v1 *r) noexcept {
    Regions spans;if(!base_layout(c,p,r,spans)||!add_cookie(spans,p->cookie,p->cookie_bytes)||p->target_bytes>8192||!spans.add(p->target,p->target_bytes)||!add_output(spans,p->summary,p->summary_capacity)||!owned_distinct(spans,*c)||!callback_context(spans,p->cancel_context,c))return MB_NEXUS_AUTH_BAD_ARGUMENT;
    Output output{p->summary,p->summary_capacity};if(c->busy)return finish(*c,r,MB_NEXUS_AUTH_BUSY);Busy busy(*c);
    if(!network(p->timeout_ms,p->connect_timeout_ms))return finish(*c,r,MB_NEXUS_AUTH_BAD_ARGUMENT);
    Operation op{*c,p->timeout_ms,p->connect_timeout_ms,p->cancel_context,p->cancelled};if(!op.start()||!op.check())return finish(*c,r,op.fault);
    expire(*c,c->last_mono);if(!session(*c,view(p->cookie,p->cookie_bytes),c->last_mono))return finish(*c,r,MB_NEXUS_AUTH_SESSION_REQUIRED);
    if(!c->configured)return finish(*c,r,MB_NEXUS_AUTH_NOT_CONFIGURED);
    ParsedCallback parsed;if(!parse_callback(view(p->target,p->target_bytes),parsed)||!c->pending.expires||!equal(view(p->cookie,p->cookie_bytes),view(c->pending.cookie,43))||!equal(view(parsed.state,43),view(c->pending.state,43)))return finish(*c,r,MB_NEXUS_AUTH_REFUSED);
    // Parsing belongs to transaction admission. Bind to a fresh sample before
    // the one-use consumption; an admitted exchange then has its own budget.
    if(!op.check())return finish(*c,r,op.fault);
    expire(*c,c->last_mono);
    if(!session(*c,view(p->cookie,p->cookie_bytes),c->last_mono))return finish(*c,r,MB_NEXUS_AUTH_SESSION_REQUIRED);
    if(!c->pending.expires||!equal(view(p->cookie,p->cookie_bytes),view(c->pending.cookie,43))||!equal(view(parsed.state,43),view(c->pending.state,43)))return finish(*c,r,MB_NEXUS_AUTH_REFUSED);
    Pending consumed=c->pending;wipe(&c->pending,sizeof(c->pending));
    if(parsed.denied){parsed.clear();wipe(&consumed,sizeof(consumed));return finish(*c,r,op.check()?MB_NEXUS_AUTH_AUTHORIZATION_CANCELLED:op.fault);}
    Status status=MB_NEXUS_AUTH_INTERNAL_ERROR;size_t length=0;Tokens candidate;
    try{
        auto work=std::make_unique<Work>();candidate.reset(new mb_nexus_oauth_tokens_v1{});
        mb_nexus_oauth_exchange_params_v1 request{};request.struct_size=sizeof(request);request.abi_version=1;
        request.fields={c->client.data(),c->client.size(),c->redirect.data(),c->redirect.size(),parsed.code,parsed.code_bytes,consumed.verifier,43};request.ca_pem=c->ca.data();request.ca_bytes=c->ca.size();request.users_resolve=c->users_resolve.empty()?nullptr:c->users_resolve.c_str();request.cancel_context=&op;request.cancelled=cancelled;request.form=work->form.data();request.form_capacity=work->form.size();request.raw_body=work->raw.data();request.raw_capacity=work->raw.size();request.tokens=candidate.get();
        if(!op.remaining(request.timeout_ms,request.connect_timeout_ms))status=op.fault;
        else {
            mb_nexus_oauth_exchange_result_v1 received{};const int rc=mb_nexus_oauth_exchange_code_v1(&request,&received);
            if(!op.check())status=op.fault;
            else if(rc==MB_NEXUS_OAUTH_CODE_TIMEOUT)status=MB_NEXUS_AUTH_TIMEOUT;
            else if(rc==MB_NEXUS_OAUTH_CODE_CANCELLED)status=MB_NEXUS_AUTH_CANCELLED;
            else if(rc==MB_NEXUS_OAUTH_CODE_TRANSPORT_FAILED||rc==MB_NEXUS_OAUTH_CODE_BAD_STATE)status=MB_NEXUS_AUTH_TRANSPORT_FAILED;
            else if(rc!=MB_NEXUS_OAUTH_CODE_OK||received.status!=MB_NEXUS_OAUTH_CODE_OK||received.struct_size!=sizeof(received)||received.abi_version!=1||received.complete!=1||!valid_tokens(*candidate))status=MB_NEXUS_AUTH_PAYLOAD_FAILED;
            else {std::string summary;status=userinfo(op,*candidate,*work,summary);length=summary.size();if(status==MB_NEXUS_AUTH_OK&&!copy_output(p->summary,p->summary_capacity,summary))status=MB_NEXUS_AUTH_OUTPUT_TOO_SMALL;}
        }
        work.reset();
    }catch(...){status=MB_NEXUS_AUTH_INTERNAL_ERROR;}
    parsed.clear();wipe(&consumed,sizeof(consumed));
    if(status!=MB_NEXUS_AUTH_OK)candidate.reset();
    if(!op.check())status=op.fault;
    if(status==MB_NEXUS_AUTH_OK){status=token_gate(op,*candidate);if(status==MB_NEXUS_AUTH_OK&&!session(*c,view(p->cookie,p->cookie_bytes),c->last_mono))status=MB_NEXUS_AUTH_SESSION_REQUIRED;}
    // All temporary erasure and fallible gates precede this bounded commit.
    // Retiring the previous private block is non-failing; no optional callback
    // or clock sample follows the ownership swap.
    if(status==MB_NEXUS_AUTH_OK){c->active.swap(candidate);candidate.reset();output.committed=true;}
    else {candidate.reset();if(!op.check())status=op.fault;}
    return finish(*c,r,status,length);
}
extern "C" int mb_nexus_auth_account_v1(mb_nexus_auth_context_v1 *c,const mb_nexus_auth_account_params_v1 *p,mb_nexus_auth_result_v1 *r) noexcept {
    Regions spans;if(!base_layout(c,p,r,spans)||!add_cookie(spans,p->cookie,p->cookie_bytes)||!add_output(spans,p->summary,p->summary_capacity)||!owned_distinct(spans,*c)||!callback_context(spans,p->cancel_context,c))return MB_NEXUS_AUTH_BAD_ARGUMENT;
    Output output{p->summary,p->summary_capacity};if(c->busy)return finish(*c,r,MB_NEXUS_AUTH_BUSY);Busy busy(*c);
    if(!network(p->timeout_ms,p->connect_timeout_ms))return finish(*c,r,MB_NEXUS_AUTH_BAD_ARGUMENT);
    Operation op{*c,p->timeout_ms,p->connect_timeout_ms,p->cancel_context,p->cancelled};if(!op.start()||!op.check())return finish(*c,r,op.fault);
    if(!session(*c,view(p->cookie,p->cookie_bytes),c->last_mono))return finish(*c,r,MB_NEXUS_AUTH_SESSION_REQUIRED);
    Status status=MB_NEXUS_AUTH_INTERNAL_ERROR;size_t length=0;
    try{
        {std::string summary;
         if(!c->active){summary="{\"status\":\"disconnected\",\"live_service\":\"not_checked\",\"credentials_present\":false,\"console_translation\":\"unverified\"}";status=MB_NEXUS_AUTH_OK;}
         else {auto work=std::make_unique<Work>();status=userinfo(op,*c->active,*work,summary);work.reset();}
         length=summary.size();if(status==MB_NEXUS_AUTH_OK&&!copy_output(p->summary,p->summary_capacity,summary))status=MB_NEXUS_AUTH_OUTPUT_TOO_SMALL;}
    }catch(...){status=MB_NEXUS_AUTH_INTERNAL_ERROR;}
    if(!op.check())status=op.fault;
    if(status==MB_NEXUS_AUTH_OK&&c->active)status=token_gate(op,*c->active);
    if(status==MB_NEXUS_AUTH_OK&&!session(*c,view(p->cookie,p->cookie_bytes),c->last_mono))status=MB_NEXUS_AUTH_SESSION_REQUIRED;
    if(status==MB_NEXUS_AUTH_OK)output.committed=true;return finish(*c,r,status,length);
}
extern "C" int mb_nexus_auth_disconnect_v1(mb_nexus_auth_context_v1 *c,const mb_nexus_auth_disconnect_params_v1 *p,mb_nexus_auth_result_v1 *r) noexcept {
    Regions spans;if(!base_layout(c,p,r,spans)||!add_binding(spans,p->cookie,p->cookie_bytes,p->csrf,p->csrf_bytes,p->origin,p->origin_bytes)||!owned_distinct(spans,*c))return MB_NEXUS_AUTH_BAD_ARGUMENT;
    if(c->busy)return finish(*c,r,MB_NEXUS_AUTH_BUSY);Busy busy(*c);uint64_t t=0;if(!now(*c,t))return finish(*c,r,MB_NEXUS_AUTH_SOURCE_FAILED);
    auto status=binding(*c,view(p->cookie,p->cookie_bytes),view(p->csrf,p->csrf_bytes),view(p->origin,p->origin_bytes),t);if(status!=MB_NEXUS_AUTH_OK)return finish(*c,r,status);
    c->active.reset();wipe(&c->pending,sizeof(c->pending));return finish(*c,r,MB_NEXUS_AUTH_OK);
}
extern "C" int mb_nexus_auth_admit_v1(mb_nexus_auth_context_v1 *c,const mb_nexus_auth_admit_params_v1 *p,mb_nexus_auth_result_v1 *r) noexcept {
    Regions spans;
    if(!base_layout(c,p,r,spans)||!add_cookie(spans,p->cookie,p->cookie_bytes)||
       p->csrf_bytes>43||p->origin_bytes>MB_NEXUS_AUTH_ORIGIN_LIMIT||
       !spans.add(p->csrf,p->csrf_bytes)||!spans.add(p->origin,p->origin_bytes)||
       (p->admission!=MB_NEXUS_AUTH_ADMISSION_READ&&p->admission!=MB_NEXUS_AUTH_ADMISSION_MUTATION)||
       p->require_account>1||
       (p->admission==MB_NEXUS_AUTH_ADMISSION_READ&&(p->csrf_bytes||p->origin_bytes))||
       !owned_distinct(spans,*c))return MB_NEXUS_AUTH_BAD_ARGUMENT;
    if(c->busy)return finish(*c,r,MB_NEXUS_AUTH_BUSY);Busy busy(*c);uint64_t t=0;
    if(!now(*c,t))return finish(*c,r,MB_NEXUS_AUTH_SOURCE_FAILED);expire(*c,t);
    auto status=MB_NEXUS_AUTH_OK;
    if(!session(*c,view(p->cookie,p->cookie_bytes),t))status=MB_NEXUS_AUTH_SESSION_REQUIRED;
    else if(p->admission==MB_NEXUS_AUTH_ADMISSION_MUTATION)
        status=binding(*c,view(p->cookie,p->cookie_bytes),view(p->csrf,p->csrf_bytes),view(p->origin,p->origin_bytes),t);
    if(status==MB_NEXUS_AUTH_OK&&p->require_account) {
        if(!c->configured)status=MB_NEXUS_AUTH_NOT_CONFIGURED;
        else if(!c->active||t>=c->active->expires_at_monotonic_ns)status=MB_NEXUS_AUTH_RECONNECT_REQUIRED;
    }
    return finish(*c,r,status);
}
extern "C" int mb_nexus_auth_catalog_v1(mb_nexus_auth_context_v1 *c,const mb_nexus_auth_catalog_params_v1 *p,mb_nexus_auth_result_v1 *r) noexcept {
    Regions spans;
    if(!base_layout(c,p,r,spans)||!add_cookie(spans,p->cookie,p->cookie_bytes)||
       p->domain_bytes>MB_NEXUS_DOMAIN_LIMIT_BYTES||!spans.add(p->domain,p->domain_bytes)||
       !add_catalog_output(spans,p->output,p->output_capacity)||!owned_distinct(spans,*c)||
       !callback_context(spans,p->cancel_context,c))return MB_NEXUS_AUTH_BAD_ARGUMENT;
    Output output{p->output,p->output_capacity};
    if(c->busy)return finish(*c,r,MB_NEXUS_AUTH_BUSY);Busy busy(*c);
    if(!network(p->timeout_ms,p->connect_timeout_ms))return finish(*c,r,MB_NEXUS_AUTH_BAD_ARGUMENT);
    const auto domain=view(p->domain,p->domain_bytes);
    const bool identity_ok=
        (p->kind==MB_NEXUS_AUTH_CATALOG_GAMES&&domain.empty()&&!p->mod_id)||
        (p->kind==MB_NEXUS_AUTH_CATALOG_MODS&&catalog_domain(domain)&&!p->mod_id)||
        ((p->kind==MB_NEXUS_AUTH_CATALOG_MOD||p->kind==MB_NEXUS_AUTH_CATALOG_FILES)&&
         catalog_domain(domain)&&p->mod_id&&p->mod_id<=static_cast<uint64_t>(INT64_MAX))||
        (p->kind==MB_NEXUS_AUTH_CATALOG_REQUIREMENTS&&catalog_domain(domain)&&p->mod_id&&
         p->mod_id<=static_cast<uint64_t>(INT32_MAX));
    if(!identity_ok)return finish(*c,r,MB_NEXUS_AUTH_BAD_ARGUMENT);
    Operation op{*c,p->timeout_ms,p->connect_timeout_ms,p->cancel_context,p->cancelled};
    if(!op.start()||!op.check())return finish(*c,r,op.fault);
    expire(*c,c->last_mono);
    if(!session(*c,view(p->cookie,p->cookie_bytes),c->last_mono))return finish(*c,r,MB_NEXUS_AUTH_SESSION_REQUIRED);
    if(!c->configured)return finish(*c,r,MB_NEXUS_AUTH_NOT_CONFIGURED);
    if(!c->active)return finish(*c,r,MB_NEXUS_AUTH_RECONNECT_REQUIRED);
    auto status=token_gate(op,*c->active);size_t length=0;
    try {
        {
            auto work=std::make_unique<CatalogWork>();std::string rendered;TextErase erase_rendered{rendered};
            if(status==MB_NEXUS_AUTH_OK) {
                modbridge::Json records;
                if(p->kind==MB_NEXUS_AUTH_CATALOG_GAMES) {
                    status=catalog_request(op,*c->active,*work,p->kind,{},0,records);
                    if(status==MB_NEXUS_AUTH_OK)rendered=modbridge::Json{{"items",std::move(records)}}.dump();
                } else if(p->kind==MB_NEXUS_AUTH_CATALOG_MODS) {
                    status=catalog_request(op,*c->active,*work,p->kind,domain,0,records);
                    if(status==MB_NEXUS_AUTH_OK) {
                        bool needs_profile=false;for(const auto& record:records)needs_profile=needs_profile||adult_candidate(record);
                        bool allow_adult=false;if(needs_profile)status=fresh_adult_preference(op,*c->active,allow_adult);
                        if(status==MB_NEXUS_AUTH_OK) {
                            modbridge::Json visible=modbridge::Json::array();
                            for(auto& record:records)if(visible_mod(record,allow_adult))visible.push_back(std::move(record));
                            rendered=modbridge::Json{{"items",std::move(visible)}}.dump();
                        }
                    }
                } else {
                    status=catalog_request(op,*c->active,*work,MB_NEXUS_AUTH_CATALOG_MOD,domain,p->mod_id,records);
                    if(status==MB_NEXUS_AUTH_OK) {
                        if(!records.is_array()||records.size()!=1)status=MB_NEXUS_AUTH_PAYLOAD_FAILED;
                        else {
                            bool allow_adult=false;
                            if(!visible_mod(records[0],false)) {
                                if(adult_candidate(records[0]))status=fresh_adult_preference(op,*c->active,allow_adult);
                                if(status==MB_NEXUS_AUTH_OK&&!visible_mod(records[0],allow_adult))status=MB_NEXUS_AUTH_CONTENT_HIDDEN;
                            }
                            if(status==MB_NEXUS_AUTH_OK&&p->kind==MB_NEXUS_AUTH_CATALOG_MOD)
                                rendered=modbridge::Json{{"item",std::move(records[0])}}.dump();
                            else if(status==MB_NEXUS_AUTH_OK&&p->kind==MB_NEXUS_AUTH_CATALOG_FILES) {
                                modbridge::Json files;
                                status=catalog_request(op,*c->active,*work,MB_NEXUS_AUTH_CATALOG_FILES,domain,p->mod_id,files);
                                if(status==MB_NEXUS_AUTH_OK)rendered=modbridge::Json{{"items",std::move(files)}}.dump();
                            } else if(status==MB_NEXUS_AUTH_OK&&p->kind==MB_NEXUS_AUTH_CATALOG_REQUIREMENTS) {
                                modbridge::Json requirements;
                                status=requirements_request(op,*c->active,*work,domain,p->mod_id,requirements);
                                if(status==MB_NEXUS_AUTH_OK)
                                    rendered=modbridge::Json{{"requirements",std::move(requirements)}}.dump();
                            }
                        }
                    }
                }
            }
            if(status==MB_NEXUS_AUTH_OK) {
                length=rendered.size();
                if(length>=p->output_capacity)status=MB_NEXUS_AUTH_OUTPUT_TOO_SMALL;
                else if(!copy_output(p->output,p->output_capacity,rendered))status=MB_NEXUS_AUTH_OUTPUT_TOO_SMALL;
            }
        } // Erase and release decoded provider buffers and rendered bytes before publication gates.
        if(status==MB_NEXUS_AUTH_OK&&!op.check())status=op.fault;
        if(status==MB_NEXUS_AUTH_OK)status=token_gate(op,*c->active);
        if(status==MB_NEXUS_AUTH_OK&&!session(*c,view(p->cookie,p->cookie_bytes),c->last_mono))status=MB_NEXUS_AUTH_SESSION_REQUIRED;
        if(status==MB_NEXUS_AUTH_OK)output.committed=true;
    } catch(...) {status=MB_NEXUS_AUTH_INTERNAL_ERROR;}
    return finish(*c,r,status,status==MB_NEXUS_AUTH_OK?length:0);
}
namespace {
int acquisition_plan(
    mb_nexus_auth_context_v1 *c,const mb_nexus_auth_acquisition_plan_params_v1 *p,
    mb_nexus_auth_acquisition_capture_v1 *capture,mb_nexus_auth_result_v1 *r) noexcept {
    Regions spans;
    Range capture_span{};
    if(!base_layout(c,p,r,spans)||
       !add_binding(spans,p->cookie,p->cookie_bytes,p->csrf,p->csrf_bytes,p->origin,p->origin_bytes)||
       p->intent_bytes>MB_NEXUS_AUTH_ACQUISITION_INTENT_LIMIT||
       !spans.add(p->intent,p->intent_bytes)||
       !add_acquisition_output(spans,p->output,p->output_capacity)||
       (capture&&(!range(capture,sizeof(*capture),capture_span)||
           reinterpret_cast<uintptr_t>(capture)%alignof(mb_nexus_auth_acquisition_capture_v1)||
           capture->struct_size!=sizeof(*capture)||capture->abi_version!=MB_NEXUS_AUTH_ABI_V1||
           !capture->output_capacity||capture->output_capacity>MB_NEXUS_AUTH_ACQUISITION_CAPTURE_LIMIT||
           !spans.add(capture,sizeof(*capture))||!spans.add(capture->output,capture->output_capacity)))||
       !owned_distinct(spans,*c)||!callback_context(spans,p->cancel_context,c))
        return MB_NEXUS_AUTH_BAD_ARGUMENT;
    Output output{p->output,p->output_capacity};
    Output capture_output{capture?capture->output:nullptr,capture?capture->output_capacity:0};
    if(capture)capture->output_bytes=0;
    if(c->busy)return finish(*c,r,MB_NEXUS_AUTH_BUSY);
    Busy busy(*c);
    if(!network(p->timeout_ms,p->connect_timeout_ms))
        return finish(*c,r,MB_NEXUS_AUTH_BAD_ARGUMENT);

    Status status=MB_NEXUS_AUTH_BAD_ARGUMENT;size_t length=0,capture_length=0;
    try {
        AcquisitionIntent intent;
        if(!parse_acquisition_intent(p->intent,p->intent_bytes,intent))
            return finish(*c,r,MB_NEXUS_AUTH_BAD_ARGUMENT);
        Operation op{*c,p->timeout_ms,p->connect_timeout_ms,p->cancel_context,p->cancelled};
        if(!op.start()||!op.check())status=op.fault;
        else {
            expire(*c,c->last_mono);
            status=binding(*c,view(p->cookie,p->cookie_bytes),view(p->csrf,p->csrf_bytes),
                           view(p->origin,p->origin_bytes),c->last_mono);
            if(status==MB_NEXUS_AUTH_OK&&!c->configured)status=MB_NEXUS_AUTH_NOT_CONFIGURED;
            if(status==MB_NEXUS_AUTH_OK&&!c->active)status=MB_NEXUS_AUTH_RECONNECT_REQUIRED;
            if(status==MB_NEXUS_AUTH_OK)status=token_gate(op,*c->active);
            if(status==MB_NEXUS_AUTH_OK) {
                {
                    auto work=std::make_unique<CatalogWork>();
                    modbridge::Json plan,captured;
                    JsonErase erase_plan{plan},erase_captured{captured};
                    status=build_acquisition_plan(op,*c->active,*work,intent,plan,capture?&captured:nullptr);
                    if(status==MB_NEXUS_AUTH_OK) {
                        std::string rendered=modbridge::Json{{"plan",std::move(plan)}}.dump();
                        TextErase erase_rendered{rendered};
                        length=rendered.size();
                        if(length>=p->output_capacity)status=MB_NEXUS_AUTH_OUTPUT_TOO_SMALL;
                        else if(!copy_output(p->output,p->output_capacity,rendered))
                            status=MB_NEXUS_AUTH_OUTPUT_TOO_SMALL;
                        if(status==MB_NEXUS_AUTH_OK&&capture) {
                            std::string snapshot=captured.dump();TextErase erase_snapshot{snapshot};
                            capture_length=snapshot.size();
                            if(!copy_output(capture->output,capture->output_capacity,snapshot))
                                status=MB_NEXUS_AUTH_OUTPUT_TOO_SMALL;
                        }
                    }
                } // Erase plan, rendered output, and private provider buffers before publication gates.
                if(status==MB_NEXUS_AUTH_OK&&!op.check())status=op.fault;
                if(status==MB_NEXUS_AUTH_OK)status=token_gate(op,*c->active);
                if(status==MB_NEXUS_AUTH_OK) {
                    status=binding(*c,view(p->cookie,p->cookie_bytes),view(p->csrf,p->csrf_bytes),
                                   view(p->origin,p->origin_bytes),c->last_mono);
                }
                if(status==MB_NEXUS_AUTH_OK) {
                    output.committed=true;capture_output.committed=true;
                    if(capture)capture->output_bytes=capture_length;
                }
            }
        }
    } catch(...) {status=MB_NEXUS_AUTH_INTERNAL_ERROR;}
    return finish(*c,r,status,status==MB_NEXUS_AUTH_OK?length:0);
}
} // namespace
extern "C" int mb_nexus_auth_acquisition_plan_v1(
    mb_nexus_auth_context_v1 *c,const mb_nexus_auth_acquisition_plan_params_v1 *p,
    mb_nexus_auth_result_v1 *r) noexcept {
    return acquisition_plan(c,p,nullptr,r);
}
extern "C" int mb_nexus_auth_acquisition_plan_capture_v1(
    mb_nexus_auth_context_v1 *c,const mb_nexus_auth_acquisition_plan_params_v1 *p,
    mb_nexus_auth_acquisition_capture_v1 *capture,mb_nexus_auth_result_v1 *r) noexcept {
    if(!capture)return MB_NEXUS_AUTH_BAD_ARGUMENT;
    return acquisition_plan(c,p,capture,r);
}
extern "C" int mb_nexus_auth_resolve_download_v1(mb_nexus_auth_context_v1 *c,const mb_nexus_auth_resolve_params_v1 *p,mb_nexus_auth_resolve_result_v1 *r) noexcept {
    Regions spans;if(!base_layout(c,p,r,spans)||!add_binding(spans,p->cookie,p->cookie_bytes,p->csrf,p->csrf_bytes,p->origin,p->origin_bytes)||!p->download||reinterpret_cast<uintptr_t>(p->download)%alignof(mb_nexus_download_params_v1)||!spans.add(p->download,sizeof(*p->download)))return MB_NEXUS_AUTH_BAD_ARGUMENT;
    const auto& d=*p->download;
    if(d.struct_size!=sizeof(d)||d.abi_version!=1||d.auth_mode||d.credential||d.credential_bytes||d.ca_pem||d.ca_bytes||d.api_resolve||d.users_resolve||!d.raw_capacity||d.raw_capacity>MB_NEXUS_DOWNLOAD_RAW_LIMIT_BYTES||!d.report_capacity||d.report_capacity>MB_NEXUS_DOWNLOAD_REPORT_LIMIT_BYTES||!d.uri_capacity||d.uri_capacity>8192||d.domain_bytes>80||!spans.add(d.raw_body,d.raw_capacity)||!spans.add(d.metadata_report,d.report_capacity)||!spans.add(d.uri,d.uri_capacity)||!spans.add(d.domain,d.domain_bytes))return MB_NEXUS_AUTH_BAD_ARGUMENT;
    if(d.grant){if(reinterpret_cast<uintptr_t>(d.grant)%alignof(mb_nexus_download_grant_v1)||!spans.add(d.grant,sizeof(*d.grant))||d.grant->struct_size!=sizeof(*d.grant)||d.grant->abi_version!=1||d.grant->domain_bytes>80||d.grant->key_bytes>1024||!spans.add(d.grant->domain,d.grant->domain_bytes)||!spans.add(d.grant->key,d.grant->key_bytes))return MB_NEXUS_AUTH_BAD_ARGUMENT;}
    if(!owned_distinct(spans,*c)||!callback_context(spans,d.cancel_context,c))return MB_NEXUS_AUTH_BAD_ARGUMENT;
    Output raw{reinterpret_cast<char*>(d.raw_body),d.raw_capacity},report{d.metadata_report,d.report_capacity},uri{d.uri,d.uri_capacity};
    mb_nexus_auth_resolve_result_v1 out{};out.struct_size=sizeof(out);out.abi_version=1;out.auth=result(*c,MB_NEXUS_AUTH_BAD_ARGUMENT);out.download.struct_size=sizeof(out.download);out.download.abi_version=1;out.download.status=MB_NEXUS_DOWNLOAD_AUTH_REQUIRED;out.download.adult_content=-1;
    Status status=MB_NEXUS_AUTH_BAD_ARGUMENT;size_t length=0;
    if(c->busy)status=MB_NEXUS_AUTH_BUSY;
    else if(network(d.timeout_ms,d.connect_timeout_ms)) {
        Busy busy(*c);Operation op{*c,d.timeout_ms,d.connect_timeout_ms,d.cancel_context,d.cancelled};
        if(!op.start()||!op.check())status=op.fault;
        else {status=binding(*c,view(p->cookie,p->cookie_bytes),view(p->csrf,p->csrf_bytes),view(p->origin,p->origin_bytes),c->last_mono);
            if(status==MB_NEXUS_AUTH_OK&&!c->active)status=MB_NEXUS_AUTH_RECONNECT_REQUIRED;
            if(status==MB_NEXUS_AUTH_OK)status=token_gate(op,*c->active);
            if(status==MB_NEXUS_AUTH_OK) {
                auto request=d;request.auth_mode=MB_NEXUS_DOWNLOAD_BEARER;request.credential=c->active->access_token;request.credential_bytes=c->active->access_token_bytes;request.ca_pem=c->ca.data();request.ca_bytes=c->ca.size();request.api_resolve=c->api_resolve.empty()?nullptr:c->api_resolve.c_str();request.users_resolve=c->users_resolve.empty()?nullptr:c->users_resolve.c_str();request.cancel_context=&op;request.cancelled=cancelled;
                if(!op.remaining(request.timeout_ms,request.connect_timeout_ms))status=op.fault;
                else {int rc=mb_nexus_resolve_download_v1(&request,&out.download);
                    switch(rc){case MB_NEXUS_DOWNLOAD_OK:status=out.download.complete==1?MB_NEXUS_AUTH_OK:MB_NEXUS_AUTH_INTERNAL_ERROR;break;case MB_NEXUS_DOWNLOAD_BAD_ARGUMENT:status=MB_NEXUS_AUTH_BAD_ARGUMENT;break;case MB_NEXUS_DOWNLOAD_TIMEOUT:status=MB_NEXUS_AUTH_TIMEOUT;break;case MB_NEXUS_DOWNLOAD_CANCELLED:status=MB_NEXUS_AUTH_CANCELLED;break;case MB_NEXUS_DOWNLOAD_OUTPUT_TOO_SMALL:status=MB_NEXUS_AUTH_OUTPUT_TOO_SMALL;break;case MB_NEXUS_DOWNLOAD_TRANSPORT_FAILED:status=MB_NEXUS_AUTH_TRANSPORT_FAILED;break;case MB_NEXUS_DOWNLOAD_AUTH_REQUIRED:case MB_NEXUS_DOWNLOAD_CONTENT_HIDDEN:status=MB_NEXUS_AUTH_REFUSED;break;default:status=MB_NEXUS_AUTH_PAYLOAD_FAILED;break;}
                }
            }
        }
        wipe(d.raw_body,d.raw_capacity);wipe(d.metadata_report,d.report_capacity);raw.committed=report.committed=true;
        if(!op.check())status=op.fault;if(status==MB_NEXUS_AUTH_OK)status=token_gate(op,*c->active);
        if(status==MB_NEXUS_AUTH_OK)status=binding(*c,view(p->cookie,p->cookie_bytes),view(p->csrf,p->csrf_bytes),view(p->origin,p->origin_bytes),c->last_mono);
        if(status==MB_NEXUS_AUTH_OK){uri.committed=true;length=out.download.uri_bytes;}
        else{out.download.complete=0;out.download.uri_bytes=0;if(status==MB_NEXUS_AUTH_TIMEOUT)out.download.status=MB_NEXUS_DOWNLOAD_TIMEOUT;else if(status==MB_NEXUS_AUTH_CANCELLED)out.download.status=MB_NEXUS_DOWNLOAD_CANCELLED;else if(status==MB_NEXUS_AUTH_RECONNECT_REQUIRED)out.download.status=MB_NEXUS_DOWNLOAD_AUTH_REQUIRED;}
    }
    out.auth=result(*c,status);if(status==MB_NEXUS_AUTH_OK){out.auth.complete=1;out.auth.output_bytes=length;}*r=out;return status;
}
