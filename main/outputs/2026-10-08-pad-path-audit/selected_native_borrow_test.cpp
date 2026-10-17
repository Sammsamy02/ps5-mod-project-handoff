#include <cstddef>
#include <memory>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <stdexcept>
extern "C" {
#include "../../outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/ue4ss-native-pad-provider-coexist/primary-sources/SDL/src/joystick/ps5/SDL_ps5joystick.h"
}
namespace RC::Input {
struct PadBinding {};
struct PadBackend { int(*a)(void*);int(*b)();int(*c)(int*);int(*d)(int,int,int,void*);int(*e)(int,PS5_PadData*);int(*f)(int);int(*g)(); };
class PS5PadSource { PadBackend b; int user; public: PS5PadSource(std::vector<PadBinding>,PadBackend b,int user):b(b),user(user){} bool activate(){return b.d(user,0,0,nullptr)>=0;} };
}

#include "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/ue4ss-native-pad-provider-coexist/port/PadServiceLease.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#ifndef __PROSPERO__
#error Native pad provider requires real PS5 target
#endif
extern "C" int sceUserServiceTerminate(void); // Retained SDK samples/browser/main.c:27.
// Shared composition diagnostic sink (diagnostic.c), the same in-link C symbol
// PS5PadSource.cpp / NativePadInput.cpp already declare; adds no import.
extern "C" void diagnostic_event(const char* tag, unsigned int value);
static_assert(sizeof(PS5_PadData)==120);
static_assert(offsetof(PS5_PadData,buttons)==0 && offsetof(PS5_PadData,connected)==76);
static_assert(offsetof(PS5_PadData,timestamp)==80 && offsetof(PS5_PadData,count)==104);
namespace {
RC::Input::Detail::PadServiceLease user_service(sceUserServiceInitialize,sceUserServiceTerminate);
int acquire_service(void* arg) { return user_service.acquire(arg); }
int release_service() { return user_service.release(); }

// [claude 2026-10-06] Borrowed-handle open (ROUTE P3 of worker-pad-defer
// PROPOSAL-ALTERNATE-COEXIST-ROUTE). The game already holds the standard port in THIS
// process, so scePadOpen(user, STANDARD, 0) returns PS5_PAD_ERROR_ALREADY_OPENED.
// scePadGetHandle(user, type, index) (SDL_ps5joystick.h:106) should return that handle;
// scePadReadState on it is a non-consuming snapshot. A borrowed handle is never closed.
//
// rev 2 (launch ca7e071f, session 162): the synchronous GetHandle call never returned
// on console - module_start stopped inside the pad factory (no pad tag, no
// runtime_start_ok) and ploader reported FAIL. GetHandle now runs ONCE on a detached
// helper thread; open_or_borrow never blocks. Until the helper finishes, the open
// reports the original ALREADY_OPENED (exactly the old deferred behavior), and
// PS5PadSource's poll()-side retry picks the handle up once it exists.
// Evidence: pad_borrow_begin (user id) before the call, pad_borrow_returned (raw rc)
// after it, pad_open_class=1 when the borrowed handle is first used. begin without
// returned = the call hangs on this firmware.
//
// rev 3 (session 174 crashed after pad_borrow_begin; root cause in packaging, not
// here: package_prx.py cloned the scePadGetHandle import identity from sceKernelOpen,
// publishing it under libkernel instead of libScePad, so the call could not bind).
// Belt and braces: the borrow is never attempted during module_start (the factory's
// first open) and only from a poll()-side retry at least PAD_BORROW_DELAY_MS later,
// i.e. after the runtime and mods have started and their evidence is written.
constexpr int kPadAlreadyOpened = static_cast<int>(PS5_PAD_ERROR_ALREADY_OPENED);
#ifndef PAD_BORROW_DELAY_MS
#define PAD_BORROW_DELAY_MS 10000
#endif
enum : int { kBorrowIdle = 0, kBorrowRunning = 1, kBorrowDone = 2 };
std::atomic<int> open_calls{0};
std::atomic<long long> first_open_ms{0};
long long steady_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
std::atomic<int> borrow_state{kBorrowIdle};
std::atomic<int> borrow_result{-1};
std::atomic<int> borrowed_handle{-1};
void start_borrow(int user, int type, int index) {
    int expected=kBorrowIdle;
    if (!borrow_state.compare_exchange_strong(expected,kBorrowRunning,std::memory_order_acq_rel)) return;
    diagnostic_event("pad_borrow_begin",static_cast<unsigned int>(user));
    std::thread([user,type,index] {
        const int game_handle=scePadGetHandle(user,type,index);
        diagnostic_event("pad_borrow_returned",static_cast<unsigned int>(game_handle));
        borrow_result.store(game_handle,std::memory_order_release);
        borrow_state.store(kBorrowDone,std::memory_order_release);
    }).detach();
}
int open_or_borrow(int user, int type, int index, void* param) {
    const int handle=scePadOpen(user,type,index,param);
    if (handle!=kPadAlreadyOpened) return handle;
    if (open_calls.fetch_add(1,std::memory_order_acq_rel)==0) {
        first_open_ms.store(steady_ms(),std::memory_order_release);
        return handle; // module_start: never borrow here (old deferral, byte-for-byte behavior).
    }
    if (borrow_state.load(std::memory_order_acquire)!=kBorrowDone) {
        if (steady_ms()-first_open_ms.load(std::memory_order_acquire)>=PAD_BORROW_DELAY_MS)
            start_borrow(user,type,index);
        return handle; // Deferred, exactly as before; the poll()-side retry comes back.
    }
    const int game_handle=borrow_result.load(std::memory_order_acquire);
    if (game_handle<0) return handle; // GetHandle refused: keep ALREADY_OPENED (one attempt only).
    int expected=-1;
    if (borrowed_handle.compare_exchange_strong(expected,game_handle,std::memory_order_acq_rel))
        diagnostic_event("pad_open_class",1u);
    return game_handle;
}
int close_owned(int handle) {
    int expected=handle;
    if (borrowed_handle.compare_exchange_strong(expected,-1,std::memory_order_acq_rel)) return 0;
    return scePadClose(handle);
}
// The poll()-side retry re-runs activate(), which calls pad_initialize each time; only
// the first scePadInit is real (its success is cached), so a retry can never fail on
// an already-initialized library (worker-pad-defer AUDIT FINDING-1).
std::atomic<bool> pad_initialized{false};
int pad_initialize_once() {
    if (pad_initialized.load(std::memory_order_acquire)) return 0;
    const int err=scePadInit();
    if (err==0) pad_initialized.store(true,std::memory_order_release);
    return err;
}
}
namespace RC::Input {
std::shared_ptr<PS5PadSource> make_native_ps5_pad_source(std::vector<PadBinding> bindings, int user) {
    auto source=std::make_shared<PS5PadSource>(std::move(bindings),PadBackend{
        acquire_service,pad_initialize_once,sceUserServiceGetLoginUserIdList,open_or_borrow,
        scePadReadState,close_owned,release_service},user);
    source->activate();
    return source;
}
}

namespace {
std::atomic<int> get_calls{}, close_calls{}, last_user{-1}, mode{}, gate{}, entered{};
void require(bool x,const char* what){if(!x)throw std::runtime_error(what);}
void wait_done(){for(int n=0;n<1000&&borrow_state.load()!=kBorrowDone;++n)std::this_thread::sleep_for(std::chrono::milliseconds(1));require(borrow_state.load()==kBorrowDone,"helper completes");}
}
extern "C" int scePadInit(){return 0;}
extern "C" int scePadOpen(int,int,int,void*){return static_cast<int>(PS5_PAD_ERROR_ALREADY_OPENED);}
extern "C" int scePadGetHandle(int user,int,int){++get_calls;last_user.store(user);entered.store(1);while(mode.load()==3&&!gate.load())std::this_thread::yield();return mode.load()==1?-1:42;}
extern "C" int scePadReadState(int,PS5_PadData*){return 0;}
extern "C" int scePadClose(int){++close_calls;return 0;}
extern "C" int sceUserServiceInitialize(void*){return 0;}
extern "C" int sceUserServiceTerminate(){return 0;}
extern "C" int sceUserServiceGetLoginUserIdList(int* users){users[0]=1;return 0;}
extern "C" void diagnostic_event(const char*,unsigned int){}
int main(int argc,char**argv){try{
 require(argc==2,"case name"); const auto c=argv[1];
 mode.store(!std::strcmp(c,"negative")?1:!std::strcmp(c,"blocked")?3:0);
 require(open_or_borrow(1,0,0,nullptr)==kPadAlreadyOpened,"startup defers");require(get_calls.load()==0,"no startup GetHandle");
 require(open_or_borrow(1,0,0,nullptr)==kPadAlreadyOpened,"first retry defers");
 if(!std::strcmp(c,"blocked")){
  for(int i=0;i<1000&&!entered.load();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));require(entered.load(),"entered helper");
  for(int i=0;i<10;++i)require(open_or_borrow(1,0,0,nullptr)==kPadAlreadyOpened,"running remains deferred");require(get_calls.load()==1,"one helper");require(borrow_state.load()==kBorrowRunning,"no timeout state transition");gate.store(1);wait_done();
 }else{
  wait_done();
  if(!std::strcmp(c,"negative")){
   for(int i=0;i<10;++i)require(open_or_borrow(1,0,0,nullptr)==kPadAlreadyOpened,"negative remains deferred");require(get_calls.load()==1,"negative never retries");
  }else{
   require(open_or_borrow(1,0,0,nullptr)==42,"positive borrowed handle");require(close_owned(42)==0&&close_calls.load()==0,"borrow not closed");
   if(!std::strcmp(c,"user-change")){require(open_or_borrow(2,0,0,nullptr)==42,"old handle reused on new user");require(last_user.load()==1&&get_calls.load()==1,"user tuple not rebound");}
   else if(!std::strcmp(c,"close-reuse")){require(open_or_borrow(1,0,0,nullptr)==42,"retired result reused");require(get_calls.load()==1,"no validation after retirement");}
  }
 }
 std::printf("PASS %s (observed selected behavior; host SDK seams)\n",c);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
