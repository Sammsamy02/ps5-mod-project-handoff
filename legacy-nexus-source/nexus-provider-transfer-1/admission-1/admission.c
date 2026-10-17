#include "provider_transfer.h"
#include <stdio.h>
#include <string.h>
static unsigned consumed;
static int consume(void *context,const uint8_t *body,size_t bytes) {
 (void)context;(void)body;(void)bytes;++consumed;return 0;
}
static int zeroed(const uint8_t *body,size_t bytes) {
 size_t i;for(i=0;i<bytes;++i)if(body[i])return 0;return 1;
}
int main(void) {
 static const char credential[]="owned-fixture-secret";
 static const uint8_t ca[]="owned-placeholder-no-network";
 static const struct {const char *url;uint32_t mode;int expected;} cases[]={
  {"https://api.nexusmods.com/v1/games",1,2},
  {"https://api.nexusmods.com:443/v1/games",2,2},
  {"https://users.nexusmods.com/oauth/userinfo",2,2},
  {"https://users.nexusmods.com/oauth/userinfo",1,1},
  {"https://foreign.test/v1/games",1,1},
  {"https://api.nexusmods.com:444/v1/games",1,1},
  {"https://api.nexusmods.com/v1/games?x=1",1,1},
  {"https://api.nexusmods.com/v1/games#part",1,1},
  {"https://x@api.nexusmods.com/v1/games",1,1},
  {"http://api.nexusmods.com/v1/games",1,1},
  {"https://api.nexusmods.com/oauth/userinfo",1,1},
  {"https://users.nexusmods.com/v1/games",2,1}
 };
 uint8_t body[128];mb_curl_provider_transfer_params_v1 p;
 mb_curl_provider_transfer_result_v1 result;size_t i;int rc;
 memset(&p,0,sizeof(p));p.struct_size=sizeof(p);p.abi_version=1;
 p.credential=credential;p.credential_bytes=sizeof(credential)-1;
 p.ca_pem=ca;p.ca_bytes=sizeof(ca)-1;p.timeout_ms=1000;p.connect_timeout_ms=100;
 p.body=body;p.body_capacity=sizeof(body);p.consume_complete_body=consume;
 for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i){
  memset(body,0xa5,sizeof(body));memset(&result,0xa5,sizeof(result));
  p.url=cases[i].url;p.auth_mode=cases[i].mode;
  rc=mb_curl_provider_transfer_v1(&p,&result);
  if(rc!=cases[i].expected||result.status!=rc||result.complete||result.http_status||
     result.received_bytes||!zeroed(body,sizeof(body))||consumed)return 10+(int)i;
 }
 p.url="https://api.nexusmods.com/v1/games";p.auth_mode=1;
 p.credential="invalid\nheader";p.credential_bytes=14;memset(body,0xa5,sizeof(body));
 rc=mb_curl_provider_transfer_v1(&p,&result);
 if(rc!=1||!zeroed(body,sizeof(body))||consumed)return 30;
 p.credential=credential;p.credential_bytes=sizeof(credential)-1;
 memset(body,0xa5,sizeof(body));rc=mb_curl_provider_transfer_v1(&p,NULL);
 if(rc!=1||!zeroed(body,sizeof(body))||consumed)return 31;
 p.credential=(const char *)body+1;p.credential_bytes=8;memset(body,0xa5,sizeof(body));
 rc=mb_curl_provider_transfer_v1(&p,&result);
 if(rc!=1||body[1]!=0xa5||consumed)return 32;
 p.credential=credential;p.credential_bytes=sizeof(credential)-1;
 memset(body,0xa5,sizeof(body));rc=mb_curl_provider_transfer_v1(&p,(void *)body);
 if(rc!=1||body[0]!=0xa5||consumed)return 33;
 puts("provider preflight cases passed; no runtime/network started");return 0;
}
