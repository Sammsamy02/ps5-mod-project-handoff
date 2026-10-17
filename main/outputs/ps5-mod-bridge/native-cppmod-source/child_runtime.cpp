#include "child_runtime.h"
#include <cstdint>
#include <limits>

extern "C" {
using ChildCallback=void(*)();
extern ChildCallback __init_array_start[],__init_array_end[];
extern ChildCallback __fini_array_start[],__fini_array_end[];
extern unsigned char __eh_frame_start[],__eh_frame_end[];
extern unsigned char __dso_handle;
void __cxa_finalize(void*);
// Genuine retained LLVM libunwind section APIs, exported by the same UE4SS
// host that supplies this child's C++ base, RTTI and exception runtime.
void __unw_add_dynamic_eh_frame_section(std::uintptr_t);
void __unw_remove_dynamic_eh_frame_section(std::uintptr_t);
}

namespace {
enum State : unsigned char { New,Starting,Active,Stopping,Finished,Failed };
State state;                         // Zero-initialized; no constructor required.
unsigned char entry_guard;
std::uint32_t live_objects;
constexpr std::uintptr_t max_callbacks=4096;

struct EntryGuard {
    bool held=!__atomic_test_and_set(&entry_guard,__ATOMIC_ACQUIRE);
    ~EntryGuard() { if(held) __atomic_clear(&entry_guard,__ATOMIC_RELEASE); }
};
bool valid_array(ChildCallback* begin,ChildCallback* end) {
    const auto b=reinterpret_cast<std::uintptr_t>(begin),e=reinterpret_cast<std::uintptr_t>(end);
    if(e<b || b%alignof(ChildCallback) || (e-b)%sizeof(ChildCallback) ||
       (e-b)/sizeof(ChildCallback)>max_callbacks || (!b && e)) return false;
    const auto count=(e-b)/sizeof(ChildCallback);
    for(std::uintptr_t i=0;i<count;++i)
        if(!begin[i] || reinterpret_cast<std::uintptr_t>(begin[i])==std::numeric_limits<std::uintptr_t>::max()) return false;
    return true;
}
bool valid_frames() {
    const auto b=reinterpret_cast<std::uintptr_t>(__eh_frame_start),e=reinterpret_cast<std::uintptr_t>(__eh_frame_end);
    // The selected writer validates records and appends the mandatory four-byte
    // zero terminator after this bounded run. These are own-image linker bounds.
    return b && e>b && e-b<=16u*1024u*1024u;
}
void remove_frames() {
    __unw_remove_dynamic_eh_frame_section(reinterpret_cast<std::uintptr_t>(__eh_frame_start));
}
}

#if defined(__x86_64__)
#define CHILD_ENTRY __attribute__((visibility("default"),force_align_arg_pointer))
#else
#define CHILD_ENTRY __attribute__((visibility("default")))
#endif

extern "C" CHILD_ENTRY int module_start(size_t argc,const void* args) {
    (void)argc;(void)args;
    EntryGuard guard;
    if(!guard.held) return MB_CPPMOD_CHILD_BUSY;
    if(state==Active) return MB_CPPMOD_CHILD_OK;
    if(state==Finished) return MB_CPPMOD_CHILD_FINISHED;
    if(state!=New) return MB_CPPMOD_CHILD_FAILED;
    state=Starting;
    if(!valid_array(__init_array_start,__init_array_end) ||
       !valid_array(__fini_array_start,__fini_array_end) || !valid_frames()) {
        state=Failed;return MB_CPPMOD_CHILD_INVALID_LAYOUT;
    }
    __unw_add_dynamic_eh_frame_section(reinterpret_cast<std::uintptr_t>(__eh_frame_start));
    bool constructor_failed=false;
    try {
        for(auto cursor=__init_array_start;cursor!=__init_array_end;++cursor) (*cursor)();
    } catch(...) {
        constructor_failed=true;
    }
    if(constructor_failed) {
        state=Failed;
        // Only successful constructors registered DSO destructors. Unrelated
        // fini-array callbacks are not safe after partial initialization.
        try { __cxa_finalize(&__dso_handle);remove_frames(); } catch(...) {}
        return MB_CPPMOD_CHILD_FAILED;
    }
    state=Active;return MB_CPPMOD_CHILD_OK;
}

extern "C" CHILD_ENTRY int module_stop(size_t argc,const void* args) {
    (void)argc;(void)args;
    EntryGuard guard;
    if(!guard.held) return MB_CPPMOD_CHILD_BUSY;
    if(state==Finished) return MB_CPPMOD_CHILD_OK;
    if(state==New) return MB_CPPMOD_CHILD_NOT_ACTIVE;
    if(state!=Active) return MB_CPPMOD_CHILD_FAILED;
    if(live_objects) return MB_CPPMOD_CHILD_BUSY;
    state=Stopping;
    try {
        for(auto cursor=__fini_array_end;cursor!=__fini_array_start;) (*--cursor)();
        __cxa_finalize(&__dso_handle);
        remove_frames();
    } catch(...) {
        // Preserve the frame registration and never replay partial cleanup.
        state=Failed;return MB_CPPMOD_CHILD_FAILED;
    }
    state=Finished;return MB_CPPMOD_CHILD_OK;
}

extern "C" int mb_cppmod_child_acquire(void) {
    EntryGuard guard;
    if(!guard.held) return MB_CPPMOD_CHILD_BUSY;
    if(state!=Active) return MB_CPPMOD_CHILD_NOT_ACTIVE;
    if(live_objects==std::numeric_limits<std::uint32_t>::max()) return MB_CPPMOD_CHILD_BUSY;
    ++live_objects;return MB_CPPMOD_CHILD_OK;
}
extern "C" int mb_cppmod_child_release(void) {
    EntryGuard guard;
    if(!guard.held) return MB_CPPMOD_CHILD_BUSY;
    if(state!=Active) return MB_CPPMOD_CHILD_NOT_ACTIVE;
    if(!live_objects) return MB_CPPMOD_CHILD_BAD_RELEASE;
    --live_objects;return MB_CPPMOD_CHILD_OK;
}
