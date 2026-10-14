#include "oauth_code.h"
#include "download/oauth_code_transfer.h"
#include "modbridge/nexus.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <span>
#include <string_view>
#include <time.h>

// The owner failure primitive has no crypto types. Keep this C++ boundary
// independent of owner.h's file-sink/MbedTLS context layout and C profile.
extern "C" void mb_curl_owner_fail_v1(void);

namespace {
using Status=mb_nexus_oauth_code_status_v1;
constexpr std::uint64_t ns_per_second=1000000000,ns_per_ms=1000000;
static_assert(MB_NEXUS_OAUTH_FORM_LIMIT_BYTES==MB_CURL_OAUTH_CODE_FORM_MAX);
static_assert(MB_NEXUS_OAUTH_RAW_LIMIT_BYTES==MB_CURL_OAUTH_CODE_BODY_MAX);
static_assert(MB_NEXUS_OAUTH_TIMEOUT_LIMIT_MS==MB_CURL_OAUTH_CODE_TIMEOUT_MAX_MS);
void clear(void *data,std::size_t bytes) noexcept {
    auto *p=static_cast<volatile unsigned char*>(data);
    while(bytes--) *p++=0;
}
struct Region { const void *data; std::size_t bytes; };
bool valid(const Region& r) noexcept {
    return !r.bytes || (r.data && reinterpret_cast<std::uintptr_t>(r.data)<=
        std::numeric_limits<std::uintptr_t>::max()-r.bytes);
}
bool overlap(const Region& a,const Region& b) noexcept {
    if(!a.bytes || !b.bytes) return false;
    const auto x=reinterpret_cast<std::uintptr_t>(a.data),y=reinterpret_cast<std::uintptr_t>(b.data);
    return x<y+b.bytes && y<x+a.bytes;
}
template<std::size_t N> bool independent(const std::array<Region,N>& regions) noexcept {
    for(const auto& r:regions) if(!valid(r)) return false;
    for(std::size_t i=0;i<N;++i) for(std::size_t j=i+1;j<N;++j)
        if(overlap(regions[i],regions[j])) return false;
    return true;
}
bool field_sizes(const mb_nexus_oauth_code_fields_v1& f) noexcept {
    return f.client_id_bytes<=MB_NEXUS_OAUTH_CLIENT_LIMIT_BYTES &&
        f.redirect_uri_bytes<=MB_NEXUS_OAUTH_REDIRECT_LIMIT_BYTES &&
        f.code_bytes<=MB_NEXUS_OAUTH_CODE_LIMIT_BYTES &&
        f.code_verifier_bytes<=MB_NEXUS_OAUTH_VERIFIER_LIMIT_BYTES;
}
std::string_view view(const char *p,std::size_t n) noexcept { return p?std::string_view(p,n):std::string_view{}; }
bool layout(const mb_nexus_oauth_exchange_params_v1& p,
            const mb_nexus_oauth_exchange_result_v1 *out) noexcept {
    if(p.struct_size!=sizeof(p) || p.abi_version!=MB_NEXUS_OAUTH_CODE_ABI_V1 ||
       !p.form_capacity || p.form_capacity>MB_NEXUS_OAUTH_FORM_LIMIT_BYTES ||
       !p.raw_capacity || p.raw_capacity>MB_NEXUS_OAUTH_RAW_LIMIT_BYTES ||
       !p.tokens || reinterpret_cast<std::uintptr_t>(p.tokens)%alignof(mb_nexus_oauth_tokens_v1) ||
       p.ca_bytes>MB_NEXUS_OAUTH_CA_LIMIT_BYTES || !field_sizes(p.fields)) return false;
    std::array<Region,11> r{{{&p,sizeof(p)},{out,out?sizeof(*out):0},
        {p.form,p.form_capacity},{p.raw_body,p.raw_capacity},{p.tokens,sizeof(*p.tokens)},
        {p.fields.client_id,p.fields.client_id_bytes},{p.fields.redirect_uri,p.fields.redirect_uri_bytes},
        {p.fields.code,p.fields.code_bytes},{p.fields.code_verifier,p.fields.code_verifier_bytes},
        {p.ca_pem,p.ca_bytes},{nullptr,0}}};
    if(!independent(r)) return false;
    if(p.users_resolve) {
        const Region first{p.users_resolve,1};
        if(!valid(first)) return false;
        for(const auto& span:r) if(overlap(first,span)) return false;
        std::size_t bytes=0;
        while(bytes<8192 && p.users_resolve[bytes]) ++bytes;
        if(bytes==8192) return false;
        r.back()={p.users_resolve,bytes+1};
        if(!independent(r)) return false;
    }
    if(p.cancel_context) {
        const Region context{p.cancel_context,1};
        if(!valid(context)) return false;
        for(std::size_t i=1;i<=4;++i) if(overlap(context,r[i])) return false;
    }
    return true;
}
bool resolve_valid(const char *text) noexcept {
    if(!text) return true;
    constexpr std::string_view prefix="users.nexusmods.com:443:";
    const std::string_view mapping(text);
    if(!mapping.starts_with(prefix) || mapping.size()==prefix.size()) return false;
    for(unsigned char c:mapping)
        if(c<33 || c>126 || c=='/' || c=='@' || c=='#' || c=='?' || c==';' || c=='*') return false;
    return true;
}
struct Operation {
    const mb_nexus_oauth_exchange_params_v1& p;
    std::uint64_t started_at=0,deadline=0,last=0;
    bool started=false;
    Status fault=MB_NEXUS_OAUTH_CODE_OK;
    bool clock(std::uint64_t& now) noexcept {
        timespec value{};errno=0;
        const int rc=clock_gettime(CLOCK_MONOTONIC,&value),error=errno;
        constexpr auto max_seconds=(INT64_MAX-999999999-
            static_cast<std::int64_t>(MB_CURL_DOWNLOAD_TIMEOUT_MAX_MS)*1000000)/1000000000;
        if(rc || error || value.tv_sec<0 || value.tv_sec>max_seconds ||
           value.tv_nsec<0 || value.tv_nsec>=static_cast<long>(ns_per_second)) {
            mb_curl_owner_fail_v1();fault=MB_NEXUS_OAUTH_CODE_INTERNAL_ERROR;return false;
        }
        now=static_cast<std::uint64_t>(value.tv_sec)*ns_per_second+static_cast<std::uint64_t>(value.tv_nsec);
        if(started && now<last) {
            mb_curl_owner_fail_v1();fault=MB_NEXUS_OAUTH_CODE_INTERNAL_ERROR;return false;
        }
        last=now;
        if(started && now>=deadline && fault!=MB_NEXUS_OAUTH_CODE_INTERNAL_ERROR)
            fault=MB_NEXUS_OAUTH_CODE_TIMEOUT;
        return fault==MB_NEXUS_OAUTH_CODE_OK;
    }
    bool start() noexcept {
        if(!clock(started_at)) return false;
        const auto budget=static_cast<std::uint64_t>(p.timeout_ms)*ns_per_ms;
        if(budget>std::numeric_limits<std::uint64_t>::max()-started_at) {
            mb_curl_owner_fail_v1();fault=MB_NEXUS_OAUTH_CODE_INTERNAL_ERROR;return false;
        }
        deadline=started_at+budget;started=true;return true;
    }
    bool checkpoint(std::uint64_t received=0) noexcept {
        std::uint64_t now=0;
        if(!clock(now)) return false;
        if(p.cancelled) {
            try { if(p.cancelled(p.cancel_context,received)) fault=MB_NEXUS_OAUTH_CODE_CANCELLED; }
            catch(...) { fault=MB_NEXUS_OAUTH_CODE_CANCELLED; }
        }
        return clock(now); // Include caller callback cost and reject backwards time.
    }
    bool remaining(std::uint32_t& timeout,std::uint32_t& connect) noexcept {
        if(!checkpoint()) return false;
        const auto millis=(deadline-last)/ns_per_ms;
        if(!millis) { fault=MB_NEXUS_OAUTH_CODE_TIMEOUT;return false; }
        timeout=static_cast<std::uint32_t>(std::min<std::uint64_t>(millis,p.timeout_ms));
        connect=std::min(timeout,p.connect_timeout_ms);return true;
    }
};
struct Consumer {
    const mb_nexus_oauth_exchange_params_v1& p;
    Operation& op;
    mb_nexus_oauth_tokens_v1 staged{};
    Status status=MB_NEXUS_OAUTH_CODE_TOKEN_REJECTED;
    bool accepted=false;
};
int consume(void *context,const std::uint8_t *body,std::size_t bytes) noexcept {
    auto& c=*static_cast<Consumer*>(context);
    try {
        if(body!=c.p.raw_body || bytes>c.p.raw_capacity || c.accepted) return 1;
        std::size_t access=0,refresh=0;
        if(!modbridge::nexus_decode_oauth_tokens({body,bytes},c.staged.access_token,access,
              c.staged.refresh_token,refresh,c.staged.expires_in_seconds)) return 1;
        const auto lifetime=static_cast<std::uint64_t>(c.staged.expires_in_seconds)*ns_per_second;
        if(lifetime>std::numeric_limits<std::uint64_t>::max()-c.op.started_at) {
            c.status=MB_NEXUS_OAUTH_CODE_INTERNAL_ERROR;return 1;
        }
        c.staged.access_token_bytes=static_cast<std::uint32_t>(access);
        c.staged.refresh_token_bytes=static_cast<std::uint32_t>(refresh);
        c.staged.expires_at_monotonic_ns=c.op.started_at+lifetime;
        c.status=MB_NEXUS_OAUTH_CODE_OK;c.accepted=true;return 0;
    } catch(...) { c.status=MB_NEXUS_OAUTH_CODE_INTERNAL_ERROR;return 1; }
}
int cancelled(void *context,std::uint64_t received) noexcept {
    return !static_cast<Operation*>(context)->checkpoint(received);
}
Status transport_status(const mb_curl_oauth_code_transfer_result_v1& result,
                        int returned,const Consumer& consumer) noexcept {
    if(result.struct_size!=sizeof(result) || result.abi_version!=MB_CURL_DOWNLOAD_ABI_V1 ||
       returned!=static_cast<int>(result.status)) return MB_NEXUS_OAUTH_CODE_TRANSPORT_FAILED;
    switch(result.status) {
    case MB_CURL_DOWNLOAD_OK:
        return result.complete==1 && consumer.accepted?MB_NEXUS_OAUTH_CODE_OK:MB_NEXUS_OAUTH_CODE_TRANSPORT_FAILED;
    case MB_CURL_DOWNLOAD_BAD_ARGUMENT:return MB_NEXUS_OAUTH_CODE_BAD_ARGUMENT;
    case MB_CURL_DOWNLOAD_BAD_STATE:return MB_NEXUS_OAUTH_CODE_BAD_STATE;
    case MB_CURL_DOWNLOAD_CANCELLED:return MB_NEXUS_OAUTH_CODE_CANCELLED;
    case MB_CURL_DOWNLOAD_TIMEOUT:return MB_NEXUS_OAUTH_CODE_TIMEOUT;
    case MB_CURL_DOWNLOAD_CONSUMER_FAILED:return consumer.status==MB_NEXUS_OAUTH_CODE_OK?
        MB_NEXUS_OAUTH_CODE_TRANSPORT_FAILED:consumer.status;
    default:return MB_NEXUS_OAUTH_CODE_TRANSPORT_FAILED;
    }
}
} // namespace

extern "C" int mb_nexus_oauth_write_code_form_v1(const mb_nexus_oauth_code_fields_v1 *fields,
    char *output,std::size_t capacity,std::size_t *written) noexcept {
    if(!fields || !written || reinterpret_cast<std::uintptr_t>(fields)%alignof(mb_nexus_oauth_code_fields_v1) ||
       reinterpret_cast<std::uintptr_t>(written)%alignof(std::size_t) ||
       capacity>MB_NEXUS_OAUTH_FORM_LIMIT_BYTES || !field_sizes(*fields)) return MB_NEXUS_OAUTH_CODE_BAD_ARGUMENT;
    const std::array<Region,7> r{{{fields,sizeof(*fields)},{output,capacity},{written,sizeof(*written)},
        {fields->client_id,fields->client_id_bytes},{fields->redirect_uri,fields->redirect_uri_bytes},
        {fields->code,fields->code_bytes},{fields->code_verifier,fields->code_verifier_bytes}}};
    if(!independent(r)) return MB_NEXUS_OAUTH_CODE_BAD_ARGUMENT;
    return modbridge::nexus_oauth_code_form(view(fields->client_id,fields->client_id_bytes),
        view(fields->redirect_uri,fields->redirect_uri_bytes),view(fields->code,fields->code_bytes),
        view(fields->code_verifier,fields->code_verifier_bytes),{output,capacity},*written)?
        MB_NEXUS_OAUTH_CODE_OK:MB_NEXUS_OAUTH_CODE_BAD_ARGUMENT;
}

extern "C" int mb_nexus_oauth_exchange_code_v1(const mb_nexus_oauth_exchange_params_v1 *params,
    mb_nexus_oauth_exchange_result_v1 *out) noexcept {
    if(!params || reinterpret_cast<std::uintptr_t>(params)%alignof(mb_nexus_oauth_exchange_params_v1) ||
       (out && reinterpret_cast<std::uintptr_t>(out)%alignof(mb_nexus_oauth_exchange_result_v1)) || !layout(*params,out))
        return MB_NEXUS_OAUTH_CODE_BAD_ARGUMENT;
    const auto& p=*params;
    mb_nexus_oauth_exchange_result_v1 result{};
    result.struct_size=sizeof(result);result.abi_version=MB_NEXUS_OAUTH_CODE_ABI_V1;
    result.status=MB_NEXUS_OAUTH_CODE_BAD_ARGUMENT;result.transport_status=MB_CURL_DOWNLOAD_BAD_ARGUMENT;
    result.curl_code=-1;
    const bool arguments=out && p.ca_bytes && p.timeout_ms && p.timeout_ms<=MB_NEXUS_OAUTH_TIMEOUT_LIMIT_MS &&
        p.connect_timeout_ms && p.connect_timeout_ms<=p.timeout_ms && resolve_valid(p.users_resolve);
    Operation op{p};
    const bool started=arguments && op.start();
    // Full initial erasure is inside the operation budget when arguments permit
    // provider work. Semantic refusals still erase independent private storage.
    clear(p.form,p.form_capacity);clear(p.raw_body,p.raw_capacity);clear(p.tokens,sizeof(*p.tokens));
    if(!arguments) { if(out) *out=result;return result.status; }
    Consumer consumer{p,op};
    if(!started) result.status=op.fault;
    else {
        std::size_t form_bytes=0;
        std::uint32_t timeout=0,connect=0;
        if(!op.checkpoint()) result.status=op.fault;
        else if(mb_nexus_oauth_write_code_form_v1(&p.fields,p.form,p.form_capacity,&form_bytes))
            result.status=MB_NEXUS_OAUTH_CODE_BAD_ARGUMENT;
        else if(!op.remaining(timeout,connect)) result.status=op.fault;
        else {
            mb_curl_oauth_code_transfer_params_v1 transfer{};
            mb_curl_oauth_code_transfer_result_v1 transferred{};
            transfer.struct_size=sizeof(transfer);transfer.abi_version=MB_CURL_DOWNLOAD_ABI_V1;
            transfer.form=p.form;transfer.form_bytes=form_bytes;transfer.ca_pem=p.ca_pem;transfer.ca_bytes=p.ca_bytes;
            transfer.resolve=p.users_resolve;transfer.timeout_ms=timeout;transfer.connect_timeout_ms=connect;
            transfer.cancel_context=&op;transfer.cancelled=cancelled;
            transfer.body=p.raw_body;transfer.body_capacity=p.raw_capacity;
            transfer.consume_context=&consumer;transfer.consume_complete_body=consume;
            const int rc=mb_curl_oauth_code_transfer_v1(&transfer,&transferred);
            result.status=transport_status(transferred,rc,consumer);
            result.transport_status=static_cast<std::uint32_t>(transferred.status);
            result.http_status=transferred.http_status;result.curl_code=transferred.curl_code;
            result.received_bytes=transferred.received_bytes;
            if(result.status==MB_NEXUS_OAUTH_CODE_OK)
                std::memcpy(p.tokens,&consumer.staged,sizeof(*p.tokens));
            clear(&transfer,sizeof(transfer));clear(&transferred,sizeof(transferred));
        }
    }
    // No owning secret scratch or costly destructor cleanup follows publication.
    clear(&consumer.staged,sizeof(consumer.staged));
    clear(p.form,p.form_capacity);clear(p.raw_body,p.raw_capacity);
    if(started) (void)op.checkpoint(result.received_bytes);
    if(op.fault!=MB_NEXUS_OAUTH_CODE_OK &&
       (op.fault==MB_NEXUS_OAUTH_CODE_INTERNAL_ERROR || result.transport_status!=MB_CURL_DOWNLOAD_OWNER_FAILED))
        result.status=op.fault;
    if(op.fault==MB_NEXUS_OAUTH_CODE_INTERNAL_ERROR) result.transport_status=MB_CURL_DOWNLOAD_OWNER_FAILED;
    if(result.status==MB_NEXUS_OAUTH_CODE_OK && op.last>=p.tokens->expires_at_monotonic_ns)
        result.status=MB_NEXUS_OAUTH_CODE_TOKEN_REJECTED;
    if(result.status!=MB_NEXUS_OAUTH_CODE_OK) {
        clear(p.tokens,sizeof(*p.tokens));
        if(started) {
            std::uint64_t now=0;(void)op.clock(now);
            if(op.fault==MB_NEXUS_OAUTH_CODE_INTERNAL_ERROR) {
                result.status=op.fault;result.transport_status=MB_CURL_DOWNLOAD_OWNER_FAILED;
            } else if(op.fault==MB_NEXUS_OAUTH_CODE_TIMEOUT && result.transport_status!=MB_CURL_DOWNLOAD_OWNER_FAILED)
                result.status=op.fault;
        }
    } else result.complete=1;
    *out=result;return result.status;
}
