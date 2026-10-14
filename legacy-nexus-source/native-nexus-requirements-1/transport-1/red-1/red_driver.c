#include "provider_transfer.h"
#include "modbridge/nexus_requirements_query.h"
#include <stdio.h>
#include <string.h>
static int consume(void *p,const uint8_t *b,size_t n){(void)p;(void)b;(void)n;return 0;}
int main(void){
 const char body[]=MB_NEXUS_REQUIREMENTS_BODY_PREFIX "silenthilltownfall" MB_NEXUS_REQUIREMENTS_BODY_MIDDLE "62" MB_NEXUS_REQUIREMENTS_BODY_SUFFIX;
 const uint8_t ca[]="owned-placeholder-no-network"; uint8_t raw[128];
 mb_curl_provider_post_transfer_params_v1 p;mb_curl_provider_transfer_result_v1 r;
 memset(&p,0,sizeof(p));memset(raw,0xa5,sizeof(raw));memset(&r,0,sizeof(r));
 p.struct_size=sizeof(p);p.abi_version=1;p.provider.struct_size=sizeof(p.provider);p.provider.abi_version=1;
 p.provider.url=MB_NEXUS_REQUIREMENTS_GRAPHQL_URL;p.provider.auth_mode=2;p.provider.credential="owned-red";p.provider.credential_bytes=9;
 p.provider.ca_pem=ca;p.provider.ca_bytes=sizeof(ca)-1;p.provider.timeout_ms=1000;p.provider.connect_timeout_ms=100;
 p.provider.body=raw;p.provider.body_capacity=sizeof(raw);p.provider.consume_complete_body=consume;p.request_body=body;p.request_body_bytes=sizeof(body)-1;
 int rc=mb_curl_provider_post_transfer_v1(&p,&r);unsigned wiped=1;for(size_t i=0;i<sizeof(raw);++i)if(raw[i])wiped=0;
 printf("REQUIREMENTS_POST_RED {\"status\":%d,\"result_status\":%d,\"expected_status\":2,\"complete\":%u,\"raw_wiped\":%u,\"runtime_started\":false}\n",rc,(int)r.status,r.complete,wiped);
 return rc==2&&r.status==2&&!r.complete&&wiped ? 0:1;
}
