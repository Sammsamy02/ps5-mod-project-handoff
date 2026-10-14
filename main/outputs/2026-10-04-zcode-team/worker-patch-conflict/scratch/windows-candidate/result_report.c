#include "result_report.h"
#include <errno.h>
struct buffer {char *out;size_t used,capacity;unsigned failed;};
static void literal(struct buffer *b,const char *text){
 for(;*text;++text){if(b->used+1>=b->capacity){b->failed=1;return;}b->out[b->used++]=*text;}
}
static void number(struct buffer *b,uint64_t n){
 char digits[21];size_t used=0;do{digits[used++]=(char)('0'+n%10);n/=10;}while(n);
 while(used){char one[2]={digits[--used],0};literal(b,one);}
}
static void signed_number(struct buffer *b,int32_t n){if(n<0){literal(b,"-");number(b,(uint64_t)(-(int64_t)n));}else number(b,(uint32_t)n);}
static void operation(struct buffer *b,const mb_pe_ps5_memory_operation_result *r){
 literal(b,"{\"operation\":");number(b,r->operation);
 literal(b,",\"native_result\":");signed_number(b,r->native_result);
 literal(b,",\"native_errno\":");signed_number(b,r->native_errno);
 literal(b,",\"offset\":");number(b,r->offset);literal(b,",\"length\":");number(b,r->length);
 literal(b,",\"rights\":");number(b,r->rights);literal(b,"}");
}
size_t mb_owned_result_format(char *out,size_t capacity,const mb_owned_result_snapshot *s){
 if(!out || !capacity)return 0;out[0]=0;
 if(!s || !s->build_id || s->phase<1 || s->phase>MB_OWNED_RESULT_RETIRED)return 0;
 for(size_t i=0;i<32;++i)if(!((s->build_id[i]>='0' && s->build_id[i]<='9') || (s->build_id[i]>='a' && s->build_id[i]<='f')))return 0;
 if(s->build_id[32])return 0;
 struct buffer b={out,0,capacity,0};const mb_owned_dll_report *r=&s->runtime;
 literal(&b,"{\"marker\":\"modbridge.owned-dll\",\"schema\":1,\"build_id\":\"");literal(&b,s->build_id);
 literal(&b,"\",\"sequence\":");number(&b,s->sequence);literal(&b,",\"phase\":");number(&b,s->phase);
 literal(&b,",\"execution\":{\"stage\":");number(&b,r->stage);
 literal(&b,",\"started\":");number(&b,r->started);literal(&b,",\"run_attempted\":");number(&b,r->run_attempted);
 literal(&b,",\"completed\":");number(&b,r->completed);
 literal(&b,",\"dll_calls_started\":");number(&b,r->dll_calls_started);
 literal(&b,",\"dll_calls_returned\":");number(&b,r->dll_calls_returned);
 literal(&b,"},\"entry\":{\"enabled\":");number(&b,r->entry_enabled);
 literal(&b,",\"attach_calls\":[");number(&b,r->entry_attach_calls[0]);literal(&b,",");number(&b,r->entry_attach_calls[1]);
 literal(&b,"],\"detach_calls\":[");number(&b,r->entry_detach_calls[0]);literal(&b,",");number(&b,r->entry_detach_calls[1]);
 literal(&b,"],\"attach_result\":[");signed_number(&b,r->entry_attach_result[0]);literal(&b,",");signed_number(&b,r->entry_attach_result[1]);
 literal(&b,"],\"detach_result\":[");signed_number(&b,r->entry_detach_result[0]);literal(&b,",");signed_number(&b,r->entry_detach_result[1]);
 literal(&b,"],\"contract_errors\":");number(&b,r->entry_contract_errors);
 literal(&b,"}");
 if(r->clock_enabled){
  literal(&b,",\"clock\":{\"enabled\":1,\"session_begins\":[");number(&b,r->clock_session_begins[0]);literal(&b,",");number(&b,r->clock_session_begins[1]);
  literal(&b,"],\"session_ends\":[");number(&b,r->clock_session_ends[0]);literal(&b,",");number(&b,r->clock_session_ends[1]);literal(&b,"]}");
 }
 if(r->worker_enabled){
  literal(&b,",\"worker\":{\"enabled\":1,\"calls_started\":");number(&b,r->worker_calls_started);
  literal(&b,",\"calls_returned\":");number(&b,r->worker_calls_returned);
  literal(&b,",\"begin\":[");for(unsigned i=0;i<4;++i){if(i)literal(&b,",");number(&b,r->worker_begin[i]);}
  literal(&b,"],\"result\":[");for(unsigned i=0;i<4;++i){if(i)literal(&b,",");number(&b,r->worker_result[i]);}
  literal(&b,"]}");
 }
 if(r->sync_enabled){literal(&b,",\"synchronization\":{\"enabled\":1}");}
 if(r->init_once_enabled){literal(&b,",\"init_once\":{\"enabled\":1}");}
 if(r->mutex_enabled){literal(&b,",\"mutex\":{\"enabled\":1}");}
 literal(&b,",\"first_failure\":{\"stage\":");number(&b,r->failure_stage);
 literal(&b,",\"native_status\":");number(&b,r->native_status);literal(&b,",\"windows_error\":");number(&b,r->windows_error);
 literal(&b,",\"module_cause\":");number(&b,r->first_module_cause);
 literal(&b,"},\"counts\":{\"consumer_imports\":");number(&b,r->consumer_imports);
 literal(&b,",\"helper_imports\":");number(&b,r->helper_imports);
 literal(&b,",\"source_acquires\":[");number(&b,r->source_acquires[0]);literal(&b,",");number(&b,r->source_acquires[1]);
 literal(&b,"],\"source_releases\":[");number(&b,r->source_releases[0]);literal(&b,",");number(&b,r->source_releases[1]);
 literal(&b,"]},\"results\":[");number(&b,r->results[0]);literal(&b,",");number(&b,r->results[1]);
 literal(&b,"],\"cleanup\":{\"pending\":");number(&b,r->cleanup_pending);
 literal(&b,",\"retired\":");number(&b,s->phase==MB_OWNED_RESULT_RETIRED && !r->cleanup_pending);
 literal(&b,"},\"memory\":{\"owner_status\":");number(&b,s->memory_status);
 literal(&b,",\"last\":");operation(&b,&s->memory.last);literal(&b,",\"first_failure\":");operation(&b,&s->memory.first_failure);
 literal(&b,"},\"delivery\":{\"first_file_error\":");signed_number(&b,s->delivery.first_file_error);
 literal(&b,",\"last_file_error\":");signed_number(&b,s->delivery.last_file_error);
 literal(&b,",\"first_stdout_error\":");signed_number(&b,s->delivery.first_stdout_error);
 literal(&b,",\"last_stdout_error\":");signed_number(&b,s->delivery.last_stdout_error);
 literal(&b,",\"file_records\":");number(&b,s->delivery.file_records);
 literal(&b,",\"stdout_records\":");number(&b,s->delivery.stdout_records);literal(&b,"}}\n");
 if(b.failed){out[0]=0;return 0;}out[b.used]=0;return b.used;
}
int mb_owned_write_record(mb_owned_record_writer write,void *context,const void *bytes,size_t size,mb_owned_write_result *r){
 if(!r)return -1;*r=(mb_owned_write_result){0};
 if(!write || !bytes || !size){r->error=EINVAL;return -1;}
 while(r->bytes<size && r->calls<MB_OWNED_WRITE_ATTEMPTS){
  int32_t error=0;++r->calls;ptrdiff_t n=write(context,(const char *)bytes+r->bytes,size-r->bytes,&error);
  if(n==-1 && error==EINTR)continue;
  if(n<=0 || (size_t)n>size-r->bytes){r->error=n==-1 && error?error:EIO;return -1;}
  r->bytes+=(size_t)n;
 }
 if(r->bytes!=size){r->error=EAGAIN;return -1;}return 0;
}
