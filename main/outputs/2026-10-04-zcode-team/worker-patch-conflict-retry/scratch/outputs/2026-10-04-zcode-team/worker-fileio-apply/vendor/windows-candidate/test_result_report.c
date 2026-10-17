#include "result_report.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
struct writer {char out[128];size_t used;unsigned calls,mode;};
static ptrdiff_t write_bytes(void *context,const void *bytes,size_t size,int32_t *error){
 struct writer *w=context;++w->calls;
 if(w->mode==1 || (w->mode==0 && w->calls==1)){*error=EINTR;return -1;}
 if(w->mode==2)return 0;
 if(w->mode==3){*error=EIO;return -1;}
 if(w->mode==4)return (ptrdiff_t)size+1;
 size_t count=size>3?3:size;memcpy(w->out+w->used,bytes,count);w->used+=count;return (ptrdiff_t)count;
}
int main(void){
 mb_owned_result_snapshot s={.build_id="0123456789abcdef0123456789abcdef",.sequence=3,.phase=MB_OWNED_RESULT_CLEANUP_PENDING};
 s.runtime.started=1;s.runtime.run_attempted=1;s.runtime.cleanup_pending=1;
 s.runtime.failure_stage=5;s.runtime.native_status=9;
 s.memory.first_failure.operation=MB_PE_PS5_MEMORY_OP_PROTECT;
 s.memory.first_failure.native_result=(int32_t)0x8002000d;s.memory.first_failure.offset=16384;
 s.memory.first_failure.length=16384;s.memory.first_failure.rights=5;
 char record[MB_OWNED_RESULT_CAPACITY];size_t n=mb_owned_result_format(record,sizeof(record),&s);
 CHECK(n && n<sizeof(record) && record[n-1]=='\n' && record[n]==0);
 CHECK(strstr(record,"\"marker\":\"modbridge.owned-dll\"") && strstr(record,"\"schema\":1"));
 CHECK(strstr(record,"\"native_result\":-2147352563") && strstr(record,"\"dll_calls_started\":0"));
 CHECK(!mb_owned_result_format(record,8,&s));
 s.build_id="bad\"id";CHECK(!mb_owned_result_format(record,sizeof(record),&s));
 struct writer w={0};mb_owned_write_result r;
 CHECK(!mb_owned_write_record(write_bytes,&w,"abcdefg\n",8,&r));
 CHECK(r.bytes==8 && r.calls==4 && !r.error && !memcmp(w.out,"abcdefg\n",8));
 for(unsigned mode=1;mode<=4;++mode){w=(struct writer){.mode=mode};CHECK(mb_owned_write_record(write_bytes,&w,"record\n",7,&r));
  CHECK(!r.bytes && r.error && r.calls==(mode==1?MB_OWNED_WRITE_ATTEMPTS:1));}
 puts("Bounded JSON schema, native denial, short writes, EINTR exhaustion and zero/invalid progress passed");return 0;
}
