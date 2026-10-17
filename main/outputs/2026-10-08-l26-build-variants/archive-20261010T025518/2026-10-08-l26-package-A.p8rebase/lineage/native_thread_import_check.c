/* Generated from exact declared import table order; no target-memory read. */
#include <stdint.h>
extern const uintptr_t modbridge_supplier_import_table[];
extern const uint32_t modbridge_supplier_import_count;
extern void diagnostic_event(const char *, uint32_t);
__attribute__((visibility("hidden")))
int bridge_native_thread_import_ready(void) {
    const uint32_t count=modbridge_supplier_import_count;
    diagnostic_event("native_thread_import_count", count);
    if(count!=16u){
        diagnostic_event("native_thread_import_count_ok",0u);
        return 0;
    }
    diagnostic_event("native_thread_import_count_ok",1u);
    const int bound=modbridge_supplier_import_table[15u]!=0;
    diagnostic_event("native_thread_import_slot_ok",(uint32_t)bound);
    return bound;
}
