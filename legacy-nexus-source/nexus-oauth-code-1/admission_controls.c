#include "oauth_code_transfer.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failed,consumed;
static int consumer(void *ctx,const uint8_t *data,size_t bytes){(void)ctx;(void)data;(void)bytes;++consumed;return 0;}
static int zero(const void *data,size_t bytes){const unsigned char*p=data;while(bytes--)if(*p++)return 0;return 1;}
static void expect(int ok,const char *name){++checks;if(!ok){++failed;fprintf(stderr,"%s failed\n",name);}}
int main(void){
 uint8_t raw[131072];
 const uint8_t ca[]="fake-test-ca";
 const char form[]="grant_type=authorization_code&redirect_uri=http%3A%2F%2F127.0.0.1%3A8085%2Fauth%2Fcallback&client_id=fake_app&code=fake%2Bcode&code_verifier=dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
 mb_curl_oauth_code_transfer_params_v1 p={0};mb_curl_oauth_code_transfer_result_v1 out;
 p.struct_size=sizeof(p);p.abi_version=1;p.form=form;p.form_bytes=sizeof(form)-1;
 p.ca_pem=ca;p.ca_bytes=sizeof(ca)-1;p.resolve="users.nexusmods.com:443:127.0.0.1";
 p.timeout_ms=1000;p.connect_timeout_ms=500;p.body=raw;p.body_capacity=sizeof(raw);p.consume_complete_body=consumer;
 memset(raw,'x',sizeof(raw));
 expect(mb_curl_oauth_code_transfer_v1(&p,&out)==MB_CURL_DOWNLOAD_BAD_STATE&&out.status==MB_CURL_DOWNLOAD_BAD_STATE&&!out.complete&&zero(raw,sizeof(raw))&&!consumed,"admitted_post_without_runtime");
#define PREFIX "grant_type=authorization_code&redirect_uri=http%3A%2F%2F127.0.0.1%3A8085%2Fauth%2Fcallback&client_id="
#define VERIFIER "&code_verifier=dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"
 const char *bad[]={"grant_type=refresh_token&redirect_uri=x&client_id=fake_app&code=x&code_verifier=x", "grant_type=authorization_code&client_id=fake_app&redirect_uri=x&code=x&code_verifier=x", (PREFIX "public_test&code=x" VERIFIER), (PREFIX "fake_app&code=x+y" VERIFIER), (PREFIX "fake_app&code=%61" VERIFIER), (PREFIX "fake_app&code=%2b" VERIFIER), (PREFIX "fake_app&code=%00" VERIFIER), (PREFIX "fake_app&code=x" VERIFIER "&client_secret=fake"), ("grant_type=authorization_code&redirect_uri=http%3A%2F%2F127.0.0.1%3A8085%2F.%2Fauth%2Fcallback&client_id=fake_app&code=x" VERIFIER)};
 for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i){p.form=bad[i];p.form_bytes=strlen(bad[i]);memset(raw,'x',sizeof(raw));expect(mb_curl_oauth_code_transfer_v1(&p,&out)==MB_CURL_DOWNLOAD_BAD_ARGUMENT&&out.status==MB_CURL_DOWNLOAD_BAD_ARGUMENT&&!out.complete&&zero(raw,sizeof(raw))&&!consumed,"noncanonical_form_refused");}
 p.form=form;p.form_bytes=sizeof(form)-1;p.resolve="api.nexusmods.com:443:127.0.0.1";memset(raw,'x',sizeof(raw));
 expect(mb_curl_oauth_code_transfer_v1(&p,&out)==MB_CURL_DOWNLOAD_BAD_ARGUMENT&&zero(raw,sizeof(raw)),"resolve_wrong_origin");
 p.resolve=NULL;memset(raw,'x',sizeof(raw));p.form=(const char*)raw;p.form_bytes=10;
 expect(mb_curl_oauth_code_transfer_v1(&p,&out)==MB_CURL_DOWNLOAD_BAD_ARGUMENT&&raw[0]=='x'&&raw[sizeof(raw)-1]=='x',"form_alias_untouched");
 p.form=form;p.form_bytes=sizeof(form)-1;memset(raw,'x',sizeof(raw));
 expect(mb_curl_oauth_code_transfer_v1(&p,NULL)==MB_CURL_DOWNLOAD_BAD_ARGUMENT&&zero(raw,sizeof(raw)),"null_result_independent_wipe");
 mb_curl_provider_transfer_params_v1 get={0};get.struct_size=sizeof(get);get.abi_version=1;get.url="https://users.nexusmods.com/oauth/token";get.auth_mode=2;get.credential="fake-bearer";get.credential_bytes=11;get.ca_pem=ca;get.ca_bytes=sizeof(ca)-1;get.timeout_ms=1000;get.connect_timeout_ms=500;get.body=raw;get.body_capacity=sizeof(raw);get.consume_complete_body=consumer;
 memset(raw,'x',sizeof(raw));expect(mb_curl_provider_transfer_v1(&get,&out)==MB_CURL_DOWNLOAD_BAD_ARGUMENT&&zero(raw,sizeof(raw)),"get_token_route_still_refused");
 get.url="https://users.nexusmods.com/oauth/userinfo";memset(raw,'x',sizeof(raw));expect(mb_curl_provider_transfer_v1(&get,&out)==MB_CURL_DOWNLOAD_BAD_STATE&&zero(raw,sizeof(raw)),"userinfo_get_preserved");
 get.url="https://api.nexusmods.com/v1/games/fake_game/mods/1/files/2/download_link?key=fake%2Bgrant&expires=123";memset(raw,'x',sizeof(raw));expect(mb_curl_provider_transfer_v1(&get,&out)==MB_CURL_DOWNLOAD_BAD_STATE&&zero(raw,sizeof(raw)),"canonical_download_query_preserved");
 get.url="https://api.nexusmods.com/v1/games/fake_game/mods/1/files/2/download_link?key=fake%2bgrant&expires=123";memset(raw,'x',sizeof(raw));expect(mb_curl_provider_transfer_v1(&get,&out)==MB_CURL_DOWNLOAD_BAD_ARGUMENT&&zero(raw,sizeof(raw)),"noncanonical_query_refused");
 printf("{\"checks\":%u,\"failed\":%u,\"consumer_calls\":%u,\"runtime_started\":false,\"network_started\":false}\n",checks,failed,consumed);
 return failed?1:0;
}
