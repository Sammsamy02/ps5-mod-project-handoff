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
static int residence;
static int bridge_native_code_residence_required(void){return residence;}

static int retire_result,retire_checks;
static int bridge_native_pad_code_residence_required(void){return 0;}
static int bridge_native_thread_authority_retire(void){++retire_checks;return retire_result;}
int module_stop(size_t argc, const void *args)
{
    (void)argc; (void)args;
    if (__atomic_test_and_set(&entry_guard, __ATOMIC_ACQUIRE))
        return -20;
    /* The owning runtime is not the only possible holder of published code.
       Closing publication and testing residence is unconditional, including
       partial-start/no-owner cases. The in-link provider refuses unknown state. */
    int residence = bridge_native_code_residence_required();
    if (residence != 0) {
        diagnostic_event("module_stop_code_resident", (uint32_t)residence);
        return leave_entry(-22);
    }
    /* The native pad helper owns executable callbacks independently of hooks.
       Closing starts and retaining a running worker precedes every finalizer. */
    int pad_residence = bridge_native_pad_code_residence_required();
    if (pad_residence != 0) {
        diagnostic_event("module_stop_pad_resident", (uint32_t)pad_residence);
        return leave_entry(-23);
    }
    /* Cancel native authority and drain its tickets before any owner/finalizer
       release. Unknown/self-wait refuses and preserves process residence. */
    int thread_retirement = bridge_native_thread_authority_retire();
    if (thread_retirement != 0) {
        diagnostic_event("module_stop_thread_authority_resident", (uint32_t)thread_retirement);
        return leave_entry(-24);
    }
    if (runtime_active) {
        diagnostic_event("runtime_stop_begin", 0);
        int stop_result = modbridge_runtime_stop();
        diagnostic_event("runtime_stop_result", (uint32_t)stop_result);
        if (stop_result != 0) {
            runtime_failure("runtime_stop_failed", stop_result,
                            "owner stop refused; preserving runtime and module residence");
            /* Nonzero positive callback values may not refuse unload. Always return
               a negative entry error. Preserve active ownership and skip every
               fini/input/channel release while code can still be referenced. */
            return leave_entry(-21);
        }
        runtime_active = 0; /* stop succeeded: ownership has been retired */
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
 runtime_active=strcmp(argv[1],"refuse-inactive")!=0;int before=runtime_active;
 retire_result=!strcmp(argv[1],"unknown")?-1:!strcmp(argv[1],"success")?0:1;
 int r=module_stop(0,0);
 if(retire_result){if(!(r<0&&retire_checks==1&&runtime_active==before&&stops==0&&finis==0&&inputs==0&&channels==0&&entry_guard==0))return 1;}
 else if(!(r==0&&retire_checks==1&&stops==1&&finis==1&&inputs==1&&channels==1))return 2;
 return 0;
}
