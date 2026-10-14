#define _POSIX_C_SOURCE 200809L
#include "../curl_download.h"
#include "fixture_ca_blob.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

struct source { unsigned calls,refuse; };
struct io { unsigned refuse,tamper,cancel; };
static int entropy(void *opaque,uint8_t out[32]) {
 struct source *s=opaque;++s->calls;
 if(s->refuse)return -1;
 errno=0;int rc=getentropy(out,32);return rc||errno?-1:0;
}
static int read_at(void *opaque,int fd,uint64_t off,uint8_t *out,uint32_t n,uint32_t *actual) {
 struct io *io=opaque;ssize_t count=pread(fd,out,n,(off_t)off);
 *actual=count<0?0:(uint32_t)count;
 if(count>0&&io->tamper)out[0]^=1u;
 return count<0?-1:0;
}
static int write_at(void *opaque,int fd,uint64_t off,const uint8_t *data,uint32_t n,uint32_t *actual) {
 struct io *io=opaque;if(io->refuse){*actual=0;return -1;}
 ssize_t count=pwrite(fd,data,n,(off_t)off);*actual=count<0?0:(uint32_t)count;return count<0?-1:0;
}
static int cancelled(void *opaque,uint64_t received) {
 return ((struct io *)opaque)->cancel&&received>0;
}
static void sha_hex(const uint8_t *sha,char out[65]) {
 static const char hex[]="0123456789abcdef";
 for(unsigned i=0;i<32;++i){out[i*2]=hex[sha[i]>>4];out[i*2+1]=hex[sha[i]&15];}out[64]=0;
}
int main(int argc,char **argv) {
 if(argc==2&&!strcmp(argv[1],"init-refuse")) {
  struct source source={.refuse=1};
  mb_curl_runtime_params_v1 p={sizeof(p),1,&source,entropy};
  mb_curl_runtime_result_v1 a,b;
  int first=mb_curl_runtime_start_v1(&p,&a),second=mb_curl_runtime_start_v1(&p,&b);
  printf("{\"kind\":\"init-refuse\",\"first\":%d,\"second\":%d,\"terminal\":%u,\"source_calls\":%u}\n",first,second,b.terminal,source.calls);
  return first!=0&&second!=0&&b.terminal&&source.calls==1?0:1;
 }
 if(argc<3||(argc-2)%4)return 2;
 struct source source={0};mb_curl_runtime_params_v1 start={sizeof(start),1,&source,entropy};
 mb_curl_runtime_result_v1 runtime;
 int init=mb_curl_runtime_start_v1(&start,&runtime);
 printf("{\"kind\":\"start\",\"status\":%d,\"initialized\":%u,\"entropy_callbacks\":%llu}\n",init,runtime.initialized,(unsigned long long)runtime.entropy_callbacks);
 if(init)return 1;
 for(int at=2,index=0;at<argc;at+=4,++index) {
  unsigned port=(unsigned)strtoul(argv[at],NULL,10);
  const char *mode=argv[at+1];uint64_t cap=strtoull(argv[at+2],NULL,10);
  unsigned timeout=(unsigned)strtoul(argv[at+3],NULL,10);
  char path[4096],url[256],resolve[256];
  const char *host=!strcmp(mode,"wrong-name")?"wrong.native-https.modbridge.test":"native-https.modbridge.test";
  if(snprintf(path,sizeof(path),"%s/job-%d.archive",argv[1],index)<0)return 2;
  snprintf(url,sizeof(url),"https://%s:%u/owned-archive?grant=fixture-only",host,port);
  snprintf(resolve,sizeof(resolve),"%s:%u:127.0.0.1",host,port);
  int fd=open(path,O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600);if(fd<0)return 2;
  struct io io={.refuse=!strcmp(mode,"sink-refuse"),.tamper=!strcmp(mode,"readback-tamper"),.cancel=!strcmp(mode,"cancel")};
  mb_curl_download_params_v1 p={0};p.struct_size=sizeof(p);p.abi_version=1;
  p.url=url;p.resolve=resolve;p.timeout_ms=timeout;p.connect_timeout_ms=timeout<5000u?timeout:5000u;
  p.ca_pem=(const uint8_t *)(!strcmp(mode,"wrong-root")?fixture_wrong_ca:fixture_ca);
  p.ca_bytes=!strcmp(mode,"wrong-root")?sizeof(fixture_wrong_ca)-1u:sizeof(fixture_ca)-1u;
  p.sink=(mb_download_file_sink_params_v1){sizeof(p.sink),1,fd,0,cap,&io,read_at,write_at};
  p.cancel_context=&io;p.cancelled=cancelled;
  mb_curl_download_result_v1 result={0};int rc=mb_curl_download_v1(&p,&result);
  char sha[65];sha_hex(result.file.sha256,sha);
  struct stat st;if(fstat(fd,&st)||close(fd))return 2;
  if(mb_curl_runtime_status_v1(&runtime))return 2;
  printf("{\"kind\":\"job\",\"index\":%d,\"return\":%d,\"status\":%u,\"complete\":%u,\"http\":%u,\"curl\":%d,\"received\":%llu,\"file_bytes\":%llu,\"file_complete\":%u,\"readback\":%u,\"sha256\":\"%s\",\"feed_calls\":%u,\"write_calls\":%u,\"read_calls\":%u,\"owner_terminal\":%u,\"entropy_callbacks\":%llu}\n",
   index,rc,(unsigned)result.status,result.complete,result.http_status,result.curl_code,
   (unsigned long long)result.received_bytes,(unsigned long long)st.st_size,result.file.complete,result.file.readback_verified,sha,
   result.file.feed_calls,result.file.write_calls,result.file.read_calls,runtime.terminal,(unsigned long long)runtime.entropy_callbacks);
 }
 int closed=mb_curl_runtime_close_v1(&runtime);
 printf("{\"kind\":\"close\",\"status\":%d,\"initialized\":%u,\"terminal\":%u,\"window_active\":%u,\"entropy_windows\":%llu,\"entropy_callbacks\":%llu,\"entropy_bytes\":%llu,\"source_calls\":%u}\n",closed,runtime.initialized,runtime.terminal,runtime.window_active,(unsigned long long)runtime.entropy_windows,(unsigned long long)runtime.entropy_callbacks,(unsigned long long)runtime.entropy_bytes,source.calls);
 return closed?1:0;
}
