#include "jobs.h"
#include "download/curl_download.h"
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
#include <sys/stat.h>
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
};
struct Intent { char domain[81]{},key[81]{};size_t domain_bytes=0,key_bytes=0;uint64_t mod=0,file=0; };
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
    Status parse(Intent& out) noexcept {
        if(!take('{'))return MB_NEXUS_JOBS_BAD_ARGUMENT;
        unsigned seen=0;
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
                if(bit==1&&value!="download_analyze")return MB_NEXUS_JOBS_REFUSED;
                if(bit==2) {
                    if(!domain(value))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    std::memcpy(out.domain,value.data(),value.size());out.domain_bytes=value.size();
                }
                if(bit==16) {
                    if(!key(value))return MB_NEXUS_JOBS_BAD_ARGUMENT;
                    std::memcpy(out.key,value.data(),value.size());out.key_bytes=value.size();
                }
            }
            space();if(position<text.size()&&text[position]=='}'){++position;break;}
            if(!take(','))return MB_NEXUS_JOBS_BAD_ARGUMENT;
        }
        space();return seen==31&&position==text.size()?MB_NEXUS_JOBS_OK:MB_NEXUS_JOBS_BAD_ARGUMENT;
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
Json public_job(const Record& record) {
    auto mod=std::to_string(record.mod_id),file=std::to_string(record.file_id);
    Json out={{"id",record.id},{"kind","download_analyze"},{"game",record.domain},{"mod_id",record.mod_id},{"file_id",record.file_id},
        {"label","Mod "+mod+" \xc2\xb7 file "+file},{"status",state_name(record.state.value)},
        {"stage",record.state.value==State::Analyzed?"analyzed":record.downloaded?"downloaded":"requested"},
        {"compatibility","unresolved"},{"source_url","https://www.nexusmods.com/"+std::string(record.domain)+"/mods/"+mod+"?tab=files&file_id="+file},
        {"runtime_supported",false},{"installation_supported",false}};
    if(record.downloaded){out["sha256"]=hex(record.sha.data(),record.sha.size());out["bytes_downloaded"]=record.bytes;}
    if(record.error!=Error::None)out["error"]={{"code",error_code(record.error)},{"message",error_message(record.error)}};
    if(record.state.value==State::Analyzed)out["analysis_report"]=Json::parse(record.report,record.report+record.report_bytes);
    return out;
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
};

namespace {
bool valid(const mb_nexus_jobs_context_v1 *c) noexcept {return aligned(c)&&c->marker==magic;}
bool owned_distinct(const mb_nexus_jobs_context_v1& c,const Regions& r) noexcept {
    return r.distinct()&&r.outside(&c,sizeof(c))&&r.outside(c.ca.data(),c.ca.size())&&
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
    if(!cleanup_file(c,record)){c.storage_failed=true;error=Error::Storage;authorization=false;}
    record.error=error;record.state.value=authorization?State::NeedsAuthorization:State::Failed;
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
    for(size_t i=0;i<c->count;++i) {
        auto& record=c->records[i];bindings_clear(record);wipe(record.key,sizeof(record.key));wipe(record.owner.data(),record.owner.size());
        wipe(record.report,sizeof(record.report));record.report_bytes=0;
        if(!cleanup_file(*c,record))cleaned=false;
    }
    if(!cleaned){c->storage_failed=true;return MB_NEXUS_JOBS_STORAGE_FAILED;}
    for(auto& record:c->records)wipe(&record,sizeof(record));
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
                if(record.mod_id!=intent.mod||record.file_id!=intent.file||std::string_view(record.domain,record.domain_bytes)!=std::string_view(intent.domain,intent.domain_bytes))
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
        std::memcpy(candidate.domain,intent.domain,intent.domain_bytes);candidate.domain_bytes=intent.domain_bytes;
        std::memcpy(candidate.key,intent.key,intent.key_bytes);candidate.key_bytes=intent.key_bytes;
        candidate.mod_id=intent.mod;candidate.file_id=intent.file;candidate.owner=owner;
        std::memcpy(candidate.cookie,p->cookie,43);std::memcpy(candidate.csrf,p->csrf,43);
        std::memcpy(candidate.origin,p->origin,p->origin_bytes);candidate.origin_bytes=p->origin_bytes;
        status=serialize(public_job(candidate),p->output,p->output_capacity,result);
        if(status!=MB_NEXUS_JOBS_OK)return finish(result,out,status);
        status=admission(*c,p->cookie,p->cookie_bytes,p->csrf,p->csrf_bytes,p->origin,p->origin_bytes,true);
        if(status!=MB_NEXUS_JOBS_OK){result.output_bytes=0;return finish(result,out,status);}
        c->records[c->count++]=candidate;counts(*c,result,&owner);output.published=true;return finish(result,out,MB_NEXUS_JOBS_OK);
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
    if(!selected){counts(*c,result);return finish(result,out,MB_NEXUS_JOBS_OK);}
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
                resolution.download.status==MB_NEXUS_DOWNLOAD_AUTH_REQUIRED;
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
        struct stat observed{};
        if(!directory_valid(*c)||fstat(record.fd,&observed)!=0||!file_private(observed)||!file_identity(record,observed)||static_cast<uint64_t>(observed.st_size)!=record.bytes||!close_file(*c,record)) {
            fail_record(*c,record,Error::Storage);counts(*c,result);return finish(result,out,c->storage_failed?MB_NEXUS_JOBS_STORAGE_FAILED:MB_NEXUS_JOBS_OK);
        }
        record.state.value=State::Analyzed;record.error=Error::None;counts(*c,result);return finish(result,out,MB_NEXUS_JOBS_OK);
    }catch(...){fail_record(*c,record,Error::Internal);counts(*c,result);return finish(result,out,c->storage_failed?MB_NEXUS_JOBS_STORAGE_FAILED:MB_NEXUS_JOBS_OK);}
}
