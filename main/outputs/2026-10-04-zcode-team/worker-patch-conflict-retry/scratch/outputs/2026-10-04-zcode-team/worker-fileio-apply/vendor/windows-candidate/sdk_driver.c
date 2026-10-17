#include "owned_runtime.h"
#include "../windows-execution/pe_ps5_memory.h"
#include <unistd.h>
#include "sdk_driver.h"
#include "owned_profile.h"
static mb_pe_ps5_memory memory;
static mb_pe_ps5_memory_slot slots[4];
static uint32_t start_attempted,memory_opened,memory_status,first_memory_error;
static uint32_t execute_attempted,sequence;
static mb_owned_result_sink sink;
/* Ordinary SDK payload CRT calls main. These additional owned entrypoints are
 * not PRX DT_INIT/DT_FINI and require the initialized genuine SDK runtime. */
__attribute__((force_align_arg_pointer)) int mb_owned_ps5_start(void){
 if(start_attempted)return -1;start_attempted=1;mb_pe_memory_ops ops;
 memory_status=mb_pe_ps5_memory_open(&memory,slots,4,&ops);
 if(memory_status!=MB_PE_PS5_MEMORY_OK){first_memory_error=memory_status;return -1;}memory_opened=1;
 return mb_owned_dll_open(&ops);
}
__attribute__((force_align_arg_pointer)) int mb_owned_ps5_run(void){return mb_owned_dll_run();}
__attribute__((force_align_arg_pointer)) int mb_owned_ps5_cleanup(void){
 if(mb_owned_dll_cleanup())return -1;
 if(memory_opened){memory_status=mb_pe_ps5_memory_close(&memory);if(memory_status!=MB_PE_PS5_MEMORY_OK){if(!first_memory_error)first_memory_error=memory_status;return -1;}memory_opened=0;}
 return 0;
}
__attribute__((force_align_arg_pointer)) uint32_t mb_owned_ps5_memory_status(void){return memory_status;}
__attribute__((force_align_arg_pointer)) void mb_owned_ps5_result(mb_owned_dll_report *out){
 if(!out)return;mb_owned_dll_result(out);if(memory_opened)out->cleanup_pending=1;
 if(first_memory_error && !out->failure_stage){out->failure_stage=18;out->native_status=first_memory_error;}
}
__attribute__((force_align_arg_pointer)) void mb_owned_ps5_delivery(mb_owned_delivery *out){if(out)*out=sink.delivery;}
static void emit(unsigned phase){
 mb_owned_result_snapshot snapshot={.build_id=MB_OWNED_BUILD_ID,.sequence=sequence++,.phase=phase,.memory_status=memory_status};
 mb_owned_ps5_result(&snapshot.runtime);(void)mb_pe_ps5_memory_get_diagnostics(&memory,&snapshot.memory);
 (void)mb_owned_result_emit(&sink,&snapshot);
}
__attribute__((force_align_arg_pointer)) int mb_owned_ps5_execute(const char *path,int fd){
 if(execute_attempted || start_attempted || !path || fd< -1 || (!path[0] && fd<0))return -1;
 execute_attempted=1;sink=(mb_owned_result_sink){.path=path,.stdout_fd=fd};
 emit(MB_OWNED_RESULT_STARTING);
 int result=mb_owned_ps5_start();emit(MB_OWNED_RESULT_INITIALIZED);
 if(!result)result=mb_owned_ps5_run();emit(MB_OWNED_RESULT_RUN_RETURNED);
 /* The first retained failure is delivered before any wait. Repeated failures
  * do not grow the report file or repeat DLL code; final retirement is separate. */
 unsigned reported_pending=0;
 while(mb_owned_ps5_cleanup()){
  if(!reported_pending){emit(MB_OWNED_RESULT_CLEANUP_PENDING);reported_pending=1;}
  sleep(1);
 }
 emit(MB_OWNED_RESULT_RETIRED);
 mb_owned_dll_report report;mb_owned_ps5_result(&report);
 return result || report.failure_stage || !report.completed || sink.delivery.first_file_error || sink.delivery.first_stdout_error;
}
#if !defined(MB_OWNED_DRIVER_TEST)
int main(void){
 return mb_owned_ps5_execute(MB_OWNED_REPORT_PATH,MB_OWNED_STDOUT?STDOUT_FILENO:-1);
}
#endif
