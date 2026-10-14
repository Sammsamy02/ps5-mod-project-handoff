#define _POSIX_C_SOURCE 200809L
#include "file_sink.h"
#include <fcntl.h>
#include <mbedtls/platform_util.h>
#include <string.h>
#include <sys/stat.h>

static int overlaps(const void *a,size_t n,const void *b,size_t m) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(n>UINTPTR_MAX-x||m>UINTPTR_MAX-y)return 1;
 return x<y+m&&y<x+n;
}
static void free_hash(mb_download_file_sink_v1 *s) {
 if(s->sha_active){mbedtls_sha256_free(&s->sha);s->sha_active=0;}
}
static int failed(mb_download_file_sink_v1 *s,mb_download_file_sink_status_v1 status) {
 free_hash(s);mbedtls_platform_zeroize(s->scratch,sizeof(s->scratch));
 s->phase=MB_DOWNLOAD_FILE_SINK_FAILED;s->status=status;memset(s->sha256,0,sizeof(s->sha256));return (int)status;
}
static void result_from(const mb_download_file_sink_v1 *s,mb_download_file_sink_result_v1 *r) {
 memset(r,0,sizeof(*r));r->struct_size=sizeof(*r);r->abi_version=1;r->status=s->status;
 r->complete=s->phase==MB_DOWNLOAD_FILE_SINK_COMPLETE;r->readback_verified=r->complete;
 r->bytes_received=s->bytes_received;r->bytes_written=s->bytes_written;memcpy(r->sha256,s->sha256,32);
 r->feed_calls=s->feed_calls;r->write_calls=s->write_calls;r->read_calls=s->read_calls;
}
int mb_download_file_sink_start_v1(mb_download_file_sink_v1 *s,const mb_download_file_sink_params_v1 *p) {
 if(!s||!p||overlaps(s,sizeof(*s),p,sizeof(*p))||p->struct_size!=sizeof(*p)||p->abi_version!=1||p->reserved||
 p->fd<0||!p->cap_bytes||p->cap_bytes>MB_DOWNLOAD_FILE_SINK_MAX_BYTES||!p->read_at||!p->write_at)return MB_DOWNLOAD_FILE_SINK_BAD_ARGUMENT;
 if(s->phase!=MB_DOWNLOAD_FILE_SINK_UNINITIALIZED||s->sha_active)return MB_DOWNLOAD_FILE_SINK_BAD_STATE;
 memset(s,0,sizeof(*s));s->params=*p;s->phase=MB_DOWNLOAD_FILE_SINK_ACTIVE;
 struct stat st;int flags=fcntl(p->fd,F_GETFL);
 if(flags<0||(flags&O_ACCMODE)!=O_RDWR||(flags&O_APPEND)||fstat(p->fd,&st)||!S_ISREG(st.st_mode)||st.st_size!=0)
 return failed(s,MB_DOWNLOAD_FILE_SINK_FD_REFUSED);
 s->device=(uint64_t)st.st_dev;s->inode=(uint64_t)st.st_ino;
 mbedtls_sha256_init(&s->sha);s->sha_active=1;
 if(mbedtls_sha256_starts(&s->sha,0))return failed(s,MB_DOWNLOAD_FILE_SINK_HASH_ERROR);
 return MB_DOWNLOAD_FILE_SINK_OK;
}
int mb_download_file_sink_feed_v1(mb_download_file_sink_v1 *s,const uint8_t *data,size_t n) {
 if(!s)return MB_DOWNLOAD_FILE_SINK_BAD_ARGUMENT;
 if(s->phase!=MB_DOWNLOAD_FILE_SINK_ACTIVE)return MB_DOWNLOAD_FILE_SINK_BAD_STATE;
 if((!data&&n)||(n&&overlaps(s,sizeof(*s),data,n)))return MB_DOWNLOAD_FILE_SINK_BAD_ARGUMENT;
 if(s->feed_calls>=MB_DOWNLOAD_FILE_SINK_MAX_CALLBACKS||n>s->params.cap_bytes-s->bytes_received)
 return failed(s,MB_DOWNLOAD_FILE_SINK_LIMIT);
 ++s->feed_calls;s->bytes_received+=n;size_t at=0;
 while(at<n){uint32_t chunk=(uint32_t)(n-at>MB_DOWNLOAD_FILE_SINK_WORK_BYTES?MB_DOWNLOAD_FILE_SINK_WORK_BYTES:n-at),actual=0;
  if(s->write_calls>=MB_DOWNLOAD_FILE_SINK_MAX_CALLBACKS)return failed(s,MB_DOWNLOAD_FILE_SINK_LIMIT);
  ++s->write_calls;
  int rc=s->params.write_at(s->params.io_context,s->params.fd,s->bytes_written,data+at,chunk,&actual);
  if(actual>chunk)return failed(s,MB_DOWNLOAD_FILE_SINK_WRITE_ERROR);
  s->bytes_written+=actual;
  if(rc||actual!=chunk)return failed(s,MB_DOWNLOAD_FILE_SINK_WRITE_ERROR);
  if(mbedtls_sha256_update(&s->sha,data+at,chunk))return failed(s,MB_DOWNLOAD_FILE_SINK_HASH_ERROR);
  at+=chunk;
 }
 return MB_DOWNLOAD_FILE_SINK_OK;
}
int mb_download_file_sink_finish_v1(mb_download_file_sink_v1 *s,int transport_complete,int has_length,uint64_t length,
                                   mb_download_file_sink_result_v1 *r) {
 if(!s)return MB_DOWNLOAD_FILE_SINK_BAD_ARGUMENT;
 if(!r||overlaps(s,sizeof(*s),r,sizeof(*r))||(transport_complete!=0&&transport_complete!=1)||(has_length!=0&&has_length!=1)){
  return MB_DOWNLOAD_FILE_SINK_BAD_ARGUMENT;
 }
 if(s->phase!=MB_DOWNLOAD_FILE_SINK_ACTIVE){result_from(s,r);r->status=MB_DOWNLOAD_FILE_SINK_BAD_STATE;r->complete=0;r->readback_verified=0;memset(r->sha256,0,32);return MB_DOWNLOAD_FILE_SINK_BAD_STATE;}
 if(!transport_complete){failed(s,MB_DOWNLOAD_FILE_SINK_TRANSFER_INCOMPLETE);result_from(s,r);return s->status;}
 if(has_length&&(length>s->params.cap_bytes||length!=s->bytes_received)){failed(s,MB_DOWNLOAD_FILE_SINK_LENGTH_MISMATCH);result_from(s,r);return s->status;}
 struct stat before,after;
 if(fstat(s->params.fd,&before)||!S_ISREG(before.st_mode)||before.st_size<0||(uint64_t)before.st_dev!=s->device||
 (uint64_t)before.st_ino!=s->inode||(uint64_t)before.st_size!=s->bytes_written||s->bytes_written!=s->bytes_received){
  failed(s,MB_DOWNLOAD_FILE_SINK_FILE_CHANGED);result_from(s,r);return s->status;}
 if(mbedtls_sha256_finish(&s->sha,s->sha256)){failed(s,MB_DOWNLOAD_FILE_SINK_HASH_ERROR);result_from(s,r);return s->status;}
 free_hash(s);
 mbedtls_sha256_context readback;mbedtls_sha256_init(&readback);
 mb_download_file_sink_status_v1 status=MB_DOWNLOAD_FILE_SINK_OK;uint8_t digest[32];uint64_t at=0;
 if(mbedtls_sha256_starts(&readback,0)){status=MB_DOWNLOAD_FILE_SINK_HASH_ERROR;goto done;}
 while(at<s->bytes_written){uint32_t n=(uint32_t)(s->bytes_written-at>sizeof(s->scratch)?sizeof(s->scratch):s->bytes_written-at),actual=0;
  if(s->read_calls>=MB_DOWNLOAD_FILE_SINK_MAX_CALLBACKS){status=MB_DOWNLOAD_FILE_SINK_LIMIT;goto done;}
  ++s->read_calls;
  if(s->params.read_at(s->params.io_context,s->params.fd,at,s->scratch,n,&actual)||actual!=n){status=MB_DOWNLOAD_FILE_SINK_READ_ERROR;goto done;}
  if(mbedtls_sha256_update(&readback,s->scratch,n)){status=MB_DOWNLOAD_FILE_SINK_HASH_ERROR;goto done;}
  at+=n;
 }
 if(mbedtls_sha256_finish(&readback,digest)){status=MB_DOWNLOAD_FILE_SINK_HASH_ERROR;goto done;}
 if(memcmp(digest,s->sha256,32)){status=MB_DOWNLOAD_FILE_SINK_READBACK_MISMATCH;goto done;}
 if(fstat(s->params.fd,&after)||after.st_size!=before.st_size||after.st_dev!=before.st_dev||after.st_ino!=before.st_ino){status=MB_DOWNLOAD_FILE_SINK_FILE_CHANGED;goto done;}
 s->phase=MB_DOWNLOAD_FILE_SINK_COMPLETE;s->status=MB_DOWNLOAD_FILE_SINK_OK;
done:
 mbedtls_sha256_free(&readback);mbedtls_platform_zeroize(s->scratch,sizeof(s->scratch));
 if(status!=MB_DOWNLOAD_FILE_SINK_OK)failed(s,status);
 result_from(s,r);return (int)status;
}
int mb_download_file_sink_abort_v1(mb_download_file_sink_v1 *s) {
 if(!s)return MB_DOWNLOAD_FILE_SINK_BAD_ARGUMENT;
 if(s->phase!=MB_DOWNLOAD_FILE_SINK_ACTIVE)return MB_DOWNLOAD_FILE_SINK_BAD_STATE;
 return failed(s,MB_DOWNLOAD_FILE_SINK_ABORTED);
}
void mb_download_file_sink_dispose_v1(mb_download_file_sink_v1 *s) {
 if(!s)return;
 if(s->phase==MB_DOWNLOAD_FILE_SINK_ACTIVE)(void)failed(s,MB_DOWNLOAD_FILE_SINK_ABORTED);
 else {free_hash(s);mbedtls_platform_zeroize(s->scratch,sizeof(s->scratch));}
}
