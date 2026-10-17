
#include <cstddef>
#include <cstdint>
#include <iostream>
enum { PROBE_ACTIVE, PROBE_FINISHED };
struct Life { int state=PROBE_ACTIVE; } lifecycle;
int ops, finalized, released_input, closed_channel, errors, runtime_stop_calls;
unsigned char entry_guard, runtime_active=1;
void diagnostic_event(const char*,uint32_t) {}
void runtime_failure(const char*,int,const char*) { ++errors; }
int modbridge_runtime_stop() { ++runtime_stop_calls; return -37; }
int probe_lifecycle_stop(Life* p,int*) { ++finalized; p->state=PROBE_FINISHED; return 0; }
void probe_runtime_release_input() { ++released_input; }
int diagnostic_close_channel() { ++closed_channel; return 0; }
int leave_entry(int result) { __atomic_clear(&entry_guard,__ATOMIC_RELEASE);return result; }
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
int main() {
    int result=module_stop(0,nullptr);
    if(result!=0 || finalized!=1 || runtime_active || errors!=1 || runtime_stop_calls!=1 || released_input!=1 || closed_channel!=1)return 1;
    std::cout<<"OBSERVED runtime stop refusal still clears runtime_active and finalizes DSO\n";
}
