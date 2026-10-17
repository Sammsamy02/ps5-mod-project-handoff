/* Offline auth admission stub proves route wiring; no account or provider. */
#include "http.h"
#include "auth.h"
#include "download_store/store.h"
#include "mod_service/read_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned calls, releases, admissions, store_calls;
int mb_download_store_list(mb_download_store *store,char *out,size_t cap){assert(store&&cap==HTTP_DOWNLOADS_LIMIT);++store_calls;strcpy(out,"{\"items\":[]}");return MB_DOWNLOAD_STORE_OK;}
static const char valid_cookie[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
int mb_nexus_auth_admit_v1(mb_nexus_auth_context_v1 *ctx,
 const mb_nexus_auth_admit_params_v1 *p, mb_nexus_auth_result_v1 *r) {
 assert(ctx && p->admission == MB_NEXUS_AUTH_ADMISSION_READ && !p->require_account);
 ++admissions; r->credentials_present = 0;
 r->status = p->cookie && p->cookie_bytes == 43 && !memcmp(p->cookie, valid_cookie,43)
  ? MB_NEXUS_AUTH_OK : MB_NEXUS_AUTH_SESSION_REQUIRED;
 r->complete = r->status == MB_NEXUS_AUTH_OK; return r->status;
}
#define STUB(name) int mb_nexus_auth_##name##_v1(mb_nexus_auth_context_v1 *c, const mb_nexus_auth_##name##_params_v1 *p, mb_nexus_auth_result_v1 *r) { (void)c;(void)p;(void)r; assert(0); return 14; }
STUB(session) STUB(start) STUB(callback) STUB(account) STUB(disconnect) STUB(catalog) STUB(acquisition_plan)
static void release(void *o,const char *b,size_t n) {(void)o;(void)b;(void)n;++releases;}
static mfs_owned request(mfs_service *s,mfs_view v) {
 (void)s; ++calls;
 assert(v.length==strlen("{\"method\":\"records.list\",\"params\":{}}") && !memcmp(v.bytes,"{\"method\":\"records.list\",\"params\":{}}",v.length));
 static const char body[]="{\"ok\":true,\"result\":{\"mods\":[],\"games\":[]}}";
 mfs_owned out={body,sizeof(body)-1,0,release};return out;
}
static void check(struct host_info *info,const char *cookie,int status) {
 unsigned char bytes[1024];struct http_response out;
 int n=snprintf((char *)bytes,sizeof(bytes),"GET /api/mod-service/records HTTP/1.1\r\nHost: 127.0.0.1:18080\r\n%s%s%s\r\n",cookie?"Cookie: mb_session=":"",cookie?cookie:"",cookie?"\r\n":"");
 http_handle(bytes,(size_t)n,info,&out);assert(out.status==status);
 if(status==200) assert(out.owned_body && out.size==strlen(out.owned_body));
 http_response_dispose(&out);
}
static void download_check(struct host_info *info,const char *cookie,const char *method,int status) {
 unsigned char bytes[1024];struct http_response out;
 int n=snprintf((char*)bytes,sizeof(bytes),"%s /api/downloads HTTP/1.1\r\nHost: 127.0.0.1:18080\r\n%s%s%s\r\n",method,cookie?"Cookie: mb_session=":"",cookie?cookie:"",cookie?"\r\n":"");
 http_handle(bytes,(size_t)n,info,&out);assert(out.status==status);
 if(status==200){assert(!strcmp(out.owned_body,"{\"items\":[]}"));assert(!strstr(out.owned_body,"archive_path"));}
 http_response_dispose(&out);
}
int main(void) {
 struct host_info info={0}; struct mb_mod_service_bridge bridge={(mfs_service *)1,request};
 strcpy(info.authority,"127.0.0.1:18080");strcpy(info.url_origin,"http://127.0.0.1:18080");
 info.auth=(mb_nexus_auth_context_v1 *)1;info.mod_service=&bridge;
 check(&info,0,401);check(&info,"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",401);
 assert(!calls && !releases && admissions==2);
 check(&info,valid_cookie,200);assert(calls==1 && releases==1 && admissions==3);
 info.download_store=(mb_download_store*)1;
 download_check(&info,0,"GET",401);download_check(&info,"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","GET",401);assert(!store_calls);
 download_check(&info,valid_cookie,"GET",200);assert(store_calls==1);
 download_check(&info,valid_cookie,"HEAD",405);assert(store_calls==1);
 info.download_store=0;download_check(&info,valid_cookie,"GET",503);
 info.auth=0;check(&info,valid_cookie,503);assert(calls==1);
 return 0;
}
