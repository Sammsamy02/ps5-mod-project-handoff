#include "oauth_code.h"
#include "download/oauth_code_transfer.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <time.h>
namespace {
unsigned checks=0,failed=0,post_calls=0,owner_faults=0;
std::uint64_t now=100000000000ull;
int mode=0;
std::string reply="{\"access_token\":\"fake-access\",\"refresh_token\":\"fake-refresh\",\"token_type\":\"Bearer\",\"expires_in\":3600}";
void expect(bool ok,const char *name){++checks;if(!ok){++failed;std::cerr<<name<<" failed\n";}}
bool zero(const void *p,std::size_t n){const auto*b=static_cast<const unsigned char*>(p);return std::all_of(b,b+n,[](auto c){return !c;});}
bool throws=false,cancelled=false;
bool post_completed=false;
const void *watched_form=nullptr,*watched_raw=nullptr;
std::size_t watched_form_bytes=0,watched_raw_bytes=0;
int cancel(void*,std::uint64_t){if(throws)throw std::runtime_error("private control");return cancelled;}
struct Fixture {
 std::array<char,65536> form;
 std::array<std::uint8_t,131072> raw;
 mb_nexus_oauth_tokens_v1 tokens;
 mb_nexus_oauth_exchange_params_v1 p{};
 mb_nexus_oauth_exchange_result_v1 out{};
 Fixture(){
  const char *v="dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
  p.struct_size=sizeof(p);p.abi_version=1;
  p.fields={"fake_app",8,"http://127.0.0.1:8085/auth/callback",35,"fake-code",9,v,43};
  p.fields.redirect_uri_bytes=std::strlen(p.fields.redirect_uri);
  static const std::uint8_t ca[]="fake-test-ca";
  p.ca_pem=ca;p.ca_bytes=sizeof(ca)-1;p.users_resolve="users.nexusmods.com:443:127.0.0.1";
  p.timeout_ms=1000;p.connect_timeout_ms=500;p.cancelled=cancel;
  p.form=form.data();p.form_capacity=form.size();p.raw_body=raw.data();p.raw_capacity=raw.size();p.tokens=&tokens;
  dirty();
 }
 void dirty(){form.fill('x');raw.fill('x');std::memset(&tokens,'x',sizeof(tokens));}
 bool scratch_zero(){return zero(form.data(),form.size())&&zero(raw.data(),raw.size());}
 bool tokens_zero(){return zero(&tokens,sizeof(tokens));}
};
}
extern "C" int controls_clock_gettime(clockid_t, timespec *value){
 if(mode==8){errno=EIO;return -1;}
 if(mode==10&&post_completed&&zero(watched_form,watched_form_bytes)&&zero(watched_raw,watched_raw_bytes))now=101000000000ull;
 value->tv_sec=static_cast<time_t>(now/1000000000ull);value->tv_nsec=static_cast<long>(now%1000000000ull);return 0;
}
extern "C" void mb_curl_owner_fail_v1(void){++owner_faults;}
/* Explicit fake network boundary only. Real form writer, strict parser,
 * wrapper cleanup and private clock boundary are tested; no TLS evidence. */
extern "C" int mb_curl_oauth_code_transfer_v1(const mb_curl_oauth_code_transfer_params_v1 *p,mb_curl_oauth_code_transfer_result_v1 *out){
 ++post_calls;std::memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);out->abi_version=1;out->http_status=200;
 const std::string want="grant_type=authorization_code&redirect_uri=http%3A%2F%2F127.0.0.1%3A8085%2Fauth%2Fcallback&client_id=fake_app&code=fake-code&code_verifier=dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
 if(std::string_view(p->form,p->form_bytes)!=want){out->status=MB_CURL_DOWNLOAD_BAD_ARGUMENT;return out->status;}
 if(mode==1){out->status=MB_CURL_DOWNLOAD_BAD_STATE;return out->status;}
 if(mode==9){throws=true;if(p->cancelled(p->cancel_context,0)){out->status=MB_CURL_DOWNLOAD_CANCELLED;return out->status;}}
 if(mode==7&&p->cancelled(p->cancel_context,0)){out->status=MB_CURL_DOWNLOAD_CANCELLED;return out->status;}
 std::memcpy(p->body,reply.data(),reply.size());out->received_bytes=reply.size();
 const int consumed=p->consume_complete_body(p->consume_context,p->body,reply.size());
 out->status=consumed?MB_CURL_DOWNLOAD_CONSUMER_FAILED:MB_CURL_DOWNLOAD_OK;out->complete=consumed?0:1;
 if(mode==2){out->status=MB_CURL_DOWNLOAD_OWNER_FAILED;out->complete=0;}
 if(mode==3){out->status=MB_CURL_DOWNLOAD_TIMEOUT;out->complete=0;}
 if(mode==4)now+=1000000000ull;
 if(mode==5)now-=1;
 if(mode==6)cancelled=true;
 post_completed=true;
 return out->status; // Intentionally leaves raw dirty: wrapper fallback must wipe all.
}
int main(){
 Fixture f;mode=0;now=100000000000ull;
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==0&&f.out.status==0&&f.out.complete==1&&f.out.transport_status==0&&f.scratch_zero()&&f.tokens.access_token_bytes==11&&f.tokens.refresh_token_bytes==12&&f.tokens.expires_in_seconds==3600&&f.tokens.expires_at_monotonic_ns==3700000000000ull&&std::string_view(f.tokens.access_token,11)=="fake-access", "whole_private_pair");
 for(const auto item:std::array<std::pair<int,int>,5>{{{1,2},{2,3},{3,6},{4,6},{5,7}}}){
  mode=item.first;now=100000000000ull;f.dirty();cancelled=false;
  const int rc=mb_nexus_oauth_exchange_code_v1(&f.p,&f.out);
  expect(rc==item.second&&f.out.status==item.second&&!f.out.complete&&f.scratch_zero()&&f.tokens_zero(),"late_transport_clock_deadline_revoke");
 }
 mode=6;now=100000000000ull;f.dirty();cancelled=false;
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==5&&!f.out.complete&&f.tokens_zero()&&f.scratch_zero(),"late_cancel_revoke");
 mode=0;now=100000000000ull;f.dirty();cancelled=true;const unsigned before=post_calls;
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==5&&post_calls==before&&f.tokens_zero()&&f.scratch_zero(),"initial_cancel_no_post");
 cancelled=false;throws=true;f.dirty();
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==5&&post_calls==before&&f.tokens_zero()&&f.scratch_zero(),"throwing_cancel_no_c_exception");
 throws=false;mode=7;f.dirty(); // transport invokes the supplied cancellation trampoline
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==0&&f.out.complete==1,"transport_cancel_trampoline_healthy");
 mode=9;f.dirty();
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==5&&!f.out.complete&&f.tokens_zero()&&f.scratch_zero(),"throwing_cancel_within_C_transport");
 throws=false;mode=10;now=100000000000ull;f.dirty();post_completed=false;
 watched_form=f.form.data();watched_form_bytes=f.form.size();watched_raw=f.raw.data();watched_raw_bytes=f.raw.size();
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==6&&!f.out.complete&&f.tokens_zero()&&f.scratch_zero(),"full_scratch_cleanup_before_final_budget_gate");
 mode=8;f.dirty();
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==7&&!f.out.complete&&f.tokens_zero()&&f.scratch_zero()&&owner_faults>=1,"clock_failure_revoke");
 mode=0;now=100000000000ull;f.dirty();const std::string good=reply;reply="{\"access_token\":\"fake\",\"access_token\":\"other\"}";
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==4&&f.out.transport_status==13&&!f.out.complete&&f.tokens_zero()&&f.scratch_zero(),"malformed_token_consumer_refusal");
 mode=3;f.dirty();
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==6&&f.out.transport_status==12&&!f.out.complete&&f.tokens_zero()&&f.scratch_zero(),"late_timeout_over_malformed");
 reply=good;mode=0;f.dirty();f.p.form_capacity=10;const unsigned short_before=post_calls;
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==1&&post_calls==short_before&&f.tokens_zero()&&zero(f.form.data(),10)&&zero(f.raw.data(),f.raw.size()),"short_form_no_code_refetch");
 f.p.form_capacity=f.form.size();f.dirty();f.p.fields.code=f.form.data();f.p.fields.code_bytes=9;const auto form_before=f.form;const auto token_before=f.tokens;
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==1&&f.form==form_before&&std::memcmp(&f.tokens,&token_before,sizeof(f.tokens))==0,"aliased_code_untouched");
 f.p.fields.code="fake-code";f.dirty();f.p.timeout_ms=30001;
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==1&&f.tokens_zero()&&f.scratch_zero(),"total_budget_bound");
 f.p.timeout_ms=1000;f.dirty();auto *saved_tokens=f.p.tokens;f.p.tokens=reinterpret_cast<mb_nexus_oauth_tokens_v1*>(f.raw.data());
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==1&&f.raw.front()=='x'&&f.raw.back()=='x'&&f.form.front()=='x',"private_tokens_raw_alias_untouched");
 f.p.tokens=saved_tokens;f.dirty();f.p.users_resolve=f.form.data();
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==1&&f.form.front()=='x'&&f.raw.back()=='x',"resolve_alias_rejected_before_NUL_scan");
 f.p.users_resolve="users.nexusmods.com:443:127.0.0.1";f.dirty();f.p.timeout_ms=30000;mode=4;now=100000000000ull;
 reply="{\"access_token\":\"fake-access\",\"refresh_token\":\"fake-refresh\",\"token_type\":\"Bearer\",\"expires_in\":1}";
 expect(mb_nexus_oauth_exchange_code_v1(&f.p,&f.out)==4&&!f.out.complete&&f.tokens_zero()&&f.scratch_zero(),"pair_expired_before_publication");
 std::cout<<"{\"checks\":"<<checks<<",\"failed\":"<<failed<<",\"scope\":\"explicit_fake_provider_and_clock_boundary_no_TLS\"}\n";
 return failed?1:0;
}
