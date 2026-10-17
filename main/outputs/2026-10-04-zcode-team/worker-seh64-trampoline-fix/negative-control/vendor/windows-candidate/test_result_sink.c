#include "result_sink.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 char path[]="/tmp/mb-owned-report-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);
 mb_owned_result_sink sink={.path=path,.stdout_fd=-1};
 mb_owned_result_snapshot s={.build_id="0123456789abcdef0123456789abcdef",.phase=MB_OWNED_RESULT_STARTING};
 CHECK(!mb_owned_result_emit(&sink,&s) && sink.delivery.file_records==1);
 char buf[MB_OWNED_RESULT_CAPACITY*2];ssize_t n=read(fd,buf,sizeof(buf)-1);CHECK(n>0);buf[n]=0;
 CHECK(strstr(buf,"\"marker\":\"modbridge.owned-dll\""));
 s.phase=MB_OWNED_RESULT_RETIRED;s.sequence=1;CHECK(!mb_owned_result_emit(&sink,&s));
 CHECK(sink.delivery.file_records==2 && !sink.delivery.first_file_error);CHECK(!close(fd));
 CHECK(!unlink(path));CHECK(!symlink("/dev/null",path));
 mb_owned_result_sink refused={.path=path,.stdout_fd=-1};CHECK(mb_owned_result_emit(&refused,&s));
 CHECK(refused.file_disabled && refused.delivery.first_file_error);int first=refused.delivery.first_file_error;
 CHECK(!unlink(path));CHECK(mb_owned_result_emit(&refused,&s));CHECK(refused.delivery.first_file_error==first);
 CHECK(!mkfifo(path,0600));refused=(mb_owned_result_sink){.path=path,.stdout_fd=-1};
 CHECK(mb_owned_result_emit(&refused,&s));CHECK(!unlink(path));
 int pair[2];CHECK(!socketpair(AF_UNIX,SOCK_STREAM,0,pair));
 mb_owned_result_sink console={.path="",.stdout_fd=pair[0]};CHECK(!mb_owned_result_emit(&console,&s));
 CHECK(read(pair[1],buf,sizeof(buf))>0 && console.delivery.stdout_records==1);CHECK(!close(pair[1]));
 CHECK(mb_owned_result_emit(&console,&s) && console.delivery.first_stdout_error);CHECK(!close(pair[0]));
 int pipefd[2];CHECK(!pipe(pipefd));console=(mb_owned_result_sink){.path="",.stdout_fd=pipefd[1]};
 CHECK(mb_owned_result_emit(&console,&s) && console.delivery.first_stdout_error==ENOTSUP);
 CHECK(!close(pipefd[0]) && !close(pipefd[1]));
 puts("Actual append file, symlink/FIFO refusal, sticky errors, socket stdout and closed-peer no-SIGPIPE passed");return 0;
}
