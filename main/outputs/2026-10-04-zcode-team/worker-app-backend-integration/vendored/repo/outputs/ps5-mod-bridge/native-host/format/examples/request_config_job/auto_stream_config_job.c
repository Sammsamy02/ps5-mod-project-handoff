#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "file_sink.h"
#include "config_request_producer.h"
#include "fd_io.h"
#include "private/posix_compat.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Ordinary local stream proof, not an HTTP/download client. The input archive
 * remains unchanged. Its size/hash are measured by the sink; profile/base data
 * alone enter the request encoder. No per-mod member facts or subprocess. */
int pm_auto_request_config_job_host(int argc,char **argv);
static mb_download_file_sink_v1 sink;
static mb_config_request_producer_params_v2 params2;
#define params (params2.common)
static uint8_t request_bytes[MB_CONFIG_REQUEST_MAX_BYTES];
static uint8_t context_bytes[MB_CONFIG_CONTEXT_MAX_BYTES];
static uint8_t stream_chunk[65536];
static mb_fd_io_v1 io={sizeof(io),MB_FD_IO_ABI_V1,UINT64_C(536870912),0,200000,0,NULL,NULL,NULL};
static int number(const char *s,uint64_t *out) {
 uint64_t n=0;if(!s||!*s)return 0;
 for(;*s;s++){if(*s<'0'||*s>'9'||n>(UINT64_MAX-(unsigned)(*s-'0'))/10)return 0;n=n*10+(unsigned)(*s-'0');}*out=n;return 1;
}
static int unhex(const char *s,uint8_t out[32]) {
 if(strlen(s)!=64)return 0;
 for(size_t i=0;i<32;i++){unsigned a=(unsigned char)s[2*i],b=(unsigned char)s[2*i+1];
 a=a>='0'&&a<='9'?a-'0':a>='a'&&a<='f'?a-'a'+10:99;
 b=b>='0'&&b<='9'?b-'0':b>='a'&&b<='f'?b-'a'+10:99;
 if(a>15||b>15)return 0;out[i]=(uint8_t)((a<<4)|b);}return 1;
}
static void hex(const uint8_t in[32],char out[65]) {
 static const char digits[]="0123456789abcdef";
 for(size_t i=0;i<32;i++){out[2*i]=digits[in[i]>>4];out[2*i+1]=digits[in[i]&15];}out[64]=0;
}
static int string(char *out,size_t cap,const char *in,uint32_t *bytes) {
 size_t n=strlen(in);if(!n||n>=cap)return 0;memcpy(out,in,n+1);if(bytes)*bytes=(uint32_t)n;return 1;
}
static int stable(const struct stat *a,const struct stat *b) {
 if(a->st_dev!=b->st_dev||a->st_ino!=b->st_ino||a->st_size!=b->st_size)return 0;
#ifdef __APPLE__
 return a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec&&a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec&&
 a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec&&a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
 return a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&
 a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static int save_record(int root,const char *name,const uint8_t *data,uint32_t bytes) {
 int fd=mb_archive_posix_openat(root,name,O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600);if(fd<0)return 1;
 uint32_t actual=0;int rc=mb_fd_io_write_at(&io,fd,0,data,bytes,&actual)||actual!=bytes||fsync(fd);
 if(!rc){uint8_t check[MB_CONFIG_CONTEXT_MAX_BYTES];struct stat s;
 rc=bytes>sizeof(check)||fstat(fd,&s)||s.st_size!=bytes||mb_fd_io_read_at(&io,fd,0,check,bytes,&actual)||actual!=bytes||memcmp(check,data,bytes);}
 if(close(fd))rc=1;return rc;
}
int main(int argc,char **argv) {
 if(argc<11||(unsigned)argc>10+MB_CONFIG_MAX_SECTIONS){
 fprintf(stderr,"usage: stream_config_job INBOX RAW_SOURCE ID BASE_BYTES BASE_SHA TOKEN BASENAME MAX_MEMBER_BYTES PLATFORM SECTION [SECTION...]\n");return 2;}
 params2.struct_size=sizeof(params2);params2.abi_version=MB_CONFIG_REQUEST_PRODUCER_ABI_V2;
 params2.target_selection=MB_CONFIG_TARGET_UNREAL_PROJECT_PLATFORM;
 params.struct_size=sizeof(params);params.abi_version=MB_CONFIG_REQUEST_PRODUCER_ABI_V1;params.role=MB_CONFIG_ROLE_SCALAR_INI;
 if(!string(params.request_id,sizeof(params.request_id),argv[3],NULL)||!number(argv[4],&params.base_bytes)||
 !unhex(argv[5],params.base_sha256)||!unhex(argv[6],params.target_token)||
 !string(params.basename,sizeof(params.basename),argv[7],&params.basename_bytes)||!number(argv[8],&params.max_member_bytes)||
 !string(params2.platform,sizeof(params2.platform),argv[9],&params2.platform_bytes))return 2;
 strcpy(params.source_ref,"source.archive");strcpy(params.base_ref,"base.pak");strcpy(params.context_ref,"profile.ctx");
 params.section_count=(uint32_t)(argc-10);
 for(uint32_t i=0;i<params.section_count;i++)if(!string(params.sections[i],sizeof(params.sections[i]),argv[10+i],&params.section_bytes[i]))return 2;
 int root=-1,input=-1,output=-1,rc=1;const char *stage="source";struct stat before,after;
 mb_download_file_sink_result_v1 received={0};mb_config_request_producer_result_v2 encoded={0};
 root=open(argv[1],O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
 input=open(argv[2],O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
 if(root<0||input<0||fstat(input,&before)||!S_ISREG(before.st_mode)||before.st_size<=0||
 (uint64_t)before.st_size>MB_DOWNLOAD_FILE_SINK_MAX_BYTES)goto done;
 output=mb_archive_posix_openat(root,params.source_ref,O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600);if(output<0)goto done;
 mb_download_file_sink_params_v1 sink_params={sizeof(sink_params),MB_DOWNLOAD_FILE_SINK_ABI_V1,output,0,
 MB_DOWNLOAD_FILE_SINK_MAX_BYTES,&io,mb_fd_io_read_at,mb_fd_io_write_at};
 stage="stream";if(mb_download_file_sink_start_v1(&sink,&sink_params))goto done;
 uint64_t at=0;uint32_t chunks=0;
 while(at<(uint64_t)before.st_size){
 /* Deliberately split the actual tiny archive across independent feeds. */
 uint32_t chunk_limit=chunks<3?97u:(uint32_t)sizeof(stream_chunk);
 uint32_t wanted=(uint32_t)((uint64_t)before.st_size-at>chunk_limit?chunk_limit:(uint64_t)before.st_size-at),actual=0;
 if(mb_fd_io_read_at(&io,input,at,stream_chunk,wanted,&actual)||actual!=wanted||mb_download_file_sink_feed_v1(&sink,stream_chunk,wanted))goto done;
 at+=wanted;chunks++;
 }
 uint8_t extra;ssize_t end=pread(input,&extra,1,(off_t)at);
 if(end!=0||fstat(input,&after)||!stable(&before,&after)||fsync(output))goto done;
 stage="stream_finish";
 if(mb_download_file_sink_finish_v1(&sink,1,1,(uint64_t)before.st_size,&received)||!received.complete||!received.readback_verified)goto done;
 params.download_complete=1;params.source_bytes=received.bytes_received;memcpy(params.source_sha256,received.sha256,32);
 stage="encode";
 if(mb_config_request_encode_v2(&params2,request_bytes,sizeof(request_bytes),context_bytes,sizeof(context_bytes),&encoded)!=MB_CONFIG_PRODUCER_OK||!encoded.complete)goto done;
 if(save_record(root,params.context_ref,context_bytes,encoded.common.context_bytes)||save_record(root,"job.request",request_bytes,encoded.common.request_bytes)||fsync(root))goto done;
 char digest[65];hex(received.sha256,digest);
 printf("{\"phase\":\"stream_to_request\",\"status\":\"PASS\",\"bytes_received\":%" PRIu64 ",\"sha256\":\"%s\",\"feed_calls\":%u,\"stream_readback_verified\":true,\"request_generated_in_C\":true,\"source_identity_measured\":true,\"request_bytes\":%u,\"context_bytes\":%u,\"preknown_member_facts\":false,\"supplied_target_mount_seed\":false,\"network_exercised\":false,\"native_execution\":false}\n",
 received.bytes_received,digest,chunks,encoded.common.request_bytes,encoded.common.context_bytes);
 rc=0;
done:
 mb_download_file_sink_dispose_v1(&sink);
 if(output>=0&&close(output))rc=1;
 if(input>=0&&close(input))rc=1;
 if(root>=0&&close(root))rc=1;
 if(rc){fprintf(stderr,"local stream preparation refused at %s; owned scratch retained; errno=%d\n",stage,errno);return rc;}
 char *job_argv[]={(char *)"request_config_job",argv[1],(char *)"job.request"};
 return pm_auto_request_config_job_host(3,job_argv);
}
