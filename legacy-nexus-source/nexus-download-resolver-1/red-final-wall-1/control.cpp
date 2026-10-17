#include "nexus_download.h"
#include "nexus_client.h"
#include "download/provider_transfer.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <time.h>
using Json=nlohmann::json;
enum Mode { NORMAL, FREE_USER, ADULT_MOD, UNKNOWN_ADULT, HIDDEN_MOD, WRONG_MOD,
            BAD_PROFILE, BAD_LINK_LAST, EMPTY_LINK, LATE_TIMEOUT, LATE_EXPIRY,
            LATE_OWNER, LINK_TIMEOUT, LINK_THROW, PROFILE_TIMEOUT, CLIENT_SHORT };
static char *observed_report=nullptr;static size_t observed_capacity=0;
struct Control { Mode mode=NORMAL; int profile=0,mod=0,link=0; uint64_t mono=0;
                 uint64_t wall=1700000000; bool premium=true,adult=true,cancel=false;
                 bool throw_cancel=false, late_callback_expiry=false, link_returned=false, expire_after_final_wipe=false, delay_final_wall=false; } control;
extern "C" int resolver_control_clock_gettime(clockid_t which, struct timespec *out) {
    if(which==CLOCK_MONOTONIC&&control.link_returned&&control.expire_after_final_wipe) {
        bool cleared=true;for(size_t i=0;i<observed_capacity;++i) if(observed_report[i]) cleared=false;
        if(cleared) control.mono=6000000000;
    }
    if(which==CLOCK_REALTIME&&control.link_returned&&control.delay_final_wall) {
        bool cleared=true;for(size_t i=0;i<observed_capacity;++i) if(observed_report[i]) cleared=false;
        if(cleared) control.mono=6000000000;
    }
    out->tv_sec=which==CLOCK_MONOTONIC ? 100+control.mono/1000000000 : control.wall;
    out->tv_nsec=which==CLOCK_MONOTONIC ? control.mono%1000000000 : 0;
    return 0;
}
static bool zero(const void *p,size_t n) { auto *b=(const unsigned char*)p; return std::all_of(b,b+n,[](auto c){return c==0;}); }
static int cancel(void*,uint64_t) {
    if(control.throw_cancel) throw std::runtime_error("private callback control");
    if(control.late_callback_expiry&&control.link_returned) control.wall=1700001000;
    return control.cancel;
}
extern "C" int mb_nexus_client_request_v1(const mb_nexus_client_params_v1 *p, mb_nexus_client_result_v1 *r) noexcept {
    *r={};r->struct_size=sizeof(*r);r->abi_version=1;
    assert(p->timeout_ms&&p->timeout_ms<=5000&&p->connect_timeout_ms<=p->timeout_ms);
    if(p->cancelled&&p->cancelled(p->cancel_context,0)) {
        r->status=MB_NEXUS_CLIENT_TRANSPORT_FAILED;return r->status;
    }
    Json body;
    if(p->kind==MB_NEXUS_MOD) {
        ++control.mod;
        assert(p->mod_id==7&&std::string_view(p->domain,p->domain_bytes)=="skyrim");
        body={{"status","decoded"},{"records",Json::array({{
            {"mod_id",control.mode==WRONG_MOD?8:7},{"game_domain","skyrim"},
            {"available",control.mode!=HIDDEN_MOD},{"status","published"},
            {"contains_adult_content",control.mode==UNKNOWN_ADULT?Json(nullptr):Json(control.mode==ADULT_MOD)}
        }})}};
    } else {
        ++control.profile;
        assert((p->auth_mode==1&&p->kind==MB_NEXUS_API_KEY_USER)||(p->auth_mode==2&&p->kind==MB_NEXUS_OAUTH_USER));
        assert(p->domain_bytes==0&&p->domain==nullptr&&p->mod_id==0);
        if(control.mode==PROFILE_TIMEOUT) {r->status=MB_NEXUS_CLIENT_TRANSPORT_FAILED;r->curl_code=28;return r->status;}
        body={{"status","decoded"},{"profile",{
            {"user_id",42},{"is_premium",control.mode!=FREE_USER&&control.premium},
            {"adult_content",p->auth_mode==1?Json(nullptr):Json(control.adult)}
        }}};
        if(control.mode==BAD_PROFILE) body["profile"]["is_premium"]=nullptr;
    }
    const std::string text=body.dump();
    if(p->report_capacity<=text.size()||control.mode==CLIENT_SHORT) {
        r->status=MB_NEXUS_CLIENT_OUTPUT_TOO_SMALL;r->report_bytes=text.size();return r->status;
    }
    std::memcpy(p->report,text.c_str(),text.size()+1);
    std::memset(p->raw_body,0,p->raw_capacity);
    r->status=MB_NEXUS_CLIENT_OK;r->complete=r->metadata_decoded=1;r->report_bytes=text.size();return r->status;
}
extern "C" int mb_curl_provider_transfer_v1(const mb_curl_provider_transfer_params_v1 *p, mb_curl_provider_transfer_result_v1 *r) {
    ++control.link;
    assert(p->timeout_ms&&p->timeout_ms<=5000&&p->connect_timeout_ms<=p->timeout_ms);
    assert(std::string_view(p->url).starts_with("https://api.nexusmods.com/v1/games/skyrim/mods/7/files/9/download_link"));
    *r={};r->struct_size=sizeof(*r);r->abi_version=1;r->http_status=200;
    std::string text=R"([{"URI":"https://cdn.example/archive.zip?token=private-owned-fixture","name":"one","short_name":"one"}])";
    if(control.mode==BAD_LINK_LAST) text=R"([{"URI":"https://cdn.example/ok","name":"one","short_name":"one"},{"URI":"http://cdn.example/bad","name":"two","short_name":"two"}])";
    if(control.mode==EMPTY_LINK) text="[]";
    assert(text.size()<=p->body_capacity);std::memcpy(p->body,text.data(),text.size());
    int consumer=p->consume_complete_body(p->consume_context,p->body,text.size());
    r->status=consumer?MB_CURL_DOWNLOAD_CONSUMER_FAILED:MB_CURL_DOWNLOAD_OK;r->complete=consumer?0:1;
    if(control.mode==LATE_TIMEOUT) control.mono+=6000000000;
    if(control.mode==LATE_EXPIRY) control.wall=1700001000;
    if(control.mode==LATE_OWNER) {r->status=MB_CURL_DOWNLOAD_OWNER_FAILED;r->complete=0;}
    if(control.mode==LINK_TIMEOUT) {r->status=MB_CURL_DOWNLOAD_TIMEOUT;r->complete=0;}
    std::memset(p->body,0,p->body_capacity);control.link_returned=true;
    if(control.mode==LINK_THROW) throw std::runtime_error("private provider control");
    return r->status;
}
struct Fixture {
    std::array<uint8_t,4096> raw;
    std::array<char,4096> report;
    std::array<char,8192> uri;
    std::array<char,32> secret{}, domain{}, grant_domain{}, key{};
    std::array<uint8_t,8> ca{};
    mb_nexus_download_grant_v1 grant{};
    mb_nexus_download_params_v1 p{};
    mb_nexus_download_result_v1 r{};
    Fixture() {
        raw.fill(0xa5);report.fill('x');uri.fill('x');std::memset(&r,0xa5,sizeof(r));
        std::memcpy(secret.data(),"private-test-only",17);std::memcpy(domain.data(),"skyrim",6);
        std::memcpy(grant_domain.data(),"skyrim",6);std::memcpy(key.data(),"p+?%",4);
        p.struct_size=sizeof(p);p.abi_version=1;p.auth_mode=MB_NEXUS_DOWNLOAD_BEARER;
        p.credential=secret.data();p.credential_bytes=17;p.domain=domain.data();p.domain_bytes=6;
        p.mod_id=7;p.file_id=9;p.ca_pem=ca.data();p.ca_bytes=ca.size();p.timeout_ms=5000;p.connect_timeout_ms=1000;
        p.raw_body=raw.data();p.raw_capacity=raw.size();p.metadata_report=report.data();p.report_capacity=report.size();
        p.uri=uri.data();p.uri_capacity=uri.size();p.cancelled=cancel;
        grant.struct_size=sizeof(grant);grant.abi_version=1;grant.domain=grant_domain.data();grant.domain_bytes=6;
        grant.mod_id=7;grant.file_id=9;grant.user_id=42;grant.expires=1700000500;grant.key=key.data();grant.key_bytes=4;
    }
};
static int cases=0;
static void run(const char *name,Fixture& f,mb_nexus_download_status_v1 expected,int profile,int mod,int link) {
    int status=mb_nexus_resolve_download_v1(&f.p,&f.r);
    if(status!=expected||f.r.status!=expected||control.profile!=profile||control.mod!=mod||control.link!=link) {
        std::cerr<<name<<" status="<<status<<" counts="<<control.profile<<","<<control.mod<<","<<control.link<<"\n";std::abort();
    }
    assert(f.r.struct_size==sizeof(f.r)&&f.r.abi_version==1&&f.r.complete==(expected==0));
    assert(zero(f.raw.data(),f.p.raw_capacity)&&zero(f.report.data(),f.p.report_capacity));
    if(expected==0) {assert(f.r.uri_bytes==std::strlen(f.uri.data())&&f.r.uri_bytes>0);assert(f.r.user_id==42&&f.r.mod_id==7&&f.r.file_id==9);}
    else {assert(f.r.uri_bytes==0&&zero(f.uri.data(),f.p.uri_capacity));}
    ++cases;
}
static void untouched(const char *name,Fixture& f) {
    auto raw=f.raw;auto report=f.report;auto uri=f.uri;auto result=f.r;auto params=f.p;
    int status=mb_nexus_resolve_download_v1(&f.p,&f.r);
    if(status!=MB_NEXUS_DOWNLOAD_BAD_ARGUMENT) {std::cerr<<name<<" admission="<<status<<"\n";std::abort();}
    assert(f.raw==raw&&f.report==report&&f.uri==uri&&std::memcmp(&f.r,&result,sizeof(result))==0&&std::memcmp(&f.p,&params,sizeof(params))==0);
    assert(control.profile==0&&control.mod==0&&control.link==0);++cases;
}
int main() {
#define RESET control={};
    {RESET Fixture f;run("premium",f,MB_NEXUS_DOWNLOAD_OK,1,1,1);assert(f.r.grant_used==0&&f.r.is_premium==1);}
    {RESET Fixture f;f.p.auth_mode=1;run("API premium",f,MB_NEXUS_DOWNLOAD_OK,1,1,1);assert(f.r.adult_content==-1);}
    {RESET Fixture f;control.mode=FREE_USER;run("free denied",f,MB_NEXUS_DOWNLOAD_AUTH_REQUIRED,1,0,0);}
    {RESET Fixture f;control.mode=FREE_USER;f.p.grant=&f.grant;run("website grant",f,MB_NEXUS_DOWNLOAD_OK,1,1,1);assert(f.r.grant_used==1&&f.r.is_premium==0);}
    {RESET Fixture f;f.p.grant=&f.grant;f.grant.user_id=43;run("wrong user",f,MB_NEXUS_DOWNLOAD_AUTH_REQUIRED,1,0,0);}
    {RESET Fixture f;f.p.grant=&f.grant;f.grant.mod_id=8;run("wrong source",f,MB_NEXUS_DOWNLOAD_AUTH_REQUIRED,0,0,0);}
    {RESET Fixture f;f.p.grant=&f.grant;f.grant.expires=control.wall;run("expired grant",f,MB_NEXUS_DOWNLOAD_AUTH_REQUIRED,0,0,0);}
    {RESET Fixture f;control.mode=ADULT_MOD;f.p.auth_mode=1;run("unknown adult preference",f,MB_NEXUS_DOWNLOAD_CONTENT_HIDDEN,1,1,0);}
    {RESET Fixture f;control.mode=ADULT_MOD;control.adult=false;run("adult preference false",f,MB_NEXUS_DOWNLOAD_CONTENT_HIDDEN,1,1,0);}
    {RESET Fixture f;control.mode=ADULT_MOD;run("adult preference true",f,MB_NEXUS_DOWNLOAD_OK,1,1,1);}
    {RESET Fixture f;control.mode=UNKNOWN_ADULT;run("unknown adult marker",f,MB_NEXUS_DOWNLOAD_CONTENT_HIDDEN,1,1,0);}
    {RESET Fixture f;control.mode=HIDDEN_MOD;run("hidden mod",f,MB_NEXUS_DOWNLOAD_CONTENT_HIDDEN,1,1,0);}
    {RESET Fixture f;control.mode=WRONG_MOD;run("wrong decoded source",f,MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED,1,1,0);}
    {RESET Fixture f;control.mode=BAD_PROFILE;run("bad live profile",f,MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED,1,0,0);}
    {RESET Fixture f;control.mode=BAD_LINK_LAST;run("invalid last mirror",f,MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED,1,1,1);}
    {RESET Fixture f;control.mode=EMPTY_LINK;run("empty mirrors",f,MB_NEXUS_DOWNLOAD_PAYLOAD_FAILED,1,1,1);}
    {RESET Fixture f;f.p.uri_capacity=8;run("short URI",f,MB_NEXUS_DOWNLOAD_OUTPUT_TOO_SMALL,1,1,1);}
    {RESET Fixture f;control.mode=CLIENT_SHORT;run("short report",f,MB_NEXUS_DOWNLOAD_OUTPUT_TOO_SMALL,1,0,0);}
    {RESET Fixture f;control.mode=LATE_TIMEOUT;run("late deadline",f,MB_NEXUS_DOWNLOAD_TIMEOUT,1,1,1);}
    {RESET Fixture f;control.mode=LATE_TIMEOUT;f.p.uri_capacity=8;run("late deadline overrides short",f,MB_NEXUS_DOWNLOAD_TIMEOUT,1,1,1);}
    {RESET Fixture f;control.mode=LATE_EXPIRY;f.p.grant=&f.grant;run("late grant expiry",f,MB_NEXUS_DOWNLOAD_AUTH_REQUIRED,1,1,1);}
    {RESET Fixture f;control.late_callback_expiry=true;f.p.grant=&f.grant;run("expiry after cleanup callback",f,MB_NEXUS_DOWNLOAD_AUTH_REQUIRED,1,1,1);}
    {RESET Fixture f;control.mode=LATE_OWNER;run("late owner failure",f,MB_NEXUS_DOWNLOAD_TRANSPORT_FAILED,1,1,1);}
    {RESET Fixture f;control.mode=LINK_TIMEOUT;run("provider timeout",f,MB_NEXUS_DOWNLOAD_TIMEOUT,1,1,1);}
    {RESET Fixture f;control.mode=PROFILE_TIMEOUT;run("profile timeout",f,MB_NEXUS_DOWNLOAD_TIMEOUT,1,0,0);}
    {RESET Fixture f;control.mode=LINK_THROW;run("provider exception",f,MB_NEXUS_DOWNLOAD_INTERNAL_ERROR,1,1,1);}
    {RESET Fixture f;control.cancel=true;run("cancel",f,MB_NEXUS_DOWNLOAD_CANCELLED,0,0,0);}
    {RESET Fixture f;control.throw_cancel=true;run("cancel exception",f,MB_NEXUS_DOWNLOAD_CANCELLED,0,0,0);}
    {RESET Fixture f;f.p.auth_mode=99;run("bad auth",f,MB_NEXUS_DOWNLOAD_BAD_ARGUMENT,0,0,0);}
    {RESET Fixture f;f.secret[0]=' ';run("bad credential",f,MB_NEXUS_DOWNLOAD_BAD_ARGUMENT,0,0,0);}
    {RESET Fixture f;f.domain[0]='S';run("bad domain",f,MB_NEXUS_DOWNLOAD_BAD_ARGUMENT,0,0,0);}
    {RESET Fixture f;f.p.metadata_report=reinterpret_cast<char*>(f.raw.data());untouched("raw/report alias",f);}
    {RESET Fixture f;f.p.grant=&f.grant;f.grant.domain=f.domain.data();untouched("nested domain alias",f);}
    {RESET Fixture f;f.p.grant=&f.grant;f.grant.key=f.uri.data();untouched("nested key/output alias",f);}
    {RESET Fixture f;f.p.credential=reinterpret_cast<char*>(UINTPTR_MAX-4);f.p.credential_bytes=8;untouched("address overflow",f);}
    {RESET Fixture f;f.p.cancel_context=f.uri.data();untouched("callback/output alias",f);}
    {RESET Fixture f;f.p.raw_capacity=MB_NEXUS_DOWNLOAD_RAW_LIMIT_BYTES+1;untouched("raw cap",f);}
    {RESET Fixture f;observed_report=f.report.data();observed_capacity=f.report.size();control.expire_after_final_wipe=true;run("final cleanup budget",f,MB_NEXUS_DOWNLOAD_TIMEOUT,1,1,1);}
    {RESET Fixture f;observed_report=f.report.data();observed_capacity=f.report.size();control.delay_final_wall=true;f.p.grant=&f.grant;run("final wall budget",f,MB_NEXUS_DOWNLOAD_TIMEOUT,1,1,1);}
    assert(mb_nexus_resolve_download_v1(nullptr,nullptr)==MB_NEXUS_DOWNLOAD_BAD_ARGUMENT);++cases;
    std::cout<<"{\"pure_boundary_cases\":"<<cases<<",\"network\":false,\"TLS\":false,\"actual_core_writer_selector\":true}"<<std::endl;
}
