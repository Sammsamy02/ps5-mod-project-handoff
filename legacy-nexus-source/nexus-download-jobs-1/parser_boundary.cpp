#include "jobs.h"
#include "download/curl_download.h"
#include <array>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
extern "C" int mb_nexus_auth_admit_v1(mb_nexus_auth_context_v1*,const mb_nexus_auth_admit_params_v1*,mb_nexus_auth_result_v1 *out) noexcept {*out={};out->status=MB_NEXUS_AUTH_OK;out->complete=1;return 0;}
extern "C" int mb_nexus_auth_resolve_download_v1(mb_nexus_auth_context_v1*,const mb_nexus_auth_resolve_params_v1*,mb_nexus_auth_resolve_result_v1*) noexcept{return MB_NEXUS_AUTH_BAD_ARGUMENT;}
extern "C" int mb_curl_download_v1(const mb_curl_download_params_v1*,mb_curl_download_result_v1*){return MB_CURL_DOWNLOAD_BAD_ARGUMENT;}
extern "C" int mb_archive_analyze_v1(const mb_archive_analysis_params_v1*,mb_archive_analysis_result_v1*) noexcept{return MB_ARCHIVE_ANALYSIS_BAD_ARGUMENT;}
int entropy(void*,uint8_t *out){std::memset(out,1,32);return 0;}
int main(){
 char name[]="/tmp/mb-jobs-parser-XXXXXX";if(!mkdtemp(name))return 10;
 int dir=open(name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(dir<0)return 11;
 unsigned char auth_marker=1,ca=1;
 mb_nexus_jobs_create_params_v1 cp{};cp.struct_size=sizeof(cp);cp.abi_version=1;cp.auth=reinterpret_cast<mb_nexus_auth_context_v1*>(&auth_marker);cp.scratch_directory_fd=dir;cp.ca_pem=&ca;cp.ca_bytes=1;cp.entropy_read32=entropy;cp.resolver_timeout_ms=1000;cp.download_timeout_ms=1000;cp.analysis_timeout_ms=1000;cp.connect_timeout_ms=100;cp.archive_cap_bytes=1024;
 mb_nexus_jobs_context_v1 *ctx=nullptr;mb_nexus_jobs_result_v1 result{};if(mb_nexus_jobs_create_v1(&cp,&ctx,&result))return 12;
 std::string cookie(43,'a'),csrf(43,'b'),origin="http://127.0.0.1:10000";
 std::string body=R"({"kind":"download_analyze","game":"skyrimspecialedition","mod_id":1,"file_id":2,"idempotency_key":"owned_key_1",)";
 std::array<char,4096> output{};output.fill('X');
 mb_nexus_jobs_submit_params_v1 sp{};sp.struct_size=sizeof(sp);sp.abi_version=1;sp.cookie=cookie.data();sp.cookie_bytes=cookie.size();sp.csrf=csrf.data();sp.csrf_bytes=csrf.size();sp.origin=origin.data();sp.origin_bytes=origin.size();sp.intent=reinterpret_cast<const uint8_t*>(body.data());sp.intent_bytes=body.size();sp.output=output.data();sp.output_capacity=output.size();
 int code=mb_nexus_jobs_submit_v1(ctx,&sp,&result);bool erased=true;for(char c:output)if(c)erased=false;
 std::cout<<"{\"status\":"<<code<<",\"complete\":"<<result.complete<<",\"job_count\":"<<result.job_count<<",\"output_erased\":"<<(erased?"true":"false")<<"}\n";
 mb_nexus_jobs_free_v1(&ctx);close(dir);rmdir(name);
 return code==MB_NEXUS_JOBS_BAD_ARGUMENT&&erased?0:1;
}
