#include "auth.h"
#include "oauth_code.h"
#include "download/provider_transfer.h"
#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <time.h>
#include <vector>

namespace {
constexpr uint64_t sec=1000000000;
uint64_t mono=100*sec;
unsigned delayed_clock_sample=0;uint64_t delayed_clock_ns=0;
bool clock_failure=false,advance_work_delete=false,last_work_zero=true,last_token_zero=true;
unsigned work_deletes=0,token_deletes=0,exchanges=0,user_requests=0,mod_requests=0,link_requests=0,sequence=0,last_credential=0,controls=0;
bool fail_exchange=false,fail_user=false,expire_after_user=false,timeout_after_user=false,expire_after_link=false,timeout_after_link=false,rfc_vector=false,verifier_matches=false;
unsigned token_ttl=600;
struct Allocation {void *pointer;size_t size;};
Allocation allocation[512]{};
void *context_to_inspect=nullptr;
bool context_erased=true;
char cookie_to_inspect[44]{},csrf_to_inspect[44]{};
constexpr size_t work_size=65536+131072+16384;
bool zero(const void *p,size_t n){auto b=static_cast<const unsigned char*>(p);for(size_t i=0;i<n;++i)if(b[i])return false;return true;}
bool contains(const void *p,size_t n,const char *text,size_t bytes){auto b=static_cast<const char*>(p);if(!bytes||bytes>n)return false;for(size_t i=0;i+bytes<=n;++i)if(!std::memcmp(b+i,text,bytes))return true;return false;}
void require(bool condition,const char *label){if(!condition)throw std::runtime_error(label);}
void observed(bool condition,const char *label){require(condition,label);++controls;}
void reset_provider(){fail_exchange=fail_user=expire_after_user=timeout_after_user=expire_after_link=timeout_after_link=false;advance_work_delete=false;token_ttl=600;}
struct Entropy {unsigned calls=0;bool fail=false,vector=false;};
int entropy(void *opaque,uint8_t output[32]){
 auto &e=*static_cast<Entropy*>(opaque);++e.calls;if(e.fail){std::memset(output,0x77,32);return 1;}
 if(e.vector&&e.calls==4){
  constexpr std::array<uint8_t,32> input{0x74,0x18,0xdf,0xb4,0x97,0x99,0xe0,0x25,0x4f,0xfa,0x60,0x7d,0xd8,0xad,0xbb,0xba,0x16,0xd4,0x25,0x4d,0x69,0xd6,0xbf,0xf0,0x5b,0x58,0x05,0x58,0x53,0x84,0x8d,0x79};std::copy(input.begin(),input.end(),output);
 }else for(unsigned i=0;i<32;++i)output[i]=static_cast<uint8_t>(e.calls+i*7);
 return 0;
}
struct Fixture {
 mb_nexus_auth_context_v1 *ctx=nullptr;Entropy ent;
 std::string origin="http://127.0.0.1:18291",client="owned_fixture_client",redirect=origin+"/auth/callback",scope="openid profile email";
 std::array<uint8_t,4> ca{'C','A','\n',0};std::array<char,44> cookie{},csrf{};mb_nexus_auth_result_v1 out{};
 Fixture(bool configured=true,bool vector=false){
  mono+=sec;reset_provider();ent.vector=vector;mb_nexus_auth_create_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.origin=origin.data();p.origin_bytes=origin.size();p.entropy_context=&ent;p.entropy_read32=entropy;
  if(configured){p.client_id=client.data();p.client_id_bytes=client.size();p.redirect_uri=redirect.data();p.redirect_uri_bytes=redirect.size();p.scope=scope.data();p.scope_bytes=scope.size();p.ca_pem=ca.data();p.ca_bytes=ca.size();}
  require(mb_nexus_auth_create_v1(&p,&ctx,&out)==0,"fixture create");require(bootstrap()==0,"fixture bootstrap");
 }
 ~Fixture(){mb_nexus_auth_free_v1(&ctx);}
 int bootstrap(const char *input=nullptr,size_t bytes=0){mb_nexus_auth_session_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.cookie=input;p.cookie_bytes=bytes;p.cookie_output=cookie.data();p.cookie_capacity=cookie.size();p.csrf_output=csrf.data();p.csrf_capacity=csrf.size();return mb_nexus_auth_session_v1(ctx,&p,&out);}
 int start(std::array<char,8192>& url,const char *alternate=nullptr,const char *cross_origin=nullptr,size_t cap=8192){mb_nexus_auth_start_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.cookie=cookie.data();p.cookie_bytes=43;p.csrf=alternate?alternate:csrf.data();p.csrf_bytes=43;p.origin=cross_origin?cross_origin:origin.data();p.origin_bytes=cross_origin?std::strlen(cross_origin):origin.size();p.authorization_url=url.data();p.authorization_capacity=cap;return mb_nexus_auth_start_v1(ctx,&p,&out);}
 std::string target(std::string_view url,std::string_view code="owned_fake_code"){
  auto a=url.find("&state=");require(a!=url.npos,"private state extraction");a+=7;auto b=url.find('&',a);return "/auth/callback?state="+std::string(url.substr(a,b-a))+"&code="+std::string(code);
 }
 int callback(const std::string &target,std::array<char,8192>& summary,const char *browser=nullptr,uint32_t timeout=30000,size_t cap=8192,mb_nexus_auth_cancel_fn_v1 cancel=nullptr,void *opaque=nullptr){
  mb_nexus_auth_callback_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.cookie=browser?browser:cookie.data();p.cookie_bytes=43;p.target=target.data();p.target_bytes=target.size();p.timeout_ms=timeout;p.connect_timeout_ms=std::min(1000u,timeout);p.summary=summary.data();p.summary_capacity=cap;p.cancelled=cancel;p.cancel_context=opaque;return mb_nexus_auth_callback_v1(ctx,&p,&out);
 }
 int account(std::array<char,8192>& summary,uint32_t timeout=30000,mb_nexus_auth_cancel_fn_v1 cancel=nullptr,void *opaque=nullptr,size_t cap=8192){
  mb_nexus_auth_account_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.cookie=cookie.data();p.cookie_bytes=43;p.timeout_ms=timeout;p.connect_timeout_ms=std::min(1000u,timeout);p.summary=summary.data();p.summary_capacity=cap;p.cancelled=cancel;p.cancel_context=opaque;return mb_nexus_auth_account_v1(ctx,&p,&out);
 }
 int disconnect(const char *alternate=nullptr){mb_nexus_auth_disconnect_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.cookie=cookie.data();p.cookie_bytes=43;p.csrf=alternate?alternate:csrf.data();p.csrf_bytes=43;p.origin=origin.data();p.origin_bytes=origin.size();return mb_nexus_auth_disconnect_v1(ctx,&p,&out);}
 void login(){std::array<char,8192> url{},summary{};require(start(url)==0,"private login start");require(callback(target(url.data()),summary)==0,"private login callback");}
 int resolve(std::vector<uint8_t>& raw,std::vector<char>& report,std::array<char,8192>& uri,mb_nexus_auth_resolve_result_v1& result,uint32_t timeout=30000){
  std::string domain="skyrimspecialedition";mb_nexus_download_params_v1 d{};d.struct_size=sizeof(d);d.abi_version=1;d.domain=domain.data();d.domain_bytes=domain.size();d.mod_id=1;d.file_id=2;d.timeout_ms=timeout;d.connect_timeout_ms=std::min(1000u,timeout);d.raw_body=raw.data();d.raw_capacity=raw.size();d.metadata_report=report.data();d.report_capacity=report.size();d.uri=uri.data();d.uri_capacity=uri.size();mb_nexus_auth_resolve_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.cookie=cookie.data();p.cookie_bytes=43;p.csrf=csrf.data();p.csrf_bytes=43;p.origin=origin.data();p.origin_bytes=origin.size();p.download=&d;return mb_nexus_auth_resolve_download_v1(ctx,&p,&result);
 }
};
int throws_cancel(void*,uint64_t){throw std::runtime_error("owned callback");}
int normal_cancel(void*,uint64_t){return 1;}
struct Reentrant {mb_nexus_auth_context_v1 **ctx;int code=-1;};
int free_during_callback(void *p,uint64_t){auto& r=*static_cast<Reentrant*>(p);r.code=mb_nexus_auth_free_v1(r.ctx);return 0;}
}
void *operator new(std::size_t bytes){void *p=std::malloc(bytes?bytes:1);if(!p)throw std::bad_alloc();for(auto& a:allocation)if(!a.pointer){a={p,bytes};break;}return p;}
void operator delete(void *p) noexcept {
 if(!p)return;for(auto& a:allocation)if(a.pointer==p){
  if(a.size==work_size){last_work_zero=last_work_zero&&zero(p,a.size);++work_deletes;if(advance_work_delete){mono+=sec;advance_work_delete=false;}}
  if(a.size==sizeof(mb_nexus_oauth_tokens_v1)){last_token_zero=last_token_zero&&zero(p,a.size);++token_deletes;}
  if(p==context_to_inspect){context_erased=!contains(p,a.size,cookie_to_inspect,43)&&!contains(p,a.size,csrf_to_inspect,43);context_to_inspect=nullptr;}
  a={};break;
 }std::free(p);
}
extern "C" int auth_control_clock_gettime(clockid_t kind,timespec *out){if(kind!=CLOCK_MONOTONIC||clock_failure){errno=0;return -1;}mono+=1000;if(delayed_clock_sample&&!--delayed_clock_sample)mono+=delayed_clock_ns;out->tv_sec=static_cast<time_t>(mono/sec);out->tv_nsec=static_cast<long>(mono%sec);return 0;}
extern "C" int mb_nexus_oauth_exchange_code_v1(const mb_nexus_oauth_exchange_params_v1 *p,mb_nexus_oauth_exchange_result_v1 *out) noexcept {
 ++exchanges;*out={};out->struct_size=sizeof(*out);out->abi_version=1;out->status=MB_NEXUS_OAUTH_CODE_TOKEN_REJECTED;std::memset(p->tokens,0,sizeof(*p->tokens));
 if(fail_exchange){fail_exchange=false;return out->status;}
 if(rfc_vector)verifier_matches=std::string_view(p->fields.code_verifier,p->fields.code_verifier_bytes)=="dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
 ++sequence;std::string access="owned_fake_access_"+std::to_string(sequence),refresh="owned_fake_refresh_"+std::to_string(sequence);auto& t=*p->tokens;t.access_token_bytes=static_cast<uint32_t>(access.size());t.refresh_token_bytes=static_cast<uint32_t>(refresh.size());t.expires_in_seconds=token_ttl;t.expires_at_monotonic_ns=mono+token_ttl*sec;std::memcpy(t.access_token,access.c_str(),access.size()+1);std::memcpy(t.refresh_token,refresh.c_str(),refresh.size()+1);
 std::memset(p->raw_body,0,p->raw_capacity);std::memset(p->form,0,p->form_capacity);out->status=MB_NEXUS_OAUTH_CODE_OK;out->complete=1;return 0;
}
extern "C" int mb_curl_provider_transfer_v1(const mb_curl_provider_transfer_params_v1 *p,mb_curl_provider_transfer_result_v1 *out){
 *out={};out->struct_size=sizeof(*out);out->abi_version=1;out->status=MB_CURL_DOWNLOAD_TRANSPORT_FAILED;
 std::string_view credential(p->credential,p->credential_bytes);constexpr std::string_view prefix="owned_fake_access_";if(p->auth_mode!=2||!credential.starts_with(prefix))return out->status;last_credential=static_cast<unsigned>(std::strtoul(p->credential+prefix.size(),nullptr,10));
 bool user=std::string_view(p->url)=="https://users.nexusmods.com/oauth/userinfo";bool link=std::string_view(p->url).find("/download_link")!=std::string_view::npos;
 const char *body=nullptr;
 if(user){++user_requests;if(fail_user){fail_user=false;std::memset(p->body,0,p->body_capacity);return out->status;}body="{\"sub\":\"42\",\"name\":\"Fixture User\",\"membership_roles\":[\"premium\"],\"preferences\":{\"adult\":true}}";}
 else if(link){++link_requests;body="[{\"name\":\"Owned mirror\",\"short_name\":\"OWN\",\"URI\":\"https://owned-cdn.invalid/fixture.zip?owned_signature=fake\"}]";}
 else{++mod_requests;body="{\"mod_id\":1,\"game_id\":1704,\"domain_name\":\"skyrimspecialedition\",\"name\":\"Fixture Mod\",\"author\":\"Fixture\",\"available\":true,\"status\":\"published\",\"contains_adult_content\":false}";}
 size_t bytes=std::strlen(body);if(bytes>p->body_capacity)return out->status;std::memcpy(p->body,body,bytes);int consumer=p->consume_complete_body(p->consume_context,p->body,bytes);std::memset(p->body,0,p->body_capacity);out->http_status=200;out->received_bytes=bytes;
 if(user&&expire_after_user){mono+=token_ttl*sec+sec;expire_after_user=false;}if(user&&timeout_after_user){mono+=sec;timeout_after_user=false;}if(link&&expire_after_link){mono+=token_ttl*sec+sec;expire_after_link=false;}if(link&&timeout_after_link){mono+=sec;timeout_after_link=false;}
 out->status=consumer?MB_CURL_DOWNLOAD_CONSUMER_FAILED:MB_CURL_DOWNLOAD_OK;out->complete=!consumer;return out->status;
}
int main(){try{
 Fixture f;f.login();std::array<char,8192> url{},summary{};require(f.start(url)==0,"pending boundary start");auto target=f.target(url.data());mono+=300*sec-100000000;unsigned before=exchanges;unsigned old=sequence;delayed_clock_sample=4;delayed_clock_ns=200000000;
 int code=f.callback(target,summary);bool no_exchange=exchanges==before;bool old_present=f.out.credentials_present==1;bool erased=zero(summary.data(),summary.size());reset_provider();bool old_preserved=f.account(summary)==0&&last_credential==old;
 std::cout<<"{\"callback_status\":"<<code<<",\"exchange_delta\":"<<(exchanges-before)<<",\"old_present\":"<<old_present<<",\"old_preserved\":"<<old_preserved<<",\"failed_output_erased\":"<<erased<<",\"scope\":\"private_clock_exchange_provider_boundary\"}\n";
 require(code==MB_NEXUS_AUTH_REFUSED&&no_exchange&&old_present&&old_preserved&&erased,"expired pending must not exchange");return 0;
 }catch(const std::exception& e){std::cerr<<"control failed: "<<e.what()<<"\n";return 1;}}
