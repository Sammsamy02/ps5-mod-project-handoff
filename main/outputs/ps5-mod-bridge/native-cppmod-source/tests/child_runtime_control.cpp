#include "../child_runtime.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace { int events[32], count; const char* scenario; }
static void event(int n) { assert(count<32); events[count++]=n; }
static void first() {
    event(1);
    assert(module_start(0,nullptr)==MB_CPPMOD_CHILD_BUSY);
    assert(mb_cppmod_child_acquire()==MB_CPPMOD_CHILD_BUSY);
}
static void second() { event(2); if(!std::strcmp(scenario,"throw-constructor")) throw std::runtime_error("owned constructor"); }
static void fini_first() { event(3); }
static void fini_second() { event(4); if(!std::strcmp(scenario,"throw-finalizer")) throw std::runtime_error("owned finalizer"); }
extern "C" {
using Callback=void(*)();
Callback mb_fixture_init_start[2]={first,second};
Callback mb_fixture_fini_start[2]={fini_first,fini_second};
unsigned char mb_fixture_eh_start[8]={};
unsigned char mb_fixture_dso;
void mb_fixture_add(std::uintptr_t p) { assert(p==reinterpret_cast<std::uintptr_t>(mb_fixture_eh_start)); event(10); }
void mb_fixture_remove(std::uintptr_t p) { assert(p==reinterpret_cast<std::uintptr_t>(mb_fixture_eh_start)); event(11); }
void mb_fixture_finalize(void* p) { assert(p==&mb_fixture_dso); event(5); }
}
#ifdef __APPLE__
asm(".globl _mb_fixture_init_end\n_mb_fixture_init_end = _mb_fixture_init_start + 16\n"
    ".globl _mb_fixture_fini_end\n_mb_fixture_fini_end = _mb_fixture_fini_start + 16\n"
    ".globl _mb_fixture_eh_end\n_mb_fixture_eh_end = _mb_fixture_eh_start + 4\n");
#else
asm(".globl mb_fixture_init_end\nmb_fixture_init_end = mb_fixture_init_start + 16\n"
    ".globl mb_fixture_fini_end\nmb_fixture_fini_end = mb_fixture_fini_start + 16\n"
    ".globl mb_fixture_eh_end\nmb_fixture_eh_end = mb_fixture_eh_start + 4\n");
#endif
int main(int argc,char**argv) {
    assert(argc==2);scenario=argv[1];
    assert(mb_cppmod_child_acquire()==MB_CPPMOD_CHILD_NOT_ACTIVE);
    assert(mb_cppmod_child_release()==MB_CPPMOD_CHILD_NOT_ACTIVE);
    assert(module_stop(0,nullptr)==MB_CPPMOD_CHILD_NOT_ACTIVE);
    if(!std::strcmp(scenario,"null-array")) mb_fixture_init_start[1]=nullptr;
    const int started=module_start(0,nullptr);
    if(!std::strcmp(scenario,"null-array")) {
        assert(started==MB_CPPMOD_CHILD_INVALID_LAYOUT && count==0);
        assert(module_start(0,nullptr)==MB_CPPMOD_CHILD_FAILED);
    } else if(!std::strcmp(scenario,"throw-constructor")) {
        assert(started==MB_CPPMOD_CHILD_FAILED);
        const int want[]={10,1,2,5,11};assert(count==5 && !std::memcmp(want,events,sizeof(want)));
        assert(module_stop(0,nullptr)==MB_CPPMOD_CHILD_FAILED);
        assert(module_start(0,nullptr)==MB_CPPMOD_CHILD_FAILED && count==5);
    } else {
        assert(started==0 && count==3);
        assert(module_start(0,nullptr)==0 && count==3);
        assert(mb_cppmod_child_release()==MB_CPPMOD_CHILD_BAD_RELEASE);
        assert(mb_cppmod_child_acquire()==0);
        assert(module_stop(0,nullptr)==MB_CPPMOD_CHILD_BUSY && count==3);
        assert(mb_cppmod_child_release()==0);
        if(!std::strcmp(scenario,"throw-finalizer")) {
            assert(module_stop(0,nullptr)==MB_CPPMOD_CHILD_FAILED);
            assert(count==4 && events[3]==4);
            assert(module_stop(0,nullptr)==MB_CPPMOD_CHILD_FAILED && count==4);
            // Frame registration remains retained on incomplete cleanup.
        } else {
            assert(module_stop(0,nullptr)==0);
            const int want[]={10,1,2,4,3,5,11};assert(count==7 && !std::memcmp(want,events,sizeof(want)));
            assert(module_stop(0,nullptr)==0 && count==7);
            assert(module_start(0,nullptr)==MB_CPPMOD_CHILD_FINISHED);
        }
        assert(mb_cppmod_child_acquire()==MB_CPPMOD_CHILD_NOT_ACTIVE);
    }
    std::puts(scenario);
}
