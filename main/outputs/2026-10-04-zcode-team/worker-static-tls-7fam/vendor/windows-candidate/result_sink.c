#include "result_sink.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
struct destination {int fd,socket;};
static ptrdiff_t write_bytes(void *context,const void *bytes,size_t size,int32_t *error){
 struct destination *d=context;ssize_t n=d->socket?send(d->fd,bytes,size,MSG_DONTWAIT|MSG_NOSIGNAL):write(d->fd,bytes,size);
 if(n<0)*error=errno;return n;
}
static int32_t inspect(int fd,struct stat *st){
 for(unsigned i=0;i<8;++i){if(!fstat(fd,st))return 0;if(errno!=EINTR)return errno;}return EAGAIN;
}
static int32_t file_record(const char *path,const char *bytes,size_t size){
 int fd=-1;for(unsigned i=0;i<8;++i){fd=open(path,O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC|O_NONBLOCK|O_NOFOLLOW,0600);if(fd>=0)break;if(errno!=EINTR)return errno;}
 if(fd<0)return EAGAIN;
 struct stat st;int32_t error=inspect(fd,&st);
 if(!error && !S_ISREG(st.st_mode))error=EINVAL;
 if(!error){struct destination d={fd,0};mb_owned_write_result result;
  if(mb_owned_write_record(write_bytes,&d,bytes,size,&result))error=result.error;}
 /* Attempt close once. Failure (including uncertain EINTR) is surfaced; no
  * retry can accidentally close a reused descriptor. Process teardown remains
  * the fallback if the native kernel kept that descriptor open. */
 if(close(fd) && !error)error=errno;
 return error;
}
static int32_t stdout_record(int fd,const char *bytes,size_t size){
 struct stat st;int32_t error=inspect(fd,&st);if(error)return error;
 if(!S_ISREG(st.st_mode) && !S_ISSOCK(st.st_mode))return ENOTSUP;
 struct destination d={fd,S_ISSOCK(st.st_mode)};mb_owned_write_result result;
 return mb_owned_write_record(write_bytes,&d,bytes,size,&result)?result.error:0;
}
static void remember(int32_t error,int32_t *first,int32_t *last){*last=error;if(error && !*first)*first=error;}
int mb_owned_result_emit(mb_owned_result_sink *sink,const mb_owned_result_snapshot *snapshot){
 if(!sink || !snapshot || !sink->path || sink->attempts>=MB_OWNED_MAX_RECORDS)return -1;
 ++sink->attempts;mb_owned_result_snapshot s=*snapshot;s.delivery=sink->delivery;
 char record[MB_OWNED_RESULT_CAPACITY];size_t n=mb_owned_result_format(record,sizeof(record),&s);if(!n)return -1;
 if(sink->path[0] && !sink->file_disabled){
  int32_t error=file_record(sink->path,record,n);
  remember(error,&sink->delivery.first_file_error,&sink->delivery.last_file_error);
  if(error)sink->file_disabled=1;else ++sink->delivery.file_records;
 }
 if(sink->stdout_fd>=0){
  s.delivery=sink->delivery;n=mb_owned_result_format(record,sizeof(record),&s);
  int32_t error=n?stdout_record(sink->stdout_fd,record,n):EINVAL;
  remember(error,&sink->delivery.first_stdout_error,&sink->delivery.last_stdout_error);
  if(!error)++sink->delivery.stdout_records;
 }
 return sink->delivery.first_file_error || sink->delivery.first_stdout_error?-1:0;
}
