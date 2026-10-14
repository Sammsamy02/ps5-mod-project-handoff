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
extern "C" int auth_control_clock_gettime(clockid_t kind,timespec *out){if(kind!=CLOCK_MONOTONIC||clock_failure){errno=0;return -1;}mono+=1000;out->tv_sec=static_cast<time_t>(mono/sec);out->tv_nsec=static_cast<long>(mono%sec);return 0;}
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
 {
  Fixture f(false);std::array<char,8192> url{},summary{};observed(f.out.configured==0,"unconfigured flag");observed(f.start(url)==MB_NEXUS_AUTH_NOT_CONFIGURED&&zero(url.data(),url.size()),"unconfigured start");observed(f.account(summary)==0&&std::string_view(summary.data()).find("disconnected")!=std::string_view::npos,"unconfigured disconnected account");
 }
 {
  Fixture f(true,true);rfc_vector=true;std::array<char,8192> url{},summary{};observed(f.start(url)==0&&std::string_view(url.data()).find("code_challenge=E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM")!=std::string_view::npos,"RFC7636 known challenge");auto t=f.target(url.data());observed(f.callback(t,summary)==0&&verifier_matches,"RFC7636 private verifier");rfc_vector=false;observed(std::string_view(summary.data()).find("Fixture User")!=std::string_view::npos&&std::string_view(summary.data()).find("owned_fake_")==std::string_view::npos&&std::string_view(summary.data()).find("code_verifier")==std::string_view::npos,"public summary no secret");unsigned before=user_requests;observed(f.account(summary)==0&&user_requests==before+1,"account live recheck");observed(f.callback(t,summary)==MB_NEXUS_AUTH_REFUSED&&zero(summary.data(),summary.size()),"callback replay refused");
 }
 {
  Fixture f;std::array<char,8192> url{},summary{};std::array<char,44> wrong{};wrong.fill('z');observed(f.start(url,wrong.data())==MB_NEXUS_AUTH_REFUSED&&zero(url.data(),url.size()),"wrong csrf");observed(f.start(url,nullptr,"http://127.0.0.1:18292")==MB_NEXUS_AUTH_REFUSED,"cross origin");observed(f.start(url)==0,"start transaction");auto first=f.target(url.data());char cookie[44];std::memcpy(cookie,f.cookie.data(),44);observed(f.bootstrap()==0,"second browser bootstrap");observed(f.start(url)==MB_NEXUS_AUTH_BUSY,"other browser busy");unsigned before=exchanges;observed(f.callback(first,summary)==MB_NEXUS_AUTH_REFUSED&&exchanges==before,"wrong browser callback");std::memcpy(f.cookie.data(),cookie,44);require(f.bootstrap(cookie,43)==0,"existing browser csrf restore");observed(f.start(url)==0,"same browser restart");auto current=f.target(url.data());observed(f.callback(first,summary)==MB_NEXUS_AUTH_REFUSED&&exchanges==before,"restarted old state refused");observed(f.callback(current,summary)==0,"current state accepted");
 }
 {
  Fixture f;std::array<char,8192> url{},summary{};require(f.start(url)==0,"grammar start");auto t=f.target(url.data());unsigned before=exchanges;auto duplicate=t+"&%73tate=bad";observed(f.callback(duplicate,summary)==MB_NEXUS_AUTH_REFUSED&&exchanges==before,"decoded duplicate refused");observed(f.callback(t+"&extra=x",summary)==MB_NEXUS_AUTH_REFUSED,"extra field refused");observed(f.callback(f.target(url.data(),"%00"),summary)==MB_NEXUS_AUTH_REFUSED,"NUL code refused");observed(f.callback(f.target(url.data(),"a+b"),summary)==MB_NEXUS_AUTH_REFUSED,"space code refused");auto cancelled=t.substr(0,t.find("&code="))+"&error=access_denied";observed(f.callback(cancelled,summary)==MB_NEXUS_AUTH_AUTHORIZATION_CANCELLED&&exchanges==before,"denial no token POST");observed(f.callback(t,summary)==MB_NEXUS_AUTH_REFUSED,"denial consumes state");
 }
 {
  Fixture f;std::array<char,8192> url{},summary{};require(f.start(url)==0,"expiry start");auto t=f.target(url.data());mono+=300*sec;unsigned before=exchanges;observed(f.callback(t,summary)==MB_NEXUS_AUTH_REFUSED&&exchanges==before,"pending expires at 300s");
 }
 {
  Fixture f;for(unsigned i=1;i<16;++i)require(f.bootstrap()==0,"cap bootstrap");observed(f.bootstrap()==MB_NEXUS_AUTH_BUSY,"16-browser cap");mono+=1800*sec;observed(f.bootstrap()==0&&f.out.new_session==1,"expired browser slot reused");
 }
 {
  Fixture f;std::array<char,8192> url{},summary{};require(f.start(url)==0,"short start");auto t=f.target(url.data());f.login();unsigned old=sequence;require(f.start(url)==0,"replace start");t=f.target(url.data());fail_user=true;observed(f.callback(t,summary)==MB_NEXUS_AUTH_TRANSPORT_FAILED&&f.out.credentials_present==1&&zero(summary.data(),summary.size()),"failed userinfo keeps account");observed(f.account(summary)==0&&last_credential==old,"old bearer preserved");require(f.start(url)==0,"short replacement start");observed(f.callback(f.target(url.data()),summary,nullptr,30000,8)==MB_NEXUS_AUTH_OUTPUT_TOO_SMALL&&zero(summary.data(),8)&&f.out.credentials_present==1,"short summary no replacement");observed(f.account(summary)==0&&last_credential==old,"short summary old bearer");fail_user=true;observed(f.account(summary)==MB_NEXUS_AUTH_TRANSPORT_FAILED&&zero(summary.data(),summary.size())&&f.out.credentials_present==1,"failed live GET no verified summary");observed(f.start(url,nullptr,nullptr,8)==MB_NEXUS_AUTH_OUTPUT_TOO_SMALL&&zero(url.data(),8),"short authorize URL erased");
 }
 {
  Fixture f;std::array<char,8192> url{},summary{};require(f.start(url)==0,"exchange failure start");fail_exchange=true;unsigned before=exchanges;auto t=f.target(url.data());observed(f.callback(t,summary)==MB_NEXUS_AUTH_PAYLOAD_FAILED&&f.out.credentials_present==0,"bad token no account");observed(f.callback(t,summary)==MB_NEXUS_AUTH_REFUSED&&exchanges==before+1,"exchange failure consumed once");
 }
 {
  Fixture f;std::array<char,8192> summary{};observed(f.account(summary,30000,throws_cancel)==MB_NEXUS_AUTH_CANCELLED&&zero(summary.data(),summary.size()),"throwing cancel closed");observed(f.account(summary,30000,normal_cancel)==MB_NEXUS_AUTH_CANCELLED,"cancel closed");Reentrant re{&f.ctx,-1};observed(f.account(summary,30000,free_during_callback,&re)==0&&re.code==MB_NEXUS_AUTH_BUSY&&f.ctx,"reentrant free busy");
 }
 {
  Fixture f;std::array<char,8192> summary{};summary.fill('Q');auto old=f.out;observed(f.account(summary,30000,nullptr,summary.data())==MB_NEXUS_AUTH_BAD_ARGUMENT&&summary[0]=='Q'&&!std::memcmp(&old,&f.out,sizeof(old)),"cancel context alias untouched");mb_nexus_auth_session_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.cookie=f.cookie.data();p.cookie_bytes=43;p.cookie_output=f.cookie.data();p.cookie_capacity=44;p.csrf_output=f.csrf.data();p.csrf_capacity=44;auto saved=f.cookie;observed(mb_nexus_auth_session_v1(f.ctx,&p,&f.out)==MB_NEXUS_AUTH_BAD_ARGUMENT&&f.cookie==saved,"cookie alias untouched");p.cookie=nullptr;p.cookie_bytes=0;p.cookie_output=reinterpret_cast<char*>(UINTPTR_MAX-10);observed(mb_nexus_auth_session_v1(f.ctx,&p,&f.out)==MB_NEXUS_AUTH_BAD_ARGUMENT,"address overflow refused");observed(f.account(summary,30000,nullptr,f.ctx)==MB_NEXUS_AUTH_BAD_ARGUMENT&&summary[0]=='Q',"owned context alias untouched");
 }
 {
  Fixture f;std::array<char,8192> url{},summary{};f.login();unsigned old=sequence;require(f.start(url)==0,"late cleanup start");advance_work_delete=true;observed(f.callback(f.target(url.data()),summary,nullptr,150)==MB_NEXUS_AUTH_TIMEOUT&&zero(summary.data(),summary.size())&&f.out.credentials_present==1&&last_work_zero,"cleanup before final timeout gate");observed(f.account(summary)==0&&last_credential==old,"late cleanup preserves old account");
 }
 {
  Fixture f;std::array<char,8192> url{},summary{};token_ttl=1;require(f.start(url)==0,"late expiry start");expire_after_user=true;observed(f.callback(f.target(url.data()),summary)==MB_NEXUS_AUTH_RECONNECT_REQUIRED&&zero(summary.data(),summary.size())&&f.out.credentials_present==0,"candidate expiry after live reply");reset_provider();f.login();mono+=601*sec;unsigned before=user_requests;observed(f.account(summary)==MB_NEXUS_AUTH_RECONNECT_REQUIRED&&user_requests==before&&zero(summary.data(),summary.size()),"stored expiry blocks live GET");
 }
 {
  Fixture f;f.login();std::vector<uint8_t> raw(262144,0x44);std::vector<char> report(131072,'P');std::array<char,8192> uri{};mb_nexus_auth_resolve_result_v1 r{};int resolved=f.resolve(raw,report,uri,r);observed(resolved==0&&r.download.complete==1&&r.download.user_id==42&&zero(raw.data(),raw.size())&&zero(report.data(),report.size()),"actual resolver injects private bearer");timeout_after_link=true;observed(f.resolve(raw,report,uri,r,150)==MB_NEXUS_AUTH_TIMEOUT&&r.download.complete==0&&r.download.uri_bytes==0&&zero(uri.data(),uri.size()),"resolver late deadline erases URI");observed(f.resolve(raw,report,uri,r)==0,"resolver healthy after timeout");token_ttl=1;f.login();expire_after_link=true;observed(f.resolve(raw,report,uri,r)==MB_NEXUS_AUTH_RECONNECT_REQUIRED&&r.download.complete==0&&r.download.uri_bytes==0&&zero(uri.data(),uri.size()),"resolver stored expiry erases URI");
 }
 {
  Fixture f;f.login();std::array<char,8192> summary{};unsigned before=user_requests;std::array<char,44> wrong{};wrong.fill('z');observed(f.disconnect(wrong.data())==MB_NEXUS_AUTH_REFUSED&&f.out.credentials_present==1,"disconnect binding refused");observed(f.disconnect()==0&&f.out.credentials_present==0,"disconnect account erased");observed(f.account(summary)==0&&user_requests==before&&std::string_view(summary.data()).find("disconnected")!=std::string_view::npos,"disconnected account no provider");std::memcpy(cookie_to_inspect,f.cookie.data(),44);std::memcpy(csrf_to_inspect,f.csrf.data(),44);context_to_inspect=f.ctx;observed(mb_nexus_auth_free_v1(&f.ctx)==0&&!f.ctx&&context_erased&&last_token_zero,"free secrets erased");observed(mb_nexus_auth_free_v1(&f.ctx)==0,"free null idempotent");
 }
 {
  Fixture f;std::array<char,8192> url{};f.ent.fail=true;observed(f.start(url)==MB_NEXUS_AUTH_SOURCE_FAILED&&zero(url.data(),url.size()),"entropy refusal terminal");f.ent.fail=false;observed(f.start(url)==MB_NEXUS_AUTH_SOURCE_FAILED,"entropy source no retry");
 }
 {
  Fixture f;std::array<char,8192> summary{};mono-=sec;observed(f.account(summary)==MB_NEXUS_AUTH_SOURCE_FAILED&&zero(summary.data(),summary.size()),"backward monotonic terminal");
 }
 {
  Fixture f;std::array<char,8192> summary{};mono+=1800*sec;unsigned before=user_requests;observed(f.account(summary)==MB_NEXUS_AUTH_SESSION_REQUIRED&&user_requests==before&&zero(summary.data(),summary.size()),"expired session blocked");
 }
 {
  Fixture f;std::array<char,8192> summary{};clock_failure=true;observed(f.account(summary)==MB_NEXUS_AUTH_SOURCE_FAILED&&zero(summary.data(),summary.size()),"clock refusal closed");clock_failure=false;observed(f.account(summary)==MB_NEXUS_AUTH_SOURCE_FAILED,"clock source no retry");
 }
 {
  std::string origin="http://127.0.0.1:18291",client="public_test",redirect=origin+"/auth/callback",scope="openid";std::array<uint8_t,4> ca{'C','A','\n',0};Entropy ent;mb_nexus_auth_context_v1 *ctx=nullptr;mb_nexus_auth_result_v1 result{};mb_nexus_auth_create_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.origin=origin.data();p.origin_bytes=origin.size();p.client_id=client.data();p.client_id_bytes=client.size();p.redirect_uri=redirect.data();p.redirect_uri_bytes=redirect.size();p.scope=scope.data();p.scope_bytes=scope.size();p.ca_pem=ca.data();p.ca_bytes=ca.size();p.entropy_context=&ent;p.entropy_read32=entropy;
  observed(mb_nexus_auth_create_v1(&p,&ctx,&result)==MB_NEXUS_AUTH_BAD_ARGUMENT&&!ctx,"borrowed client identity refused");client="owned_fixture_client";p.client_id=client.data();p.client_id_bytes=client.size();redirect="http://127.0.0.1:18292/auth/callback";p.redirect_uri=redirect.data();p.redirect_uri_bytes=redirect.size();observed(mb_nexus_auth_create_v1(&p,&ctx,&result)==MB_NEXUS_AUTH_BAD_ARGUMENT&&!ctx,"exact callback mismatch refused");redirect=origin+"/auth/callback";p.redirect_uri=redirect.data();p.redirect_uri_bytes=redirect.size();p.entropy_context=&result;result.status=MB_NEXUS_AUTH_BUSY;observed(mb_nexus_auth_create_v1(&p,&ctx,&result)==MB_NEXUS_AUTH_BAD_ARGUMENT&&result.status==MB_NEXUS_AUTH_BUSY&&!ctx,"entropy context alias untouched");
 }
 {
  Fixture f;f.login();std::string domain="skyrimspecialedition";std::array<uint8_t,4096> raw{};std::array<char,4096> report{},uri{};mb_nexus_download_params_v1 d{};d.struct_size=sizeof(d);d.abi_version=1;d.domain=domain.data();d.domain_bytes=domain.size();d.mod_id=1;d.file_id=2;d.timeout_ms=30000;d.connect_timeout_ms=1000;d.raw_body=raw.data();d.raw_capacity=raw.size();d.metadata_report=report.data();d.report_capacity=report.size();d.uri=uri.data();d.uri_capacity=uri.size();mb_nexus_auth_resolve_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.cookie=f.cookie.data();p.cookie_bytes=43;p.csrf=f.csrf.data();p.csrf_bytes=43;p.origin=f.origin.data();p.origin_bytes=f.origin.size();p.download=&d;mb_nexus_auth_resolve_result_v1 result{};std::memset(&result,0x51,sizeof(result));auto saved=result;raw.fill(0x44);report.fill('R');uri.fill('U');d.auth_mode=MB_NEXUS_DOWNLOAD_API_KEY;observed(mb_nexus_auth_resolve_download_v1(f.ctx,&p,&result)==MB_NEXUS_AUTH_BAD_ARGUMENT&&raw[0]==0x44&&report[0]=='R'&&uri[0]=='U'&&!std::memcmp(&saved,&result,sizeof(saved)),"caller auth override untouched");d.auth_mode=0;d.raw_body=reinterpret_cast<uint8_t*>(domain.data());d.raw_capacity=domain.size();auto text=domain;observed(mb_nexus_auth_resolve_download_v1(f.ctx,&p,&result)==MB_NEXUS_AUTH_BAD_ARGUMENT&&domain==text&&uri[0]=='U'&&!std::memcmp(&saved,&result,sizeof(saved)),"nested resolver alias untouched");d.raw_body=raw.data();d.raw_capacity=raw.size();d.cancel_context=uri.data();observed(mb_nexus_auth_resolve_download_v1(f.ctx,&p,&result)==MB_NEXUS_AUTH_BAD_ARGUMENT&&uri[0]=='U'&&!std::memcmp(&saved,&result,sizeof(saved)),"nested cancel URI alias untouched");
 }
 observed(last_work_zero&&work_deletes>0&&last_token_zero&&token_deletes>0,"owned scratch and token erasure observed");
 std::cout<<"{\"controls\":"<<controls<<",\"status\":\"pass\",\"metadata_and_resolver\":\"actual\",\"provider_exchange\":\"private_substitutes\",\"tls\":false,\"console\":false}\n";return 0;
 }catch(const std::exception& e){std::cerr<<"control failed: "<<e.what()<<"\n";return 1;}}
