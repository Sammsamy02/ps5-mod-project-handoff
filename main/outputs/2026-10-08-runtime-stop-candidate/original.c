#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned char entry_guard;
static int runtime_active, rc, stops, finis, inputs, channels;
#define PROBE_FINISHED 5
static struct {int state;} lifecycle;
static int ops;
static void diagnostic_event(const char*t,uint32_t v){(void)t;(void)v;}
static int modbridge_runtime_stop(void){++stops;return rc;}
static void runtime_failure(const char*t,int v,const char*d){(void)t;(void)v;(void)d;}
static int probe_lifecycle_stop(void*s,const void*o){(void)s;(void)o;++finis;lifecycle.state=PROBE_FINISHED;return 0;}
static void probe_runtime_release_input(void){++inputs;}
static int diagnostic_close_channel(void){++channels;return 0;}
static int leave_entry(int result){__atomic_clear(&entry_guard,__ATOMIC_RELEASE);return result;}
int module_stop(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))
        return -20;
    if (runtime_active) {
        diagnostic_event("runtime_stop_begin", 0);
        int stop_result = modbridge_runtime_stop();
        diagnostic_event("runtime_stop_result", (uint32_t)stop_result);
        runtime_active = 0; /* the owner is gone (or was never stoppable) */
        if (stop_result != 0)
            runtime_failure("runtime_stop_failed", stop_result,
                            "owner stop reported failure");
    }
    int result = probe_lifecycle_stop(&lifecycle, &ops);
    diagnostic_event("module_stop_return", (uint32_t)result);
    if (result == 0 && lifecycle.state == PROBE_FINISHED) {
        probe_runtime_release_input();
        (void)diagnostic_close_channel();
    }
    return leave_entry(result);
}

int main(int argc,char**argv){
 if(argc!=2)return 99;
 if(!strcmp(argv[1],"busy")){entry_guard=1;runtime_active=1;int r=module_stop(0,0);return r==-20&&stops==0&&finis==0&&runtime_active==1?0:1;}
 runtime_active=strcmp(argv[1],"inactive")!=0;
 rc=!strcmp(argv[1],"negative")?-8:!strcmp(argv[1],"positive")?1:0;
 int r=module_stop(0,0);
 if(rc){if(!(r<0&&runtime_active==1&&finis==0&&inputs==0&&channels==0&&entry_guard==0))return 1;
   rc=0;r=module_stop(0,0);if(!(r==0&&runtime_active==0&&stops==2&&finis==1&&inputs==1&&channels==1))return 2;
 }else if(!(r==0&&runtime_active==0&&finis==1&&inputs==1&&channels==1))return 3;
 puts("PASS actual module_stop control flow; host owner/finalizer seams");return 0;}
