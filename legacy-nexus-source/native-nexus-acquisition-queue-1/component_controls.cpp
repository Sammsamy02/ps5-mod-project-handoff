#include "jobs.h"
#include "modbridge/nexus.hpp"
extern "C" {
#include "download/curl_download.h"
}
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
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
bool free_user=false,expire_job_entropy=false;
mb_nexus_jobs_context_v1 **reentrant_jobs=nullptr;int reentrant_jobs_code=-1;
uint64_t mono=100*sec;
bool clock_failure=false,advance_work_delete=false,last_work_zero=true,last_token_zero=true;
unsigned work_deletes=0,token_deletes=0,exchanges=0,user_requests=0,mod_requests=0,link_requests=0,sequence=0,last_credential=0,controls=0;
bool fail_exchange=false,fail_user=false,expire_after_user=false,timeout_after_user=false,expire_after_link=false,timeout_after_link=false,rfc_vector=false,verifier_matches=false;
unsigned token_ttl=600;
unsigned file_requests=0,post_requests=0;
bool unknown_file_hints=false,changed_primary=false,post_failure=false;
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
struct Entropy {unsigned calls=0;bool fail=false,vector=false;unsigned fail_at=0,collision_at=0,advance_at=0;uint64_t advance_ns=0;};
int entropy(void *opaque,uint8_t output[32]){
 auto &e=*static_cast<Entropy*>(opaque);++e.calls;if(e.fail||(e.fail_at&&e.calls==e.fail_at)){std::memset(output,0x77,32);return 1;}
 if(e.vector&&e.calls==4){
  constexpr std::array<uint8_t,32> input{0x74,0x18,0xdf,0xb4,0x97,0x99,0xe0,0x25,0x4f,0xfa,0x60,0x7d,0xd8,0xad,0xbb,0xba,0x16,0xd4,0x25,0x4d,0x69,0xd6,0xbf,0xf0,0x5b,0x58,0x05,0x58,0x53,0x84,0x8d,0x79};std::copy(input.begin(),input.end(),output);
 }else for(unsigned i=0;i<32;++i)output[i]=static_cast<uint8_t>(e.calls+i*7);
 if(e.collision_at&&e.calls==e.collision_at)for(unsigned i=0;i<32;++i)output[i]=static_cast<uint8_t>(1+i*7);
 if(e.advance_at&&e.calls==e.advance_at)mono+=e.advance_ns;
 if(expire_job_entropy){mono+=1801*sec;expire_job_entropy=false;}
 if(reentrant_jobs){reentrant_jobs_code=mb_nexus_jobs_free_v1(reentrant_jobs);reentrant_jobs=nullptr;}
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
 if(user){++user_requests;if(fail_user){fail_user=false;std::memset(p->body,0,p->body_capacity);return out->status;}body=free_user?"{\"sub\":\"42\",\"name\":\"Fixture User\",\"membership_roles\":[],\"preferences\":{\"adult\":true}}":"{\"sub\":\"42\",\"name\":\"Fixture User\",\"membership_roles\":[\"premium\"],\"preferences\":{\"adult\":true}}";}
 else if(link){++link_requests;body="[{\"name\":\"Owned mirror\",\"short_name\":\"OWN\",\"URI\":\"https://owned-cdn.invalid/fixture.zip?owned_signature=fake\"}]";}
 else{
  const std::string url=p->url;const auto at=url.find("/mods/");if(at==url.npos)return out->status;
  const uint64_t mod=std::strtoull(p->url+at+6,nullptr,10);if(!mod)return out->status;
  static thread_local std::string owned;
  if(url.ends_with("/files")) {
   ++file_requests;const uint64_t id=mod*10+2+(changed_primary?100:0);
   nlohmann::json row={{"file_id",id},{"name","Owned file"},{"file_name","owned.zip"},{"version","owned label"},{"description",""},{"size_kb",1},{"is_primary",true},{"category_id",1},{"category_name","MAIN"}};
   if(unknown_file_hints)row["is_primary"]="unknown";
   owned=nlohmann::json{{"files",nlohmann::json::array({row})}}.dump();
  } else {
   ++mod_requests;owned=nlohmann::json{{"mod_id",mod},{"game_id",1704},{"domain_name","skyrimspecialedition"},{"name","Fixture Mod"},{"author","Fixture"},{"available",true},{"status","published"},{"contains_adult_content",false}}.dump();
  }
  body=owned.c_str();
 }
 size_t bytes=std::strlen(body);if(bytes>p->body_capacity)return out->status;std::memcpy(p->body,body,bytes);int consumer=p->consume_complete_body(p->consume_context,p->body,bytes);std::memset(p->body,0,p->body_capacity);out->http_status=200;out->received_bytes=bytes;
 if(user&&expire_after_user){mono+=token_ttl*sec+sec;expire_after_user=false;}if(user&&timeout_after_user){mono+=sec;timeout_after_user=false;}if(link&&expire_after_link){mono+=token_ttl*sec+sec;expire_after_link=false;}if(link&&timeout_after_link){mono+=sec;timeout_after_link=false;}
 out->status=consumer?MB_CURL_DOWNLOAD_CONSUMER_FAILED:MB_CURL_DOWNLOAD_OK;out->complete=!consumer;return out->status;
}

// Owned lower-provider substitute only. Actual auth planner, typed client,
// fixed query writer and strict owning decoder execute in these controls.
extern "C" int mb_curl_provider_post_transfer_v1(const mb_curl_provider_post_transfer_params_v1 *p,mb_curl_provider_transfer_result_v1 *out) {
 ++post_requests;*out={};out->struct_size=sizeof(*out);out->abi_version=1;out->status=MB_CURL_DOWNLOAD_TRANSPORT_FAILED;
 const auto& provider=p->provider;
 if(std::string_view(provider.url)!="https://api.nexusmods.com/v2/graphql"||provider.auth_mode!=2)return out->status;
 auto request=nlohmann::json::parse(p->request_body,p->request_body+p->request_body_bytes);
 const auto mod=request["variables"]["modId"].get<uint64_t>();const auto game=request["variables"]["gameDomainName"].get<std::string>();
 std::array<char,4096> canonical{};size_t written=0;
 if(!modbridge::nexus_requirements_body(game,mod,canonical,written)||std::string_view(canonical.data(),written)!=std::string_view(p->request_body,p->request_body_bytes))return out->status;
 nlohmann::json nodes=nlohmann::json::array();
 if(mod==2)nodes.push_back({{"id","owned-dependency"},{"externalRequirement",false},{"gameId",game},{"modId","1"},{"modName","Owned prerequisite"},{"notes",nullptr},{"url",""}});
 nlohmann::json data={{"data",{{"modRequirements",{{"nexusRequirements",{{"nodes",nodes},{"nodesCount",nodes.size()},{"totalCount",nodes.size()}}},{"dlcRequirements",nlohmann::json::array()}}}}}};
 if(post_failure)data["errors"]=nlohmann::json::array({{{"message","owned private failure"}}});
 auto body=data.dump();if(body.size()>provider.body_capacity)return out->status;
 std::memcpy(provider.body,body.data(),body.size());int consumed=provider.consume_complete_body(provider.consume_context,provider.body,body.size());std::memset(provider.body,0,provider.body_capacity);
 out->http_status=200;out->received_bytes=body.size();out->complete=!consumed;out->status=consumed?MB_CURL_DOWNLOAD_CONSUMER_FAILED:MB_CURL_DOWNLOAD_OK;return out->status;
}

namespace {
using Json=nlohmann::json;
enum class Producer { Success, Interrupted, Corrupt, OverCap };
Producer producer=Producer::Success;
std::vector<uint8_t> archive_bytes;
bool sink_source_valid=true,secret_uri_seen=false;
std::string input(std::string_view key="owned_key_1",uint64_t file=2) {
 return "{\"kind\":\"download_analyze\",\"game\":\"skyrimspecialedition\",\"mod_id\":1,\"file_id\":"+std::to_string(file)+",\"idempotency_key\":\""+std::string(key)+"\"}";
}
struct JobFixture {
 Fixture& auth;Entropy ent;int directory=-1;std::string path;
 mb_nexus_jobs_context_v1 *ctx=nullptr;mb_nexus_jobs_result_v1 result{};
 std::vector<char> output=std::vector<char>(MB_NEXUS_JOBS_OUTPUT_LIMIT);
 JobFixture(Fixture& f,uint64_t cap=MB_NEXUS_JOBS_ARCHIVE_LIMIT):auth(f) {
  char directory_name[]="/tmp/mb-job-controls-XXXXXX";require(mkdtemp(directory_name),"private temp directory");path=directory_name;
  directory=open(path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);require(directory>=0,"directory descriptor");
  mb_nexus_jobs_create_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.auth=f.ctx;p.scratch_directory_fd=directory;
  p.ca_pem=f.ca.data();p.ca_bytes=f.ca.size();p.entropy_context=&ent;p.entropy_read32=entropy;
  p.resolver_timeout_ms=30000;p.download_timeout_ms=30000;p.analysis_timeout_ms=30000;p.connect_timeout_ms=1000;p.archive_cap_bytes=cap;
  require(mb_nexus_jobs_create_v1(&p,&ctx,&result)==0,"jobs create");
 }
 ~JobFixture(){if(ctx){int code=mb_nexus_jobs_free_v1(&ctx);if(code)std::cerr<<"cleanup refusal "<<code<<"\n";}if(directory>=0)close(directory);rmdir(path.c_str());}
 int submit(const std::string& body,const char *csrf=nullptr,const char *origin=nullptr,size_t capacity=MB_NEXUS_JOBS_OUTPUT_LIMIT) {
  std::fill(output.begin(),output.end(),'X');
  mb_nexus_jobs_submit_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;
  p.cookie=auth.cookie.data();p.cookie_bytes=43;p.csrf=csrf?csrf:auth.csrf.data();p.csrf_bytes=43;p.origin=origin?origin:auth.origin.data();p.origin_bytes=origin?std::strlen(origin):auth.origin.size();
  p.intent=reinterpret_cast<const uint8_t*>(body.data());p.intent_bytes=body.size();p.output=output.data();p.output_capacity=capacity;
  return mb_nexus_jobs_submit_v1(ctx,&p,&result);
 }
 int list(const char *cookie=nullptr,size_t capacity=MB_NEXUS_JOBS_OUTPUT_LIMIT) {
  std::fill(output.begin(),output.end(),'X');mb_nexus_jobs_list_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.cookie=cookie?cookie:auth.cookie.data();p.cookie_bytes=43;p.output=output.data();p.output_capacity=capacity;
  return mb_nexus_jobs_list_v1(ctx,&p,&result);
 }
 int get(const std::string& id,const char *cookie=nullptr) {
  std::fill(output.begin(),output.end(),'X');mb_nexus_jobs_get_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.cookie=cookie?cookie:auth.cookie.data();p.cookie_bytes=43;p.id=id.data();p.id_bytes=id.size();p.output=output.data();p.output_capacity=output.size();
  return mb_nexus_jobs_get_v1(ctx,&p,&result);
 }
 int run(){return mb_nexus_jobs_run_one_v1(ctx,&result);}
 Json value() const {require(result.complete&&result.output_bytes&&output[result.output_bytes]==0,"whole owning result");return Json::parse(output.data(),output.data()+result.output_bytes);}
 size_t files() const {size_t n=0;for(const auto& entry:std::filesystem::directory_iterator(path)){(void)entry;++n;}return n;}
 bool queued_secrets_erased() const {
  for(const auto& allocation_entry:allocation)if(allocation_entry.pointer==ctx)
   return !contains(ctx,allocation_entry.size,auth.cookie.data(),43)&&!contains(ctx,allocation_entry.size,auth.csrf.data(),43);
  return false;
 }
};
}
// The external producing operation is substituted only in these ignored
// component controls. Its actual file sink, positional adapter, readback SHA
// and archive analyzer are production modules. This is not an HTTPS receipt.
extern "C" int mb_curl_download_v1(const mb_curl_download_params_v1 *p,mb_curl_download_result_v1 *out) {
 *out={};out->struct_size=sizeof(*out);out->abi_version=1;out->status=MB_CURL_DOWNLOAD_TRANSPORT_FAILED;
 secret_uri_seen=std::string_view(p->url).find("owned_signature=fake")!=std::string_view::npos;
 struct stat s{};sink_source_valid=sink_source_valid&&fstat(p->sink.fd,&s)==0&&S_ISREG(s.st_mode)&&s.st_size==0&&(s.st_mode&07777)==0600&&s.st_nlink==1;
 mb_download_file_sink_v1 sink{};
 if(mb_download_file_sink_start_v1(&sink,&p->sink)){mb_download_file_sink_dispose_v1(&sink);return out->status;}
 std::vector<uint8_t> body=producer==Producer::Corrupt?std::vector<uint8_t>{'n','o','t',' ','Z','I','P'}:archive_bytes;
 if(producer==Producer::OverCap)body.resize(static_cast<size_t>(p->sink.cap_bytes)+1,0x44);
 if(producer==Producer::Interrupted)body.resize(17);
 auto feed=mb_download_file_sink_feed_v1(&sink,body.data(),body.size());
 if(feed||producer==Producer::Interrupted){mb_download_file_sink_dispose_v1(&sink);out->status=feed?MB_CURL_DOWNLOAD_LIMIT:MB_CURL_DOWNLOAD_TRANSPORT_FAILED;return out->status;}
 if(fsync(p->sink.fd)||mb_download_file_sink_finish_v1(&sink,1,1,body.size(),&out->file)){mb_download_file_sink_dispose_v1(&sink);return out->status;}
 mb_download_file_sink_dispose_v1(&sink);out->received_bytes=body.size();out->status=MB_CURL_DOWNLOAD_OK;out->complete=1;return 0;
}

int legacy_job_main(int argc,char **argv){try{
 require(argc==2,"owned archive fixture argument");std::ifstream archive(argv[1],std::ios::binary);archive_bytes.assign(std::istreambuf_iterator<char>(archive),{});require(archive_bytes.size()>100,"saved archive bytes");
 {
  Fixture f;f.login();JobFixture j(f);
  observed(j.list()==0&&j.value()["items"].empty(),"empty cookie-scoped list");
  observed(j.run()==0&&!j.result.processed,"empty queue no job");
  std::string unknown(43,'z');unsigned before=f.ent.calls;
  observed(j.list(unknown.data())==MB_NEXUS_JOBS_SESSION_REQUIRED&&zero(j.output.data(),j.output.size())&&f.ent.calls==before,"unknown cookie refuses without session creation");
  std::string bad_csrf(43,'z');
  observed(j.submit(input(),bad_csrf.data())==MB_NEXUS_JOBS_REFUSED&&zero(j.output.data(),j.output.size())&&j.ent.calls==0,"CSRF refuses admission");
  observed(j.submit(input(),nullptr,"http://127.0.0.1:1")==MB_NEXUS_JOBS_REFUSED&&j.ent.calls==0,"exact Origin refuses admission");
  const std::array<std::string,15> malformed={
   "[]","{}",input().substr(0,input().size()-1)+",",input()+"x",
   "{\"kind\":\"download_analyze\",\"game\":\"skyrimspecialedition\",\"mod_id\":1,\"mod_id\":1,\"file_id\":2,\"idempotency_key\":\"owned_key_1\"}",
   "{\"kind\":\"download_analyze\",\"game\":\"skyrimspecialedition\",\"mod_id\":01,\"file_id\":2,\"idempotency_key\":\"owned_key_1\"}",
   "{\"kind\":\"download_analyze\",\"game\":\"skyrimspecialedition\",\"mod_id\":1.0,\"file_id\":2,\"idempotency_key\":\"owned_key_1\"}",
   "{\"kind\":\"download_analyze\",\"game\":\"skyrimspecialedition\",\"mod_id\":1e0,\"file_id\":2,\"idempotency_key\":\"owned_key_1\"}",
   "{\"kind\":\"download_analyze\",\"game\":\"skyrimspecialedition\",\"mod_id\":true,\"file_id\":2,\"idempotency_key\":\"owned_key_1\"}",
   "{\"kind\":\"download_analyze\",\"game\":\"skyrimspecialedition\",\"mod_id\":9223372036854775808,\"file_id\":2,\"idempotency_key\":\"owned_key_1\"}",
   "{\"kind\":\"download_analyze\",\"game\":\"Skyrim\",\"mod_id\":1,\"file_id\":2,\"idempotency_key\":\"owned_key_1\"}",
   "{\"kind\":\"download_analyze\",\"game\":\"skyrimspecialedition\",\"mod_id\":-1,\"file_id\":2,\"idempotency_key\":\"owned_key_1\"}",
   "{\"kind\":\"download_analyze\",\"game\":\"skyrimspecialedition\",\"mod_id\":0,\"file_id\":2,\"idempotency_key\":\"owned_key_1\"}",
   "{\"kind\":\"download_analyze\",\"game\":\"skyrimspecialedition\",\"mod_id\":\"1\",\"file_id\":2,\"idempotency_key\":\"owned_key_1\"}",
   "{\"k\\u0069nd\":\"download_analyze\",\"game\":\"skyrimspecialedition\",\"mod_id\":1,\"file_id\":2,\"idempotency_key\":\"owned_key_1\"}"
  };
  for(const auto& body:malformed)observed(j.submit(body)!=0&&zero(j.output.data(),j.output.size())&&j.ent.calls==0,"strict malformed intent refuses before entropy");
  for(const char *extra:{"\"nxm\":\"nxm://owned\"","\"target_ref\":{}","\"other\":1"}) {
   auto body=input();body.pop_back();body+=",";body+=extra;body+="}";
   observed(j.submit(body)!=0&&zero(j.output.data(),j.output.size())&&j.ent.calls==0,"unsupported intent field refused");
  }
  observed(j.submit(input(),nullptr,nullptr,1)==MB_NEXUS_JOBS_OUTPUT_TOO_SMALL&&j.output[0]==0,"short output admits no prefix or record");
  observed(j.list()==0&&j.value()["items"].empty(),"short output has no queued record");
  j.ent.fail=true;observed(j.submit(input())==MB_NEXUS_JOBS_SOURCE_FAILED&&zero(j.output.data(),j.output.size()),"checked entropy refuses queue mutation");j.ent.fail=false;
  observed(j.submit(input())==0&&j.value()["status"]=="queued","valid selection queues");auto first=j.value();auto id=first["id"].get<std::string>();before=j.ent.calls;
  observed(j.submit(input())==0&&j.value()["id"]==id&&j.ent.calls==before&&j.result.job_count==1,"idempotent repeat has same ID and no entropy");
  observed(j.submit(input("owned_key_1",3))==MB_NEXUS_JOBS_INTENT_CONFLICT&&zero(j.output.data(),j.output.size()),"idempotency rejects source conflict");
  observed(j.get(id)==0&&j.value()["id"]==id,"scoped ID retrieval");
  auto old_cookie=f.cookie;require(f.bootstrap()==0,"second browser bootstrap");
  observed(j.get(id)==MB_NEXUS_JOBS_NOT_FOUND&&zero(j.output.data(),j.output.size()),"other browser cannot retrieve first job");
  observed(j.list()==0&&j.value()["items"].empty(),"other browser list excludes first job");
  observed(j.submit(input())==0&&j.value()["id"]!=id&&j.result.job_count==1,"same key is scoped to each browser");
  observed(j.get(id,old_cookie.data())==0&&j.value()["id"]==id,"original browser retains scope");
  auto text=j.value().dump();observed(text.find("idempotency_key")==std::string::npos&&text.find("owned_signature")==std::string::npos&&text.find("cookie")==std::string::npos&&text.find("csrf")==std::string::npos,"public queue omits private bindings and URI");
 }
 {
  Fixture f;f.login();JobFixture j(f);for(unsigned n=0;n<8;++n)require(j.submit(input("owned_key_"+std::to_string(n)))==0,"eight bounded records");
  observed(j.submit(input("owned_key_ninth"))==MB_NEXUS_JOBS_LIMIT_REACHED&&j.list()==0&&j.value()["items"].size()==8,"ninth record refused without eviction");
  unsigned calls=j.ent.calls;observed(j.submit(input("owned_key_0"))==0&&j.ent.calls==calls&&j.result.job_count==8,"idempotency retained at queue cap");
 }
 {
  Fixture f;f.login();JobFixture j(f);expire_job_entropy=true;
  observed(j.submit(input())==MB_NEXUS_JOBS_SESSION_REQUIRED&&zero(j.output.data(),j.output.size()),"session expiry during entropy revokes queue admission");
  require(f.bootstrap()==0,"fresh session after private expiry");
  observed(j.list()==0&&j.value()["items"].empty(),"expired commit gate created no record");
 }
 {
  Fixture f;f.login();JobFixture j(f);reentrant_jobs=&j.ctx;
  observed(j.submit(input())==0&&j.ctx&&reentrant_jobs_code==MB_NEXUS_JOBS_BUSY,"entropy reentry cannot free active queue context");
 }
 {
  Fixture f;f.login();JobFixture j(f);require(j.submit(input())==0,"free job queued");auto id=j.value()["id"].get<std::string>();free_user=true;
  observed(j.run()==0&&j.result.processed==1&&j.get(id)==0&&j.value()["status"]=="needs_nexus_authorization"&&j.files()==0,"live free user requires website authorization without archive");free_user=false;
  observed(j.queued_secrets_erased(),"free refusal erases queued bindings");
  unsigned requests=user_requests;observed(j.submit(input())==0&&j.value()["id"]==id&&j.run()==0&&!j.result.processed&&user_requests==requests,"authorization repeat is idempotent and does not rerun");
 }
 {
  Fixture f;f.login();JobFixture j(f);require(j.submit(input())==0,"disconnect job queued");auto id=j.value()["id"].get<std::string>();require(f.disconnect()==0,"owned disconnect");
  observed(j.run()==0&&j.get(id)==0&&j.value()["status"]=="failed"&&j.value()["error"]["code"]=="reconnect_required"&&j.files()==0&&j.queued_secrets_erased(),"queued disconnect fails privately without archive");
 }
 if(std::getenv("MB_JOBS_QUEUE_ONLY"))goto completed;
 {
  Fixture f;f.login();JobFixture j(f);producer=Producer::Success;require(j.submit(input())==0,"successful archive queue");auto id=j.value()["id"].get<std::string>();
  observed(j.run()==0&&j.result.processed==1&&j.get(id)==0&&j.value()["status"]=="analyzed","real sink receipt and actual archive scan reach analyzed");auto job=j.value();
  observed(job["bytes_downloaded"]==archive_bytes.size()&&job["analysis_report"]["source_bytes"]==archive_bytes.size()&&job["sha256"]==job["analysis_report"]["source_sha256"],"complete byte count and SHA bound into analyzer");
  observed(job["analysis_report"]["member_count"]==6&&job["analysis_report"]["candidate_types"]["dll"]==1&&!job["runtime_supported"].get<bool>()&&!job["installation_supported"].get<bool>(),"filename candidates do not grant compatibility");
  observed(j.files()==1&&j.queued_secrets_erased()&&sink_source_valid&&secret_uri_seen,"unchanged successful source stays private and bindings erase");
  auto name=j.path+"/mb-job-"+id+".archive";std::ifstream source(name,std::ios::binary);std::vector<uint8_t> retained(std::istreambuf_iterator<char>(source),{});
  observed(retained==archive_bytes,"retained bytes equal original archive");source.close();
  unsigned requests=user_requests;observed(j.submit(input())==0&&j.value()["id"]==id&&j.run()==0&&!j.result.processed&&user_requests==requests,"analyzed idempotency does not duplicate transfer");
  observed(mb_nexus_jobs_free_v1(&j.ctx)==0&&!j.ctx&&j.files()==0,"shutdown removes unchanged owned archive");
 }
 for(auto selected:{Producer::Interrupted,Producer::Corrupt,Producer::OverCap}) {
  Fixture f;f.login();JobFixture j(f,selected==Producer::OverCap?10:MB_NEXUS_JOBS_ARCHIVE_LIMIT);producer=selected;
  require(j.submit(input())==0,"failure job queued");auto id=j.value()["id"].get<std::string>();
  observed(j.run()==0&&j.get(id)==0&&j.value()["status"]=="failed"&&j.files()==0&&j.queued_secrets_erased(),"failed transfer/archive removes private scratch and bindings");
  auto job=j.value();observed(!job.contains("analysis_report")&&((selected==Producer::Corrupt&&job.contains("sha256")&&job["stage"]=="downloaded")||(selected!=Producer::Corrupt&&!job.contains("sha256")&&job["stage"]=="requested")),"no failed prefix or analysis publication");
 }
 producer=Producer::Success;
 {
  Fixture f;f.login();JobFixture j(f);require(j.submit(input())==0,"collision queued");auto id=j.value()["id"].get<std::string>();auto name=j.path+"/mb-job-"+id+".archive";
  int existing=open(name.c_str(),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW,0600);require(existing>=0&&write(existing,"unowned",7)==7,"existing file fixture");close(existing);
  observed(j.run()==MB_NEXUS_JOBS_STORAGE_FAILED&&j.get(id)==0&&j.value()["status"]=="failed","exclusive name collision fails closed");
  std::ifstream retained(name);std::string bytes((std::istreambuf_iterator<char>(retained)),{});observed(bytes=="unowned","collision never truncates unrelated leaf");retained.close();
  observed(j.submit(input("owned_new_key"))==MB_NEXUS_JOBS_STORAGE_FAILED,"storage latch refuses new work");
  observed(mb_nexus_jobs_free_v1(&j.ctx)==0&&std::filesystem::exists(name),"free never removes unrelated collision");unlink(name.c_str());
 }
 {
  Fixture f;f.login();JobFixture j(f);require(j.submit(input())==0,"replacement queued");auto id=j.value()["id"].get<std::string>();require(j.run()==0,"replacement source analyzed");
  auto name=j.path+"/mb-job-"+id+".archive",saved=j.path+"/owned-original";require(rename(name.c_str(),saved.c_str())==0,"owned replacement simulation");int replacement=open(name.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);require(replacement>=0,"substituted file");close(replacement);
  observed(mb_nexus_jobs_free_v1(&j.ctx)==MB_NEXUS_JOBS_STORAGE_FAILED&&j.ctx&&std::filesystem::exists(name)&&std::filesystem::exists(saved),"cleanup retains handle and never unlinks substituted inode");
  require(unlink(name.c_str())==0&&rename(saved.c_str(),name.c_str())==0,"restore original for explicit cleanup retry");
  observed(mb_nexus_jobs_free_v1(&j.ctx)==0&&!j.ctx&&j.files()==0,"identity-safe cleanup retry removes original");
 }
 {
  Fixture f;f.login();JobFixture j(f);auto body=input();auto saved=f.cookie;mb_nexus_jobs_submit_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;p.cookie=f.cookie.data();p.cookie_bytes=43;p.csrf=f.csrf.data();p.csrf_bytes=43;p.origin=f.origin.data();p.origin_bytes=f.origin.size();p.intent=reinterpret_cast<const uint8_t*>(body.data());p.intent_bytes=body.size();p.output=f.cookie.data();p.output_capacity=44;mb_nexus_jobs_result_v1 result{};std::memset(&result,0x55,sizeof(result));auto before=result;
  observed(mb_nexus_jobs_submit_v1(j.ctx,&p,&result)==MB_NEXUS_JOBS_BAD_ARGUMENT&&f.cookie==saved&&!std::memcmp(&before,&result,sizeof(result)),"known input-output alias remains untouched");
 }
completed:
 observed(last_work_zero&&last_token_zero,"actual auth scratch and private token cleanup retained");
 std::cout<<"{\"controls\":"<<controls<<",\"status\":\"pass\",\"auth_admission_resolver_decoder_sink_positional_io_archive\":\"actual\",\"external_token_provider_producer\":\"private_substitutes\",\"tls\":false,\"console\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<"control failed: "<<e.what()<<"\n";return 1;}}

namespace {
unsigned discovery(){return mod_requests+file_requests+post_requests;}
std::string acquisition_input(std::string_view group_key="owned_group_key",bool reordered=false) {
 Json value={{"game","skyrimspecialedition"},{"mod_ids",Json::array({2,1})},
  {"file_choices",Json::array({{{"mod_id",2},{"file_id",22}},{{"mod_id",1},{"file_id",12}}})},
  {"file_policy","unique_current_primary"},{"idempotency_key",group_key}};
 if(reordered){value["mod_ids"]=Json::array({1,2});value["file_choices"]=Json::array({{{"file_id",12},{"mod_id",1}},{{"file_id",22},{"mod_id",2}}});}
 return value.dump();
}
int acquisition_submit(JobFixture& j,const std::string& body,size_t capacity=MB_NEXUS_JOBS_OUTPUT_LIMIT,const char *csrf=nullptr,const char *origin=nullptr) {
 std::fill(j.output.begin(),j.output.end(),'X');mb_nexus_jobs_submit_params_v1 p{};p.struct_size=sizeof(p);p.abi_version=1;
 p.cookie=j.auth.cookie.data();p.cookie_bytes=43;p.csrf=csrf?csrf:j.auth.csrf.data();p.csrf_bytes=43;p.origin=origin?origin:j.auth.origin.data();p.origin_bytes=origin?std::strlen(origin):j.auth.origin.size();
 p.intent=reinterpret_cast<const uint8_t*>(body.data());p.intent_bytes=body.size();p.output=j.output.data();p.output_capacity=capacity;
 return mb_nexus_jobs_submit_acquisition_v1(j.ctx,&p,&j.result);
}
void reset_acquisition(){unknown_file_hints=changed_primary=post_failure=false;}
void no_records(JobFixture& j){observed(j.list()==0&&j.value()["items"].empty()&&j.files()==0,"atomic rollback leaves no record or file");}
int acquisition_main(){try{
 {
  Fixture f;f.login();JobFixture j(f);auto cookie=f.cookie;
  for(unsigned i=0;i<6;++i)require(j.submit(input("prefill_key_"+std::to_string(i)))==0,"six prefill");
  require(f.bootstrap()==0,"other cookie");unsigned before=discovery();
  observed(acquisition_submit(j,acquisition_input())==0&&j.result.job_count==2&&j.result.queued_count==2,"global six plus two complete group");
  auto receipt=j.value()["acquisition"];observed(receipt["source_job_ids"].size()==2&&receipt["selected_sources"][0]["mod_id"]==1&&receipt["selected_sources"][1]["mod_id"]==2&&discovery()>before,"actual planner preserves prerequisite-first identities");
  observed(!receipt["download_authorized"].get<bool>()&&!receipt["runtime_supported"].get<bool>()&&!receipt["installation_supported"].get<bool>()&&j.files()==0,"accepted carries no authority or source file");
  auto ids=receipt["source_job_ids"];unsigned entropy_before=j.ent.calls;before=discovery();changed_primary=true;
  observed(acquisition_submit(j,acquisition_input("owned_group_key",true))==0&&j.value()["acquisition"]["source_job_ids"]==ids&&discovery()==before&&j.ent.calls==entropy_before,"canonical replay at full global capacity uses original identities");
  auto conflict=Json::parse(acquisition_input());conflict["file_choices"][0]["file_id"]=23;
  observed(acquisition_submit(j,conflict.dump())==MB_NEXUS_JOBS_INTENT_CONFLICT&&zero(j.output.data(),j.output.size())&&discovery()==before&&j.ent.calls==entropy_before,"changed canonical choice conflicts without discovery");
  const auto first=ids[0].get<std::string>();observed(j.get(first,cookie.data())==MB_NEXUS_JOBS_NOT_FOUND,"other cookie cannot borrow group children");reset_acquisition();
 }
 {
  Fixture f;f.login();JobFixture j(f);for(unsigned i=0;i<7;++i)require(j.submit(input("prefill_key_"+std::to_string(i)))==0,"seven prefill");
  const unsigned before=j.ent.calls;
  observed(acquisition_submit(j,acquisition_input())==MB_NEXUS_JOBS_LIMIT_REACHED&&zero(j.output.data(),j.output.size())&&j.ent.calls==before,"global seven plus two refuses before staged entropy");
  observed(j.list()==0&&j.value()["items"].size()==7&&j.files()==0,"capacity refusal preserves whole existing queue");
 }
 for(bool collision:{false,true}) {
  Fixture f;f.login();JobFixture j(f);if(collision)j.ent.collision_at=2;else j.ent.fail_at=2;
  observed(acquisition_submit(j,acquisition_input())==MB_NEXUS_JOBS_SOURCE_FAILED&&j.ent.calls==2&&zero(j.output.data(),j.output.size()),"second staged entropy failure or ID collision rolls back entire group");no_records(j);
  j.ent.fail_at=j.ent.collision_at=0;
  observed(acquisition_submit(j,acquisition_input())==0&&j.result.job_count==2,"failed staging leaves no replay reservation");
 }
 {
  Fixture f;f.login();JobFixture j(f);
  observed(acquisition_submit(j,acquisition_input(),1)==MB_NEXUS_JOBS_OUTPUT_TOO_SMALL&&j.output[0]==0,"short acquisition output publishes no prefix");no_records(j);
  observed(acquisition_submit(j,acquisition_input())==0&&j.result.job_count==2,"short output leaves no group reservation");
 }
 {
  Fixture f;token_ttl=3600;f.login();JobFixture j(f);mono+=1799*sec;j.ent.advance_at=2;j.ent.advance_ns=2*sec;
  observed(acquisition_submit(j,acquisition_input())==MB_NEXUS_JOBS_SESSION_REQUIRED&&zero(j.output.data(),j.output.size()),"final staged session expiry revokes whole group");
  require(f.bootstrap()==0,"new session after expired boundary");no_records(j);
 }
 {
  Fixture f;token_ttl=3600;f.login();JobFixture j(f);j.ent.advance_at=2;j.ent.advance_ns=31*sec;
  observed(acquisition_submit(j,acquisition_input())==MB_NEXUS_JOBS_SOURCE_FAILED&&zero(j.output.data(),j.output.size()),"final whole configured budget revokes staged group");no_records(j);
 }
 {
  Fixture f;f.login();JobFixture j(f);unsigned before=discovery();
  auto forged=Json::parse(acquisition_input());forged["plan"]={{"acquisition_ready",true}};
  const int forged_status=acquisition_submit(j,forged.dump());observed((forged_status==MB_NEXUS_JOBS_REFUSED||forged_status==MB_NEXUS_JOBS_BAD_ARGUMENT)&&discovery()==before&&j.ent.calls==0,"client plan refused before discovery");
  const std::array<std::string,9> malformed={"{}",acquisition_input()+"x",
   R"({"game":"skyrimspecialedition","mod_ids":[1,1],"file_choices":[],"file_policy":"unique_current_primary","idempotency_key":"owned_key_1"})",
   R"({"game":"skyrimspecialedition","mod_ids":[1.0],"file_choices":[],"file_policy":"unique_current_primary","idempotency_key":"owned_key_1"})",
   R"({"game":"skyrimspecialedition","mod_ids":[2147483648],"file_choices":[],"file_policy":"unique_current_primary","idempotency_key":"owned_key_1"})",
   R"({"game":"skyrimspecialedition","mod_ids":[1],"file_choices":[{"mod_id":1,"mod_id":1,"file_id":12}],"file_policy":"unique_current_primary","idempotency_key":"owned_key_1"})",
   R"({"game":"skyrimspecialedition","mod_ids":[1],"file_choices":[{"mod_id":1,"file_id":12},{"mod_id":1,"file_id":13}],"file_policy":"unique_current_primary","idempotency_key":"owned_key_1"})",
   R"({"game":"skyrimspecialedition","game":"skyrimspecialedition","mod_ids":[1],"file_choices":[],"file_policy":"unique_current_primary","idempotency_key":"owned_key_1"})",
   R"({"game":"skyrimspecialedition","mod_ids":[1],"file_choices":[{"mod_id":1,"file_id":9223372036854775808}],"file_policy":"unique_current_primary","idempotency_key":"owned_key_1"})"};
  for(const auto& body:malformed)observed(acquisition_submit(j,body)!=0&&zero(j.output.data(),j.output.size())&&discovery()==before&&j.ent.calls==0,"malformed acquisition is pre-admission refusal");
  std::string bad(43,'z');observed(acquisition_submit(j,acquisition_input(),j.output.size(),bad.data())==MB_NEXUS_JOBS_REFUSED,"wrong CSRF refuses group");
  observed(acquisition_submit(j,acquisition_input(),j.output.size(),nullptr,"http://127.0.0.1:1")==MB_NEXUS_JOBS_REFUSED,"wrong Origin refuses group");no_records(j);
 }
 {
  Fixture f;f.login();JobFixture j(f);auto body=Json::parse(acquisition_input());body["file_choices"]=Json::array();unknown_file_hints=true;
  observed(acquisition_submit(j,body.dump())==MB_NEXUS_JOBS_ACQUISITION_UNRESOLVED&&j.ent.calls==0&&zero(j.output.data(),j.output.size()),"actual non-ready planner cannot queue selected subset");no_records(j);reset_acquisition();
  post_failure=true;observed(acquisition_submit(j,body.dump())==MB_NEXUS_JOBS_SOURCE_FAILED&&j.ent.calls==0,"real strict decoder provider failure publishes no group");no_records(j);reset_acquisition();
 }
 {
  Fixture f;f.login();JobFixture j(f);reentrant_jobs=&j.ctx;
  observed(acquisition_submit(j,acquisition_input())==0&&j.ctx&&reentrant_jobs_code==MB_NEXUS_JOBS_BUSY,"actual auth under job Busy works while entropy re-entry cannot free it");
  auto ids=j.value()["acquisition"]["source_job_ids"];auto old=f.cookie;require(f.bootstrap()==0,"independent second group cookie");
  observed(acquisition_submit(j,acquisition_input())==0&&j.value()["acquisition"]["source_job_ids"]!=ids&&j.result.job_count==2,"same group key independently cookie scoped");
  observed(j.get(ids[0].get<std::string>())==MB_NEXUS_JOBS_NOT_FOUND&&j.get(ids[0].get<std::string>(),old.data())==0,"source retrieval retains group browser ownership");
  observed(j.submit(input("owned_group_key"))==0&&j.value()["kind"]=="download_analyze","group and single-job keys have separate namespaces");
 }
 std::cout<<"acquisition_controls="<<controls<<" actual_auth_planner_decoder_jobs=1 lower_provider_substituted=1 tls=0 console=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"acquisition control failed: "<<e.what()<<"\n";return 1;}}
}
int main(int argc,char **argv){if(acquisition_main())return 1;if(std::getenv("MB_SKIP_LEGACY"))return 0;unsigned before=controls;int rc=legacy_job_main(argc,argv);std::cout<<"legacy_controls="<<controls-before<<"\n";return rc;}
