#include "result_sink.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned close_calls;
/* Only result_sink.c's host call site is renamed. Model an interrupted close
 * which already consumed the descriptor, so retry could affect another owner. */
int mb_test_report_close(int fd){++close_calls;if(close(fd))abort();errno=EINTR;return -1;}
int main(void){
 char path[]="/tmp/mb-owned-close-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0 && !close(fd));
 mb_owned_result_sink sink={.path=path,.stdout_fd=-1};
 mb_owned_result_snapshot s={.build_id="0123456789abcdef0123456789abcdef",.phase=MB_OWNED_RESULT_STARTING};
 CHECK(mb_owned_result_emit(&sink,&s) && sink.delivery.first_file_error==EINTR && close_calls==1);
 fd=open(path,O_RDONLY);CHECK(fd>=0);CHECK(mb_owned_result_emit(&sink,&s));
 CHECK(close_calls==1 && fcntl(fd,F_GETFD)>=0 && !sink.delivery.file_records);
 CHECK(!close(fd) && !unlink(path));
 puts("Interrupted close is reported once, file disabled and reused descriptor remains owned by caller");return 0;
}
